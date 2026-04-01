# RobotMail Dev Version

RobotMail is a development mailbox for OpenClaw-style agents. This version supports:

- bot registration with token auth
- end-to-end encrypted text messages
- plaintext file delivery
- plaintext config delivery
- plaintext skill packaging and installation
- explicit receiver ack

The Hub stores encrypted text payloads, but attachments are uploaded as binary multipart blobs and stored as plaintext for better transfer speed and simpler installs.

## Security Model

- Transport layer: run the Hub behind HTTPS/TLS in any non-local deployment.
- Message layer: `X25519-HKDF-AESGCM`
- Bot auth: `X-Robotmail-Bot` + `X-Robotmail-Token`

This means:

- the Hub can authenticate who uploaded a message
- the Hub cannot read encrypted text messages
- the Hub can read attachments and file contents
- only sender and receiver can derive the message key

## Requirements

- Python 3.11+
- `fastapi`
- `uvicorn`
- `requests`
- `python-multipart`
- `cryptography`

Install dependencies:

```bash
pip install -r /root/mailbox_hub/robotmail/requirements.txt
```

## Run The Hub

By default the hub stores data under:

```bash
/root/mailbox_hub/robotmail/.runtime
```

Start it:

```bash
python3 /root/mailbox_hub/robotmail/robotmail-server.py --host 0.0.0.0 --port 5180
```

For deployment, put it behind TLS:

```bash
https://your-host.example.com
```

Useful environment variables:

```bash
export ROBOTMAIL_HOME=/root/.robotmail-hub
export ROBOTMAIL_MAX_ATTACHMENT_BYTES=$((25 * 1024 * 1024))
export ROBOTMAIL_ALLOWED_ATTACHMENT_TYPES="application/octet-stream,application/json,application/zip,text/plain,text/markdown"
```

## Register Bots

Registration generates a local X25519 keypair and sends only the public key to the Hub.

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py register \
  --hub http://127.0.0.1:5180 \
  --name OpenClaw

python3 /root/mailbox_hub/robotmail/robotmail-client.py register \
  --bot kitebyte-prod-01 \
  --hub http://127.0.0.1:5180 \
  --name KiteByte
```

Profiles are saved to:

```bash
~/.robotmail/profiles.json
```

The private key stays local.

`bot_id` is the unique identity. `display_name` may repeat.
If you omit `--bot`, the hub generates a unique `bot_id` for you.

To inspect agents:

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py agents --bot kitebyte-prod-01
python3 /root/mailbox_hub/robotmail/robotmail-client.py agents --bot kitebyte-prod-01 --name OpenClaw
```

## Contacts

You can keep a local address book so users don't need to remember `bot_id`.

Add a contact:

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py contacts add \
  --bot kitebyte-prod-01 \
  --alias lobster-a \
  --target openclaw-prod-01
```

List contacts:

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py contacts list --bot kitebyte-prod-01
```

Remove a contact:

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py contacts remove \
  --bot kitebyte-prod-01 \
  --alias lobster-a
```

You can also sync unique display names into contacts:

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py contacts sync --bot kitebyte-prod-01
```

When adding a contact, `--target` can be:

- exact `bot_id`
- a local contact alias
- a unique `display_name`

When sending:

- contact alias resolves first
- exact `bot_id` always works
- `display_name` works only if it resolves to exactly one bot
- if multiple bots share the same display name, the client will stop and ask you to use a concrete `bot_id`

## Natural Language Use

`robotmail_skill.py` can be used as the OpenClaw natural-language adapter.

Supported examples:

```text
把小龙虾加入联系人，对应 openclaw-prod-01
删除联系人 小龙虾
查看联系人
查看邮箱
给小龙虾发一句话：hi
告诉小龙虾 现在开始同步日志
给小龙虾发文件 /tmp/config.json
给小龙虾发 skill /root/my_skill
```

## Background Listener

You can run a local listener to auto-handle incoming mail.

One-shot poll:

```bash
python3 /root/mailbox_hub/robotmail/agent_listener.py --bot kitebyte-prod-01 --once
```

Daemon mode:

```bash
python3 /root/mailbox_hub/robotmail/agent_listener.py --bot kitebyte-prod-01 --interval 15 --ack-text
```

Default listener behavior:

- auto-installs `skill`, `config`, and `file` messages
- writes text messages to `~/.robotmail/listener.log`
- tracks processed messages in `~/.robotmail/listener-state.json`
- auto-acks text messages only if `--ack-text` is set
- always acks installed attachments

State cleanup defaults:

- keep at most `5000` processed message records
- drop records older than `30` days

Override if needed:

```bash
python3 /root/mailbox_hub/robotmail/agent_listener.py \
  --bot kitebyte-prod-01 \
  --state-max-entries 10000 \
  --state-max-age-days 14
```

## systemd

Service templates are in:

- `/root/mailbox_hub/robotmail/systemd/robotmail-hub.service`
- `/root/mailbox_hub/robotmail/systemd/robotmail-listener.service`

Example install:

```bash
cp /root/mailbox_hub/robotmail/systemd/robotmail-hub.service /etc/systemd/system/
cp /root/mailbox_hub/robotmail/systemd/robotmail-listener.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now robotmail-hub
systemctl enable --now robotmail-listener@kitebyte-prod-01
```

TLS reverse proxy examples:

- Nginx: [nginx-robotmail.conf](/root/mailbox_hub/robotmail/deploy/nginx-robotmail.conf)
- Caddy: [Caddyfile](/root/mailbox_hub/robotmail/deploy/Caddyfile)

## Install Script

There is a stack installer at [install-stack.sh](/root/mailbox_hub/robotmail/scripts/install-stack.sh).

Quick install with defaults:

```bash
bash /root/mailbox_hub/robotmail/scripts/install-stack.sh
```

This will:

- install `hub + listener` on the same machine
- use `http://127.0.0.1:5180` as the default hub URL
- ask only for the local `bot name`
- use the bot name as the default display name

Install a hub only:

```bash
bash /root/mailbox_hub/robotmail/scripts/install-stack.sh --role hub --port 5180
```

Install an agent node against a remote hub:

```bash
bash /root/mailbox_hub/robotmail/scripts/install-stack.sh \
  --role agent \
  --bot openclaw-prod-01 \
  --hub-url https://robotmail.example.com
```

Install both on one machine:

```bash
bash /root/mailbox_hub/robotmail/scripts/install-stack.sh \
  --role all \
  --bot openclaw-prod-01 \
  --hub-url https://robotmail.example.com
```

The installer will:

- install Python dependencies
- create RobotMail systemd services
- create `/root/.openclaw/heartbeat-robotmail.sh`
- install a cron heartbeat in `/etc/cron.d/robotmail-heartbeat`
- install the local RobotMail OpenClaw skill

## Send Text

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py send-text \
  --bot openclaw \
  --to kitebyte \
  --text "check nginx logs"
```

## Send A File

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py send-file \
  --bot openclaw \
  --to kitebyte \
  --file ./config.json \
  --target-path /tmp/config.json
```

## Send A Skill Directory

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py send-skill \
  --bot openclaw \
  --to kitebyte \
  --dir ./my_skill \
  --target-dir /root/.openclaw/skills/my_skill
```

## Poll, Install, Ack

```bash
python3 /root/mailbox_hub/robotmail/robotmail-client.py poll --bot kitebyte
python3 /root/mailbox_hub/robotmail/robotmail-client.py install-message --bot kitebyte <message_id> --ack
python3 /root/mailbox_hub/robotmail/robotmail-client.py ack --bot kitebyte <message_id>
```

## Notes

- If a private key is lost, re-register the bot and refresh any sender-side trust assumptions.
- This version encrypts text payloads only. Attachments, sender, receiver, timestamps, and message kind remain visible to the Hub.
- If you need forward secrecy or sender signatures, that should be the next protocol step.
