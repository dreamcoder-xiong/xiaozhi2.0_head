#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    XZ_UI_STATE_WAIT = 0,
    XZ_UI_STATE_LISTENING = 1,
    XZ_UI_STATE_SPEAKING = 2,
} xz_ui_state_t;

xz_ui_state_t app_get_xz_ui_state(void);

#ifdef __cplusplus
}
#endif
void app_force_to_wait(void);
