# 给对面 AI 代理的移植指令

> 本文件是给接收方 AI 代理的执行提示词。请按步骤执行，并在每一步后向用户汇报结果。

## 角色与目标

你负责把一套已验证的 ESP32-S3 小智机器人固件迁移到本机，并完成构建、烧录和功能验证。项目是官方 `xiaozhi-esp32 v1.8.6` 的定制版（esp-sparkbot 改版硬件），已验证云端视觉、语音对话、动态待机表情。

**请全程用中文回复。操作前说明目的和影响，操作后汇报结果。不确定时列选项让用户决定，禁止猜测。**

## 输入文件

用户会提供两个压缩包（可能只给主包）：

1. `xiaozhi2-sparkbot-1.8.6-custom-src.zip` —— **必需**，源码 + 配置 + 已验证固件 + 文档
2. `xiaozhi2-sparkbot-1.8.6-custom-deps.zip` —— **可选**，`managed_components/`（469MB 依赖）

解压后项目根目录应包含：`main/`、`.git/`、`sdkconfig`、`dependencies.lock`、`build_sparkbot.ps1`、`MIGRATION.md`、`PROMPT_FOR_AGENT.md`。

## 执行步骤

### 第 0 步：环境检查（先做，不要直接构建）

在 PowerShell 中检查：

```powershell
ls F:\Espressif\frameworks\          # 期望存在 esp-idf-v5.5.5
ls F:\Espressif\python_env\          # 期望存在 idf5.5_py3.11_env
```

- 如果有 **5.5.5**：进入下一步
- 如果只有其他版本或没有：先告诉用户需要安装 ESP-IDF 5.5.5（与当前项目要求一致），**不要擅自升级到 6.x**
- 同时检查设备串口：`[System.IO.Ports.SerialPort]::GetPortNames()`

### 第 1 步：解压与路径

1. 把主包解压到**无中文、无空格**的路径（建议 `F:\Company\xiaozhi-esp32-1.8.6-sparkbot`）
2. 如果用户提供了依赖包，解压后把 `managed_components/` 放到项目根目录
3. 修改两处硬编码路径（先给用户看 diff，再改）：
   - `build_sparkbot.ps1`：
     - `$env:IDF_PYTHON_ENV_PATH` 指向本机 `idf5.5_py3.11_env`
     - `export.ps1` 路径指向本机 `esp-idf-v5.5.5`
     - `Set-Location` 指向本机项目路径
   - `.vscode/settings.json`：`idf.currentSetup` 改为本机 IDF 路径

### 第 2 步：确认配置

检查项目根目录存在：

- `sdkconfig`（完整配置，主包已带）
- `dependencies.lock`（75 个组件版本锁，主包已带）
- `main/boards/esp-sparkbot/config.h` 包含 6 处定制 GPIO

如果 `sdkconfig` 缺失，先让用户确认：可以从 `sdkconfig.defaults.esp32s3` 重新生成，但可能丢失隐藏配置。

### 第 3 步：构建

**必须在 PowerShell 中执行**（Git Bash 会因 MSYSTEM 变量导致 idf.py 拒绝运行）：

```powershell
powershell -ExecutionPolicy Bypass -File "F:\Company\xiaozhi-esp32-1.8.6-sparkbot\build_sparkbot.ps1"
```

- 首次构建可能需要下载组件。如果卡在 `esp-sr` 等大组件，提示用户设置代理：
  ```powershell
  $env:HTTP_PROXY = "http://127.0.0.1:<端口>"
  $env:HTTPS_PROXY = "http://127.0.0.1:<端口>"
  ```
- 成功标志：`Project build complete`，生成 `build/xiaozhi.bin`

### 第 4 步：烧录

先让用户确认设备已连接、COM 号正确：

```powershell
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
$env:IDF_PYTHON_ENV_PATH = "<本机 idf5.5_py3.11_env>"
& "<本机 esp-idf-v5.5.5>\export.ps1"
cd <项目路径>
idf.py -p COMx flash monitor
```

- **禁止**使用 `erase-flash` 或全片擦除（会丢 WiFi 凭据和虚拟 MAC 换绑）
- 烧录失败时先检查 COM 口是否被 monitor 占用

### 第 5 步：验证（按顺序汇报）

1. 启动日志包含：`App version: 1.8.6`、`Detected OV2640 camera`、`Es8311AudioCodec initialized`、`Got IP`、`MQTT: Connected`
2. 待机时屏幕显示黑底大眼睛动画，日志有：
   ```
   IdleFace: Idle face ready (240x240), 3 animations
   IdleFace: state 3 -> idle face shown
   IdleFace: face -> look / think / ask   （每 3~8 秒一次）
   ```
3. 说“你好小智”：唤醒成功，动画隐藏，回到 emoji + 文字界面
4. 说“看看周围”：触发 `self.camera.take_photo`，云端播报画面内容
5. 拍照方向上下正确
6. 无 `Guru Meditation Error`、无重启循环

### 第 6 步：交付报告

向用户输出：

- 环境版本（IDF、工具链、Python）
- 项目路径
- 构建产物大小
- 烧录端口
- 每项验证结果（通过/失败）
- 如果失败：完整错误日志 + 已尝试的排查 + 建议

## 禁止事项

- ❌ 不要升级到 ESP-IDF 6.x
- ❌ 不要拷贝或复用原机器的 `build/` 目录（本包未包含）
- ❌ 不要执行 `erase-flash`
- ❌ 不要 push 到 `origin`（远程是官方仓库，无权限）
- ❌ 不要删除 `managed_components/` 后强行构建（会触发大组件重新下载）
- ❌ 不要修改 6 处 GPIO 适配，除非用户明确要求

## 失败排查提示

| 现象 | 可能原因 |
|---|---|
| `idf.py` 报 MSYSTEM 相关错误 | 在 Git Bash 里运行了，改用 PowerShell |
| 组件下载卡住 | 网络问题，设置代理 |
| 编译报 ThorVG pthread 错误 | `CONFIG_THORVG_THREAD_ENABLED` 未关闭 |
| 启动即 `LoadProhibited` 崩溃 | ThorVG 的 `set_target`/`push` 顺序被改错 |
| 花屏/闪烁 | 烧了官方原版固件（GPIO 不匹配），需用本包源码构建 |
| 屏幕上下颠倒 | 摄像头 VFlip 配置被改回开启 |
| 无法连接 WiFi | NVS 被擦除，需要重新配网 |

## 补充：虚拟 MAC 验证（本版本新增）

设备之前做过换绑，服务器绑定的是虚拟 MAC `ac:a7:04:xx:xx:xx`。
本版本已移植 `components/esp_hi_rebind`，会从 NVS `rebind/mac` 读取并上报。

烧录后先在日志中确认：

```
RebindMac: Device-Id MAC: ac:a7:04:xx:xx:xx (chip: ac:a7:04:yy:yy:yy)
```

- 如果是 `4c:fb:19`：身份正确，继续测试视觉
- 如果是 `e3:bc:20`：NVS 值丢失，**不要执行 erase-flash**，
  先向用户报告，由用户确认后再处理 NVS 写入

另外注意：新增组件后首次构建需要重新跑 CMake 配置；
如果直接 build 找不到 `esp_hi_rebind.h`，先执行 `idf.py reconfigure`。
