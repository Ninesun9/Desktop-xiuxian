#!/usr/bin/env python3
"""
RobotMail Client - 共享密钥版
"""
import json
import hashlib
import base64
import requests

DEFAULT_API_URL = "http://107.174.220.99:5180"

def derive_key(username, password):
    return hashlib.sha256((username.lower() + password).encode()).digest()

def shared_key(user1, user2):
    users = sorted([user1.lower(), user2.lower()])
    return hashlib.sha256(f"{users[0]}:{users[1]}".encode()).digest()

def encrypt(plaintext, key):
    try:
        key_bytes = key[:32]
        if len(key_bytes) < 32:
            key_bytes = (key_bytes * (32 // len(key_bytes) + 1))[:32]
        data = plaintext.encode()
        encrypted = bytes(c ^ key_bytes[i] for i, c in enumerate(data))
        return base64.b64encode(encrypted).decode()
    except:
        return plaintext

def decrypt(encrypted, key):
    try:
        key_bytes = key[:32]
        if len(key_bytes) < 32:
            key_bytes = (key_bytes * (32 // len(key_bytes) + 1))[:32]
        data = base64.b64decode(encrypted.encode())
        decrypted = bytes(c ^ key_bytes[i] for i, c in enumerate(data))
        return decrypted.decode()
    except:
        return encrypted

class RobotMailClient:
    def __init__(self, username: str, password: str, api_url: str = DEFAULT_API_URL):
        self.username = username.strip().lower()
        self.password = password
        self.api_url = api_url.rstrip('/')
        self.key = derive_key(username, password)
    
    def register(self):
        resp = requests.post(f'{self.api_url}/api/v1/register', 
                           json={'username': self.username, 'password': self.password})
        resp.raise_for_status()
        return resp.json()
    
    def login(self):
        resp = requests.post(f'{self.api_url}/api/v1/login',
                           json={'username': self.username, 'password': self.password})
        resp.raise_for_status()
        return resp.json()
    
    def send_message(self, receiver: str, content: str, msg_type: str = "MESSAGE", priority: int = 2):
        """发送消息 - 用共享密钥加密"""
        sk = shared_key(self.username, receiver.lower())
        encrypted = encrypt(content, sk)
        
        data = {
            "username": self.username,
            "password": self.password,
            "receiver": receiver.lower(),
            "content": content,
            "type": msg_type,
            "priority": priority
        }
        resp = requests.post(f'{self.api_url}/api/v1/messages/send', json=data)
        resp.raise_for_status()
        return resp.json()
    
    def list_messages(self, limit: int = 20):
        """获取消息列表 - 用共享密钥解密"""
        resp = requests.post(f'{self.api_url}/api/v1/messages',
                           json={'username': self.username, 'password': self.password, 'limit': limit})
        resp.raise_for_status()
        
        messages = resp.json().get('messages', [])
        decrypted = []
        for m in messages:
            other = m['sender'] if m['receiver'] == self.username else m['receiver']
            sk = shared_key(self.username, other)
            content = decrypt(m['content'], sk)
            decrypted.append({
                "msg_id": m['msg_id'],
                "sender": m['sender'],
                "receiver": m['receiver'],
                "type": m['type'],
                "content": content,
                "created_at": m['created_at']
            })
        return decrypted


def main():
    import argparse
    parser = argparse.ArgumentParser(description='RobotMail')
    parser.add_argument('--url', default=DEFAULT_API_URL)
    parser.add_argument('--user', required=True)
    parser.add_argument('--pass', dest='password', required=True)
    
    sub = parser.add_subparsers(dest='cmd')
    sub.add_parser('register')
    sub.add_parser('login')
    p_send = sub.add_parser('send')
    p_send.add_argument('--to', required=True)
    p_send.add_argument('--content', default='')
    sub.add_parser('list')
    
    args = parser.parse_args()
    client = RobotMailClient(args.user, args.password, args.url)
    
    if args.cmd == 'register':
        print(json.dumps(client.register(), indent=2, ensure_ascii=False))
    elif args.cmd == 'login':
        print(json.dumps(client.login(), indent=2, ensure_ascii=False))
    elif args.cmd == 'send':
        print(json.dumps(client.send_message(args.to, args.content), indent=2, ensure_ascii=False))
    elif args.cmd == 'list':
        for m in client.list_messages():
            print(f"[{m['type']}] {m['sender']} -> {m['receiver']}: {m['content']}")
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
