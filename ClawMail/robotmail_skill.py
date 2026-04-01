#!/usr/bin/env python3
"""
Natural-language RobotMail adapter for OpenClaw.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path


DEFAULT_BOT_ID = "openclaw"


def resolve_client_path() -> Path:
    skill_dir = Path(__file__).resolve().parent
    candidates = [
        Path(__file__).resolve().parent / "robotmail-client.py",
        Path("/opt/robotmail/robotmail-client.py"),
        Path("/root/mailbox_hub/robotmail/robotmail-client.py"),
        skill_dir.parent / "robotmail-client.py",
    ]

    env_value = os.environ.get("ROBOTMAIL_CLIENT")
    env_client = Path(env_value) if env_value else None
    if env_client is not None:
        candidates.insert(0, env_client)

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError("robotmail-client.py not found; set ROBOTMAIL_CLIENT or install the client first")


def run_client(args: list[str]) -> str:
    client = resolve_client_path()
    result = subprocess.run(
        [sys.executable, str(client), *args],
        capture_output=True,
        text=True,
        timeout=180,
    )
    return (result.stdout or result.stderr).strip()


def is_robotmail_request(text: str) -> bool:
    keywords = [
        "联系人",
        "通讯录",
        "地址簿",
        "发给",
        "发送给",
        "传给",
        "寄给",
        "转给",
        "发一句话",
        "发个消息",
        "发条消息",
        "捎句话",
        "发文件",
        "传文件",
        "传个文件",
        "上传文件",
        "发skill",
        "发技能",
        "传skill",
        "传技能",
        "邮箱",
        "收件箱",
        "信箱",
        "邮件",
        "查邮件",
        "查收件箱",
        "clawmail",
        "robotmail",
    ]
    lowered = text.lower()
    return any(keyword.lower() in lowered for keyword in keywords)


def handle_add_contact(text: str, bot_id: str) -> str | None:
    patterns = [
        r"把\s*(.+?)\s*加入联系人[，,\s]*对应\s*(\S+)",
        r"添加联系人\s*(.+?)[，,\s]*对应\s*(\S+)",
        r"把\s*(.+?)\s*设为联系人[，,\s]*对应\s*(\S+)",
        r"把\s*(.+?)\s*加入通讯录[，,\s]*对应\s*(\S+)",
        r"添加通讯录\s*(.+?)[，,\s]*对应\s*(\S+)",
        r"把\s*(.+?)\s*记为联系人[，,\s]*对应\s*(\S+)",
    ]
    for pattern in patterns:
        match = re.search(pattern, text)
        if match:
            alias = match.group(1).strip()
            target = match.group(2).strip()
            return run_client(["contacts", "add", "--bot", bot_id, "--alias", alias, "--target", target])
    return None


def handle_remove_contact(text: str, bot_id: str) -> str | None:
    patterns = [
        r"删除联系人\s*(\S+)",
        r"移除联系人\s*(\S+)",
        r"把联系人\s*(\S+)\s*删掉",
        r"删除通讯录\s*(\S+)",
        r"把\s*(\S+)\s*从联系人里删掉",
        r"把\s*(\S+)\s*从通讯录里移除",
    ]
    for pattern in patterns:
        match = re.search(pattern, text)
        if match:
            alias = match.group(1).strip()
            return run_client(["contacts", "remove", "--bot", bot_id, "--alias", alias])
    return None


def handle_list_contacts(text: str, bot_id: str) -> str | None:
    if re.search(r"(查看|看看|列出|显示|打开|检查).*(联系人|通讯录|地址簿)", text):
        return run_client(["contacts", "list", "--bot", bot_id])
    return None


def handle_inbox(text: str, bot_id: str) -> str | None:
    if re.search(r"(查看|看看|检查|查一下|打开|读取).*(邮箱|收件箱|信箱|邮件)", text):
        return run_client(["poll", "--bot", bot_id, "--limit", "20"])
    return None


def handle_send_text(text: str, bot_id: str) -> str | None:
    patterns = [
        r"给\s*(.+?)\s*发一句话[:：]?\s*(.+)",
        r"给\s*(.+?)\s*发送[:：]?\s*(.+)",
        r"给\s*(.+?)\s*发消息[:：]?\s*(.+)",
        r"给\s*(.+?)\s*发个消息[:：]?\s*(.+)",
        r"给\s*(.+?)\s*发条消息[:：]?\s*(.+)",
        r"给\s*(.+?)\s*捎句话[:：]?\s*(.+)",
        r"给\s*(.+?)\s*带句话[:：]?\s*(.+)",
        r"告诉\s*(.+?)\s+(.+)",
        r"跟\s*(.+?)\s*说[:：]?\s*(.+)",
        r"把这句话发给\s*(.+?)[:：]?\s*(.+)",
    ]
    for pattern in patterns:
        match = re.search(pattern, text)
        if match:
            target = match.group(1).strip()
            content = match.group(2).strip()
            return run_client(["send-text", "--bot", bot_id, "--to", target, "--text", content])
    return None


def handle_send_file_or_skill(text: str, bot_id: str) -> str | None:
    patterns = [
        (r"给\s*(.+?)\s*发\s*skill\s+(\S+)", "send-skill", "--dir"),
        (r"给\s*(.+?)\s*发送\s*skill\s+(\S+)", "send-skill", "--dir"),
        (r"给\s*(.+?)\s*传\s*skill\s+(\S+)", "send-skill", "--dir"),
        (r"给\s*(.+?)\s*发技能\s+(\S+)", "send-skill", "--dir"),
        (r"给\s*(.+?)\s*传技能\s+(\S+)", "send-skill", "--dir"),
        (r"把\s*(\S+)\s*这个\s*skill\s*发给\s*(.+)", "send-skill", "--dir-reverse"),
        (r"把\s*(\S+)\s*这个\s*技能\s*发给\s*(.+)", "send-skill", "--dir-reverse"),
        (r"给\s*(.+?)\s*发文件\s+(\S+)", "send-file", "--file"),
        (r"给\s*(.+?)\s*发送文件\s+(\S+)", "send-file", "--file"),
        (r"给\s*(.+?)\s*传文件\s+(\S+)", "send-file", "--file"),
        (r"给\s*(.+?)\s*上传文件\s+(\S+)", "send-file", "--file"),
        (r"把文件\s*(\S+)\s*发给\s*(.+)", "send-file", "--file-reverse"),
        (r"把文件\s*(\S+)\s*传给\s*(.+)", "send-file", "--file-reverse"),
    ]
    for pattern, command, mode in patterns:
        match = re.search(pattern, text, re.IGNORECASE)
        if not match:
            continue

        if mode == "--dir":
            target = match.group(1).strip()
            path = match.group(2).strip()
            return run_client([command, "--bot", bot_id, "--to", target, "--dir", path])

        if mode == "--dir-reverse":
            path = match.group(1).strip()
            target = match.group(2).strip()
            return run_client([command, "--bot", bot_id, "--to", target, "--dir", path])

        if mode == "--file":
            target = match.group(1).strip()
            path = match.group(2).strip()
            return run_client([command, "--bot", bot_id, "--to", target, "--file", path])

        if mode == "--file-reverse":
            path = match.group(1).strip()
            target = match.group(2).strip()
            return run_client([command, "--bot", bot_id, "--to", target, "--file", path])
    return None


def handle_request(text: str, bot_id: str = DEFAULT_BOT_ID) -> str:
    text = text.strip()

    for handler in [
        handle_add_contact,
        handle_remove_contact,
        handle_list_contacts,
        handle_inbox,
        handle_send_file_or_skill,
        handle_send_text,
    ]:
        result = handler(text, bot_id)
        if result:
            return result

    return json.dumps(
        {
            "status": "ignored",
            "message": "无法识别为 RobotMail 指令",
        },
        ensure_ascii=False,
    )


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("usage: robotmail_skill.py <request> [--bot BOT_ID]")

    args = sys.argv[1:]
    bot_id = DEFAULT_BOT_ID
    if "--bot" in args:
        index = args.index("--bot")
        if index + 1 >= len(args):
            raise SystemExit("--bot requires a value")
        bot_id = args[index + 1]
        del args[index:index + 2]

    print(handle_request(" ".join(args), bot_id=bot_id))


if __name__ == "__main__":
    main()
