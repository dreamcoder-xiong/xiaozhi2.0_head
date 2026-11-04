# 二代机器人固件迁移说明（xiaozhi-esp32 v1.8.6 定制版）

## 1. 项目概况

| 项 | 内容 |
|---|---|
| 硬件 | ESP32-S3-WROOM-1U-N16R8，esp-sparkbot 改版（240×240 ST7789、OV2640、ES8311、履带底盘） |
| 固件 | 官方 xiaozhi-esp32 **v1.8.6** + 定制适配 |
| 已验证功能 | 语音对话、云端视觉（拍照分析）、动态待机表情、底盘/灯光 MCP 工具 |
| git 标签 | `v1.8.6-sparkbot-custom`（视觉基线）、`v1.8.6-sparkbot-custom-face`（含动态表情） |

关键定制：

1. **6 处 GPIO 适配**：`main/boards/esp-sparkbot/config.h`
   - `DISPLAY_CS_GPIO` = 46（官方 44）
   - `DISPLAY_BACKLIGHT_PIN` = 44（官方 46）
   - `DISPLAY_BACKLIGHT_OUTPUT_INVERT` = true
   - `AUDIO_CODEC_I2C_SDA_PIN` = 0（官方 4）
   - `AUDIO_CODEC_PA_PIN` = NC
   - `BOOT_BUTTON_GPIO` = NC
2. **禁用 OTA**：`main/application.cc:109` → `if (false && ota.HasNewVersion())`
3. **摄像头方向**：`esp_sparkbot_board.cc` 中 HMirror=开、VFlip=关
4. **动态待机表情**：`sparkbot_lottie.c` + `sparkbot_idle_face.cc`（ThorVG + Lottie，idle 时每 3~8 秒随机切换 look/think/ask）

## 2. 压缩包内容

### 主包：`xiaozhi2-sparkbot-1.8.6-custom-src.zip`

- 完整源码 + `.git`（含官方历史和全部定制提交）
- `sdkconfig`（完整编译配置）
- `dependencies.lock`（75 个组件版本锁）
- `firmware/xiaozhi_v1.8.6_custom.bin`（已验证固件，可直接烧录）
- `docs/ai-context/`（项目上下文，供 AI 代理参考）
- `MIGRATION.md`、`PROMPT_FOR_AGENT.md`

### 依赖包：`xiaozhi2-sparkbot-1.8.6-custom-deps.zip`（可选）

- `managed_components/`（469MB，75 个组件，含 ThorVG / esp-sr / esp-dl）
- 不传则首次构建会联网下载；其中 esp-sr 超过 100MB，无代理时容易卡住

## 3. 环境要求

| 组件 | 版本 | 说明 |
|---|---|---|
| ESP-IDF | **5.5.5** | 必须一致；6.x 不兼容当前组件 |
| Python | 3.11 | IDF 自带 `idf5.5_py3.11_env` |
| 工具链 | `xtensa-esp-elf esp-14.2.0_20260121` | 随 IDF 5.5.5 安装 |
| 系统 | Windows 10/11 + PowerShell | 不要在 Git Bash 里运行 idf.py（MSYSTEM 会报错） |
| 驱动 | ESP32-S3 USB Serial/JTAG | Win10/11 通常免驱；若板子用 CP210x/CH340 需另装 |

## 4. 迁移步骤

1. 解压主包到**无中文、无空格**的路径，例如：
   `F:\Company\xiaozhi-esp32-1.8.6-sparkbot`
2. （可选）解压依赖包，把 `managed_components` 放到项目根目录
3. 修改硬编码路径（两处）：
   - `build_sparkbot.ps1`：`IDF_PYTHON_ENV_PATH`、`export.ps1`、`Set-Location`
   - `.vscode/settings.json`：`idf.currentSetup`
4. 确认 `sdkconfig`、`dependencies.lock` 位于项目根目录（主包已包含）
5. 构建（PowerShell）：
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\build_sparkbot.ps1
   ```
6. 烧录（COM 号以设备管理器为准）：
   ```powershell
   Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
   $env:IDF_PYTHON_ENV_PATH = "F:\Espressif\python_env\idf5.5_py3.11_env"
   & "F:\Espressif\frameworks\esp-idf-v5.5.5\export.ps1"
   cd <项目路径>
   idf.py -p COMx flash
   ```
7. 按第 6 节验证

## 5. 构建/烧录完整命令

```powershell
# 构建
powershell -ExecutionPolicy Bypass -File "F:\Company\xiaozhi-esp32-1.8.6-sparkbot\build_sparkbot.ps1"

# 烧录 + 监控
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
$env:IDF_PYTHON_ENV_PATH = "F:\Espressif\python_env\idf5.5_py3.11_env"
& "F:\Espressif\frameworks\esp-idf-v5.5.5\export.ps1"
cd F:\Company\xiaozhi-esp32-1.8.6-sparkbot
idf.py -p COMx flash monitor
```

## 6. 验证清单

启动日志应包含：

```
App version:      1.8.6
Detected OV2640 camera
Es8311AudioCodec initialized
Got IP: 192.168.x.x
MQTT: Connected to endpoint
IdleFace: Idle face ready (240x240), 3 animations
IdleFace: state 3 -> idle face shown
```

待机时：

- 屏幕显示黑底大眼睛动画
- 每 3~8 秒日志出现 `IdleFace: face -> look/think/ask`
- 画面无重复图形、无彩条

对话测试：

- 说“你好小智”：唤醒成功，动画消失，回到 emoji + 文字界面
- 说“看看周围”：日志出现 `self.camera.take_photo`，云端播报图片内容
- 拍照画面**上下不颠倒**
- 全程无 `Guru Meditation Error`、无重启循环

## 7. 已知坑（务必注意）

1. **不要拷贝 `build/`**：含绝对路径缓存，换机器后容易编译异常；删除后重新构建
2. **不要执行 `idf.py erase-flash` 或全片擦除**：会丢失 NVS 中的 WiFi 凭据和虚拟 MAC 换绑信息
3. **不要 push 到 origin**：远程指向官方 `78/xiaozhi-esp32`，无推送权限
4. **ThorVG 调用顺序**：`tvg_swcanvas_set_target()` 必须在 `tvg_canvas_push()` 之前，否则 `surface` 为 NULL，启动即崩溃重启
5. **ThorVG 线程**：`CONFIG_THORVG_THREAD_ENABLED` 必须关闭（xtensa 下 pthread 检测失败）
6. **IDF 版本**：必须 5.5.5；6.x 会因组件依赖/API 变化编译失败
7. **串口号变化**：ESP32-S3 USB Serial/JTAG 的 COM 号换机器后会变
8. **组件下载卡住**：需要时设置代理，例如
   ```powershell
   $env:HTTP_PROXY = "http://127.0.0.1:7897"
   $env:HTTPS_PROXY = "http://127.0.0.1:7897"
   ```

## 8. 回滚方式

```bash
git reset --hard v1.8.6-sparkbot-custom        # 回到视觉基线（无表情）
git reset --hard v1.8.6-sparkbot-custom-face   # 回到含动态表情版本
```

或直接烧录 `firmware/xiaozhi_v1.8.6_custom.bin`。

## 9. 虚拟 MAC（换绑身份恢复）——重要

设备做过换绑，服务器控制台绑定的设备 MAC 是虚拟 MAC `ac:a7:04:4c:fb:19`。
1.8.6 原始固件只上报物理 MAC `ac:a7:04:e3:bc:20`，会导致服务器识别为另一台设备，
视觉配置（vision url/token）对不上。

已新增 `components/esp_hi_rebind`：
- 从 NVS `rebind/mac` 读取虚拟 MAC，作为 `SystemInfo::GetMacAddress()` 的返回值
- 只影响上报的 Device-Id，不改变 WiFi 实际 MAC
- NVS 无值或非法时回退芯片 MAC（不会自动生成随机 MAC）

**烧录后必须检查启动日志：**

```
I (xxx) RebindMac: Device-Id MAC: ac:a7:04:4c:fb:19 (chip: ac:a7:04:e3:bc:20)
```

- 显示 `4c:fb:19` → 身份恢复正确，控制台设备应在线，视觉配置应恢复
- 显示 `e3:bc:20` → NVS 中的 rebind/mac 已丢失，需要手动写入虚拟 MAC 后再试

> 注意：如果 NVS 值丢失，不要用 `erase-flash`，也不要重新配网；
> 需要单独把 `rebind/mac = ac:a7:04:4c:fb:19` 写入 NVS（可临时加一段 NVS 写入代码）。
