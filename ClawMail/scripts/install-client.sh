#!/usr/bin/env bash
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

HUB_URL="${HUB_URL:-http://107.174.220.99:5180}"
DOWNLOAD_BASE="${DOWNLOAD_BASE:-${HUB_URL%/}}"
INSTALL_DIR="${INSTALL_DIR:-/opt/robotmail}"
SKILL_DIR="${SKILL_DIR:-${HOME}/.openclaw/skills/robotmail}"
REGISTER_BOT="${REGISTER_BOT:-yes}"
INSTALL_LISTENER="${INSTALL_LISTENER:-yes}"
LISTENER_INTERVAL="${LISTENER_INTERVAL:-15}"
LISTENER_ACK_TEXT="${LISTENER_ACK_TEXT:-yes}"
OPENCLAW_NOTIFY="${OPENCLAW_NOTIFY:-yes}"
OPENCLAW_NOTIFY_CHANNEL="${OPENCLAW_NOTIFY_CHANNEL:-}"
OPENCLAW_NOTIFY_TARGET="${OPENCLAW_NOTIFY_TARGET:-}"
BOT_ID="${BOT_ID:-}"
DISPLAY_NAME="${DISPLAY_NAME:-}"
ACTIVE_BOT_ID="${BOT_ID:-}"

echo -e "${GREEN}RobotMail OpenClaw 安装脚本${NC}"
echo "================================"

if [[ "${EUID}" -ne 0 ]]; then
    echo -e "${RED}请使用 root 用户运行。${NC}"
    exit 1
fi

echo -e "\n${YELLOW}正在安装系统依赖...${NC}"
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y curl python3 python3-pip python3-venv

echo -e "\n${YELLOW}正在准备目录...${NC}"
mkdir -p "${INSTALL_DIR}" "${SKILL_DIR}" "${HOME}/.robotmail"

echo -e "\n${YELLOW}正在下载客户端、监听器和 Skill...${NC}"
curl -fsSL "${DOWNLOAD_BASE}/downloads/robotmail-client.py" -o "${INSTALL_DIR}/robotmail-client.py"
curl -fsSL "${DOWNLOAD_BASE}/downloads/agent_listener.py" -o "${INSTALL_DIR}/agent_listener.py"
curl -fsSL "${DOWNLOAD_BASE}/downloads/robotmail_skill.py" -o "${SKILL_DIR}/robotmail_skill.py"
curl -fsSL "${DOWNLOAD_BASE}/downloads/SKILL.md" -o "${SKILL_DIR}/SKILL.md"
cp "${INSTALL_DIR}/robotmail-client.py" "${SKILL_DIR}/robotmail-client.py"
chmod 0755 "${INSTALL_DIR}/robotmail-client.py" "${INSTALL_DIR}/agent_listener.py" "${SKILL_DIR}/robotmail_skill.py" "${SKILL_DIR}/robotmail-client.py"

echo -e "\n${YELLOW}正在检查 Hub 健康状态...${NC}"
curl -fsSL "${HUB_URL%/}/api/v1/health"

if [[ "${REGISTER_BOT}" == "yes" ]]; then
    echo -e "\n${YELLOW}正在注册 Bot...${NC}"
    register_args=(register --hub "${HUB_URL}")
    if [[ -n "${BOT_ID}" ]]; then
        register_args+=(--bot "${BOT_ID}")
    fi
    if [[ -n "${DISPLAY_NAME}" ]]; then
        register_args+=(--name "${DISPLAY_NAME}")
    fi

    register_output="$(python3 "${INSTALL_DIR}/robotmail-client.py" "${register_args[@]}")"
    echo "${register_output}"
    ACTIVE_BOT_ID="$(REGISTER_OUTPUT="${register_output}" python3 - <<'PY'
import json
import os

payload = json.loads(os.environ["REGISTER_OUTPUT"])
print(payload["bot_id"])
PY
)"
elif [[ -z "${ACTIVE_BOT_ID}" && -f "${HOME}/.robotmail/profiles.json" ]]; then
    ACTIVE_BOT_ID="$(python3 - <<'PY'
import json
from pathlib import Path

path = Path.home() / '.robotmail' / 'profiles.json'
data = json.loads(path.read_text(encoding='utf-8'))
profiles = data.get('profiles', {})
if len(profiles) == 1:
    print(next(iter(profiles)))
PY
)"
fi

if [[ "${INSTALL_LISTENER}" == "yes" ]]; then
    if [[ -z "${ACTIVE_BOT_ID}" ]]; then
        echo -e "${RED}无法确定要绑定 listener 的 Bot ID。请先完成注册，或显式传入 BOT_ID。${NC}"
        exit 1
    fi

    echo -e "\n${YELLOW}正在安装 listener 后台服务...${NC}"
    ACK_FLAG=""
    if [[ "${LISTENER_ACK_TEXT}" == "yes" ]]; then
        ACK_FLAG="--ack-text"
    fi

    NOTIFY_FLAG=""
    if [[ "${OPENCLAW_NOTIFY}" == "yes" ]]; then
        NOTIFY_FLAG="--notify-openclaw"
    fi

    CHANNEL_FLAG=""
    if [[ -n "${OPENCLAW_NOTIFY_CHANNEL}" ]]; then
        CHANNEL_FLAG="--openclaw-channel ${OPENCLAW_NOTIFY_CHANNEL}"
    fi

    TARGET_FLAG=""
    if [[ -n "${OPENCLAW_NOTIFY_TARGET}" ]]; then
        TARGET_FLAG="--openclaw-target ${OPENCLAW_NOTIFY_TARGET}"
    fi

    cat > /etc/systemd/system/robotmail-listener@.service <<EOF
[Unit]
Description=RobotMail Listener for %i
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=${INSTALL_DIR}
ExecStart=/usr/bin/python3 ${INSTALL_DIR}/agent_listener.py --bot %i --interval ${LISTENER_INTERVAL} ${ACK_FLAG} ${NOTIFY_FLAG} ${CHANNEL_FLAG} ${TARGET_FLAG}
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    systemctl enable --now "robotmail-listener@${ACTIVE_BOT_ID}.service"
    echo -e "${GREEN}listener 服务已启用并设置为开机自启。${NC}"
fi

echo -e "\n${GREEN}安装完成。${NC}"
echo "客户端路径: ${INSTALL_DIR}/robotmail-client.py"
echo "Skill 路径: ${SKILL_DIR}/robotmail_skill.py"
echo "Hub 地址: ${HUB_URL}"
if [[ "${OPENCLAW_NOTIFY}" == "yes" ]]; then
    echo "OpenClaw 通知: 已启用"
fi
if [[ -n "${OPENCLAW_NOTIFY_CHANNEL}" ]]; then
    echo "OpenClaw 通知通道: ${OPENCLAW_NOTIFY_CHANNEL}"
fi
if [[ -n "${OPENCLAW_NOTIFY_TARGET}" ]]; then
    echo "OpenClaw 通知目标: ${OPENCLAW_NOTIFY_TARGET}"
fi

if [[ -n "${ACTIVE_BOT_ID}" ]]; then
    echo "Bot ID: ${ACTIVE_BOT_ID}"
fi

echo
echo "常用命令示例:"
echo "  python3 ${INSTALL_DIR}/robotmail-client.py register --hub ${HUB_URL}"
echo "  python3 ${INSTALL_DIR}/robotmail-client.py contacts list --bot ${ACTIVE_BOT_ID:-<bot_id>}"
echo "  python3 ${INSTALL_DIR}/robotmail-client.py poll --bot ${ACTIVE_BOT_ID:-<bot_id>}"
echo "  curl -fsSL ${DOWNLOAD_BASE}/install-client.sh | bash"
echo "  curl -fsSL ${DOWNLOAD_BASE}/install-client.sh | OPENCLAW_NOTIFY_CHANNEL=telegram OPENCLAW_NOTIFY_TARGET=@mychat bash"
