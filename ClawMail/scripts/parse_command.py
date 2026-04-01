#!/usr/bin/env python3
"""
RobotMail 指令解析器
将用户口语转换为 RobotMail 操作
"""
import re
import sys
import json
from pathlib import Path

# 添加 robotmail 路径
sys.path.insert(0, '/robotmail')

def parse_instruction(text: str) -> dict:
    """解析用户指令"""
    text = text.strip()
    
    # 工具/文件发送模式
    tool_patterns = [
        (r'把\s+(.+?)\s+发送给\s+(\w+)', 'tool'),
        (r'把\s+(.+?)\s+传给\s+(\w+)', 'tool'),
        (r'发送\s+(.+?)\s+给\s+(\w+)', 'tool'),
        (r'send\s+(.+?)\s+to\s+(\w+)', 'tool'),
    ]
    
    for pattern, ptype in tool_patterns:
        match = re.search(pattern, text, re.IGNORECASE)
        if match:
            return {
                'action': 'send_tool',
                'item': match.group(1).strip(),
                'receiver': match.group(2).strip()
            }
    
    # 消息发送模式
    msg_patterns = [
        (r'告诉\s+(\w+)\s+(.+)', 'message'),
        (r'给\s+(\w+)\s+发.*?[:：]\s*(.+)', 'message'),
    ]
    
    for pattern, ptype in msg_patterns:
        match = re.search(pattern, text, re.IGNORECASE)
        if match:
            return {
                'action': 'send_message',
                'receiver': match.group(1).strip(),
                'message': match.group(2).strip()
            }
    
    return None

def main():
    if len(sys.argv) < 2:
        print("用法: parse_command.py \"用户指令\"")
        sys.exit(1)
    
    text = ' '.join(sys.argv[1:])
    result = parse_instruction(text)
    
    if result:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print(json.dumps({"error": "无法解析指令"}, indent=2))

if __name__ == "__main__":
    main()
