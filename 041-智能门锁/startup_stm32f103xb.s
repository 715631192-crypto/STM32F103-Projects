;===========================================================
;  STM32F103C8T6 启动文件（Keil MDK-ARM 汇编）
;  适用于：ARM Compiler 5 / 6 / clang
;  Flash：64KB  |  RAM：20KB
;  说明：精简版，仅含必要的栈/堆初始化与向量表。
;        其余中断向量在 stm32f10x_it.c 中以 weak 形式覆盖，
;        FreeRTOS 接管 SVC/PendSV/SysTick 三个向量。
;===========================================================

Stack_Size      EQU     0x00000400           ; 1KB 栈（任务栈由 FreeRTOS 分配）
Heap_Size       EQU     0x00000000           ; 不用 malloc，heap = 0

                PRESERVE8                    ; 8 字节对齐
                THUMB                        ; Thumb 指令集

;===========================================================
;  RAM 区域：栈（NOINIT 不初始化）
;===========================================================
                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp                                 ; 栈顶地址，初始 SP

;===========================================================
;  RAM 区域：堆（保留给将来的 libc 用）
;===========================================================
                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base                                  ; 堆起始地址
Heap_Mem        SPACE   Heap_Size
__heap_limit                                 ; 堆结束地址

;===========================================================
;  中断向量表（必须放在 Flash 起始位置 0x08000000）
;  FreeRTOS 已接管：SVC_Handler / PendSV_Handler / SysTick_Handler
;===========================================================
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors       DCD     __initial_sp           ; 0x00  初始栈指针
                DCD     Reset_Handler          ; 0x04  复位
                DCD     NMI_Handler            ; 0x08  NMI
                DCD     HardFault_Handler      ; 0x0C  硬错
                DCD     MemManage_Handler      ; 0x10  内存管理
                DCD     BusFault_Handler       ; 0x14  总线错
                DCD     UsageFault_Handler     ; 0x18  用法错
                DCD     0                      ; 0x1C  保留
                DCD     0                      ; 0x20  保留
                DCD     0                      ; 0x24  保留
                DCD     0                      ; 0x28  保留
                DCD     SVC_Handler            ; 0x2C  SVCall（FreeRTOS）
                DCD     DebugMon_Handler       ; 0x30  调试监控
                DCD     0                      ; 0x34  保留
                DCD     PendSV_Handler         ; 0x38  PendSV（FreeRTOS）
                DCD     SysTick_Handler        ; 0x3C  SysTick（FreeRTOS）

;---------- 外部中断（STM32F103 中密度 60 个） ----------
                DCD     WWDG_IRQHandler        ; 0x40  窗口看门狗
                DCD     PVD_IRQHandler         ; 0x44  PVD
                DCD     TAMPER_IRQHandler      ; 0x48  侵入检测
                DCD     RTC_IRQHandler         ; 0x4C  RTC
                DCD     FLASH_IRQHandler       ; 0x50  Flash
                DCD     RCC_IRQHandler         ; 0x54  RCC
                DCD     EXTI0_IRQHandler       ; 0x58  EXTI 线0（保留，本工程未用）
                DCD     EXTI1_IRQHandler       ; 0x5C  EXTI 线1（键盘列 PA1 唤醒）
                DCD     EXTI2_IRQHandler       ; 0x60  EXTI 线2
                DCD     EXTI3_IRQHandler       ; 0x64  EXTI 线3
                DCD     EXTI4_IRQHandler       ; 0x68  EXTI 线4
                DCD     DMA1_Channel1_IRQHandler; 0x6C
                DCD     DMA1_Channel2_IRQHandler; 0x70
                DCD     DMA1_Channel3_IRQHandler; 0x74
                DCD     DMA1_Channel4_IRQHandler; 0x78
                DCD     DMA1_Channel5_IRQHandler; 0x7C
                DCD     DMA1_Channel6_IRQHandler; 0x80
                DCD     DMA1_Channel7_IRQHandler; 0x84
                DCD     ADC1_2_IRQHandler       ; 0x88
                DCD     USB_HP_CAN1_TX_IRQHandler; 0x8C
                DCD     USB_LP_CAN1_RX0_IRQHandler; 0x90
                DCD     CAN1_RX1_IRQHandler    ; 0x94
                DCD     CAN1_SCE_IRQHandler    ; 0x98
                DCD     EXTI9_5_IRQHandler     ; 0x9C  EXTI 线9_5（PB9/PA8 键盘列）
                DCD     TIM1_BRK_IRQHandler    ; 0xA0
                DCD     TIM1_UP_IRQHandler     ; 0xA4
                DCD     TIM1_TRG_COM_IRQHandler; 0xA8
                DCD     TIM1_CC_IRQHandler     ; 0xAC
                DCD     TIM2_IRQHandler        ; 0xB0
                DCD     TIM3_IRQHandler        ; 0xB4
                DCD     TIM4_IRQHandler        ; 0xB8
                DCD     I2C1_EV_IRQHandler     ; 0xBC
                DCD     I2C1_ER_IRQHandler     ; 0xC0
                DCD     I2C2_EV_IRQHandler     ; 0xC4
                DCD     I2C2_ER_IRQHandler     ; 0xC8
                DCD     SPI1_IRQHandler         ; 0xCC  MFRC522
                DCD     SPI2_IRQHandler         ; 0xD0  W25Q64
                DCD     USART1_IRQHandler       ; 0xD4  ESP8266
                DCD     USART2_IRQHandler       ; 0xD8  ZW101 指纹
                DCD     USART3_IRQHandler       ; 0xDC  调试串口
                DCD     EXTI15_10_IRQHandler    ; 0xE0  PA15 列/PC14 防撬
                DCD     RTCAlarm_IRQHandler     ; 0xE4
                DCD     USBWakeUp_IRQHandler    ; 0xE8
                DCD     TIM8_BRK_IRQHandler     ; 0xEC
                DCD     TIM8_UP_IRQHandler      ; 0xF0
                DCD     TIM8_TRG_COM_IRQHandler ; 0xF4
                DCD     TIM8_CC_IRQHandler      ; 0xF8
                DCD     ADC3_IRQHandler         ; 0xFC
                DCD     FSMC_IRQHandler         ; 0x100
                DCD     SDIO_IRQHandler         ; 0x104
                DCD     TIM5_IRQHandler         ; 0x108
                DCD     SPI3_IRQHandler         ; 0x10C
                DCD     UART4_IRQHandler        ; 0x110
                DCD     UART5_IRQHandler        ; 0x114
                DCD     TIM6_IRQHandler         ; 0x118
                DCD     TIM7_IRQHandler         ; 0x11C
                DCD     DMA2_Channel1_IRQHandler; 0x120
                DCD     DMA2_Channel2_IRQHandler; 0x124
                DCD     DMA2_Channel3_IRQHandler; 0x128
                DCD     DMA2_Channel4_5_IRQHandler; 0x12C

__Vectors_End

__Vectors_Size  EQU     __Vectors_End - __Vectors

;===========================================================
;  复位处理：跳转到 SystemInit 和 main
;===========================================================
                AREA    |.text|, CODE, READONLY

Reset_Handler   PROC
                EXPORT  Reset_Handler         [WEAK]
                IMPORT  SystemInit
                IMPORT  __main

                LDR     R0, =SystemInit
                BLX     R0
                LDR     R0, =__main
                BX      R0
                ENDP

;===========================================================
;  默认中断处理（weak 符号，用户可在 stm32f10x_it.c 覆盖）
;  FreeRTOS 已接管 SVC/PendSV/SysTick，不要在这里实现
;===========================================================
Default_Handler PROC
                EXPORT  NMI_Handler           [WEAK]
                EXPORT  HardFault_Handler     [WEAK]
                EXPORT  MemManage_Handler     [WEAK]
                EXPORT  BusFault_Handler      [WEAK]
                EXPORT  UsageFault_Handler    [WEAK]
                EXPORT  SVC_Handler           [WEAK]
                EXPORT  DebugMon_Handler      [WEAK]
                EXPORT  PendSV_Handler        [WEAK]
                EXPORT  SysTick_Handler       [WEAK]
                EXPORT  WWDG_IRQHandler           [WEAK]
                EXPORT  PVD_IRQHandler            [WEAK]
                EXPORT  TAMPER_IRQHandler         [WEAK]
                EXPORT  RTC_IRQHandler            [WEAK]
                EXPORT  FLASH_IRQHandler          [WEAK]
                EXPORT  RCC_IRQHandler            [WEAK]
                EXPORT  EXTI0_IRQHandler          [WEAK]
                EXPORT  EXTI1_IRQHandler          [WEAK]
                EXPORT  EXTI2_IRQHandler          [WEAK]
                EXPORT  EXTI3_IRQHandler          [WEAK]
                EXPORT  EXTI4_IRQHandler          [WEAK]
                EXPORT  DMA1_Channel1_IRQHandler  [WEAK]
                EXPORT  DMA1_Channel2_IRQHandler  [WEAK]
                EXPORT  DMA1_Channel3_IRQHandler  [WEAK]
                EXPORT  DMA1_Channel4_IRQHandler  [WEAK]
                EXPORT  DMA1_Channel5_IRQHandler  [WEAK]
                EXPORT  DMA1_Channel6_IRQHandler  [WEAK]
                EXPORT  DMA1_Channel7_IRQHandler  [WEAK]
                EXPORT  ADC1_2_IRQHandler         [WEAK]
                EXPORT  USB_HP_CAN1_TX_IRQHandler [WEAK]
                EXPORT  USB_LP_CAN1_RX0_IRQHandler[WEAK]
                EXPORT  CAN1_RX1_IRQHandler       [WEAK]
                EXPORT  CAN1_SCE_IRQHandler       [WEAK]
                EXPORT  EXTI9_5_IRQHandler        [WEAK]
                EXPORT  TIM1_BRK_IRQHandler       [WEAK]
                EXPORT  TIM1_UP_IRQHandler        [WEAK]
                EXPORT  TIM1_TRG_COM_IRQHandler   [WEAK]
                EXPORT  TIM1_CC_IRQHandler        [WEAK]
                EXPORT  TIM2_IRQHandler           [WEAK]
                EXPORT  TIM3_IRQHandler           [WEAK]
                EXPORT  TIM4_IRQHandler           [WEAK]
                EXPORT  I2C1_EV_IRQHandler        [WEAK]
                EXPORT  I2C1_ER_IRQHandler        [WEAK]
                EXPORT  I2C2_EV_IRQHandler        [WEAK]
                EXPORT  I2C2_ER_IRQHandler        [WEAK]
                EXPORT  SPI1_IRQHandler           [WEAK]
                EXPORT  SPI2_IRQHandler           [WEAK]
                EXPORT  USART1_IRQHandler         [WEAK]
                EXPORT  USART2_IRQHandler         [WEAK]
                EXPORT  USART3_IRQHandler         [WEAK]
                EXPORT  EXTI15_10_IRQHandler      [WEAK]
                EXPORT  RTCAlarm_IRQHandler       [WEAK]
                EXPORT  USBWakeUp_IRQHandler      [WEAK]
                EXPORT  TIM8_BRK_IRQHandler       [WEAK]
                EXPORT  TIM8_UP_IRQHandler        [WEAK]
                EXPORT  TIM8_TRG_COM_IRQHandler   [WEAK]
                EXPORT  TIM8_CC_IRQHandler        [WEAK]
                EXPORT  ADC3_IRQHandler           [WEAK]
                EXPORT  FSMC_IRQHandler           [WEAK]
                EXPORT  SDIO_IRQHandler           [WEAK]
                EXPORT  TIM5_IRQHandler           [WEAK]
                EXPORT  SPI3_IRQHandler           [WEAK]
                EXPORT  UART4_IRQHandler          [WEAK]
                EXPORT  UART5_IRQHandler          [WEAK]
                EXPORT  TIM6_IRQHandler           [WEAK]
                EXPORT  TIM7_IRQHandler           [WEAK]
                EXPORT  DMA2_Channel1_IRQHandler  [WEAK]
                EXPORT  DMA2_Channel2_IRQHandler  [WEAK]
                EXPORT  DMA2_Channel3_IRQHandler  [WEAK]
                EXPORT  DMA2_Channel4_5_IRQHandler[WEAK]

NMI_Handler
HardFault_Handler
MemManage_Handler
BusFault_Handler
UsageFault_Handler
SVC_Handler
DebugMon_Handler
PendSV_Handler
SysTick_Handler
WWDG_IRQHandler
PVD_IRQHandler
TAMPER_IRQHandler
RTC_IRQHandler
FLASH_IRQHandler
RCC_IRQHandler
EXTI0_IRQHandler
EXTI1_IRQHandler
EXTI2_IRQHandler
EXTI3_IRQHandler
EXTI4_IRQHandler
DMA1_Channel1_IRQHandler
DMA1_Channel2_IRQHandler
DMA1_Channel3_IRQHandler
DMA1_Channel4_IRQHandler
DMA1_Channel5_IRQHandler
DMA1_Channel6_IRQHandler
DMA1_Channel7_IRQHandler
ADC1_2_IRQHandler
USB_HP_CAN1_TX_IRQHandler
USB_LP_CAN1_RX0_IRQHandler
CAN1_RX1_IRQHandler
CAN1_SCE_IRQHandler
EXTI9_5_IRQHandler
TIM1_BRK_IRQHandler
TIM1_UP_IRQHandler
TIM1_TRG_COM_IRQHandler
TIM1_CC_IRQHandler
TIM2_IRQHandler
TIM3_IRQHandler
TIM4_IRQHandler
I2C1_EV_IRQHandler
I2C1_ER_IRQHandler
I2C2_EV_IRQHandler
I2C2_ER_IRQHandler
SPI1_IRQHandler
SPI2_IRQHandler
USART1_IRQHandler
USART2_IRQHandler
USART3_IRQHandler
EXTI15_10_IRQHandler
RTCAlarm_IRQHandler
USBWakeUp_IRQHandler
TIM8_BRK_IRQHandler
TIM8_UP_IRQHandler
TIM8_TRG_COM_IRQHandler
TIM8_CC_IRQHandler
ADC3_IRQHandler
FSMC_IRQHandler
SDIO_IRQHandler
TIM5_IRQHandler
SPI3_IRQHandler
UART4_IRQHandler
UART5_IRQHandler
TIM6_IRQHandler
TIM7_IRQHandler
DMA2_Channel1_IRQHandler
DMA2_Channel2_IRQHandler
DMA2_Channel3_IRQHandler
DMA2_Channel4_5_IRQHandler
                B       .
                ENDP

                ALIGN

;===========================================================
;  用户栈/堆初始化（供 C 库使用）
;===========================================================
                IF      :LNOT::DEF:__MICROLIB
                IMPORT  __use_two_region_memory
                EXPORT  __user_initial_stackheap
__user_initial_stackheap
                LDR     R0, =Heap_Mem
                LDR     R1, =(Stack_Mem + Stack_Size)
                LDR     R2, =(Heap_Mem + Heap_Size)
                LDR     R3, =Stack_Mem
                BX      LR
                ELSE
                EXPORT  __initial_sp
                EXPORT  __heap_base
                EXPORT  __heap_limit
                ENDIF

                END
