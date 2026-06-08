#ifndef CLI_ALGO_H
#define CLI_ALGO_H

#include <stdint.h>
#include <stdbool.h>

/** Command IDs returned by cli_parse_cmd */
typedef enum {
    CLI_UNKNOWN,
    CLI_HELP,
    CLI_STATUS,
    CLI_ACCEL,
    CLI_TRIGGER_GET,
    CLI_TRIGGER_SET,
    CLI_LOG,
    CLI_TEST,
    CLI_SDTEST,
    CLI_MODEM_ON,
} CliCmd_t;

/** Result of parsing a CLI command */
typedef struct {
    CliCmd_t cmd;        /**< Identified command */
    float    value;      /**< Parsed float value (for CLI_TRIGGER_SET) */
    bool     valid;      /**< true if value is valid (for range validation) */
} CliParseResult_t;

/** @brief Parse a CLI command string (no HAL/CMSIS dependencies).
 *  @param buf Null-terminated command buffer (without \r\n).
 *  @return Parsed result with cmd ID and optional float value.
 */
CliParseResult_t cli_parse_cmd(const char *buf);

#endif /* CLI_ALGO_H */
