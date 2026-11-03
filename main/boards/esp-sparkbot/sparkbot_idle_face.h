#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化待机动态表情层。
 * 设备进入 idle 状态时自动显示，唤醒/对话时自动隐藏。
 * 需在显示模块初始化完成后调用。
 */
void sparkbot_idle_face_init(void);

#ifdef __cplusplus
}
#endif
