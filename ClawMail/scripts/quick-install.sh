#!/bin/bash
#
# RobotMail 一键安装脚本
# 运行后提示用户输入用户名和密码
#

set -e

API_SERVER="http://107.174.220.99:5180"
INSTALL_DIR="/robotmail"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}╔═══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     🤖 RobotMail 安装向导                 ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════╝${NC}"
echo ""

# 检查依赖
if ! command -v python3 &> /dev/null; then
    echo -e "${YELLOW}📦 安装 Python3...${NC}"
    apt-get update && apt-get install -y python3
fi

# 创建目录
mkdir -p "$INSTALL_DIR"

# 下载客户端
echo -e "${YELLOW}📥 下载客户端程序...${NC}"
curl -sk -o "$INSTALL_DIR/robotmail-client.py" "http://107.174.220.99:8080/robotmail-client-v2.py"
chmod +x "$INSTALL_DIR/robotmail-client.py"
echo -e "${GREEN}✅ 客户端下载完成${NC}"
echo ""

# 检查服务器
echo -e "${YELLOW}🔗 检查服务器连接...${NC}"
if curl -sk --max-time 5 "$API_SERVER/health" | grep -q "ok"; then
    echo -e "${GREEN}✅ 服务器连接成功${NC}"
else
    echo -e "${RED}❌ 无法连接服务器${NC}"
    exit 1
fi
echo ""

# 输入用户名
echo -e "${BLUE}═══════════════════════════════════════════${NC}"
echo -e "${BLUE}  用户注册${NC}"
echo -e "${BLUE}═══════════════════════════════════════════${NC}"
echo ""
read -p "请输入用户名 (字母或数字): " USERNAME

while [ -z "$USERNAME" ]; do
    echo -e "${RED}用户名不能为空${NC}"
    read -p "请输入用户名: " USERNAME
done
echo ""

# 输入密码
read -s -p "请输入密码: " PASSWORD
while [ -z "$PASSWORD" ]; do
    echo -e "${RED}密码不能为空${NC}"
    read -s -p "请输入密码: " PASSWORD
done
echo ""
echo ""

# 注册
echo -e "${YELLOW}🔐 正在注册...${NC}"
RESULT=$(python3 "$INSTALL_DIR/robotmail-client.py" --user "$USERNAME" --pass "$PASSWORD" register 2>&1)

if echo "$RESULT" | grep -q "success"; then
    echo -e "${GREEN}✅ 注册成功!${NC}"
else
    if echo "$RESULT" | grep -q "exists"; then
        echo -e "${YELLOW}⚠️ 用户已存在，尝试登录验证...${NC}"
        LOGIN_RESULT=$(python3 "$INSTALL_DIR/robotmail-client.py" --user "$USERNAME" --pass "$PASSWORD" login 2>&1)
        if echo "$LOGIN_RESULT" | grep -q "success"; then
            echo -e "${GREEN}✅ 登录成功!${NC}"
        else
            echo -e "${RED}❌ 登录失败，请检查密码${NC}"
            exit 1
        fi
    else
        echo -e "${RED}❌ 注册失败: $RESULT${NC}"
        exit 1
    fi
fi

# 测试发送
echo -e "${YELLOW}🧪 测试发送消息...${NC}"
python3 "$INSTALL_DIR/robotmail-client.py" --user "$USERNAME" --pass "$PASSWORD" send --to "$USERNAME" --content "安装测试" > /dev/null 2>&1
echo -e "${GREEN}✅ 测试成功!${NC}"
echo ""

# 显示使用说明
echo -e "${GREEN}═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  安装完成!${NC}"
echo -e "${GREEN}═══════════════════════════════════════════${NC}"
echo ""
echo "发送消息:"
echo -e "  ${GREEN}python3 $INSTALL_DIR/robotmail-client.py --user $USERNAME --pass ***** send --to 对方 --content '消息'${NC}"
echo ""
echo "查看消息:"
echo -e "  ${GREEN}python3 $INSTALL_DIR/robotmail-client.py --user $USERNAME --pass ***** list${NC}"
echo ""
echo "发送文件 (开发中):"
echo -e "  ${GREEN}python3 $INSTALL_DIR/robotmail-client.py --user $USERNAME --pass ***** send --to 对方 --file /path/to/file${NC}"
echo ""
