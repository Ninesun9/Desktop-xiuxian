# Tauri + OpenClaw 桌面宠物伴侣项目骨架

## 1. 目标

该项目将现有桌宠客户端重构为：

- 前端桌面容器：Tauri
- UI 技术栈：TypeScript + React
- 本地系统桥接：Rust sidecar / Tauri commands
- 业务后端：TypeScript + Fastify
- 实时通信：WebSocket
- 数据层：PostgreSQL + Redis
- 外部能力：OpenClaw Adapter

设计目标：

- 保留桌宠产品形态：透明、置顶、托盘、常驻、自启动
- 将原始修仙玩法抽象为宠物状态机与任务系统
- 兼容 OpenClaw 能力，作为桌面伴侣编排层
- 将本地系统能力、业务 API、外部 AI 能力解耦
- 支持后续增加插件、任务、聊天、提醒、挂机与陪伴功能

## 2. 推荐仓库结构

建议采用 monorepo：

```text
desktop-companion/
  apps/
    desktop/
      src/
        app/
          routes/
          layouts/
          store/
          hooks/
          services/
          features/
            pet/
            chat/
            tasks/
            memory/
            settings/
            companion/
        components/
        assets/
        styles/
        types/
        lib/
      src-tauri/
        src/
          commands/
          sidecar/
          state/
          window/
          tray/
          autostart/
          openclaw/
          main.rs
        capabilities/
        icons/
        tauri.conf.json
      package.json
      vite.config.ts
    api/
      src/
        modules/
          auth/
          users/
          pet/
          companion/
          sessions/
          tasks/
          memory/
          integrations/
          openclaw/
          telemetry/
        plugins/
        middleware/
        lib/
        config/
        index.ts
      package.json
      tsconfig.json
  packages/
    shared/
      src/
        contracts/
        dto/
        schemas/
        events/
        constants/
        openclaw/
      package.json
    ui/
      src/
        pet-shell/
        task-card/
        chat-panel/
        setting-form/
      package.json
    config/
      eslint/
      typescript/
      prettier/
  infra/
    docker/
      docker-compose.yml
    sql/
      migrations/
      seeds/
    env/
      api.env.example
      desktop.env.example
  docs/
    tauri-openclaw-architecture.md
    api-contracts.md
    event-protocol.md
  scripts/
    dev.ps1
    build.ps1
    bootstrap.ps1
  package.json
  pnpm-workspace.yaml
  turbo.json
```

## 3. 服务边界

### 3.1 desktop 应用

职责：

- 提供桌宠可视化外壳与交互层
- 管理窗口、托盘、自启动、快捷键
- 展示宠物状态、聊天面板、任务面板、通知面板
- 与本地 Rust 能力层通信
- 与远端 API、OpenClaw 编排层交互

不负责：

- 持久化业务主数据
- 保存高权限密钥
- 直接信任前端数值上报

### 3.2 src-tauri / Rust 桥接层

职责：

- 窗口控制：透明、无边框、置顶、拖拽、隐藏、贴边
- 系统托盘、通知、自启动、文件访问
- 安全地执行本地命令或 sidecar
- 维护本地少量安全状态，例如设备 ID、本地会话缓存
- 对 OpenClaw 本地能力做适配

不负责：

- 业务规则判断
- 排行榜或云端同步逻辑

### 3.3 api 服务

职责：

- 用户、设备、宠物、任务、会话、记忆管理
- 对接 OpenClaw provider
- 统一鉴权、审计、限流、遥测
- 提供 HTTP API 与 WebSocket 事件网关

不负责：

- UI 展示
- 客户端特有的窗口行为

### 3.4 OpenClaw Adapter

职责：

- 将 OpenClaw 的输入输出转换为项目内部统一协议
- 抽象能力类型：chat、tool_call、memory、task、presence
- 统一 trace_id、session_id、tool_result、stream_event

不负责：

- 业务数据库操作
- 直接操作桌面窗口

## 4. 模块拆分

### 4.1 desktop 前端模块

- pet-shell：桌宠形态、待机动画、拖拽、点击交互
- companion-panel：主控制台，展示 AI 伴侣状态
- chat-panel：用户与伴侣对话
- task-center：任务、提醒、自动化动作
- memory-view：伴侣记忆、偏好、上下文摘要
- settings：模型、OpenClaw 连接、通知、自启动、隐私设置
- sync-client：调用 API 与 WebSocket

### 4.2 api 模块

- auth：登录、设备绑定、token 刷新
- users：用户资料、偏好、配额
- pet：宠物状态、成长、陪伴值、外观
- companion：伴侣人格配置、状态、presence
- sessions：聊天会话、上下文窗口、消息历史
- tasks：提醒、定时任务、自动行动
- memory：长期记忆、短期摘要、显式收藏
- openclaw：provider 配置、能力映射、事件流
- telemetry：日志、埋点、错误追踪

## 5. 领域模型

### 5.1 User

```ts
type User = {
  id: string;
  name: string;
  avatarUrl?: string;
  createdAt: string;
};
```

### 5.2 Device

```ts
type Device = {
  id: string;
  userId: string;
  platform: 'windows' | 'macos' | 'linux';
  displayName: string;
  lastSeenAt: string;
  createdAt: string;
};
```

### 5.3 PetState

```ts
type PetState = {
  id: string;
  userId: string;
  mood: 'idle' | 'happy' | 'busy' | 'sleepy' | 'alert';
  energy: number;
  intimacy: number;
  level: number;
  skin: string;
  statusText?: string;
  updatedAt: string;
};
```

### 5.4 CompanionSession

```ts
type CompanionSession = {
  id: string;
  userId: string;
  provider: 'openclaw';
  title?: string;
  summary?: string;
  createdAt: string;
  updatedAt: string;
};
```

### 5.5 TaskItem

```ts
type TaskItem = {
  id: string;
  userId: string;
  title: string;
  kind: 'reminder' | 'routine' | 'companion' | 'automation';
  status: 'pending' | 'running' | 'done' | 'failed' | 'cancelled';
  scheduleAt?: string;
  metadata?: Record<string, unknown>;
  createdAt: string;
  updatedAt: string;
};
```

## 6. API 边界与接口定义

统一约定：

- Base URL：/api/v1
- 鉴权：Bearer token
- 响应格式：JSON
- 错误格式统一为 code + message + details

### 6.1 认证

#### POST /api/v1/auth/device-login

请求：

```json
{
  "deviceId": "dev_001",
  "deviceName": "Nines-PC",
  "platform": "windows"
}
```

响应：

```json
{
  "accessToken": "jwt-token",
  "refreshToken": "refresh-token",
  "user": {
    "id": "user_001",
    "name": "nines"
  }
}
```

### 6.2 宠物状态

#### GET /api/v1/pet/state

响应：

```json
{
  "id": "pet_001",
  "mood": "idle",
  "energy": 83,
  "intimacy": 42,
  "level": 3,
  "skin": "default"
}
```

#### PATCH /api/v1/pet/state

请求：

```json
{
  "mood": "happy",
  "statusText": "正在陪你工作"
}
```

### 6.3 会话与聊天

#### POST /api/v1/sessions

请求：

```json
{
  "provider": "openclaw",
  "title": "今日陪伴"
}
```

响应：

```json
{
  "id": "sess_001",
  "provider": "openclaw",
  "title": "今日陪伴"
}
```

#### GET /api/v1/sessions/:sessionId/messages

响应：

```json
{
  "items": [
    {
      "id": "msg_001",
      "role": "user",
      "content": "今天帮我安排工作",
      "createdAt": "2026-03-13T08:00:00Z"
    },
    {
      "id": "msg_002",
      "role": "assistant",
      "content": "我先帮你拆成三段。",
      "createdAt": "2026-03-13T08:00:01Z"
    }
  ]
}
```

#### POST /api/v1/sessions/:sessionId/messages

请求：

```json
{
  "content": "帮我整理今天的待办",
  "stream": true
}
```

响应：

```json
{
  "accepted": true,
  "requestId": "req_001"
}
```

### 6.4 任务系统

#### GET /api/v1/tasks

查询参数：

- status
- kind
- page
- pageSize

#### POST /api/v1/tasks

请求：

```json
{
  "title": "晚上 8 点提醒我健身",
  "kind": "reminder",
  "scheduleAt": "2026-03-13T20:00:00+08:00"
}
```

#### PATCH /api/v1/tasks/:taskId

请求：

```json
{
  "status": "cancelled"
}
```

### 6.5 记忆系统

#### GET /api/v1/memory

响应：

```json
{
  "items": [
    {
      "id": "mem_001",
      "kind": "preference",
      "content": "用户偏好简短直接的提醒",
      "score": 0.89
    }
  ]
}
```

#### POST /api/v1/memory

请求：

```json
{
  "kind": "note",
  "content": "用户最近在重构桌宠项目"
}
```

### 6.6 OpenClaw 集成

#### GET /api/v1/openclaw/status

响应：

```json
{
  "provider": "openclaw",
  "connected": true,
  "capabilities": ["chat", "tools", "memory", "stream"]
}
```

#### POST /api/v1/openclaw/invoke

请求：

```json
{
  "sessionId": "sess_001",
  "input": "分析今天的工作安排",
  "tools": ["calendar.read", "todo.list"],
  "stream": true
}
```

响应：

```json
{
  "accepted": true,
  "requestId": "ocl_001"
}
```

## 7. WebSocket 事件协议

连接地址：

```text
GET /api/v1/ws?token=...
```

统一事件信封：

```json
{
  "event": "session.message.delta",
  "traceId": "trace_001",
  "sessionId": "sess_001",
  "timestamp": "2026-03-13T08:00:02Z",
  "payload": {}
}
```

关键事件：

- session.message.started
- session.message.delta
- session.message.completed
- tool.call.started
- tool.call.completed
- task.updated
- pet.state.updated
- companion.presence.changed
- notification.created
- openclaw.error

### 7.1 session.message.delta

```json
{
  "event": "session.message.delta",
  "traceId": "trace_001",
  "sessionId": "sess_001",
  "payload": {
    "messageId": "msg_002",
    "delta": "我先帮你拆成三段。"
  }
}
```

### 7.2 tool.call.completed

```json
{
  "event": "tool.call.completed",
  "traceId": "trace_001",
  "sessionId": "sess_001",
  "payload": {
    "toolName": "todo.list",
    "ok": true,
    "result": {
      "count": 5
    }
  }
}
```

## 8. Tauri 本地能力接口

建议仅暴露高层命令，不暴露底层危险能力。

### 8.1 前端调用 Rust command

```ts
type DesktopCommandMap = {
  'window.pin': { pinned: boolean };
  'window.setScale': { scale: number };
  'window.snapEdge': { edge: 'left' | 'right' };
  'tray.setTooltip': { text: string };
  'autostart.enable': { enabled: boolean };
  'notify.show': { title: string; body: string };
  'openclaw.localStatus': Record<string, never>;
};
```

### 8.2 Rust command 示例

```ts
await invoke('window_pin', { pinned: true });
await invoke('autostart_enable', { enabled: true });
await invoke('notify_show', { title: '提醒', body: '该休息了' });
```

## 9. OpenClaw 兼容层定义

内部统一能力枚举：

```ts
type CompanionCapability =
  | 'chat'
  | 'stream'
  | 'tools'
  | 'memory'
  | 'presence'
  | 'task-execution';
```

Provider 适配接口：

```ts
interface CompanionProvider {
  getStatus(): Promise<{
    connected: boolean;
    capabilities: CompanionCapability[];
  }>;

  sendMessage(input: {
    sessionId: string;
    content: string;
    stream: boolean;
    tools?: string[];
  }): Promise<{ requestId: string }>;

  cancel(requestId: string): Promise<void>;
}
```

核心原则：

- OpenClaw 只是 provider，不是系统核心模型
- 任何 OpenClaw 特有字段都应在 adapter 内消化
- 业务层只能依赖统一 CompanionProvider 接口

### 9.1 当前仓库落地方式

当前仓库已经有一层实际可运行的 OpenClaw adapter 骨架：

- API 适配入口：[apps/api/src/modules/openclaw/index.ts](apps/api/src/modules/openclaw/index.ts)
- API 调用入口：[apps/api/src/index.ts](apps/api/src/index.ts)
- 前端消费入口：[apps/desktop/src/App.tsx](apps/desktop/src/App.tsx)

当前实现不是直接把桌面前端连到 OpenClaw，而是采用下面这条链路：

```text
React / Tauri Desktop
  -> Fastify API
  -> OpenClaw Adapter
  -> OpenClaw Gateway / Provider
```

这样做的原因是：

- 不把 OpenClaw 高权限密钥下放到桌面端
- 不让前端直接依赖 OpenClaw 私有协议
- 后续更换 provider 时只改 adapter，不改 UI
- 可以在 API 层统一审计、限流、缓存和记忆拼装

### 9.2 当前项目约定的 OpenClaw Gateway 契约

为了先把项目结构接起来，当前仓库对 OpenClaw gateway 约定了两个最小接口：

#### GET {OPENCLAW_BASE_URL}/status

响应：

```json
{
  "connected": true,
  "capabilities": ["chat", "stream", "tools", "memory", "presence"]
}
```

#### POST {OPENCLAW_BASE_URL}/chat

请求：

```json
{
  "sessionId": "sess_001",
  "userId": "user_001",
  "traceId": "trace_001",
  "model": "openclaw-default",
  "input": "分析今天的工作安排",
  "history": [
    { "role": "user", "content": "今天状态一般" },
    { "role": "assistant", "content": "我来帮你拆任务。" }
  ]
}
```

响应：

```json
{
  "content": "我先把今天的安排拆成三段：深度工作、沟通、休息。"
}
```

注意：

- 这是当前项目的 gateway 约定，不等同于 OpenClaw 官方固定协议
- 如果未来接入真实 OpenClaw SDK 或官方网关，只需要在 adapter 内转换
- 现在桌面端的流式体验仍由 API 层把完整内容拆成 WebSocket delta 推送

### 9.3 环境变量约定

API 侧目前使用这些变量驱动 OpenClaw 集成：

```env
OPENCLAW_PROVIDER=mock
OPENCLAW_BASE_URL=http://localhost:8787
OPENCLAW_API_KEY=
OPENCLAW_MODEL=
```

其中：

- `mock`：不访问外部网关，返回本地占位响应
- `gateway`：调用 `OPENCLAW_BASE_URL` 指向的 HTTP gateway

### 9.4 下一步如何接真实 OpenClaw

要把当前项目真正接到 OpenClaw，建议按下面顺序推进：

1. 明确 OpenClaw 的真实调用方式：官方 SDK、HTTP gateway 或 sidecar。
2. 如果是 HTTP gateway，就把 [apps/api/src/modules/openclaw/index.ts](apps/api/src/modules/openclaw/index.ts) 中的 `HttpOpenClawAdapter` 按真实协议改掉。
3. 如果是本地 sidecar，就在 [apps/desktop/src-tauri](apps/desktop/src-tauri) 增加 sidecar 管理，再由 API 调本地桥或由 Rust 暴露内部命令。
4. 把当前内存消息上下文替换为数据库中的会话与记忆拼装。
5. 在 WebSocket 事件里增加 tool call、presence、memory update 等更细粒度事件。

## 10. 数据表建议

建议首批表：

- users
- devices
- pet_states
- companion_sessions
- companion_messages
- tasks
- task_runs
- memories
- provider_accounts
- audit_logs

示例：pet_states

```sql
create table pet_states (
  id uuid primary key,
  user_id uuid not null,
  mood varchar(32) not null,
  energy int not null default 100,
  intimacy int not null default 0,
  level int not null default 1,
  skin varchar(64) not null default 'default',
  status_text varchar(255),
  updated_at timestamptz not null default now()
);
```

## 11. 安全边界

必须落实：

- 不在前端保存数据库、Redis、OpenClaw 高权限密钥
- 本地命令白名单化，禁止前端任意执行 shell
- 全部 API 参数校验使用 zod 或 typebox
- WebSocket 建立后做用户与设备双重校验
- 审计所有 tool call 与关键状态变更
- 远端通信默认 HTTPS

## 12. MVP 开发范围

第一阶段只做这些：

1. Tauri 透明桌宠窗口
2. 托盘与自启动
3. 登录与设备绑定
4. 宠物状态展示
5. 基础聊天面板
6. OpenClaw 状态检测
7. 一条消息发送与流式返回
8. 简单提醒任务

明确不在 MVP 内：

- 复杂插件市场
- 多宠物系统
- 云端多人协作
- 完整自动化工作流引擎

## 13. 首批接口最小集合

首周只实现以下接口即可启动联调：

- POST /api/v1/auth/device-login
- GET /api/v1/pet/state
- PATCH /api/v1/pet/state
- POST /api/v1/sessions
- POST /api/v1/sessions/:sessionId/messages
- GET /api/v1/openclaw/status
- GET /api/v1/ws

## 14. 开发顺序

### Sprint 1

- 搭 monorepo
- 起 Tauri 空壳
- 起 Fastify API
- 打通 device-login
- 做 pet state 假数据联调

### Sprint 2

- 接入 WebSocket
- 做聊天会话与消息流
- 接 OpenClaw status 与 invoke
- 加托盘、自启动、通知

### Sprint 3

- 做任务系统
- 做记忆系统
- 做陪伴状态与动画切换
- 增加日志、限流、审计

## 15. 技术选型建议

前端：

- React
- Vite
- Zustand
- TanStack Query
- Tailwind 或 UnoCSS
- Framer Motion

后端：

- Fastify
- Zod
- Prisma 或 Drizzle
- PostgreSQL
- Redis
- ws 或 Socket.IO

工程：

- pnpm workspace
- Turbo
- ESLint
- Prettier
- Vitest
- Playwright

## 16. 一句话落地方案

这个项目建议做成：

- Tauri 负责桌宠外壳与本地系统能力
- TypeScript API 负责业务规则与 OpenClaw 编排
- 共享 contracts 包统一接口、事件、类型
- OpenClaw 作为可替换 provider 接入，不直接污染业务核心

按这个骨架开工，可以先在两周内拿到一个可联调的桌面宠物伴侣 MVP。