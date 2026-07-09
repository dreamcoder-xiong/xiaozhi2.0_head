# 下次会话提示词

直接复制下面这段话发给 Claude：

---

继续视觉任务攻坚。笔记本 192.168.1.13（静态IP）、机器人 192.168.1.2、COM10。
第九轮稳定版——全链路 ~17-20s，46 个问题已解决。

参考文件：
- 完整记录：`C:\Users\86188\Desktop\study note\2026\7.09-vision-full-summary.md`
- Claude 记忆：`C:\Users\86188\.claude\projects\C--Users-86188\memory\vision-integration.md`

关键配置速查：
- 固件：USE_MDNS=0，固定 IP 192.168.1.13:8080，WDT 30s，I2S 输入暂停
- 服务端：ROBOT_IP=192.168.1.2，num_predict=50，MAX_TTS_CHARS=50
- 启动服务：`cd "C:\Users\86188\Desktop\with claude" && python vision_server.py`
- 编译烧录：`powershell -ExecutionPolicy Bypass -File "F:/head_code/rebuild.ps1"`
- 串口监控：`python F:\head_code\monitor_com10.py`

当前已知问题：
1. 播完 ~2s 延迟（云端 WebSocket 重建）
2. 全链路 ~17-20s（可考虑流式推理/量化模型）

我现在需要：[在此描述你的需求]

---
