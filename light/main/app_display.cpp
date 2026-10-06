#include "app_display.h"
#include "app_priv.h"
#include "sdkconfig.h"
#if CONFIG_CYD_DISPLAY
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <platform/CHIPDeviceLayer.h>
#include <atomic>
#include <cstring>
extern uint16_t light_endpoint_id;
using namespace esp_matter;
using namespace chip::app::Clusters;
static const char *TAG = "display";
static spi_device_handle_t lcd, touch;
static std::atomic<bool> pending{false};
static esp_err_t transfer(spi_device_handle_t dev, const void *data, size_t size) {
    spi_transaction_t t = {};
    t.length = size * 8;
    t.tx_buffer = data;
    return spi_device_transmit(dev, &t);
}
static esp_err_t lcd_command(uint8_t cmd, const uint8_t *data = nullptr, size_t size = 0) {
    gpio_set_level(GPIO_NUM_2, 0);
    ESP_RETURN_ON_ERROR(transfer(lcd, &cmd, 1), TAG, "command");
    gpio_set_level(GPIO_NUM_2, 1);
    return size ? transfer(lcd, data, size) : ESP_OK;
}
static esp_err_t rectangle(int x, int y, int w, int h, uint16_t color) {
    const uint8_t xs[] = {uint8_t(x >> 8), uint8_t(x), uint8_t((x+w-1) >> 8), uint8_t(x+w-1)};
    const uint8_t ys[] = {uint8_t(y >> 8), uint8_t(y), uint8_t((y+h-1) >> 8), uint8_t(y+h-1)};
    ESP_RETURN_ON_ERROR(lcd_command(0x2a, xs, 4), TAG, "column");
    ESP_RETURN_ON_ERROR(lcd_command(0x2b, ys, 4), TAG, "row");
    ESP_RETURN_ON_ERROR(lcd_command(0x2c), TAG, "pixels");
    uint8_t pixels[64]; // No full framebuffer or DMA allocation.
    for (size_t i = 0; i < sizeof(pixels); i += 2) {
        pixels[i] = color >> 8; pixels[i+1] = color & 0xff;
    }
    int remaining = w * h;
    while (remaining > 0) {
        int count = remaining > 32 ? 32 : remaining;
        ESP_RETURN_ON_ERROR(transfer(lcd, pixels, count * 2), TAG, "pixels");
        remaining -= count;
    }
    return ESP_OK;
}
// Original 5x7 glyphs for the button labels.
static const uint8_t glyphs[][7] = {
    {16,16,16,16,16,16,31}, {14,4,4,4,4,4,14},
    {14,17,16,23,17,17,14}, {14,17,17,31,17,17,17},
    {30,17,17,30,20,18,17}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {15,16,16,14,1,1,30},
    {17,17,17,17,17,17,14}, {31,1,2,4,8,16,31}
};
static esp_err_t label(const char *text, int y, int scale) {
    const char *alphabet = "LIGARDESUZ";
    int x = (240 - int(strlen(text)) * 6 * scale + scale) / 2;
    for (; *text; ++text, x += 6 * scale) {
        const char *p = strchr(alphabet, *text);
        if (!p) continue;
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (glyphs[p-alphabet][row] & (1 << (4-col)))
                    ESP_RETURN_ON_ERROR(rectangle(x+col*scale, y+row*scale, scale, scale, 0xffff), TAG, "label");
    }
    return ESP_OK;
}
static esp_err_t draw(bool on) {
    ESP_RETURN_ON_ERROR(rectangle(0, 0, 240, 320, on ? 0x0340 : 0x2104), TAG, "background");
    ESP_RETURN_ON_ERROR(label("LUZ", 40, 5), TAG, "title");
    ESP_RETURN_ON_ERROR(label(on ? "LIGADA" : "DESLIGADA", 110, 4), TAG, "state");
    ESP_RETURN_ON_ERROR(rectangle(10, 190, 220, 95, on ? 0xb800 : 0x0460), TAG, "button");
    return label(on ? "DESLIGAR" : "LIGAR", 220, 4);
}
static void toggle(intptr_t) {
    esp_matter_attr_val_t value = {};
    esp_err_t err = attribute::get_val(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &value);
    if (err == ESP_OK) {
        value.val.b = !value.val.b;
        err = attribute::update(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &value);
    }
    if (err != ESP_OK) ESP_LOGE(TAG, "Touch toggle failed: %s", esp_err_to_name(err));
    pending.store(false);
}
static esp_err_t read_pressure(bool *pressed) {
    // XPT2046 Z1, differential conversion. PD=00 restores PENIRQ.
    spi_transaction_t t = {};
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 24; t.tx_data[0] = 0xb0;
    esp_err_t err = spi_device_transmit(touch, &t);
    if (err == ESP_OK) {
        int z1 = ((int(t.rx_data[1]) << 8) | t.rx_data[2]) >> 3;
        *pressed = z1 > 200 && gpio_get_level(GPIO_NUM_36) == 0;
    }
    return err;
}
static void display_task(void *) {
    bool rendered = false, last_state = false, latched = false;
    int down = 0, up = 0;
    while (true) {
        esp_matter_attr_val_t value = {};
        chip::DeviceLayer::PlatformMgr().LockChipStack();
        esp_err_t err = attribute::get_val(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &value);
        chip::DeviceLayer::PlatformMgr().UnlockChipStack();
        if (err == ESP_OK && (!rendered || last_state != value.val.b)) {
            if (draw(value.val.b) == ESP_OK) { rendered = true; last_state = value.val.b; }
        }
        bool pressed = false;
        if (read_pressure(&pressed) == ESP_OK) {
            down = pressed ? down + 1 : 0;
            up = pressed ? 0 : up + 1;
            if (down >= 2 && !latched) {
                latched = true;
                if (!pending.exchange(true)) {
                    if (chip::DeviceLayer::PlatformMgr().ScheduleWork(toggle, 0) != CHIP_NO_ERROR) {
                        pending.store(false);
                        ESP_LOGE(TAG, "Could not schedule touch command");
                    }
                }
            }
            if (up >= 3) latched = false;
            if (down > 3) down = 3;
            if (up > 3) up = 3;
        } else { down = up = 0; } // Failed SPI never counts as release.
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
esp_err_t app_display_init() {
    gpio_config_t output = {};
    output.pin_bit_mask = (1ULL << 2) | (1ULL << 21);
    output.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&output), TAG, "LCD GPIO");
    gpio_set_level(GPIO_NUM_21, 0);
    gpio_config_t irq = {};
    irq.pin_bit_mask = 1ULL << 36; irq.mode = GPIO_MODE_INPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&irq), TAG, "touch IRQ");
    spi_bus_config_t bus = {};
    bus.mosi_io_num = 13; bus.miso_io_num = 12; bus.sclk_io_num = 14;
    bus.quadwp_io_num = bus.quadhd_io_num = -1; bus.max_transfer_sz = 64;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED), TAG, "LCD SPI");
    spi_device_interface_config_t dev = {};
    dev.clock_speed_hz = 20 * 1000 * 1000; dev.spics_io_num = 15; dev.queue_size = 1;
    esp_err_t err = spi_bus_add_device(SPI2_HOST, &dev, &lcd);
    if (err != ESP_OK) { spi_bus_free(SPI2_HOST); return err; }
    bus.mosi_io_num = 32; bus.miso_io_num = 39; bus.sclk_io_num = 25;
    err = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_DISABLED);
    if (err == ESP_OK) {
        dev.clock_speed_hz = 1000 * 1000; dev.spics_io_num = 33;
        err = spi_bus_add_device(SPI3_HOST, &dev, &touch);
        if (err != ESP_OK) spi_bus_free(SPI3_HOST);
    }
    if (err == ESP_OK) err = lcd_command(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));
    if (err == ESP_OK) err = lcd_command(0x28);
    // ILI9341 power, timing and gamma registers for the CYD panel.
    struct Init { uint8_t cmd, count, data[15]; };
    static const Init setup[] = {
        {0xcf,3,{0x00,0xc1,0x30}}, {0xed,4,{0x64,0x03,0x12,0x81}},
        {0xe8,3,{0x85,0x00,0x78}}, {0xcb,5,{0x39,0x2c,0x00,0x34,0x02}},
        {0xf7,1,{0x20}}, {0xea,2,{0x00,0x00}}, {0xc0,1,{0x10}},
        {0xc1,1,{0x00}}, {0xc5,2,{0x30,0x30}}, {0xc7,1,{0xb7}},
        {0xb1,2,{0x00,0x1a}}, {0xb6,3,{0x08,0x82,0x27}},
        {0xf2,1,{0x00}}, {0x26,1,{0x01}},
        {0xe0,15,{0x0f,0x2a,0x28,0x08,0x0e,0x08,0x54,0xa9,0x43,0x0a,0x0f,0x00,0x00,0x00,0x00}},
        {0xe1,15,{0x00,0x15,0x17,0x07,0x11,0x06,0x2b,0x56,0x3c,0x05,0x10,0x0f,0x3f,0x3f,0x0f}}
    };
    for (const auto &item : setup) {
        if (err == ESP_OK) err = lcd_command(item.cmd, item.data, item.count);
    }
    const uint8_t format = 0x55, orientation = 0x48;
    if (err == ESP_OK) err = lcd_command(0x3a, &format, 1);
    if (err == ESP_OK) err = lcd_command(0x36, &orientation, 1);
    if (err == ESP_OK) err = lcd_command(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    if (err == ESP_OK) err = lcd_command(0x29);
    if (err == ESP_OK && xTaskCreate(display_task, "lcd_touch", 4096, nullptr, 3, nullptr) != pdPASS)
        err = ESP_ERR_NO_MEM;
    if (err != ESP_OK) {
        if (touch) { spi_bus_remove_device(touch); spi_bus_free(SPI3_HOST); touch = nullptr; }
        spi_bus_remove_device(lcd); spi_bus_free(SPI2_HOST); lcd = nullptr;
        return err;
    }
    gpio_set_level(GPIO_NUM_21, 1);
    return ESP_OK;
}
#else
esp_err_t app_display_init() { return ESP_OK; }
#endif
