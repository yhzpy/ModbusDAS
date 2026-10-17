/**
 * @file    sht30.c
 * @brief   SHT30 Driver Implementation
 * TODO: Step 1 - implement I2C transactions.
 */

#include "sht30.h"
#include "i2c1_reg.h"
#include "delay.h"
//如果我自己设计函数 当一个参数可以是NULL的时候 我应当自己加判断逻辑 不然会报错
//#define NULL ((void *)0)   /* 标准库里大致长这样 */

int8_t SHT30_Init(void)
{
    uint8_t buf[3];
    int8_t ret;
    /* TODO Step 1:
     * 1. Send soft reset command (0x30A2)
     * 2. Wait 2ms
     * 3. Read status register (0xF32D), verify CRC
     * 4. Return SHT30_OK if device ACKs
     */
    ret = I2C1_WriteCmd16(SHT30_ADDR, SHT30_CMD_SOFT_RESET, NULL, 0);
    if (ret != I2C_OK) return SHT30_ERR_I2C;

    delay_ms(2);

    ret = I2C1_ReadCmd16(SHT30_ADDR, SHT30_CMD_READ_STATUS, buf, 3);
    if(ret != I2C_OK) return SHT30_ERR_I2C;

    if (SHT30_CRC8(buf, 2)!=buf[2]) return SHT30_ERR_CRC;

    return SHT30_OK; /* placeholder */
}

int8_t SHT30_Read(int16_t *temp_x100, uint16_t *humi_x100)
{
    uint8_t buf[6];
    int8_t ret;
    /* TODO Step 1:
     * 1. Write command 0x2400 (high repeatability, no clock stretch)
     * 2. Wait 15ms (measurement time)
     * 3. Read 6 bytes: [temp_msb, temp_lsb, temp_crc, humi_msb, humi_lsb, humi_crc]
     * 4. Verify CRC-8 for temp (first 2 bytes) and humi (next 2 bytes)
     * 5. Convert:
     *    raw_temp = (temp_msb << 8) | temp_lsb
     *    *temp_x100 = (int16_t)(-4500 + (17500 * (int32_t)raw_temp) / 65535)
     *    raw_humi = (humi_msb << 8) | humi_lsb
     *    *humi_x100 = (uint16_t)((10000 * (uint32_t)raw_humi) / 65535)
     */
    ret = I2C1_WriteCmd16(SHT30_ADDR, SHT30_CMD_MEAS_STRETCH, NULL, 0);
    if (ret != I2C_OK) return SHT30_ERR_I2C;
    delay_ms(15);
    ret = I2C1_ReadCmd16(SHT30_ADDR, SHT30_CMD_MEAS_STRETCH, buf, 6);
    if(ret != I2C_OK) return SHT30_ERR_I2C;
    if (SHT30_CRC8(buf, 2) != buf[2] || SHT30_CRC8(buf+3, 2) != buf[5]) return SHT30_ERR_CRC;

    *temp_x100 = (int16_t)(-4500 + (17500 * (int32_t)buf[0] << 8 | buf[1]) / 65535);
    *humi_x100 = (uint16_t)((10000 * (uint32_t)buf[3] << 8 | buf[4]) / 65535);
    
    return SHT30_OK;
}

uint8_t SHT30_CRC8(const uint8_t *data, uint8_t len)
{
    /* CRC-8: polynomial 0x31 (x^8 + x^5 + x^4 + 1), init 0xFF */
    uint8_t crc = 0xFF;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}
