#include "spi_reg.h"
#include <stdint.h>

#define MAX_WAIT_TIME 50000
/*SPI_CR1寄存器*/
#define SPI_CR1_CPHA        ((uint16_t)(1u << 0))   /* 时钟相位 */
#define SPI_CR1_CPOL        ((uint16_t)(1u << 1))   /* 时钟极性 */
#define SPI_CR1_MSTR        ((uint16_t)(1u << 2))   /* 主模式 */
#define SPI_CR1_SPE         ((uint16_t)(1u << 6))   /* SPI 使能 */
#define SPI_CR1_LSBFIRST    ((uint16_t)(1u << 7))   /* LSB 先行 */
#define SPI_CR1_SSI         ((uint16_t)(1u << 8))   /* 内部从选 */
#define SPI_CR1_SSM         ((uint16_t)(1u << 9))   /* 软件 NSS 管理 */
#define SPI_CR1_RXONLY        ((uint16_t)(1u << 10))  /* 只读 */
#define SPI_CR1_DFF         ((uint16_t)(1u << 11))  /* 16 位帧 */
#define SPI_CR1_CRCNEXT       ((uint16_t)(1u << 17))  /* CRC 下一个 */
#define SPI_CR1_CRCEN         ((uint16_t)(1u << 18))  /* CRC 启用 */
#define SPI_CR1_BIDIMODE      ((uint16_t)(1u << 15))  /* 双线模式 */
#define SPI_CR1_BIDIOE        ((uint16_t)(1u << 16))  /* 双线输出使能 */

/*SPI SR寄存器*/
#define SPI_SR_RXNE         ((uint16_t)(1u << 0))   /* 接收缓存区非空中断 */
#define SPI_SR_TXE          ((uint16_t)(1u << 1))   /* 发送缓存区空中断 */
#define SPI_SR_UDR          ((uint16_t)(1u << 3))   /* 如果主机已经开始打时钟了，而从机还没来得及往 DR 里写要发的数据，则 UDR 标志被设置 */
#define SPI_SR_CRCERR       ((uint16_t)(1u << 4))   /* CRC 错误中断 */
#define SPI_SR_MODF         ((uint16_t)(1u << 5))   /* 模式故障中断 */
#define SPI_SR_OVR          ((uint16_t)(1u << 6))   /* 过载中断 */
#define SPI_SR_BSY          ((uint16_t)(1u << 7))   /* 忙碌标志 */


/* BR[2:0] 是 3 位的多值字段，按档位定义而不是按位 */
#define SPI_CR1_BR_DIV4     ((uint16_t)(1u << 3))   /* fPCLK/4 */
#define SPI_CR1_BR_DIV16    ((uint16_t)(3u << 3))   /* fPCLK/16 */

#define SPI_CR2_SSOE        ((uint16_t)(1u << 2))   /* NSS 输出使能 */



/*选择 BR[2:0] 位来定义串行时钟波特率（见 SPI_CR1 寄存器）。
选择 CPOL 和 CPHA 位来定义数据传输与串行时钟的四种关系之一（见图 240）。
设置 DFF 位来定义 8 位或 16 位数据帧格式。在 SPI_CR1 寄存器中配置 LSBFIRST 位以定义帧格式。
如果 NSS 引脚在输入模式下需要，在硬件模式下，在整个字节传输过程中将 NSS 引脚连接到高电平信号。
在 NSS 软件模式下，设置 SPI_CR1 寄存器中的 SSM 和 SSI 位。如果 NSS 引脚在输出模式下需要，
只需设置 SSOE 位。必须设置 MSTR 和 SPE 位（它们只有在 NSS 引脚连接到高电平信号时才保持设置状态）。*/
int8_t SPI_Init(SPI_TypeDef *spi)
{
    spi->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_DIV4 | SPI_CR1_SSM | SPI_CR1_SSI;
    // spi->CR2 = SPI_CR2_SSOE;
    spi->CR1 |= SPI_CR1_SPE;
    return SPI_OK;
}

/*在全双工模式下（BIDIMODE=0 且 RXONLY=0）—— 当数据写入 SPI_DR 寄存器（Tx 缓冲区）时，序列开始
。—— 数据随后在第一次比特传输期间从 Tx 缓冲区并行加载到 8 位移位寄存器中，然后串行移出到 MOSI 引脚。
—— 同时，MISO 引脚上接收到的数据会串行移入到 8 位移位寄存器中，然后并行加载到 SPI_DR 寄存器（Rx 缓冲区）。*/


/*spi的双缓冲机制*/
/*写 DR ──→ [ Tx 缓冲区 ] ──→ [ 8位移位寄存器 ] ──→ MOSI 引脚
              ↑                    ↑
            TXE 看这里           BSY 看这里*/

/*写 DR ──> [ Tx 缓冲区 ] ──> ┌──────────────────┐ ──> MOSI（移出）
                             │  移位寄存器（唯一）│
  读 DR <── [ Rx 缓冲区 ] <── └──────────────────┘ <── MISO（移入）*/
  //每次写出去一个字节 就会收回一个字节 dr没有及时读取的话就会触发ovr过载错误
static int8_t SPI_Wait_Flag(SPI_TypeDef *spi, uint16_t flag)
{
    uint16_t time = MAX_WAIT_TIME;
    while(--time)
    {
        /*错误原因：MODF/OVR 是"读序列清除"类标志（RM0008），直接写 0 无效：
          OVR 必须由"读 SR + 读 DR"序列清除，MODF 必须由"读 SR + 写 CR1"序列清除。
          之前 spi->SR &= ~xxx 写 0 清不掉标志，报错返回后标志仍挂着，
          下次调用还会持续报同样的错误。*/
        if(spi->SR & SPI_SR_MODF)
        {
            (void)spi->SR;              /* 读 SR */
            spi->CR1 = spi->CR1;        /* 写 CR1 → 清 MODF */
            return SPI_MODF;
        }
        if(spi->SR & SPI_SR_OVR)
        {
            (void)spi->SR;              /* 读 SR */
            (void)spi->DR;              /* 读 DR → 清 OVR */
            return SPI_OVR;
        }
        if(spi->SR & flag)
        {
            return SPI_OK;
        }
    }
    return SPI_TIMEOUT;
}

/*等待 BSY 清零，确认最后一帧真正移完（BSY=1 表示忙）
错误原因：之前用 SPI_Wait_Flag(spi, SPI_SR_BSY) 等 BSY 置位，
再用"等置位超时"反推"位是 0"，有两个问题：
1) 传输结束后 BSY 本来就是 0，永远等不到置位，每次都要白空转 MAX_WAIT_TIME 次循环；
2) 真正忙的时候反而立刻返回 SPI_BUSY，并没有继续等到总线空闲，
   调用方会误以为传输完成，其实最后一帧可能还在移位寄存器里。
所以等待"清零"必须单独写循环，等待"置位"的函数表达不了这个语义。*/
static int8_t SPI_Wait_BSY(SPI_TypeDef *spi)
{
    uint16_t time = MAX_WAIT_TIME;
    while(--time)
    {
        if(!(spi->SR & SPI_SR_BSY))
        {
            return SPI_OK;              /* BSY=0，总线空闲，传输完成 */
        }
    }
    return SPI_TIMEOUT;                 /* 一直忙，等待超时 */
}

int8_t SPI_Write(SPI_TypeDef *spi,const uint8_t* buf, uint16_t len)
{
    int8_t ret;
    if(buf == NULL)
        return SPI_ERROR;
    if(len < 1) return SPI_ERROR;
    if(len == 1)
    {
        ret = SPI_Wait_Flag(spi, SPI_SR_TXE);
        if(ret != SPI_OK)  return ret;/*如果在这里直接return 会吞掉本来的错误码  所以必须return ret*/
        spi->DR = buf[0];
        
        ret = SPI_Wait_Flag(spi, SPI_SR_RXNE);
        if(ret != SPI_OK) return ret;
        (void)spi->DR;
        /*(void)表达式; 就是把一个表达式的值强制"转成" void 类型。
        void 意味着"无值"，所以整句话的意思就是："我就是要执行这个表达式，但明确声明结果我不用了"。*/
        ret = SPI_Wait_BSY(spi);
        if(ret != SPI_OK) return ret;   /* 末帧未移完或超时，必须上报，不能照样返回成功 */
    }
    else
    {
        for(uint16_t i = 0; i < len; i++)
        {
            ret = SPI_Wait_Flag(spi, SPI_SR_TXE);
            if(ret != SPI_OK) return ret;
            spi->DR = buf[i];
            ret = SPI_Wait_Flag(spi, SPI_SR_RXNE);
            if(ret != SPI_OK) return ret;
            (void)spi->DR;
        }
        ret = SPI_Wait_BSY(spi);
        if(ret != SPI_OK) return ret;   /* 末帧未移完或超时，必须上报 */
    }
    return SPI_OK;
}
    
int8_t SPI_Read(SPI_TypeDef *spi, uint8_t* buf, uint16_t len)
{
    int8_t ret;
    if(buf == NULL)
        return SPI_ERROR;
    if(len < 1) return SPI_ERROR;
    if(len == 1)
    {
        ret = SPI_Wait_Flag(spi, SPI_SR_TXE);
        if(ret != SPI_OK)  return ret;
        spi->DR = 0xFF;
        ret = SPI_Wait_Flag(spi, SPI_SR_RXNE);
        if(ret != SPI_OK) return ret;
        buf[0] = spi->DR;
        
        ret = SPI_Wait_BSY(spi);
        if(ret != SPI_OK) return ret;   /* 末帧未移完或超时，必须上报 */
    }
    else
    {
        for(uint16_t i = 0; i < len; i++)
        {
            ret = SPI_Wait_Flag(spi, SPI_SR_TXE);
            if(ret != SPI_OK) return ret;
            spi->DR = 0xFF;
            ret = SPI_Wait_Flag(spi, SPI_SR_RXNE);
            if(ret != SPI_OK) return ret;
            buf[i] = spi->DR;
        }
        
        ret = SPI_Wait_BSY(spi);
        if(ret != SPI_OK) return ret;   /* 末帧未移完或超时，必须上报 */
    }
    return SPI_OK;
}