/**
 * @file    i2c1_reg.c
 * @brief   STM32F103 硬件 I2C1 寄存器级驱动实现（纯 CMSIS，无 HAL）
 *
 * 时钟前提: APB1 = 36 MHz（由 SystemClock_Config 保证）
 * 引脚:     PB6 = SCL, PB7 = SDA（AF 开漏，模块外部上拉 4.7kΩ）
 * 速率:     100 kHz 标准模式
 *
 * 寄存器参考: RM0008 Reference Manual §26 I2C
 *   - CR1:  PE/START/STOP/ACK/SWRST
 *   - CR2:  FREQ[5:0] = APB1 MHz
 *   - CCR:  CCR[11:0] = f_pclk1 / (2 × f_scl)
 *   - TRISE: 最大允许上升时间（以 T_pclk1 为单位）+ 1
 *   - SR1:  SB/ADDR/BTF/TXE/RXNE/AF/BERR
 *   - SR2:  BUSY/MSL/TRA
 *   - DR:   8-bit 数据收发
 */

#include "stm32f1xx.h"      /* CMSIS 寄存器定义（I2C1, GPIOB, RCC），不是 HAL 函数 */
#include "i2c1_reg.h"
#include <stdio.h>          /* I2C1_Scan 用 printf */

/*============================================================================
 * 内部宏与常量
 *============================================================================*/

/* 超时计数值（约 50ms @ 72MHz 空循环，实际取决于编译器优化等级） */
#define TIMEOUT_MAX         50000U
//1左移多少  就是第几位 只要看后面那个数字  sb第0位  btf第二位
/* SR1 位定义（避免 magic number，对照 RM0008 Table 136） */
#define SR1_SB              (1 << 0)    /* Start Bit: 起始条件已产生 */
#define SR1_ADDR            (1 << 1)    /* Address Sent: 地址已发送且收到 ACK */
#define SR1_BTF             (1 << 2)    /* Byte Transfer Finished: 一字节收发完成 */
#define SR1_RXNE            (1 << 6)    /* Receive Not Empty: DR 有数据可读 */
#define SR1_TXE             (1 << 7)    /* Transmit Empty: DR 空可写 */
#define SR1_BERR            (1 << 8)    /* Bus Error */
#define SR1_AF              (1 << 10)   /* Acknowledge Failure */

/* SR2 位定义（RM0008 Table 137） */
#define SR2_MSL             (1 << 0)    /* Master/Slave: 1=主机模式 */
#define SR2_BUSY            (1 << 1)    /* Bus Busy: 总线被占用 */
#define SR2_TRA             (1 << 2)    /* Transmitter/Receiver: 1=发送 */

/* CR1 位定义 */
#define CR1_PE              (1 << 0)    /* Peripheral Enable */
#define CR1_START           (1 << 8)    /* Start Generation */
#define CR1_STOP            (1 << 9)    /* Stop Generation */
#define CR1_ACK             (1 << 10)   /* Acknowledge Enable */
#define CR1_SWRST           (1 << 15)   /* Software Reset */

/*============================================================================
 * 内部辅助函数
 *============================================================================*/

/**
 * @brief  等待 SR1 中某个标志置位，同时检测错误
 * @param  flag: SR1 的位掩码（如 SR1_SB, SR1_ADDR, SR1_TXE）
 * @return I2C_OK / I2C_ERR_TIMEOUT / I2C_ERR_NACK
 */
static int8_t i2c_wait_sr1(uint16_t flag)
{
    uint32_t timeout = TIMEOUT_MAX;

    while (timeout--) {
        uint16_t sr1 = I2C1->SR1;

        /* 先检查错误标志（AF 优先级高于正常流程） */
        if (sr1 & SR1_AF) {
            I2C1->SR1 &= ~SR1_AF;     /* 写 0 清除 AF */
            return I2C_ERR_NACK;
        }
        if (sr1 & SR1_BERR) {
            I2C1->SR1 &= ~SR1_BERR;
            return I2C_ERR_BUS;
        }

        /* 检查目标标志 */
        if (sr1 & flag) {
            return I2C_OK;
        }
    }

    return I2C_ERR_TIMEOUT;
}

/**
 * @brief  等待 SR2 中 BUSY 位清零（总线空闲）
 */
static int8_t i2c_wait_busy_clear(void)
{
    uint32_t timeout = TIMEOUT_MAX;

    while (timeout--) {
        if (!(I2C1->SR2 & SR2_BUSY)) {
            return I2C_OK;
        }
    }

    /* 总线锁死恢复：发 9 个时钟脉冲释放从机（SDA 被从机拉低的经典场景）
     * 这里简化处理：SWRST 复位 I2C 外设再重新使能 */
    I2C1->CR1 |= CR1_SWRST;
    I2C1->CR1 &= ~CR1_SWRST;
    I2C1->CR1 |= CR1_PE;
    /* 重新配置（SWRST 会清除所有配置寄存器） */
    I2C1->CR2 = 36;
    I2C1->CCR = 180;
    I2C1->TRISE = 37;

    return I2C_ERR_TIMEOUT;
}

/**
 * @brief  清除 ADDR 标志（硬件要求：读 SR1 再读 SR2，缺一不可）
 *         RM0008 §26.6.2: "The ADDR flag is cleared by software sequence:
 *         a read of SR1 register followed by a read of SR2 register."
 */
static inline void i2c_clear_addr(void)
{
    (void)I2C1->SR1;   /* 读 SR1（dummy read） */
    (void)I2C1->SR2;   /* 读 SR2 → 硬件清除 ADDR */
}

/**
 * @brief  产生 START 条件并等待 SB 置位
 */
static int8_t i2c_start(void)
{
    I2C1->CR1 |= CR1_START;    /* 置 START 位，硬件在总线上产生起始条件 */

    /* 等待 SB=1：START 已成功发出，DR 可以写地址了 */
    return i2c_wait_sr1(SR1_SB);
}

/**
 * @brief  产生 STOP 条件
 */
static inline void i2c_stop(void)
{
    I2C1->CR1 |= CR1_STOP;
}

/**
 * @brief  发送 1 字节地址并等待 ADDR（从机应答）
 * @param  addr8: 8-bit 地址（7-bit 左移 + R/W 位）
 *                 写: devAddr << 1 | 0
 *                 读: devAddr << 1 | 1
 */
static int8_t i2c_send_addr(uint8_t addr8)
{
    I2C1->DR = addr8;          /* 写地址到 DR，硬件自动移位发送 */

    /* 等待 ADDR=1：地址已发出且从机回了 ACK */
    int8_t ret = i2c_wait_sr1(SR1_ADDR);
    if (ret != I2C_OK) {
        i2c_stop();            /* 地址无人应答，释放总线 */
        return ret;
    }
    return I2C_OK;
}

/*============================================================================
 * 公共 API 实现
 *============================================================================*/

int8_t I2C1_Init(void)
{
    /*--- 第一步：开时钟 ---*/
    RCC->APB2ENR |= (1 << 3);     /* IOPBEN: GPIOB 时钟使能（PB6/PB7） */
    RCC->APB1ENR |= (1 << 21);    /* I2C1EN: I2C1 外设时钟使能 */

    /*--- 第二步：配置 PB6/PB7 为 AF 开漏 50MHz ---*/
    /*
     * GPIOB->CRL 管 Pin0~Pin7，每 Pin 占 4 bit [CNF1:CNF0:MODE1:MODE0]
     * PB6 = bit[27:24], PB7 = bit[31:28]
     * AF 开漏 50MHz: CNF=11, MODE=11 → 0xF
     */
    //ODR = Output Data Register（输出数据寄存器）
    //GPIO 输出模式下，每个引脚一个位，写什么就输出什么：  GPIOB->ODR |=  (1 << 6);  pb6输出高电平
    //但是ODR只有通用模式下有意义
    //CRL里面有32位 4位管一个引脚  cnf 1：0   mode 1：0   
    //                           cnf 11 表示复用开漏   mode11 表示50MHz
    GPIOB->CRL &= ~(0xFFU << 24);  /* 清除 PB6+PB7 原有配置 */
    GPIOB->CRL |=  (0xFFU << 24);  /* PB6=0xF, PB7=0xF */

    /*--- 第三步：SWRST 复位 I2C（确保从干净状态开始） ---*/
    I2C1->CR1 |= CR1_SWRST;
    I2C1->CR1 &= ~CR1_SWRST;

    /*--- 第四步：配置 I2C 时序 ---*/
    /*
     * CR2.FREQ = APB1 时钟频率（MHz）= 36
     * 硬件用这个值计算内部定时（上升沿滤波、SDA setup/hold）
     */
    I2C1->CR2 = 36;

    /*
     * CCR = f_PCLK1 / (2 × f_SCL)  (标准模式, F/S=0, DUTY=0)
     *     = 36,000,000 / (2 × 100,000) = 180
     *
     * SCL 高电平时间 = CCR × T_pclk1 = 180 × 27.78ns = 5μs
     * SCL 低电平时间 = CCR × T_pclk1 = 5μs
     * 周期 = 10μs → 频率 = 100kHz ✓
     */
    I2C1->CCR = 180;             /* F/S=0(标准模式), DUTY=0, CCR=180 */

    /*
     * TRISE = (最大允许上升时间 / T_pclk1) + 1
     *        = (1000ns / 27.78ns) + 1 = 36 + 1 = 37
     *
     * I2C 规范: 标准模式最大上升时间 1000ns
     * 这个值告诉硬件 SCL 拉高后多久采样 SDA（滤波毛刺）
     */
    I2C1->TRISE = 37;

    /*--- 第五步：使能 I2C 外设 ---*/
    I2C1->CR1 |= CR1_PE;

    /*--- 第六步：验证总线不忙 ---*/
    /* 如果上电瞬间 SDA 被从机拉低（比如上次通信中途 MCU 复位），
     * 手动发 9 个 SCL 脉冲让从机释放 SDA */
    if (I2C1->SR2 & SR2_BUSY) {
        /* 总线忙恢复：临时把 PB6 改成 GPIO 推挽输出，手动打 9 个脉冲 */
        I2C1->CR1 &= ~CR1_PE;            /* 先关 I2C */

        GPIOB->CRL &= ~(0xFU << 24);     /* PB6 改成通用推挽 50MHz = 0x3 */
        GPIOB->CRL |=  (0x3U << 24);

        for (int i = 0; i < 9; i++) {
            GPIOB->BSRR = (1 << 6);      /* SCL 高 */
            for (volatile int d = 0; d < 100; d++);
            GPIOB->BRR  = (1 << 6);      /* SCL 低 *///BRR写1代表对应引脚低电平
            for (volatile int d = 0; d < 100; d++);
        }
        GPIOB->BSRR = (1 << 6);          /* SCL 最终拉高 */
// ODR	直接写 16 个引脚的值	会拉低引脚	否（读-改-写）	整组输出（如 8 位并口）
// BSRR	置位+复位一体	         无效	   是	          单/多引脚翻转，最通用
// BRR	只复位（拉低）	         无效	   是	          明确表达"拉低"的语义
        /* 恢复 PB6 为 AF 开漏 */
        GPIOB->CRL &= ~(0xFU << 24);
        GPIOB->CRL |=  (0xFU << 24);

        /* 重新使能 I2C */
        I2C1->CR1 |= CR1_PE;

        /* 产生一个 STOP 条件彻底释放总线 */
        I2C1->CR1 |= CR1_START;
        I2C1->CR1 |= CR1_STOP;
        for (volatile int d = 0; d < 1000; d++);
    }

    return I2C_OK;
}
// 0x01 和 1 是同一个数
// uint8_t a = 0x01;   // 十六进制写法
// uint8_t b = 1;      // 十进制写法
// 这两个编译后在内存里一模一样，都是 0000 0001。因为：
// 进制只是"人怎么读"，不是"数本身"。数在内存/机器里永远是二进制。0x01、1、01（八进制）都是同一个东西的三种写法。
//uint8_t buf[] = {0x01, 0x10, 0xFF};   // 十六进制
//uint8_t buf[] = {1,    16,   255};    // 十进制
//以上俩种写法是完全一样的  就是单纯人看着不一样  在内存里一样的 就取决于uint8而已
int8_t I2C1_Write(uint8_t devAddr, uint8_t regAddr, const uint8_t *data, uint8_t len)
{
    if (!data || len == 0) return I2C_ERR_PARAM;

    int8_t ret;

    /* ① 等总线空闲 */
    ret = i2c_wait_busy_clear();
    if (ret != I2C_OK) return ret;

    /* ② 产生 START */
    ret = i2c_start();
    if (ret != I2C_OK) { i2c_stop(); return ret; }

    /* ③ 发送从机地址（写模式: bit0=0） */
    ret = i2c_send_addr(devAddr << 1 | 0);
    if (ret != I2C_OK) return ret;

    /* ④ 清除 ADDR（读 SR1 再读 SR2） */
    i2c_clear_addr();

    /* ⑤ 等 TXE（DR 空），发送寄存器地址 */
    ret = i2c_wait_sr1(SR1_TXE);
    if (ret != I2C_OK) { i2c_stop(); return ret; }
    I2C1->DR = regAddr;

    /* ⑥ 逐字节发送数据 */
    for (uint8_t i = 0; i < len; i++) {
        ret = i2c_wait_sr1(SR1_TXE);
        if (ret != I2C_OK) { i2c_stop(); return ret; }
        I2C1->DR = data[i];
    }

    /* ⑦ 等 BTF（最后一个字节已经从移位寄存器移出） */
    ret = i2c_wait_sr1(SR1_BTF);
    if (ret != I2C_OK) { i2c_stop(); return ret; }

    /* ⑧ 产生 STOP */
    i2c_stop();

    return I2C_OK;
}

int8_t I2C1_Read(uint8_t devAddr, uint8_t regAddr, uint8_t *buf, uint8_t len)
{
    if (!buf || len == 0) return I2C_ERR_PARAM;

    int8_t ret;

    /* ① 等总线空闲 */
    ret = i2c_wait_busy_clear();
    if (ret != I2C_OK) return ret;

    /* ========== 阶段一：写寄存器地址 ========== */

    /* ② START */
    ret = i2c_start();
    if (ret != I2C_OK) { i2c_stop(); return ret; }

    /* ③ 发地址（写模式） */
    ret = i2c_send_addr(devAddr << 1 | 0);
    if (ret != I2C_OK) return ret;

    /* ④ 清 ADDR */
    i2c_clear_addr();

    /* ⑤ 发寄存器地址 */
    ret = i2c_wait_sr1(SR1_TXE);
    if (ret != I2C_OK) { i2c_stop(); return ret; }
    I2C1->DR = regAddr;

    /* ⑥ 等 BTF（寄存器地址发完了） */
    ret = i2c_wait_sr1(SR1_BTF);
    if (ret != I2C_OK) { i2c_stop(); return ret; }

    /* ========== 阶段二：Repeated START + 读数据 ========== */

    if (len == 1) {
        /*--- 单字节读（特殊时序，RM0008 §26.6.8 Fig 244）---*/

        /* ⑦ 清 ACK（告诉硬件：收到这字节后发 NACK） */
        I2C1->CR1 &= ~CR1_ACK;

        /* ⑧ Repeated START */
        ret = i2c_start();
        if (ret != I2C_OK) { i2c_stop(); return ret; }

        /* ⑨ 发地址（读模式: bit0=1） */
        ret = i2c_send_addr(devAddr << 1 | 1);
        if (ret != I2C_OK) return ret;

        /* ⑩ 清 ADDR 后立刻发 STOP（RM0008 要求：
         *    单字节读时必须在 ADDR 清除后、RXNE 置位前发 STOP，
         *    否则从机会继续驱动 SDA 导致总线锁死） */
        i2c_clear_addr();
        i2c_stop();

        /* ⑪ 等 RXNE，读出数据 */
        ret = i2c_wait_sr1(SR1_RXNE);
        if (ret != I2C_OK) return ret;
        buf[0] = I2C1->DR;

    } else {
        /*--- 多字节读（RM0008 §26.6.8 Fig 245）---*/

        /* ⑦ 置 ACK（告诉硬件：每收一字节自动回 ACK） */
        I2C1->CR1 |= CR1_ACK;

        /* ⑧ Repeated START */
        ret = i2c_start();
        if (ret != I2C_OK) { i2c_stop(); return ret; }

        /* ⑨ 发地址（读模式） */
        ret = i2c_send_addr(devAddr << 1 | 1);
        if (ret != I2C_OK) return ret;

        /* ⑩ 清 ADDR */
        i2c_clear_addr();

        /* ⑪ 循环读取 N-1 字节（硬件自动 ACK） */
        for (uint8_t i = 0; i < len - 1; i++) {
            ret = i2c_wait_sr1(SR1_RXNE);
            if (ret != I2C_OK) { i2c_stop(); return ret; }
            buf[i] = I2C1->DR;
        }

        /* ⑫ 最后一字节：先清 ACK → 等 BTF → 发 STOP → 读 DR
         *
         * 为什么是这个顺序：
         *   清 ACK 后，当前正在接收的字节完成时硬件发 NACK，
         *   从机收到 NACK 释放 SDA。
         *   等 BTF 确保这字节已经完全收到 DR 里。
         *   先发 STOP 再读 DR（或先读再 STOP 都行，RM0008 两种都允许，
         *   但先 STOP 更安全，避免从机多驱动一个字节）。 */
        I2C1->CR1 &= ~CR1_ACK;            /* 清 ACK */
        ret = i2c_wait_sr1(SR1_BTF);       /* 等最后字节收完 */
        if (ret != I2C_OK) { i2c_stop(); return ret; }
        i2c_stop();                         /* 发 STOP */
        buf[len - 1] = I2C1->DR;           /* 读出最后字节 */
    }

    /* 恢复 ACK 使能（为下次通信准备） */
    I2C1->CR1 |= CR1_ACK;

    return I2C_OK;
}

int8_t I2C1_WriteCmd16(uint8_t devAddr, uint16_t cmd, const uint8_t *data, uint8_t len)
{
    /*
     * SHT30 的命令是 16-bit（如 0x2400 = 触发测量），
     * 相当于先发高字节再发低字节，然后跟数据。
     * 本质就是 Write(addr, cmd_MSB, [cmd_LSB, data...])，
     * 但为了接口清晰单独封装。
     */
    uint8_t buf[2 + 255];
    buf[0] = (uint8_t)(cmd >> 8);    /* 命令高字节 */
    buf[1] = (uint8_t)(cmd & 0xFF);  /* 命令低字节 */

    int8_t ret;

    /* 等总线空闲 */
    ret = i2c_wait_busy_clear();
    if (ret != I2C_OK) return ret;

    /* START */
    ret = i2c_start();
    if (ret != I2C_OK) { i2c_stop(); return ret; }

    /* 发地址（写） */
    ret = i2c_send_addr(devAddr << 1 | 0);
    if (ret != I2C_OK) return ret;
    i2c_clear_addr();

    /* 发 16-bit 命令 */
    for (int i = 0; i < 2; i++) {
        ret = i2c_wait_sr1(SR1_TXE);
        if (ret != I2C_OK) { i2c_stop(); return ret; }
        I2C1->DR = buf[i];
    }

    /* 发数据（如果有） */
    if (data && len > 0) {
        for (uint8_t i = 0; i < len; i++) {
            ret = i2c_wait_sr1(SR1_TXE);
            if (ret != I2C_OK) { i2c_stop(); return ret; }
            I2C1->DR = data[i];
        }
    }

    /* 等 BTF + STOP */
    ret = i2c_wait_sr1(SR1_BTF);
    if (ret != I2C_OK) { i2c_stop(); return ret; }
    i2c_stop();

    return I2C_OK;
}

int8_t I2C1_ReadCmd16(uint8_t devAddr, uint16_t cmd, uint8_t *buf, uint8_t len)
{
    if (!buf || len == 0) return I2C_ERR_PARAM;

    int8_t ret;

    /* 阶段一：发 16-bit 命令 */
    ret = i2c_wait_busy_clear();
    if (ret != I2C_OK) return ret;

    ret = i2c_start();
    if (ret != I2C_OK) { i2c_stop(); return ret; }

    ret = i2c_send_addr(devAddr << 1 | 0);
    if (ret != I2C_OK) return ret;
    i2c_clear_addr();

    /* 发命令高字节 */
    ret = i2c_wait_sr1(SR1_TXE);
    if (ret != I2C_OK) { i2c_stop(); return ret; }
    I2C1->DR = (uint8_t)(cmd >> 8);

    /* 发命令低字节 */
    ret = i2c_wait_sr1(SR1_TXE);
    if (ret != I2C_OK) { i2c_stop(); return ret; }
    I2C1->DR = (uint8_t)(cmd & 0xFF);

    /* 等 BTF */
    ret = i2c_wait_sr1(SR1_BTF);
    if (ret != I2C_OK) { i2c_stop(); return ret; }

    /* 阶段二：Repeated START 读数据 */
    if (len == 1) {
        I2C1->CR1 &= ~CR1_ACK;
        ret = i2c_start();
        if (ret != I2C_OK) { i2c_stop(); return ret; }
        ret = i2c_send_addr(devAddr << 1 | 1);
        if (ret != I2C_OK) return ret;
        i2c_clear_addr();
        i2c_stop();
        ret = i2c_wait_sr1(SR1_RXNE);
        if (ret != I2C_OK) return ret;
        buf[0] = I2C1->DR;

    } else {
        I2C1->CR1 |= CR1_ACK;
        ret = i2c_start();
        if (ret != I2C_OK) { i2c_stop(); return ret; }
        //线上字节:  [  A6 A5 A4 A3 A2 A1 A0  ] [ R/W ]
        //          \_______ 7-bit 地址 _____/  方向位
        //             占据 bit7 ~ bit1         占 bit0
        //区分读取头还是写头  就是要把地址左移动1位 然后自己补上0或1
        ret = i2c_send_addr(devAddr << 1 | 1);
        if (ret != I2C_OK) return ret;
        i2c_clear_addr();

        for (uint8_t i = 0; i < len - 1; i++) {
            ret = i2c_wait_sr1(SR1_RXNE);
            if (ret != I2C_OK) { i2c_stop(); return ret; }
            buf[i] = I2C1->DR;
        }

        /* 最后字节：清 ACK → 等 BTF → STOP → 读 DR */
        I2C1->CR1 &= ~CR1_ACK;
        ret = i2c_wait_sr1(SR1_BTF);
        if (ret != I2C_OK) { i2c_stop(); return ret; }
        i2c_stop();
        buf[len - 1] = I2C1->DR;
    }

    I2C1->CR1 |= CR1_ACK;  /* 恢复 ACK */
    return I2C_OK;
}

int8_t I2C1_IsDeviceReady(uint8_t devAddr)
{
    int8_t ret;

    ret = i2c_wait_busy_clear();
    if (ret != I2C_OK) return ret;

    ret = i2c_start();
    if (ret != I2C_OK) { i2c_stop(); return ret; }

    /* 只发地址，看有没有 ACK */
    ret = i2c_send_addr(devAddr << 1 | 0);
    i2c_stop();

    return ret;  /* I2C_OK = 设备在线, I2C_ERR_NACK = 不在线 */
}

void I2C1_Scan(void)
{
    printf("\r\n--- I2C1 Bus Scan ---\r\n");
    uint8_t found = 0;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (I2C1_IsDeviceReady(addr) == I2C_OK) {
            printf("  Found: 0x%02X", addr);

            /* 标注已知设备 */
            if (addr == I2C_ADDR_SHT30)   printf(" (SHT30 Temp/Humi)");
            if (addr == I2C_ADDR_DS3231)  printf(" (DS3231 RTC)");
            if (addr == I2C_ADDR_AT24C32) printf(" (AT24C32 EEPROM)");

            printf("\r\n");
            found++;
        }
    }

    if (found == 0) {
        printf("  No device found! Check:\r\n");
        printf("    1. SCL->PB6, SDA->PB7 wiring\r\n");
        printf("    2. Pull-up resistors (4.7k to 3.3V)\r\n");
        printf("    3. Module VCC = 3.3V, GND connected\r\n");
    } else {
        printf("  Total: %d device(s)\r\n", found);
    }
    printf("--- Scan End ---\r\n\r\n");
}
