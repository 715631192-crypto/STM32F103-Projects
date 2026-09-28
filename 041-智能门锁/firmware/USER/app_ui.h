#ifndef __APP_UI_H
#define __APP_UI_H

#include <stdint.h>

/*
 * OLED 人机界面模块（页面状态机）
 * ================================================================
 * 页面：待机(时钟) → 密码输入 → 结果提示 → 管理菜单(6项)
 *       → 指纹录入/删除、改密码、添卡、日志、设置 等子页面
 *
 * 键盘事件统一由本模块消费；指纹/刷卡在"待机页"由后台任务轮询。
 */

/* app 层 → UI 的事件通知（认证结果、报警等） */
#define UI_EVENT_NONE         0
#define UI_EVENT_OK           1    /* 验证成功           */
#define UI_EVENT_FAIL         2    /* 验证失败           */
#define UI_EVENT_LOCKED       3    /* 进入错误锁定       */
#define UI_EVENT_UNLOCKED     4    /* 锁定解除           */
#define UI_EVENT_COMBO_WAIT   5    /* 双因素：等第二因子 */
#define UI_EVENT_ALARM        6    /* 报警提示           */
#define UI_EVENT_WIFI         7    /* WiFi 状态变化      */
#define UI_EVENT_FAIL_FINGER  8    /* 指纹验证失败       */
#define UI_EVENT_FAIL_CARD    9    /* 卡片无效/未注册    */

void app_ui_init(void);
void app_ui_process(void);                 /* 50ms 周期调用 */
void app_ui_notify(uint8_t event);         /* 其他模块通知  */
uint8_t app_ui_page_idle(void);            /* 1=当前在待机页(允许指纹/刷卡轮询) */

#endif
