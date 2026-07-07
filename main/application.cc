#include "application.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "ml307_ssl_transport.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "font_awesome_symbols.h"
#include "iot/thing_manager.h"
#include "assets/lang_config.h"
#include "mbedtls/sha256.h"
#include "settings.h"
#include "ui.h"

#include <cstring>
#include <algorithm>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>
#include <esp_app_desc.h>
#include "./boards/esp-sparkbot/wifi/web_server.h"
#include <esp_http_client.h>
#include <esp_task_wdt.h>

// ========== 视觉服务配置 ==========
// mDNS 主机名（推荐）：ESP32 通过 mDNS 自动发现笔记本，IP 变化无需改固件
// 笔记本端需启动 vision_server.py（自带 mDNS 广告，需 pip install zeroconf netifaces）
// 备用方案：直接使用 IP 地址（在笔记本终端执行 ipconfig 查看）
#define VISION_SERVER_HOST "vision-server.local"
#define VISION_SERVER_FALLBACK_IP "192.168.1.13"
#define VISION_SERVER_PORT 8080
// 编译时决定使用 mDNS 还是固定 IP
#define USE_MDNS 1  // 1=优先 mDNS, 失败回退固定 IP; 0=只用固定 IP

#define VISION_SERVER_URL "/vision"
#define TTS_SERVER_URL "/tts"
#define IS_VISION_KEYWORD(msg) \
    ((msg).find("\xe7\x9c\x8b\xe7\x9c\x8b") != std::string::npos || \
     (msg).find("\xe7\x9c\x8b\xe5\x91\xa8\xe5\x9b\xb4") != std::string::npos || \
     (msg).find("\xe8\xbf\x99\xe6\x98\xaf\xe4\xbb\x80\xe4\xb9\x88") != std::string::npos)
// ==================================

// 构建服务 URL，优先 mDNS 主机名，失败回退固定 IP
static std::string BuildServerUrl(const std::string& path, bool use_fallback = false) {
    std::string base;
#if USE_MDNS
    if (!use_fallback) {
        base = "http://" + std::string(VISION_SERVER_HOST) + ":" + std::to_string(VISION_SERVER_PORT);
    } else {
        base = "http://" + std::string(VISION_SERVER_FALLBACK_IP) + ":" + std::to_string(VISION_SERVER_PORT);
    }
#else
    base = "http://" + std::string(VISION_SERVER_FALLBACK_IP) + ":" + std::to_string(VISION_SERVER_PORT);
#endif
    return base + path;
}

// URL percent-encoding for non-ASCII characters
static std::string UrlEncode(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 3);
    for (unsigned char c : str) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += '+';
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            result += hex;
        }
    }
    return result;
}

#define TAG "Application"
static const char *const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "fatal_error",
    "invalid_state"};

Application::Application()
{
    event_group_ = xEventGroupCreate();
    background_task_ = new BackgroundTask(4096 * 8);

    esp_timer_create_args_t clock_timer_args = {
        .callback = [](void *arg)
        {
            Application *app = (Application *)arg;
            app->OnClockTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true};
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);
}

Application::~Application()
{
    if (clock_timer_handle_ != nullptr)
    {
        esp_timer_stop(clock_timer_handle_);
        esp_timer_delete(clock_timer_handle_);
    }
    if (background_task_ != nullptr)
    {
        delete background_task_;
    }
    vEventGroupDelete(event_group_);
}

void Application::CheckNewVersion()
{
    auto &board = Board::GetInstance();
    auto display = board.GetDisplay();
    // Check if there is a new firmware version available
    ota_.SetPostData(board.GetJson());

    const int MAX_RETRY = 10;
    int retry_count = 0;

    board.lock_lvgl();

    while (true)
    {
        if (!ota_.CheckVersion())
        {
            retry_count++;
            if (retry_count >= MAX_RETRY)
            {
                ESP_LOGE(TAG, "Too many retries, exit version check");
                return;
            }
            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", 60, retry_count, MAX_RETRY);
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }
        retry_count = 0;

        if (ota_.HasNewVersion())
        {
            Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "happy", Lang::Sounds::P3_UPGRADE);
            // Wait for the chat state to be idle
            do
            {
                vTaskDelay(pdMS_TO_TICKS(3000));
            } while (GetDeviceState() != kDeviceStateIdle);

            // Use main task to do the upgrade, not cancelable
            Schedule([this, display]()
                     {
                SetDeviceState(kDeviceStateUpgrading);
                
                display->SetIcon(FONT_AWESOME_DOWNLOAD);
                std::string message = std::string(Lang::Strings::NEW_VERSION) + ota_.GetFirmwareVersion();
                display->SetChatMessage("system", message.c_str());

                auto& board = Board::GetInstance();
                board.SetPowerSaveMode(false);
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StopDetection();
#endif
                // 预先关闭音频输出，避免升级过程有音频操作
                auto codec = board.GetAudioCodec();
                codec->EnableInput(false);
                codec->EnableOutput(false);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    audio_decode_queue_.clear();
                }
                background_task_->WaitForCompletion();
                delete background_task_;
                background_task_ = nullptr;
                vTaskDelay(pdMS_TO_TICKS(1000));

                ota_.StartUpgrade([display](int progress, size_t speed) {
                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "%d%% %zuKB/s", progress, speed / 1024);
                    display->SetChatMessage("system", buffer);
                });

                // If upgrade success, the device will reboot and never reach here
                display->SetStatus(Lang::Strings::UPGRADE_FAILED);
                ESP_LOGI(TAG, "Firmware upgrade failed...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                Reboot(); });

            return;
        }

        // No new version, mark the current version as valid
        ota_.MarkCurrentVersionValid();
        std::string message = std::string(Lang::Strings::VERSION) + ota_.GetCurrentVersion();
        display->ShowNotification(message.c_str());

        if (ota_.HasActivationCode())
        {
            // Activation code is valid
            SetDeviceState(kDeviceStateActivating);
            ShowActivationCode();

            // Check again in 60 seconds or until the device is idle
            for (int i = 0; i < 60; ++i)
            {
                if (device_state_ == kDeviceStateIdle)
                {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            continue;
        }

        SetDeviceState(kDeviceStateIdle);
        display->SetChatMessage("system", "");
        PlaySound(Lang::Sounds::P3_SUCCESS);
        // Exit the loop if upgrade or idle
        break;
    }
    board.unlock_lvgl();
}

void Application::ShowActivationCode()
{
    auto &message = ota_.GetActivationMessage();
    auto &code = ota_.GetActivationCode();

    struct digit_sound
    {
        char digit;
        const std::string_view &sound;
    };
    static const std::array<digit_sound, 10> digit_sounds{{digit_sound{'0', Lang::Sounds::P3_0},
                                                           digit_sound{'1', Lang::Sounds::P3_1},
                                                           digit_sound{'2', Lang::Sounds::P3_2},
                                                           digit_sound{'3', Lang::Sounds::P3_3},
                                                           digit_sound{'4', Lang::Sounds::P3_4},
                                                           digit_sound{'5', Lang::Sounds::P3_5},
                                                           digit_sound{'6', Lang::Sounds::P3_6},
                                                           digit_sound{'7', Lang::Sounds::P3_7},
                                                           digit_sound{'8', Lang::Sounds::P3_8},
                                                           digit_sound{'9', Lang::Sounds::P3_9}}};
    // auto &board = Board::GetInstance();
    // board.Change_Screen_xiaozhi();
    // This sentence uses 9KB of SRAM, so we need to wait for it to finish
    Alert(Lang::Strings::ACTIVATION, message.c_str(), "happy", Lang::Sounds::P3_ACTIVATION);
    vTaskDelay(pdMS_TO_TICKS(1000));
    background_task_->WaitForCompletion();

    for (const auto &digit : code)
    {
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
                               [digit](const digit_sound &ds)
                               { return ds.digit == digit; });
        if (it != digit_sounds.end())
        {
            PlaySound(it->sound);
        }
    }
}

void Application::Alert(const char *status, const char *message, const char *emotion, const std::string_view &sound)
{
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty())
    {
        PlaySound(sound);
    }
}

void Application::DismissAlert()
{
    if (device_state_ == kDeviceStateIdle)
    {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

void Application::PlaySound(const std::string_view &sound)
{
    auto codec = Board::GetInstance().GetAudioCodec();
    codec->EnableOutput(true);
    SetDecodeSampleRate(16000);
    const char *data = sound.data();
    size_t size = sound.size();
    for (const char *p = data; p < data + size;)
    {
        auto p3 = (BinaryProtocol3 *)p;
        p += sizeof(BinaryProtocol3);

        auto payload_size = ntohs(p3->payload_size);
        std::vector<uint8_t> opus;
        opus.resize(payload_size);
        memcpy(opus.data(), p3->payload, payload_size);
        p += payload_size;

        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.emplace_back(std::move(opus));
    }
}

void Application::ToggleChatState()
{
    if (device_state_ == kDeviceStateActivating)
    {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_)
    {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle)
    {
        Schedule([this]()
                 {
            SetDeviceState(kDeviceStateConnecting);
            if (!protocol_->OpenAudioChannel()) {
                return;
            }

            keep_listening_ = true;
            protocol_->SendStartListening(kListeningModeAutoStop);
            SetDeviceState(kDeviceStateListening); });
    }
    else if (device_state_ == kDeviceStateSpeaking)
    {
        Schedule([this]()
                 { AbortSpeaking(kAbortReasonNone); });
    }
    else if (device_state_ == kDeviceStateListening)
    {
        Schedule([this]()
                 { protocol_->CloseAudioChannel(); });
    }
}

void Application::StartListening()
{
    if (device_state_ == kDeviceStateActivating)
    {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_)
    {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    keep_listening_ = false;
    if (device_state_ == kDeviceStateIdle)
    {
        Schedule([this]()
                 {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }
            protocol_->SendStartListening(kListeningModeManualStop);
            SetDeviceState(kDeviceStateListening); });
    }
    else if (device_state_ == kDeviceStateSpeaking)
    {
        Schedule([this]()
                 {
            AbortSpeaking(kAbortReasonNone);
            protocol_->SendStartListening(kListeningModeManualStop);
            SetDeviceState(kDeviceStateListening); });
    }
}

void Application::StopListening()
{
    Schedule([this]()
             {
        if (device_state_ == kDeviceStateListening) {
            protocol_->SendStopListening();
            SetDeviceState(kDeviceStateIdle);
        } });
}

void Application::Start()
{
    auto &board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);

    /* Setup the display */
    auto display = board.GetDisplay();

    /* Setup the audio codec */
    auto codec = board.GetAudioCodec();
    opus_decode_sample_rate_ = codec->output_sample_rate();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(opus_decode_sample_rate_, 1);
    opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
    // For ML307 boards, we use complexity 5 to save bandwidth
    // For other boards, we use complexity 3 to save CPU
    if (board.GetBoardType() == "ml307")
    {
        ESP_LOGI(TAG, "ML307 board detected, setting opus encoder complexity to 5");
        opus_encoder_->SetComplexity(5);
    }
    else
    {
        ESP_LOGI(TAG, "WiFi board detected, setting opus encoder complexity to 3");
        opus_encoder_->SetComplexity(3);
    }

    if (codec->input_sample_rate() != 16000)
    {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);
    }
    codec->OnInputReady([this, codec]()
                        {
        BaseType_t higher_priority_task_woken = pdFALSE;
        xEventGroupSetBitsFromISR(event_group_, AUDIO_INPUT_READY_EVENT, &higher_priority_task_woken);
        return higher_priority_task_woken == pdTRUE; });
    codec->OnOutputReady([this]()
                         {
        BaseType_t higher_priority_task_woken = pdFALSE;
        xEventGroupSetBitsFromISR(event_group_, AUDIO_OUTPUT_READY_EVENT, &higher_priority_task_woken);
        return higher_priority_task_woken == pdTRUE; });
    codec->Start();

    /* Start the main loop */
    xTaskCreate([](void *arg)
                {
        Application* app = (Application*)arg;
        app->MainLoop();
        vTaskDelete(NULL); }, "main_loop", 4096 * 2, this, 4, nullptr);

    /* Wait for the network to be ready */
    board.StartNetwork();

    // Initialize the protocol
    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    protocol_ = std::make_unique<WebsocketProtocol>();
#else
    protocol_ = std::make_unique<MqttProtocol>();
#endif
    protocol_->OnNetworkError([this](const std::string &message)
                              {
        SetDeviceState(kDeviceStateIdle);
        Alert(Lang::Strings::ERROR, message.c_str(), "sad", Lang::Sounds::P3_EXCLAMATION); });
    protocol_->OnIncomingAudio([this](std::vector<uint8_t> &&data)
                               {
        std::lock_guard<std::mutex> lock(mutex_);
        if (vision_mode_) return;  // 视觉流程中屏蔽云端音频
        if (device_state_ == kDeviceStateSpeaking) {
            audio_decode_queue_.emplace_back(std::move(data));
        } });
    protocol_->OnAudioChannelOpened([this, codec, &board]()
                                    {
        board.SetPowerSaveMode(false);
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        SetDecodeSampleRate(protocol_->server_sample_rate());
        auto& thing_manager = iot::ThingManager::GetInstance();
        protocol_->SendIotDescriptors(thing_manager.GetDescriptorsJson());
        std::string states;
        if (thing_manager.GetStatesJson(states, false)) {
            protocol_->SendIotStates(states);
        } });
    protocol_->OnAudioChannelClosed([this, &board]()
                                    {
        if (vision_mode_) return;  // 视觉流程中完全跳过，不调 Schedule 避免延迟清屏
        board.SetPowerSaveMode(true);
        Schedule([this]() {
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        }); });
    protocol_->OnIncomingJson([this, display](const cJSON *root)
                              {
        // 视觉流程中屏蔽所有云端消息（tts/llm/stt），防止对话割裂
        if (vision_mode_) return;
        // Parse JSON data
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                Schedule([this]() {
                    aborted_ = false;
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                Schedule([this]() {
                    if (device_state_ == kDeviceStateSpeaking) {
                        background_task_->WaitForCompletion();
                        if (keep_listening_) {
                            protocol_->SendStartListening(kListeningModeAutoStop);
                            SetDeviceState(kDeviceStateListening);
                        } else {
                            SetDeviceState(kDeviceStateIdle);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                std::string message(text->valuestring);
                ESP_LOGI(TAG, ">> %s", message.c_str());

                // 视觉指令检测：用户说了"看看"等关键词
                if (IS_VISION_KEYWORD(message)) {
                    ESP_LOGI(TAG, "Vision keyword detected, triggering vision flow");
                    // 立即设标志位，阻止后续云端消息（tts/llm）在 HandleVisionCommand
                    // Schedule 执行前进入队列，消除竞态条件
                    vision_mode_ = true;
                    Schedule([this, display, message]() {
                        display->SetChatMessage("user", message.c_str());
                        HandleVisionCommand(message);
                    });
                } else {
                    Schedule([this, display, message]() {
                        display->SetChatMessage("user", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "iot") == 0) {
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
                auto& thing_manager = iot::ThingManager::GetInstance();
                for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
                    auto command = cJSON_GetArrayItem(commands, i);
                    thing_manager.Invoke(command);
                }
            }
        } });
    protocol_->Start();

    // Check for new firmware version or get the MQTT broker address
    ota_.SetCheckVersionUrl(CONFIG_OTA_VERSION_URL);
    ota_.SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    ota_.SetHeader("Client-Id", board.GetUuid());
    ota_.SetHeader("Accept-Language", Lang::CODE);
    auto app_desc = esp_app_get_description();
    ota_.SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);

    xTaskCreate([](void *arg)
                {
        Application* app = (Application*)arg;
        app->CheckNewVersion();
        vTaskDelete(NULL); }, "check_new_version", 4096 * 2, this, 2, nullptr);

#if CONFIG_USE_AUDIO_PROCESSOR
    audio_processor_.Initialize(codec->input_channels(), codec->input_reference());
    audio_processor_.OnOutput([this](std::vector<int16_t> &&data)
                              { background_task_->Schedule([this, data = std::move(data)]() mutable
                                                           { opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t> &&opus)
                                                                                   { Schedule([this, opus = std::move(opus)]()
                                                                                              { protocol_->SendAudio(opus); }); }); }); });
    audio_processor_.OnVadStateChange([this](bool speaking)
                                      {
        if (device_state_ == kDeviceStateListening) {
            Schedule([this, speaking]() {
                if (speaking) {
                    voice_detected_ = true;
                } else {
                    voice_detected_ = false;
                }
                auto led = Board::GetInstance().GetLed();
                led->OnStateChanged();
            });
        } });
#endif
extern lv_obj_t * ui_face_screen;
#if CONFIG_USE_WAKE_WORD_DETECT
    wake_word_detect_.Initialize(codec->input_channels(), codec->input_reference());
    wake_word_detect_.OnWakeWordDetected([this](const std::string &wake_word)
                                         { Schedule([this, &wake_word]()
                                                    {
            if (device_state_ == kDeviceStateIdle) {
                SetDeviceState(kDeviceStateConnecting);
                wake_word_detect_.EncodeWakeWordData();

                if (!protocol_->OpenAudioChannel()) {
                    wake_word_detect_.StartDetection();
                    return;
                }
                
                std::vector<uint8_t> opus;
                // Encode and send the wake word data to the server
                while (wake_word_detect_.GetWakeWordOpus(opus)) {
                    protocol_->SendAudio(opus);
                }
                // Set the chat state to wake word detected
                std::string wake_word_xiaozhi="你好小智";
                protocol_->SendWakeWordDetected(wake_word_xiaozhi);
                ESP_LOGI(TAG, "Wake word detected: %s", wake_word_xiaozhi.c_str());
                auto &board = Board::GetInstance();
                /* 判断当前 UI */
                lv_obj_t *current_screen = lv_disp_get_scr_act(NULL);
                if (current_screen != ui_face_screen)
                {
                     board.Change_Screen_xiaozhi();
                }
                else
                {
                     ESP_LOGI(TAG, "Currently in face screen, keep current UI");
                }
                //board.Change_Screen_xiaozhi();
                keep_listening_ = true;
                SetDeviceState(kDeviceStateIdle);
            } else if (device_state_ == kDeviceStateSpeaking) {
                AbortSpeaking(kAbortReasonWakeWordDetected);
            } else if (device_state_ == kDeviceStateActivating) {
                SetDeviceState(kDeviceStateIdle);
            } }); });
    wake_word_detect_.StartDetection();
#endif

    SetDeviceState(kDeviceStateIdle);
    esp_timer_start_periodic(clock_timer_handle_, 1000000);
}
void Application::OnClockTimer()
{
    clock_ticks_++;
    if (clock_ticks_ % 10 == 0) // Print the debug info every 10 seconds
    {
        // SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);

        // If we have synchronized server time, set the status to clock "HH:MM" if the device is idle
        if (ota_.HasServerTime())
        {
            if (device_state_ == kDeviceStateIdle)
            {
                Schedule([this]()
                         {
                    // Set status to clock "HH:MM"
                    time_t now = time(NULL);
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%H:%M  ", localtime(&now));
                    Board::GetInstance().GetDisplay()->SetStatus(time_str); });
            }
        }
    }
}

void Application::Schedule(std::function<void()> callback)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);
}

// The Main Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainLoop()
{
    while (true)
    {
        auto bits = xEventGroupWaitBits(event_group_,
                                        SCHEDULE_EVENT | AUDIO_INPUT_READY_EVENT | AUDIO_OUTPUT_READY_EVENT,
                                        pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & AUDIO_INPUT_READY_EVENT)
        {
            InputAudio();
        }
        if (bits & AUDIO_OUTPUT_READY_EVENT)
        {
            OutputAudio();
        }
        if (bits & SCHEDULE_EVENT)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto &task : tasks)
            {
                task();
            }
        }
    }
}

void Application::ResetDecoder()
{
    std::lock_guard<std::mutex> lock(mutex_);
    opus_decoder_->ResetState();
    audio_decode_queue_.clear();
    last_output_time_ = std::chrono::steady_clock::now();
}

void Application::HandleVisionCommand(const std::string& question) {
    ESP_LOGI(TAG, "=== Vision Command Triggered: %s ===", question.c_str());
    ESP_LOGI(TAG, "Free heap before vision: %lu", esp_get_free_heap_size());

    // vision_mode_ 已在 OnIncomingJson 的 IS_VISION_KEYWORD 分支提前置 true，
    // 此处不再重复设置——竞态窗口已消除

    // 注册到任务看门狗：后续 HTTP 请求可能阻塞 15-25 秒，远超 TWDT 5s 超时
    esp_task_wdt_add(NULL);

    // 1. 打断当前对话：停说话 + 停聆听，保持通道不关
    if (device_state_ == kDeviceStateSpeaking) {
        AbortSpeaking(kAbortReasonNone);
    }
    if (device_state_ == kDeviceStateListening && protocol_) {
        protocol_->SendStopListening();
    }
    SetDeviceState(kDeviceStateIdle);
    // 清空可能已到达的云端音频队列
    {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.clear();
    }

    // 2. 调用视觉服务 API（使用 WiFi 直连 HTTP，不走 ML307 4G）
    //    优先使用 mDNS 主机名，失败则回退到固定 IP
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus("识别中…");
    std::string answer;
    esp_err_t err = ESP_FAIL;

    for (int attempt = 0; attempt < 2; attempt++) {
        bool use_fallback = (attempt == 1);
        std::string url = BuildServerUrl(VISION_SERVER_URL, use_fallback) + "?question=" + UrlEncode(question);
        ESP_LOGI(TAG, "Calling vision API (attempt %d): %s", attempt + 1, url.c_str());
        esp_task_wdt_reset();  // 喂狗，防止长时间 HTTP 触发看门狗

        // HTTP event handler: accumulate response body
        struct {
            static esp_err_t OnHttpEvent(esp_http_client_event_t *evt) {
                auto* buf = static_cast<std::string*>(evt->user_data);
                switch (evt->event_id) {
                    case HTTP_EVENT_ON_DATA:
                        buf->append(static_cast<char*>(evt->data), evt->data_len);
                        break;
                    default:
                        break;
                }
                return ESP_OK;
            }
        } http_handler;

        esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 25000;  // 视觉推理 ~15-20s，留足余量
        config.event_handler = http_handler.OnHttpEvent;
        config.user_data = &answer;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "Vision API status=%d, body=%s", status, answer.c_str());

            cJSON* root = cJSON_Parse(answer.c_str());
            if (root) {
                cJSON* ans = cJSON_GetObjectItem(root, "answer");
                if (ans && ans->valuestring) {
                    answer = ans->valuestring;
                }
                cJSON_Delete(root);
            }
            esp_http_client_cleanup(client);
            break;  // 成功，跳出重试循环
        } else {
            ESP_LOGW(TAG, "Vision API attempt %d failed: %s", attempt + 1, esp_err_to_name(err));
            answer.clear();
            esp_http_client_cleanup(client);
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Vision API all attempts failed");
        answer = "抱歉，视觉服务暂时不可用，请检查服务器是否启动";
    }

    if (answer.empty()) {
        ESP_LOGW(TAG, "Vision API returned empty answer");
        answer = "抱歉，我没有识别到什么";
    }

    // 3. 显示结果在屏幕上
    display->SetChatMessage("assistant", answer.c_str());

    // 4. 本地 TTS 播报：POST /tts，文本放请求体（避免 URL 超长）
    //    同样使用 mDNS 优先 + IP 回退策略
    if (!answer.empty()) {
        std::string post_body = "text=" + UrlEncode(answer);
        std::vector<uint8_t> wav_data;
        bool tts_ok = false;

        for (int tts_attempt = 0; tts_attempt < 2 && !tts_ok; tts_attempt++) {
            bool use_fallback = (tts_attempt == 1);
            std::string tts_url = BuildServerUrl(TTS_SERVER_URL, use_fallback);
            wav_data.clear();

            ESP_LOGI(TAG, "Fetching TTS audio via POST (attempt %d): %s", tts_attempt + 1, tts_url.c_str());
            esp_task_wdt_reset();  // 喂狗

            struct {
                static esp_err_t OnHttpEvent(esp_http_client_event_t *evt) {
                    auto* buf = static_cast<std::vector<uint8_t>*>(evt->user_data);
                    switch (evt->event_id) {
                        case HTTP_EVENT_ON_DATA:
                            buf->insert(buf->end(),
                                        static_cast<uint8_t*>(evt->data),
                                        static_cast<uint8_t*>(evt->data) + evt->data_len);
                            break;
                        default:
                            break;
                    }
                    return ESP_OK;
                }
            } tts_http_handler;

            esp_http_client_config_t tts_config = {};
            tts_config.url = tts_url.c_str();
            tts_config.method = HTTP_METHOD_POST;
            tts_config.timeout_ms = 30000;
            tts_config.event_handler = tts_http_handler.OnHttpEvent;
            tts_config.user_data = &wav_data;

            esp_http_client_handle_t tts_client = esp_http_client_init(&tts_config);
            esp_http_client_set_header(tts_client, "Content-Type", "application/x-www-form-urlencoded");
            esp_http_client_set_post_field(tts_client, post_body.c_str(), post_body.size());
            esp_err_t tts_err = esp_http_client_perform(tts_client);

            if (tts_err == ESP_OK && !wav_data.empty()) {
                int tts_status = esp_http_client_get_status_code(tts_client);
                ESP_LOGI(TAG, "TTS API status=%d, audio size=%u", tts_status, wav_data.size());

                // 解析 WAV，找到 "data" chunk 提取 PCM
                const char* data_marker = "data";
                auto it = std::search(wav_data.begin(), wav_data.end(),
                                      data_marker, data_marker + 4);
                if (it != wav_data.end() && (it + 8) <= wav_data.end()) {
                    // 读取 4 字节 little-endian data size
                    size_t off = it - wav_data.begin() + 4;
                    uint32_t data_size = (uint32_t)wav_data[off]
                                       | ((uint32_t)wav_data[off + 1] << 8)
                                       | ((uint32_t)wav_data[off + 2] << 16)
                                       | ((uint32_t)wav_data[off + 3] << 24);
                    const int16_t* pcm_ptr = reinterpret_cast<const int16_t*>(
                        wav_data.data() + off + 4);
                    int pcm_samples = data_size / 2;

                    ESP_LOGI(TAG, "PCM samples=%d, playing locally via I2S", pcm_samples);

                    // 本地 I2S 播放
                    auto codec = Board::GetInstance().GetAudioCodec();
                    if (codec) {
                        SetDeviceState(kDeviceStateSpeaking);
                        codec->EnableOutput(true);

                        // 分块写入 I2S，每块 ~60ms (960 samples @ 16kHz)
                        const int chunk_samples = 960;
                        int offset_samples = 0;
                        while (offset_samples < pcm_samples) {
                            int n = std::min(chunk_samples, pcm_samples - offset_samples);
                            std::vector<int16_t> chunk(pcm_ptr + offset_samples,
                                                       pcm_ptr + offset_samples + n);
                            codec->OutputData(chunk);
                            offset_samples += n;
                        }

                        // 等待 DMA 播放完最后一帧
                        int duration_ms = (pcm_samples * 1000) / 16000 + 100;
                        vTaskDelay(pdMS_TO_TICKS(duration_ms));

                        // 播放完毕 → 直接开通道回到聆听（不用 ToggleChatState，
                        // 它走 Connecting 状态会清屏 SetChatMessage("system", "")）
                        ESP_LOGI(TAG, "Free heap after vision: %lu", esp_get_free_heap_size());
                        vision_mode_ = false;
                        esp_task_wdt_delete(NULL);
                        SetDeviceState(kDeviceStateIdle);
                        display->SetStatus("可以说话了");
                        vTaskDelay(pdMS_TO_TICKS(100));
                        // 直接开通道 + 聆听（在主任务上，不走 Schedule/Connecting）
                        if (protocol_ && protocol_->OpenAudioChannel()) {
                            keep_listening_ = true;
                            protocol_->SendStartListening(kListeningModeAutoStop);
                            SetDeviceState(kDeviceStateListening);
                        }
                        ESP_LOGI(TAG, "Local TTS playback complete, back to chat listening");
                        tts_ok = true;
                    }
                } else {
                    ESP_LOGW(TAG, "WAV parse failed: 'data' chunk not found");
                }
            } else {
                ESP_LOGW(TAG, "TTS attempt %d failed: err=%s, size=%u",
                         tts_attempt + 1, esp_err_to_name(tts_err), wav_data.size());
            }
            esp_http_client_cleanup(tts_client);
        }
    }

    // 安全网：确保 vision_mode_ 已清除并回到聆听状态
    if (vision_mode_) {
        ESP_LOGW(TAG, "Vision mode still active (TTS path incomplete), restoring listening state");
        ESP_LOGI(TAG, "Free heap at safety net: %lu", esp_get_free_heap_size());
        vision_mode_ = false;
        esp_task_wdt_delete(NULL);
        SetDeviceState(kDeviceStateIdle);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (protocol_ && protocol_->OpenAudioChannel()) {
            keep_listening_ = true;
            protocol_->SendStartListening(kListeningModeAutoStop);
            SetDeviceState(kDeviceStateListening);
        }
    }
}

void Application::OutputAudio()
{
    auto now = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    const int max_silence_seconds = 10;

    std::unique_lock<std::mutex> lock(mutex_);
    if (audio_decode_queue_.empty())
    {
        // Disable the output if there is no audio data for a long time
        if (device_state_ == kDeviceStateIdle)
        {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            if (duration > max_silence_seconds)
            {
                codec->EnableOutput(false);
            }
        }
        return;
    }

    if (device_state_ == kDeviceStateListening)
    {
        audio_decode_queue_.clear();
        return;
    }

    last_output_time_ = now;
    auto opus = std::move(audio_decode_queue_.front());
    audio_decode_queue_.pop_front();
    lock.unlock();

    background_task_->Schedule([this, codec, opus = std::move(opus)]() mutable
                               {
        if (aborted_) {
            return;
        }

        std::vector<int16_t> pcm;
        if (!opus_decoder_->Decode(std::move(opus), pcm)) {
            return;
        }

        // Resample if the sample rate is different
        if (opus_decode_sample_rate_ != codec->output_sample_rate()) {
            int target_size = output_resampler_.GetOutputSamples(pcm.size());
            std::vector<int16_t> resampled(target_size);
            output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
            pcm = std::move(resampled);
        }
        
        codec->OutputData(pcm); });
}

void Application::InputAudio()
{
    auto codec = Board::GetInstance().GetAudioCodec();
    std::vector<int16_t> data;
    if (!codec->InputData(data))
    {
        return;
    }

    if (codec->input_sample_rate() != 16000)
    {
        if (codec->input_channels() == 2)
        {
            auto mic_channel = std::vector<int16_t>(data.size() / 2);
            auto reference_channel = std::vector<int16_t>(data.size() / 2);
            for (size_t i = 0, j = 0; i < mic_channel.size(); ++i, j += 2)
            {
                mic_channel[i] = data[j];
                reference_channel[i] = data[j + 1];
            }
            auto resampled_mic = std::vector<int16_t>(input_resampler_.GetOutputSamples(mic_channel.size()));
            auto resampled_reference = std::vector<int16_t>(reference_resampler_.GetOutputSamples(reference_channel.size()));
            input_resampler_.Process(mic_channel.data(), mic_channel.size(), resampled_mic.data());
            reference_resampler_.Process(reference_channel.data(), reference_channel.size(), resampled_reference.data());
            data.resize(resampled_mic.size() + resampled_reference.size());
            for (size_t i = 0, j = 0; i < resampled_mic.size(); ++i, j += 2)
            {
                data[j] = resampled_mic[i];
                data[j + 1] = resampled_reference[i];
            }
        }
        else
        {
            auto resampled = std::vector<int16_t>(input_resampler_.GetOutputSamples(data.size()));
            input_resampler_.Process(data.data(), data.size(), resampled.data());
            data = std::move(resampled);
        }
    }

#if CONFIG_USE_WAKE_WORD_DETECT
    if (wake_word_detect_.IsDetectionRunning())
    {
        wake_word_detect_.Feed(data);
    }
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning())
    {
        audio_processor_.Input(data);
    }
#else
    if (device_state_ == kDeviceStateListening)
    {
        background_task_->Schedule([this, data = std::move(data)]() mutable
                                   { opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t> &&opus)
                                                           { Schedule([this, opus = std::move(opus)]()
                                                                      { protocol_->SendAudio(opus); }); }); });
    }
#endif
}

void Application::AbortSpeaking(AbortReason reason)
{
    ESP_LOGI(TAG, "Abort speaking");
    aborted_ = true;
    protocol_->SendAbortSpeaking(reason);
}

void Application::SetDeviceState(DeviceState state)
{
    if (device_state_ == state)
    {
        return;
    }

    clock_ticks_ = 0;
    auto previous_state = device_state_;
    device_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);
    // The state is changed, wait for all background tasks to finish
    background_task_->WaitForCompletion();

    auto &board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    auto display = board.GetDisplay();
    auto led = board.GetLed();
    led->OnStateChanged();
    switch (state)
    {
    case kDeviceStateUnknown:
    case kDeviceStateIdle:
        display->SetStatus(Lang::Strings::STANDBY);
         ui_face_set_status_text("WAIT...");
        display->SetEmotion("neutral");
#if CONFIG_USE_AUDIO_PROCESSOR
        audio_processor_.Stop();
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
        wake_word_detect_.StartDetection();
#endif
        break;
    case kDeviceStateConnecting:
        display->SetStatus(Lang::Strings::CONNECTING);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
        break;
    case kDeviceStateListening:
        ui_face_set_status_text("LISTENING...");
        display->SetStatus(Lang::Strings::LISTENING);
        display->SetEmotion("neutral");
        ResetDecoder();
        opus_encoder_->ResetState();
#if CONFIG_USE_AUDIO_PROCESSOR
        audio_processor_.Start();
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
        wake_word_detect_.StopDetection();
#endif
        UpdateIotStates();
        if (previous_state == kDeviceStateSpeaking)
        {
            // FIXME: Wait for the speaker to empty the buffer
            vTaskDelay(pdMS_TO_TICKS(120));
        }
        break;
    case kDeviceStateSpeaking:
        ui_face_set_status_text("SPEAKING...");
        display->SetStatus(Lang::Strings::SPEAKING);
        ResetDecoder();
        codec->EnableOutput(true);
#if CONFIG_USE_AUDIO_PROCESSOR
        audio_processor_.Stop();
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
        wake_word_detect_.StartDetection();
#endif
        break;
    default:
        // Do nothing
        break;
    }
}

void Application::SetDecodeSampleRate(int sample_rate)
{
    if (opus_decode_sample_rate_ == sample_rate)
    {
        return;
    }

    opus_decode_sample_rate_ = sample_rate;
    opus_decoder_.reset();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(opus_decode_sample_rate_, 1);

    auto codec = Board::GetInstance().GetAudioCodec();
    if (opus_decode_sample_rate_ != codec->output_sample_rate())
    {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decode_sample_rate_, codec->output_sample_rate());
        output_resampler_.Configure(opus_decode_sample_rate_, codec->output_sample_rate());
    }
}

void Application::UpdateIotStates()
{
    auto &thing_manager = iot::ThingManager::GetInstance();
    std::string states;
    if (thing_manager.GetStatesJson(states, true))
    {
        protocol_->SendIotStates(states);
    }
}

void Application::Reboot()
{
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();
}

void Application::WakeWordInvoke(const std::string &wake_word)
{
    if (device_state_ == kDeviceStateIdle)
    {
        ToggleChatState();
        Schedule([this, wake_word]()
                 {
            if (protocol_) {
                protocol_->SendWakeWordDetected(wake_word); 
            } });
    }
    else if (device_state_ == kDeviceStateSpeaking)
    {
        Schedule([this]()
                 { AbortSpeaking(kAbortReasonNone); });
    }
    else if (device_state_ == kDeviceStateListening)
    {
        Schedule([this]()
                 {
            if (protocol_) {
                protocol_->CloseAudioChannel();
            } });
    }
}

bool Application::CanEnterSleepMode()
{
    if (device_state_ != kDeviceStateIdle)
    {
        return false;
    }

    if (protocol_ && protocol_->IsAudioChannelOpened())
    {
        return false;
    }

    // Now it is safe to enter sleep mode
    return true;
}

extern "C" xz_ui_state_t app_get_xz_ui_state(void)
{
    DeviceState st = Application::GetInstance().GetDeviceState();

    if (st == kDeviceStateSpeaking)  return XZ_UI_STATE_SPEAKING;
    if (st == kDeviceStateListening) return XZ_UI_STATE_LISTENING;

    // Idle / Connecting / Upgrading / Activating / Starting... 都按 WAIT 处理
    return XZ_UI_STATE_WAIT;
}
extern "C" void app_force_to_wait(void)
{
    // 用主循环线程执行，避免多线程直接碰 protocol_/state
    Application::GetInstance().Schedule([]() {
        auto &app = Application::GetInstance();

        // 这句确保不会自动继续 listening
        // （如果 keep_listening_ 是私有成员，这里访问不了；那就不写它）
        // app.keep_listening_ = false;

        // Speaking -> Abort
        if (app.GetDeviceState() == kDeviceStateSpeaking) {
            app.AbortSpeaking(kAbortReasonNone);
        }

        // Listening -> StopListening
        if (app.GetDeviceState() == kDeviceStateListening) {
            app.StopListening();
        }

        // 无论如何都回到 Idle（WAIT）
        app.SetDeviceState(kDeviceStateIdle);
    });
}
