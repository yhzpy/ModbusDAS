/**
 * @file    i2c1_reg.h
 * @brief   STM32F103 硬件 I2C1 寄存器级驱动（无 HAL 依赖）
 *
 * 引脚: PB6 = SCL, PB7 = SDA（AF 开漏，外部上拉）
 * 速率: 100 kHz 标准模式
 * 设备: SHT30(0x44) + DS3231(0x68) + AT24C32(0x57)
 */

#ifndef __I2C1_REG_H
#define __I2C1_REG_H

#include <stdint.h>

/*============================================================================
 * 错误码
 *============================================================================*/
#define I2C_OK              0
#define I2C_ERR_TIMEOUT    -1   /* 等待标志超时（总线忙死或从机无响应） */
#define I2C_ERR_NACK       -2   /* 从机未应答（地址不存在或设备忙） */
#define I2C_ERR_BUS        -3   /* 总线错误（仲裁丢失或多主冲突） */
#define I2C_ERR_PARAM      -4   /* 参数非法（空指针/长度0） */

/*============================================================================
 * I2C 设备地址（7-bit 格式，驱动内部自动左移）
 *============================================================================*/
#define I2C_ADDR_SHT30      0x44
#define I2C_ADDR_DS3231     0x68
#define I2C_ADDR_AT24C32    0x57

/*============================================================================
 * 公共 API
 *============================================================================*/

/**
 * @brief  初始化 I2C1 外设 + GPIO（PB6/PB7 AF开漏）
 *         调用时机: main() 里 HAL_Init() + SystemClock_Config() 之后
 * @return I2C_OK 或错误码
 */
int8_t I2C1_Init(void);

/**
 * @brief  向从机写数据（主发送模式）
 * @param  devAddr: 7-bit 从机地址（如 I2C_ADDR_SHT30）
 * @param  regAddr: 从机内部寄存器地址（8-bit）
 * @param  data:    要写入的数据缓冲区
 * @param  len:     字节数（1-255）
 * @return I2C_OK 或错误码
 *
 * 时序: START → 地址+W → ACK → 寄存器地址 → ACK → data[0..n] → ACK → STOP
 */
int8_t I2C1_Write(uint8_t devAddr, uint8_t regAddr, const uint8_t *data, uint8_t len);

/**
 * @brief  从从机读数据（先写寄存器地址，再 Repeated START 读）
 * @param  devAddr: 7-bit 从机地址
 * @param  regAddr: 从机内部寄存器起始地址
 * @param  buf:     接收缓冲区
 * @param  len:     要读的字节数（1-255）
 * @return I2C_OK 或错误码
 *
 * 时序: START → 地址+W → ACK → regAddr → ACK
 *       → ReSTART → 地址+R → ACK → data[0..n-1](ACK) → data[n](NACK) → STOP
 */
int8_t I2C1_Read(uint8_t devAddr, uint8_t regAddr, uint8_t *buf, uint8_t len);

/**
 * @brief  向从机写 16-bit 寄存器地址再写数据（SHT30 命令格式）
 * @param  devAddr: 7-bit 从机地址
 * @param  cmd:     16-bit 命令/寄存器地址（MSB先发）
 * @param  data:    数据（可为 NULL 表示只发命令不带数据）
 * @param  len:     数据长度（0 = 只发命令）
 * @return I2C_OK 或错误码
 *
 * SHT30 的寄存器地址是 16-bit（如 0x2400），不同于 DS3231 的 8-bit。
 */
int8_t I2C1_WriteCmd16(uint8_t devAddr, uint16_t cmd, const uint8_t *data, uint8_t len);

/**
 * @brief  发送 16-bit 命令后读数据（SHT30 测量流程）
 * @param  devAddr: 7-bit 从机地址
 * @param  cmd:     16-bit 命令
 * @param  buf:     接收缓冲区
 * @param  len:     要读的字节数
 * @return I2C_OK 或错误码
 */
int8_t I2C1_ReadCmd16(uint8_t devAddr, uint16_t cmd, uint8_t *buf, uint8_t len);

/**
 * @brief  扫描 I2C 总线，打印所有应答的设备地址
 *         调试用：验证接线和上拉电阻是否正确
 */
void I2C1_Scan(void);

/**
 * @brief  检测指定地址的设备是否在线（发地址等 ACK）
 * @param  devAddr: 7-bit 地址
 * @return I2C_OK = 在线, I2C_ERR_NACK = 不在线
 */
int8_t I2C1_IsDeviceReady(uint8_t devAddr);

#endif /* __I2C1_REG_H */
