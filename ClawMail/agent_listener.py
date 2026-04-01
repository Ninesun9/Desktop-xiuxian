#!/usr/bin/env python3
"""
Background listener for RobotMail agents.

Default behavior:
- poll inbox on an interval
- auto-install skill/config/file messages
- optionally ack text messages
- write a local event log
"""
from __future__ import annotations

import argparse
import json
import subprocess
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List


BASE_DIR = Path(__file__).resolve().parent
CLIENT = BASE_DIR / "robotmail-client.py"


def run_client(args: List[str]) -> Dict[str, Any]:
    proc = subprocess.run(
        ["python3", str(CLIENT), *args],
        capture_output=True,
        text=True,
        timeout=300,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or proc.stdout.strip() or "client command failed")
    output = proc.stdout.strip()
    return json.loads(output) if output else {}


def format_display_time(timestamp: str | None) -> str:
    if not timestamp:
        return "未知"
    try:
        normalized = timestamp.replace("Z", "+00:00")
        parsed = datetime.fromisoformat(normalized)
        return parsed.astimezone().strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return timestamp


def notify_openclaw(
    message: Dict[str, Any],
    channel: str | None,
    target: str | None,
    install_result: Dict[str, Any] | None = None,
) -> Dict[str, Any]:
    text = (message.get("text") or "").strip()
    summary = text.replace("\r", " ").replace("\n", " ").strip()
    if len(summary) > 80:
        summary = f"{summary[:77]}..."
    if not summary:
        summary = "无正文摘要"

    display_time = format_display_time(message.get("created_at"))
    kind = (message.get("kind") or "").lower()
    installed = install_result.get("installed", []) if install_result else []

    if kind == "text":
        notice_lines = [
            "你收到一条新的 RobotMail 文本消息。",
            f"发送方：{message['sender']}",
            f"发送时间：{display_time}",
            f"摘要：{summary}",
            f"消息 ID：{message['id']}",
        ]
        if message.get("subject"):
            notice_lines.append(f"主题：{message['subject']}")
        notice_lines.append("如需处理，请在 OpenClaw 中查看详情并决定后续动作。")
    elif kind == "skill":
        installed_path = installed[0] if installed else "未记录安装路径"
        notice_lines = [
            "你收到一个新的 RobotMail Skill。",
            f"发送方：{message['sender']}",
            f"发送时间：{display_time}",
            f"消息 ID：{message['id']}",
            f"安装位置：{installed_path}",
        ]
        if message.get("subject"):
            notice_lines.append(f"主题：{message['subject']}")
        notice_lines.append("Skill 已自动安装，你可以按需检查或手动启用。")
    elif kind in {"file", "config"}:
        installed_text = "，".join(installed) if installed else "未记录文件路径"
        notice_lines = [
            f"你收到一个新的 RobotMail{'配置' if kind == 'config' else '文件'}消息。",
            f"发送方：{message['sender']}",
            f"发送时间：{display_time}",
            f"消息 ID：{message['id']}",
            f"落地路径：{installed_text}",
        ]
        if message.get("subject"):
            notice_lines.append(f"主题：{message['subject']}")
        notice_lines.append("文件已自动保存，如需处理请在 OpenClaw 中继续操作。")
    else:
        notice_lines = [
            "你收到一条新的 RobotMail 消息。",
            f"发送方：{message['sender']}",
            f"发送时间：{display_time}",
            f"消息 ID：{message['id']}",
        ]
        if message.get("subject"):
            notice_lines.append(f"主题：{message['subject']}")

    command = ["openclaw", "message", "send"]
    if channel:
        command.extend(["--channel", channel])
    if target:
        command.extend(["--target", target])
    command.extend(["--message", "\n".join(notice_lines)])

    proc = subprocess.run(
        command,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or proc.stdout.strip() or "openclaw notify failed")

    return {
        "status": "ok",
        "command": command,
        "output": (proc.stdout or proc.stderr).strip(),
    }


def append_log(path: Path, record: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, ensure_ascii=False) + "\n")


def load_state(path: Path) -> Dict[str, Any]:
    if not path.exists():
        return {"processed": {}}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {"processed": {}}


def save_state(path: Path, state: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(state, indent=2, ensure_ascii=False), encoding="utf-8")


def prune_state(state: Dict[str, Any], max_entries: int, max_age_days: int) -> Dict[str, Any]:
    processed = state.setdefault("processed", {})
    now = time.time()
    cutoff = now - (max_age_days * 86400)
    filtered = {
        msg_id: record
        for msg_id, record in processed.items()
        if record.get("ts", 0) >= cutoff
    }
    if len(filtered) > max_entries:
        ordered = sorted(
            filtered.items(),
            key=lambda item: item[1].get("ts", 0),
            reverse=True,
        )[:max_entries]
        filtered = dict(ordered)
    state["processed"] = filtered
    return state


def handle_message(
    bot_id: str,
    message: Dict[str, Any],
    downloads_dir: Path,
    auto_ack_text: bool,
    notify_openclaw_enabled: bool,
    openclaw_channel: str | None,
    openclaw_target: str | None,
    log_path: Path,
    state: Dict[str, Any],
    state_path: Path,
) -> None:
    processed = state.setdefault("processed", {})
    if message["id"] in processed:
        return

    record = {
        "message_id": message["id"],
        "kind": message["kind"],
        "sender": message["sender"],
        "subject": message.get("subject"),
        "created_at": message.get("created_at"),
    }

    if message["kind"] in {"skill", "file", "config"}:
        result = run_client(
            [
                "install-message",
                "--bot",
                bot_id,
                message["id"],
                "--dest-dir",
                str(downloads_dir / message["id"]),
                "--ack",
            ]
        )
        record["action"] = "installed"
        record["result"] = result
        if notify_openclaw_enabled:
            try:
                record["openclaw_notify"] = notify_openclaw(
                    message,
                    openclaw_channel,
                    openclaw_target,
                    install_result=result,
                )
            except Exception as exc:
                record["openclaw_notify_error"] = str(exc)
        append_log(log_path, record)
        processed[message["id"]] = {"status": "installed", "ts": time.time()}
        save_state(state_path, state)
        return

    if message["kind"] == "text":
        record["action"] = "received_text"
        record["text"] = message.get("text")
        if notify_openclaw_enabled:
            try:
                record["openclaw_notify"] = notify_openclaw(message, openclaw_channel, openclaw_target)
            except Exception as exc:
                record["openclaw_notify_error"] = str(exc)
        append_log(log_path, record)
        if auto_ack_text:
            ack = run_client(["ack", "--bot", bot_id, message["id"], "--note", "received"])
            record["ack"] = ack
            append_log(log_path, {"message_id": message["id"], "action": "acked_text"})
        processed[message["id"]] = {"status": "seen_text", "ts": time.time()}
        save_state(state_path, state)
        return

    record["action"] = "ignored"
    append_log(log_path, record)
    processed[message["id"]] = {"status": "ignored", "ts": time.time()}
    save_state(state_path, state)


def listener_loop(
    bot_id: str,
    interval: int,
    downloads_dir: Path,
    auto_ack_text: bool,
    notify_openclaw_enabled: bool,
    openclaw_channel: str | None,
    openclaw_target: str | None,
    log_path: Path,
    state_path: Path,
    state_max_entries: int,
    state_max_age_days: int,
    once: bool,
) -> None:
    state = load_state(state_path)
    state = prune_state(state, state_max_entries, state_max_age_days)
    save_state(state_path, state)
    cycles = 0
    while True:
        try:
            payload = run_client(["poll", "--bot", bot_id, "--limit", "50"])
            for message in payload.get("messages", []):
                handle_message(
                    bot_id=bot_id,
                    message=message,
                    downloads_dir=downloads_dir,
                    auto_ack_text=auto_ack_text,
                    notify_openclaw_enabled=notify_openclaw_enabled,
                    openclaw_channel=openclaw_channel,
                    openclaw_target=openclaw_target,
                    log_path=log_path,
                    state=state,
                    state_path=state_path,
                )
        except Exception as exc:
            append_log(
                log_path,
                {"action": "listener_error", "error": str(exc), "bot_id": bot_id},
            )

        cycles += 1
        if cycles >= 20:
            state = prune_state(state, state_max_entries, state_max_age_days)
            save_state(state_path, state)
            cycles = 0

        if once:
            return
        time.sleep(interval)


def parse_args() -> Any:
    parser = argparse.ArgumentParser(description="RobotMail agent listener")
    parser.add_argument("--bot", required=True, help="local bot id")
    parser.add_argument("--interval", type=int, default=15, help="poll interval in seconds")
    parser.add_argument(
        "--downloads-dir",
        default=str(Path.home() / ".robotmail" / "listener-downloads"),
        help="working directory for downloaded attachments",
    )
    parser.add_argument(
        "--log-file",
        default=str(Path.home() / ".robotmail" / "listener.log"),
        help="newline-delimited JSON log file",
    )
    parser.add_argument(
        "--state-file",
        default=str(Path.home() / ".robotmail" / "listener-state.json"),
        help="processed message state file",
    )
    parser.add_argument("--state-max-entries", type=int, default=5000, help="max retained processed message records")
    parser.add_argument("--state-max-age-days", type=int, default=30, help="retention window for processed message records")
    parser.add_argument("--ack-text", action="store_true", help="ack text messages automatically")
    parser.add_argument("--notify-openclaw", action="store_true", help="notify OpenClaw when a text message is received")
    parser.add_argument("--openclaw-channel", help="optional OpenClaw message channel, e.g. telegram")
    parser.add_argument("--openclaw-target", help="optional OpenClaw message target, e.g. @mychat")
    parser.add_argument("--once", action="store_true", help="run one poll cycle and exit")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    listener_loop(
        bot_id=args.bot,
        interval=max(1, args.interval),
        downloads_dir=Path(args.downloads_dir).expanduser().resolve(),
        auto_ack_text=args.ack_text,
        notify_openclaw_enabled=args.notify_openclaw,
        openclaw_channel=args.openclaw_channel,
        openclaw_target=args.openclaw_target,
        log_path=Path(args.log_file).expanduser().resolve(),
        state_path=Path(args.state_file).expanduser().resolve(),
        state_max_entries=max(100, args.state_max_entries),
        state_max_age_days=max(1, args.state_max_age_days),
        once=args.once,
    )


if __name__ == "__main__":
    main()
