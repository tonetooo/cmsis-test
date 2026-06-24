#include "tasks.h"
#include "wdt.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include "console.h"
#include "quectel_drive.h"

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
    if (buf != NULL && size > 0) {
        strncpy(buf, upload_queue.files[upload_queue.head], size - 1);
        buf[size - 1] = '\0';
    }
    upload_queue.head = (upload_queue.head + 1) % UPLOAD_QUEUE_SIZE;
    upload_queue.count--;
    return 0;
}

void StartFileTask(void *argument) {
    CONS_INFO("[FILE] Task started (prio=Normal)\r\n");

    /* === BOOT: Init RTC backup registers (survive NRST) for upload tracking === */
    Modem_BackupInit();

    /* === BOOT: Scan SD for highest TRIG_*.CSV + populate queue with pending === */
    if (osMutexAcquire(sd_mutexHandle, 1000) == osOK) {
        FRESULT fr = f_mount(&fs, "0:/", 1);
        if (fr == FR_OK) {
            DIR dir;
            FILINFO fno;
            fr = f_opendir(&dir, "0:/");
            if (fr == FR_OK) {
                int max_idx = 0;
                int pending_indices[UPLOAD_QUEUE_SIZE];
                int pending_count = 0;

                while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                    int idx;
                    if (sscanf(fno.fname, "TRIG_%d.CSV", &idx) == 1) {
                        if (idx > max_idx) max_idx = idx;

                        /* PRIMARY CHECK: RTC backup register (survives NRST, no NAND delay) */
                        if (Modem_IsUploaded(idx)) {
                            CONS_INFO("[FILE] TRIG_%03d skipped: backup register set\r\n", idx);
                            continue;
                        }

                        /* FALLBACK CHECK: .DONE marker file on SD */
                        char done_name[32];
                        snprintf(done_name, sizeof(done_name), "TRIG_%03d.DONE", idx);
                        FIL done_f;
                        FRESULT done_fr = f_open(&done_f, done_name, FA_READ);
                        if (done_fr == FR_OK) {
                            f_close(&done_f);  /* Already uploaded, skip */
                            CONS_INFO("[FILE] TRIG_%03d skipped: .DONE file exists\r\n", idx);
                        } else {
                            /* No .DONE file + unset backup register → needs upload */
                            if (pending_count < UPLOAD_QUEUE_SIZE) {
                                pending_indices[pending_count++] = idx;
                                CONS_INFO("[FILE] TRIG_%03d queued: pending upload\r\n", idx);
                            } else {
                                CONS_WARN("[FILE] TRIG_%03d DROPPED: queue full (%d)\r\n", idx, UPLOAD_QUEUE_SIZE);
                            }
                        }
                    }
                }

                /* Sort pending indices ascending (simple bubble for tiny array) */
                for (int i = 0; i < pending_count - 1; i++) {
                    for (int j = 0; j < pending_count - i - 1; j++) {
                        if (pending_indices[j] > pending_indices[j + 1]) {
                            int tmp = pending_indices[j];
                            pending_indices[j] = pending_indices[j + 1];
                            pending_indices[j + 1] = tmp;
                        }
                    }
                }

                /* Push pending files to upload queue in order */
                for (int i = 0; i < pending_count; i++) {
                    char fname[32];
                    snprintf(fname, sizeof(fname), "TRIG_%03d.CSV", pending_indices[i]);
                    upload_queue_push(fname);
                    CONS_INFO("[FILE] Queued pending: %s\r\n", fname);
                }
                CONS_INFO("[FILE] Boot SD scan: highest TRIG_%03d, pending=%d",
                          max_idx, pending_count);
                f_closedir(&dir);

                /* Wake modem_task if there are pending files to upload */
                if (pending_count > 0) {
                    CONS_INFO("[FILE] Signaling EVT_FILE_READY for %d pending upload(s)\r\n",
                              pending_count);
                    osEventFlagsSet(sensor_event_flagsHandle, EVT_FILE_READY);
                }
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
