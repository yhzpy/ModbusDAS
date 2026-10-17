#include "./BSP/DMA/dma.h"
#include "./SYSTEM/delay/delay.h"
#include "usart.h"

DMA_HandleTypeDef  g_dma_handle;          
extern UART_HandleTypeDef huart1;   



void dma_init(DMA_Channel_TypeDef* DMAx_CHx)
{
    if ((uint32_t)DMAx_CHx > (uint32_t)DMA1_Channel7)    
    {
        __HAL_RCC_DMA2_CLK_ENABLE();                     
    }
    else 
    {
        __HAL_RCC_DMA1_CLK_ENABLE();                     
    }
    
    __HAL_LINKDMA(&huart1, hdmatx, g_dma_handle);          
    

    g_dma_handle.Instance = DMAx_CHx;                             
    g_dma_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;           
    g_dma_handle.Init.PeriphInc = DMA_PINC_DISABLE;                
    g_dma_handle.Init.MemInc = DMA_MINC_ENABLE;                     
    g_dma_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;    
    g_dma_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;      
    g_dma_handle.Init.Mode = DMA_NORMAL;                            
    g_dma_handle.Init.Priority = DMA_PRIORITY_MEDIUM;             

    HAL_DMA_Init(&g_dma_handle);
}
























