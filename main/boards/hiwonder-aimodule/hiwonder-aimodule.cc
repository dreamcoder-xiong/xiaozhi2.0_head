#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "settings.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <driver/uart.h>
#include <cstring>

#include "esp32_camera.h"
#include "esp_heap_caps.h"
#include "esp_lcd_touch_ft5x06.h"

#define TAG "hiwonder_aimodule"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class HiwonderEs8311AudioCodec : public Es8311AudioCodec {
public:
    HiwonderEs8311AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
                        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
                        gpio_num_t pa_pin, uint8_t es8311_addr, bool use_mclk = true)
        : Es8311AudioCodec(i2c_master_handle, i2c_port, input_sample_rate, output_sample_rate,
                             mclk,  bclk,  ws,  dout,  din,pa_pin,  es8311_addr,  use_mclk = true) {}

    void EnableOutput(bool enable) override {
        if (enable == output_enabled_) return;
        if (enable) {
            Es8311AudioCodec::EnableOutput(enable);
        }
        // 不禁用输出以避免显示IO冲突
    }
};

class HiWonderAIModule : public WifiBoard {
private:
    Button left_button;
    Button right_button;
    i2c_master_bus_handle_t i2c_bus_;
    Display* display_;
    Esp32Camera* camera_;
    
    // 唤醒相关
    static constexpr uint8_t WAKE_FLAG[] = {0xAA, 0x55, 0x03, 0x00, 0xFB};
    static constexpr size_t WAKE_FLAG_LEN = sizeof(WAKE_FLAG);
    uint8_t uart0_rx_buffer[UART0_BUF_SIZE];
    TaskHandle_t uart_task_handle_ = nullptr;
    TaskHandle_t touch_task_handle_ = nullptr;

    esp_lcd_touch_handle_t touch_handle_ = nullptr;
    
    void InitializeButtons() {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << 43);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        left_button.OnClick([this]() {

        });
        left_button.OnLongPress([this]() {
            esp_restart();
        });
        right_button.OnClick([this]() {
            auto& app = Application::GetInstance();
            app.ToggleChatState();
        });
        right_button.OnLongPress([this]() {
            ResetWifiConfiguration();
        });
    }

    void InitializeI2CMaster() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeUart0() {
        uart_config_t uart_config = {
            .baud_rate = UART0_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };

        ESP_ERROR_CHECK(uart_driver_install(UART0_PORT_NUM, UART0_BUF_SIZE * 2, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(UART0_PORT_NUM, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(UART0_PORT_NUM, -1, UART0_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

        xTaskCreate([](void* module_ptr) {
            static_cast<HiWonderAIModule*>(module_ptr)->Uart0ReceiveTask();
        }, "uart0_rx", 2048, this, 5, &uart_task_handle_);

        ESP_LOGI(TAG, "UART0 initialized on RXD:%d", UART0_RXD);
    }

    void Uart0ReceiveTask() {
        while (true) {
            int len = uart_read_bytes(UART0_PORT_NUM, uart0_rx_buffer, sizeof(uart0_rx_buffer), 20 / portTICK_PERIOD_MS);
            if (len > 0) {
                for (int i = 0; i + WAKE_FLAG_LEN <= len; i++) {
                    if (memcmp(&uart0_rx_buffer[i], WAKE_FLAG, WAKE_FLAG_LEN) == 0) {
                        ESP_LOGI(TAG, "Wake Up");
                        break;
                    }
                }
            }
        }
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_GPIO;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_GPIO;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_GPIO;
        io_config.dc_gpio_num = DISPLAY_DC_GPIO;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_disp_on_off(panel, true);
        
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = font_emoji_64_init(),
                                    });
        ESP_LOGI(TAG, "Initialize LCD driver");
    }

    void InitializeTouch() {
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_NC,
            .levels = { .reset = 0, .interrupt = 0 },
            .flags = { .swap_xy = 1, .mirror_x = 1, .mirror_y = 0 },
        };
        
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        tp_io_config.scl_speed_hz = 400 * 1000;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &touch_handle_));
        ESP_LOGI(TAG, "Initialize Touch driver");
        CreateTouchTask();
    }

    void CreateTouchTask() {
        xTaskCreate([](void* param) {
            auto* self = static_cast<HiWonderAIModule*>(param);
            uint16_t touch_x[1], touch_y[1], touch_strength[1];
            uint8_t touch_cnt = 0;

            while (true) {
                if (self->touch_handle_) {
                    esp_lcd_touch_read_data(self->touch_handle_);
                    bool pressed = esp_lcd_touch_get_coordinates(
                        self->touch_handle_, touch_x, touch_y, touch_strength, &touch_cnt, 1);
                    
                    if (pressed && touch_cnt > 0) {
                        ESP_LOGI(TAG, "Touch: X=%d, Y=%d", touch_x[0], touch_y[0]);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }, "touch_task", 2048, this, 5, &touch_task_handle_);
    }

    void InitializeCamera() {
        camera_config_t camera_config = {};
        camera_config.pin_pwdn = HIWONDER_CAMERA_PWDN;
        camera_config.pin_reset = HIWONDER_CAMERA_RESET;
        camera_config.pin_xclk = HIWONDER_CAMERA_XCLK;
        camera_config.pin_pclk = HIWONDER_CAMERA_PCLK;
        camera_config.pin_sccb_sda = HIWONDER_CAMERA_SIOD;
        camera_config.pin_sccb_scl = HIWONDER_CAMERA_SIOC;

        camera_config.pin_d0 = HIWONDER_CAMERA_D0;
        camera_config.pin_d1 = HIWONDER_CAMERA_D1;
        camera_config.pin_d2 = HIWONDER_CAMERA_D2;
        camera_config.pin_d3 = HIWONDER_CAMERA_D3;
        camera_config.pin_d4 = HIWONDER_CAMERA_D4;
        camera_config.pin_d5 = HIWONDER_CAMERA_D5;
        camera_config.pin_d6 = HIWONDER_CAMERA_D6;
        camera_config.pin_d7 = HIWONDER_CAMERA_D7;

        camera_config.pin_vsync = HIWONDER_CAMERA_VSYNC;
        camera_config.pin_href = HIWONDER_CAMERA_HSYNC;
        camera_config.xclk_freq_hz = HIWONDER_CAMERA_XCLK_FREQ;
        camera_config.ledc_timer = HIWONDER_LEDC_TIMER;
        camera_config.ledc_channel = HIWONDER_LEDC_CHANNEL;
        camera_config.fb_location = CAMERA_FB_IN_PSRAM;
        camera_config.sccb_i2c_port = I2C_NUM_0;
        camera_config.pixel_format = PIXFORMAT_RGB565;
        camera_config.frame_size = FRAMESIZE_QVGA;
        camera_config.jpeg_quality = 12;
        camera_config.fb_count = 1;
        camera_config.grab_mode = CAMERA_GRAB_LATEST;
        
        camera_ = new Esp32Camera(camera_config);
        Settings settings("hiwonder_ai", false);
        bool camera_flipped = static_cast<bool>(settings.GetInt("camera-flipped", 1));
        camera_->SetHMirror(camera_flipped);
    }

public:
    HiWonderAIModule() : left_button(LEFT_BUTTON_GPIO), right_button(RIGHT_BUTTON_GPIO) {
        InitializeI2CMaster();
        InitializeSpi();
        InitializeDisplay();
        InitializeUart0();
        InitializeTouch();
        InitializeButtons();
        InitializeCamera();
        GetBacklight()->RestoreBrightness();
    }

    ~HiWonderAIModule() {
        if (uart_task_handle_) {
            vTaskDelete(uart_task_handle_);
        }
        if (touch_task_handle_) {
            vTaskDelete(touch_task_handle_);
        }
    }

    virtual AudioCodec* GetAudioCodec() override {
         static HiwonderEs8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};
DECLARE_BOARD(HiWonderAIModule);