#!/usr/bin/env python3
"""
RobotMail development client with encrypted text and plaintext attachments.
"""
from __future__ import annotations

import base64
import json
import os
import shutil
import tempfile
from pathlib import Path
from typing import Any, Dict, List, Optional

import requests
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import x25519
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDF


def _default_config_home() -> Path:
    return Path(os.environ.get("ROBOTMAIL_CLIENT_HOME", str(Path.home() / ".robotmail"))).expanduser()


CONFIG_HOME = _default_config_home()
PROFILE_PATH = CONFIG_HOME / "profiles.json"
CONTACTS_PATH = CONFIG_HOME / "contacts.json"


def ensure_config_home() -> None:
    CONFIG_HOME.mkdir(parents=True, exist_ok=True)


def load_profiles() -> Dict[str, Any]:
    ensure_config_home()
    if not PROFILE_PATH.exists():
        return {"profiles": {}}
    return json.loads(PROFILE_PATH.read_text())


def save_profiles(data: Dict[str, Any]) -> None:
    ensure_config_home()
    PROFILE_PATH.write_text(json.dumps(data, indent=2, ensure_ascii=False))


def load_contacts() -> Dict[str, Any]:
    ensure_config_home()
    if not CONTACTS_PATH.exists():
        return {"contacts": {}}
    return json.loads(CONTACTS_PATH.read_text())


def save_contacts(data: Dict[str, Any]) -> None:
    ensure_config_home()
    CONTACTS_PATH.write_text(json.dumps(data, indent=2, ensure_ascii=False))


def get_contact_bucket(hub_url: str) -> Dict[str, Any]:
    contacts = load_contacts()
    root = contacts.setdefault("contacts", {})
    if any(isinstance(value, dict) and "bot_id" in value for value in root.values()):
        migrated = {"default": root}
        contacts["contacts"] = migrated
        save_contacts(contacts)
        root = contacts["contacts"]
    bucket = root.setdefault(hub_url, {})
    return bucket


def b64e(data: bytes) -> str:
    return base64.b64encode(data).decode("ascii")


def b64d(data: str) -> bytes:
    return base64.b64decode(data.encode("ascii"))


def generate_keypair() -> Dict[str, str]:
    private_key = x25519.X25519PrivateKey.generate()
    public_key = private_key.public_key()
    private_bytes = private_key.private_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PrivateFormat.Raw,
        encryption_algorithm=serialization.NoEncryption(),
    )
    public_bytes = public_key.public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )
    return {
        "private_key": b64e(private_bytes),
        "public_key": b64e(public_bytes),
    }


def derive_message_key(private_key_b64: str, peer_public_key_b64: str, salt: bytes) -> bytes:
    private_key = x25519.X25519PrivateKey.from_private_bytes(b64d(private_key_b64))
    public_key = x25519.X25519PublicKey.from_public_bytes(b64d(peer_public_key_b64))
    shared_secret = private_key.exchange(public_key)
    return HKDF(
        algorithm=hashes.SHA256(),
        length=32,
        salt=salt,
        info=b"robotmail-message",
    ).derive(shared_secret)


def encrypt_bytes(key: bytes, plaintext: bytes) -> Dict[str, str]:
    nonce = os.urandom(12)
    ciphertext = AESGCM(key).encrypt(nonce, plaintext, None)
    return {
        "nonce": b64e(nonce),
        "ciphertext": b64e(ciphertext),
    }


def decrypt_bytes(key: bytes, nonce_b64: str, ciphertext_b64: str) -> bytes:
    return AESGCM(key).decrypt(b64d(nonce_b64), b64d(ciphertext_b64), None)


def build_headers(profile: Dict[str, str]) -> Dict[str, str]:
    return {
        "X-Robotmail-Bot": profile["bot_id"],
        "X-Robotmail-Token": profile["token"],
    }


def get_profile(bot_id: str) -> Dict[str, str]:
    profiles = load_profiles().get("profiles", {})
    profile = profiles.get(bot_id)
    if not profile:
        raise SystemExit(f"bot '{bot_id}' is not registered locally; run register first")
    if "private_key" not in profile or "public_key" not in profile:
        raise SystemExit(f"bot '{bot_id}' has no local encryption keys; re-register it")
    return profile


class RobotMailClient:
    def __init__(self, bot_id: str):
        self.bot_id = bot_id
        self.profile = get_profile(bot_id)
        self.hub_url = self.profile["hub_url"].rstrip("/")
        self.session = requests.Session()
        self.session.headers.update(build_headers(self.profile))

    @staticmethod
    def register(bot_id: Optional[str], hub_url: str, display_name: Optional[str] = None) -> Dict[str, Any]:
        keypair = generate_keypair()
        response = requests.post(
            f"{hub_url.rstrip('/')}/api/v1/register",
            json={
                "bot_id": bot_id,
                "display_name": display_name,
                "public_key": keypair["public_key"],
            },
            timeout=15,
        )
        response.raise_for_status()
        data = response.json()
        resolved_bot_id = data["bot_id"]

        profiles = load_profiles()
        profiles.setdefault("profiles", {})[resolved_bot_id] = {
            "bot_id": resolved_bot_id,
            "display_name": data["display_name"],
            "hub_url": hub_url.rstrip("/"),
            "token": data["token"],
            "private_key": keypair["private_key"],
            "public_key": keypair["public_key"],
        }
        save_profiles(profiles)
        return data

    def get_agent(self, bot_id: str) -> Dict[str, Any]:
        response = self.session.get(f"{self.hub_url}/api/v1/agents/{bot_id}", timeout=30)
        response.raise_for_status()
        return response.json()

    def list_agents(self, name: Optional[str] = None, limit: int = 50) -> Dict[str, Any]:
        params = {"limit": limit}
        if name:
            params["name"] = name
        response = self.session.get(f"{self.hub_url}/api/v1/agents", params=params, timeout=30)
        response.raise_for_status()
        return response.json()

    def list_contacts(self) -> Dict[str, Any]:
        contacts = get_contact_bucket(self.hub_url)
        return {
            "contacts": [
                {"alias": alias, **value}
                for alias, value in sorted(contacts.items(), key=lambda item: item[0].lower())
            ]
        }

    def add_contact(self, alias: str, target: str) -> Dict[str, Any]:
        alias = alias.strip()
        if not alias:
            raise SystemExit("alias cannot be empty")
        resolved_target = self.resolve_recipient(target)
        agent = self.get_agent(resolved_target)
        contacts = load_contacts()
        bucket = contacts.setdefault("contacts", {}).setdefault(self.hub_url, {})
        bucket[alias] = {
            "bot_id": agent["bot_id"],
            "display_name": agent["display_name"],
            "hub_url": self.hub_url,
        }
        save_contacts(contacts)
        return {"status": "ok", "alias": alias, "bot_id": agent["bot_id"], "display_name": agent["display_name"]}

    def remove_contact(self, alias: str) -> Dict[str, Any]:
        contacts = load_contacts()
        bucket = contacts.setdefault("contacts", {}).setdefault(self.hub_url, {})
        removed = bucket.pop(alias, None)
        if removed is None:
            raise SystemExit(f"contact alias '{alias}' not found")
        save_contacts(contacts)
        return {"status": "ok", "alias": alias}

    def sync_contacts(self, name: Optional[str] = None, limit: int = 200) -> Dict[str, Any]:
        agents = self.list_agents(name=name, limit=limit).get("agents", [])
        contacts = load_contacts()
        contact_book = contacts.setdefault("contacts", {}).setdefault(self.hub_url, {})
        synced = []
        for agent in agents:
            alias = agent["display_name"]
            existing = contact_book.get(alias)
            if existing and existing.get("bot_id") != agent["bot_id"]:
                continue
            contact_book[alias] = {
                "bot_id": agent["bot_id"],
                "display_name": agent["display_name"],
                "hub_url": self.hub_url,
            }
            synced.append({"alias": alias, "bot_id": agent["bot_id"]})
        save_contacts(contacts)
        return {"status": "ok", "synced": synced}

    def resolve_recipient(self, target: str) -> str:
        contacts = get_contact_bucket(self.hub_url)
        if target in contacts:
            return contacts[target]["bot_id"]
        try:
            agent = self.get_agent(target)
            return agent["bot_id"]
        except requests.HTTPError as exc:
            if exc.response is None or exc.response.status_code != 404:
                raise

        matches = self.list_agents(name=target).get("agents", [])
        exact_display = [row for row in matches if row["display_name"] == target]
        if len(exact_display) == 1:
            return exact_display[0]["bot_id"]
        if len(exact_display) > 1:
            choices = ", ".join(row["bot_id"] for row in exact_display)
            raise SystemExit(
                f"recipient name '{target}' is ambiguous; use one of these bot ids: {choices}"
            )
        raise SystemExit(f"recipient '{target}' was not found")

    def _derive_encryption_context(self, peer_public_key: str, salt: bytes) -> bytes:
        return derive_message_key(
            self.profile["private_key"],
            peer_public_key,
            salt,
        )

    def _send_message(
        self,
        receiver: str,
        kind: str,
        subject: Optional[str],
        text: Optional[str],
        file_paths: List[Path],
        install: Optional[Dict[str, Any]] = None,
        requires_ack: bool = True,
        labels: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        if kind != "text":
            raise ValueError("_send_message only supports encrypted text messages")

        resolved_receiver = self.resolve_recipient(receiver)
        receiver_agent = self.get_agent(resolved_receiver)
        salt = os.urandom(16)
        message_key = self._derive_encryption_context(receiver_agent["public_key"], salt)

        attachment_meta = []
        encrypted_files = []
        for index, path in enumerate(file_paths):
            upload_name = f"blob-{index}.bin"
            attachment_meta.append(
                {
                    "upload_name": upload_name,
                    "original_name": path.name,
                }
            )
            encrypted_blob = encrypt_bytes(message_key, path.read_bytes())
            encrypted_files.append(
                (
                    upload_name,
                    json.dumps(encrypted_blob, ensure_ascii=False).encode("utf-8"),
                )
            )

        plaintext_payload = {
            "subject": subject,
            "text": text,
            "install": install or {},
            "labels": labels or [],
            "attachments_meta": attachment_meta,
        }
        encrypted_payload = encrypt_bytes(
            message_key,
            json.dumps(plaintext_payload, ensure_ascii=False).encode("utf-8"),
        )

        metadata = {
            "receiver": resolved_receiver,
            "kind": kind,
            "requires_ack": requires_ack,
            "encrypted_payload": encrypted_payload,
            "encryption": {
                "algorithm": "X25519-HKDF-AESGCM",
                "salt": b64e(salt),
            },
        }

        files = [
            ("attachments", (upload_name, encrypted_bytes, "application/octet-stream"))
            for upload_name, encrypted_bytes in encrypted_files
        ]
        response = self.session.post(
            f"{self.hub_url}/api/v1/messages/send",
            data={"payload_json": json.dumps(metadata, ensure_ascii=False)},
            files=files,
            timeout=120,
        )
        response.raise_for_status()
        return response.json()

    def _send_attachment_message(
        self,
        receiver: str,
        kind: str,
        subject: Optional[str],
        file_paths: List[Path],
        install: Optional[Dict[str, Any]] = None,
        labels: Optional[List[str]] = None,
        requires_ack: bool = True,
    ) -> Dict[str, Any]:
        metadata = {
            "receiver": self.resolve_recipient(receiver),
            "kind": kind,
            "requires_ack": requires_ack,
            "subject": subject,
            "text": None,
            "install": install or {},
            "labels": labels or [],
            "encryption": {"algorithm": "none"},
            "encrypted_payload": None,
        }
        files = [
            ("attachments", (path.name, path.open("rb"), "application/octet-stream"))
            for path in file_paths
        ]
        try:
            response = self.session.post(
                f"{self.hub_url}/api/v1/messages/send",
                data={"payload_json": json.dumps(metadata, ensure_ascii=False)},
                files=files,
                timeout=120,
            )
        finally:
            for _, file_tuple in files:
                file_tuple[1].close()
        response.raise_for_status()
        return response.json()

    def _decrypt_message(self, message: Dict[str, Any]) -> Dict[str, Any]:
        if message["kind"] != "text":
            payload = message.get("payload", {})
            message["subject"] = payload.get("subject")
            message["text"] = payload.get("text")
            return message

        salt = b64d(message["payload"]["encryption"]["salt"])
        peer_public_key = (
            message["receiver_public_key"]
            if message["sender"] == self.bot_id
            else message["sender_public_key"]
        )
        message_key = self._derive_encryption_context(peer_public_key, salt)
        decrypted_payload = json.loads(
            decrypt_bytes(
                message_key,
                message["payload"]["encrypted_payload"]["nonce"],
                message["payload"]["encrypted_payload"]["ciphertext"],
            ).decode("utf-8")
        )
        message["subject"] = decrypted_payload.get("subject")
        message["text"] = decrypted_payload.get("text")
        message["payload"] = {
            "install": decrypted_payload.get("install", {}),
            "labels": decrypted_payload.get("labels", []),
            "encryption_salt": message["payload"]["encryption"]["salt"],
        }
        attachment_meta = decrypted_payload.get("attachments_meta", [])
        for index, attachment in enumerate(message.get("attachments", [])):
            if index < len(attachment_meta):
                attachment["name"] = attachment_meta[index]["original_name"]
                attachment["upload_name"] = attachment_meta[index]["upload_name"]
        return message

    def send_text(
        self,
        receiver: str,
        text: str,
        subject: Optional[str] = None,
        requires_ack: bool = True,
    ) -> Dict[str, Any]:
        return self._send_message(
            receiver=receiver,
            kind="text",
            subject=subject,
            text=text,
            file_paths=[],
            requires_ack=requires_ack,
        )

    def send_file(
        self,
        receiver: str,
        file_path: Path,
        subject: Optional[str] = None,
        target_path: Optional[str] = None,
        kind: str = "file",
    ) -> Dict[str, Any]:
        install = {"target_path": target_path} if target_path else {}
        return self._send_attachment_message(
            receiver=receiver,
            kind=kind,
            subject=subject or f"send {file_path.name}",
            file_paths=[file_path],
            install=install,
            labels=[kind],
            requires_ack=True,
        )

    def send_config(
        self,
        receiver: str,
        config_path: Path,
        target_path: Optional[str],
        subject: Optional[str] = None,
    ) -> Dict[str, Any]:
        return self.send_file(
            receiver=receiver,
            file_path=config_path,
            subject=subject or f"config {config_path.name}",
            target_path=target_path,
            kind="config",
        )

    def send_skill(
        self,
        receiver: str,
        skill_dir: Path,
        target_dir: Optional[str] = None,
        subject: Optional[str] = None,
    ) -> Dict[str, Any]:
        if not skill_dir.is_dir():
            raise SystemExit(f"skill dir not found: {skill_dir}")

        target_dir = target_dir or f"/root/.openclaw/skills/{skill_dir.name}"
        with tempfile.TemporaryDirectory(prefix="robotmail-skill-") as temp_root:
            archive_base = Path(temp_root) / skill_dir.name
            archive_path = shutil.make_archive(
                str(archive_base),
                "zip",
                root_dir=str(skill_dir.parent),
                base_dir=skill_dir.name,
            )
            return self._send_attachment_message(
                receiver=receiver,
                kind="skill",
                subject=subject or f"skill {skill_dir.name}",
                file_paths=[Path(archive_path)],
                install={
                    "target_dir": target_dir,
                    "unpack_archive": True,
                    "archive_format": "zip",
                },
                requires_ack=True,
                labels=["skill"],
            )

    def poll(self, limit: int = 20, include_acked: bool = False) -> Dict[str, Any]:
        response = self.session.get(
            f"{self.hub_url}/api/v1/messages/poll",
            params={"limit": limit, "include_acked": str(include_acked).lower()},
            timeout=30,
        )
        response.raise_for_status()
        data = response.json()
        data["messages"] = [self._decrypt_message(message) for message in data["messages"]]
        return data

    def get_message(self, message_id: str) -> Dict[str, Any]:
        response = self.session.get(
            f"{self.hub_url}/api/v1/messages/{message_id}",
            timeout=30,
        )
        response.raise_for_status()
        return self._decrypt_message(response.json())

    def ack(self, message_id: str, note: Optional[str] = None) -> Dict[str, Any]:
        response = self.session.post(
            f"{self.hub_url}/api/v1/messages/{message_id}/ack",
            json={"note": note},
            timeout=30,
        )
        response.raise_for_status()
        return response.json()

    def download_attachments(self, message: Dict[str, Any], dest_dir: Path) -> List[Path]:
        dest_dir.mkdir(parents=True, exist_ok=True)
        saved_paths: List[Path] = []
        for attachment in message.get("attachments", []):
            response = self.session.get(
                f"{self.hub_url}{attachment['download_url']}",
                timeout=120,
            )
            response.raise_for_status()
            path = dest_dir / attachment["name"]
            if message["kind"] == "text":
                encrypted_blob = json.loads(response.content.decode("utf-8"))
                salt = b64d(message["payload"]["encryption_salt"])
                peer_public_key = (
                    message["receiver_public_key"]
                    if message["sender"] == self.bot_id
                    else message["sender_public_key"]
                )
                message_key = self._derive_encryption_context(peer_public_key, salt)
                plaintext = decrypt_bytes(
                    message_key,
                    encrypted_blob["nonce"],
                    encrypted_blob["ciphertext"],
                )
                path.write_bytes(plaintext)
            else:
                path.write_bytes(response.content)
            saved_paths.append(path)
        return saved_paths

    def install_message(self, message_id: str, dest_dir: Optional[Path] = None) -> Dict[str, Any]:
        message = self.get_message(message_id)
        install = message.get("payload", {}).get("install", {})
        attachments = message.get("attachments", [])

        if not attachments:
            return {"status": "ok", "message_id": message_id, "installed": []}

        work_dir = dest_dir or (CONFIG_HOME / "downloads" / message_id)
        saved_paths = self.download_attachments(message, work_dir)
        installed: List[str] = []

        if message["kind"] == "skill":
            target_dir = Path(install.get("target_dir", work_dir))
            target_dir.parent.mkdir(parents=True, exist_ok=True)
            archive_path = saved_paths[0]
            if target_dir.exists():
                shutil.rmtree(target_dir)
            shutil.unpack_archive(str(archive_path), str(target_dir.parent))
            unpacked_dir = target_dir.parent / archive_path.stem
            if unpacked_dir != target_dir and unpacked_dir.exists():
                if target_dir.exists():
                    shutil.rmtree(target_dir)
                unpacked_dir.rename(target_dir)
            installed.append(str(target_dir))
        else:
            target_path = install.get("target_path")
            for path in saved_paths:
                final_path = Path(target_path) if target_path else (work_dir / path.name)
                final_path.parent.mkdir(parents=True, exist_ok=True)
                if path != final_path:
                    shutil.copy2(path, final_path)
                installed.append(str(final_path))

        return {"status": "ok", "message_id": message_id, "installed": installed}


def print_json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, indent=2, ensure_ascii=False))


def parse_args() -> Any:
    import argparse

    parser = argparse.ArgumentParser(description="RobotMail development client")
    sub = parser.add_subparsers(dest="cmd")

    p_register = sub.add_parser("register", help="register bot and save local profile")
    p_register.add_argument("--bot", help="bot id; if omitted the hub generates one")
    p_register.add_argument("--hub", required=True, help="hub url")
    p_register.add_argument("--name", help="display name")

    p_text = sub.add_parser("send-text", help="send encrypted text message")
    p_text.add_argument("--bot", required=True)
    p_text.add_argument("--to", required=True)
    p_text.add_argument("--text", required=True)
    p_text.add_argument("--subject")
    p_text.add_argument("--no-ack", action="store_true")

    p_file = sub.add_parser("send-file", help="send encrypted file")
    p_file.add_argument("--bot", required=True)
    p_file.add_argument("--to", required=True)
    p_file.add_argument("--file", required=True)
    p_file.add_argument("--subject")
    p_file.add_argument("--target-path")

    p_config = sub.add_parser("send-config", help="send encrypted config file")
    p_config.add_argument("--bot", required=True)
    p_config.add_argument("--to", required=True)
    p_config.add_argument("--file", required=True)
    p_config.add_argument("--target-path")
    p_config.add_argument("--subject")

    p_skill = sub.add_parser("send-skill", help="zip and send encrypted skill dir")
    p_skill.add_argument("--bot", required=True)
    p_skill.add_argument("--to", required=True)
    p_skill.add_argument("--dir", required=True)
    p_skill.add_argument("--target-dir")
    p_skill.add_argument("--subject")

    p_poll = sub.add_parser("poll", help="poll inbox and decrypt messages")
    p_poll.add_argument("--bot", required=True)
    p_poll.add_argument("--limit", type=int, default=20)
    p_poll.add_argument("--include-acked", action="store_true")

    p_get = sub.add_parser("get", help="get one decrypted message")
    p_get.add_argument("--bot", required=True)
    p_get.add_argument("message_id")

    p_ack = sub.add_parser("ack", help="ack a message")
    p_ack.add_argument("--bot", required=True)
    p_ack.add_argument("message_id")
    p_ack.add_argument("--note")

    p_install = sub.add_parser("install-message", help="download, decrypt and install payload")
    p_install.add_argument("--bot", required=True)
    p_install.add_argument("message_id")
    p_install.add_argument("--dest-dir")
    p_install.add_argument("--ack", action="store_true")

    p_agents = sub.add_parser("agents", help="list known agents from the hub")
    p_agents.add_argument("--bot", required=True)
    p_agents.add_argument("--name")
    p_agents.add_argument("--limit", type=int, default=50)

    p_contacts = sub.add_parser("contacts", help="manage local contact aliases")
    p_contacts_sub = p_contacts.add_subparsers(dest="contacts_cmd")

    p_contacts_list = p_contacts_sub.add_parser("list", help="list local contacts")
    p_contacts_list.add_argument("--bot", required=True)

    p_contacts_add = p_contacts_sub.add_parser("add", help="bind alias to a bot id")
    p_contacts_add.add_argument("--bot", required=True)
    p_contacts_add.add_argument("--alias", required=True)
    p_contacts_add.add_argument("--target", required=True, help="exact bot_id to bind")

    p_contacts_remove = p_contacts_sub.add_parser("remove", help="remove a contact alias")
    p_contacts_remove.add_argument("--bot", required=True)
    p_contacts_remove.add_argument("--alias", required=True)

    p_contacts_sync = p_contacts_sub.add_parser("sync", help="sync unique display names into contacts")
    p_contacts_sync.add_argument("--bot", required=True)
    p_contacts_sync.add_argument("--name")
    p_contacts_sync.add_argument("--limit", type=int, default=200)

    sub.add_parser("profiles", help="show saved profiles")

    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.cmd == "register":
        print_json(RobotMailClient.register(args.bot, args.hub, args.name))
        return

    if args.cmd == "profiles":
        print_json(load_profiles())
        return

    if not args.cmd:
        raise SystemExit("missing command")

    bot_for_session = getattr(args, "bot", None)
    client = RobotMailClient(bot_for_session) if bot_for_session else None

    if args.cmd == "send-text":
        print_json(
            client.send_text(
                receiver=args.to,
                text=args.text,
                subject=args.subject,
                requires_ack=not args.no_ack,
            )
        )
    elif args.cmd == "send-file":
        print_json(
            client.send_file(
                receiver=args.to,
                file_path=Path(args.file).expanduser().resolve(),
                subject=args.subject,
                target_path=args.target_path,
            )
        )
    elif args.cmd == "send-config":
        print_json(
            client.send_config(
                receiver=args.to,
                config_path=Path(args.file).expanduser().resolve(),
                target_path=args.target_path,
                subject=args.subject,
            )
        )
    elif args.cmd == "send-skill":
        print_json(
            client.send_skill(
                receiver=args.to,
                skill_dir=Path(args.dir).expanduser().resolve(),
                target_dir=args.target_dir,
                subject=args.subject,
            )
        )
    elif args.cmd == "poll":
        print_json(client.poll(limit=args.limit, include_acked=args.include_acked))
    elif args.cmd == "get":
        print_json(client.get_message(args.message_id))
    elif args.cmd == "ack":
        print_json(client.ack(args.message_id, args.note))
    elif args.cmd == "install-message":
        result = client.install_message(
            args.message_id,
            Path(args.dest_dir).expanduser().resolve() if args.dest_dir else None,
        )
        if args.ack:
            client.ack(args.message_id, "installed")
        print_json(result)
    elif args.cmd == "agents":
        print_json(client.list_agents(name=args.name, limit=args.limit))
    elif args.cmd == "contacts":
        if args.contacts_cmd == "list":
            print_json(client.list_contacts())
        elif args.contacts_cmd == "add":
            print_json(client.add_contact(args.alias, args.target))
        elif args.contacts_cmd == "remove":
            print_json(client.remove_contact(args.alias))
        elif args.contacts_cmd == "sync":
            print_json(client.sync_contacts(name=args.name, limit=args.limit))
        else:
            raise SystemExit("missing contacts subcommand")
    else:
        raise SystemExit(f"unsupported command: {args.cmd}")


if __name__ == "__main__":
    main()
