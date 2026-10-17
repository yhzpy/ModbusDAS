/**
 * @file    ec11.h
 * @brief   EC11 Rotary Encoder Driver (EXTI on PE0/PE1, button on PE2)
 *
 * Hardware: EC11 module with built-in pull-ups
 *   A (CLK) -> PE0 -> EXTI0 (independent vector)
 *   B (DT)  -> PE1 -> EXTI1 (independent vector)
 *   SW      -> PE2 -> GPIO input (polled or EXTI2)
 *
 * Decoding: State machine on A edge, read B to determine direction.
 *   A rising + B low  = clockwise  (increment)
 *   A rising + B high = counter-clockwise (decrement)
 *
 * Step 5 implementation target (for LVGL indev).
 */

#ifndef __EC11_H
#define __EC11_H

#include <stdint.h>

/* Pin definitions */
#define EC11_A_PORT     GPIOE
#define EC11_A_PIN      GPIO_PIN_0
#define EC11_B_PORT     GPIOE
#define EC11_B_PIN      GPIO_PIN_1
#define EC11_SW_PORT    GPIOE
#define EC11_SW_PIN     GPIO_PIN_2

/**
 * @brief  Initialize EC11 (GPIO + EXTI + NVIC).
 */
void EC11_Init(void);

/**
 * @brief  Get accumulated encoder count since last call (and reset).
 * @return positive = clockwise, negative = counter-clockwise
 */
int16_t EC11_GetDelta(void);

/**
 * @brief  Check if button is currently pressed (active low with pull-up).
 * @return 1 = pressed, 0 = released
 */
uint8_t EC11_IsPressed(void);

/**
 * @brief  Check for button click event (debounced, consumed on read).
 * @return 1 = click detected since last call, 0 = none
 */
uint8_t EC11_GetClick(void);

/**
 * @brief  EXTI callback - call from HAL_GPIO_EXTI_Callback().
 *         Implements state machine for direction detection.
 */
void EC11_EXTI_Handler(uint16_t GPIO_Pin);

#endif /* __EC11_H */
