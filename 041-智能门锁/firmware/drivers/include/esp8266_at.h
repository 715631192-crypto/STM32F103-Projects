/*
 * esp8266_at.h
 *
 * 轻量 MQTT 3.1.1 帧打包/解析器，专为本项目协议而设计：
 *
 *  -  打包 PUBLISH、CONNECT、SUBSCRIBE 三种报文的最小字段集合；
 *  -  从串口字节流中识别已收到的 PUBLISH 报文（QoS 0/1）。
 *
 * 设计动机：
 *  - ESP8266 AT 固件对 MQTT 的支持在不同批次之间表现差异大；
 *  - protocol/mqtt.md 仅需要三种 topic 与两种方向，所以不需要上 Paho。
 *
 * 严格保持 portable：所有上下文由调用方提供；零堆分配。
 * 不修改任何 portable API 或 board_port 协议。
 */
#ifndef SMART_LOCK_ESP8266_AT_H
#define SMART_LOCK_ESP8266_AT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ----- 缓冲与常量 ----- */

#define MQTT_MAX_TOPIC_LENGTH    64U
#define MQTT_MAX_PAYLOAD_LENGTH  256U

/* CONNECT 报文以 Level 4 QoS 为目标设计，仅实现最小字段：
 *  - 协议名 Protocol Name = "MQTT"
 *  - 协议级别 Protocol Level = 4
 *  - 连接标志 Connect Flags  = Clean Session | Password Flag
 *  - 保活时间 Keep Alive    = 60s
 *  - 客户端 ID Client Id     = "<device-id>"
 *  - Password      = device_secret（也作为用户名的占位）
 * 真实设备上若希望匿名 TLS，应将其升级为带 Username/Will 字段的版本。 */
typedef struct {
    char client_id[24];
    char username[24];
    char password[24];
} mqtt_connect_config_t;

/* 解析出来的 PUBLISH 报文。payload 限长由调用方决定的缓冲提供。 */
typedef struct {
    uint16_t packet_id;     /* QoS 0 时为 0 */
    uint8_t qos;
    bool retain;
    bool duplicate;
    char topic[MQTT_MAX_TOPIC_LENGTH];
    size_t topic_length;
    const uint8_t *payload; /* 指向输入缓冲的内部位置 */
    size_t payload_length;
} mqtt_publish_t;

/* 解析 PUBLISH 的累积器。同样避免堆分配。 */
typedef struct {
    enum {
        MQTT_PARSE_HEADER = 0,
        MQTT_PARSE_REMAINING,
        MQTT_PARSE_PAYLOAD
    } state;
    uint8_t fixed_header;
    uint32_t remaining_length;
    uint32_t consumed;
    uint32_t publish_topic_length;
    uint32_t publish_consumed_topic;
    uint32_t publish_consumed_payload;
    uint16_t publish_packet_id;
    mqtt_publish_t publish;
} mqtt_parser_t;

void mqtt_parser_init(mqtt_parser_t *parser);

/* 喂入一个字节。若成功识别出完整 PUBLISH 报文，调用方即可读取
 * parser->publish 的字段。如果发现非 PUBLISH 报文（如 CONNECT、
 * SUBSCRIBE、CONNACK 等控制报文），将返回 false；调用方应当继续
 * 累积后续字节以识别下一个 PUBLISH。 */
bool mqtt_parser_consume(mqtt_parser_t *parser, uint8_t byte);

/* ----- 打包 ----- */

/* 计算 PUBLISH 报文长度（不含固定头 + remaining length 字节），
 * 用于调用方分配缓冲。 */
size_t mqtt_publish_measure(const char *topic, size_t topic_length,
                            const uint8_t *payload, size_t payload_length);

/* 写入 PUBLISH。output 应至少有 mqtt_publish_measure 返回值的容量。 */
bool mqtt_publish_pack(uint8_t *output, size_t output_capacity,
                       const char *topic, size_t topic_length,
                       const uint8_t *payload, size_t payload_length,
                       uint8_t qos);

/* CONNECT 报文：调用方负责把 device_secret 写到 password。 */
size_t mqtt_connect_measure(const mqtt_connect_config_t *config);
bool mqtt_connect_pack(uint8_t *output, size_t output_capacity,
                       const mqtt_connect_config_t *config);

/* SUBSCRIBE 报文：用于接收命令 topic。 */
size_t mqtt_subscribe_measure(const char *topic, size_t topic_length);
bool mqtt_subscribe_pack(uint8_t *output, size_t output_capacity,
                         uint16_t packet_id, const char *topic,
                         size_t topic_length);

#endif
