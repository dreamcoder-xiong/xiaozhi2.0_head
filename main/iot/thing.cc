#include "thing.h"
#include "application.h"

#include <esp_log.h>

#define TAG "Thing"


namespace iot {

static std::map<std::string, std::function<Thing*()>>* thing_creators = nullptr;

void RegisterThing(const std::string& type, std::function<Thing*()> creator) {
    if (thing_creators == nullptr) {
        thing_creators = new std::map<std::string, std::function<Thing*()>>();
    }
    (*thing_creators)[type] = creator;
}

Thing* CreateThing(const std::string& type) {
    auto creator = thing_creators->find(type);
    if (creator == thing_creators->end()) {
        ESP_LOGE(TAG, "Thing type not found: %s", type.c_str());
        return nullptr;
    }
    return creator->second();
}

std::string Thing::GetDescriptorJson() {
    std::string json_str = "{";
    json_str += "\"name\":\"" + name_ + "\",";
    json_str += "\"description\":\"" + description_ + "\",";
    json_str += "\"properties\":" + properties_.GetDescriptorJson() + ",";
    json_str += "\"methods\":" + methods_.GetDescriptorJson();
    json_str += "}";
    return json_str;
}

std::string Thing::GetStateJson() {
    std::string json_str = "{";
    json_str += "\"name\":\"" + name_ + "\",";
    json_str += "\"state\":" + properties_.GetStateJson();
    json_str += "}";
    return json_str;
}

void Thing::Invoke(const cJSON* command) {
    // P0修复: NULL检查, 防止 method 字段缺失 → 崩溃
    auto method_name = cJSON_GetObjectItem(command, "method");
    if (method_name == NULL || method_name->valuestring == NULL) {
        ESP_LOGE(TAG, "IoT command missing 'method' field");
        return;
    }

    auto input_params = cJSON_GetObjectItem(command, "parameters");

    try {
        auto& method = methods_[method_name->valuestring];
        for (auto& param : method.parameters()) {
            auto input_param = cJSON_GetObjectItem(input_params, param.name().c_str());
            if (param.required() && input_param == nullptr) {
                throw std::runtime_error("Parameter " + param.name() + " is required");
            }
            // P0修复: 可选参数不存在时跳过, 防止 NULL 解引用
            if (input_param == nullptr) {
                continue;
            }
            if (param.type() == kValueTypeNumber) {
                param.set_number(input_param->valueint);
            } else if (param.type() == kValueTypeString) {
                param.set_string(input_param->valuestring);
            } else if (param.type() == kValueTypeBoolean) {
                param.set_boolean(input_param->valueint == 1);
            }
        }

        // P0修复: 直接同步调用 method.Invoke(), 不使用 Schedule
        // 原因: Schedule 的 lambda 捕获了栈上的局部引用变量 &method,
        // Thing::Invoke 返回后栈帧销毁 → Schedule 延迟执行时访问悬空引用
        // → use-after-free (UB): 命令随机丢失 / 偶尔成功 / 极端情况下崩溃
        // UART 写入是同步且快速的 (uart_write_bytes 非阻塞), 无需异步
        method.Invoke();
    } catch (const std::runtime_error& e) {
        // P0修复: method_name 已在上方验证过非NULL, 此处安全
        ESP_LOGE(TAG, "Method not found: %s", method_name->valuestring);
    }
}


} // namespace iot
