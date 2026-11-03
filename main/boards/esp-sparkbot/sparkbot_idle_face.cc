#include "sparkbot_idle_face.h"

#include "sparkbot_lottie.h"
#include "device_state_event.h"
#include "config.h"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "lottie_assets/look_json.h"
#include "lottie_assets/think_json.h"
#include "lottie_assets/ask_json.h"

static const char* TAG = "IdleFace";

struct IdleFaceAnim {
    const unsigned char* data;
    unsigned int len;
    float fps;
    const char* name;
};

static const IdleFaceAnim kIdleFaceAnims[] = {
    {look_json,  look_json_len,  12.0f, "look"},
    {think_json, think_json_len, 24.0f, "think"},
    {ask_json,   ask_json_len,   24.0f, "ask"},
};

static const size_t kIdleFaceAnimCount = sizeof(kIdleFaceAnims) / sizeof(kIdleFaceAnims[0]);

static lv_obj_t* idle_face_ = nullptr;
static lv_timer_t* switch_timer_ = nullptr;
static int current_anim_ = -1;

static void SwitchToRandomFace(void)
{
    if (idle_face_ == nullptr || kIdleFaceAnimCount == 0) {
        return;
    }

    size_t idx = esp_random() % kIdleFaceAnimCount;
    /* 避免连续两次选中同一个表情 */
    if (idx == static_cast<size_t>(current_anim_)) {
        idx = (idx + 1) % kIdleFaceAnimCount;
    }
    current_anim_ = static_cast<int>(idx);

    const IdleFaceAnim& anim = kIdleFaceAnims[idx];
    if (!sparkbot_lottie_set_src(idle_face_, anim.data, anim.len, anim.fps)) {
        ESP_LOGE(TAG, "Failed to switch to %s", anim.name);
    } else {
        ESP_LOGI(TAG, "face -> %s", anim.name);
    }
}

static void ScheduleNextSwitch(void)
{
    if (switch_timer_ == nullptr) {
        return;
    }
    uint32_t ms = 3000 + (esp_random() % 5001);  /* 3~8 秒 */
    lv_timer_set_period(switch_timer_, ms);
    lv_timer_reset(switch_timer_);
}

static void SwitchTimerCb(lv_timer_t* timer)
{
    (void)timer;
    SwitchToRandomFace();
    ScheduleNextSwitch();
}

static void OnDeviceStateChanged(DeviceState previous, DeviceState current)
{
    (void)previous;
    if (idle_face_ == nullptr) {
        return;
    }

    bool show = (current == kDeviceStateIdle);

    lvgl_port_lock(0);
    sparkbot_lottie_set_visible(idle_face_, show);
    if (show) {
        if (current_anim_ < 0) {
            SwitchToRandomFace();
        }
        lv_timer_resume(switch_timer_);
        ScheduleNextSwitch();
    } else {
        lv_timer_pause(switch_timer_);
    }
    lvgl_port_unlock();

    ESP_LOGI(TAG, "state %d -> idle face %s", static_cast<int>(current), show ? "shown" : "hidden");
}

extern "C" void sparkbot_idle_face_init(void)
{
    if (idle_face_ != nullptr) {
        return;
    }

    lvgl_port_lock(0);
    idle_face_ = sparkbot_lottie_create(lv_screen_active(), DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (idle_face_ != nullptr) {
        /* 初始加载 look；进入 idle 后会按 3~8 秒随机切换 */
        if (!sparkbot_lottie_set_src(idle_face_, look_json, look_json_len, 12.0f)) {
            ESP_LOGE(TAG, "Failed to load look.json");
        }
        current_anim_ = 0;
        switch_timer_ = lv_timer_create(SwitchTimerCb, 5000, nullptr);
        lv_timer_pause(switch_timer_);
    }
    lvgl_port_unlock();

    if (idle_face_ == nullptr) {
        ESP_LOGE(TAG, "Idle face init failed");
        return;
    }

    DeviceStateEventManager::GetInstance().RegisterStateChangeCallback(OnDeviceStateChanged);
    ESP_LOGI(TAG, "Idle face ready (%dx%d), %d animations",
             DISPLAY_WIDTH, DISPLAY_HEIGHT, static_cast<int>(kIdleFaceAnimCount));
}
