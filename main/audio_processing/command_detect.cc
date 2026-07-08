#include "command_detect.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_mn_speech_commands.h>
#include <model_path.h>
#include <cstring>
#include <cstdio>

#define DETECTION_RUNNING_EVENT 1

static const char* TAG = "CommandDetect";

//=============================================================================
// 构造 / 析构
//=============================================================================

CommandDetect::CommandDetect()
{
    event_group_ = xEventGroupCreate();
}

CommandDetect::~CommandDetect()
{
    StopDetection();

    if (model_data_ != nullptr && multinet_ != nullptr) {
        multinet_->destroy(model_data_);
        model_data_ = nullptr;
    }

    if (event_group_ != nullptr) {
        vEventGroupDelete(event_group_);
        event_group_ = nullptr;
    }
}

//=============================================================================
// 初始化
//=============================================================================

bool CommandDetect::Initialize()
{
    // 获取已挂载的模型列表（由 WakeWordDetect 的 esp_srmodel_init("model") 挂载）
    srmodel_list_t* models = get_static_srmodels();
    if (models == nullptr) {
        models = esp_srmodel_init("model");
        if (models == nullptr) {
            ESP_LOGE(TAG, "Failed to initialize model partition");
            return false;
        }
    }

    // 打印模型列表（调试用）
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models->model_name[i]);
    }

    // 查找中文 MultiNet 模型
    char* model_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    if (model_name == nullptr) {
        ESP_LOGE(TAG, "No Chinese MultiNet model found in partition");
        return false;
    }
    ESP_LOGI(TAG, "Found MultiNet model: %s", model_name);

    // 获取 MultiNet 接口
    multinet_ = esp_mn_handle_from_name(model_name);
    if (multinet_ == nullptr) {
        ESP_LOGE(TAG, "Failed to get MultiNet interface");
        return false;
    }

    // 验证核心函数指针不为空
    if (multinet_->create == nullptr || multinet_->destroy == nullptr ||
        multinet_->detect == nullptr || multinet_->get_results == nullptr ||
        multinet_->get_samp_chunksize == nullptr || multinet_->get_samp_rate == nullptr ||
        multinet_->clean == nullptr || multinet_->set_speech_commands == nullptr) {
        ESP_LOGE(TAG, "MultiNet interface missing required function pointers");
        return false;
    }

    // 创建模型实例（3000ms 超时）
    model_data_ = multinet_->create(model_name, 3000);
    if (model_data_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create MultiNet model instance");
        return false;
    }

    // 尝试切换加载模式（如果接口支持）
    if (multinet_->switch_loader_mode != nullptr) {
        multinet_->switch_loader_mode(model_data_, ESP_MN_LOAD_FROM_PSRAM_FLASH);
    }

    // 获取音频参数
    chunksize_ = multinet_->get_samp_chunksize(model_data_);
    int sample_rate = multinet_->get_samp_rate(model_data_);
    ESP_LOGI(TAG, "MultiNet sample_rate=%d, chunksize=%d", sample_rate, chunksize_);

    // 设置检测阈值，降低误识别率（0.0~0.9999，越高越严格）
    if (multinet_->set_det_threshold != nullptr) {
        multinet_->set_det_threshold(model_data_, 0.3f);
    }

    // 加载自定义命令词
    if (!LoadCommands()) {
        ESP_LOGE(TAG, "Failed to load speech commands");
        multinet_->destroy(model_data_);
        model_data_ = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Command detect initialized successfully");
    return true;
}

//=============================================================================
// 命令词加载
//=============================================================================

bool CommandDetect::LoadCommands()
{
    esp_err_t ret = esp_mn_commands_alloc(multinet_, model_data_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate command list: %d", ret);
        return false;
    }

    // 命令词定义：command_id → 拼音字符串
    // MultiNet 中文模型使用拼音（不带声调），空格分隔
    struct CommandEntry {
        int id;
        const char* pinyin;
    };

    static const CommandEntry commands[] = {
        // ═══ 前进 ═══
        {0, "qian jin"},
        {0, "xiang qian zou"},
        {0, "wang qian zou"},
        {0, "xiang qian"},
        {0, "zhi zou"},
        {0, "zou"},
        // ═══ 后退 ═══
        {1, "hou tui"},
        {1, "xiang hou tui"},
        {1, "wang hou tui"},
        {1, "xiang hou"},
        {1, "dao tui"},
        // ═══ 左转 ═══
        {2, "zuo zhuan"},
        {2, "xiang zuo zhuan"},
        {2, "wang zuo zhuan"},
        {2, "zuo guai"},
        {2, "xiang zuo guai"},
        // ═══ 右转 ═══
        {3, "you zhuan"},
        {3, "xiang you zhuan"},
        {3, "wang you zhuan"},
        {3, "you guai"},
        {3, "xiang you guai"},
        // ═══ 停止 ═══
        {4, "ting zhi"},
        {4, "ting xia"},
        {4, "ting"},
        {4, "bie dong"},
        {4, "zhan zhu"},
        // ═══ 跳舞 ═══
        {5, "tiao wu"},
        {5, "tiao ge wu"},
        {5, "lai yi duan"},
        // ═══ 灯光 ═══
        {6, "deng guang san"},
        {7, "deng guang si"},
        {8, "deng guang wu"},
        {9, "deng guang liu"},
        {10, "deng guang qi"},
        {11, "deng guang ba"},
    };

    for (const auto& cmd : commands) {
        ret = esp_mn_commands_add(cmd.id, cmd.pinyin);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to add command '%s' (id=%d): %d", cmd.pinyin, cmd.id, ret);
        }
    }

    // 必须调用 update 来重建 FST 语言模型
    esp_mn_error_t* errors = esp_mn_commands_update();
    if (errors != nullptr) {
        ESP_LOGW(TAG, "%d command(s) could not be parsed:", errors->num);
        for (int i = 0; i < errors->num; i++) {
            ESP_LOGW(TAG, "  - '%s' (id=%d)", errors->phrases[i]->string, errors->phrases[i]->command_id);
        }
    }

    // 注意: 不调用 print_active_speech_commands — 该函数指针在 mn6_cn 模型上可能为 NULL
    ESP_LOGI(TAG, "Commands loaded successfully");
    return true;
}

//=============================================================================
// 命令 ID 映射
//=============================================================================

ChassisCommand CommandDetect::MapCommandId(int command_id)
{
    switch (command_id) {
        case 0:  return ChassisCommand::FORWARD;
        case 1:  return ChassisCommand::BACKWARD;
        case 2:  return ChassisCommand::TURN_LEFT;
        case 3:  return ChassisCommand::TURN_RIGHT;
        case 4:  return ChassisCommand::STOP;
        case 5:  return ChassisCommand::DANCE;
        case 6:  return ChassisCommand::LIGHT_3;
        case 7:  return ChassisCommand::LIGHT_4;
        case 8:  return ChassisCommand::LIGHT_5;
        case 9:  return ChassisCommand::LIGHT_6;
        case 10: return ChassisCommand::LIGHT_7;
        case 11: return ChassisCommand::LIGHT_8;
        default: return ChassisCommand::NONE;
    }
}

//=============================================================================
// 检测控制
//=============================================================================

void CommandDetect::StartDetection()
{
    if (model_data_ != nullptr && multinet_ != nullptr && multinet_->clean != nullptr) {
        multinet_->clean(model_data_);
    }
    input_buffer_.clear();
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}

void CommandDetect::StopDetection()
{
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
    if (model_data_ != nullptr && multinet_ != nullptr && multinet_->clean != nullptr) {
        multinet_->clean(model_data_);
    }
    input_buffer_.clear();
}

bool CommandDetect::IsDetectionRunning() const
{
    return (xEventGroupGetBits(event_group_) & DETECTION_RUNNING_EVENT) != 0;
}

void CommandDetect::OnCommandDetected(std::function<void(ChassisCommand)> callback)
{
    command_detected_callback_ = std::move(callback);
}

std::string CommandDetect::GetLastCommandString() const
{
    static const char* strings[] = {
        "前进", "后退", "左转", "右转", "停止", "跳舞",
        "灯光三", "灯光四", "灯光五", "灯光六", "灯光七", "灯光八"
    };
    int idx = static_cast<int>(last_command_);
    if (idx >= 0 && idx < static_cast<int>(sizeof(strings) / sizeof(strings[0]))) {
        return strings[idx];
    }
    return "未知";
}

//=============================================================================
// 音频喂入与检测
//=============================================================================

void CommandDetect::Feed(const std::vector<int16_t>& data)
{
    if (!IsDetectionRunning() || model_data_ == nullptr || multinet_ == nullptr) {
        return;
    }

    // 累积音频到内部缓冲区
    input_buffer_.insert(input_buffer_.end(), data.begin(), data.end());

    // 当缓冲区数据足够一个 chunk 时，送入 MultiNet 检测
    while (input_buffer_.size() >= static_cast<size_t>(chunksize_)) {
        esp_mn_state_t state = multinet_->detect(model_data_, input_buffer_.data());
        input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + chunksize_);

        if (state == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t* results = multinet_->get_results(model_data_);
            if (results != nullptr && results->num > 0) {
                int command_id = results->command_id[0];
                int phrase_id = results->phrase_id[0];
                float prob = results->prob[0];

                ESP_LOGI(TAG, "Command detected: id=%d, phrase=%d, prob=%.3f, raw='%s'",
                         command_id, phrase_id, prob, results->raw_string);

                // 过滤低置信度结果（低于 0.3 大概率是误识别）
                if (prob < 0.3f) {
                    ESP_LOGD(TAG, "Command rejected: probability too low (%.3f < 0.3)", prob);
                    return;
                }

                ChassisCommand cmd = MapCommandId(command_id);
                if (cmd != ChassisCommand::NONE) {
                    last_command_ = cmd;
                    StopDetection();

                    if (command_detected_callback_) {
                        command_detected_callback_(cmd);
                    }
                }
            }
            return;
        }
        else if (state == ESP_MN_STATE_TIMEOUT) {
            ESP_LOGD(TAG, "MultiNet timeout, resetting");
            if (multinet_->clean != nullptr) {
                multinet_->clean(model_data_);
            }
        }
    }
}
