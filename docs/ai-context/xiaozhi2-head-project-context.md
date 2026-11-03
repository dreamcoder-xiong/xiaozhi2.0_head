---
name: xiaozhi2-head-project-context
description: "二代机器人（xiaozhi2.0_head, ESP32-S3 esp-sparkbot 定制板）项目上下文：视觉已打通（官方1.8.6+GPIO适配）、双版本布局、环境坑"
metadata: 
  node_type: memory
  type: project
  originSessionId: ed94fecf-cb06-4468-9095-a8202bd7dd43
---

# 二代机器人项目（xiaozhi2.0_head）上下文

## 🎉 重大状态（2026-09-24）：视觉功能已打通！

**当前设备运行**：官方 xiaozhi-esp32 v1.8.6 完整固件（视觉正常工作）
- 源码目录：`F:\Company\xiaozhi-esp32-1.8.6-sparkbot`（从幻尔包复制，非 git）
- 构建环境：**IDF 5.5.5**（1.8.6 要求 ≥5.4）+ `idf5.5_py3.11_env`
- 视觉链路：语音"看看周围/拍照" → 云端大模型调 `self.camera.take_photo` → 设备拍照(OV2640 320x240) → HTTP POST 到 `api.xiaozhi.me/vision/explain` → 分析文本回传 → 云端 TTS 播报

### 定制板 GPIO 适配（6 处，在 main/boards/esp-sparkbot/config.h）
| 宏 | 官方值 | 定制板值 | 原因 |
|---|---|---|---|
| DISPLAY_CS_GPIO | 44 | **46** | CS/背光互换（省三极管） |
| DISPLAY_BACKLIGHT_PIN | 46 | **44** | 同上 |
| DISPLAY_BACKLIGHT_OUTPUT_INVERT | false | **true** | 背光低有效 |
| AUDIO_CODEC_I2C_SDA_PIN | 4 | **0** | 腾 GPIO4 做触摸 |
| AUDIO_CODEC_PA_PIN | 46 | **NC** | 未接 |
| BOOT_BUTTON_GPIO | 0 | **NC** | GPIO0 被 I2C SDA 占用 |

### ⚠️ 关键坑：OTA 自动升级必须禁用！
官方 OTA 固件用原版 GPIO（SDA=GPIO4），定制板升级后 I2C 全挂 → 摄像头/音频崩 → 回滚 → 再升级 → **死循环**。
已修：application.cc 的 `if (ota.HasNewVersion())` 改为 `if (false && ...)`（保留配置获取，跳过升级）。
**v2.4.2 "花屏"根因同理**——不是驱动 bug，是官方固件 GPIO 与定制板不匹配！

### 幻尔源码包的坑
`F:\BaiduNetdiskDownload\WonderLLM大模型模块（ESP32-S3）` 里的 xiaozhi-esp32 源码被幻尔删减过 managed_components（esp-dsp 的 modules 全空、esp_codec_dev 缺 zl38063 子目录）——**必须删掉 managed_components 重新下载**（挂代理 127.0.0.1:7897，esp-sr 100MB+ 直连会卡死）。

## 项目布局（三个版本）
- `F:\Company\xiaozhi-esp32-1.8.6-sparkbot` = **视觉版（当前主力）**，官方 1.8.6 + GPIO 适配 + 禁 OTA
- `F:\Company\xiaozhi2.0_head` = **v1.5.2 定制版**（git 仓库，main + feature/rebind-mac 分支），含天气页/摄像头预览/人脸检测/触摸板/底盘/rebind 换绑，但云端视觉不通（工具被服务器过滤）
- `F:\head_code` = 旧视觉版备份（本地 Ollama 方案，只读参考）

## v1.5.2 定制版的视觉排查结论（为何放弃该路线）
- MCP 栈移植成功、tools/list 上报成功、服务器下发 vision url（token=test-token）
- 音量/亮度工具大模型能调用，**唯独 take_photo 被过滤**（AI 说"没有摄像头"）
- 排除了：工具数量、顺序、描述语言、frame_size、摄像头驱动
- 根因推测：服务器按设备型号/账号过滤摄像头工具；官方完整固件则无此问题
- v1.5.2 版本号伪装 1.8.6 也无效——服务器可能通过协议细节识别真实固件

## 定制板硬件（ESP32-S3-WROOM-1U-N16R8 esp-sparkbot 改版）
- 240×240 ST7789 SPI 屏（CS=46/背光=44 低有效）、OV2640（引脚与官方一致）、ES8311 音频（I2C SDA=0/SCL=5）
- 4 路触摸板（GPIO1-4，仅 v1.5.2 版支持）、底盘 UART（38/48）、BMI270 IMU
- 16MB Flash / 8MB PSRAM / 内部 RAM 可用 ~34KB（紧张）

## 构建环境
1. **视觉版用 IDF 5.5.5**：`$env:IDF_PYTHON_ENV_PATH="F:\Espressif\python_env\idf5.5_py3.11_env"` + export.ps1
2. **v1.5.2 用 IDF 5.3.2**：`idf5.3_py3.11_env`（I2C 头文件手动修复过，重装会丢）
3. git bash 里 idf.py 拒绝运行（MSYSTEM 检测），必须 PowerShell
4. v1.5.2 构建脚本：`build_xiaozhi.ps1`；视觉版：`build_sparkbot.ps1`
5. 烧录：`idf.py -p COM10 flash`（UART 非 JTAG）

## v1.5.2 版遗留功能（视觉版没有的）
- 天气页/摄像头预览页（左右键切换）、板载人脸检测（esp-dl 画绿框）
- 触摸板 4 键交互、rebind 网页换绑（http://设备IP/rebind.html）
- 底盘控制（Chassis MCP 工具）、IMU 手势
- 这些如需在视觉版上恢复，要在 1.8.6 源码上重新开发

## 视觉版当前状态（2026-09-24）
- ✅ 摄像头/音频/屏幕/WiFi/对话/云端视觉全部正常
- ✅ MCP 工具：chassis 前后左右/跳舞/灯效 + camera.set_camera_flipped（官方 esp-sparkbot 板级自带）
- ⚠️ 无触摸板支持（BOOT 键已禁用，交互全靠语音）
- ⚠️ OTA 已禁用（升级必崩，见上）
