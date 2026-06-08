#include "algo/csv_algo.h"
#include <stdio.h>

int csv_format_line(char *buf, size_t buf_size,
                    uint32_t rel_s, uint32_t rel_ms,
                    uint32_t abs_s,
                    float x_g, float y_g, float z_g,
                    float voltage, float current, float power) {
    if (!buf || buf_size < 128) return -1;
    return snprintf(buf, buf_size,
        "%lu.%03lu;%lu.%03lu;%lu.%03lu;%.6f;%.6f;%.6f;%.2f;%.2f;%.2f\r\n",
        (unsigned long)rel_s, (unsigned long)rel_ms,
        (unsigned long)abs_s,  (unsigned long)rel_ms,
        (unsigned long)abs_s,  (unsigned long)rel_ms,
        x_g, y_g, z_g,
        voltage, current, power);
}
