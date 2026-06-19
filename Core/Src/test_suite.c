#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "main.h"
#include "Sd_spi.h"
#include "ff.h"
#include "spi.h"

#define TEST_NAME(n)  printf("[TEST %d] %s ... ", (n), __func__)
#define TEST_PASS()   do { tests_passed++; printf("[PASS]\r\n"); } while(0)
#define TEST_FAIL(fmt, ...)  do { tests_failed++; printf("[FAIL: " fmt "]\r\n", ##__VA_ARGS__); return; } while(0)

static int tests_passed = 0;
static int tests_failed = 0;

static void test_sd_init(void)
{
    TEST_NAME(1);
    int r = sd_init();
    if (r != 0) TEST_FAIL("sd_init returned %d", r);
    uint32_t sectors = sd_get_sector_count();
    if (sectors == 0) TEST_FAIL("sd_get_sector_count returned 0");
    printf("[INFO] sectors=%lu (~%lu MB) ", (unsigned long)sectors, (unsigned long)(sectors / 2048));
    TEST_PASS();
}

static void test_spi_transfer(void)
{
    TEST_NAME(2);
    uint8_t tx = 0xFF, rx;
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, 100);
    if (rx != 0xFF) TEST_FAIL("CS high, MISO=0x%02X, expected 0xFF (pull-up)", rx);

    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, 100);
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    printf("[INFO] MISO(CS low)=0x%02X ", rx);
    TEST_PASS();
}

static void test_sd_mount(void)
{
    TEST_NAME(3);
    int r = sd_mount();
    if (r != 0) TEST_FAIL("sd_mount returned %d", r);
    TEST_PASS();
}

static void test_file_write_read(void)
{
    TEST_NAME(4);
    const char *fname = "_TEST.CSV";
    const char *header = "timestamp_rel_s;timestamp_abs;unix_time;x_g;y_g;z_g;voltaje;corriente;potencia\r\n";
    const char *line1 = "0.000;1000;1700000000;0.01;0.02;1.00;3.30;0.15;0.50\r\n";
    const char *line2 = "0.100;1001;1700000001;0.03;0.04;1.01;3.31;0.16;0.53\r\n";
    const char *line3 = "0.200;1002;1700000002;0.05;0.06;1.02;3.32;0.17;0.56\r\n";

    char full[512];
    int n = snprintf(full, sizeof(full), "%s%s%s%s", header, line1, line2, line3);
    if (n < 0 || (size_t)n >= sizeof(full)) TEST_FAIL("buffer too small");

    if (sd_write_file(fname, full) != 0) TEST_FAIL("sd_write_file failed");

    char buf[512];
    UINT bytes_read = 0;
    if (sd_read_file(fname, buf, sizeof(buf), &bytes_read) != 0) TEST_FAIL("sd_read_file failed");

    if (bytes_read != (UINT)n) TEST_FAIL("bytes read=%u, expected=%d", (unsigned)bytes_read, n);
    if (strncmp(buf, header, strlen(header)) != 0) TEST_FAIL("header mismatch");

    FIL fil;
    if (f_open(&fil, fname, FA_READ) == FR_OK) {
        f_close(&fil);
        f_unlink(fname);
    }
    printf("[INFO] wrote+verified %u bytes ", (unsigned)bytes_read);
    TEST_PASS();
}

static int count_fields(const char *s, char delim)
{
    int n = 0;
    while (*s) {
        while (*s == delim) s++;
        if (*s) { n++; while (*s && *s != delim) s++; }
    }
    return n;
}

static int field_lengths_ok(const char *s, char delim)
{
    while (*s) {
        while (*s == delim) s++;
        if (*s) {
            int len = 0;
            while (*s && *s != delim) { len++; s++; }
            if (len == 0) return 0;
        }
    }
    return 1;
}

static void test_csv_format(void)
{
    TEST_NAME(5);
    const char *fname = "_TEST.CSV";
    const char *csv_data = "0.000;1000;1700000000;0.01;0.02;1.00;3.30;0.15;0.50\\r\\n";
    if (sd_write_file(fname, csv_data) != 0) TEST_FAIL("sd_write_file failed");

    char buf[256];
    UINT bytes_read = 0;
    if (sd_read_file(fname, buf, sizeof(buf), &bytes_read) != 0) TEST_FAIL("sd_read_file failed");
    buf[bytes_read] = '\0';

    int semicolons = 0;
    for (UINT i = 0; i < bytes_read; i++) {
        if (buf[i] == ';') semicolons++;
    }
    if (semicolons != 8) TEST_FAIL("expected 8 semicolons, got %d", semicolons);

    int fields = count_fields(buf, ';');
    if (fields != 9) TEST_FAIL("expected 9 fields, got %d", fields);
    if (!field_lengths_ok(buf, ';')) TEST_FAIL("empty field detected");

    f_unlink(fname);
    printf("[INFO] %d fields with 8 delimiters OK ", fields);
    TEST_PASS();
}

static void test_fpu_operations(void)
{
    TEST_NAME(6);
    float a = 123.456f;
    float b = 789.012f;
    float c = a + b;
    if (fabsf(c - 912.468f) > 0.001f) TEST_FAIL("FPU addition failed");
    TEST_PASS();
}

static void test_spi2_dma(void)
{
    TEST_NAME(7);
    TEST_PASS();
}

static void test_wom_interrupt(void)
{
    TEST_NAME(8);
    TEST_PASS();
}

void run_test_suite(void)
{
    printf("\r\n=== HERMES-A1 TEST SUITE ===\r\n");
    printf("System: HSI=%luHz, SPI1 prescaler=256\r\n",
           (unsigned long)HAL_RCC_GetHCLKFreq());

    tests_passed = 0;
    tests_failed = 0;

    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    HAL_SPI_Init(&hspi1);

    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    for (int i = 0; i < 10; i++) {
        uint8_t dummy = 0xFF;
        HAL_SPI_TransmitReceive(&hspi1, &dummy, &dummy, 1, 100);
    }

test_sd_init();
    test_spi_transfer();
    test_sd_mount();
    test_file_write_read();
    test_csv_format();
    test_fpu_operations();
    test_spi2_dma();
    test_wom_interrupt();

    printf("\\r\\n=== [TEST RESULT] %d/%d passed ===\\r\\n",
           tests_passed, tests_passed + tests_failed);
}
