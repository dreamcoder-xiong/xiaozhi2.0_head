/**
 * 待机动态表情播放器（Lottie + ThorVG）
 *
 * 移植自 v1.5.2 定制版的 lottie_player/lv_lottie.c，
 * 适配 LVGL 9：不再注册自定义 widget class，改用普通 lv_canvas + lv_timer。
 */

#include "sparkbot_lottie.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "thorvg_capi.h"

static const char* TAG = "SparkbotLottie";

#define LOTTIE_FRAME_PERIOD_MS 66  /* 15fps 刷新，按动画自身 fps 换算步进 */

typedef struct {
    lv_obj_t* canvas;
    Tvg_Canvas* tvg_canvas;
    Tvg_Animation* tvg_anim;
    Tvg_Paint* tvg_paint;
    uint8_t* fb;              /* ARGB8888 渲染缓冲（RGB565 canvas 共用，见下） */
    int32_t w;
    int32_t h;
    float frame;
    float total_frames;
    float fps;
    lv_timer_t* timer;
    bool visible;
    bool loaded;
} sparkbot_lottie_t;

static sparkbot_lottie_t* GetLottie(lv_obj_t* obj)
{
    return obj ? (sparkbot_lottie_t*)lv_obj_get_user_data(obj) : NULL;
}

/* 说明：ThorVG 直接输出 ARGB8888（内存序 BGRA），
 * 由 LVGL 的 canvas 负责转换到屏幕的 RGB565，
 * 避免手动转换时 stride/字节序出错。 */

static void RenderFrame(sparkbot_lottie_t* lottie)
{
    if (lottie == NULL || !lottie->loaded) {
        return;
    }

    lottie->frame += (LOTTIE_FRAME_PERIOD_MS / 1000.0f) * lottie->fps;
    if (lottie->frame >= lottie->total_frames) {
        lottie->frame -= lottie->total_frames;
    }

    tvg_animation_set_frame(lottie->tvg_anim, lottie->frame);
    tvg_canvas_update(lottie->tvg_canvas);
    tvg_canvas_draw(lottie->tvg_canvas);
    tvg_canvas_sync(lottie->tvg_canvas);

    lv_obj_invalidate(lottie->canvas);
}

static void RenderTimerCb(lv_timer_t* timer)
{
    sparkbot_lottie_t* lottie = (sparkbot_lottie_t*)lv_timer_get_user_data(timer);
    if (lottie != NULL && lottie->visible) {
        RenderFrame(lottie);
    }
}

lv_obj_t* sparkbot_lottie_create(lv_obj_t* parent, int width, int height)
{
    if (parent == NULL || width <= 0 || height <= 0) {
        return NULL;
    }

    sparkbot_lottie_t* lottie = (sparkbot_lottie_t*)calloc(1, sizeof(sparkbot_lottie_t));
    if (lottie == NULL) {
        ESP_LOGE(TAG, "No memory for lottie context");
        return NULL;
    }
    lottie->w = width;
    lottie->h = height;

    /* 4 字节/像素：ThorVG 先按 ARGB8888 渲染，再原地转成 RGB565。
     * canvas 只使用前 w*h*2 字节，因此缓冲区大小必须按 4 字节分配。 */
    size_t fb_size = (size_t)width * height * 4;
    lottie->fb = (uint8_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (lottie->fb == NULL) {
        ESP_LOGW(TAG, "PSRAM alloc failed, falling back to internal RAM");
        lottie->fb = (uint8_t*)malloc(fb_size);
    }
    if (lottie->fb == NULL) {
        ESP_LOGE(TAG, "Frame buffer alloc failed (%u bytes)", (unsigned)fb_size);
        free(lottie);
        return NULL;
    }
    memset(lottie->fb, 0, fb_size);

    lottie->canvas = lv_canvas_create(parent);
    lv_obj_set_pos(lottie->canvas, 0, 0);
    lv_obj_set_size(lottie->canvas, width, height);
    lv_obj_remove_flag(lottie->canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(lottie->canvas, 0, 0);
    lv_obj_set_style_border_width(lottie->canvas, 0, 0);
    lv_obj_set_style_pad_all(lottie->canvas, 0, 0);
    lv_obj_set_style_bg_color(lottie->canvas, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lottie->canvas, LV_OPA_COVER, 0);
    lv_obj_add_flag(lottie->canvas, LV_OBJ_FLAG_HIDDEN);

    lv_canvas_set_buffer(lottie->canvas, lottie->fb, width, height, LV_COLOR_FORMAT_ARGB8888);

    if (tvg_engine_init(TVG_ENGINE_SW, 0) != TVG_RESULT_SUCCESS) {
        ESP_LOGE(TAG, "ThorVG engine init failed");
    }
    lottie->tvg_canvas = tvg_swcanvas_create();
    lottie->tvg_anim = tvg_animation_new();
    if (lottie->tvg_canvas == NULL || lottie->tvg_anim == NULL) {
        ESP_LOGE(TAG, "ThorVG canvas/anim create failed");
        lv_obj_del(lottie->canvas);
        free(lottie->fb);
        free(lottie);
        return NULL;
    }
    lottie->tvg_paint = tvg_animation_get_picture(lottie->tvg_anim);

    /* 注意顺序：必须先设置 target 再 push。
     * ThorVG 的 Canvas::push() 会把 status 改为 Updating，
     * 之后 target() 会因状态不满足直接返回失败（surface 保持 NULL），
     * 再 draw 就会在 rasterClear(surface) 处崩溃。 */
    Tvg_Result target_result = tvg_swcanvas_set_target(
        lottie->tvg_canvas, (uint32_t*)lottie->fb, width, width, height,
        TVG_COLORSPACE_ARGB8888);
    if (target_result != TVG_RESULT_SUCCESS) {
        ESP_LOGE(TAG, "ThorVG set target failed (%d)", (int)target_result);
        tvg_animation_del(lottie->tvg_anim);
        tvg_canvas_destroy(lottie->tvg_canvas);
        tvg_engine_term(TVG_ENGINE_SW);
        lv_obj_del(lottie->canvas);
        free(lottie->fb);
        free(lottie);
        return NULL;
    }
    tvg_canvas_push(lottie->tvg_canvas, lottie->tvg_paint);

    lottie->timer = lv_timer_create(RenderTimerCb, LOTTIE_FRAME_PERIOD_MS, lottie);
    lv_timer_pause(lottie->timer);

    lv_obj_set_user_data(lottie->canvas, lottie);
    return lottie->canvas;
}

bool sparkbot_lottie_set_src(lv_obj_t* obj, const void* json, size_t len, float fps)
{
    sparkbot_lottie_t* lottie = GetLottie(obj);
    if (lottie == NULL || json == NULL || len == 0) {
        return false;
    }

    /* 切换动画源时重建动画对象，避免旧动画数据残留或泄漏 */
    if (lottie->loaded) {
        tvg_canvas_clear(lottie->tvg_canvas, true);
        tvg_animation_del(lottie->tvg_anim);
        lottie->tvg_anim = tvg_animation_new();
        if (lottie->tvg_anim == NULL) {
            lottie->loaded = false;
            return false;
        }
        lottie->tvg_paint = tvg_animation_get_picture(lottie->tvg_anim);
        tvg_canvas_push(lottie->tvg_canvas, lottie->tvg_paint);
    }

    Tvg_Result result = tvg_picture_load_data(lottie->tvg_paint, json, (uint32_t)len,
                                              "lottie", true);
    if (result != TVG_RESULT_SUCCESS) {
        ESP_LOGE(TAG, "Failed to load lottie json (%d)", (int)result);
        return false;
    }

    tvg_picture_set_size(lottie->tvg_paint, lottie->w, lottie->h);

    float total = 0.0f;
    tvg_animation_get_total_frame(lottie->tvg_anim, &total);
    if (total <= 0.0f) {
        ESP_LOGE(TAG, "Invalid lottie frame count");
        return false;
    }

    lottie->total_frames = total;
    lottie->frame = 0.0f;
    lottie->fps = (fps > 0.0f) ? fps : 12.0f;
    lottie->loaded = true;
    RenderFrame(lottie);
    return true;
}

void sparkbot_lottie_set_visible(lv_obj_t* obj, bool visible)
{
    sparkbot_lottie_t* lottie = GetLottie(obj);
    if (lottie == NULL) {
        return;
    }

    lottie->visible = visible;
    if (visible) {
        lv_obj_remove_flag(lottie->canvas, LV_OBJ_FLAG_HIDDEN);
        lv_timer_resume(lottie->timer);
        RenderFrame(lottie);
    } else {
        lv_obj_add_flag(lottie->canvas, LV_OBJ_FLAG_HIDDEN);
        lv_timer_pause(lottie->timer);
    }
}
