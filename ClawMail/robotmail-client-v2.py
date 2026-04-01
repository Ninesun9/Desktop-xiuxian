#!/usr/bin/env python3
"""
RobotMail API Client v2 - 安全版
"""
import os
import sys
import json
import hashlib
import base64
import requests
from pathlib import Path

DEFAULT_API_URL = "http://107.174.220.99:5180"

class RobotMailClient:
    """RobotMail API 客户端"""
    
    def __init__(self, username: str, password: str, api_url: str = DEFAULT_API_URL):
        self.username = username.strip()
        self.password = password
        self.api_url = api_url.rstrip('/')
        self.session = requests.Session()
        self.public_key = None
    
    def _derive_key(self) -> bytes:
        """派生密钥: SHA256(username + password)"""
        return hashlib.sha256((self.username + self.password).encode()).digest()
    
    def register(self) -> dict:
        """注册用户"""
        resp = requests.post(f'{self.api_url}/api/v1/register', 
                           json={'username': self.username, 'password': self.password})
        resp.raise_for_status()
        data = resp.json()
        
        if data.get('status') == 'success':
            self.public_key = data.get('public_key')
        
        return data
    
    def login(self) -> dict:
        """登录"""
        resp = requests.post(f'{self.api_url}/api/v1/login',
                           json={'username': self.username, 'password': self.password})
        resp.raise_for_status()
        return resp.json()
    
    def profile(self) -> dict:
        """获取用户信息"""
        resp = requests.post(f'{self.api_url}/api/v1/profile',
                           json={'username': self.username, 'password': self.password})
        resp.raise_for_status()
        return resp.json()
    
    def send_message(self, receiver: str, content: str, 
                    msg_type: str = "MESSAGE", priority: int = 2) -> dict:
        """发送加密消息"""
        data = {
            "username": self.username,
            "password": self.password,
            "receiver": receiver.strip(),
            "content": content,
            "type": msg_type,
            "priority": priority
        }
        
        resp = requests.post(f'{self.api_url}/api/v1/messages/send', json=data)
        resp.raise_for_status()
        return resp.json()
    
    def list_messages(self, limit: int = 20) -> list:
        """获取消息列表"""
        resp = requests.post(f'{self.api_url}/api/v1/messages',
                           json={'username': self.username, 'password': self.password, 'limit': limit})
        resp.raise_for_status()
        return resp.json().get('messages', [])
    
    def get_message(self, msg_id: str) -> dict:
        """获取并解密消息"""
        resp = requests.post(f'{self.api_url}/api/v1/messages/{msg_id}',
                           json={'username': self.username, 'password': self.password})
        resp.raise_for_status()
        return resp.json()


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='RobotMail API Client v2')
    parser.add_argument('--url', default=DEFAULT_API_URL, help='API 服务器地址')
    parser.add_argument('--user', required=True, help='用户名')
    parser.add_argument('--pass', dest='password', required=True, help='密码')
    
    sub = parser.add_subparsers(dest='cmd')
    
    sub.add_parser('register', help='注册')
    sub.add_parser('login', help='登录')
    sub.add_parser('profile', help='查看信息')
    
    p_send = sub.add_parser('send', help='发送消息')
    p_send.add_argument('--to', required=True, help='接收方')
    p_send.add_argument('--content', default='', help='内容')
    
    p_list = sub.add_parser('list', help='消息列表')
    p_list.add_argument('--limit', type=int, default=20)
    
    p_read = sub.add_parser('read', help='读取消息')
    p_read.add_argument('msg_id', help='消息ID')
    
    args = parser.parse_args()
    
    client = RobotMailClient(args.user, args.password, args.url)
    
    if args.cmd == 'register':
        result = client.register()
        print(json.dumps(result, indent=2, ensure_ascii=False))
    
    elif args.cmd == 'login':
        result = client.login()
        print(json.dumps(result, indent=2, ensure_ascii=False))
    
    elif args.cmd == 'profile':
        result = client.profile()
        print(json.dumps(result, indent=2, ensure_ascii=False))
    
    elif args.cmd == 'send':
        result = client.send_message(args.to, args.content)
        print(json.dumps(result, indent=2, ensure_ascii=False))
    
    elif args.cmd == 'list':
        msgs = client.list_messages(limit=args.limit)
        for m in msgs:
            print(f"[{m['type']}] {m['sender']} -> {m['receiver']} | {m['created_at']}")
    
    elif args.cmd == 'read':
        result = client.get_message(args.msg_id)
        print(json.dumps(result, indent=2, ensure_ascii=False))
    
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
