#ifndef __APP_LINK_H
#define __APP_LINK_H

#include <stdint.h>

/*
 * 智能家居联动模块
 * ================================================================
 * 维护"门当前是否开着"的本地状态，并周期性把 door_status 属性推给云端，
 * 为将来接家庭网关（灯/窗帘/空调）留出触发点。
 *
 * ★ 事件上报已不归本模块管（2026-09-11 修正）：
 *   本模块以前自己 publish 到 `thing/event/{id}/post` —— 主题是错的，
 *   平台静默丢弃，APP 日志永远空。现事件统一由 app_lock.c 调用
 *   app_wifi_push_door_event() / app_wifi_push_duress_event() 上报。
 *
 * 触发源：app_lock_open() 在认证通过后回调
 * 反触发：app_lock_force_close() 上锁时回调 + 5s 保持标记自动超时
 */

typedef enum {
    LINK_EVT_OPEN  = 1,    /* 门正常开锁       */
    LINK_EVT_CLOSE = 2,    /* 门自动上锁       */
    LINK_EVT_DURESS = 3,   /* 胁迫（特殊上报） */
} LinkEvent;

void app_link_init(void);                          /* 在 main 中先于 wifi 调用 */
void app_link_on_event(LinkEvent ev, uint8_t method);/* 应用层回调              */
void app_link_process(void);                       /* 100ms 周期（task_wifi）  */

#endif
