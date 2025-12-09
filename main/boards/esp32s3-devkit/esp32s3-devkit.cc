//
// Created by ivoglent on 11/4/2025.
//
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#include "boards/esp32s3-devkit/config.h"
#include "codecs/no_audio_codec.h"
#include "led/single_led.h"
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include "oled_display.h"
#include "wifi_station.h"
#include "assets/lang_config.h"
#include "mcp_utils.h"
#define TAG ""

std::string exampleJson = "{\"success\":true,\"data\":{\"room\":\"Working room\",\"toolsVersion\":12,\"nodeUUID\":\"lyly-working-room\",\"tools\":[{\"name\":\"smarthome.working-room-switch-01-L1\",\"description\":\"Bật tắt Unused\",\"roomContext\":{\"roomId\":3,\"nodeUUID\":\"lyly-working-room\",\"roomName\":\"Working room\"},\"parameters\":[{\"name\":\"state\",\"type\":\"string\",\"required\":true}]},{\"name\":\"smarthome.working-room-switch-01-L2\",\"description\":\"Bật tắt Đèn tuýp\",\"roomContext\":{\"roomId\":3,\"nodeUUID\":\"lyly-working-room\",\"roomName\":\"Working room\"},\"parameters\":[{\"name\":\"state\",\"type\":\"string\",\"required\":true}]},{\"name\":\"smarthome.working-room-switch-01-L3\",\"description\":\"Bật tắt Quạt\",\"roomContext\":{\"roomId\":3,\"nodeUUID\":\"lyly-working-room\",\"roomName\":\"Working room\"},\"parameters\":[{\"name\":\"state\",\"type\":\"string\",\"required\":true}]},{\"name\":\"smarthome.working-room-switch-01-L4\",\"description\":\"Bật tắt Đèn trần\",\"roomContext\":{\"roomId\":3,\"nodeUUID\":\"lyly-working-room\",\"roomName\":\"Working room\"},\"parameters\":[{\"name\":\"state\",\"type\":\"string\",\"required\":true}]},{\"name\":\"mcp.tool.test\",\"description\":\"execute test tool and checking system status\",\"roomContext\":null,\"parameters\":[]}]},\"message\":null}";

class Esp32s3Devkit : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    Display* display_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    ReturnValue call_tool_execute(const std::string& name, const PropertyList& args) {
        try {
            const auto state = args["state"].value<std::string>();
            ESP_LOGI(TAG, "Executing tool: %s, state: %s", name.c_str(), state.c_str());

            return std::string("OK: state=" + state);
        }
        catch (std::exception& e) {
            return std::string("Error: ") + e.what();
        }
    }

    void registerTools() {
        cJSON* root = cJSON_Parse(exampleJson.c_str());
        if (!root) return;

        cJSON* data = cJSON_GetObjectItem(root, "data");
        if (!data) { cJSON_Delete(root); return; }

        cJSON* tools = cJSON_GetObjectItem(data, "tools");
        if (!tools || !cJSON_IsArray(tools)) {
            cJSON_Delete(root);
            return;
        }

        cJSON* toolItem = nullptr;
        auto& mcp_server = McpServer::GetInstance();
        cJSON_ArrayForEach(toolItem, tools) {
            cJSON* jName = cJSON_GetObjectItem(toolItem, "name");
            cJSON* jDesc = cJSON_GetObjectItem(toolItem, "description");
            cJSON* jParams = cJSON_GetObjectItem(toolItem, "parameters");

            if (!jName || !jDesc || !jParams)
                continue;

            std::string name = jName->valuestring;
            std::string desc = jDesc->valuestring;

            PropertyList props = ParseToolParameters(jParams);

            // Create tool
            McpTool* tool = nullptr;
            tool = new McpTool(
                name,
                desc,
                props,
                [name, this](const PropertyList& args) -> ReturnValue {
                    return call_tool_execute(name, args);
                }
            );

            // Add to Server
            mcp_server.AddTool(tool);
        }

        cJSON_Delete(root);
    }


    // MCP Tools 初始化
    void InitializeTools() {
        /*auto& mcp_server = McpServer::GetInstance();
        // 例1：无参数，控制机器人前进
        mcp_server.AddTool("self.iot.turn_on_light", "Bật tắt đèn phòng tắm", PropertyList({
            Property("turn", kPropertyTypeBoolean),
        }), [this](const PropertyList& properties) -> ReturnValue {
            ESP_LOGI(TAG, "Turn %d light in the bath room", properties["turn"].value<bool>());
            return true;
        });*/
        registerTools();
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

public:
    // 构造函数
    Esp32s3Devkit() : boot_button_(BOOT_BUTTON_GPIO), volume_up_button_(VOLUME_UP_BUTTON_GPIO), volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        level = 100;
        charging = false;
        discharging = true;
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

// 注册开发板
DECLARE_BOARD(Esp32s3Devkit);