# 桌面修仙 · Desktop Xiuxian

一款运行在 Windows 桌面的修仙挂机宠物，配套 Node.js 后端 API 与 Python 邮件机器人。

---

## 项目结构

```
xiuxian/
├── Client3/          # 桌面客户端（Qt 6 / C++ / CMake）← 主工程
├── apps/api/         # 后端 API（Node.js / Fastify / TypeScript）
└── ClawMail/         # 邮件机器人系统（Python）
```

---

## Client3 — Qt 6 桌面客户端

### 功能模块

| 模块 | 说明 |
|------|------|
| 桌面宠物 | 透明悬浮窗，支持 GIF 换肤、始终置顶 |
| 角色系统 | 自动生成角色（姓名/属性随机），本地 INI 存档 |
| 境界与修为 | 修炼自动累积，境界分炼气→化神十级 |
| 聊天室 | TCP 长连接，Base64 协议，内置词语过滤与 Emoji 选择器 |
| 邮件抽屉 | 收发站内信，侧边弹出，对接后端 REST API |
| 占卜 | 六爻起卦，依角色属性生成卦象 |
| 江湖历练 | 本地 NPC 对战，技能条 + 战斗 AI |
| 排行榜 | 拉取服务器修为排名 |
| 自动更新检测 | 启动时查询后端版本号 |

### 技术栈

- **Qt 6.11** · Widgets / Network / Gui
- **C++17** · CMake 3.21+
- MinGW 13（Windows）
- 无第三方依赖，纯 Qt 6 API

### 编译

```bash
# 需要 Qt 6 已安装，cmake/mingw32-make 在 PATH 中
cd Client3
cmake -B build -G "MinGW Makefiles" \
      -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64 \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build
# 产物：build/XiuxianPet.exe（windeployqt 已自动部署 DLL）
```

### 首次运行

1. 启动 `build/XiuxianPet.exe`
2. 弹出对话框输入角色名，自动创建存档
3. 右键桌面宠物打开功能菜单

---

## apps/api — 后端 API

Node.js + Fastify + TypeScript，提供：

- `POST /api/v1/auth/device-login` — 设备登录 / 注册
- `GET|PATCH /api/v1/pet/state` — 宠物状态读写
- `GET /api/v1/pet/rankings` — 修为排行榜
- `GET|POST /api/v1/social/*` — 社交 / 邮件接口

```bash
cd apps/api
npm install
npm run dev       # 开发模式，监听 :3000
```

---

## ClawMail — 邮件机器人

Python 实现的 AI 邮件代理，支持 Claude API 自动回信，基于文件 spool 目录收发。

```bash
cd ClawMail
pip install -r requirements.txt
python robotmail-server.py   # 启动服务端
python robotmail-client.py   # 启动客户端代理
```

详见 [ClawMail/README.md](ClawMail/README.md)。

---

## 开发思路（待实现）

- **财富系统** — 道友根据修为修炼可以获得对应数量金币
- **彩票系统** — 押注后瓜分金币
- **赏金任务系统** — 领取任务进行挂机历练
- **商店系统** — 购买装备提升赏金任务成功率
- **随机事件** — 在赏金任务中触发概率性事件

---

## 致谢

原始桌宠思路来自 [851896022/DesktopXiuXian](https://github.com/851896022/DesktopXiuXian) 与 [52pojie 帖子](https://www.52pojie.cn/forum.php?mod=viewthread&tid=1498266&ctid=1776)。本仓库在此基础上进行了完整重构：迁移至 Qt 6 + CMake、重写后端 API、新增多个功能模块。
