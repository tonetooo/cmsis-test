#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "Sd_spi.h"
#include "ff_gen_drv.h"

/***************************************************************
 * 🔧 USER CONFIGURATION - MODIFY THIS FOR YOUR BOARD
 ***************************************************************/
#define USE_DMA 0  // Set to 1 for DMA, 0 for polling

extern SPI_HandleTypeDef hspi1;
#define SD_SPI_HANDLE hspi1

// CS Pin - Change SD_CS to match your CubeMX label
#define SD_CS_LOW()     HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define SD_CS_HIGH()    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)

/***************************************************************
 * 🚫 DO NOT MODIFY BELOW THIS LINE
 ***************************************************************/

// FATFS Variables
FATFS fs;
FIL fil;
static char sd_path[4] = "0:/";
static uint8_t sd_card_type = SD_TYPE_UNKNOWN;

static const char* sd_card_type_str(uint8_t t) {
    switch (t) {
        case SD_TYPE_UNKNOWN: return "UNKNOWN";
        case SD_TYPE_V1:      return "SDv1";
        case SD_TYPE_V2:      return "SDv2 (non-HC)";
        case SD_TYPE_V2HC:    return "SDv2 HC/SDXC";
        case SD_TYPE_MMC:     return "MMC";
        default:              return "???";
    }
}

// SPI Transfer Functions
static uint8_t spi_send(uint8_t data)
{
    uint8_t rx_data;
#if USE_DMA
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &data, &rx_data, 1, 1000);
#else
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &data, &rx_data, 1, 1000);
#endif
    return rx_data;
}

// Wait for card ready
static uint8_t sd_wait_ready(void)
{
    uint8_t res;
    uint16_t retry = 0;
    do {
        res = spi_send(0xFF);
        retry++;
    } while (res != 0xFF && retry < 500);
    return (res == 0xFF) ? 0 : 1;
}

// SD Card Command Functions
static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t res, retry = 0;

    if (cmd != CMD0) {
        if (sd_wait_ready() != 0) return 0xFF;
    }

    // Send command packet
    spi_send(0x40 | cmd);
    spi_send((uint8_t)(arg >> 24));
    spi_send((uint8_t)(arg >> 16));
    spi_send((uint8_t)(arg >> 8));
    spi_send((uint8_t)arg);

    // CRC
    uint8_t crc = 0xFF;
    if (cmd == CMD0) crc = 0x95;
    else if (cmd == CMD8) crc = 0x87;
    spi_send(crc);

    // Wait for response
    retry = 0;
    do {
        res = spi_send(0xFF);
        retry++;
    } while ((res & 0x80) && retry < 255);

    return res;
}

// Read CSD register and return sector count
uint32_t sd_get_sector_count(void)
{
    uint8_t csd[16];
    SD_CS_LOW();
    uint8_t res = sd_send_cmd(CMD9, 0);
    if (res != 0x00) { SD_CS_HIGH(); spi_send(0xFF); return 0; }
    uint16_t retry = 0;
    do { res = spi_send(0xFF); retry++; } while (res != 0xFE && retry < 5000);
    if (res != 0xFE) { SD_CS_HIGH(); spi_send(0xFF); return 0; }
    for (int i = 0; i < 16; i++) csd[i] = spi_send(0xFF);
    spi_send(0xFF); spi_send(0xFF); // CRC
    SD_CS_HIGH(); spi_send(0xFF);
    uint8_t ver = (csd[0] >> 6) & 0x03;
    uint32_t sectors = 0;
    if (ver == 1) {
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
        sectors = (c_size + 1) * 1024;
    } else if (ver == 0) {
        uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) | ((uint32_t)csd[7] << 2) | ((csd[8] >> 6) & 0x03);
        uint32_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
        uint32_t read_bl_len = csd[5] & 0x0F;
        uint32_t block_count = (c_size + 1) * (uint32_t)(1 << (c_size_mult + 2));
        uint32_t block_len = (uint32_t)1 << read_bl_len;
        sectors = (block_count * block_len) / 512;
    }
    printf("[SD] CSD version=%d, sectors=%lu (~%lu MB)\r\n", ver, (unsigned long)sectors, (unsigned long)(sectors / 2048));
    return sectors;
}

// SD Card Initialization
uint8_t sd_init(void)
{
    uint8_t res, retry = 0;

    printf("[STORAGE] === SD INIT START ===\r\n");
    printf("[STORAGE] Step 1/7: Stabilizing SD card (150ms delay)...\r\n");
    HAL_Delay(150);

    // Slow down SPI for initialization (< 400kHz)
    printf("[STORAGE] Step 2/7: SPI clock = 351 kHz (prescaler 256)\r\n");
    SD_SPI_HANDLE.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    if (HAL_SPI_Init(&SD_SPI_HANDLE) != HAL_OK) {
        printf("[STORAGE] [FAIL] HAL_SPI_Init (slow) failed\r\n");
        return 1;
    }
    printf("[STORAGE] [OK] SPI re-initialized at 351 kHz\r\n");

    printf("[STORAGE] Recovery: 80 dummy clocks with CS low (clears interrupted transaction)...\r\n");
    SD_CS_LOW();
    for(int i=0; i<10; i++) {
        spi_send(0xFF);
    }
    SD_CS_HIGH();
    spi_send(0xFF);

    printf("[STORAGE] Sending 80 dummy clocks (CS high) to enter SPI mode...\r\n");
    for(int i=0; i<20; i++) {
        spi_send(0xFF);
    }

    // SPI register diagnostics
    printf("[STORAGE] [DIAG] SPI1->CR1 = 0x%08lX\r\n", SPI1->CR1);
    printf("[STORAGE] [DIAG] SPI1->CR2 = 0x%08lX\r\n", SPI1->CR2);
    printf("[STORAGE] [DIAG] SPI1->SR  = 0x%08lX\r\n", SPI1->SR);
    printf("[STORAGE] [DIAG] GPIOA->MODER = 0x%08lX\r\n", GPIOA->MODER);
    printf("[STORAGE] [DIAG] GPIOA->AFR[0] = 0x%08lX\r\n", GPIOA->AFR[0]);
    printf("[STORAGE] [DIAG] GPIOB->MODER = 0x%08lX\r\n", GPIOB->MODER);
    printf("[STORAGE] [DIAG] GPIOB->ODR = 0x%08lX\r\n", GPIOB->ODR);
    printf("[STORAGE] [DIAG] RCC->APB2ENR = 0x%08lX\r\n", RCC->APB2ENR);

    // Manual toggle test: assert CS, send 0xFF, read response, de-assert CS
    printf("[STORAGE] [DIAG] Manual CS toggle test:\r\n");
    SD_CS_HIGH(); spi_send(0xFF);
    printf("[STORAGE] [DIAG]   CS high -> spi_send(0xFF) = 0x%02X\r\n", spi_send(0xFF));
    SD_CS_LOW();
    printf("[STORAGE] [DIAG]   CS low  -> spi_send(0xFF) = 0x%02X\r\n", spi_send(0xFF));
    printf("[STORAGE] [DIAG]   CS low  -> spi_send(0xFF) = 0x%02X\r\n", spi_send(0xFF));
    SD_CS_HIGH();
    printf("[STORAGE] [DIAG]   CS high -> spi_send(0xFF) = 0x%02X\r\n", spi_send(0xFF));
    printf("[STORAGE] [DIAG] GPIOB->ODR after toggle = 0x%08lX\r\n", GPIOB->ODR);

    // CMD0: GO_IDLE_STATE
    printf("[STORAGE] Step 3/7: Sending CMD0 (GO_IDLE_STATE)...\r\n");
    retry = 0;
    do {
        SD_CS_HIGH();
        spi_send(0xFF);
        
        SD_CS_LOW();
        res = sd_send_cmd(CMD0, 0);
        SD_CS_HIGH();
        
        if (res == 0x01) {
            break;
        }
        
        spi_send(0xFF);
        HAL_Delay(10);
        retry++;
    } while (retry < 100);

    if (res != 0x01) {
        printf("[STORAGE] [RETRY] CMD0 failed after %d tries, retry sequence with 100 sync clocks (CS low)...\r\n", retry);
        SD_CS_LOW();
        for(int i=0; i<100; i++) spi_send(0xFF);
        SD_CS_HIGH();
        spi_send(0xFF);
        
        retry = 0;
        do {
            SD_CS_LOW();
            res = sd_send_cmd(CMD0, 0);
            SD_CS_HIGH();
            if (res == 0x01) break;
            HAL_Delay(50);
            retry++;
        } while (retry < 150);
    }

    if (res != 0x01) {
        printf("[STORAGE] [FAIL] CMD0 failed (Res=0x%02X after %d retries)\r\n", res, retry);
        SD_CS_HIGH();
        return 1;
    }

    printf("[STORAGE] [OK] CMD0 (GO_IDLE_STATE) => response 0x%02X after %d retries\r\n", res, retry);

    // CMD8: SEND_IF_COND
    printf("[STORAGE] Step 4/7: Sending CMD8 (SEND_IF_COND, 0x1AA)...\r\n");
    SD_CS_LOW();
    res = sd_send_cmd(CMD8, 0x1AA);
    if (res == 0x01) {
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = spi_send(0xFF);
        SD_CS_HIGH();
        spi_send(0xFF);

        printf("[STORAGE] [INFO] V2.0 card, OCR echo: %02X %02X %02X %02X\r\n", ocr[0], ocr[1], ocr[2], ocr[3]);

        if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
            printf("[STORAGE] [INFO] Voltage range 2.7-3.6V, check pattern 0xAA => Valid SDv2\r\n");
            printf("[STORAGE] Step 5/7: Sending ACMD41 with HCS bit (waiting up to 255 tries)...\r\n");
            retry = 0;
            do {
                SD_CS_LOW();
                sd_send_cmd(CMD55, 0);
                SD_CS_HIGH();
                spi_send(0xFF);

                SD_CS_LOW();
                res = sd_send_cmd(ACMD41, 0x40000000);
                SD_CS_HIGH();
                spi_send(0xFF);

                retry++;
                HAL_Delay(10);
            } while (res != 0x00 && retry < 255);

            printf("[STORAGE] [INFO] ACMD41 completed after %d tries (res=0x%02X)\r\n", retry, res);

            if (res == 0x00) {
                printf("[STORAGE] Step 6/7: Reading OCR (CMD58) to check CCS bit...\r\n");
                SD_CS_LOW();
                if (sd_send_cmd(CMD58, 0) == 0x00) {
                    for (int i = 0; i < 4; i++) ocr[i] = spi_send(0xFF);
                    sd_card_type = (ocr[0] & 0x40) ? SD_TYPE_V2HC : SD_TYPE_V2;
                    printf("[STORAGE] [INFO] OCR = %02X %02X %02X %02X, CCS bit = %d => %s\r\n",
                           ocr[0], ocr[1], ocr[2], ocr[3],
                           (ocr[0] & 0x40) ? 1 : 0,
                           sd_card_type_str(sd_card_type));
                }
                SD_CS_HIGH();
                spi_send(0xFF);
            }
        } else {
            printf("[STORAGE] [WARN] CMD8 pattern mismatch, retrying as V1 card\r\n");
            goto v1_init;
        }
    } else {
        SD_CS_HIGH();
        spi_send(0xFF);
        v1_init:
        printf("[STORAGE] Step 4b/7: V1.x/MMC Card path\r\n");
        retry = 0;
        do {
            SD_CS_LOW();
            sd_send_cmd(CMD55, 0);
            SD_CS_HIGH();
            spi_send(0xFF);

            SD_CS_LOW();
            res = sd_send_cmd(ACMD41, 0);
            SD_CS_HIGH();
            spi_send(0xFF);

            if (res == 0x00) {
                sd_card_type = SD_TYPE_V1;
                printf("[STORAGE] [OK] ACMD41 accepted => SDv1\r\n");
                break;
            }

            SD_CS_LOW();
            res = sd_send_cmd(CMD1, 0);
            SD_CS_HIGH();
            spi_send(0xFF);

            if (res == 0x00) {
                sd_card_type = SD_TYPE_MMC;
                printf("[STORAGE] [OK] CMD1 accepted => MMC\r\n");
                break;
            }
            retry++;
            HAL_Delay(10);
        } while (retry < 255);
    }

    if (sd_card_type == SD_TYPE_UNKNOWN) {
        printf("[STORAGE] [FAIL] Unknown card type (res=0x%02X after %d retries)\r\n", res, retry);
        SD_CS_HIGH();
        return 1;
    }

    // Set block size to 512 bytes (for non-SDHC cards)
    if (sd_card_type != SD_TYPE_V2HC) {
        printf("[STORAGE] [INFO] SDHC not detected, setting block size to 512 via CMD16\r\n");
        sd_send_cmd(CMD16, 512);
    }

    SD_CS_HIGH();
    spi_send(0xFF);

    printf("[STORAGE] Step 7/7: Speed up SPI for data transfer (11.25 MHz)...\r\n");
    SD_SPI_HANDLE.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; 
    if (HAL_SPI_Init(&SD_SPI_HANDLE) != HAL_OK) {
        printf("[STORAGE] [FAIL] Final SPI init at 11.25 MHz failed\r\n");
    } else {
        printf("[STORAGE] [OK] SPI running at 11.25 MHz\r\n");
    }
    printf("[STORAGE] === SD INIT DONE: card_type=%s ===\r\n", sd_card_type_str(sd_card_type));

    return 0;
}

// Read Single Block
uint8_t sd_read_block(uint8_t *buf, uint32_t sector)
{
    uint8_t res;
    uint16_t retry = 0;

    SD_CS_LOW();

    // For SDHC, sector = block number; for SD, sector = byte address
    if (sd_card_type != SD_TYPE_V2HC) {
        sector *= 512;
    }

    res = sd_send_cmd(CMD17, sector);
    if (res != 0x00) {
        SD_CS_HIGH();
        return 1;
    }

    // Wait for data token
    do {
        res = spi_send(0xFF);
        retry++;
    } while (res != 0xFE && retry < 5000);

    if (res != 0xFE) {
        SD_CS_HIGH();
        return 2;
    }

    // Read 512 bytes
#if USE_DMA
    HAL_SPI_Receive_DMA(&SD_SPI_HANDLE, buf, 512);
    while (HAL_SPI_GetState(&SD_SPI_HANDLE) != HAL_SPI_STATE_READY);
#else
    for (int i = 0; i < 512; i++) {
        buf[i] = spi_send(0xFF);
    }
#endif

    // Read CRC (2 bytes)
    spi_send(0xFF);
    spi_send(0xFF);

    SD_CS_HIGH();
    spi_send(0xFF);

    return 0;
}

// Write Single Block
uint8_t sd_write_block(const uint8_t *buf, uint32_t sector)
{
    uint8_t res;
    uint16_t retry = 0;

    SD_CS_LOW();

    if (sd_card_type != SD_TYPE_V2HC) {
        sector *= 512;
    }

    printf("[SD] WRITE sector=%lu ... ", (unsigned long)sector);
    res = sd_send_cmd(CMD24, sector);
    if (res != 0x00) {
        SD_CS_HIGH();
        printf("CMD24 failed (res=0x%02X)\r\n", res);
        return 1;
    }

    spi_send(0xFF);
    spi_send(0xFE); // Data token

    // Write 512 bytes
#if USE_DMA
    HAL_SPI_Transmit_DMA(&SD_SPI_HANDLE, (uint8_t*)buf, 512);
    while (HAL_SPI_GetState(&SD_SPI_HANDLE) != HAL_SPI_STATE_READY);
#else
    for (int i = 0; i < 512; i++) {
        spi_send(buf[i]);
    }
#endif

    // Dummy CRC
    spi_send(0xFF);
    spi_send(0xFF);

    // Wait for response
    res = spi_send(0xFF);
    if ((res & 0x1F) != 0x05) {
        SD_CS_HIGH();
        printf("data response err (res=0x%02X)\r\n", res);
        return 2;
    }

    // Wait for card to finish (up to 500ms)
    retry = 0;
    do {
        res = spi_send(0xFF);
        retry++;
    } while (res == 0x00 && retry < 100000);

    SD_CS_HIGH();
    spi_send(0xFF);

    printf("OK (busy=%u)\r\n", retry);
    return 0;
}

// Multiple block read/write
uint8_t sd_read_blocks(uint8_t *buf, uint32_t sector, uint32_t count)
{
    uint8_t res;
    if (count == 1) return sd_read_block(buf, sector);

    SD_CS_LOW();
    if (sd_card_type != SD_TYPE_V2HC) sector *= 512;

    res = sd_send_cmd(CMD18, sector); // READ_MULTIPLE_BLOCK
    if (res != 0x00) {
        SD_CS_HIGH();
        return 1;
    }

    do {
        uint16_t retry = 0;
        while (spi_send(0xFF) != 0xFE && retry < 5000) retry++;
        if (retry >= 5000) break;

        for (int i = 0; i < 512; i++) *buf++ = spi_send(0xFF);
        spi_send(0xFF); // CRC
        spi_send(0xFF);
    } while (--count);

    sd_send_cmd(CMD12, 0); // STOP_TRANSMISSION
    SD_CS_HIGH();
    spi_send(0xFF);

    return (count == 0) ? 0 : 1;
}

uint8_t sd_write_blocks(const uint8_t *buf, uint32_t sector, uint32_t count)
{
    uint8_t res;
    if (count == 0) return 0;
    // NOTE: count==1 is handled via CMD25 multi-block write below.
    // CMD24 (single-block) is avoided because some cards in SPI mode
    // accept CMD24 but don't persist the data (observed on this HW).

    SD_CS_LOW();
    if (sd_card_type != SD_TYPE_V2HC) sector *= 512;

    printf("[SD] WRITE-MULTI sector=%lu count=%lu ... ", (unsigned long)sector, (unsigned long)count);
    res = sd_send_cmd(CMD25, sector); // WRITE_MULTIPLE_BLOCK
    if (res != 0x00) {
        SD_CS_HIGH();
        printf("CMD25 failed (res=0x%02X)\r\n", res);
        return 1;
    }

    do {
        spi_send(0xFF);
        spi_send(0xFC); // Multi-block data token
        for (int i = 0; i < 512; i++) spi_send(*buf++);
        spi_send(0xFF); // CRC
        spi_send(0xFF);
        
        res = spi_send(0xFF);
        if ((res & 0x1F) != 0x05) { printf("data err at block (res=0x%02X)\r\n", res); break; }
        
        uint32_t retry = 0;
        while (spi_send(0xFF) == 0x00 && retry < 100000) retry++;
        if (retry >= 100000) { printf("busy timeout\r\n"); break; }
    } while (--count);

    spi_send(0xFD); // Stop token
    uint32_t retry_wip = 0;
    while (spi_send(0xFF) == 0x00 && retry_wip < 100000) retry_wip++;

    SD_CS_HIGH();
    spi_send(0xFF);

    uint8_t ok = (count == 0) && (retry_wip < 100000);
    printf("done (remaining=%lu, stop_busy=%lu)%s\r\n",
           (unsigned long)count, (unsigned long)retry_wip,
           ok ? "" : " WARN");
    return ok ? 0 : 1;
}

/***************************************************************
 * FORMAT VERIFICATION HELPERS
 ***************************************************************/

// Quick check: read sector 0 and validate FAT32 BPB signature
// Returns 1 if sector looks like valid FAT32, 0 otherwise
static int sd_check_sector0_fat32(void)
{
    uint8_t buf[512];
    if (sd_read_block(buf, 0) != 0) {
        printf("[STORAGE] [WARN] Cannot read sector 0 for verification\r\n");
        return 0;
    }

    // Check 0x55AA signature
    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        printf("[STORAGE] [VERIFY] Sector 0: no 0x55AA signature\r\n");
        return 0;
    }

    // Check JmpBoot (should be 0xEB or 0xE9 for valid FAT boot sector)
    if (buf[0] != 0xEB && buf[0] != 0xE9) {
        printf("[STORAGE] [VERIFY] Sector 0: invalid JmpBoot (0x%02X)\r\n", buf[0]);
        return 0;
    }

    // Check BytesPerSector (must be 512 = 0x0200 little-endian)
    uint16_t bps = buf[11] | ((uint16_t)buf[12] << 8);
    if (bps != 512) {
        printf("[STORAGE] [VERIFY] Sector 0: bad BytesPerSector (%u)\r\n", bps);
        return 0;
    }

    // Check FATSz32 (must be > 0 for FAT32)
    uint32_t fatsz32 = (uint32_t)buf[36] | ((uint32_t)buf[37] << 8)
                     | ((uint32_t)buf[38] << 16) | ((uint32_t)buf[39] << 24);
    if (fatsz32 == 0) {
        printf("[STORAGE] [VERIFY] Sector 0: FATSz32=0 (not FAT32?)\r\n");
        return 0;
    }

    printf("[STORAGE] [VERIFY] Sector 0: valid FAT32 (FATSz32=%lu, BPS=%u)\r\n",
           (unsigned long)fatsz32, bps);
    return 1;
}

/***************************************************************
 * HIGH-LEVEL FILE OPERATIONS
 ***************************************************************/

// Mount SD Card
int sd_mount(void)
{
    FRESULT res;

    printf("[STORAGE] === MOUNT START ===\r\n");
    printf("[STORAGE] Phase 1/4: SD hardware init...\r\n");
    if (sd_init() != 0) {
        printf("[STORAGE] [FAIL] SD hardware init failed\r\n");
        return -1;
    }
    printf("[STORAGE] [OK] SD hardware init complete\r\n");

    printf("[STORAGE] Phase 2/4: Linking FatFs disk driver...\r\n");
    extern const Diskio_drvTypeDef USER_Driver;
    if (FATFS_LinkDriver(&USER_Driver, sd_path) != 0)
    {
        printf("[STORAGE] [FAIL] FATFS_LinkDriver failed\r\n");
        return -1;
    }
    printf("[STORAGE] [OK] Driver linked at path '%s'\r\n", sd_path);

    printf("[STORAGE] Phase 3/4: Mounting filesystem (f_mount)...\r\n");
    res = f_mount(&fs, sd_path, 1);

    if (res != FR_OK) {
        printf("[STORAGE] [WARN] f_mount returned FRESULT=%d\r\n", res);
        printf("[STORAGE]        => %s\r\n",
               res == FR_NO_FILESYSTEM ? "NO FILESYSTEM (needs format)" :
               res == FR_NOT_READY     ? "NOT READY (card removed?)" :
               res == FR_DISK_ERR      ? "DISK ERROR (hardware issue)" :
               res == FR_INT_ERR       ? "INTERNAL ERROR" :
               res == FR_INVALID_DRIVE ? "INVALID DRIVE" :
                                          "UNKNOWN ERROR");

        // --- Pre-format check: read sector 0 to see if BPB exists ---
        printf("[STORAGE] Phase 3a: Checking sector 0 before format...\r\n");
        if (sd_check_sector0_fat32()) {
            printf("[STORAGE] [WARN] Sector 0 has valid FAT32 BPB, but f_mount failed.\r\n");
            printf("[STORAGE] [WARN] Skipping format (card may have corrupted FAT).\r\n");
            // Don't format, return the original error
        } else {
            printf("[STORAGE] Phase 3b: Direct write/read test on sector 0...\r\n");
            {
                uint8_t test_w[512], test_r[512];
                memset(test_w, 0xA5, sizeof(test_w));
                test_w[0] = 'H'; test_w[1] = 'E'; test_w[2] = 'R'; test_w[3] = 'M';
                test_w[4] = 'E'; test_w[5] = 'S'; test_w[510] = 0x55; test_w[511] = 0xAA;
                printf("[STORAGE] [TEST] Writing test pattern to sector 0 (CMD25, count=1)...\r\n");
                uint8_t wr = sd_write_blocks((const uint8_t*)test_w, 0, 1);
                printf("[STORAGE] [TEST] sd_write_blocks returned %d\r\n", wr);
                printf("[STORAGE] [TEST] Reading sector 0 back...\r\n");
                uint8_t rr = sd_read_block(test_r, 0);
                printf("[STORAGE] [TEST] sd_read_block returned %d\r\n", rr);
                int match = (memcmp(test_w, test_r, 512) == 0);
                int sig   = (test_r[510] == 0x55 && test_r[511] == 0xAA);
                printf("[STORAGE] [TEST] Data match=%d, 0x55AA=%d, test_r[0..5]=%c%c%c%c%c%c\r\n",
                       match, sig, test_r[0], test_r[1], test_r[2],
                       test_r[3], test_r[4], test_r[5]);
            }

            printf("[STORAGE] Phase 3c: Attempting auto-format (f_mkfs)...\r\n");
            printf("[STORAGE] [INFO] Work buffer = 4096 bytes (stack-allocated)\r\n");

            f_mount(NULL, sd_path, 0); // Unmount before format

            BYTE work[4096];
            printf("[STORAGE] [INFO] Trying FM_FAT32 | FM_SFD (superfloppy)...\r\n");
            res = f_mkfs(sd_path, FM_FAT32 | FM_SFD, 0, work, sizeof(work));

            if (res == FR_OK) {
                printf("[STORAGE] [OK] Format returned FR_OK\r\n");

                // --- Post-format verification: read back sector 0 ---
                printf("[STORAGE] [INFO] Verifying format: reading sector 0 back...\r\n");
                if (sd_check_sector0_fat32()) {
                    printf("[STORAGE] [OK] Format write verified!\r\n");
                } else {
                    printf("[STORAGE] [FAIL] Sector 0 verify FAILED after format\r\n");
                    // Keep res=FR_OK to let remount attempt report its own error
                }

                printf("[STORAGE] [INFO] Remounting...\r\n");
                res = f_mount(&fs, sd_path, 1);
                if (res != FR_OK) {
                    printf("[STORAGE] [FAIL] Remount after format failed: FRESULT=%d\r\n", res);
                }
            } else {
                printf("[STORAGE] [FAIL] Format returned FRESULT=%d\r\n", res);
                printf("[STORAGE]        => %s\r\n",
                       res == FR_NOT_READY     ? "NOT READY" :
                       res == FR_DISK_ERR      ? "DISK ERROR" :
                       res == FR_MKFS_ABORTED  ? "MKFS ABORTED (wrong param)" :
                       res == FR_INVALID_DRIVE ? "INVALID DRIVE" :
                                                 "UNKNOWN ERROR");
            }
        }
    }

    if (res != FR_OK) {
        printf("[STORAGE] [FAIL] Mount failed after all attempts (FRESULT=%d)\r\n", res);
        FATFS_UnLinkDriver(sd_path);
        return -1;
    }

    printf("[STORAGE] [OK] Filesystem mounted successfully\r\n");

    // Show volume info
    FATFS* ptfs;
    DWORD fre_clust, fre_sect, tot_sect;
    if (f_getfree(sd_path, &fre_clust, &ptfs) == FR_OK) {
        tot_sect = (ptfs->n_fatent - 2) * ptfs->csize;
        fre_sect = fre_clust * ptfs->csize;
        printf("[STORAGE] [INFO] Total: ~%lu MB, Free: ~%lu MB\r\n",
               (unsigned long)(tot_sect / 2048),
               (unsigned long)(fre_sect / 2048));
    }
    printf("[STORAGE] === MOUNT DONE ===\r\n");
    return 0;
}

// Unmount SD Card
void sd_unmount(void)
{
    f_mount(NULL, sd_path, 0);
    printf("[STORAGE] SD Card Unmounted\r\n");
}

// Write/Create File
int sd_write_file(const char *filename, const char *data)
{
    UINT bw;
    printf("[STORAGE] === FILE WRITE: \"%s\" ===\r\n", filename);
    printf("[STORAGE] [INFO] Data size = %zu bytes\r\n", strlen(data));

    FRESULT res = f_open(&fil, filename, FA_CREATE_ALWAYS | FA_WRITE);

    if (res != FR_OK) {
        printf("[STORAGE] [WARN] f_open('%s') = %d, trying full path \"%swater_log.csv\"...\r\n", filename, res, sd_path);
        char fullpath[50];
        sprintf(fullpath, "%swater_log.csv", sd_path);
        res = f_open(&fil, fullpath, FA_CREATE_ALWAYS | FA_WRITE);
        printf("[STORAGE] [INFO] f_open full path => %d\r\n", res);
        if (res != FR_OK) {
            printf("[STORAGE] [FAIL] Could not open any path for writing\r\n");
            return -1;
        }
    }

    res = f_write(&fil, data, strlen(data), &bw);
    printf("[STORAGE] [INFO] f_write => %d, bytes_written=%u\r\n", res, bw);

    f_close(&fil);
    printf("[STORAGE] [INFO] f_close done\r\n");

    if (res == FR_OK) {
        printf("[STORAGE] [OK] Wrote %u bytes to '%s'\r\n", bw, filename);
        return 0;
    }
    printf("[STORAGE] [FAIL] Write failed\r\n");
    return -1;
}

// Read File
int sd_read_file(const char *filename, char *buffer, uint32_t size, UINT *bytes_read)
{
    FRESULT res = f_open(&fil, filename, FA_READ);
    if (res != FR_OK) {
        printf("[STORAGE] Read Failed: %d\r\n", res);
        return -1;
    }

    res = f_read(&fil, buffer, size - 1, bytes_read);
    buffer[*bytes_read] = '\0';
    f_close(&fil);

    return (res == FR_OK) ? 0 : -1;
}

// Append to File
int sd_append_file(const char *filename, const char *data)
{
    printf("[STORAGE] === FILE APPEND: \"%s\" (%zu bytes) ===\r\n", filename, strlen(data));
    FRESULT res = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (res != FR_OK) {
        printf("[STORAGE] [FAIL] f_open(APPEND) = %d\r\n", res);
        return -1;
    }

    UINT bw;
    res = f_write(&fil, data, strlen(data), &bw);
    printf("[STORAGE] [INFO] f_write => %d, bytes_written=%u\r\n", res, bw);
    f_close(&fil);

    if (res == FR_OK) {
        printf("[STORAGE] [OK] Appended %u bytes\r\n", bw);
        return 0;
    }
    printf("[STORAGE] [FAIL] Append write failed\r\n");
    return -1;
}

// List Files
void sd_list_files(void)
{
    DIR dir;
    FILINFO fno;
    uint32_t total = 0;

    printf("[STORAGE] === LISTING FILES ===\r\n");
    if (f_opendir(&dir, "/") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
            printf("  %-20s %10lu bytes\r\n", fno.fname, (unsigned long)fno.fsize);
            total++;
        }
        if (total == 0) {
            printf("  (empty directory)\r\n");
        } else {
            printf("  --------------------------\r\n");
            printf("  TOTAL: %lu files\r\n", (unsigned long)total);
        }
        f_closedir(&dir);
    } else {
        printf("[STORAGE] [FAIL] f_opendir failed\r\n");
    }
    printf("[STORAGE] === LIST END ===\r\n");
}
