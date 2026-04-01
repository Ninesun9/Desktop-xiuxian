#!/bin/bash
#
# RobotMail 安装脚本
# 用于在新机器上快速部署 RobotMail Client 和 Skill
#

set -e

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}🤖 RobotMail 安装脚本${NC}"
echo "================================"

# 配置
ROBOTMAIL_DIR="/robotmail"
VPS_HOST="${VPS_HOST:-root@107.174.220.99}"
SKILL_DIR="${HOME}/.openclaw/skills/robotmail"

# 检查 root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}❌ 请使用 root 用户运行${NC}"
    exit 1
fi

# 1. 创建目录
echo -e "\n${YELLOW}📁 创建目录...${NC}"
mkdir -p "${ROBOTMAIL_DIR}"/{config,spool,attachments,archive,scripts}
mkdir -p "${SKILL_DIR}"

# 2. 检查 SSH 连接
echo -e "\n${YELLOW}🔗 检查 VPS 连接...${NC}"
if ssh -o ConnectTimeout=5 ${VPS_HOST} "echo ok" &>/dev/null; then
    echo -e "${GREEN}✅ VPS 连接成功${NC}"
else
    echo -e "${RED}❌ 无法连接 VPS: ${VPS_HOST}${NC}"
    echo "请设置 VPS_HOST 环境变量 或 修改脚本中的默认值"
    exit 1
fi

# 3. 复制 RobotMail Client
echo -e "\n${YELLOW}📦 安装 RobotMail Client...${NC}"
scp "${VPS_HOST}:/robotmail/robotmail-client.py" "${ROBOTMAIL_DIR}/"
scp "${VPS_HOST}:/robotmail/robotmail-server.py" "${ROBOTMAIL_DIR}/"
scp "${VPS_HOST}:/robotmail/config/agents.json" "${ROBOTMAIL_DIR}/config/"
scp "${VPS_HOST}:/robotmail/config/resources.json" "${ROBOTMAIL_DIR}/config/" 2>/dev/null || true

# 复制脚本
scp "${VPS_HOST}:/robotmail/scripts/parse_command.py" "${ROBOTMAIL_DIR}/scripts/" 2>/dev/null || true
chmod +x "${ROBOTMAIL_DIR}"/*.py "${ROBOTMAIL_DIR}"/scripts/*.py 2>/dev/null || true

# 4. 安装 SSHFS (可选)
echo -e "\n${YELLOW}🔧 检查 SSHFS...${NC}"
if ! command -v sshfs &>/dev/null; then
    echo "安装 SSHFS..."
    apt-get update && apt-get install -y sshfs
fi

# 5. 挂载远程 spool (可选)
echo -e "\n${YELLOW}📡 挂载远程 Spool...${NC}"
read -p "是否挂载远程 spool? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    # 检查并添加 SSH 密钥
    if [ ! -f "${HOME}/.ssh/id_ed25519" ]; then
        ssh-keygen -t ed25519 -f "${HOME}/.ssh/id_ed25519" -N ""
    fi
    echo "复制 SSH 密钥到 VPS..."
    ssh-copy-id -i "${HOME}/.ssh/id_ed25519.pub" ${VPS_HOST} 2>/dev/null || {
        ssh ${VPS_HOST} "mkdir -p ~/.ssh && chmod 700 ~/.ssh"
        cat "${HOME}/.ssh/id_ed25519.pub" | ssh ${VPS_HOST} "cat >> ~/.ssh/authorized_keys"
    }
    
    # 挂载
    sshfs -o allow_other,default_permissions,IdentityFile="${HOME}/.ssh/id_ed25519" \
        "${VPS_HOST}:/robotmail/spool" "${ROBOTMAIL_DIR}/spool"
    
    if mountpoint -q "${ROBOTMAIL_DIR}/spool"; then
        echo -e "${GREEN}✅ 挂载成功${NC}"
    fi
fi

# 6. 安装 OpenClaw Skill
echo -e "\n${YELLOW}🎭 安装 OpenClaw Skill...${NC}"

# 创建 skill 文件
cat > "${SKILL_DIR}/SKILL.md" << 'EOF'
# 🤖 RobotMail Skill

## 触发关键词
- "发送给" + 机器人名
- "发给" + 机器人名  
- "告诉" + 机器人名
- "检查 robotmail"

## 功能
- 发送工具给其他机器人
- 发送消息
- 检查收件箱

## 使用
直接说话即可触发技能
EOF

cat > "${SKILL_DIR}/robotmail_skill.py" << 'EOF'
#!/usr/bin/env python3
"""RobotMail OpenClaw Skill"""
import os, sys, json, subprocess
from pathlib import Path

ROBOTMAIL_DIR = Path("/robotmail")
CLIENT = ROBOTMAIL_DIR / "robotmail-client.py"
PARSE = ROBOTMAIL_DIR / "scripts" / "parse_command.py"

KEYWORDS = ["发送给", "发给", "传给", "send to", "告诉", "robotmail", "检查"]

def check(text):
    return any(k in text for k in KEYWORDS)

def run(text):
    try:
        # 解析
        p = subprocess.run(["python3", str(PARSE), text], capture_output=True, text=True, timeout=5)
        cmd = json.loads(p.stdout) if p.returncode == 0 else {}
        
        action = cmd.get("action")
        
        if action == "send_tool":
            r = subprocess.run(["python3", str(CLIENT), "--bot", "openclaw", "--action", "send_tool", 
                              "--to", cmd["receiver"], "--tool", cmd["item"]], capture_output=True, text=True)
            return f"✅ 工具已发送" if r.returncode == 0 else f"❌ 失败"
        
        if action == "send_message":
            r = subprocess.run(["python3", str(CLIENT), "--bot", "openclaw", "--action", "send",
                              "--to", cmd["receiver"], "--summary", cmd["message"]], capture_output=True, text=True)
            return f"✅ 消息已发送" if r.returncode == 0 else f"❌ 失败"
        
    except Exception as e:
        return f"❌ 错误: {e}"
    
    return "❓ 无法处理"

if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(run(" ".join(sys.argv[1:])))
EOF

chmod +x "${SKILL_DIR}"/*.py

# 7. 测试
echo -e "\n${YELLOW}🧪 测试安装...${NC}"
python3 "${CLIENT}" --bot openclaw --action heartbeat &>/dev/null && echo -e "${GREEN}✅ Client 测试成功${NC}" || echo -e "${RED}❌ Client 测试失败${NC}"

# 完成
echo -e "\n${GREEN}================================"
echo -e "🎉 安装完成!"
echo -e "================================${NC}"
echo ""
echo "使用方式:"
echo "  python3 ${CLIENT} --bot openclaw --action send --to kitebyte --summary '你好'"
echo "  python3 ${CLIENT} --bot openclaw --action send_tool --to kitebyte --tool scanner"
echo ""
echo "OpenClaw Skill 已安装到: ${SKILL_DIR}"
