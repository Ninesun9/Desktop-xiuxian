# Qt 到 Tauri 迁移清单

## 目标

这份清单用于把现有 Qt 客户端能力映射到新的 Tauri + React + API 架构，明确：

- 现有能力在哪里
- Tauri 目标层应该放哪里
- 迁移时先后顺序是什么
- 哪些部分可以直接迁，哪些需要重构


## 一、能力对照

| 能力域 | Qt 现状 | 当前代码位置 | Tauri 目标落点 | 迁移建议 |
| --- | --- | --- | --- | --- |
| 主窗体外壳 | 已实现透明、无边框、置顶、拖拽 | Client2/dialog.cpp | apps/desktop/src-tauri/src/main.rs + 前端 pet-shell | 优先迁移。属于 Tauri 最适合接管的一层 |
| 窗口置顶切换 | 已实现右键菜单切换置顶 | Client2/dialog.cpp | Tauri command + React 状态 | 直接迁。当前 main.rs 已有 window_pin 雏形 |
| 常驻后台 | 已实现关闭最后窗口不退出 | Client2/main.cpp | Tauri 生命周期 + tray | 需要补系统托盘后一起迁 |
| 系统托盘 | Qt 代码中未见完整托盘实现 | 无明确落地 | src-tauri/tray | 新增实现，不是平移 |
| 自启动 | 直接写 Windows 注册表 | Client2/dialog.cpp | src-tauri/autostart | 必须重写，当前实现是 Windows 专属 |
| 本地存档 | QSettings + ini 文件 | Client2/dialog.cpp, Client2/chatmessage/chatwindow.cpp | Tauri plugin-store 或 Rust 安全存储 | 需要重构存储模型，不建议照搬 ini |
| 用户初始化 | 首次启动创建名字、生成 uuid、初始化数值 | Client2/dialog.cpp | React 首次启动向导 + API device/user 初始化 | 需要拆成前端交互 + 服务端模型 |
| 修炼数值循环 | 1s/10s/30s 定时推进 | Client2/dialog.cpp | 前端 store + API 同步策略 | 先保留本地模拟，再决定是否服务端托管 |
| 境界升级和数值演进 | 本地规则计算 | Client2/dialog.cpp | 前端 domain store 或 API pet/task 模块 | 建议把规则迁到服务端，避免客户端作弊 |
| 自定义皮肤 | 选择本地图片并调整主窗尺寸 | Client2/dialog.cpp | Tauri 文件选择 + React 资源管理 | 可迁，但需要先定义皮肤资源模型 |
| 排行榜 | 直接请求服务端 HTML 排行页 | Client2/ranklist.cpp | React rank view + API JSON 接口 | 不能直接平移，先改 API 协议 |
| 反馈 | 独立对话框提交文本 | Client2/UpdataInfo/feedback.cpp | React settings/feedback 面板 + API | 低成本迁移 |
| 占卜 | 独立窗口模块 | Client2/Augur | React feature/augur | UI 可迁，业务规则需单独梳理 |
| 江湖系统 | 独立窗口和全局状态 | Client2/jianghu/* | React feature/tasks 或 feature/jianghu + API | 中高成本迁移，建议晚于基础桌宠 |
| 聊天窗口 | 独立窗口、消息列表、头像、表情 | Client2/chatmessage/chatwindow.cpp | React chat-panel | 核心功能，可迁，但协议要重做 |
| 聊天网络层 | 自建 socket 协议 + base64 文本消息 | Client2/chatmessage/conservsocket.cpp | Fastify + WebSocket 统一事件协议 | 需要整体替换，不建议兼容旧协议 |
| 敏感词过滤 | 本地替换式过滤 | Client2/chatmessage/wordfilter.cpp | API 服务端过滤 + 前端提示 | 逻辑应迁服务端，前端只保留体验增强 |
| 消息弹窗通知 | 自定义浮层通知 | Client2/messageshow.cpp, Client2/popupmanage.cpp | Tauri 通知或前端 overlay | 先用前端 overlay，系统通知再补 |
| 远程上报 | 定时上传修炼数值和江湖积分 | Client2/dialog.cpp, Client2/jianghu/jianghuglobal.cpp | API pet sync / task sync | 协议已变化，需按新接口重写 |
| Redis 直连 | 客户端内置 Redis 库 | Client2/RedisLib/* | 后端内部能力 | 不应迁到 Tauri 前端，必须移除客户端直连 |
| 发布形态 | qmake / Qt 工程 | Client2/Xiuxian.pro | Tauri bundler | 重新建立构建与分发链路 |


## 二、迁移结论

### 适合直接迁到 Tauri 原生层的能力

- 主窗体透明、无边框、置顶
- 拖拽和窗口固定行为
- 自启动
- 后台常驻
- 后续托盘、通知、快捷键、文件选择

这些能力原本就属于桌面容器层，不属于业务规则。Qt 里能做，Tauri 也能做，而且跨平台实现会更清晰。

### 适合迁到 React 前端层的能力

- 主界面展示
- 聊天面板
- 排行榜视图
- 设置页、反馈页
- 占卜和江湖等功能面板
- 皮肤配置和状态展示

这些能力原来被拆成多个 Qt 对话框和窗口。迁到 React 后应优先收敛成 feature 模块，而不是继续维持一堆分散窗口。

### 必须重构，不能照搬的能力

- 自启动注册表逻辑
- QSettings 的 Windows 路径存储
- 客户端直接生成用户身份和数值
- 聊天 socket 私有协议
- 排行榜依赖 HTML 页面
- 客户端敏感词过滤作为唯一防线
- 客户端直连 Redis

这些部分要么严重依赖 Windows，要么本身架构边界不合理。迁移时不能只做语法搬运。


## 三、推荐迁移阶段

### 阶段 1：桌面壳子替换

目标：先让 Tauri 能承担 Qt 的“桌宠壳”职责。

状态：进行中

已完成：

- 已建立 Tauri 主窗口骨架，透明、无边框、always on top 已对齐到 [apps/desktop/src-tauri/tauri.conf.json](apps/desktop/src-tauri/tauri.conf.json)
- 已建立 React 主壳、宠物状态卡和桌面控制卡，位置在 [apps/desktop/src/App.tsx](apps/desktop/src/App.tsx)

进行中：

- 已补前端托盘创建、关闭到托盘、自启动控制逻辑，位置在 [apps/desktop/src/app/lib/tauriDesktop.ts](apps/desktop/src/app/lib/tauriDesktop.ts)
- 已补 Rust 侧 autostart 插件和 capability 配置，位置在 [apps/desktop/src-tauri/src/main.rs](apps/desktop/src-tauri/src/main.rs) 与 [apps/desktop/src-tauri/capabilities/default.json](apps/desktop/src-tauri/capabilities/default.json)

未开始：

- 拖拽、贴边、缩放等更完整的桌宠窗口行为还没迁
- 托盘与自启动尚未经过本机 Rust/Tauri 运行验证

完成标志：不依赖 Qt 也能启动一个可交互、可常驻、可置顶的桌宠空壳。

### 阶段 2：状态与存档重建

目标：把 Qt 的本地 ini 状态迁成新架构可维护的数据模型。

状态：进行中

已完成：

- 用户、设备、宠物状态、会话消息等基础 shared types 已定义在 [packages/shared/src/contracts/api.ts](packages/shared/src/contracts/api.ts)

进行中：

- 新的 API 状态边界已经立住，但还是内存假数据，位置在 [apps/api/src/index.ts](apps/api/src/index.ts)

未开始：

- xiuxian.ini 和 xiuxian_chat.ini 还没映射到新 store
- 还没有替代 QSettings 的正式存储方案
- 首次启动和身份初始化仍未迁移

完成标志：Qt 的 QSettings 不再是主数据来源。

### 阶段 3：聊天能力替换

目标：用新的 API + WebSocket 替代旧聊天协议。

状态：进行中

已完成：

- React 聊天面板已建立，并已接入 API + WebSocket 链路，位置在 [apps/desktop/src/app/features/chat/ChatPanel.tsx](apps/desktop/src/app/features/chat/ChatPanel.tsx)
- Fastify + WebSocket 的新聊天链路已存在，位置在 [apps/api/src/index.ts](apps/api/src/index.ts)

进行中：

- 已开始向 Qt ChatWindow 靠拢，补了快捷提示、时间戳、键盘发送、系统消息和失败提示
- OpenClaw adapter 已接入消息流，但默认仍可走 mock，位置在 [apps/api/src/modules/openclaw/index.ts](apps/api/src/modules/openclaw/index.ts)

未开始：

- 头像、表情选择器、聊天偏好等 Qt 聊天细节还没迁
- 敏感词过滤还没完全迁到服务端
- 旧的 ConServSocket 协议还没有正式下线

完成标志：聊天不再依赖 Qt socket 线程和 base64 文本协议。

### 阶段 4：功能模块迁移

目标：把排行榜、反馈、占卜、江湖模块迁入新的 feature 架构。

状态：未开始

已完成：

- 文档里已经明确了排行榜、反馈、占卜、江湖的目标落点

进行中：

- 无

未开始：

- 排行榜、反馈、占卜、江湖都还没有进入 React feature 的实作阶段

完成标志：原来独立 Qt 对话框的大部分业务功能都能在 React 中访问。

### 阶段 5：安全与边界收口

目标：把旧客户端的不安全边界彻底移除。

状态：进行中

已完成：

- Python 服务和 Node API 已补一轮安全加固，鉴权和 OpenClaw adapter 边界已经收口一部分

进行中：

- Qt 客户端到服务端的传输配置已开始统一，但旧业务边界还没有彻底拆掉

未开始：

- 客户端直连 Redis、客户端关键数值逻辑、排行榜服务端化等还没有完全收口
- 旧私有协议还没有彻底移除

完成标志：前端只负责展示和交互，关键业务状态以服务端为准。


## 四、优先级排序

### P0：先做

- Tauri 主窗壳子
- 自启动、托盘、置顶控制
- 本地存储模型重建
- 聊天协议替换

### P1：随后做

- 排行榜 JSON 化
- 反馈面板迁移
- 通知系统迁移
- 皮肤系统迁移

### P2：最后做

- 占卜模块
- 江湖扩展玩法
- 各类动画和视觉细节


## 五、建议的代码归属

### 放到 src-tauri

- 窗口控制
- 自启动
- 托盘
- 通知
- 文件选择和本地文件访问
- 设备级标识缓存

### 放到 apps/desktop/src

- pet-shell
- chat-panel
- rank view
- feedback panel
- settings
- skin manager
- startup wizard

### 放到 apps/api/src

- 设备登录
- 宠物状态同步
- 排行榜
- 会话消息
- 敏感词校验
- 江湖积分和排名
- 审计与限流


## 六、迁移时不要保留的旧做法

- 不要继续使用 Windows 注册表作为唯一自启动实现
- 不要继续把数据保存在固定 AppData 字符串路径
- 不要继续让客户端直接生成关键业务身份和数值
- 不要继续让排行榜依赖服务端返回 HTML
- 不要继续保留客户端直连 Redis
- 不要继续把安全校验只放在客户端


## 七、建议的第一批落地项

如果要尽快看到成果，建议第一批只做下面 6 项：

- [ ] Tauri 主窗透明、置顶、无边框
- [ ] Tauri 自启动和托盘
- [ ] React 版主状态卡片
- [ ] React 版聊天面板接现有 API
- [ ] 统一 settings 存储
- [ ] 排行榜 API 改 JSON

这样能最快形成“新桌宠壳子已经能跑”的里程碑，同时不会被江湖和占卜这些支线模块拖慢。