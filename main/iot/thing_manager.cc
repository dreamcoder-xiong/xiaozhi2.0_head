#include "thing_manager.h"

#include <esp_log.h>

#define TAG "ThingManager"

namespace iot {

void ThingManager::AddThing(Thing* thing) {
    things_.push_back(thing);
}

std::string ThingManager::GetDescriptorsJson() {
    std::string json_str = "[";
    for (auto& thing : things_) {
        json_str += thing->GetDescriptorJson() + ",";
    }
    if (json_str.back() == ',') {
        json_str.pop_back();
    }
    json_str += "]";
    return json_str;
}

bool ThingManager::GetStatesJson(std::string& json, bool delta) {
    if (!delta) {
        last_states_.clear();
    }
    bool changed = false;
    json = "[";
    // 枚举thing，获取每个thing的state，如果发生变化，则更新，保存到last_states_
    // 如果delta为true，则只返回变化的部分
    for (auto& thing : things_) {
        std::string state = thing->GetStateJson();
        if (delta) {
            // 如果delta为true，则只返回变化的部分
            auto it = last_states_.find(thing->name());
            if (it != last_states_.end() && it->second == state) {
                continue;
            }
            changed = true;
            last_states_[thing->name()] = state;
        }
        json += state + ",";
    }
    if (json.back() == ',') {
        json.pop_back();
    }
    json += "]";
    return changed;
}

void ThingManager::Invoke(const cJSON* command) {
    // P0修复: NULL检查, 防止 name 字段缺失 → 崩溃
    auto name = cJSON_GetObjectItem(command, "name");
    if (name == NULL || name->valuestring == NULL) {
        ESP_LOGE(TAG, "IoT command missing 'name' field");
        return;
    }
    for (auto& thing : things_) {
        if (thing->name() == name->valuestring) {
            thing->Invoke(command);
            return;
        }
    }
    // P0修复: Thing 未匹配时打印警告日志, 便于排查服务端/固件 Thing 名不一致问题
    ESP_LOGW(TAG, "Thing not found: %s", name->valuestring);
}

} // namespace iot
