#include "tasks.h"
#include "wdt.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include "console.h"

extern FATFS fs;

char latest_filename[32] = {0};
volatile uint8_t upload_busy = 0;

/* RAM upload queue — no SD I/O, avoids FatFs cache persistence issues */
UploadQueue_t upload_queue = { .head = 0, .tail = 0, .count = 0 };

int upload_queue_push(const char* filename) {
    if (upload_queue.count >= UPLOAD_QUEUE_SIZE) {
        CONS_ERR("[QUEUE] Push '%s' FAILED — queue full (%d)", filename, UPLOAD_QUEUE_SIZE);
        return 0;
    }
    strncpy(upload_queue.files[upload_queue.tail], filename, 31);
    upload_queue.files[upload_queue.tail][31] = '\0';
    upload_queue.tail = (upload_queue.tail + 1) % UPLOAD_QUEUE_SIZE;
    upload_queue.count++;
    return 1;
}

int upload_queue_peek(char* buf, int size) {
    if (upload_queue.count <= 0) return -1;
    strncpy(buf, upload_queue.files[upload_queue.head], size - 1);
    buf[size - 1] = '\0';
    return 0;
}

int upload_queue_pop(char* buf, int size) {
    if (upload_queue.count <= 0) return -1;
    strncpy(buf, upload_queue.files[upload_queue.head], size - 1);
    buf[size - 1] = '\0';
    upload_queue.head = (upload_queue.head + 1) % UPLOAD_QUEUE_SIZE;
    upload_queue.count--;
    return 0;
}

void StartFileTask(void *argument) {
    CONS_INFO("[FILE] Task started (prio=Normal)\r\n");

    /* === BOOT: Scan SD for highest TRIG_*.CSV === */
    if (osMutexAcquire(sd_mutexHandle, 1000) == osOK) {
        FRESULT fr = f_mount(&fs, "0:/", 1);
        if (fr == FR_OK) {
            DIR dir;
            FILINFO fno;
            fr = f_opendir(&dir, "0:/");
            if (fr == FR_OK) {
                int max_idx = 0;
                while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                    int idx;
                    if (sscanf(fno.fname, "TRIG_%d.CSV", &idx) == 1) {
                        if (idx > max_idx) max_idx = idx;
                    }
                }
                CONS_INFO("[FILE] Boot SD scan: highest TRIG_%03d, next=%d",
                          max_idx, max_idx + 1);
                f_closedir(&dir);
            }
        }
        osMutexRelease(sd_mutexHandle);
    }

    /* === Idle loop — available for future expansion === */
    for (;;) {
        WDT_Refresh();
        osDelay(1000);
    }
}
