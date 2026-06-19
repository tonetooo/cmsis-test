#ifndef SENSOR_ALGO_H
#define SENSOR_ALGO_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Check if acceleration magnitude exceeds the motion trigger threshold.
 *  @param magnitude Current |a| = sqrt(x^2 + y^2) in g
 *  @param trigger_g Threshold in g (e.g. 0.02f)
 *  @return true if magnitude >= trigger_g
 */
bool sensor_is_motion(float magnitude, float trigger_g);

/** @brief Check if the signal has settled (below threshold long enough).
 *
 *  Returns true when the settling timer has exceeded the required duration
 *  AND the acquisition has run longer than the minimum duration.
 *  Micro-motion debouncing is handled in sensor_task.c.
 *
 *  @param settling_ms       Elapsed time since settling started (ms)
 *  @param settling_duration Required settling time (ms, e.g. 3000)
 *  @param elapsed_ms        Total elapsed acquisition time (ms)
 *  @param min_duration      Minimum acquisition duration (ms, e.g. 3000)
 *  @return true if settling is complete (event finished)
 */
bool sensor_check_settling(uint32_t settling_ms, uint32_t settling_duration,
                           uint32_t elapsed_ms, uint32_t min_duration);

/** @brief Check if any axis exceeds earthquake threshold.
 *  @param x X-axis acceleration in g
 *  @param y Y-axis acceleration in g
 *  @param z Z-axis acceleration in g
 *  @param threshold Max absolute value per axis (e.g. 2.0f)
 *  @return true if any axis exceeds |threshold|
 */
bool sensor_is_earthquake(float x, float y, float z, float threshold);

/** @brief Sign-extend a 20-bit two's complement value to int32.
 *  @param raw 20-bit signed value in lower 20 bits
 *  @return Sign-extended int32
 */
int32_t sensor_sign_extend_20bit(uint32_t raw);

/** @brief Convert raw 20-bit accelerometer value to G force.
 *  @param raw        Raw 20-bit register value
 *  @param sensitivity Sensitivity in g/LSB (e.g. 0.000061035f for +/-8g range)
 *  @return Acceleration in g
 */
float sensor_raw_to_g(uint32_t raw, float sensitivity);

#endif /* SENSOR_ALGO_H */
