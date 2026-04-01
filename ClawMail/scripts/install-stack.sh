#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
SYSTEMD_DIR="${PROJECT_DIR}/systemd"

ROLE="all"
BOT_ID=""
DISPLAY_NAME=""
HUB_URL="http://127.0.0.1:5180"
LISTENER_INTERVAL="15"
PORT="5180"
ROBOTMAIL_HOME="/root/.robotmail-hub"
CLIENT_HOME="/root/.robotmail"
DOMAIN=""
ENABLE_HUB="auto"
ENABLE_LISTENER="auto"
ENABLE_HEARTBEAT="yes"
REGISTER_BOT="yes"
CRON_FILE="/etc/cron.d/robotmail-heartbeat"

usage() {
  cat <<'EOF'
Usage:
  install-stack.sh [options]

Options:
  --role ROLE                 Install role: hub, agent, or all (default: all)
  --bot BOT_ID                Local bot id for listener/registration
  --name DISPLAY_NAME         Human-readable display name
  --hub-url URL               Hub URL for agent registration (default: http://127.0.0.1:5180)
  --port PORT                 Hub listen port for local hub (default: 5180)
  --listener-interval SEC     Listener poll interval (default: 15)
  --robotmail-home PATH       Hub data directory (default: /root/.robotmail-hub)
  --client-home PATH          Client config directory (default: /root/.robotmail)
  --domain DOMAIN             Optional public domain, used for output hints only
  --skip-register             Do not register the bot during install
  --skip-heartbeat            Do not install heartbeat script/timer
  --help                      Show this help

Examples:
  install-stack.sh
  install-stack.sh --bot openclaw-prod-01
  install-stack.sh --role agent --bot openclaw-prod-01 --hub-url https://robotmail.example.com
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --role) ROLE="$2"; shift 2 ;;
    --bot) BOT_ID="$2"; shift 2 ;;
    --name) DISPLAY_NAME="$2"; shift 2 ;;
    --hub-url) HUB_URL="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --listener-interval) LISTENER_INTERVAL="$2"; shift 2 ;;
    --robotmail-home) ROBOTMAIL_HOME="$2"; shift 2 ;;
    --client-home) CLIENT_HOME="$2"; shift 2 ;;
    --domain) DOMAIN="$2"; shift 2 ;;
    --skip-register) REGISTER_BOT="no"; shift ;;
    --skip-heartbeat) ENABLE_HEARTBEAT="no"; shift ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage; exit 1 ;;
  esac
done

if [[ "${EUID}" -ne 0 ]]; then
  echo "Please run as root." >&2
  exit 1
fi

if [[ "${ROLE}" != "hub" && "${ROLE}" != "agent" && "${ROLE}" != "all" ]]; then
  echo "Invalid role: ${ROLE}" >&2
  exit 1
fi

if [[ -z "${BOT_ID}" && ( "${ROLE}" == "agent" || "${ROLE}" == "all" ) ]]; then
  read -r -p "Bot name: " BOT_ID
  BOT_ID="${BOT_ID// /-}"
  if [[ -z "${BOT_ID}" ]]; then
    echo "Bot name is required." >&2
    exit 1
  fi
fi

if [[ -z "${DISPLAY_NAME}" && -n "${BOT_ID}" ]]; then
  DISPLAY_NAME="${BOT_ID}"
fi

if [[ "${ROLE}" == "agent" || "${ROLE}" == "all" ]]; then
  if [[ -z "${HUB_URL}" ]]; then
    echo "--hub-url is required for role ${ROLE}" >&2
    exit 1
  fi
fi

mkdir -p /etc/robotmail
mkdir -p "${CLIENT_HOME}"
mkdir -p /root/.openclaw
mkdir -p /root/.openclaw/skills

python3 -m pip install -r "${PROJECT_DIR}/requirements.txt"

install_hub() {
  cat > /etc/robotmail/hub.env <<EOF
ROBOTMAIL_HOME=${ROBOTMAIL_HOME}
ROBOTMAIL_MAX_ATTACHMENT_BYTES=$((25 * 1024 * 1024))
ROBOTMAIL_ALLOWED_ATTACHMENT_TYPES=application/octet-stream,application/json,application/zip,application/x-zip-compressed,text/plain,text/markdown,application/x-sh,application/gzip
EOF

  cat > /etc/systemd/system/robotmail-hub.service <<EOF
[Unit]
Description=RobotMail Hub
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=${PROJECT_DIR}
EnvironmentFile=/etc/robotmail/hub.env
ExecStart=/usr/bin/python3 ${PROJECT_DIR}/robotmail-server.py --host 0.0.0.0 --port ${PORT}
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
}

install_listener() {
  cat > /etc/robotmail/listener-${BOT_ID}.env <<EOF
ROBOTMAIL_CLIENT_HOME=${CLIENT_HOME}
EOF

  cat > /etc/systemd/system/robotmail-listener@.service <<EOF
[Unit]
Description=RobotMail Listener for %i
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=${PROJECT_DIR}
EnvironmentFile=/etc/robotmail/listener-%i.env
ExecStart=/usr/bin/python3 ${PROJECT_DIR}/agent_listener.py --bot %i --interval ${LISTENER_INTERVAL} --ack-text
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
}

install_heartbeat() {
  cat > /root/.openclaw/heartbeat-robotmail.sh <<EOF
#!/usr/bin/env bash
set -euo pipefail

LOG_TAG="robotmail-heartbeat"
STATUS=0

if systemctl list-unit-files | grep -q '^robotmail-hub.service'; then
  if ! systemctl is-active --quiet robotmail-hub.service; then
    echo "[\$(date -Is)] robotmail-hub.service is not active"
    STATUS=1
  fi
fi

if [[ -n "${BOT_ID}" ]] && systemctl list-unit-files | grep -q '^robotmail-listener@.service'; then
  if ! systemctl is-active --quiet robotmail-listener@${BOT_ID}.service; then
    echo "[\$(date -Is)] robotmail-listener@${BOT_ID}.service is not active"
    STATUS=1
  fi
fi

if [[ -n "${HUB_URL}" ]]; then
  if ! curl -fsS --max-time 10 "${HUB_URL%/}/api/v1/health" >/dev/null; then
    echo "[\$(date -Is)] hub health check failed: ${HUB_URL%/}/api/v1/health"
    STATUS=1
  fi
fi

if [[ \${STATUS} -eq 0 ]]; then
  echo "[\$(date -Is)] robotmail heartbeat ok"
fi
exit \${STATUS}
EOF
  chmod +x /root/.openclaw/heartbeat-robotmail.sh

  cat > "${CRON_FILE}" <<EOF
SHELL=/bin/bash
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
*/5 * * * * root /root/.openclaw/heartbeat-robotmail.sh >> /var/log/robotmail-heartbeat.log 2>&1
EOF
  chmod 0644 "${CRON_FILE}"
}

register_bot() {
  local register_args=(register --bot "${BOT_ID}" --hub "${HUB_URL}")
  if [[ -n "${DISPLAY_NAME}" ]]; then
    register_args+=(--name "${DISPLAY_NAME}")
  fi
  ROBOTMAIL_CLIENT_HOME="${CLIENT_HOME}" python3 "${PROJECT_DIR}/robotmail-client.py" "${register_args[@]}"
}

install_skill() {
  mkdir -p /root/.openclaw/skills/robotmail
  install -m 0644 "${PROJECT_DIR}/robotmail_skill.py" /root/.openclaw/skills/robotmail/robotmail_skill.py
  cat > /root/.openclaw/skills/robotmail/SKILL.md <<'EOF'
# RobotMail

## Purpose
Use RobotMail to send text, config files, and skills between OpenClaw nodes.

## Hints
- Ask to "send" or "mail" a message to a contact.
- Ask to check the mailbox or inbox.
- Contacts are local aliases managed by the operator.
EOF
}

if [[ "${ROLE}" == "hub" || "${ROLE}" == "all" ]]; then
  install_hub
fi

if [[ "${ROLE}" == "agent" || "${ROLE}" == "all" ]]; then
  install_listener
  install_skill
fi

if [[ "${ENABLE_HEARTBEAT}" == "yes" ]]; then
  install_heartbeat
fi

systemctl daemon-reload

if [[ "${ROLE}" == "hub" || "${ROLE}" == "all" ]]; then
  systemctl enable --now robotmail-hub.service
fi

if [[ "${ROLE}" == "agent" || "${ROLE}" == "all" ]]; then
  if [[ "${REGISTER_BOT}" == "yes" ]]; then
    register_bot
  fi
  systemctl enable --now "robotmail-listener@${BOT_ID}.service"
fi

cat <<EOF

Install complete.

Project directory: ${PROJECT_DIR}
Client home: ${CLIENT_HOME}
Hub data home: ${ROBOTMAIL_HOME}
Role: ${ROLE}
EOF

if [[ -n "${BOT_ID}" ]]; then
  echo "Bot ID: ${BOT_ID}"
fi
if [[ -n "${HUB_URL}" ]]; then
  echo "Hub URL: ${HUB_URL}"
fi
if [[ -n "${DOMAIN}" ]]; then
  echo "Domain hint: ${DOMAIN}"
fi

echo
echo "Useful checks:"
echo "  systemctl status robotmail-hub.service"
if [[ -n "${BOT_ID}" ]]; then
  echo "  systemctl status robotmail-listener@${BOT_ID}.service"
fi
echo "  grep robotmail-heartbeat ${CRON_FILE}"
echo "  /root/.openclaw/heartbeat-robotmail.sh"
