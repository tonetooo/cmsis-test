#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stdio.h>

/* ============================================================
 * Console Logging System — log levels + debug toggle
 *
 * Levels:
 *   CONS_OK   — [OK] success messages
 *   CONS_ERR  — [ERR] error / failure messages
 *   CONS_WARN — [WARN] warning / retry messages
 *   CONS_INFO — plain info / step messages (passthrough)
 *   CONS_DBG  — debug / diagnostic — hidden unless cons_dbg=1
 *   CONS      — plain passthrough (unchanged)
 *
 * All macros preserve the format string exactly as written.
 * Type 'debug' in CLI to toggle CONS_DBG visibility.
 * ============================================================ */

/* Debug toggle — 0 = hide CONS_DBG, 1 = show */
extern volatile uint8_t cons_dbg;

/* Log level macros (plain text, no ANSI — compatible with all terminals) */
#define CONS(fmt, ...)       printf(fmt, ##__VA_ARGS__)
#define CONS_OK(fmt, ...)    printf(fmt, ##__VA_ARGS__)
#define CONS_ERR(fmt, ...)   printf(fmt, ##__VA_ARGS__)
#define CONS_WARN(fmt, ...)  printf(fmt, ##__VA_ARGS__)
#define CONS_INFO(fmt, ...)  printf(fmt, ##__VA_ARGS__)

#define CONS_DBG(fmt, ...)   do { if (cons_dbg) { printf(fmt, ##__VA_ARGS__); } } while(0)

#endif /* CONSOLE_H */
