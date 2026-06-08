#ifndef CSV_ALGO_H
#define CSV_ALGO_H

#include <stdint.h>
#include <stddef.h>

/** @brief Format a CSV data line matching the sensor log format.
 *  Format: "rel_s.rel_ms;abs_s.rel_ms;abs_s.rel_ms;x_g;y_g;z_g;V;I;P\r\n"
 *  @param buf      Output buffer (must be >= 128 bytes)
 *  @param buf_size Buffer size
 *  @param rel_s    Relative time seconds
 *  @param rel_ms   Relative time milliseconds
 *  @param abs_s    Absolute/Unix time seconds
 *  @param x_g      X-axis acceleration in g
 *  @param y_g      Y-axis acceleration in g
 *  @param z_g      Z-axis acceleration in g
 *  @param voltage  Supply voltage
 *  @param current  Current consumption
 *  @param power    Power (V * I)
 *  @return Number of chars written (excluding null), or negative on error
 */
int csv_format_line(char *buf, size_t buf_size,
                    uint32_t rel_s, uint32_t rel_ms,
                    uint32_t abs_s,
                    float x_g, float y_g, float z_g,
                    float voltage, float current, float power);

#endif /* CSV_ALGO_H */
