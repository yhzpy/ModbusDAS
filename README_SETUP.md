# ModbusDAS - Step 0 Setup Guide（全手写版，不用 CubeMX 生成）

## 一、前提：已有 CubeMX 生成的基础设施（不动）

这些是旧工程继承的，**保留原样不碰**：
- USART1 + 板载 CH340 → 调试 printf（Core/Src/usart.c）
- FSMC → LCD 并口（Core/Src/fsmc.c）
- TIM1 → HAL 时基（Core/Src/stm32f1xx_hal_timebase_tim.c）
- TIM4 → 已有（暂不用，后面可给 LVGL tick）
- GPIO → LED/KEY 初始化（Core/Src/gpio.c）

**新增外设全部自己写初始化**，代码放在对应 DRIVER/ 目录下。

---

## 二、手动配置参考表（你自己写代码，这里只给"要设成什么值"）

### I2C1（SHT30 + DS3231 + AT24C32 共享总线）

| 配置项 | 值 | 为什么 |
|--------|------|--------|
| 实例 | I2C1 | PB6/PB7 是 I2C1 复用引脚 |
| 时钟源 | APB1 = 36 MHz | I2C1 挂 APB1 |
| 速率 | 100 kHz (Standard) | 先低速跑通，多设备总线 400k 容易翻车 |
| 寻址 | 7-bit | SHT30=0x44, DS3231=0x68, AT24C32=0x57 |
| GPIO模式 | AF开漏 (GPIO_MODE_AF_OD) | I2C 必须开漏，靠外部上拉拉高 |
| GPIO上拉 | 不启用内部上拉 (NOPULL) | 模块板载 4.7kΩ 外部上拉已够 |
| GPIO速度 | GPIO_SPEED_FREQ_HIGH | 100kHz 够用，但开漏需要快速切换 |
| Own Address | 0（不做从机） | F103 只做主机 |
| Dual Address | 关闭 | 不需要 |
| General Call | 关闭 | 不用广播 |
| No Stretch | 关闭（允许时钟拉伸） | SHT30 测量时会拉低 SCL |
| 中断 | 不开（Step 1 用轮询） | HAL_I2C_Master_Transmit/Receive 阻塞式 |
| HAL MSP | __HAL_RCC_I2C1_CLK_ENABLE() + GPIOB CLK | 使能外设时钟 |

**关键寄存器值（如果你要看 HAL 源码理解）**：
- I2C1->CR2: FREQ = 36（APB1 MHz）
- I2C1->CCR: 180（= 36MHz / (2 × 100kHz) = 180，标准模式）
- I2C1->TRISE: 37（= 36 + 1，标准模式最大上升时间 1000ns）

### USART2 + DMA（RS485 Modbus 通信）

| 配置项 | 值 | 为什么 |
|--------|------|--------|
| 实例 | USART2 | PA2=TX, PA3=RX |
| 波特率 | 115200（初始值） | 运行时可通过 Modbus 寄存器改 |
| 字长 | 8 bit | Modbus RTU 标准 |
| 校验 | None | Modbus RTU 用 CRC 不用奇偶 |
| 停止位 | 1 | 标准 |
| 模式 | TX + RX | 半双工但 UART 本身全双工，方向靠 DE 控制 |
| 硬件流控 | None | RS485 不用 RTS/CTS |
| GPIO PA2 | AF 推挽 (GPIO_MODE_AF_PP) | TX 输出 |
| GPIO PA3 | 浮空输入 (GPIO_MODE_INPUT_FLOATING) | RX 输入（F1 HAL 风格）|
| DMA RX | DMA1_Channel6, Normal模式 | USART2_RX 固定映射 DMA1 Ch6 |
| DMA TX | DMA1_Channel7, Normal模式 | USART2_TX 固定映射 DMA1 Ch7 |
| DMA 数据宽度 | Byte (Periph) / Byte (Mem) | 串口按字节收发 |
| DMA 地址递增 | Mem递增, Periph不递增 | 标准 |
| IDLE 中断 | 开启 | 检测帧结束（最后一个字节后总线空闲） |
| NVIC USART2 | 优先级 13 | 必须 ≥12 才能安全调 FromISR |
| NVIC DMA1_Ch6 | 优先级 13 | DMA RX 完成中断 |
| NVIC DMA1_Ch7 | 优先级 13 | DMA TX 完成中断 |

**DMA 通道映射（F103 固定，不可改）**：
- DMA1 Channel 6 = USART2_RX
- DMA1 Channel 7 = USART2_TX

**IDLE 中断原理**：USART2 收到数据后，如果超过一帧时间（约 1/115200 ≈ 8.7μs × 10bit = 87μs）没有新字节，硬件置位 SR 寄存器 IDLE 位。此时 DMA 已把收到的字节搬进 buffer，你读 DMA CNDTR 寄存器就知道收了几个字节。

### TIM6（Modbus T3.5 字符间隔超时）

| 配置项 | 值 | 为什么 |
|--------|------|--------|
| 时钟源 | APB1 × 2 = 72 MHz | TIM6 挂 APB1，APB1 分频≠1 所以 TIM 时钟 ×2 |
| Prescaler | 71 | 72MHz ÷ (71+1) = 1MHz 计数频率 = 1μs/tick |
| Period (ARR) | 动态 | 见下表 |
| 模式 | One Pulse (OPM) | 单次触发，每次收字节重启定时器 |
| 更新中断 | 开启 | 超时后通知 Task_Modbus |
| NVIC | 优先级 14 | FromISR 安全 |

**T3.5 计算**（Modbus 规范：3.5 个字符时间）：

| 波特率 | 1字符时间 | T3.5 | ARR 值 |
|--------|-----------|------|--------|
| 9600 | 11bit/9600 = 1.146ms | 4.01ms | 4010 |
| 19200 | 0.573ms | 2.00ms | 2005 |
| 38400 | 0.286ms | 1.00ms | 1002 |
| 57600+ | <0.19ms | 固定 1.75ms | 1750 |

注：Modbus 规范规定波特率 > 19200 时 T3.5 固定 1.75ms。

**One Pulse Mode 关键**：普通定时器到 ARR 后自动重装继续计数。OPM 模式计数到 ARR 后**自动停止**（CEN 清零），只产生一次更新事件。每收到新字节时在 ISR 里 `__HAL_TIM_SET_COUNTER(&htim6, 0); __HAL_TIM_ENABLE(&htim6);` 重启，相当于"看门狗喂狗"——超过 T3.5 没有新字节才触发超时。

### PD2（MAX485 DE/RE 方向控制）

| 配置项 | 值 | 为什么 |
|--------|------|--------|
| 模式 | 推挽输出 | 驱动 MAX485 DE 输入 |
| 初始电平 | LOW | 默认接收模式 |
| 速度 | LOW | 不需要快速翻转 |

时序关键：发送完最后一个字节后，必须等 USART2 TC 标志（发送完成）再拉 LOW 切回接收。TXE ≠ TC：TXE 只表示数据寄存器空（可以装下一字节），TC 表示移位寄存器也空了（最后一个 bit 已经出去了）。

### PE0 / PE1 / PE2（EC11 编码器）

| 引脚 | 模式 | 中断 | 说明 |
|------|------|------|------|
| PE0 | 输入上拉 | EXTI0 双边沿 | A 相，触发方向判断 |
| PE1 | 输入上拉 | 无（轮询读取） | B 相，A 边沿时读电平判方向 |
| PE2 | 输入上拉 | 无（轮询消抖） | SW 按键 |

EXTI0 优先级 14。PE0 双边沿中断里读 PE1 电平：A 上升沿 + B 低 = 顺时针，A 上升沿 + B 高 = 逆时针。

---

## 三、Keil 工程配置（这步还是要做的）

### 1. 重命名工程文件
`MDK-ARM/DMA.uvprojx` → `MDK-ARM/ModbusDAS.uvprojx`

### 2. 添加新源文件

Manage Project Items → 新建 Group：

**Group "DRIVER"**：
- `DRIVER/SHT30/sht30.c`
- `DRIVER/DS3231/ds3231.c`
- `DRIVER/MAX485/max485.c`
- `DRIVER/EC11/ec11.c`
- `DRIVER/flash_log.c`

**Group "PROTOCOL"**：
- `PROTOCOL/modbus_rtu.c`

### 3. Include Paths

Options for Target → C/C++ → Include Paths，添加：
```
..\DRIVER\SHT30
..\DRIVER\DS3231
..\DRIVER\MAX485
..\DRIVER\EC11
..\DRIVER
..\PROTOCOL
```

### 4. 编译选项
- Define: `USE_HAL_DRIVER, STM32F103xE`
- C99 Mode: 勾选
- Optimization: -O1（调试阶段）

### 5. HAL 模块使能

打开 `Core/Inc/stm32f1xx_hal_conf.h`，确认以下模块 **没有被注释**：
```c
#define HAL_I2C_MODULE_ENABLED      // SHT30 + DS3231
#define HAL_DMA_MODULE_ENABLED      // USART2 DMA
#define HAL_UART_MODULE_ENABLED     // USART1 + USART2
#define HAL_TIM_MODULE_ENABLED      // TIM1/4/6
#define HAL_GPIO_MODULE_ENABLED     // 全部 GPIO
#define HAL_RCC_MODULE_ENABLED      // 时钟
#define HAL_CORTEX_MODULE_ENABLED   // NVIC
#define HAL_FLASH_MODULE_ENABLED    // W25Q64 间接需要
#define HAL_SRAM_MODULE_ENABLED     // FSMC LCD
```

如果 I2C 或 DMA 被注释了，取消注释即可。

---

## 四、验证判据（Step 0 完成标准）

编译 **零 Error**，烧录后：

1. LCD 白底黑字 "ModbusDAS v0.1" + "Booting FreeRTOS..."
2. 串口 115200 打印：
   ```
   === ModbusDAS Boot OK ===
   FreeRTOS Heap: 20480 bytes, Tasks: 4
   [Sensor] Task started
   [Logger] Task started
   [Modbus] Task started (slave addr=1)
   [GUI] Task started (LVGL placeholder)
   ```
3. LED1 闪烁 500ms（GUI 任务心跳）
4. LED0 不亮（没有 Stack Overflow）
5. 不 HardFault、不卡死

---

## 五、RAM 预算

| 占用 | 大小 | 备注 |
|------|------|------|
| MSP Stack | 4 KB | startup .s |
| C Heap | 1 KB | printf 用 |
| FreeRTOS Heap | 20 KB | 任务栈+队列+信号量 |
| 全局变量 | ~3 KB | g_regMap + counters + buffers |
| **当前合计** | **~28 KB** | |
| Step 5 LVGL Pool | +12 KB | lv_conf.h LV_MEM_SIZE |
| Step 5 Draw Buffer | +10 KB | static array |
| **最终预估** | **~50 KB** | 64KB 余量 14KB |

---

## 六、Step 1 你要做的第一件事

在 `DRIVER/SHT30/sht30.c` 顶部写一个 `I2C1_Init()` 函数：
1. 开 GPIOB + I2C1 时钟
2. 配 PB6/PB7 为 AF 开漏
3. 填 I2C_HandleTypeDef 结构体（参考上表的值）
4. 调 HAL_I2C_Init()
5. 用 HAL_I2C_IsDeviceReady(&hi2c1, SHT30_ADDR<<1, 3, 100) 测试应答

注意 HAL 的 I2C 地址是 **8-bit 格式**（7-bit 左移一位），所以 SHT30 的地址传 `0x44 << 1 = 0x88`。

搞定这个，SHT30 就"在总线上了"。然后你写 SHT30_Read() 发命令、收 6 字节、验 CRC、转公式。
