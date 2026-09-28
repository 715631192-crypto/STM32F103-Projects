# -*- coding: utf-8 -*-
"""
生成 docs/嵌入式知识大总结.docx
把本项目（STM32F103 智能门锁）涉及到的嵌入式知识点分条列出。

每条知识点统一三段式：
    原理：xxx
    本项目：xxx（落到具体文件 / 函数 / 场景）
    要点：xxx（坑、注意事项）
"""
import os
from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.section import WD_ORIENT
from docx.oxml.ns import qn

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "docs", "嵌入式知识大总结.docx")

FONT = "微软雅黑"
ACCENT = RGBColor(0x1F, 0x6F, 0xB2)
DARK = RGBColor(0x12, 0x21, 0x33)
GREY = RGBColor(0x5A, 0x6B, 0x7B)
WARN = RGBColor(0xC0, 0x39, 0x2B)

# ---------------------------------------------------------------- 内容
# (章节名, [(知识点, 原理, 本项目用法, 要点), ...])
CHAPTERS = [
("一、RTOS 与并发（FreeRTOS）", [
 ("抢占式调度（Preemptive Scheduling）",
  "高优先级任务一旦就绪，立即剥夺当前低优先级任务的 CPU；调度发生在 SysTick 节拍与任务主动让出时。",
  "configUSE_PREEMPTION=1，1ms 节拍（configTICK_RATE_HZ=1000）。access_task(优先级4) 能在刷卡瞬间立刻抢到 CPU，保证开锁判定不被日志/刷屏拖慢。",
  "抢占是内核做的；任务自己只会让出，不会抢。写任务时要保证每个任务都有让出点，否则会饿死低优先级任务。"),

 ("优先级与时间片轮转",
  "同优先级的多个就绪任务按时间片轮流占用 CPU；不同优先级则高者独占。",
  "5 个任务优先级：access(4) > input(3) > network(2) > log/ui(1)。log 与 ui 同为 1，靠 configUSE_TIME_SLICING=1 轮转。",
  "优先级不是越高越好：给太多任务高优先级会让低优先级长期饿死。本工程只把“必须立刻响应”的判决策任务设为 4。"),

 ("静态任务创建 xTaskCreateStatic",
  "任务控制块 TCB 与栈都由调用方提供静态数组，不从堆分配，编译期即可确定内存占用。",
  "5 个任务全部用 xTaskCreateStatic；栈是文件作用域数组 input_task_stack[256] 等（smart_lock_app.c）。",
  "必须同时实现 vApplicationGetIdleTaskMemory 给空闲任务供内存，否则链接或运行期报错。"),

 ("静态队列创建 xQueueCreateStatic",
  "队列结构体与存储区同样由调用方提供，避免运行期分配失败与内存碎片。",
  "6 个队列全部用 xQueueCreateStatic，存储区为 input_queue_storage[12*sizeof(...)] 等静态数组。",
  "静态存储区的大小 = 队列长度 × 单条消息字节数，改结构体时要同步检查，否则会静默越界。"),

 ("队列的值拷贝语义",
  "xQueueSend 把整个消息**拷贝**进队列存储区，接收方拿到的是副本，而非发送方的缓冲区指针。",
  "所有跨任务消息（board_input_event_t / remote_command_t / event_log_record_t）都按值传递。",
  "好处是零共享、免加锁；代价是大结构体拷贝变慢。本项目单条消息仅几十字节，值拷贝是最优解。"),

 ("阻塞延时 vTaskDelay 与忙等 delay_us 的区别",
  "vTaskDelay 把任务置为阻塞态并让出 CPU；忙等是靠循环消耗 CPU 时间，期间谁也跑不了。",
  "input_task/network_task 用 vTaskDelay；软 I2C、SPI 片选等待等微秒级时序用 DWT 忙等 delay_us。",
  "★ 毫秒级等待一律用 vTaskDelay；微秒级（I2C/SPI 时序）才用忙等。RC522 自愈里 13ms 用的是 vTaskDelay，所以不卡系统。"),

 ("互斥量（Mutex）与优先级继承",
  "互斥量用于保护共享资源；FreeRTOS 的互斥量带优先级继承——低优先级持锁者会被临时抬到等待者的优先级。",
  "I2C 总线锁 s_i2c_mtx 保护 PB8/PB9 上的 OLED 与 DS3231。",
  "优先级继承解决的是“无界优先级反转”；没有它，中优先级任务会插队把高优先级任务无限期挡在外面。"),

 ("递归互斥量（Recursive Mutex）★",
  "允许同一个任务多次获取同一把锁（内部计数），必须等量释放；可避免自身重入造成死锁。",
  "xSemaphoreCreateRecursiveMutex 创建 s_i2c_mtx。因为 oled.c 的 s_oled_mtx 也是递归的，会嵌套进入 I2C 事务层。",
  "★ 锁序恒为 s_oled_mtx → s_i2c_mtx，无环所以不会死锁。若不加递归锁，OLED 内部嵌套调用 I2C 会把自己锁死。"),

 ("锁序与死锁避免",
  "多把锁必须约定统一的获取顺序；只要不存在循环等待，就不会死锁。",
  "本项目只有两层锁（器件层 s_oled_mtx、总线层 s_i2c_mtx），且方向唯一。",
  "新增锁时要先画依赖图。经验：互斥量尽量少、粒度尽量小、持有时间尽量短。"),

 ("栈溢出检测与钩子",
  "FreeRTOS 用固定图案（0xA5A5A5A5）填充未使用的栈，检测到被改写即触发钩子。",
  "configCHECK_FOR_STACK_OVERFLOW=2；vApplicationStackOverflowHook 实现为关中断 + 死循环。",
  "★ 本工程钩子不打印 → 表现为“静默死机”。定位法：map 查栈地址 → ST-Link 热插拔读栈底 16 字节看是否被改写。"),

 ("空闲任务与静态内存供给",
  "开启静态分配后，内核要求应用提供空闲任务的 TCB 与栈。",
  "freertos_hooks.c 的 vApplicationGetIdleTaskMemory 提供 idle_task_control 与 idle_task_stack。",
  "忘了实现会编译不过或运行期崩溃，是静态分配的必选项。"),

 ("内存分配失败钩子",
  "pvPortMalloc 失败时触发，用于捕获堆耗尽。",
  "vApplicationMallocFailedHook 同样实现为死循环；4KB 堆只服务 cJSON/MqttKit/ESP8266 解包。",
  "应用层自身不动态分配，所以一旦触发基本可定位为第三方库或报文异常。"),

 ("heap_4 与内存碎片",
  "heap_4 会把相邻空闲块合并，缓解碎片；堆大小需按实际峰值用量计算。",
  "configTOTAL_HEAP_SIZE=4KB。照抄参考工程的 16KB 会直接链接失败（L6406E）。",
  "★ 20KB RAM 已被 MSP 栈 + 静态区 + 驱动缓冲占掉约 11KB，4KB 是算出来的余量，不是拍脑袋。"),

 ("中断优先级与 FromISR 边界",
  "只有优先级数值 ≥ configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断才能调用带 FromISR 的 RTOS API。",
  "configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5；本工程 ISR 只置 volatile 标志，不碰 RTOS 对象。",
  "在 ISR 里调用非 FromISR 的 API 会直接触发断言并死循环。"),

 ("临界区与关中断",
  "taskENTER_CRITICAL 关闭受控中断，用于极短的原子操作。",
  "队列内部本身就用了调度锁/临界区保证线程安全，所以业务代码基本不需要自己加。",
  "临界区越长，中断延迟越大；只用于几条指令的原子读改。"),

 ("看门狗（IWDG）与故障恢复",
  "独立看门狗超时未被喂则复位整个芯片，是故障的最后兜底。",
  "main 启动失败死循环等它复位；正常运行时 board_watchdog_refresh 喂狗。栈溢出/断言失败进入死循环后，靠 IWDG 把设备拉回可用态。",
  "不要在中断里喂狗——那样主循环死了也发现不了。"),

 ("软件定时器的取舍",
  "xTimer 系列依赖内核的定时器服务任务，会额外占用栈、TCB 与队列。",
  "configUSE_TIMERS=0（全工程无 xTimer* 调用），省下约 1.2KB RAM 给管理菜单。",
  "★ RAM 吃紧时优先关定时器服务，而不是去压缩业务任务栈。"),
]),

("二、MCU 与 Cortex-M3 基础", [
 ("时钟树与 PLL",
  "外部晶振经 PLL 倍频得到系统时钟，再分频给 AHB/APB 总线与各外设。",
  "STM32F103 由启动文件 SystemInit 配到 72MHz（HSE × 9）；APB1=36MHz、APB2=72MHz。",
  "改系统时钟必须同步改 UART 波特率、SPI 分频、SysTick —— 串口乱码第一查项就是时钟。"),

 ("SysTick 系统节拍",
  "Cortex-M 内核自带的 24 位倒计时定时器，通常作为 RTOS 心跳。",
  "configTICK_RATE_HZ=1000 → 1ms 一次节拍中断，vTaskDelay 的精度即 1ms。",
  "FreeRTOS 接管了 SysTick_Handler，启动文件里不能重复定义该向量。"),

 ("NVIC 中断优先级分组",
  "Cortex-M3 用 4 位表示优先级，可拆分为抢占优先级与子优先级。",
  "configPRIO_BITS=4，configLIBRARY_LOWEST_INTERRUPT_PRIORITY=15。",
  "分组方式必须在系统初始化时统一设置一次，中途改动会导致优先级解释混乱。"),

 ("启动文件与分散加载（scatter file）",
  "启动文件负责初始化栈指针、数据段、BSS，再跳转到 main；分散加载文件决定各段在 Flash/RAM 的布局。",
  "startup_stm32f10x_hd/md.s + STM32F103C8.sct；启动文件已划走 1KB MSP 栈。",
  "链接报 No space in execution regions 时，要回头看 MSP 栈与静态区占了多少。"),

 ("GPIO 的八种工作模式",
  "输入（浮空/上拉/下拉/模拟）与输出（推挽/开漏/复用推挽/复用开漏），各有适用场景。",
  "SPI 的 SCK/MOSI 用复用推挽、MISO 用浮空输入；I2C 的 SCL/SDA 必须开漏 + 外部上拉；CS 用推挽软件控制。",
  "★ 开漏是 I2C 能“线与”和双向通信的前提；推挽会烧毁总线或导致无法仲裁。"),

 ("引脚重映射（AFIO）与 JTAG 释放",
  "部分引脚默认被调试接口占用，需通过 AFIO 重映射释放为普通 GPIO。",
  "PA15、PB3、PB4 默认 JTAG，需 GPIO_Remap_SWJ_JTAGDisable 才能用作键盘行列。",
  "★ 忘了释放会表现为“键盘完全没反应”，且调试下载仍正常，极易误判为软件问题。"),

 ("EXTI 外部中断",
  "外部引脚的电平/边沿变化可触发中断，用于按键唤醒等场景。",
  "PA8 唤醒键配置为 EXTI8；中断里只置 volatile 标志，真正的唤醒动作放到任务里做。",
  "ISR 里不做耗时操作、不碰 RTOS 对象，这是实时性的基本纪律。"),

 ("定时器与 PWM 输出",
  "定时器输出比较可产生固定频率、可调占空比的方波。",
  "TIM3_CH3（PB0）输出 50Hz PWM 驱动 SG90 舵机，0°=锁、90°=开。",
  "舵机看的是脉宽（0.5~2.5ms）而非占空比，50Hz 只是载波周期。"),

 ("复位源与启动模式",
  "复位可能来自上电、看门狗、软件、引脚；BOOT0/BOOT1 决定启动区域。",
  "PB2（BOOT1）在本板悬空；-rst 只复位 MCU，ESP8266 需单独断电或 AT+RST。",
  "烧录后模块状态不复位是常见误解，表现为“MCU 重启了但网络没重连”。"),
]),

("三、通信总线与时序", [
 ("UART 异步串行通信",
  "双方约定波特率，靠起始位/停止位同步，无时钟线，全双工。",
  "USART1=ESP8266、USART2=AS608 指纹、USART3=调试打印，均为 115200 8N1。",
  "三路串口波特率不同源误差要小；乱码优先查时钟配置与波特率寄存器。"),

 ("环形缓冲（Ring Buffer）",
  "用 head/tail 双指针在定长数组上实现先进先出，天然适配中断写入、任务读取。",
  "SYSTEM/usart.c 的串口接收缓冲：ISR 只写缓冲，任务侧再取走解析。",
  "满/空的判定要留一个位置或用计数，否则会出现“满和空状态相同”的经典 bug。"),

 ("SPI 同步串行外设接口",
  "四线全双工（SCK/MOSI/MISO/CS），靠 CPOL/CPHA 定义四种模式，主从由硬件 CS 或软件 GPIO 控制。",
  "RC522 挂在 SPI1，72MHz/16=4.5MHz，模式 0，MSB 在前，CS 用 PA4 软件控制。",
  "★ 模式（CPOL/CPHA）必须与从器件一致，否则数据整体偏移一位。"),

 ("I2C 两线串行总线",
  "SCL/SDA 均为开漏 + 上拉，支持多主多从、线与仲裁，靠地址寻址。",
  "软件 I2C 在 PB8/PB9 上挂了 OLED(0x78) 与 DS3231(0xD0) 两个从器件。",
  "地址是 7 位，很多资料给的是“写地址”，左移一位后最低位表示读写。"),

 ("软件 I2C（Bit-bang）",
  "用 GPIO 翻转模拟 I2C 时序，牺牲 CPU 换引脚灵活性与规避硬件缺陷。",
  "HARDWARE/i2c_soft.c 用延时循环产生 SCL 波形，实现 START/STOP/ACK/字节收发。",
  "★ 一次事务由几十次翻转组成，中途被抢占就会撕裂 → 必须用互斥量保护整段事务。"),

 ("重复起始条件（Repeated Start）",
  "在不发 STOP 的情况下重新发 START，用于“写寄存器地址后立即读”的原子操作。",
  "IIC_ReadRegs 里写寄存器地址后不发 STOP，直接重发起始条件切换到读。",
  "若中间发了 STOP，从器件可能释放总线或复位内部地址指针，导致读到错误数据。"),

 ("上拉电阻与总线电容",
  "开漏总线依靠上拉电阻回到高电平，阻值与总线电容共同决定上升沿速度。",
  "SCL/SDA 需外部上拉；速率越高上拉要越小（典型 4.7kΩ~10kΩ，按速率与节点数取舍）。",
  "上拉过大 → 上升沿过缓、高速通信失败；过小 → 功耗与灌电流超标。"),

 ("波特率与时钟分频",
  "通信速率由外设时钟经分频得到，必须是双方都能容忍的误差范围内。",
  "SPI 用 SPI_BaudRatePrescaler_16；USART 由库按 APB 时钟与波特率算分频。",
  "改系统时钟后所有分频都要重算，否则通信全乱。"),

 ("总线并发与事务原子性",
  "同一总线上多个器件被不同任务访问时，必须保证单次事务不被打断。",
  "★ 这正是本项目“屏幕花屏”的根因：I2C 事务被高优先级任务抢断，两条事务交织。",
  "通用结论：共享总线的互斥必须加在总线/事务层，而不是某个器件的驱动层。"),
]),

("四、存储与数据完整性", [
 ("NOR Flash 的先擦后写",
  "Flash 只能把位从 1 写成 0，要恢复成 1 必须整块擦除，所以写前必须擦。",
  "W25Q64 写配置/日志前先发 0x20 扇区擦除，再发 0x02 页编程。",
  "擦除是有寿命的（约 10 万次），频繁改写同一扇区会提前写坏。"),

 ("扇区 / 页 / 块的概念",
  "擦除以扇区（4KB）为最小单位，编程以页（256B）为单位，读写要对齐边界。",
  "本工程一个配置槽 = 一个 4KiB 扇区，一条日志记录也占一个扇区（空间换简单）。",
  "跨页写入要分页循环，不能指望硬件自动跨页。"),

 ("掉电安全的 A/B 双槽",
  "两份存储轮流写，永远写“非活动”的那一份，写成功前旧数据始终可读。",
  "config_store：A 槽 0x000000 / B 槽 0x001000，带序号与 CRC；读时取 CRC 正确且序号较大的那份。",
  "★ 这是“擦写途中断电也不丢配置”的核心，比任何写后校验都可靠。"),

 ("CRC32 循环冗余校验",
  "用生成多项式对数据做除法取余，可检出绝大多数突发错误。",
  "event_log 与 config_store 都用 CRC32（多项式 0xEDB88320）校验记录完整性。",
  "CRC 只能检错、不能防篡改（无密钥）；防篡改要用 HMAC 或签名。"),

 ("磨损均衡（环形日志）",
  "把写入轮流分摊到多个扇区，避免单一扇区被反复擦写。",
  "event_log 用 128 个扇区做环形写，写完一圈回到起点。",
  "环形虽简单，但热点数据（如频繁改的配置）仍需 A/B 槽这类专门策略。"),

 ("Magic + Version 信封格式",
  "在数据开头放魔数与版本号，读取时先校验，防止把无效/旧格式数据当有效数据解析。",
  "配置信封：magic=SLC1(0x31434C53) + version + payload_length + sequence + payload + crc32。",
  "★ 升级固件改了结构体却没升版本号，会导致旧数据被误解析 —— 版本号是兼容性的生命线。"),

 ("序号回绕的正确比较",
  "32 位序号迟早回绕，直接比较大小会在回绕点出错。",
  "用 (int32_t)(a - b) >= 0 判断 a 是否不旧于 b，利用有符号溢出特性正确处理回绕。",
  "这是嵌入式里非常经典的一个“看似正确实则会在运行很久后爆雷”的细节。"),
]),

("五、传感器、执行器与人机交互", [
 ("RC522 与 RFID / Mifare",
  "13.56MHz 非接触式射频识别，读卡器发 REQA，卡回 ATQA，随后防冲撞取 UID。",
  "SPI1 驱动 RC522，单轮重试 3 次寻卡，读到后主动 HALT 让卡回到 IDLE。",
  "★ RC522 只能读 13.56MHz 卡，125kHz 的 ID 卡物理上读不到，别搞混。"),

 ("防冲撞与 HALT",
  "多张卡同时在场时要按 UID 分层选择；读完发 HALT 让卡回到待机可再次被寻到。",
  "read_card_uid 里 MFRC522_Anticoll 取 UID，成功后 MFRC522_Halt。",
  "★ 不加 HALT 会“读一次再也读不到，必须把卡拿开重放” —— 这是最常见的读卡体验 bug。"),

 ("射频增益与天线匹配",
  "接收增益决定弱耦合/远距离时能否正确解调卡片的回包。",
  "RFCfgReg 由复位值 0x48(33dB) 改为 0x70(48dB)；无卡等待超时由 30 改为 10。",
  "增益拉满在单卡场景无副作用，多卡场景可能互相干扰 —— 门锁场景恰好只有一张卡。"),

 ("AS608 指纹模块",
  "独立的光学指纹模块，内部完成图像采集、特征提取、比对，通过串口与主机交互。",
  "USART2 通信；用户页 1~47，48=管理员，49=胁迫指纹；指令含 GetImage/GenChar/Search 等。",
  "★ 指纹存在模块内部，擦 MCU Flash 清不掉；这也意味着模块本身是独立攻击面。"),

 ("DS3231 高精度 RTC",
  "带温度补偿的实时时钟芯片，靠 VBAT 纽扣电池在断电后继续走时。",
  "I2C 地址 0xD0，芯片内存 UTC；显示/输入用北京时间（DS3231_LOCAL_TZ_HOURS=8）。",
  "★ 电池缺失会导致断电后时间跳回编译时刻 → TOTP 全部失效。日志出现 soft clock 即异常。"),

 ("BCD 编码与 Unix 时间戳",
  "RTC 芯片寄存器多用 BCD 存年月日时分秒；业务层用 Unix 时间戳更方便计算。",
  "DS3231 驱动做 BCD↔二进制转换，对外提供 Unix 时间戳接口。",
  "时区只应在显示/输入层换算，存储与计算一律用 UTC，避免夏令时与跨时区混乱。"),

 ("OLED（SSD1306）与字库",
  "单色点阵屏，按页/列寻址；显示内容来自字模数组。",
  "I2C 地址 0x78；字库 oledfont_cn.h 由 tools/gen_oled_font.py 维护，收录 73 个常用汉字。",
  "★ 每行显示完要补满剩余像素，否则上一屏更长的文字会留下残像；Init 后必须立即 Clear。"),

 ("矩阵键盘与消抖",
  "行列扫描定位按键；机械触点闭合会抖动，需多次采样确认稳定。",
  "4×4 矩阵，行输出低电平、列上拉读回；KEYPAD_DEBOUNCE_MS=25，靠 10ms 节拍采样。",
  "10ms 不是随手定的：它是消抖 25ms 的约数，也是蜂鸣/RGB 处理的公约数。"),

 ("舵机控制（PWM 角度映射）",
  "舵机按 PWM 脉宽（非占空比）决定转角，典型 0.5~2.5ms 对应 0°~180°。",
  "TIM3_CH3 输出 50Hz；SERVO_LOCK_ANGLE=0、SERVO_UNLOCK_ANGLE=90。",
  "舵机是感性负载，启动电流大；上电抖一下属正常，但不应有大幅扫舵（本项目已删除自检扫舵）。"),
]),

("六、网络与物联网", [
 ("ESP8266 与 AT 指令",
  "WiFi 模组以串口 AT 指令与主机交互，主机侧做状态机驱动。",
  "app_wifi.c 状态机：STEP_AT → STEP_CHECK → STEP_JOIN → STEP_MQTT → STEP_SUB → STEP_RUN。",
  "★ 模组固件崩溃会进入死循环（Fatal exception），此时 MCU 无能为力，需重刷固件或换模块。"),

 ("MQTT 发布/订阅",
  "轻量级的发布订阅消息协议，基于 TCP，支持 QoS 等级与保活。",
  "连接 mqtts.heclouds.com:1883；CONNECT 报文含协议名、级别、连接标志、保活时间。",
  "保活时间要小于服务端超时；断线后要带退避重连，避免疯狂重连打爆网络。"),

 ("JSON 与 cJSON",
  "轻量级数据交换格式；嵌入式常用 cJSON 做解析与组包。",
  "MIDDLEWARE/cJSON.c 解析云端下行命令、组上行的物模型报文。",
  "★ cJSON 用 pvPortMalloc，是本项目 4KB 动态堆的主要消费者；解析完必须及时 Delete 释放。"),

 ("OneNET 物模型与 Token 鉴权",
  "平台侧定义属性/事件；设备用带过期时间的 token 做鉴权，而非固定密码。",
  "token = base64(HMAC-SHA1(key, 待签串)) + version/res/et/method/sign 参数。",
  "token 里的 et 是绝对过期时间 → 依赖设备时钟准确，时钟跑偏会直接鉴权失败。"),

 ("HMAC-SHA1 与 Base64",
  "HMAC 用密钥与消息生成认证摘要；Base64 把二进制编码为可打印字符。",
  "OneNET token 签名与凭据标签都用到；app/sha1.c、base64.c 提供实现。",
  "HMAC 证明“持有密钥且消息未改”；Base64 只是编码，不是加密。"),

 ("断线重连与退避",
  "网络异常时按指数或固定间隔退避重试，避免风暴。",
  "network_task 每 50ms 一轮，连接失败带 retry_delay_ms 重试。",
  "没有退避的重连会让模块在弱网下反复重启，反而更难恢复。"),
]),

 ("七、安全与密码学应用", [
 ("常数时间比较（时序侧信道）",
  "比较耗时若随“匹配前缀长度”变化，攻击者可借响应耗时逐字节爆破。",
  "lock_constant_time_equal 把差异 OR 进一个字节，比完全部再判断，耗时恒定。",
  "★ 绝不能用 strcmp/memcmp 比较密码或 MAC —— 它们的提前返回正是侧信道来源。"),

 ("凭据存 HMAC 而非明文",
  "存密钥派生值（HMAC 标签）而不是明文口令，即使读出存储也无法直接使用。",
  "device_secret 参与 HMAC 计算，比对时用常数时间比较标签。",
  "本工程 device_secret 仍明文存放在 Flash（无安全芯片），能读 Flash 仍可离线计算 —— 属已知取舍。"),

 ("虚拟前后缀与胁迫机制",
  "允许在正确口令前后附加任意数字也算通过，从而实现“表面正常、后台报警”的胁迫码。",
  "pin_matches 允许 allow_virtual 时滑动匹配子串；胁迫码命中走 grant_duress。",
  "内层比较仍要常数时间，不能因为“多试几个起始位置”就引入提前退出。"),

 ("TOTP / HOTP 动态口令",
  "HOTP = HMAC 的一次性口令，TOTP 把计数器换成时间步；再做动态截断取 N 位。",
  "hotp_generate 按 RFC4226 动态截断；totp_verify 用 30s 步长、6 位、±1 步容差。",
  "★ digest[19]&0x0F 取偏移、首字节 &0x7F 去掉符号位 —— 这两步漏一个就与标准验证器不兼容。"),

 ("时间窗容差与下溢防护",
  "为抵消时钟与网络抖动，允许校验前后若干个时间步；但计数器回绕/为负要防护。",
  "totp_verify 遍历 [base-w, base+w]；delta<0 且 base 不够减时跳过，避免绕成天文数字。",
  "★ 无符号减法下溢是这类代码最隐蔽的 bug：uint64 减成巨大值会导致校验异常通过或崩溃。"),

 ("双因子认证（2FA）",
  "要求两类不同类型的凭据同时满足，显著降低单一凭据泄露的风险。",
  "FINGERPRINT_PIN 模式：先刷指纹/刷卡置 pending 第一因子，再输 PIN 才放行。",
  "命中后必须 clear_pending_factor，并给第一因子设超时窗口，防止凭据被后续复用。"),

 ("失败锁定与防暴力破解",
  "连续失败达到阈值后锁定一段时间，把在线爆破变成不可行。",
  "lock_auth_verify_pin：满 3 次失败锁定 60s；锁定期内直接返回 LOCKED。",
  "锁定状态与时间戳要能被持久化或在重启后合理重置，否则重启即可绕过。"),

 ("重放攻击防护",
  "相同命令被重复投递会造成重复动作，需要用唯一标识去重。",
  "handle_remote_command 用 last_accepted_request_id 拒绝重复的 request_id。",
  "request_id 只需记录“最近一次”，不必保存全集 —— 嵌入式要权衡内存与安全。"),

 ("安全状态机（LOCKOUT / ALARM）",
  "把异常态建模为显式状态，进入后拒绝一切开锁请求，直到明确解除。",
  "lock_controller 四态：LOCKED / UNLOCKED / LOCKOUT / ALARM；ALARM 中无视其它事件。",
  "状态机要在最前面处理最高危事件（如 TAMPER），并保证异常态无法被普通事件绕过。"),
]),

("八、软件架构与工程方法", [
 ("分层架构与硬件抽象",
  "按职责把系统分为应用逻辑层、板级适配层、驱动层、内核层，依赖单向向下。",
  "app/（纯逻辑，可 PC 单测）→ stm32/（适配与编排）→ HARDWARE/（驱动）→ FreeRTOS。",
  "收益：换 MCU 只改板级层；认证/锁控这些“最该正确”的代码不依赖硬件，可离线验证。"),

 ("依赖倒置与函数指针表",
  "高层模块定义接口，低层提供实现并以函数指针注入，实现解耦。",
  "event_log_storage_t 用 read/write/erase 函数指针表，让日志模块完全不认识 W25Q64。",
  "换存储介质只需换一张表，不动日志逻辑 —— 这也是可测试性的基础。"),

 ("有限状态机（FSM）",
  "把行为建模为状态 + 事件 + 转移，避免用大量 if/else 拼时序。",
  "lock_controller 是典型 FSM；menu 菜单态机、app_wifi 联网态机同理。",
  "状态机只负责算状态，不直接动硬件 —— 便于测试，也便于审计“为什么变成这个态”。"),

 ("事件驱动与生产者-消费者",
  "采集方只产出事件，处理方按需消费，双方通过队列解耦。",
  "input_task 产出事件入队，access_task 消费并决策；log/ui 再消费下游队列。",
  "队列满要有策略（本工程超时后丢弃 + 上报诊断），不能无限阻塞生产者。"),

 ("单一决策点",
  "所有同类判断集中到唯一入口，避免多处各自决策导致状态不一致。",
  "access_task 是唯一仲裁：本地输入与云端命令都只“发”不“判”。",
  "★ 这从架构上消除了“刷卡与云端命令同时到达 → 双判竞态”这类最难复现的 bug。"),

 ("主机可测试性（Stub + 纯 C）",
  "业务逻辑不依赖硬件时，可在 PC 上用桩件编译并跑断言。",
  "firmware/tests/ 提供 stubs（task.h/queue.h/FreeRTOS.h 等），test_main.c 覆盖 TOTP/base64/config_store/event_log。",
  "把“需要正确性”的代码做成纯函数，是嵌入式里性价比最高的质量手段。"),

 ("防御式编程与断言",
  "对入参、返回值、边界做检查；用断言把“不可能”变成“一旦发生立刻暴露”。",
  "大量 if (ptr == NULL) return；configASSERT 实现为关中断死循环。",
  "断言失败即死循环适合量产（配合看门狗），但调试期建议加打印或断点。"),

 ("版本化与向后兼容",
  "持久化数据带版本号，读取时按版本解析；新增枚举只能追加不能重排。",
  "配置信封带 version；event_type 按 uint8_t 持久化，新值只能追加到末尾。",
  "★ 重排枚举会让已落盘的历史记录被解释成完全不同的含义，且不可逆。"),
]),

("九、调试、构建与工程实践", [
 ("SWD 调试与 ST-Link",
  "两线调试接口（SWDIO/SWCLK），可下载、断点、查看寄存器与内存。",
  "STM32_Programmer_CLI -c port=SWD -w hex -v -rst 完成烧录与校验。",
  "-rst 只复位 MCU，片外模块（ESP8266）不随之复位，这是常见误解。"),

 ("热插拔读实时寄存器",
  "以不复位、不中断运行的方式读取芯片当前寄存器值。",
  "STM32_Programmer_CLI -c port=SWD mode=hotplug -r32 <地址> <长度>。",
  "★ mode=hotplug 必加，否则会复位目标、读到的是复位值而非运行值。"),

 ("printf 重定向与串口日志",
  "把标准输出重定向到串口，是最廉价也最有效的调试手段。",
  "USART3（PB10/PB11）115200 输出启动日志：[OK] MFRC522 / [RTC] DS3231 online / [BOOT] ready。",
  "日志要“一眼能定位问题”：本项目用 [OK]/[ERR]/[RTC] 前缀区分模块与严重级别。"),

 ("编译体积分析（Code/RO/RW/ZI）",
  "Code=代码、RO=只读常量、RW=已初始化变量、ZI=未初始化变量（占 RAM）。",
  "实测 Code=53388 RO=8104 RW=408 ZI=18208；ROM 余量 <4KB。",
  "★ RAM 吃紧时先砍字库等常量，优先保证安全逻辑；改动后务必对比体积变化。"),

 ("中文源码的编码陷阱",
  "非 ASCII 源文件缺少 UTF-8 BOM 时，AC5 会按本地代码页解析导致编译错误。",
  "所有 .c/.h 必须 UTF-8 带 BOM；tools/fix_source_bom.py --check 可批量校验。",
  "★ 典型报错是 #8: missing closing quote，看起来像语法错，实为编码问题。"),

 ("静默死机的定位方法",
  "栈溢出/断言失败进入死循环且不打印，表现为串口输出突然中断。",
  "定位：map 查任务栈地址 → 热插拔读栈底 16 字节 → 看 0xA5A5A5A5 是否被改写 → 反查 map 定位函数。",
  "network_task 栈曾因 MQTT 局部 392B 溢出，现为 768 字。RAM 吃紧优先关定时器服务。"),

 ("上板验收清单",
  "把关键功能固化为可重复执行的检查项，避免“改完觉得没问题”。",
  "清单含：MFRC522/DS3231 日志、密码与锁定、胁迫码、TOTP、长按卡无花屏、连续读卡自愈、自动落锁、菜单改时间。",
  "现场演示时按清单跑一遍，是验证“改动没引入回归”最快的方式。"),
]),

("十、电源、可靠性与抗干扰", [
 ("退耦电容与峰值电流",
  "芯片/模块瞬时大电流会拉垮电源，需在电源脚就近放置退耦电容。",
  "RC522 峰值约 100mA、ESP8266 峰值 300mA+，均建议在 VCC-GND 就近并 100µF + 0.1µF。",
  "退耦电容必须“就近”，放在电源输出端无效 —— 走线电感会抵消其作用。"),

 ("棕色复位（Brownout）",
  "电压短暂跌落到复位门限以下，芯片复位但程序不认为自己重启过，状态错乱。",
  "ESP8266 发射瞬间拉低 3V3 → RC522 复位丢失配置 → 读卡“用一阵就彻底没反应”。",
  "★ 这是本项目最难查的硬件类问题：软件侧只能自愈兜底，根因要靠供电/布局解决。"),

 ("上电时序与复位",
  "各芯片上电/复位有先后顺序要求，尤其是带内部状态机的外设。",
  "RC522 无上电自动清状态机，故 MFRC522_Init 先给 PC14 硬复位脉冲，再做软复位。",
  "★ MCU 复位不等于外设复位；必要时用GPIO 控制外设 RST 引脚主动复位。"),

 ("低功耗 STOP 模式",
  "关掉时钟与大部分外设以省电，靠外部中断唤醒，唤醒后需恢复时钟。",
  "SMART_LOCK_ENABLE_STOP_MODE 默认 0，configUSE_TICKLESS_IDLE=0，暂未启用。",
  "★ 原则：唤醒链路未验证前不进 STOP，否则会“能进不能出”，现场变砖。"),

 ("共地与信号完整性",
  "多模块共地不良或走线过长会引入噪声，表现为通信偶发失败。",
  "SPI/I2C 走线尽量短，模块间单点共地，避免大电流回路与小信号回路重叠。",
  "偶发、难以复现的通信失败，第一嫌疑是电源与地，而不是协议栈。"),

 ("故障自愈设计",
  "承认硬件会出错，在运行期检测并恢复，而不是等人工断电。",
  "RC522 两级自愈：回读配置寄存器判断是否丢失 → 整片重初始化；并周期性无条件刷新兜底。",
  "★ 健康探针必须回读“自己写下去的配置”，不能用只读 ID 寄存器 —— VersionReg 是典型反例。"),
]),
]


# ---------------------------------------------------------------- 生成
def set_font(run, name=FONT, size=10.5, color=DARK, bold=False):
    run.font.name = name
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.color.rgb = color
    rPr = run._r.get_or_add_rPr()
    for tag in ("w:rFonts",):
        pass
    # 同时设置东亚字体，避免中文回退成宋体
    rFonts = rPr.find(qn("w:rFonts"))
    if rFonts is None:
        rFonts = rPr.makeelement(qn("w:rFonts"), {})
        rPr.append(rFonts)
    for attr in ("w:ascii", "w:hAnsi", "w:eastAsia", "w:cs"):
        rFonts.set(qn(attr), name)


def para(doc, text, size=10.5, color=DARK, bold=False, align=None,
         space_before=2, space_after=2, indent=0.0, first_line=0.0):
    p = doc.add_paragraph()
    if align is not None:
        p.alignment = align
    pf = p.paragraph_format
    pf.space_before = Pt(space_before)
    pf.space_after = Pt(space_after)
    if indent:
        pf.left_indent = Cm(indent)
    if first_line:
        pf.first_line_indent = Cm(first_line)
    r = p.add_run(text)
    set_font(r, size=size, color=color, bold=bold)
    return p


def main():
    doc = Document()

    # 页面
    sec = doc.sections[0]
    sec.page_width = Cm(21.0)
    sec.page_height = Cm(29.7)
    sec.left_margin = Cm(2.6)
    sec.right_margin = Cm(2.6)
    sec.top_margin = Cm(2.4)
    sec.bottom_margin = Cm(2.2)

    # 默认样式
    style = doc.styles["Normal"]
    style.font.name = FONT
    style.font.size = Pt(10.5)
    style.element.rPr.rFonts.set(qn("w:eastAsia"), FONT)

    # ---------------- 封面 ----------------
    para(doc, "STM32 智能门锁项目", size=13, color=ACCENT, bold=True,
         align=WD_ALIGN_PARAGRAPH.CENTER, space_before=90, space_after=6)
    para(doc, "嵌入式知识点大总结", size=30, color=DARK, bold=True,
         align=WD_ALIGN_PARAGRAPH.CENTER, space_before=6, space_after=10)
    para(doc, "按知识域分章 · 每条含「原理 / 本项目用法 / 要点」",
         size=12, color=GREY, align=WD_ALIGN_PARAGRAPH.CENTER, space_after=40)

    total = sum(len(items) for _, items in CHAPTERS)
    para(doc, "共 %d 章 · %d 个知识点" % (len(CHAPTERS), total),
         size=13, color=ACCENT, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER,
         space_after=8)
    para(doc, "平台：STM32F103C8 @72MHz · FreeRTOS V10.5.1",
         size=11, color=GREY, align=WD_ALIGN_PARAGRAPH.CENTER, space_after=4)
    para(doc, "覆盖：RTOS / MCU / 总线 / 存储 / 传感器 / 网络 / 安全 / 架构 / 调试 / 可靠性",
         size=10, color=GREY, align=WD_ALIGN_PARAGRAPH.CENTER, space_after=4)

    # ---------------- 目录 ----------------
    doc.add_page_break()
    para(doc, "目 录", size=18, color=DARK, bold=True, space_after=14)
    idx = 0
    for cname, items in CHAPTERS:
        idx += 1
        para(doc, "%s（%d 条）" % (cname, len(items)), size=12,
             color=ACCENT, bold=True, space_before=6, space_after=2)
        for it in items:
            para(doc, "· %s" % it[0], size=10, color=GREY,
                 indent=0.6, space_before=0, space_after=0)

    # ---------------- 正文 ----------------
    n = 0
    for cname, items in CHAPTERS:
        doc.add_page_break()
        para(doc, cname, size=17, color=ACCENT, bold=True, space_after=10)
        for it in items:
            name, principle, usage, tip = it
            n += 1
            para(doc, "%d. %s" % (n, name), size=12.5, color=DARK,
                 bold=True, space_before=10, space_after=3)
            para(doc, "原理：%s" % principle, size=10.5, color=DARK,
                 indent=0.5, space_before=1, space_after=1)
            para(doc, "本项目：%s" % usage, size=10.5, color=DARK,
                 indent=0.5, space_before=1, space_after=1)
            para(doc, "要点：%s" % tip, size=10.5, color=WARN,
                 indent=0.5, space_before=1, space_after=1)

    # ---------------- 结语 ----------------
    doc.add_page_break()
    para(doc, "结语：这份清单怎么用", size=17, color=ACCENT, bold=True, space_after=10)
    for line in [
        "1. 当讲义：每章可独立成课，一条知识点讲 1~3 分钟，合计约 3 小时。",
        "2. 当排查手册：遇到现象先按章节定位（如“读卡失灵” → 第五章 / 第十章）。",
        "3. 当面试清单：每条都来自真实工程决策，能讲清“为什么这么选”比背定义更有说服力。",
        "4. 当重构参考：新增功能时对照第八章的架构原则，避免把逻辑写进驱动层。",
        "5. 持续维护：本项目每修一个坑，都应回到对应条目补一句“要点”。",
    ]:
        para(doc, line, size=11, color=DARK, space_before=4, space_after=4)

    out = os.path.abspath(OUT)
    doc.save(out)
    print("SAVED", out)
    print("chapters =", len(CHAPTERS), " items =", total)


if __name__ == "__main__":
    main()
