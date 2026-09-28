#include "sys.h"

/**
 * @brief 系统级初始化
 *        1) NVIC 中断分组为组 2（2 位抢占 + 2 位响应）
 *        2) 使能 AFIO 时钟（引脚重映射必需）
 *        3) 禁用 JTAG、保留 SWD：
 *           释放 PA15(JTDI)、PB3(JTDO)、PB4(NJTRST) 给矩阵键盘
 */
void sys_init(void)
{
    /* 1. 中断分组：整个工程只调用一次 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* 2. AFIO 时钟使能（做 GPIO_PinRemapConfig 前必须打开） */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    /* 3. 只禁用 JTAG，SWD 保留 → 仍可用 SWD 下载/调试
     *    注意：此句必须放在任何 PA15/PB3/PB4 的 GPIO 初始化之前 */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
}
