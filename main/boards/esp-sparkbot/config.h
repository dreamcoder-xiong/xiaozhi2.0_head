#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/uart.h>

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

#define AUDIO_INPUT_REFERENCE false

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_45
#define AUDIO_I2S_GPIO_WS GPIO_NUM_41
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_39
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_40
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_42

#define AUDIO_CODEC_PA_PIN GPIO_NUM_NC // GPIO_NUM_46   原版这个引脚和屏幕的背光控制引脚接到一起了，感觉是bug；
                                       //   45coll版本之间接高电平，虽然会导致喇叭结束的时候会有吧嗒吧嗒声音，但是总体体验区别不大
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_0 // GPIO_NUM_4   iic的sda引脚修改为GPIO0。为了引出gpio4作为触摸按键使用
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_5
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR

#define BUILTIN_LED_GPIO GPIO_NUM_NC
#define BOOT_BUTTON_GPIO GPIO_NUM_NC // GPIO_NUM_0  删除boot物理按键功能
#define VOLUME_UP_BUTTON_GPIO GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_DC_GPIO GPIO_NUM_43
#define DISPLAY_CS_GPIO GPIO_NUM_46 //  GPIO_NUM_44  把背光控制引脚和屏幕的cs引脚交换，本来可以不换，但是为了省一个三极管就换了
#define DISPLAY_CLK_GPIO GPIO_NUM_21
#define DISPLAY_MOSI_GPIO GPIO_NUM_47
#define DISPLAY_RST_GPIO GPIO_NUM_NC

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_44    // GPIO_NUM_46
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true // true  屏幕背光控制引脚输出低电平有效

#define UART_ECHO_TXD GPIO_NUM_38
#define UART_ECHO_RXD GPIO_NUM_48
#define UART_ECHO_RTS (-1)
#define UART_ECHO_CTS (-1)

#define MOTOR_SPEED_MAX 100
#define MOTOR_SPEED_80 80
#define MOTOR_SPEED_60 60
#define MOTOR_SPEED_MIN 0

#define ECHO_UART_PORT_NUM UART_NUM_1
#define ECHO_UART_BAUD_RATE (115200)
#define BUF_SIZE (1024)

#define CONFIG_TOUCH_ELEM_CALLBACK 1
#define TOUCH_BUTTON_NUM 4
/* Camera */
#define BSP_CAMERA_XCLK (GPIO_NUM_15)
#define BSP_CAMERA_PCLK (GPIO_NUM_13)
#define BSP_CAMERA_VSYNC (GPIO_NUM_6)
#define BSP_CAMERA_HSYNC (GPIO_NUM_7)
#define BSP_CAMERA_D0 (GPIO_NUM_11)
#define BSP_CAMERA_D1 (GPIO_NUM_9)
#define BSP_CAMERA_D2 (GPIO_NUM_8)
#define BSP_CAMERA_D3 (GPIO_NUM_10)
#define BSP_CAMERA_D4 (GPIO_NUM_12)
#define BSP_CAMERA_D5 (GPIO_NUM_18)
#define BSP_CAMERA_D6 (GPIO_NUM_17)
#define BSP_CAMERA_D7 (GPIO_NUM_16)
#define BSP_I2C_NUM 0

#define BSP_CAMERA_DEFAULT_CONFIG          \
    {                                      \
        .pin_pwdn = GPIO_NUM_NC,           \
        .pin_reset = GPIO_NUM_NC,          \
        .pin_xclk = BSP_CAMERA_XCLK,       \
        .pin_sccb_sda = GPIO_NUM_NC,       \
        .pin_sccb_scl = GPIO_NUM_NC,       \
        .pin_d7 = BSP_CAMERA_D7,           \
        .pin_d6 = BSP_CAMERA_D6,           \
        .pin_d5 = BSP_CAMERA_D5,           \
        .pin_d4 = BSP_CAMERA_D4,           \
        .pin_d3 = BSP_CAMERA_D3,           \
        .pin_d2 = BSP_CAMERA_D2,           \
        .pin_d1 = BSP_CAMERA_D1,           \
        .pin_d0 = BSP_CAMERA_D0,           \
        .pin_vsync = BSP_CAMERA_VSYNC,     \
        .pin_href = BSP_CAMERA_HSYNC,      \
        .pin_pclk = BSP_CAMERA_PCLK,       \
        .xclk_freq_hz = 16000000,          \
        .ledc_timer = LEDC_TIMER_0,        \
        .ledc_channel = LEDC_CHANNEL_0,    \
        .pixel_format = PIXFORMAT_RGB565,  \
        .frame_size = FRAMESIZE_240X240,   \
        .jpeg_quality = 12,                \
        .fb_count = 2,                     \
        .fb_location = CAMERA_FB_IN_PSRAM, \
        .sccb_i2c_port = BSP_I2C_NUM,      \
    }

#define BSP_CAMERA_HMIRROR 0
#define BSP_CAMERA_VFLIP 1
typedef enum
{
    LIGHT_MODE_CHARGING_BREATH = 0,
    LIGHT_MODE_POWER_LOW,
    LIGHT_MODE_ALWAYS_ON,
    LIGHT_MODE_BLINK,
    LIGHT_MODE_WHITE_BREATH_SLOW,
    LIGHT_MODE_WHITE_BREATH_FAST,
    LIGHT_MODE_FLOWING,
    LIGHT_MODE_SHOW,
    LIGHT_MODE_SLEEP,
    LIGHT_MODE_MAX
} light_mode_t;

#endif // _BOARD_CONFIG_H_
