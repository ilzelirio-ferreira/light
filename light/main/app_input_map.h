#pragma once
#include <stdint.h>
constexpr uint8_t APP_DEFAULT_INPUTS[6] = {34,36,38,40,17,21};
constexpr uint8_t APP_OUTPUT_PINS[6] = {39,37,35,33,18,16};
constexpr bool app_input_map_valid(const uint8_t *pins) {
    if (!pins) return false;
    for (unsigned i=0;i<6;++i) {
        bool allowed=false;
        for (auto pin: APP_DEFAULT_INPUTS) if (pins[i]==pin) allowed=true;
        if (!allowed) return false;
        for (unsigned j=0;j<i;++j) if (pins[i]==pins[j]) return false;
    }
    return true;
}
