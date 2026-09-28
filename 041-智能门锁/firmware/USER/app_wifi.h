#ifndef __APP_WIFI_H
#define __APP_WIFI_H

#include <stdint.h>

/*
 * 云端业务模块（OneNET Studio MQTT 物模型）
 * ================================================================
 * 上行：
 *   $sys/{pid}/{dn}/thing/property/post   属性上报
 *     {"id":"1","version":"1.0","params":{"door_status":{"value":true}}}
 *   $sys/{pid}/{dn}/thing/event/post      事件上报（APP 日志页的数据源）
 *     door_event  ：params.door_event.value.value = {type,method,since,reason}
 *     duress_scene：params.duress_scene.value      = {type,ts}
 *
 * 下行：
 *   $sys/{pid}/{dn}/thing/property/set    平台/APP 下发的属性设置
 *   $sys/{pid}/{dn}/thing/service/+/invoke 服务调用（本产品暂未定义服务）
 *
 * ★ 已废弃：cmd/ 主题族。本产品是【物模型】产品，平台拒订 cmd/#（SUBACK 0x80）
 *   ⇒ /datapoint/synccmds 永久 10500，别再往那个方向改。详见 esp8266.h。
 */

void app_wifi_init(void);
void app_wifi_process(void);          /* 100ms 周期调用（连接状态机+心跳） */
uint8_t app_wifi_online(void);        /* 1 = MQTT 已连接                  */

/* 供其他模块调用的推送接口（离线时静默丢弃） */
void app_wifi_push_alarm(uint8_t alarm_type);
void app_wifi_push_status(void);
void app_wifi_push_door(uint8_t door_open);       /* 门状态变化推送    */

/* 物模型「事件」上行 —— APP 日志页读的 /device/event-log 就靠这两条
 * （属性只进属性表，不进事件日志；没有事件 = APP 日志页永远空） */
void app_wifi_push_door_event(const char *type, uint8_t method);  /* "unlock"/"lock" */
void app_wifi_push_duress_event(void);                            /* 胁迫场景        */

/* 通用发布：任意自定义主题 $sys/{pid}/{dn}/<sub_topic>
 * ⚠️ 物模型事件**不要**用它（别自己拼 thing/event/{id}/post，平台会静默丢弃），
 *    事件请用上面两个 app_wifi_push_*_event()。 */
void app_wifi_publish(const char *sub_topic, const char *payload);

#endif
