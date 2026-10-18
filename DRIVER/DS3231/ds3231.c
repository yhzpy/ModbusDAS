/**
 * @file    ds3231.c
 * @brief   DS3231 RTC + AT24C32 EEPROM Driver Implementation
 * TODO: Step 1 - implement I2C transactions.
 */

#include "ds3231.h"
#include "i2c1_reg.h"
#include "delay.h"
/* TODO Step 1: #include "i2c.h" for hi2c1 */

/* BCD conversion helpers */
static uint8_t bcd2bin(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t bin2bcd(uint8_t bin) { return ((bin / 10) << 4) | (bin % 10); }

int8_t DS3231_Init(void)
{
    uint8_t buf_SR[1];
    uint8_t buf_CTRL[1];
    int8_t ret;
    uint8_t sr;
    uint8_t ctrl;
    /* TODO Step 1:
     * 1. Read status register (0x0F)
     * 2. Check OSF bit (bit 7): if set, time is invalid (battery was removed)
     * 3. Clear OSF bit
     * 4. Disable 32kHz output (clear bit 3 of status reg) - save power
     * 5. Ensure control reg (0x0E) = 0x1C (BBSQW=1, others default)
     */
    ret = I2C1_Read(DS3231_ADDR, DS3231_REG_STATUS, buf_SR, 1);
    if (ret != I2C_OK) return DS3231_ERR_I2C;
    sr = buf_SR[0];

    ret = I2C1_Read(DS3231_ADDR, DS3231_REG_CTRL, buf_CTRL, 1);
    if (ret != I2C_OK) return DS3231_ERR_I2C;
    ctrl = buf_CTRL[0];
    ctrl |= DS3231_CTRL_BBSQW;    /* Enable battery square wave */

    if(sr & DS3231_BIT_OSF) return DS3231_ERR_OSF;
    sr &= ~DS3231_BIT_OSF;
    sr &= ~DS3231_BIT_EN32KHZ;

    ret = I2C1_Write(DS3231_ADDR, DS3231_REG_STATUS, &sr, 1);
    if (ret != I2C_OK) return DS3231_ERR_I2C;

    ret = I2C1_Write(DS3231_ADDR, DS3231_REG_CTRL, &ctrl, 1);
    return DS3231_OK;
}

int8_t DS3231_GetDateTime(DS3231_DateTime_t *dt)
{
    uint8_t buf[7];
    /* TODO Step 1:
     * 1. Read registers 0x00-0x06 (7 bytes)
     * 2. Convert BCD to binary:
     *    dt->sec   = bcd2bin(buf[0] & 0x7F)
     *    dt->min   = bcd2bin(buf[1] & 0x7F)
     *    dt->hour  = bcd2bin(buf[2] & 0x3F)  // 24h mode
     *    dt->wday  = bcd2bin(buf[3] & 0x07)
     *    dt->mday  = bcd2bin(buf[4] & 0x3F)
     *    dt->month = bcd2bin(buf[5] & 0x1F)  // bit7 = century, ignore
     *    dt->year  = bcd2bin(buf[6])
     */
    /*读完一个字节后"寄存器地址自动 +1"不是 DS3231 的专利，而是几乎所有带寄存器的 I2C 芯片的通用约定。
    但要注意一个前提：它不是 I2C 协议本身规定的，而是每个芯片自己在内部实现的。*/
    I2C1_Read(DS3231_ADDR, DS3231_REG_SEC, buf, 7);
    dt->sec = bcd2bin(buf[0] & 0x7F);
    dt->min = bcd2bin(buf[1] & 0x7F);
    dt->hour = bcd2bin(buf[2] & 0x3F);
    dt->wday = bcd2bin(buf[3] & 0x07);
    dt->mday = bcd2bin(buf[4] & 0x3F);
    dt->month = bcd2bin(buf[5] & 0x1F);
    dt->year = bcd2bin(buf[6]);
    return DS3231_OK;
}

int8_t DS3231_SetDateTime(const DS3231_DateTime_t *dt)
{
    /* TODO Step 1:
     * 1. Convert binary to BCD
     * 2. Write registers 0x00-0x06 (7 bytes in one I2C transaction)
     * 3. Hour register: clear bit6 (force 24h mode)
     */
    uint8_t buf[7];

    /* 参数是 const，不能修改调用者的数据：转换结果直接写入局部缓冲 */
    buf[0] = bin2bcd(dt->sec);
    buf[1] = bin2bcd(dt->min);
    buf[2] = bin2bcd(dt->hour) & 0x3F;   /* 清 bit6，强制 24h 制 */
    buf[3] = bin2bcd(dt->wday);
    buf[4] = bin2bcd(dt->mday);
    buf[5] = bin2bcd(dt->month);
    buf[6] = bin2bcd(dt->year);

    I2C1_Write(DS3231_ADDR, DS3231_REG_SEC, buf, 7);
    return DS3231_OK;
}


int16_t DS3231_ReadTemperature(void)
{
    /* TODO Step 1:
     * 1. Read registers 0x11-0x12 (2 bytes)
     * 2. temp_msb is signed int8 (two's complement)
     * 3. temp_lsb upper 2 bits = fractional (0.25 C per bit)
     * 4. return (int16_t)(temp_msb * 100 + (temp_lsb >> 6) * 25)
     */
    int8_t ret;//int8_t 是 C99 标准库 <stdint.h> 里定义的有符号 8 位整数
    int8_t temp_msb, temp_lsb;
    uint8_t buf[2];

    ret = I2C1_Read(DS3231_ADDR, DS3231_REG_TEMP_H, buf, 2);
    if (ret != I2C_OK) return DS3231_ERR_I2C;

    temp_msb = buf[0];
    temp_lsb = buf[1];
    return (int16_t)(temp_msb * 100 + (temp_lsb >> 6) * 25);
}

/*---- 时间戳计算辅助函数 ----*/

/* 平年每月天数（2 月是 28 天，闰年在计算时单独 +1） */
static const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
 * @brief  判断闰年：4 年一闰，100 年不闰，400 年再闰
 * @param  year: 完整年份（如 2000、2024）
 * @return 1 = 闰年, 0 = 平年
 */
static uint8_t is_leap_year(uint16_t year)
{
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

/**
 * @brief  计算某个日期距离 2000-01-01 经过的天数（2000-01-01 = 第 0 天）
 * @param  year:  完整年份（2000~2099，对应寄存器里的 0~99）
 * @param  month: 1~12
 * @param  day:   1~31
 * @return 天数
 */
static uint32_t days_since_2000(uint16_t year, uint8_t month, uint8_t day)
{
    uint32_t days = 0;
    uint16_t y;
    uint8_t m;

    /* ① 2000 年到去年为止的整年：平年 365 天，闰年 366 天 */
    for (y = 2000; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }

    /* ② 当年 1 月到上个月为止的整月（2 月闰年 +1 天） */
    for (m = 1; m < month; m++) {
        days += month_days[m - 1] + ((m == 2 && is_leap_year(year)) ? 1 : 0);
    }

    /* ③ 本月已过去的天数（今天还没过完，不算整天） */
    days += day - 1;

    return days;
}

uint32_t DS3231_GetTimestamp(void)
{
    DS3231_DateTime_t dt;
    uint32_t days;

    /* 从芯片读出当前日期时间（BCD 已在 GetDateTime 里转成二进制） */
    if (DS3231_GetDateTime(&dt) != DS3231_OK) {
        return 0;    /* 读取失败，返回 0 表示无效 */
    }

    days = days_since_2000(2000 + (uint16_t)dt.year, dt.month, dt.mday);

    /* 天数 x 86400 + 当日时分秒
     * 2000~2099 最多约 3.156e9 秒 < 2^32 (4.29e9)，uint32 装得下 */
    return days * 86400UL
         + (uint32_t)dt.hour * 3600UL
         + (uint32_t)dt.min  * 60UL
         + (uint32_t)dt.sec;
}

/*---- AT24C32 EEPROM ----*/
/*数据手册确实不会告诉你A0/A1/A2是1还是0，因为它本来就不是固定的。这三个脚是硬件配置脚（address strap pins），
由板子/模块的设计者自己决定接法。
手册只告诉你"地址字里的A2A1A0位必须和这三个引脚的实际电平一致"，至于引脚接成什么电平，是你硬件的事。*/
int8_t AT24C32_Write(uint16_t memAddr, const uint8_t *data, uint16_t len)
{
    int8_t  ret;
    uint16_t chunk;

    if (data == NULL || len == 0) {
        return DS3231_ERR_I2C;             /* 参数非法 */
    }

    while (len > 0) {
        /* 本页剩余空间：保证本块不跨页 */
        chunk = AT24C32_PAGE_SIZE - (memAddr % AT24C32_PAGE_SIZE);
        if (chunk > len) {
            chunk = len;                   /* 最后不足一页的部分 */
        }

        /* START → 0x57(W) → addr[15:8] → addr[7:0] → data[0..chunk-1] → STOP */
        ret = I2C1_WriteCmd16(AT24C32_ADDR, memAddr, data, (uint8_t)chunk);
        if (ret != I2C_OK) {
            return DS3231_ERR_I2C;         /* 错误码翻译为本驱动的错误码 */
        }

        delay_ms(AT24C32_WRITE_CYCLE);     /* 等内部写周期完成 */

        memAddr += chunk;
        data    += chunk;
        len     -= chunk;
    }
    return DS3231_OK;
}

int8_t AT24C32_Read(uint16_t memAddr, uint8_t *data, uint16_t len)
{
    int8_t  ret;
    uint8_t chunk;

    if (data == NULL || len == 0) {
        return DS3231_ERR_I2C;             /* 参数非法 */
    }

    /* 顺序读没有页边界限制，但底层 I2C1_ReadCmd16 单次最多 255 字节，
     * 超过就分段读，各段地址连续递增 */
    while (len > 0) {
        chunk = (len > 255u) ? 255u : (uint8_t)len;

        /* START → 0x57(W) → addr[15:8] → addr[7:0] → Sr → 0x57(R) → data... → STOP
         * 读/写头（devAddr<<1 | R/W）由 I2C1_ReadCmd16 内部追加 */
        ret = I2C1_ReadCmd16(AT24C32_ADDR, memAddr, data, chunk);
        if (ret != I2C_OK) {
            return DS3231_ERR_I2C;         /* 错误码翻译为本驱动的错误码 */
        }

        memAddr += chunk;
        data    += chunk;
        len     -= chunk;
    }
    return DS3231_OK;
}

