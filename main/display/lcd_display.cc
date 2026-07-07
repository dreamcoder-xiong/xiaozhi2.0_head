#include "lcd_display.h"

#include <vector>
#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"
#include <cstring>
#include "settings.h"

#include "board.h"
#include "ui.h"


#define TAG "LcdDisplay"

/*改时间更新BUG加入*/
static bool s_lvgl_inited = false;


// Color definitions for dark theme
#define DARK_BACKGROUND_COLOR lv_color_hex(0x121212)       // Dark background
#define DARK_TEXT_COLOR lv_color_white()                   // White text
#define DARK_CHAT_BACKGROUND_COLOR lv_color_hex(0x1E1E1E)  // Slightly lighter than background
#define DARK_USER_BUBBLE_COLOR lv_color_hex(0x1A6C37)      // Dark green
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333) // Dark gray
#define DARK_SYSTEM_BUBBLE_COLOR lv_color_hex(0x2A2A2A)    // Medium gray
#define DARK_SYSTEM_TEXT_COLOR lv_color_hex(0xAAAAAA)      // Light gray text
#define DARK_BORDER_COLOR lv_color_hex(0x333333)           // Dark gray border
#define DARK_LOW_BATTERY_COLOR lv_color_hex(0xFF0000)      // Red for dark mode

// Color definitions for light theme
#define LIGHT_BACKGROUND_COLOR lv_color_white()            // White background
#define LIGHT_TEXT_COLOR lv_color_black()                  // Black text
#define LIGHT_CHAT_BACKGROUND_COLOR lv_color_hex(0xE0E0E0) // Light gray background
#define LIGHT_USER_BUBBLE_COLOR lv_color_hex(0x95EC69)     // WeChat green
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()      // White
#define LIGHT_SYSTEM_BUBBLE_COLOR lv_color_hex(0xE0E0E0)   // Light gray
#define LIGHT_SYSTEM_TEXT_COLOR lv_color_hex(0x666666)     // Dark gray text
#define LIGHT_BORDER_COLOR lv_color_hex(0xE0E0E0)          // Light gray border
#define LIGHT_LOW_BATTERY_COLOR lv_color_black()           // Black for light mode

// Theme color structure
struct ThemeColors
{
    lv_color_t background;
    lv_color_t text;
    lv_color_t chat_background;
    lv_color_t user_bubble;
    lv_color_t assistant_bubble;
    lv_color_t system_bubble;
    lv_color_t system_text;
    lv_color_t border;
    lv_color_t low_battery;
};

// Define dark theme colors
static const ThemeColors DARK_THEME = {
    .background = DARK_BACKGROUND_COLOR,
    .text = DARK_TEXT_COLOR,
    .chat_background = DARK_CHAT_BACKGROUND_COLOR,
    .user_bubble = DARK_USER_BUBBLE_COLOR,
    .assistant_bubble = DARK_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = DARK_SYSTEM_BUBBLE_COLOR,
    .system_text = DARK_SYSTEM_TEXT_COLOR,
    .border = DARK_BORDER_COLOR,
    .low_battery = DARK_LOW_BATTERY_COLOR};

// Define light theme colors
static const ThemeColors LIGHT_THEME = {
    .background = LIGHT_BACKGROUND_COLOR,
    .text = LIGHT_TEXT_COLOR,
    .chat_background = LIGHT_CHAT_BACKGROUND_COLOR,
    .user_bubble = LIGHT_USER_BUBBLE_COLOR,
    .assistant_bubble = LIGHT_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = LIGHT_SYSTEM_BUBBLE_COLOR,
    .system_text = LIGHT_SYSTEM_TEXT_COLOR,
    .border = LIGHT_BORDER_COLOR,
    .low_battery = LIGHT_LOW_BATTERY_COLOR};

// Current theme - initialize based on default config
static ThemeColors current_theme = DARK_THEME;

LV_FONT_DECLARE(font_awesome_30_4);

SpiLcdDisplay::SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                             int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                             DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts)
{
    width_ = width;
    height_ = height;

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++)
    {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");

   // lv_init();
   // ESP_LOGI(TAG, "Initialize LVGL port");
    //lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    //port_cfg.task_priority = 1;
    //port_cfg.task_stack = 1024 * 32;
    //port_cfg.timer_period_ms = 30;
    //lvgl_port_init(&port_cfg);
    if (!s_lvgl_inited)
    {
         ESP_LOGI(TAG, "Initialize LVGL library (once)");
            lv_init();

         ESP_LOGI(TAG, "Initialize LVGL port (CPU0)");

        lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
        port_cfg.task_priority = 4;              // ❗比 idle 高
        port_cfg.task_stack = 1024 * 6;           // 够用，别浪费内存
        port_cfg.timer_period_ms = 10;            // 防止一次补跑太久
        port_cfg.task_affinity = 0;               // ❗固定在 CPU0（关键）

        lvgl_port_init(&port_cfg);

        s_lvgl_inited = true;
    }


    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 0,
            .buff_spiram = 1,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr)
    {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // Update the theme
    // if (current_theme_name_ == "dark")
    // {
    //     current_theme = DARK_THEME;
    // }
    // else if (current_theme_name_ == "light")
    // {
    //     current_theme = LIGHT_THEME;
    // }

    SetupUI();
}

// RGB LCD实现
RgbLcdDisplay::RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                             int width, int height, int offset_x, int offset_y,
                             bool mirror_x, bool mirror_y, bool swap_xy,
                             DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts)
{
    width_ = width;
    height_ = height;

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++)
    {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

   // ESP_LOGI(TAG, "Initialize LVGL library");
    //lv_init();
   // ESP_LOGI(TAG, "Initialize LVGL port");
  // lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
   // port_cfg.task_priority = 1;
    //lvgl_port_init(&port_cfg);
    if (!s_lvgl_inited)
    {
        ESP_LOGI(TAG, "Initialize LVGL library (once)");
        lv_init();
        ESP_LOGI(TAG, "Initialize LVGL port (CPU0)");

        lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
        port_cfg.task_priority = 4;
        port_cfg.task_stack = 1024 * 6;
        port_cfg.timer_period_ms = 10;
        port_cfg.task_affinity = 0;

        lvgl_port_init(&port_cfg);

         s_lvgl_inited = true;
    }


    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = true,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .full_refresh = 1,
            .direct_mode = 1,
        },
    };

    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        }};

    display_ = lvgl_port_add_disp_rgb(&display_cfg, &rgb_cfg);
    if (display_ == nullptr)
    {
        ESP_LOGE(TAG, "Failed to add RGB display");
        return;
    }

    // Update the theme
    if (current_theme_name_ == "dark")
    {
        current_theme = DARK_THEME;
    }
    else if (current_theme_name_ == "light")
    {
        current_theme = LIGHT_THEME;
    }

    SetupUI();
}

LcdDisplay::~LcdDisplay()
{
    // 然后再清理 LVGL 对象
    if (content_ != nullptr)
    {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr)
    {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr)
    {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr)
    {
        lv_obj_del(container_);
    }
    if (display_ != nullptr)
    {
        ;
    }

    if (panel_ != nullptr)
    {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr)
    {
        esp_lcd_panel_io_del(panel_io_);
    }
}

bool LcdDisplay::Lock(int timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void LcdDisplay::Unlock()
{
    lvgl_port_unlock();
}

void LcdDisplay::SetupUI()
{
    DisplayLockGuard lock(this);
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, 240, 240);
    lv_obj_set_align(container_, LV_ALIGN_CENTER);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(container_, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(container_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Status bar */
    // status_bar_ = lv_obj_create(container_);
    // lv_obj_set_size(status_bar_, width_, fonts_.emoji_font->line_height);
    // lv_obj_set_style_radius(status_bar_, 0, 0);
    // lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);
    // lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);

    /* Content - Chat area */
    content_ = lv_obj_create(container_);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_align(content_, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_x(content_, -10);
    lv_obj_set_width(content_, width_);
    lv_obj_set_height(content_, height_ - 25); // Adjust height to fit the status bar
    // lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_bg_color(content_, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(content_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(content_, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(content_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(content_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // Enable scrolling for chat content
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);

    // Create a flex container for chat messages
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    // lv_obj_set_style_pad_row(content_, 10, 0); // Space between messages

    // We'll create chat messages dynamically in SetChatMessage
    chat_message_label_ = nullptr;
}

void LcdDisplay::SetStatus(const char *status)
{
    DisplayLockGuard lock(this);
    if (ui_title_state == nullptr)
    {
        return;
    }
    lv_label_set_text(ui_title_state, status);
    // lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    // lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
}
void LcdDisplay::ShowNotification(const char *notification, int duration_ms)
{
    ;
}
void LcdDisplay::SetEmotion(const char *emotion)
{
    ;
}
#define MAX_MESSAGES 10

void LcdDisplay::SetChatMessage(const char *role, const char *content)
{
    DisplayLockGuard lock(this);
    if (content_ == nullptr)
    {
        return;
    }

    // 避免出现空的消息框
    if (strlen(content) == 0)
        return;

    // Create a message bubble
    lv_obj_t *msg_bubble = lv_obj_create(content_);
    lv_obj_set_style_radius(msg_bubble, 8, 0);
    lv_obj_set_scrollbar_mode(msg_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(msg_bubble, 1, 0);
    lv_obj_set_style_border_color(msg_bubble, current_theme.border, 0);
    // lv_obj_set_style_pad_all(msg_bubble, 8, 0);

    // Create the message text
    lv_obj_t *msg_text = lv_label_create(msg_bubble);
    lv_label_set_text(msg_text, content);

    // 计算文本实际宽度
    lv_coord_t text_width = lv_txt_get_width(content, strlen(content), fonts_.text_font, 0, 0);

    // 计算气泡宽度
    lv_coord_t max_width = width_ * 85 / 100 - 16; // 屏幕宽度的85%
    lv_coord_t min_width = 20;
    lv_coord_t bubble_width;

    // 确保文本宽度不小于最小宽度
    if (text_width < min_width)
    {
        text_width = min_width;
    }

    // 如果文本宽度小于最大宽度，使用文本宽度
    if (text_width < max_width)
    {
        bubble_width = text_width;
    }
    else
    {
        bubble_width = max_width;
    }

    // 设置消息文本的宽度
    lv_obj_set_width(msg_text, bubble_width + 5); // 减去padding
    lv_label_set_long_mode(msg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(msg_text, fonts_.text_font, 0);

    // 设置气泡宽度
    lv_obj_set_width(msg_bubble, bubble_width);
    lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);

    // Set alignment and style based on message role
    if (strcmp(role, "user") == 0)
    {
        // User messages are right-aligned with green background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.user_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.text, 0);

        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void *)"user");

        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);

        // Add some margin
        lv_obj_set_style_pad_left(msg_bubble, 2, 0);

        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    }
    else if (strcmp(role, "assistant") == 0)
    {
        // Assistant messages are left-aligned with white background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.assistant_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.text, 0);

        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void *)"assistant");

        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);

        // Add some margin
        lv_obj_set_style_pad_left(msg_bubble, 5, 0);

        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    }
    else if (strcmp(role, "system") == 0)
    {
        // System messages are center-aligned with light gray background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.system_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.system_text, 0);

        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void *)"system");

        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);

        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    }

    // Create a full-width container for user messages to ensure right alignment
    if (strcmp(role, "user") == 0)
    {
        // Create a full-width container
        lv_obj_t *container = lv_obj_create(content_);
        lv_obj_set_width(container, width_);
        lv_obj_set_height(container, LV_SIZE_CONTENT);

        // Make container transparent and borderless
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);

        // Move the message bubble into this container
        lv_obj_set_parent(msg_bubble, container);

        // Right align the bubble in the container
        lv_obj_align(msg_bubble, LV_ALIGN_RIGHT_MID, -10, 0);

        // Auto-scroll to this container
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
    }
    else if (strcmp(role, "system") == 0)
    {
        // 为系统消息创建全宽容器以确保居中对齐
        lv_obj_t *container = lv_obj_create(content_);
        lv_obj_set_width(container, width_);
        lv_obj_set_height(container, LV_SIZE_CONTENT);

        // 使容器透明且无边框
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);

        // 将消息气泡移入此容器
        lv_obj_set_parent(msg_bubble, container);

        // 将气泡居中对齐在容器中
        lv_obj_align(msg_bubble, LV_ALIGN_CENTER, 0, 0);

        // 自动滚动底部
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
    }
    else
    {
        // For assistant messages
        // Left align assistant messages
        lv_obj_align(msg_bubble, LV_ALIGN_LEFT_MID, 0, 0);

        // Auto-scroll to the message bubble
        lv_obj_scroll_to_view_recursive(msg_bubble, LV_ANIM_ON);
    }

    // Store reference to the latest message label
    chat_message_label_ = msg_text;

    // 检查消息数量是否超过限制
    uint32_t msg_count = lv_obj_get_child_cnt(content_);
    while (msg_count >= MAX_MESSAGES)
    {
        // 删除最早的消息（第一个子节点）
        lv_obj_t *oldest_msg = lv_obj_get_child(content_, 0);
        if (oldest_msg != nullptr)
        {
            lv_obj_del(oldest_msg);
            msg_count--;
            // 删除最早的消息会导致所有气泡整体往上移
            // 所以需要重新滚动到当前消息气泡位置
            lv_obj_scroll_to_view_recursive(msg_bubble, LV_ANIM_ON);
        }
        else
        {
            break;
        }
    }
}
void LcdDisplay::SetIcon(const char *icon)
{
    ;
}
void LcdDisplay::SetTheme(const std::string &theme_name)
{
    ;
}