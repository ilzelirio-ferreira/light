#pragma once
#include <esp_err.h>
esp_err_t app_relay_init();
esp_err_t app_relay_set_power(bool on);
esp_err_t app_relay_start_switch();
