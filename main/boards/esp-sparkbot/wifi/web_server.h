#pragma once
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t start_webserver(void);
void stop_webserver();
void set_verify();
uint8_t cdk_verify();
#ifdef __cplusplus
}
#endif