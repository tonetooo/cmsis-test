#include "adxl355.h"
#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>
#include "console.h"

// Global handle for SPI (must be defined in main.c)
static SPI_HandleTypeDef *adxl_hspi;
static ADXL355_Range_t current_range = ADXL355_RANGE_2G;
static float current_sensitivity = 256000.0f; // Default for 2g (approx)
static float adxl_offset_x_g = 0.0f;
static float adxl_offset_y_g = 0.0f;
static float adxl_offset_z_g = 0.0f;
static uint8_t adxl_offsets_valid = 0;
static volatile uint32_t adxl_spi_error_count = 0;

// Retry wrapper for HAL_SPI_Transmit (3 attempts, 1ms delay between)
static HAL_StatusTypeDef spi_tx(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t size, uint32_t timeout) {
    HAL_StatusTypeDef s;
    for (int r = 0; r < 3; r++) {
        s = HAL_SPI_Transmit(hspi, data, size, timeout);
        if (s == HAL_OK) return HAL_OK;
        osDelay(1);
    }
    adxl_spi_error_count++;
    CONS_ERR("[SPI ERR] TX fail (size=%u, status=%d, total=%lu)",
             size, (int)s, (unsigned long)adxl_spi_error_count);
    return s;
}

// Retry wrapper for HAL_SPI_Receive (3 attempts, 1ms delay between)
static HAL_StatusTypeDef spi_rx(SPI_HandleTypeDef *hspi, uint8_t *data, uint16_t size, uint32_t timeout) {
    HAL_StatusTypeDef s;
    for (int r = 0; r < 3; r++) {
        s = HAL_SPI_Receive(hspi, data, size, timeout);
        if (s == HAL_OK) return HAL_OK;
        osDelay(1);
    }
    adxl_spi_error_count++;
    CONS_ERR("[SPI ERR] RX fail (size=%u, status=%d, total=%lu)",
             size, (int)s, (unsigned long)adxl_spi_error_count);
    return s;
}

uint32_t ADXL355_Get_SPI_Error_Count(void) {
    return adxl_spi_error_count;
}

static void ADXL355_Read_Data_Internal(ADXL355_Data_t *data, uint8_t apply_offsets);

// Internal helpers
void ADXL355_Write_Reg(uint8_t reg, uint8_t value) {
    uint8_t data[2];
    data[0] = (reg << 1) & 0xFE; // Write bit = 0
    data[1] = value;
    
    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_RESET);
    spi_tx(adxl_hspi, data, 2, 100);
    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
}

uint8_t ADXL355_Read_Reg(uint8_t reg) {
    uint8_t tx_data = (reg << 1) | 0x01; // Read bit = 1
    uint8_t rx_data = 0;
    
    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_RESET);
    if (spi_tx(adxl_hspi, &tx_data, 1, 100) != HAL_OK) {
        HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
        return 0xFF;
    }
    if (spi_rx(adxl_hspi, &rx_data, 1, 100) != HAL_OK) {
        HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
        return 0xFF;
    }
    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
    
    return rx_data;
}

uint8_t ADXL355_Read_Status(void) {
    return ADXL355_Read_Reg(ADXL355_STATUS);
}

uint8_t ADXL355_Init(SPI_HandleTypeDef *hspi) {
    adxl_hspi = hspi;
    // 1. Reset Sensor
    ADXL355_Write_Reg(ADXL355_RESET, 0x52);
    HAL_Delay(100);
    
    // 2. Check Device IDs
    uint8_t devid_ad = 0;
    uint8_t devid_mst = 0;
    uint8_t partid = 0;

    for(int i=0; i<5; i++) {
        devid_ad = ADXL355_Read_Reg(ADXL355_DEVID_AD);
        devid_mst = ADXL355_Read_Reg(ADXL355_DEVID_MST);
        partid = ADXL355_Read_Reg(ADXL355_PARTID);
        
        // Check if we have the correct IDs
        // ADXL355: AD=0xAD, MST=0x1D, PARTID=0xED
        if(devid_ad == 0xAD && devid_mst == 0x1D && partid == 0xED) break;
        
        HAL_Delay(10);
    }

    if (devid_ad != 0xAD || devid_mst != 0x1D || partid != 0xED) {
        CONS_ERR("Error: IDs mismatch. AD:0x%02X(Exp:0xAD), MST:0x%02X(Exp:0x1D), PART:0x%02X(Exp:0xED)", devid_ad, devid_mst, partid);
        return 0; // Error
    }
    
    // 3. Configure Filter (Default ODR = 125Hz -> 0x15)
    // 0x15 = 0b00010101: bits 6:4=000 HPF OFF, bit 3=0 ACT_HPF OFF, bits 2:0=101 ODR=125Hz
    // Activity detection uses X+Y only (ACT_EN=0x03) to avoid 1g gravity false triggers on Z
    ADXL355_Write_Reg(ADXL355_FILTER, 0x15);
    
    // 4. Power Control (Measurement Mode)
    // Standby = 0 (Bit 0) -> Measurement Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x00);
    
    // 5. Set default range to 2g explicitly
    // (RANGE register default after reset has bits 1:0 = 00 = ±2g)
    // Bit 6 is reserved (must be 0) per datasheet.
    ADXL355_Set_Range(ADXL355_RANGE_2G);

    return 1; // Success
}

void ADXL355_Set_Range(ADXL355_Range_t range) {
    // 1. Enter Standby Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x01);

    // 2. Configure Range (ADXL355 datasheet)
    // Bits 1:0 determine range.
    // 00 = +/- 2g
    // 01 = +/- 4g
    // 10 = +/- 8g
    uint8_t range_reg = ADXL355_Read_Reg(ADXL355_RANGE);
    range_reg &= 0xFC; // Clear bits 1:0
    range_reg |= (range & 0x03);
    ADXL355_Write_Reg(ADXL355_RANGE, range_reg);

    // 3. Update internal sensitivity
    current_range = range;
    switch(range) {
        case ADXL355_RANGE_2G:
            current_sensitivity = 256000.0f; // ~3.9ug/LSB
            break;
        case ADXL355_RANGE_4G:
            current_sensitivity = 128000.0f; // ~7.8ug/LSB
            break;
        case ADXL355_RANGE_8G:
            current_sensitivity = 64000.0f;  // ~15.6ug/LSB
            break;
        default:
            current_sensitivity = 256000.0f;
            break;
    }

    // 4. Return to Measurement Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x00);
}

float ADXL355_Get_Full_Scale(void) {
    switch(current_range) {
        case ADXL355_RANGE_2G: return 2.0f;
        case ADXL355_RANGE_4G: return 4.0f;
        case ADXL355_RANGE_8G: return 8.0f;
        default: return 2.0f;
    }
}

void ADXL355_Set_ODR(ADXL355_ODR_t odr) {
    // 1. Enter Standby Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x01);

    // 2. Configure ODR
    // Read current filter register to preserve High Pass settings if any (assuming bits 7:4 are 0 for now)
    // Only updating ODR (bits 3:0)
    uint8_t filter_reg = ADXL355_Read_Reg(ADXL355_FILTER);
    filter_reg &= 0xF0; // Clear lower 4 bits
    filter_reg |= (odr & 0x0F);
    ADXL355_Write_Reg(ADXL355_FILTER, filter_reg);

    // 3. Return to Measurement Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x00);
}

void ADXL355_Set_HPF(uint8_t enable) {
    // 1. Enter Standby Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x01);
    
    // 2. Configure HPF
    // Bits 6:4 determine corner frequency.
    // 000 = OFF
    // 001 = 24.7e-4 * ODR (Corner freq)
    uint8_t filter_reg = ADXL355_Read_Reg(ADXL355_FILTER);
    filter_reg &= 0x8F; // Clear bits 6:4 (HPF)
    
    if (enable) {
        // Set to 001 (Basic HPF)
        filter_reg |= (0x01 << 4);
    }
    
    ADXL355_Write_Reg(ADXL355_FILTER, filter_reg);
    
    // 3. Return to Measurement Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x00);
}

void ADXL355_Config_Activity_Int(uint16_t threshold, uint8_t count) {
    // 1. Enter Standby Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x01);

    // 2. Set Activity Threshold
    ADXL355_Write_Reg(ADXL355_ACT_THRESH_H, (uint8_t)(threshold >> 8));
    ADXL355_Write_Reg(ADXL355_ACT_THRESH_L, (uint8_t)(threshold & 0xFF));
    
    // 3. Set Activity Count
    ADXL355_Write_Reg(ADXL355_ACT_COUNT, count);
    
    // 4. Map to INT1 (Clear bit 4 of INT_MAP -> Activity)
    uint8_t int_map = ADXL355_Read_Reg(ADXL355_INT_MAP);
    int_map &= ~0x10; 
    ADXL355_Write_Reg(ADXL355_INT_MAP, int_map);
    
    // 5. Enable Activity Detection
    ADXL355_Write_Reg(ADXL355_ACT_EN, 0x01);

    // 6. Return to Measurement Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x00);
}

void ADXL355_Config_WakeOnMotion(float threshold_g, uint8_t count) {
    // 1. Enter Standby Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x01);

    // ADXL355 Activity threshold registers (0x25, 0x26) are compared to the 16 MSBs of the acceleration data.
    // For 2g range, sensitivity is 256,000 LSB/g (20-bit). 
    // The 16 MSBs comparison means 1g = 256,000 / 16 = 16,000.
    uint16_t threshold_val = (uint16_t)(threshold_g * (current_sensitivity / 16.0f)); 
    
    // 2. Set Activity Threshold
    ADXL355_Write_Reg(ADXL355_ACT_THRESH_H, (uint8_t)(threshold_val >> 8));
    ADXL355_Write_Reg(ADXL355_ACT_THRESH_L, (uint8_t)(threshold_val & 0xFF));
    
    // 3. Set Activity Count (Number of consecutive samples above threshold)
    ADXL355_Write_Reg(ADXL355_ACT_COUNT, count);
    
    // 4. Enable Axes (X, Y) in ACT_EN (0x24)
    // HPF is OFF so Z with 1g gravity would trigger continuously.
    // X+Y only is safe: no DC gravity, any motion above threshold triggers.
    // Software polling in sensor_task catches Z-motion as fallback.
    ADXL355_Write_Reg(ADXL355_ACT_EN, 0x03); 
    
    // 5. Map Activity Interrupt to INT1
    // Register 0x2A (INT_MAP): Bit 3 is ACT_INT1 (0 = route to INT1, 1 = route to INT2)
    uint8_t int_map = ADXL355_Read_Reg(ADXL355_INT_MAP);
    int_map &= ~0x08; // Clear bit 3 to route activity to INT1
    ADXL355_Write_Reg(ADXL355_INT_MAP, int_map);

    // 7. Return to Measurement Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x00);
    
    // 8. Clear any pending interrupts by reading STATUS
    // Esto asegura que la linea INT1 baje antes de empezar a esperar
    HAL_Delay(10);
    ADXL355_Read_Reg(ADXL355_STATUS);
}

void ADXL355_LevelToZero(void) {
    CONS_INFO("[CAL] Iniciando level-to-zero, mantenga el sensor quieto...");
    HAL_Delay(200);
    const int samples = 512;
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;
    ADXL355_Data_t d;
    for (int i = 0; i < samples; i++) {
        ADXL355_Read_Data_Internal(&d, 0);
        sum_x += d.x_g;
        sum_y += d.y_g;
        sum_z += d.z_g;
        HAL_Delay(2);
    }
    float avg_x = sum_x / samples;
    float avg_y = sum_y / samples;
    float avg_z = sum_z / samples;
    adxl_offset_x_g = avg_x;
    adxl_offset_y_g = avg_y;
    adxl_offset_z_g = avg_z;
    adxl_offsets_valid = 1;
    CONS_INFO("[CAL] Promedio en reposo (g): X=%.4f, Y=%.4f, Z=%.4f", avg_x, avg_y, avg_z);
    CONS_INFO("[CAL] Offsets aplicados (g): X=%.4f, Y=%.4f, Z=%.4f", adxl_offset_x_g, adxl_offset_y_g, adxl_offset_z_g);
    CONS_OK("[CAL] Level-to-zero completado; lecturas futuras se corrigen con estos offsets.");
}

static void ADXL355_Read_Data_Internal(ADXL355_Data_t *data, uint8_t apply_offsets) {
    uint8_t tx_data = (ADXL355_XDATA3 << 1) | 0x01;
    uint8_t raw_data[9] = {0};
    
    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_RESET);
    if (spi_tx(adxl_hspi, &tx_data, 1, 100) != HAL_OK) {
        HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
        memset(data, 0, sizeof(*data));
        data->timestamp = HAL_GetTick();
        return;
    }
    if (spi_rx(adxl_hspi, raw_data, 9, 100) != HAL_OK) {
        HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
        memset(data, 0, sizeof(*data));
        data->timestamp = HAL_GetTick();
        return;
    }
    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
    
    int32_t x = ((int32_t)raw_data[0] << 12) | ((int32_t)raw_data[1] << 4) | ((int32_t)raw_data[2] >> 4);
    int32_t y = ((int32_t)raw_data[3] << 12) | ((int32_t)raw_data[4] << 4) | ((int32_t)raw_data[5] >> 4);
    int32_t z = ((int32_t)raw_data[6] << 12) | ((int32_t)raw_data[7] << 4) | ((int32_t)raw_data[8] >> 4);
    
    if (x & 0x80000) x |= 0xFFF00000;
    if (y & 0x80000) y |= 0xFFF00000;
    if (z & 0x80000) z |= 0xFFF00000;
    
    data->x = x;
    data->y = y;
    data->z = z;
    
    data->x_g = (float)x / current_sensitivity;
    data->y_g = (float)y / current_sensitivity;
    data->z_g = (float)z / current_sensitivity;

    if (apply_offsets && adxl_offsets_valid) {
        data->x_g -= adxl_offset_x_g;
        data->y_g -= adxl_offset_y_g;
        data->z_g -= adxl_offset_z_g;
    }
    
    data->timestamp = HAL_GetTick();
}

void ADXL355_Read_Data(ADXL355_Data_t *data) {
    ADXL355_Read_Data_Internal(data, 1);
}

uint8_t ADXL355_Get_FIFO_Entries(void) {
    return ADXL355_Read_Reg(ADXL355_FIFO_ENTRIES);
}

void ADXL355_Config_FIFO(uint8_t samples) {
    // Must be in Standby
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x01);

    // Configure FIFO in Stream Mode (0x01)
    // Stream Mode: FIFO collects data; when full, oldest data is overwritten.
    // Store X,Y,Z data (implicit default if not specified otherwise, but we just set mode)
    
    // Set FIFO Samples (e.g., 96 samples = 0x60)
    ADXL355_Write_Reg(ADXL355_FIFO_SAMPLES, samples); 
    
    // Restore Measurement Mode
    ADXL355_Write_Reg(ADXL355_POWER_CTL, 0x00);
}

void ADXL355_Read_FIFO_Data(ADXL355_Data_t *data) {
    uint8_t tx_data = (ADXL355_FIFO_DATA << 1) | 0x01;
    uint8_t raw_data[9] = {0};
    
    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_RESET);
    if (spi_tx(adxl_hspi, &tx_data, 1, 100) != HAL_OK) {
        HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
        memset(data, 0, sizeof(*data));
        return;
    }
    if (spi_rx(adxl_hspi, raw_data, 9, 100) != HAL_OK) {
        HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
        memset(data, 0, sizeof(*data));
        return;
    }
    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
    
    // Check marker bits (Bit 0 of byte 2, 5, 8 indicates empty/special)
    // For now assume valid X,Y,Z data
    
    int32_t x = ((int32_t)raw_data[0] << 12) | ((int32_t)raw_data[1] << 4) | ((int32_t)raw_data[2] >> 4);
    int32_t y = ((int32_t)raw_data[3] << 12) | ((int32_t)raw_data[4] << 4) | ((int32_t)raw_data[5] >> 4);
    int32_t z = ((int32_t)raw_data[6] << 12) | ((int32_t)raw_data[7] << 4) | ((int32_t)raw_data[8] >> 4);
    
    if (x & 0x80000) x |= 0xFFF00000;
    if (y & 0x80000) y |= 0xFFF00000;
    if (z & 0x80000) z |= 0xFFF00000;
    
    data->x = x;
    data->y = y;
    data->z = z;
}

void ADXL355_Read_FIFO(ADXL355_Data_t *buffer, uint8_t count) {
    // This implementation is slow (toggle CS every sample), but safe.
    // For burst read, we should keep CS low, but let's stick to simple first.
    // Actually, reading FIFO_DATA multiple times in one transaction is better.
    // But let's reuse the single sample read for simplicity if performance allows.
    // However, to read FIFO correctly, we just read from 0x11 repeatedly.
    
    for(int i=0; i<count; i++) {
        ADXL355_Read_FIFO_Data(&buffer[i]);
    }
}

int ADXL355_Read_FIFO_Burst(SPI_HandleTypeDef *hspi, ADXL355_Data_t *buffer, uint8_t count) {
    for(int i=0; i<count; i++) {
        ADXL355_Read_Data(&buffer[i]);
    }
    return count;
}

// =====================================================================
// DMA implementation for SPI2 (ADI-ADXL355)
// STM32F446RE: SPI2_TX = DMA1 Stream 4 Ch 0, SPI2_RX = DMA1 Stream 3 Ch 0
// =====================================================================
static DMA_HandleTypeDef hdma_spi2_tx;
static DMA_HandleTypeDef hdma_spi2_rx;
static osSemaphoreId_t dma_spi2_sem;

static void adxl355_DMA_Init(void) {
    static uint8_t initialized = 0;
    if (initialized) return;
    initialized = 1;

    __HAL_RCC_DMA1_CLK_ENABLE();

    // TX: DMA1 Stream 4, Channel 0
    hdma_spi2_tx.Instance = DMA1_Stream4;
    hdma_spi2_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi2_tx.Init.Mode = DMA_NORMAL;
    hdma_spi2_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_spi2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi2_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(adxl_hspi, hdmatx, hdma_spi2_tx);

    // RX: DMA1 Stream 3, Channel 0
    hdma_spi2_rx.Instance = DMA1_Stream3;
    hdma_spi2_rx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_spi2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi2_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi2_rx.Init.Mode = DMA_NORMAL;
    hdma_spi2_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_spi2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi2_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(adxl_hspi, hdmarx, hdma_spi2_rx);

    // Enable DMA interrupts (priority 5 = low, safe for FreeRTOS)
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

    // Binary semaphore for task→ISR sync
    static const osSemaphoreAttr_t sem_attr = { .name = "dma_spi2" };
    dma_spi2_sem = osSemaphoreNew(1, 0, &sem_attr);
    if (dma_spi2_sem == NULL) {
        /* Heap exhausted — allow retry on next call */
        initialized = 0;
    }
}

// DMA ISRs
void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_spi2_rx);
}
void DMA1_Stream4_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_spi2_tx);
}

// SPI full-duplex transfer complete callback (called from DMA ISR chain)
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI2) {
        /* Guard: semaphore might not be initialized yet */
        if (dma_spi2_sem != NULL) {
            osSemaphoreRelease(dma_spi2_sem);
        }
    }
}

uint8_t ADXL355_Read_Data_DMA(ADXL355_Data_t *data) {
    adxl355_DMA_Init();

    uint8_t tx_buf[10] = { (ADXL355_XDATA3 << 1) | 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t rx_buf[10] = {0};

    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef stat = HAL_SPI_TransmitReceive_DMA(adxl_hspi, tx_buf, rx_buf, 10);
    if (stat != HAL_OK) {
        HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
        return 0;
    }

    // Wait for DMA completion (binary semaphore released by HAL_SPI_TxRxCpltCallback)
    // 10ms timeout = ~2000 bytes @ 21MHz, plenty for 10 bytes
    if (dma_spi2_sem == NULL || osSemaphoreAcquire(dma_spi2_sem, 10) != osOK) {
        HAL_SPI_DMAStop(adxl_hspi);
        HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);
        return 0;
    }

    HAL_GPIO_WritePin(ADXL_CS_GPIO_Port, ADXL_CS_Pin, GPIO_PIN_SET);

    // Parse: rx_buf[0] is garbage (received during command byte), [1..9] = XDATA3..ZDATA1
    int32_t x = ((int32_t)rx_buf[1] << 12) | ((int32_t)rx_buf[2] << 4) | ((int32_t)rx_buf[3] >> 4);
    int32_t y = ((int32_t)rx_buf[4] << 12) | ((int32_t)rx_buf[5] << 4) | ((int32_t)rx_buf[6] >> 4);
    int32_t z = ((int32_t)rx_buf[7] << 12) | ((int32_t)rx_buf[8] << 4) | ((int32_t)rx_buf[9] >> 4);

    /* Sign-extend 20-bit two's complement */
    if (x & 0x80000) x |= 0xFFF00000;
    if (y & 0x80000) y |= 0xFFF00000;
    if (z & 0x80000) z |= 0xFFF00000;

    data->x = x;
    data->y = y;
    data->z = z;

    data->x_g = (float)x / current_sensitivity;
    data->y_g = (float)y / current_sensitivity;
    data->z_g = (float)z / current_sensitivity;

    if (adxl_offsets_valid) {
        data->x_g -= adxl_offset_x_g;
        data->y_g -= adxl_offset_y_g;
        data->z_g -= adxl_offset_z_g;
    }

    data->timestamp = HAL_GetTick();
    return 1;
}
