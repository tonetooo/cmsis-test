#include "algo/sensor_algo.h"

bool sensor_is_motion(float magnitude, float trigger_g) {
    return magnitude >= trigger_g;
}

bool sensor_check_settling(uint32_t settling_ms, uint32_t settling_duration,
                           uint32_t elapsed_ms, uint32_t min_duration) {
    return (settling_ms > settling_duration && elapsed_ms > min_duration);
}

bool sensor_is_earthquake(float x, float y, float z, float threshold) {
    return (x > threshold || x < -threshold ||
            y > threshold || y < -threshold ||
            z > threshold || z < -threshold);
}

int32_t sensor_sign_extend_20bit(uint32_t raw) {
    if (raw & 0x80000) {
        return (int32_t)(raw | 0xFFF00000);
    }
    return (int32_t)raw;
}

float sensor_raw_to_g(uint32_t raw, float sensitivity) {
    return (float)sensor_sign_extend_20bit(raw) * sensitivity;
}
