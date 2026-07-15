# RouterMonitor

一个用于监控 ZTE RAX3000Ze (CMCC ZXW ZD.01) 路由器所有接入设备实时上下行速度的 Windows 桌面程序。

## 功能特性

- 📊 **实时表格**：所有接入设备当前上下行速度，按速率自动排序
- 🪟 **HUD 悬浮窗**：半透明置顶悬浮窗，显示总上下行速率
- 📈 **历史趋势图**：选中设备后查看近 10 分钟速率曲线
- 🔔 **系统托盘**：最小化到托盘，后台静默运行
- ⚡ **轻量**：单文件 exe ~1.5 MB，零运行时依赖，CPU 占用极低

## 截图

(运行后填入截图)

## 编译要求

- Windows 10/11 x64
- Visual Studio 2022 (含 "使用 C++ 的桌面开发" 工作负载) **或** CMake 3.15+
- Windows 10 SDK

## 一键编译

打开 "x64 Native Tools Command Prompt for VS 2022"，进入项目目录：

```cmd
cd O:\try\RouterMonitor
build.bat
```

或在 Git Bash / PowerShell：

```bash
cd /o/try/RouterMonitor
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

编译产物：`build\bin\Release\RouterMonitor.exe`

## 使用方法

1. 首次运行，在弹出"设置"对话框中填入：
   - **路由器地址**（默认 `192.168.x.1`）
   - **管理员用户名**（默认 `admin`）
   - **管理员密码**
2. 点击"登录"，看到主窗口表格中显示设备列表即为成功
3. 右键托盘图标可切换 HUD、退出等

配置文件位置：`%APPDATA%\RouterMonitor\config.ini`

## 已知 TODO

下面是实施过程中需要逐步确认的协议细节（实施时所有请求/响应都会写入日志）：

| # | 假设 | 验证方式 |
|---|---|---|
| 1 | 登录密码以明文 POST | 若路由器返回 `result != 0` 或 401，则需逆向 JS 中的加密 |
| 2 | SessionId 用作后续请求 Body 字段或 Cookie 头 | 查看日志对照浏览器行为 |
| 3 | 设备列表 API Body 字段名 | 当前假设 `{"module":"wifi","action":"get_sta_list"}`，按实际情况调整 |
| 4 | `txrate`/`rxrate` 单位 | 当前假设为 Mbps 字符串，对照实际速率调整 |

## ZTE RAX3000Ze API 抓包笔记

### 抓包流程

1. 浏览器登录 `http://192.168.x.1/`
2. F12 → Network → 过滤 `middleware.cgi`
3. 主要接口：
   - `POST /cgi-bin/middleware.cgi`  Content-Type: application/json
4. 登录响应示例：
   ```json
   {"token":"1635728946","login_lock_sec":60,"login_remain_times":5,"login_total_times":5,"guestloginflag":0,"result":0}
   ```
5. 后续请求带 `sessionId`（base64）：
   ```json
   {"result":0,"account_level":3,"sessionId":"NjE3ZjNlMzIzYTdkOWY5ZTAzYTRlNzZk"}
   ```
6. 设备列表响应（节选）：
   ```json
   {
     "data": [
       {
         "mac":"AA:BB:CC:DD:EE:FF",
         "devname":"anonymous",
         "ipaddr":"192.168.x.10",
         "radio":"2.4G",
         "txrate":"0.000",
         "rxrate":"0.000",
         "uptime":0
       }
     ]
   }
   ```

## 项目结构

```
RouterMonitor/
├── CMakeLists.txt
├── build.bat                # 一键编译脚本
├── include/
│   ├── HttpClient.h         # WinHTTP 封装
│   ├── JsonParser.h         # 极简 JSON 解析
│   ├── Config.h             # INI 配置
│   ├── RouterApi.h          # 路由器协议封装
│   ├── PollerThread.h       # 后台轮询
│   ├── RingBuffer.h         # 环形缓冲
│   └── ui/
│       ├── MainWindow.h
│       ├── HudWindow.h
│       └── TrayIcon.h
└── src/
    ├── main.cpp
    ├── HttpClient.cpp
    ├── JsonParser.cpp
    ├── Config.cpp
    ├── RouterApi.cpp
    ├── PollerThread.cpp
    └── ui/
        ├── MainWindow.cpp
        ├── HudWindow.cpp
        └── TrayIcon.cpp
```

## 协议说明

本项目基于 MIT 协议开源。路由器交互代码仅供学习与个人使用。