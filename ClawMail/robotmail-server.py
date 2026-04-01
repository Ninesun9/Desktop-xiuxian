#!/usr/bin/env python3
"""
RobotMail development hub.

Security model:
- transport auth with bot token headers
- end-to-end encrypted text payloads
- attachments are stored as plaintext blobs with integrity metadata
"""
from __future__ import annotations

import hashlib
import json
import os
import secrets
import sqlite3
import uuid
from contextlib import closing
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional

from fastapi import Depends, FastAPI, File, Form, Header, HTTPException, Query, UploadFile
from fastapi.responses import FileResponse
from pydantic import BaseModel, Field


def _default_home() -> Path:
    return Path(__file__).resolve().parent / ".runtime"


PROJECT_DIR = Path(__file__).resolve().parent
BASE_DIR = Path(os.environ.get("ROBOTMAIL_HOME", str(_default_home()))).expanduser()
DATA_DIR = BASE_DIR / "data"
STORE_DIR = BASE_DIR / "store"
DB_PATH = DATA_DIR / "robotmail.db"
MAX_ATTACHMENT_BYTES = int(os.environ.get("ROBOTMAIL_MAX_ATTACHMENT_BYTES", str(25 * 1024 * 1024)))
ALLOWED_ATTACHMENT_TYPES = {
    item.strip()
    for item in os.environ.get(
        "ROBOTMAIL_ALLOWED_ATTACHMENT_TYPES",
        ",".join(
            [
                "application/octet-stream",
                "application/json",
                "application/zip",
                "application/x-zip-compressed",
                "text/plain",
                "text/markdown",
                "application/x-sh",
                "application/gzip",
            ]
        ),
    ).split(",")
    if item.strip()
}

app = FastAPI(title="RobotMail Dev Hub", version="0.2.0")

PUBLIC_DOWNLOADS = {
    "agent_listener.py": PROJECT_DIR / "agent_listener.py",
    "SKILL.md": PROJECT_DIR / "SKILL.md",
    "install-client.sh": PROJECT_DIR / "scripts" / "install-client.sh",
    "robotmail-client.py": PROJECT_DIR / "robotmail-client.py",
    "robotmail_skill.py": PROJECT_DIR / "robotmail_skill.py",
}


class RegisterRequest(BaseModel):
    bot_id: Optional[str] = Field(default=None, min_length=1, max_length=64)
    display_name: Optional[str] = Field(default=None, max_length=128)
    public_key: str = Field(min_length=16)


class AckRequest(BaseModel):
    note: Optional[str] = Field(default=None, max_length=512)


class SendMessageRequest(BaseModel):
    receiver: str = Field(min_length=1, max_length=64)
    kind: str = Field(min_length=1, max_length=32)
    requires_ack: bool = True
    subject: Optional[str] = None
    text: Optional[str] = None
    install: Dict[str, Any] = Field(default_factory=dict)
    labels: List[str] = Field(default_factory=list)
    encrypted_payload: Optional[Dict[str, str]] = None
    encryption: Optional[Dict[str, str]] = None


class AgentContext(BaseModel):
    bot_id: str
    display_name: str
    token: str
    public_key: str


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def ensure_layout() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    STORE_DIR.mkdir(parents=True, exist_ok=True)


def get_db() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def ensure_column(conn: sqlite3.Connection, table: str, column: str, ddl: str) -> None:
    columns = {
        row["name"]
        for row in conn.execute(f"PRAGMA table_info({table})").fetchall()
    }
    if column not in columns:
        conn.execute(f"ALTER TABLE {table} ADD COLUMN {ddl}")


def init_db() -> None:
    ensure_layout()
    with closing(get_db()) as conn:
        cursor = conn.cursor()
        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS agents (
                bot_id TEXT PRIMARY KEY,
                display_name TEXT NOT NULL,
                token TEXT NOT NULL,
                public_key TEXT NOT NULL,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL,
                last_seen_at TEXT
            )
            """
        )
        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS messages (
                id TEXT PRIMARY KEY,
                sender TEXT NOT NULL,
                receiver TEXT NOT NULL,
                kind TEXT NOT NULL,
                payload_json TEXT NOT NULL,
                requires_ack INTEGER NOT NULL DEFAULT 1,
                created_at TEXT NOT NULL,
                delivered_at TEXT,
                acked_at TEXT,
                ack_note TEXT,
                FOREIGN KEY (sender) REFERENCES agents(bot_id),
                FOREIGN KEY (receiver) REFERENCES agents(bot_id)
            )
            """
        )
        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS attachments (
                id TEXT PRIMARY KEY,
                message_id TEXT NOT NULL,
                original_name TEXT NOT NULL,
                stored_name TEXT NOT NULL,
                stored_path TEXT NOT NULL,
                size_bytes INTEGER NOT NULL,
                sha256 TEXT NOT NULL,
                created_at TEXT NOT NULL,
                FOREIGN KEY (message_id) REFERENCES messages(id)
            )
            """
        )

        ensure_column(conn, "agents", "public_key", "public_key TEXT NOT NULL DEFAULT ''")
        cursor.execute(
            "CREATE INDEX IF NOT EXISTS idx_messages_receiver ON messages(receiver, acked_at, created_at)"
        )
        cursor.execute(
            "CREATE INDEX IF NOT EXISTS idx_attachments_message ON attachments(message_id)"
        )
        conn.commit()


@app.on_event("startup")
def on_startup() -> None:
    init_db()


def compute_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def get_public_download(name: str) -> Path:
    path = PUBLIC_DOWNLOADS.get(name)
    if path is None or not path.exists():
        raise HTTPException(status_code=404, detail="download not found")
    return path


def validate_attachment_type(upload: UploadFile) -> None:
    content_type = (upload.content_type or "application/octet-stream").strip()
    if content_type not in ALLOWED_ATTACHMENT_TYPES:
        raise HTTPException(
            status_code=400,
            detail=f"attachment type not allowed: {content_type}",
        )


def get_agent_row(bot_id: str) -> Optional[sqlite3.Row]:
    with closing(get_db()) as conn:
        row = conn.execute("SELECT * FROM agents WHERE bot_id = ?", (bot_id,)).fetchone()
    return row


def require_agent(
    x_robotmail_bot: str = Header(..., alias="X-Robotmail-Bot"),
    x_robotmail_token: str = Header(..., alias="X-Robotmail-Token"),
) -> AgentContext:
    row = get_agent_row(x_robotmail_bot)
    if not row or row["token"] != x_robotmail_token:
        raise HTTPException(status_code=401, detail="invalid bot credentials")
    if not row["public_key"]:
        raise HTTPException(status_code=400, detail="bot has no registered public key")

    with closing(get_db()) as conn:
        now = utc_now()
        conn.execute(
            "UPDATE agents SET last_seen_at = ?, updated_at = ? WHERE bot_id = ?",
            (now, now, x_robotmail_bot),
        )
        conn.commit()

    return AgentContext(
        bot_id=row["bot_id"],
        display_name=row["display_name"],
        token=row["token"],
        public_key=row["public_key"],
    )


def build_attachment_payload(message_id: str, row: sqlite3.Row) -> Dict[str, Any]:
    return {
        "id": row["id"],
        "name": row["original_name"],
        "size_bytes": row["size_bytes"],
        "sha256": row["sha256"],
        "download_url": f"/api/v1/messages/{message_id}/attachments/{row['id']}",
    }


def load_message(message_id: str) -> Optional[Dict[str, Any]]:
    with closing(get_db()) as conn:
        message = conn.execute("SELECT * FROM messages WHERE id = ?", (message_id,)).fetchone()
        if not message:
            return None
        attachments = conn.execute(
            "SELECT * FROM attachments WHERE message_id = ? ORDER BY created_at ASC",
            (message_id,),
        ).fetchall()
        sender = conn.execute(
            "SELECT bot_id, display_name, public_key FROM agents WHERE bot_id = ?",
            (message["sender"],),
        ).fetchone()
        receiver = conn.execute(
            "SELECT bot_id, display_name, public_key FROM agents WHERE bot_id = ?",
            (message["receiver"],),
        ).fetchone()

    payload = json.loads(message["payload_json"])
    return {
        "id": message["id"],
        "sender": message["sender"],
        "sender_public_key": sender["public_key"] if sender else None,
        "receiver": message["receiver"],
        "receiver_public_key": receiver["public_key"] if receiver else None,
        "kind": message["kind"],
        "payload": payload,
        "requires_ack": bool(message["requires_ack"]),
        "created_at": message["created_at"],
        "delivered_at": message["delivered_at"],
        "acked_at": message["acked_at"],
        "ack_note": message["ack_note"],
        "attachments": [
            build_attachment_payload(message["id"], row) for row in attachments
        ],
    }


@app.get("/api/v1/health")
def health() -> Dict[str, str]:
    ensure_layout()
    return {"status": "ok", "time": utc_now()}


@app.get("/downloads/{name}")
def download_public_file(name: str) -> FileResponse:
    path = get_public_download(name)
    media_type = "text/x-shellscript" if path.suffix == ".sh" else "application/octet-stream"
    return FileResponse(path, filename=path.name, media_type=media_type)


@app.get("/install-client.sh")
def download_install_script() -> FileResponse:
    return download_public_file("install-client.sh")


@app.get("/api/v1/agents/{bot_id}")
def get_agent(bot_id: str, agent: AgentContext = Depends(require_agent)) -> Dict[str, str]:
    row = get_agent_row(bot_id)
    if not row:
        raise HTTPException(status_code=404, detail="agent not found")
    return {
        "bot_id": row["bot_id"],
        "display_name": row["display_name"],
        "public_key": row["public_key"],
    }


@app.get("/api/v1/agents")
def list_agents(
    name: Optional[str] = Query(default=None),
    limit: int = Query(default=50, ge=1, le=200),
    agent: AgentContext = Depends(require_agent),
) -> Dict[str, Any]:
    with closing(get_db()) as conn:
        if name:
            rows = conn.execute(
                """
                SELECT bot_id, display_name, public_key, created_at, updated_at, last_seen_at
                FROM agents
                WHERE bot_id = ? OR display_name = ?
                ORDER BY updated_at DESC
                LIMIT ?
                """,
                (name, name, limit),
            ).fetchall()
        else:
            rows = conn.execute(
                """
                SELECT bot_id, display_name, public_key, created_at, updated_at, last_seen_at
                FROM agents
                ORDER BY updated_at DESC
                LIMIT ?
                """,
                (limit,),
            ).fetchall()

    return {
        "agents": [
            {
                "bot_id": row["bot_id"],
                "display_name": row["display_name"],
                "public_key": row["public_key"],
                "created_at": row["created_at"],
                "updated_at": row["updated_at"],
                "last_seen_at": row["last_seen_at"],
            }
            for row in rows
        ]
    }


@app.post("/api/v1/register")
def register_agent(request: RegisterRequest) -> Dict[str, str]:
    now = utc_now()
    bot_id = (request.bot_id or f"bot-{uuid.uuid4().hex[:12]}").strip()
    token = secrets.token_hex(24)
    display_name = request.display_name or bot_id
    with closing(get_db()) as conn:
        conn.execute(
            """
            INSERT INTO agents (bot_id, display_name, token, public_key, created_at, updated_at, last_seen_at)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(bot_id) DO UPDATE SET
                display_name = excluded.display_name,
                token = excluded.token,
                public_key = excluded.public_key,
                updated_at = excluded.updated_at
            """,
            (bot_id, display_name, token, request.public_key, now, now, now),
        )
        conn.commit()

    return {
        "status": "ok",
        "bot_id": bot_id,
        "display_name": display_name,
        "token": token,
        "hub_home": str(BASE_DIR),
    }


@app.post("/api/v1/messages/send")
async def send_message(
    payload_json: str = Form(...),
    attachments: List[UploadFile] = File(default_factory=list),
    agent: AgentContext = Depends(require_agent),
) -> Dict[str, Any]:
    try:
        request = SendMessageRequest.model_validate_json(payload_json)
    except Exception as exc:
        raise HTTPException(status_code=400, detail=f"invalid payload_json: {exc}") from exc

    receiver = request.receiver.strip()
    kind = request.kind.strip().lower()
    requires_ack = request.requires_ack
    encrypted_payload = request.encrypted_payload or {}
    encryption = request.encryption or {}

    if not receiver:
        raise HTTPException(status_code=400, detail="receiver is required")
    if receiver == agent.bot_id:
        raise HTTPException(status_code=400, detail="sender and receiver must differ")
    if not get_agent_row(receiver):
        raise HTTPException(status_code=404, detail=f"receiver not registered: {receiver}")
    if kind not in {"text", "file", "skill", "config"}:
        raise HTTPException(status_code=400, detail=f"unsupported kind: {kind}")
    if kind == "text":
        if not encrypted_payload:
            raise HTTPException(status_code=400, detail="encrypted_payload is required for text messages")
        if encryption.get("algorithm") != "X25519-HKDF-AESGCM":
            raise HTTPException(status_code=400, detail="unsupported encryption algorithm")
    elif encrypted_payload or encryption:
        if encryption and encryption.get("algorithm") not in {"", "none"}:
            raise HTTPException(status_code=400, detail="attachments do not support encrypted payloads in mixed mode")

    message_id = str(uuid.uuid4())
    created_at = utc_now()
    payload: Dict[str, Any]
    if kind == "text":
        payload = {
            "encrypted_payload": encrypted_payload,
            "encryption": encryption,
        }
    else:
        payload = {
            "install": request.install,
            "labels": request.labels,
            "subject": request.subject,
            "text": request.text,
            "attachments_meta": [],
        }

    stored_attachments: List[Dict[str, Any]] = []
    message_store_dir = STORE_DIR / message_id
    message_store_dir.mkdir(parents=True, exist_ok=True)

    for index, upload in enumerate(attachments):
        validate_attachment_type(upload)
        safe_name = Path(upload.filename or f"blob-{index}.bin").name
        attachment_id = str(uuid.uuid4())
        stored_name = f"{attachment_id}-{safe_name}"
        destination = message_store_dir / stored_name

        written = 0
        with destination.open("wb") as handle:
            while True:
                chunk = await upload.read(1024 * 1024)
                if not chunk:
                    break
                written += len(chunk)
                if written > MAX_ATTACHMENT_BYTES:
                    handle.close()
                    destination.unlink(missing_ok=True)
                    raise HTTPException(
                        status_code=400,
                        detail=f"attachment too large: {safe_name}",
                    )
                handle.write(chunk)
        await upload.close()

        stored_attachments.append(
            {
                "id": attachment_id,
                "original_name": safe_name,
                "stored_name": stored_name,
                "stored_path": str(destination),
                "size_bytes": destination.stat().st_size,
                "sha256": compute_sha256(destination),
                "created_at": created_at,
            }
        )

    if kind != "text":
        payload["attachments_meta"] = [
            {
                "upload_name": item["original_name"],
                "original_name": item["original_name"],
            }
            for item in stored_attachments
        ]

    with closing(get_db()) as conn:
        conn.execute(
            """
            INSERT INTO messages
            (id, sender, receiver, kind, payload_json, requires_ack, created_at)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (
                message_id,
                agent.bot_id,
                receiver,
                kind,
                json.dumps(payload, ensure_ascii=False),
                1 if requires_ack else 0,
                created_at,
            ),
        )
        for item in stored_attachments:
            conn.execute(
                """
                INSERT INTO attachments
                (id, message_id, original_name, stored_name, stored_path, size_bytes, sha256, created_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    item["id"],
                    message_id,
                    item["original_name"],
                    item["stored_name"],
                    item["stored_path"],
                    item["size_bytes"],
                    item["sha256"],
                    item["created_at"],
                ),
            )
        conn.commit()

    return {
        "status": "ok",
        "message_id": message_id,
        "receiver": receiver,
        "kind": kind,
        "attachments": [
            {
                "id": item["id"],
                "size_bytes": item["size_bytes"],
                "sha256": item["sha256"],
            }
            for item in stored_attachments
        ],
    }


@app.get("/api/v1/messages/poll")
def poll_messages(
    limit: int = Query(default=20, ge=1, le=200),
    include_acked: bool = Query(default=False),
    agent: AgentContext = Depends(require_agent),
) -> Dict[str, Any]:
    with closing(get_db()) as conn:
        if include_acked:
            rows = conn.execute(
                """
                SELECT id FROM messages
                WHERE receiver = ?
                ORDER BY created_at DESC
                LIMIT ?
                """,
                (agent.bot_id, limit),
            ).fetchall()
        else:
            rows = conn.execute(
                """
                SELECT id FROM messages
                WHERE receiver = ? AND acked_at IS NULL
                ORDER BY created_at ASC
                LIMIT ?
                """,
                (agent.bot_id, limit),
            ).fetchall()

        message_ids = [row["id"] for row in rows]
        now = utc_now()
        for message_id in message_ids:
            conn.execute(
                """
                UPDATE messages
                SET delivered_at = COALESCE(delivered_at, ?)
                WHERE id = ?
                """,
                (now, message_id),
            )
        conn.commit()

    messages = [load_message(message_id) for message_id in message_ids]
    return {"messages": [message for message in messages if message is not None]}


@app.get("/api/v1/messages/{message_id}")
def get_message(message_id: str, agent: AgentContext = Depends(require_agent)) -> Dict[str, Any]:
    message = load_message(message_id)
    if not message:
        raise HTTPException(status_code=404, detail="message not found")
    if agent.bot_id not in {message["sender"], message["receiver"]}:
        raise HTTPException(status_code=403, detail="message not visible for this bot")
    return message


@app.post("/api/v1/messages/{message_id}/ack")
def ack_message(
    message_id: str,
    payload: AckRequest,
    agent: AgentContext = Depends(require_agent),
) -> Dict[str, Any]:
    message = load_message(message_id)
    if not message:
        raise HTTPException(status_code=404, detail="message not found")
    if message["receiver"] != agent.bot_id:
        raise HTTPException(status_code=403, detail="only receiver can ack this message")

    acked_at = utc_now()
    with closing(get_db()) as conn:
        conn.execute(
            """
            UPDATE messages
            SET acked_at = ?, ack_note = ?
            WHERE id = ?
            """,
            (acked_at, payload.note, message_id),
        )
        conn.commit()

    return {"status": "ok", "message_id": message_id, "acked_at": acked_at}


@app.get("/api/v1/messages/{message_id}/attachments/{attachment_id}")
def download_attachment(
    message_id: str,
    attachment_id: str,
    agent: AgentContext = Depends(require_agent),
) -> FileResponse:
    message = load_message(message_id)
    if not message:
        raise HTTPException(status_code=404, detail="message not found")
    if agent.bot_id not in {message["sender"], message["receiver"]}:
        raise HTTPException(status_code=403, detail="attachment not visible for this bot")

    with closing(get_db()) as conn:
        row = conn.execute(
            """
            SELECT * FROM attachments
            WHERE id = ? AND message_id = ?
            """,
            (attachment_id, message_id),
        ).fetchone()

    if not row:
        raise HTTPException(status_code=404, detail="attachment not found")

    return FileResponse(
        row["stored_path"],
        filename=row["original_name"],
        media_type="application/octet-stream",
    )


def main() -> None:
    import argparse
    import uvicorn

    parser = argparse.ArgumentParser(description="RobotMail development hub")
    parser.add_argument("--host", default="0.0.0.0", help="listen host")
    parser.add_argument("--port", type=int, default=5180, help="listen port")
    parser.add_argument("--reload", action="store_true", help="enable uvicorn reload")
    args = parser.parse_args()

    init_db()
    uvicorn.run(
        "robotmail-server:app",
        host=args.host,
        port=args.port,
        reload=args.reload,
    )


if __name__ == "__main__":
    main()
