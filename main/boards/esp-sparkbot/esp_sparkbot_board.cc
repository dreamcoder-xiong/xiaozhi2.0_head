#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "board.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include "touch_element/touch_button.h"
#include "./ui/ui.h"
#include "app_animation.h"
#include "app_imu.h"
#include "app_power.h"
#include "app_weather.h"
#include "assets/lang_config.h"
#include "./wifi/wifi_config.h"
#include "./wifi/web_server.h"


#define TAG "esp_sparkbot"
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);
class TouchButton
{
private:
    /* Touch buttons handle */
    touch_button_handle_t button_handle[TOUCH_BUTTON_NUM];
    static volatile bool button_busy;   //  新增
    /* Touch buttons channel array */
    const touch_pad_t channel_array[TOUCH_BUTTON_NUM] = {
        TOUCH_PAD_NUM1,
        TOUCH_PAD_NUM2,
        TOUCH_PAD_NUM3,
        TOUCH_PAD_NUM4,
    };

    /* Touch buttons channel sensitivity array */
    const float channel_sens_array[TOUCH_BUTTON_NUM] = {
        0.025F,
        0.085F,
        0.085F,
        0.025F,
    };
    typedef enum
    {
        TOUCH_BUTTON_UP = 1,
        TOUCH_BUTTON_LEFT,
        TOUCH_BUTTON_RIGHT,
        TOUCH_BUTTON_DOWN
    } bsp_adc_button_t;
    static void button_handler(touch_button_handle_t out_handle, touch_button_message_t *out_message, void *arg)
    {
        lv_obj_t *current_screen = lv_disp_get_scr_act(NULL);

        (void)out_handle; // Unused
        int button_num = (int)arg;

        if (out_message->event == TOUCH_BUTTON_EVT_ON_PRESS)
        {
            if (button_busy)
            {
                 ESP_LOGW(TAG, "Button[%d] ignored (busy)", button_num);
                return;
            }
            // ⭐ 标记为忙
             button_busy = true;
            ESP_LOGI(TAG, "Button[%d] Press", button_num);
            switch (button_num)
            {
            case TOUCH_BUTTON_UP:
                ui_send_sys_event(current_screen, (lv_event_code_t)LV_EVENT_UP_CLICK, NULL);
                break;
            case TOUCH_BUTTON_DOWN:
                ui_send_sys_event(current_screen, (lv_event_code_t)LV_EVENT_DOWN_CLICK, NULL);
                break;
            case TOUCH_BUTTON_LEFT:
                ui_send_sys_event(current_screen, (lv_event_code_t)LV_EVENT_LEFT_CLICK, NULL);
                break;
            case TOUCH_BUTTON_RIGHT:
                ui_send_sys_event(current_screen, (lv_event_code_t)LV_EVENT_RIGHT_CLICK, NULL);
                break;
            default:
                break;
            }
        }
        else if (out_message->event == TOUCH_BUTTON_EVT_ON_RELEASE)
        {
             button_busy = false;
             ESP_LOGI(TAG, "Button[%d] Release", button_num);
        }
        else if (out_message->event == TOUCH_BUTTON_EVT_ON_LONGPRESS)
        {
            ESP_LOGI(TAG, "Button[%d] LongPress", (int)arg);
            switch (button_num)
            {
            case TOUCH_BUTTON_UP:
                ui_send_sys_event(current_screen, (lv_event_code_t)LV_EVENT_UP_LONG_CLICK, NULL);
                break;
            case TOUCH_BUTTON_DOWN:
                ui_send_sys_event(current_screen, (lv_event_code_t)LV_EVENT_DOWN_LONG_CLICK, NULL);
                break;
            case TOUCH_BUTTON_LEFT:
                ui_send_sys_event(current_screen, (lv_event_code_t)LV_EVENT_LEFT_LONG_CLICK, NULL);
                break;
            case TOUCH_BUTTON_RIGHT:
                ui_send_sys_event(current_screen, (lv_event_code_t)LV_EVENT_RIGHT_LONG_CLICK, NULL);
                break;
            default:
                break;
            }
        }
    }

public:
    void bsp_touch_button_create()
    {
        /* Initialize Touch Element library */
        touch_elem_global_config_t global_config = TOUCH_ELEM_GLOBAL_DEFAULT_CONFIG();
        ESP_ERROR_CHECK(touch_element_install(&global_config));
        ESP_LOGI(TAG, "Touch element library installed");

        touch_button_global_config_t button_global_config = TOUCH_BUTTON_GLOBAL_DEFAULT_CONFIG();
        ESP_ERROR_CHECK(touch_button_install(&button_global_config));
        ESP_LOGI(TAG, "Touch button installed");
        for (int i = 0; i < TOUCH_BUTTON_NUM; i++)
        {
            touch_button_config_t button_config = {
                .channel_num = channel_array[i],
                .channel_sens = channel_sens_array[i]};
            /* Create Touch buttons */
            ESP_ERROR_CHECK(touch_button_create(&button_config, &button_handle[i]));
            /* Subscribe touch button events (On Press, On Release, On LongPress) */
            ESP_ERROR_CHECK(touch_button_subscribe_event(button_handle[i],
                                                         TOUCH_ELEM_EVENT_ON_PRESS | TOUCH_ELEM_EVENT_ON_RELEASE | TOUCH_ELEM_EVENT_ON_LONGPRESS,
                                                         (void *)channel_array[i]));
#ifdef CONFIG_TOUCH_ELEM_EVENT
            /* Set EVENT as the dispatch method */
            ESP_ERROR_CHECK(touch_button_set_dispatch_method(button_handle[i], TOUCH_ELEM_DISP_EVENT));
#elif CONFIG_TOUCH_ELEM_CALLBACK
            /* Set EVENT as the dispatch method */
            ESP_ERROR_CHECK(touch_button_set_dispatch_method(button_handle[i], TOUCH_ELEM_DISP_CALLBACK));
            /* Register a handler function to handle event messages */
            ESP_ERROR_CHECK(touch_button_set_callback(button_handle[i], button_handler));
#endif
            /* Set LongPress event trigger threshold time */
            ESP_ERROR_CHECK(touch_button_set_longpress(button_handle[i], 2000));
        }
        ESP_LOGI(TAG, "Touch buttons created");

#ifdef CONFIG_TOUCH_ELEM_EVENT
        /* Create a handler task to handle event messages */
        xTaskCreate(&button_handler_task, "button_handler_task", 4 * 1024, NULL, 5, NULL);
#endif

        touch_element_start();
        ESP_LOGI(TAG, "Touch element library start");
    }
};
volatile bool TouchButton::button_busy = false;
class SparkBotEs8311AudioCodec : public Es8311AudioCodec
{
private:
public:
    SparkBotEs8311AudioCodec(void *i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
                             gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
                             gpio_num_t pa_pin, uint8_t es8311_addr, bool use_mclk = true)
        : Es8311AudioCodec(i2c_master_handle, i2c_port, input_sample_rate, output_sample_rate,
                           mclk, bclk, ws, dout, din, pa_pin, es8311_addr, use_mclk = true) {}

    void EnableOutput(bool enable) override
    {
        if (enable == output_enabled_)
        {
            return;
        }
        if (enable)
        {
            Es8311AudioCodec::EnableOutput(enable);
        }
        else
        {
            // Nothing todo because the display io and PA io conflict
        }
    }
};
i2c_master_bus_handle_t get_handle_i2c_master_bus_handle_t()
{
    return i2c_bus_get_master_bus_handle();
}
class EspSparkBot : public WifiBoard
{
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Display *display_;
    TouchButton touch_button_;

    void InitializeI2c()
    {
        app_imu_init();
        power_adc_init();

        // Initialize I2C peripheral
        // i2c_master_bus_config_t i2c_bus_cfg = {
        //     .i2c_port = I2C_NUM_0,
        //     .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        //     .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        //     .clk_source = I2C_CLK_SRC_DEFAULT,
        //     .glitch_ignore_cnt = 7,
        //     .intr_priority = 0,
        //     .trans_queue_depth = 0,
        //     .flags = {
        //         .enable_internal_pullup = 1,
        //     },
        // };
        // ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        i2c_bus_ = get_handle_i2c_master_bus_handle_t();
    }

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_GPIO;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_GPIO;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons()
    {
        touch_button_.bsp_touch_button_create();
    }

    void InitializeDisplay()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_GPIO;
        io_config.dc_gpio_num = DISPLAY_DC_GPIO;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 16;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");

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
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                     {
                                         .text_font = &font_puhui_20_4,
                                         .icon_font = &font_awesome_20_4,
                                         .emoji_font = nullptr,
                                     });

        app_animation_init();

        ui_Initialize();

        
        _ui_screen_change(&ui_weather_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_weather_screen_init);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
        thing_manager.AddThing(iot::CreateThing("Chassis"));
    }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override
    {
        level = get_power_value();
        charging = false;
        discharging = true;
        return true;
    }

    static void ui_send_event_update(void *param)
    {
        auto &app = Application::GetInstance();
        auto codec = Board::GetInstance().GetAudioCodec();
        while (1)
        {
            uint8_t event_code;
            if (pdPASS == xQueueReceive(ui_send_event, &event_code, portMAX_DELAY))
            {
                // ESP_LOGI(TAG, "event_code:%d", event_code);
                switch (event_code)
                {
                case UI_EVENT_AUDIO_PLAY_MUYU:
                {
                    // app.PlaySound(Lang::Sounds::P3_SUCCESS);
                    void *audio = (void *)mmap_assets_get_mem(asset_audio, MMAP_AUDIO_MUYU_WAV);
                    uint32_t len = mmap_assets_get_size(asset_audio, MMAP_AUDIO_MUYU_WAV);
                    codec->Write_Break(audio, len);
                    break;
                }
                case UI_EVENT_AUDIO_UP:
                {
                    auto volume = codec->output_volume() + 10;
                    if (volume > 100)
                    {
                        volume = 100;
                    }
                    if (volume < 0)
                    {
                        volume = 0;
                    }
                    codec->SetOutputVolume(volume);
                    break;
                    // app.PlaySound(Lang::Sounds::P3_WIFICONFIG);

                    // GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
                }
                case UI_EVENT_AUDIO_DOWN:
                {
                    auto volume = codec->output_volume() - 10;
                    if (volume > 100)
                    {
                        volume = 100;
                    }
                    if (volume < 0)
                    {
                        volume = 0;
                    }
                    codec->SetOutputVolume(volume);
                    break;
                    // app.PlaySound(Lang::Sounds::P3_WIFICONFIG);

                    // GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
                }
                case UI_EVENT_XIAOZHI_LEAVE:
                {
                    // if (app.GetDeviceState() == kDeviceStateSpeaking)
                    // {
                    //     app.ToggleChatState();
                    //     vTaskdelay(100 / portTICK_PERIOD_MS);
                    //     app.ToggleChatState();
                    // }
                    // if(app.GetDeviceState() == kDeviceStateSpeaking)
                    // app.SetDeviceState(kDeviceStateIdle);
                    break;
                }

                case UI_EVENT_XIAOZHI_DOWN:
                {
                    app.ToggleChatState();
                    break;
                }
                case UI_EVENT_WIFI_AP_MODE:
                {
                    static bool wifi_ap_mode = false;
                    if (wifi_ap_mode == false)
                    {
                        // auto board = static_cast<WifiBoard *>(&Board::GetInstance());
                        // if (board && board->GetBoardType() == "wifi")
                        // {
                        //     board->ResetWifiConfiguration();
                        // }
                        wifi_init(2);
                        auto &app = Application::GetInstance();
                        app.PlaySound(Lang::Sounds::P3_WIFICONFIG);
                    }
                    wifi_ap_mode = true;
                    break;
                }
                default:
                    break;
                }
            }
        }
    }
    void ui_send_event_init()
    {

        ui_send_event = xQueueCreate(4, sizeof(uint8_t));
        xTaskCreate(ui_send_event_update, "ui_send_event_update", 4096, NULL, 2, NULL);
    }

public:
    EspSparkBot() : boot_button_(BOOT_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
        ui_send_event_init();
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static SparkBotEs8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                                    AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                                    AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }

    virtual Backlight *GetBacklight() override
    {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
    virtual void Change_Screen_xiaozhi() override
    {
        _ui_screen_change(&ui_xiaozhi, LV_SCR_LOAD_ANIM_NONE, 0, 0, NULL);
    }
    virtual void Change_Screen_weather() override
    {
        _ui_screen_change(&ui_weather_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_weather_screen_init);
    }
    virtual void lock_lvgl() override
    {
        lvgl_port_lock(0);
    }
    virtual void unlock_lvgl() override
    {
        lvgl_port_unlock();
    }
    virtual void StartNetwork()
    {
        wifi_init(1);
        while (wifi_sta_connect_flag == 0)
        {
            vTaskDelay(300 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "wifi_sta_connect_flag:%d", wifi_sta_connect_flag);
        app_weather_get();

        // 播报配置 WiFi 的提示
        // auto &app = Application::GetInstance();
        // app.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);
    }
};

DECLARE_BOARD(EspSparkBot);
