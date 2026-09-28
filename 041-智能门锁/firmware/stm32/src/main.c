/*
 * 固件入口。
 *
 * 初始化顺序与参考工程保持一致：具体的引脚、时钟、外设与应用初始化都在
 * smart_lock_app_start() → board_init() 里完成（见 board_port.c）。这里只
 * 负责"启动应用 → 起调度器"两件事，避免两份初始化互相打架。
 *
 * 注意：72 MHz 时钟由启动文件调用的 SystemInit()（system_stm32f10x.c）
 * 完成，本函数不需要再配时钟。
 */

#include "FreeRTOS.h"
#include "task.h"

#include "smart_lock_app.h"

int main(void)
{
    if (!smart_lock_app_start())// 启动应用
    {
        /*
         * 启动失败（Flash 未响应 / 出厂配置写不进去 / 任务创建失败）不可恢复。
         * 停在这里等待独立看门狗复位（SMART_LOCK_ENABLE_IWDG=1 的发布版），
         */
        for (;;) {
        }
    }

    vTaskStartScheduler();// 启动任务调度器

    /* 正常不会返回；只有堆不足导致调度器都没起来才会走到这里 */
    for (;;) {
    }
}
