#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 当前上报给服务器的 Device-Id MAC
 *
 * 从 NVS 读取 rebind/mac：
 *   - 值为 "chip"          -> 使用芯片真实 MAC
 *   - 值为 xx:xx:...:xx    -> 使用该虚拟 MAC
 *   - 键不存在/值非法      -> 回退芯片真实 MAC（不会自动生成随机 MAC）
 *
 * 返回内部静态缓冲，长期有效。
 */
const char *esp_hi_rebind_get_mac(void);

#ifdef __cplusplus
}
#endif
