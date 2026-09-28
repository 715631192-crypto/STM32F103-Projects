#ifndef __MQTTKIT_H
#define __MQTTKIT_H

/*
 * MQTT 3.1.1 报文封包/解包库（MqttKit V1.6，作者：张继瑞 / 中移物联）
 * ============================================================================
 * 本工程用途：让 STM32 自己完成 MQTT 协议，ESP8266 只当一根 TCP 透传网线。
 *
 * 为什么不用 ESP8266 内置的 AT+MQTT* 指令？
 *   AT+MQTTUSERCFG / MQTTCONN / MQTTSUB / MQTTPUB 是 ESP-AT V2.0（2019 年）才有的，
 *   本工程手上的模块固件是 AT version:0.40.0.0 (Aug 8 2015) + SDK 1.3.0，
 *   根本没有这些指令，怎么调都连不上。
 *   而下面这套"MCU 自己封包 + AT+CIPSEND 透传"只用 AT / CWMODE / CWJAP /
 *   CIPSTART / CIPSEND / CIPCLOSE，2015 年的老固件全部支持 → 无需重刷固件。
 *
 * 内存分配：
 *   原版用 malloc/free，但本工程开启 microlib 且 STM32F103C8.sct 未定义
 *   ARM_LIB_HEAP 段，链接期会因缺少 __heap_base/__heap_limit 报错。
 *   这里改接 FreeRTOS heap_4 的 pvPortMalloc/vPortFree
 *   （线程安全、对 NULL 释放安全，且不占用额外静态 RAM）。
 * ============================================================================
 */

#include <stddef.h>     /* size_t（MQTT_DumpLength 用到） */
#include <stdint.h>

#include "FreeRTOS.h"   /* 用 FreeRTOS 堆替代标准库堆 */
#include "task.h"

/* ------------------------- 基础类型（沿用原版命名） ------------------------- */
typedef unsigned char  uint8;
typedef char           int8;
typedef unsigned short uint16;
typedef short          int16;
typedef unsigned int   uint32;
typedef int            int32;
typedef _Bool          uint1;

/* ------------------------- 内存分配接口 ------------------------- */
#define MQTT_MallocBuffer   pvPortMalloc
#define MQTT_FreeBuffer     vPortFree

#define MOSQ_MSB(A)         (uint8)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A)         (uint8)(A & 0x00FF)

/* ------------------------- 内存分配方式标志 ------------------------- */
#define MEM_FLAG_NULL       0
#define MEM_FLAG_ALLOC      1
#define MEM_FLAG_STATIC     2

/**
 * @brief MQTT 报文缓冲结构
 *        _data   ：缓冲区首地址
 *        _len    ：当前已写入字节数
 *        _size   ：缓冲区总容量
 *        _memFlag：内存来源（动态/静态），决定 DeleteBuffer 是否释放
 */
typedef struct Buffer
{
    uint8  *_data;
    uint32  _len;
    uint32  _size;
    uint8   _memFlag;
} MQTT_PACKET_STRUCTURE;

/* ------------------------- MQTT 报文类型（固定头高 4 位） ------------------------- */
enum MqttPacketType
{
    MQTT_PKT_CONNECT     = 1,   /**< 连接请求 */
    MQTT_PKT_CONNACK,           /**< 连接确认 */
    MQTT_PKT_PUBLISH,           /**< 发布消息 */
    MQTT_PKT_PUBACK,            /**< 发布确认（QoS1） */
    MQTT_PKT_PUBREC,            /**< 发布收到（QoS2） */
    MQTT_PKT_PUBREL,            /**< 发布释放（QoS2） */
    MQTT_PKT_PUBCOMP,           /**< 发布完成（QoS2） */
    MQTT_PKT_SUBSCRIBE,         /**< 订阅请求 */
    MQTT_PKT_SUBACK,            /**< 订阅确认 */
    MQTT_PKT_UNSUBSCRIBE,       /**< 取消订阅 */
    MQTT_PKT_UNSUBACK,          /**< 取消订阅确认 */
    MQTT_PKT_PINGREQ,           /**< 心跳请求 */
    MQTT_PKT_PINGRESP,          /**< 心跳响应 */
    MQTT_PKT_DISCONNECT,        /**< 断开连接 */

    MQTT_PKT_CMD                /**< 平台下行命令（旧版 EDP 体系兼容用） */
};

/* ------------------------- MQTT QoS 等级 ------------------------- */
enum MqttQosLevel
{
    MQTT_QOS_LEVEL0,    /**< 最多一次 */
    MQTT_QOS_LEVEL1,    /**< 至少一次 */
    MQTT_QOS_LEVEL2     /**< 只有一次 */
};

/* ------------------------- CONNECT 标志位 ------------------------- */
enum MqttConnectFlag
{
    MQTT_CONNECT_CLEAN_SESSION  = 0x02,
    MQTT_CONNECT_WILL_FLAG      = 0x04,
    MQTT_CONNECT_WILL_QOS0      = 0x00,
    MQTT_CONNECT_WILL_QOS1      = 0x08,
    MQTT_CONNECT_WILL_QOS2      = 0x10,
    MQTT_CONNECT_WILL_RETAIN    = 0x20,
    MQTT_CONNECT_PASSORD        = 0x40,
    MQTT_CONNECT_USER_NAME      = 0x80
};

/* ------------------------- 报文 ID（本工程固定值即可） ------------------------- */
#define MQTT_PUBLISH_ID         10
#define MQTT_SUBSCRIBE_ID       20
#define MQTT_UNSUBSCRIBE_ID     30

/* ------------------------- 接口 ------------------------- */

void  MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *mqttPacket);

uint8 MQTT_UnPacketRecv(uint8 *dataPtr);

uint8 MQTT_PacketConnect(const int8 *user, const int8 *password, const int8 *devid,
                         uint16 cTime, uint1 clean_session, uint1 qos,
                         const int8 *will_topic, const int8 *will_msg, int32 will_retain,
                         MQTT_PACKET_STRUCTURE *mqttPacket);

uint1 MQTT_PacketDisConnect(MQTT_PACKET_STRUCTURE *mqttPacket);

uint8 MQTT_UnPacketConnectAck(uint8 *rev_data);

/* 数据上传：自带 $sys/{pid}/{dn}/thing/property/post 主题 */
uint1 MQTT_PacketSaveData(const int8 *pro_id, const char *dev_name,
                          int16 send_len, int8 *type_bin_head,
                          MQTT_PACKET_STRUCTURE *mqttPacket);

uint1 MQTT_PacketSaveBinData(const int8 *name, int16 file_len,
                             MQTT_PACKET_STRUCTURE *mqttPacket);

uint8 MQTT_UnPacketCmd(uint8 *rev_data, int8 **cmdid, int8 **req, uint16 *req_len);

uint1 MQTT_PacketCmdResp(const int8 *cmdid, const int8 *req,
                         MQTT_PACKET_STRUCTURE *mqttPacket);

uint8 MQTT_PacketSubscribe(uint16 pkt_id, enum MqttQosLevel qos,
                           const int8 *topics[], uint8 topics_cnt,
                           MQTT_PACKET_STRUCTURE *mqttPacket);

uint8 MQTT_UnPacketSubscribe(uint8 *rev_data);

uint8 MQTT_PacketUnSubscribe(uint16 pkt_id, const int8 *topics[], uint8 topics_cnt,
                             MQTT_PACKET_STRUCTURE *mqttPacket);

uint1 MQTT_UnPacketUnSubscribe(uint8 *rev_data);

uint8 MQTT_PacketPublish(uint16 pkt_id, const int8 *topic,
                         const int8 *payload, uint32 payload_len,
                         enum MqttQosLevel qos, int32 retain, int32 own,
                         MQTT_PACKET_STRUCTURE *mqttPacket);

uint8 MQTT_UnPacketPublish(uint8 *rev_data, int8 **topic, uint16 *topic_len,
                           int8 **payload, uint16 *payload_len,
                           uint8 *qos, uint16 *pkt_id);

uint1 MQTT_PacketPublishAck(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);
uint1 MQTT_UnPacketPublishAck(uint8 *rev_data);

uint1 MQTT_PacketPublishRec(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);
uint1 MQTT_UnPacketPublishRec(uint8 *rev_data);

uint1 MQTT_PacketPublishRel(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);
uint1 MQTT_UnPacketPublishRel(uint8 *rev_data, uint16 pkt_id);

uint1 MQTT_PacketPublishComp(uint16 pkt_id, MQTT_PACKET_STRUCTURE *mqttPacket);
uint1 MQTT_UnPacketPublishComp(uint8 *rev_data);

uint1 MQTT_PacketPing(MQTT_PACKET_STRUCTURE *mqttPacket);

#endif /* __MQTTKIT_H */
