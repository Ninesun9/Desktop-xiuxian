# 开工说明

## 当前已落地内容

- 根级 npm workspaces monorepo
- apps/api：Fastify 最小 API
- apps/desktop：React + Vite 桌面前端壳子
- apps/desktop/src-tauri：Tauri Rust 入口骨架
- packages/shared：共享接口类型

## 当前可用命令

### 安装依赖

```powershell
npm install
```

### 启动 API

```powershell
npm run dev:api
```

### 启动前端壳子

```powershell
npm run dev:desktop
```

### 类型检查

```powershell
npm run typecheck
```

### 构建桌面前端

```powershell
npm run build -w @desktop-companion/desktop
```

## 已有接口

- GET /health
- POST /api/v1/auth/device-login
- GET /api/v1/pet/state
- PATCH /api/v1/pet/state
- POST /api/v1/sessions
- GET /api/v1/sessions/:sessionId/messages
- POST /api/v1/sessions/:sessionId/messages
- GET /api/v1/openclaw/status
- GET /api/v1/ws

## 现阶段限制

- 尚未安装 Rust/Cargo，Tauri 原生壳子还不能本地编译
- 尚未安装 pnpm，当前脚本先用 npm workspaces 驱动
- OpenClaw 已有 adapter 骨架，默认走 mock；切到真实 gateway 还需要配置环境变量和对方网关契约
- 数据层当前是内存假数据，尚未接 PostgreSQL / Redis

## OpenClaw 接入

当前 API 已接入 OpenClaw adapter，位置在 [apps/api/src/modules/openclaw/index.ts](apps/api/src/modules/openclaw/index.ts)。

支持两种模式：

- `OPENCLAW_PROVIDER=mock`：返回本地占位响应，便于前端联调
- `OPENCLAW_PROVIDER=gateway`：把请求转发到 `OPENCLAW_BASE_URL`

当使用 `gateway` 时，当前项目约定 OpenClaw gateway 暴露两个 HTTP 接口：

- `GET {OPENCLAW_BASE_URL}/status`
- `POST {OPENCLAW_BASE_URL}/chat`

`POST /chat` 请求体约定为：

```json
{
	"sessionId": "sess_001",
	"userId": "user_001",
	"traceId": "trace_001",
	"model": "",
	"input": "帮我整理今天的工作",
	"history": [
		{ "role": "user", "content": "你好" },
		{ "role": "assistant", "content": "你好，我在。" }
	]
}
```

响应体约定为：

```json
{
	"content": "我先帮你把今天的工作分成三段。"
}
```

示例环境变量：

```powershell
$env:OPENCLAW_PROVIDER="gateway"
$env:OPENCLAW_BASE_URL="http://localhost:8787"
$env:OPENCLAW_API_KEY="replace-me"
$env:OPENCLAW_MODEL="openclaw-default"
npm run dev:api
```

## 建议下一步

1. 安装 Rust 与 Tauri CLI，先跑通桌面壳子
2. 接入 PostgreSQL 和迁移工具
3. 把 pet state、sessions、messages 从内存切到数据库
4. 把 OpenClaw gateway 从项目约定契约升级为真实 provider 契约
