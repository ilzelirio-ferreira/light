#include "app_relay.h"
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S2
#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_matter.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <platform/CHIPDeviceLayer.h>
#include <atomic>
extern uint16_t light_endpoint_id;
static const char *TAG = "relay_switch";
static std::atomic<bool> pending{false};
static constexpr gpio_num_t relay_pin = GPIO_NUM_4;
static constexpr gpio_num_t switch_pin = GPIO_NUM_2;

esp_err_t app_relay_set_power(bool on)
{
#if CONFIG_RELAY_ACTIVE_LOW
    on = !on;
#endif
    return gpio_set_level(relay_pin, on);
}

esp_err_t app_relay_init()
{
    // Preload the inactive output before enabling the driver.
    ESP_RETURN_ON_ERROR(app_relay_set_power(false), TAG, "relay initial level");
    gpio_config_t output = {};
    output.pin_bit_mask = 1ULL << relay_pin;
    output.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&output), TAG, "relay GPIO");
    gpio_config_t input = {};
    input.pin_bit_mask = 1ULL << switch_pin;
    input.mode = GPIO_MODE_INPUT;
    input.pull_up_en = GPIO_PULLUP_ENABLE;
    return gpio_config(&input);
}

static void toggle(intptr_t)
{
    using namespace chip::app::Clusters;
    esp_matter_attr_val_t value = {};
    esp_err_t err = esp_matter::attribute::get_val(light_endpoint_id, OnOff::Id,
                                                   OnOff::Attributes::OnOff::Id, &value);
    if (err == ESP_OK) {
        value.val.b = !value.val.b;
        err = esp_matter::attribute::update(light_endpoint_id, OnOff::Id,
                                            OnOff::Attributes::OnOff::Id, &value);
    }
    if (err != ESP_OK) ESP_LOGE(TAG, "Switch update failed: %s", esp_err_to_name(err));
    pending.store(false);
}

static void switch_task(void *)
{
    // Initial switch position does not override the restored Matter state.
    int stable = gpio_get_level(switch_pin);
    int candidate = stable, samples = 0;
    while (true) {
        int level = gpio_get_level(switch_pin);
        if (level != candidate) { candidate = level; samples = 1; }
        else if (samples < 4) { ++samples; }
        if (samples >= 4 && candidate != stable && !pending.exchange(true)) {
            // Change either direction: a wall switch remains usable after app control.
            if (chip::DeviceLayer::PlatformMgr().ScheduleWork(toggle, 0) == CHIP_NO_ERROR) {
                stable = candidate;
            } else {
                pending.store(false);
                ESP_LOGE(TAG, "Could not schedule switch update");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t app_relay_start_switch()
{
    return xTaskCreate(switch_task, "wall_switch", 3072, nullptr, 3, nullptr) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}
#else
esp_err_t app_relay_init() { return ESP_OK; }
esp_err_t app_relay_set_power(bool) { return ESP_OK; }
esp_err_t app_relay_start_switch() { return ESP_OK; }
#endif
