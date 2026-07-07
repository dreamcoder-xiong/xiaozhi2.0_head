# 小智 2.0 头部机器人

> 桌面机器人头部固件，基于 ESP32-S3 + 小智开源项目二次开发。

## 硬件配置

| 模块 | 型号 | 说明 |
|------|------|------|
| 主控 | ESP32-S3 | 双核 240MHz，16MB Flash，8MB PSRAM |
| 屏幕 | ST7789 / 240×240 | SPI 接口 LCD |
| 音频输入 | MSM261S4030H0 | I2S 麦克风（16kHz） |
| 音频输出 | ES8311 | I2S DAC + 喇叭功放 |
| 摄像头 | GC032A / OV2640 | 240×240 RGB565（视觉识别） |
| 电机 | PWM 舵机 ×2 | 头部 X/Y 轴运动 |
| 触摸 | 电容触摸按键 ×4 | 音量/开关等交互 |
| 4G 模块 | ML307 | 蜂窝网络连接（可选 WiFi） |

## 功能特性

### 云端对话（小智开源基础功能）
- WebSocket / MQTT 双协议接入
- 云端 STT → LLM → TTS 全链路
- 唤醒词检测、音量调节、OTA 升级
- 多语言（中文/英文/日文）

### 视觉识别（自研）
- **触发**：说"小智，看看" / "看看周围" / "看看这个"
- **流程**：摄像头拍照 → 笔记本视觉模型（minicpm-v:8b）→ 屏幕气泡 + 喇叭语音播报
- **本地 TTS**：POST /tts 接口，edge-tts 合成 → miniaudio 解码 → I2S ES8311 播放
- **mDNS 发现**：自动发现局域网视觉服务器 `vision-server.local`
- 详见 `vision_server.py`（运行于笔记本端）

### 本地交互
- Lottie 动画表情
- 2048 小游戏
- 天气显示（高德地图 + 和风天气 API）

## 目录结构

```
├── main/                   # 主程序
│   ├── application.cc/h    # 核心应用逻辑（含视觉识别 HandleVisionCommand）
│   ├── boards/             # 板级配置（40+ 开发板）
│   │   └── esp-sparkbot/   # ★ 我们的定制板
│   │       ├── config.h    # GPIO 引脚定义
│   │       ├── ui/         # LVGL 界面
│   │       ├── wifi/       # WiFi 配网 & web server
│   │       └── ...         # 游戏、Lottie、驱动
│   ├── protocols/          # WebSocket / MQTT 协议
│   ├── audio_codecs/       # 音频编解码（ES8311/ES8388）
│   ├── display/            # LCD / OLED 显示驱动
│   └── assets/             # 多语言声音资源
├── components/             # 第三方组件（字体、摄像头、I2C）
├── docs/                   # 硬件接线图、文档
├── scripts/                # 编译辅助脚本
├── partitions.csv          # 分区表
└── sdkconfig.defaults      # 默认 Kconfig
```

## 开发环境

| 工具 | 版本 |
|------|------|
| ESP-IDF | v5.3.2 |
| 工具链 | xtensa-esp-elf 13.2.0 |
| Python | 3.12+ |
| CMake | ≥ 3.16 |

## 编译 & 烧录

```powershell
# 1. 激活 ESP-IDF 环境
F:\Espressif\frameworks\esp-idf-v5.3.2\export.ps1

# 2. 编译（esp-sparkbot 板）
idf.py build

# 3. 烧录（替换 COM 口号）
idf.py -p COM10 flash

# 4. 串口监控
idf.py -p COM10 monitor
```

> ⚠️ 注意：ESP-IDF 不支持 MSys2/bash，必须用 PowerShell 编译。
> 如遇中文路径编译错误，设置 `$env:CCACHE_DISABLE=1; $env:PYTHONUTF8=1`。

## 视觉服务（配套）

笔记本端运行 `vision_server.py`（不在本仓库），提供：
- `GET /vision?question=...` → 拍照 + minicpm-v:8b 识别
- `POST /tts` → edge-tts 语音合成（WAV）
- `GET /preview` → 调试用查看最新画面

## 协作说明

- **主分支**：`main`（保护分支，需 PR 合并）
- **开发分支**：`dev`（日常开发）
- **个人分支**：`feature/xxx`（功能开发）
- 提交信息用中文，简要说明改了什么
- 不要提交 `build/`、`managed_components/`、`sdkconfig`（已在 `.gitignore`）

## 上游开源

本项目基于 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)（MIT License），感谢原作者。

## 许可证

MIT License — 详见 [LICENSE](LICENSE)
