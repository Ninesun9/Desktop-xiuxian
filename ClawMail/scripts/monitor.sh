#!/bin/bash
# RobotMail 监控脚本
# 检查服务状态、消息队列、心跳等

ROBOTMAIL_HOST="root@107.174.220.99"
SPOOL="/robotmail/spool"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "========================================"
echo "🤖 RobotMail 状态监控"
echo "========================================"

# 1. 检查 SSHFS 挂载
echo -n "📡 SSHFS 挂载: "
if mountpoint -q "$SPOOL" 2>/dev/null; then
    echo -e "${GREEN}✅ 已挂载${NC}"
else
    echo -e "${RED}❌ 未挂载${NC}"
fi

# 2. 检查 Server 状态
echo -n "🖥️ Server 状态: "
SERVER_STATUS=$(ssh $ROBOTMAIL_HOST "systemctl is-active robotmail" 2>/dev/null)
if [ "$SERVER_STATUS" = "active" ]; then
    echo -e "${GREEN}✅ 运行中${NC}"
else
    echo -e "${RED}❌ 未运行 ($SERVER_STATUS)${NC}"
fi

# 3. 消息统计
echo ""
echo "📊 消息统计:"
ssh $ROBOTMAIL_HOST "cd /robotmail && python3 rbmail status" 2>/dev/null | grep -E "^(📊|   |🤖)"

# 4. 待处理消息
echo ""
echo -n "📬 待处理: "
PENDING=$(ssh $ROBOTMAIL_HOST "ls /robotmail/spool/*/out/*.json 2>/dev/null | wc -l" 2>/dev/null)
echo "$PENDING 条"

# 5. 磁盘使用
echo ""
echo "💾 磁盘使用:"
ssh $ROBOTMAIL_HOST "df -h /robotmail 2>/dev/null" | tail -1

# 6. tmpfs 使用
echo ""
echo "📀 Spool (tmpfs):"
df -h "$SPOOL" 2>/dev/null | tail -1

# 7. 最近日志
echo ""
echo "📝 最近日志 (最后5条):"
ssh $ROBOTMAIL_HOST "tail -5 /robotmail/robotmail-server.log" 2>/dev/null | grep -E "\[INFO\]|\[ERROR\]" | tail -5

echo ""
echo "========================================"
