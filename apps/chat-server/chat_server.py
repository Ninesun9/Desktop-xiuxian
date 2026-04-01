#!/usr/bin/env python3
"""
TCP 广播聊天服务器
协议：每条消息为 Base64 编码字节串 + '\n' 结尾。
收到任意客户端的消息后，原样广播给所有已连接客户端（含发送者，
客户端自己用 UUID 区分自己/他人消息）。
"""

import asyncio
import logging
import signal

HOST = "0.0.0.0"
PORT = 8524
MAX_MSG_BYTES = 65536   # 单条消息上限（Base64 后约 48 KB 原始数据）

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger(__name__)

# 全局连接集合
clients: set[asyncio.StreamWriter] = set()


async def handle(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    addr = writer.get_extra_info("peername")
    log.info("connected: %s  total=%d", addr, len(clients) + 1)
    clients.add(writer)
    buf = b""

    try:
        while True:
            chunk = await reader.read(4096)
            if not chunk:
                break

            buf += chunk
            if len(buf) > MAX_MSG_BYTES:
                log.warning("oversized message from %s, dropping connection", addr)
                break

            # 可能一次收到多条（粘包），逐条处理
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.strip()
                if not line:
                    continue
                await broadcast(line + b"\n")

    except (asyncio.IncompleteReadError, ConnectionResetError):
        pass
    finally:
        clients.discard(writer)
        log.info("disconnected: %s  total=%d", addr, len(clients))
        try:
            writer.close()
            await writer.wait_closed()
        except Exception:
            pass


async def broadcast(data: bytes):
    dead = set()
    for w in list(clients):
        try:
            w.write(data)
            await w.drain()
        except Exception:
            dead.add(w)
    for w in dead:
        clients.discard(w)


async def main():
    server = await asyncio.start_server(handle, HOST, PORT)
    addrs = ", ".join(str(s.getsockname()) for s in server.sockets)
    log.info("chat server listening on %s", addrs)

    loop = asyncio.get_running_loop()
    stop = loop.create_future()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop.set_result, None)

    async with server:
        await stop
    log.info("shutting down")


if __name__ == "__main__":
    asyncio.run(main())
