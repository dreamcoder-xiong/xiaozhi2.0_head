#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "mmap_generate_lottie_assets.h"
#include "mmap_generate_weather.h"
#include "mmap_generate_audio.h"
#ifdef __cplusplus
extern "C" {
#endif


esp_err_t app_animation_init(void);
void ui_init_timer();
extern mmap_assets_handle_t asset_lottie;
extern mmap_assets_handle_t asset_weather;
extern mmap_assets_handle_t asset_audio;
#ifdef __cplusplus
}
#endif