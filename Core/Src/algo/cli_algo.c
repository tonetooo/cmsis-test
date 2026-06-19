#include "algo/cli_algo.h"
#include <string.h>
#include <stdlib.h>

CliParseResult_t cli_parse_cmd(const char *buf) {
    CliParseResult_t result = { CLI_UNKNOWN, 0.0f, false };
    if (!buf) return result;

    if (strcmp(buf, "help") == 0) {
        result.cmd = CLI_HELP;
    } else if (strcmp(buf, "status") == 0) {
        result.cmd = CLI_STATUS;
    } else if (strcmp(buf, "accel") == 0) {
        result.cmd = CLI_ACCEL;
    } else if (strcmp(buf, "trigger") == 0) {
        result.cmd = CLI_TRIGGER_GET;
    } else if (strncmp(buf, "trigger ", 8) == 0) {
        result.cmd = CLI_TRIGGER_SET;
        result.value = atof(buf + 8);
        result.valid = (result.value > 0.0f && result.value < 10.0f);
    } else if (strcmp(buf, "log") == 0) {
        result.cmd = CLI_LOG;
    } else if (strcmp(buf, "test") == 0) {
        result.cmd = CLI_TEST;
    } else if (strcmp(buf, "sdtest") == 0) {
        result.cmd = CLI_SDTEST;
    } else if (strcmp(buf, "modem_on") == 0) {
        result.cmd = CLI_MODEM_ON;
    }
    return result;
}
