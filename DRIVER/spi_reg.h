#ifndef __SPI_REG_H__
#define __SPI_REG_H__

#include "delay.h"

#define SPI_OK 0
#define SPI_BUSY 1
#define SPI_ERROR -1
#define SPI_MODF -2
#define SPI_OVR -3
#define SPI_CRCERR -4
#define SPI_TIMEOUT -5


/*这里我犯的错误 如果是私有辅助api 不应该在头文件里声明 不然别人include了这个声明 会去他自己.c里找 找不到定义*/
/*公共api才要在头文件声明 而且不应该带static*/
/*公共 API（SPI_Write/SPI_Read）	头文件里声明,不带static	 .c里定义,不带 static
  私有辅助（SPI_Wait_Flag）	        只在.c 里声明	         .c里定义,带 static*/
int8_t SPI_Init(SPI_TypeDef *spi);

int8_t SPI_Write(SPI_TypeDef *spi,const uint8_t* buf, uint16_t len);
int8_t SPI_Read(SPI_TypeDef *spi, uint8_t* buf, uint16_t len);
#endif
