#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 创建 Lottie 动画层（全屏 canvas，覆盖在当前屏幕上）。
 * 必须在 LVGL 锁内调用。
 */
lv_obj_t* sparkbot_lottie_create(lv_obj_t* parent, int width, int height);

/**
 * 设置/切换动画源（JSON 数据）。
 * fps 为动画自身帧率（用于换算每帧步进），传 0 时按 12fps 处理。
 * 返回 true 表示加载成功。
 */
bool sparkbot_lottie_set_src(lv_obj_t* obj, const void* json, size_t len, float fps);

/**
 * 显示/隐藏动画。隐藏时暂停渲染以节省 CPU。
 */
void sparkbot_lottie_set_visible(lv_obj_t* obj, bool visible);

#ifdef __cplusplus
}
#endif
