#ifndef COMMAND_DETECT_H
#define COMMAND_DETECT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_mn_iface.h>
#include <esp_mn_models.h>

#include <functional>
#include <string>
#include <vector>

// SparkBot 底盘运动命令枚举（对应 UART 协议）
enum class ChassisCommand {
    NONE = -1,
    FORWARD = 0,       // 前进:  x0.0 y1.0
    BACKWARD = 1,      // 后退:  x0.0 y-1.0
    TURN_LEFT = 2,     // 左转:  x-1.0 y0.0
    TURN_RIGHT = 3,    // 右转:  x1.0 y0.0
    STOP = 4,          // 停止:  x0.0 y0.0
    DANCE = 5,         // 跳舞:  d1
    LIGHT_3 = 6,       // 灯光3: w3
    LIGHT_4 = 7,       // 灯光4: w4
    LIGHT_5 = 8,       // 灯光5: w5
    LIGHT_6 = 9,       // 灯光6: w6
    LIGHT_7 = 10,      // 灯光7: w7
    LIGHT_8 = 11,      // 灯光8: w8
};

class CommandDetect {
public:
    CommandDetect();
    ~CommandDetect();

    /**
     * @brief 初始化 MultiNet 模型和命令列表
     * @return true 成功, false 失败
     */
    bool Initialize();

    /**
     * @brief 喂入 16kHz 16-bit mono PCM 音频数据
     * @param data 音频采样数据
     */
    void Feed(const std::vector<int16_t>& data);

    /**
     * @brief 启动命令检测
     */
    void StartDetection();

    /**
     * @brief 停止命令检测并重置状态
     */
    void StopDetection();

    /**
     * @brief 检测是否正在运行
     */
    bool IsDetectionRunning() const;

    /**
     * @brief 设置命令检测回调
     * @param callback 当本地命令被识别时调用，参数为 ChassisCommand 枚举
     */
    void OnCommandDetected(std::function<void(ChassisCommand)> callback);

    /**
     * @brief 获取最后一次识别到的命令的中文描述
     */
    std::string GetLastCommandString() const;

private:
    const esp_mn_iface_t* multinet_ = nullptr;
    model_iface_data_t* model_data_ = nullptr;
    int chunksize_ = 0;
    EventGroupHandle_t event_group_ = nullptr;
    std::vector<int16_t> input_buffer_;
    std::function<void(ChassisCommand)> command_detected_callback_;
    ChassisCommand last_command_ = ChassisCommand::NONE;

    /**
     * @brief 将所有命令词加载到 MultiNet FST 中
     * @return true 成功
     */
    bool LoadCommands();

    /**
     * @brief 将 MultiNet command_id 映射到 ChassisCommand 枚举
     */
    static ChassisCommand MapCommandId(int command_id);
};

#endif // COMMAND_DETECT_H
