#ifndef __ESP8266_H
#define __ESP8266_H

#include <stdint.h>
#include "onenet_token.h"      /* onenet_cred_t 三元组类型 */

/*
 * ESP8266 WiFi 模块驱动（USART1，AT 指令固件）
 * ============================================================================
 * 架构：ESP8266 只当一根"带 IP 的串口网线"（TCP 透传），
 *       MQTT 3.1.1 协议由 STM32 自己用 MqttKit 封包/解包。
 *
 * 为什么不用 AT+MQTT* ？
 *   AT+MQTTUSERCFG / MQTTCONN / MQTTSUB / MQTTPUB 是 ESP-AT V2.0(2019) 才有的指令；
 *   本工程模块固件为 AT version:0.40.0.0 (Aug 8 2015) + SDK 1.3.0，没有这些指令。
 *   而本文件用到的全部指令（AT / CWMODE / CWDHCP / CWJAP / CIPSTART / CIPSEND /
 *   CIPCLOSE）在 2015 年的老固件上就已齐备 → **无需重刷 ESP8266 固件**。
 *
 * 平台：OneNET Studio（设备密钥鉴权，HMAC-SHA1 token）
 *
 * 连接流程（由 app_wifi 状态机驱动）：
 *   1. esp8266_check()          发 AT 确认模块在线
 *   2. esp8266_join_ap()        CWMODE + CWJAP 连路由器
 *   3. esp8266_mqtt_connect()   CIPSTART 开 TCP → MQTT CONNECT → 等 CONNACK
 *   4. esp8266_mqtt_subscribe() MQTT SUBSCRIBE 订阅下行主题
 *   5. esp8266_process()        周期调用：收 +IPD 裸 MQTT 字节 → 解包 → 回调
 * ============================================================================
 */

/* ---------- 用户配置区：WiFi ---------- */
#define ESP8266_WIFI_SSID     "Starry_Wang"          /* WiFi 名称 */
#define ESP8266_WIFI_PASS     "88888888"    /* WiFi 密码 */

/* ---------- 用户配置区：OneNET Studio 三元组 ---------- */
#define ONENET_PRODUCT_ID     "v4nyue7h41"              /* ① ProductID             */
#define ONENET_DEVICE_NAME    "smartlock_001"           /* ② DeviceName (ClientID) */
#define ONENET_DEVICE_SECRET  "QmdPMGFia2c3Ym84UU1ONW5XNXk3WEc0SklHVjdHMVo="
                                                        /* ③ 设备密钥 DeviceSecret (base64) */

/*
 * 服务器：OneNET Studio MQTT 接入点。
 * 参考工程 037-DHT11 实测可用；1883 为明文 TCP（ESP8266 RAM 不足，跑不了 TLS）。
 */
#define ESP8266_MQTT_SERVER   "mqtts.heclouds.com"
#define ESP8266_MQTT_PORT     1883

/* token 过期时间：当前时间 + 365 天 */
#define ONENET_TOKEN_LIFETIME_S   (365UL * 24UL * 3600UL)

/* ---------- OneNET Studio 物模型主题（与参考工程 037-DHT11 对齐） ---------- */
/* 上行：属性上报（APP 端 /thingmodel/query-device-property 直接读这批属性） */
#define ONENET_TOPIC_PROP_POST   "$sys/%s/%s/thing/property/post"
/* 下行：平台/控制台下发的属性设置 */
#define ONENET_TOPIC_PROP_SET    "$sys/%s/%s/thing/property/set"
/* 上行：属性设置应答（收到 set 后必须回，否则控制台显示超时） */
#define ONENET_TOPIC_PROP_REPLY  "$sys/%s/%s/thing/property/set_reply"

/* 下行：物模型「服务调用」（本产品当前未定义服务；先订阅着，日后加服务免改代码） */
#define ONENET_TOPIC_SRV_INVOKE  "$sys/%s/%s/thing/service/+/invoke"

/*
 * 上行：物模型「事件上报」—— 2026-09-11 新增，用来让 APP「日志」页有数据。
 *
 * ★ 为什么必须走事件而不是属性：
 *   APP 的日志页读的是 OneNET /device/event-log（设备事件日志）。
 *   以前固件只上报属性（property/post），一条事件都没发过 ⇒ 该接口永远返回
 *   {"list":null}，APP 上表现为「开锁日志无信息」。
 *
 * ★ 实测确认的两个报文形状（用设备凭证直连 broker 发，平台回 code:200）：
 *   开门事件 door_event   {"params":{"door_event":{"value":{"value":{...}}}}}
 *      ↑ 该事件在物模型里唯一的输出参数名字就叫 value（struct），所以嵌套两层
 *   胁迫场景 duress_scene {"params":{"duress_scene":{"value":{...}}}}
 *      ↑ 输出参数是 type / ts 两个平级字段，只嵌一层
 *   （发错形状平台会回 2308 event param count error / 2409 required value）
 *   另：id 必须是**纯数字**，否则回 2405 msg id invalid。
 */
#define ONENET_TOPIC_EVENT_POST  "$sys/%s/%s/thing/event/post"

/*
 * ★★★ 2026-09-11 定案：本产品是【物模型】产品，cmd 主题族不可订阅 ★★★
 *
 * 用设备凭证直连 broker，逐条读 SUBACK 的返回码（0x80 = 平台拒绝）：
 *
 *   主题                                         SUBACK      结论
 *   ------------------------------------------   ---------   ----------------
 *   $sys/{pid}/{dn}/thing/property/set           0x00        ✅ 可订阅
 *   $sys/{pid}/{dn}/thing/service/+/invoke       0x00        ✅ 可订阅
 *   $sys/{pid}/{dn}/thing/#                      0x00        ✅ 可订阅
 *   $sys/{pid}/{dn}/cmd/#                        ★0x80★     ❌ 平台拒绝
 *   $sys/{pid}/{dn}/cmd/request/+                ★0x80★     ❌ 平台拒绝
 *
 * ⇒ /datapoint/synccmds（走 cmd/request/{cmdid}）对本产品**永久**返回
 *   10500 "device not subscribed"，怎么调都不会通。
 *
 * ⇒ 下行唯一可行通道 = 【物模型属性设置】thing/property/set
 *   （本产品可写属性只有 door_status，bool，access=rw）
 *
 * 下面两个宏保留但**不再订阅**，仅作历史参考；若日后把产品切成
 * 「数据流」协议，cmd 族才会重新可用。
 */
#define ONENET_TOPIC_CMD_DOWN    "$sys/%s/%s/cmd/#"              /* 不可订阅，勿用 */
#define ONENET_TOPIC_CMD_RESP    "$sys/%s/%s/cmd/response/%s"    /* 不可订阅，勿用 */

/* 兼容旧名（app_wifi.c 里的 publish_dp 用） */
#define ONENET_TOPIC_DP_UP       ONENET_TOPIC_PROP_POST

/* 下行消息回调：topic/payload 指向驱动内部缓冲，回调内请勿久存 */
typedef void (*esp8266_msg_cb_t)(const char *topic, const char *payload);

void esp8266_set_msg_cb(esp8266_msg_cb_t cb);
void esp8266_process(void);                       /* 周期调用(50~100ms) */

uint8_t esp8266_check(void);                      /* AT 同步，1=在线      */
uint8_t esp8266_join_ap(void);                    /* 连接 WiFi            */
uint8_t esp8266_mqtt_connect(void);               /* TCP + MQTT CONNECT   */
uint8_t esp8266_mqtt_subscribe(void);             /* 订阅下行主题         */
uint8_t esp8266_mqtt_publish(const char *topic, const char *payload);
uint8_t esp8266_mqtt_connected(void);             /* MQTT 连接状态        */
void    esp8266_mqtt_disconnect(void);            /* 断开 TCP/MQTT        */

/**
 * @brief 取出 OneNET 三元组（仅读指针，不可修改）
 */
const onenet_cred_t *esp8266_onenet_cred(void);

#endif
