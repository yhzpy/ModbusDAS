/**
 * @file    ec11.c
 * @brief   EC11 Rotary Encoder Implementation
 * TODO: Step 5 - implement EXTI state machine + LVGL indev binding.
 */

#include "ec11.h"
#include "FreeRTOS.h"
#include "task.h"

static volatile int16_t s_encoderDelta = 0;
static volatile uint8_t s_lastA = 1;  /* idle high (pull-up) */
static volatile uint8_t s_clickFlag = 0;

void EC11_Init(void)
{
    /* TODO Step 5:
     * 1. PE0, PE1 as input with pull-up (or rely on module's built-in pull-ups)
     * 2. PE2 as input with pull-up (button, active low)
     * 3. Configure EXTI0 on PE0, rising+falling edge
     * 4. Configure EXTI1 on PE1 (optional, for finer resolution)
     * 5. NVIC: EXTI0_IRQn priority 14 (must be > configMAX_SYSCALL_INTERRUPT_PRIORITY)
     *
     * Note: If using FreeRTOS, EXTI ISR must NOT call blocking APIs.
     * Use task notification or ISR-safe queue (xQueueSendFromISR).
     */
}

int16_t EC11_GetDelta(void)
{
    /* Atomic read-and-clear */
    taskENTER_CRITICAL();
    int16_t delta = s_encoderDelta;
    s_encoderDelta = 0;
    taskEXIT_CRITICAL();
    return delta;
}

uint8_t EC11_IsPressed(void)
{
    /* TODO Step 5: return HAL_GPIO_ReadPin(EC11_SW_PORT, EC11_SW_PIN) == GPIO_PIN_RESET; */
    return 0;
}

uint8_t EC11_GetClick(void)
{
    uint8_t clicked = s_clickFlag;
    s_clickFlag = 0;
    return clicked;
}

void EC11_EXTI_Handler(uint16_t GPIO_Pin)
{
    /* TODO Step 5: State machine
     *
     * if (GPIO_Pin == EC11_A_PIN) {
     *     uint8_t currentA = HAL_GPIO_ReadPin(EC11_A_PORT, EC11_A_PIN);
     *     if (currentA != s_lastA) {
     *         // A edge detected, read B to determine direction
     *         uint8_t currentB = HAL_GPIO_ReadPin(EC11_B_PORT, EC11_B_PIN);
     *         if (currentA == GPIO_PIN_SET) {
     *             // Rising edge on A
     *             if (currentB == GPIO_PIN_RESET)
     *                 s_encoderDelta++;   // CW
     *             else
     *                 s_encoderDelta--;   // CCW
     *         }
     *         s_lastA = currentA;
     *     }
     * }
     */
    (void)GPIO_Pin;
}
