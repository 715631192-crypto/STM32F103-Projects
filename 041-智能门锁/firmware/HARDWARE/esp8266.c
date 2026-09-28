#include "esp8266.h"
#include "usart.h"
#include "delay.h"
#include "ds3231.h"
#include "onenet_token.h"
#include "MqttKit.h"
#include <string.h>
#include <stdio.h>

/*
 * ESP8266 驱动：AT 指令 + TCP 透传 + STM32 自封 MQTT
 * ============================================================================
 * 与旧实现的根本区别
 * ----------------------------------------------------------------------------
 * 旧实现让 ESP8266 内置 MQTT 客户端跑（AT+MQTTUSERCFG/LONGPASSWORD/MQTTCONN/
 * MQTTSUB/MQTTPUB）。这些指令是 ESP-AT V2.0(2019) 才加入的，而本工程模块固件是
 *      AT 固件版本:0.40.0.0 (Aug 8 2015 14:45:58)
 *      SDK 版本:1.3.0
 * 里面根本没有 AT+MQTT* 指令族 → 无论怎么调都连不上 OneNET。
 *
 * 新实现参照已验证可用的工程（Hanma 037-DHT11）：
 *      ESP8266 只当一根"带 IP 的串口网线"（AT+CIPSTART 开 TCP），
 *      收发的内容是裸 MQTT 3.1.1 报文，由 STM32 用 MqttKit 自己封包/解包。
 * 这套指令（AT / CWMODE / CWDHCP / CWJAP / CIPSTART / CIPSEND / CIPCLOSE）
 * 2015 年的老固件全部支持 → **不用重刷 ESP8266 固件**。
 *
 * 收发模型
 * ----------------------------------------------------------------------------
 * ESP8266 在 TCP 透传下会把收到的网络数据以
 *      \r\n+IPD,<len>:<len 个原始字节>
 * 的形式塞进串口。所以本驱动用两态接收机：
 *      RX_IDLE      ：按行收集 AT 应答（OK / ERROR / CONNECT / SEND OK ...）
 *      RX_IPD_DATA  ：已经看到 "+IPD,<len>:" 头，改为原样收 <len> 个字节
 * 收满后交给 mqtt_on_packet() 解包。
 * 注意 "+IPD" 头的载荷里可能含 \r \n 0x00，绝不能按行解析。
 * ============================================================================
 */

/* 串口调试开关：1=打印每行模块回显 + 每步连接进度，0=静默 */
#define ESP8266_DEBUG_PRINT   1

#if ESP8266_DEBUG_PRINT
#define ESP_DBG(...)  printf(__VA_ARGS__)
#else
#define ESP_DBG(...)  ((void)0)
#endif

/* ---------------- 接收缓冲 ---------------- */

/* AT 应答行缓冲：TCP 方案下 AT 行都很短（WIFI GOT IP / CONNECT / SEND OK / +IPD,123:）
 * 旧实现要 400 字节是因为 +MQTTSUBRECV 一行很长，现在不需要了 → 回收 304 字节 RAM */
#define AT_LINE_MAX      96
static char     s_line[AT_LINE_MAX];
static uint16_t s_pos = 0;

/* MQTT 报文缓冲（+IPD 载荷）。尾部多留 4 字节，便于补 '\0' 让 cJSON 解析。
 * 512 与 USART1 环形缓冲同尺寸：环形缓冲放不下的报文本来就会丢，再大无意义 */
#define MQTT_RX_MAX      512
static uint8_t  s_rx[MQTT_RX_MAX + 4];

/* 接收状态机 */
enum { RX_IDLE = 0, RX_IPD_DATA };
static uint8_t  s_rx_state = RX_IDLE;
static uint16_t s_ipd_need = 0;
static uint16_t s_ipd_got  = 0;

/* ---------------- AT 同步标志 ---------------- */
static volatile uint8_t s_got_ok, s_got_err, s_got_expect;
static char     s_expect[48];

/* at_cmd 重入深度：MQTT 下行回调里会再发指令，用于区分顶层/回调内调用 */
static uint8_t  s_at_depth = 0;

/* ---------------- 连接状态 ---------------- */
static volatile uint8_t s_tcp_open    = 0;   /* AT+CIPSTART 成功 */
static volatile uint8_t s_mqtt_conn   = 0;   /* 收到 CONNACK     */
static volatile uint8_t s_got_connack = 0;
static volatile uint8_t s_got_suback  = 0;
static esp8266_msg_cb_t s_cb = 0;

/* ================= 报文解包 ================= */

/**
 * @brief 处理一条完整的 MQTT 报文（来自 +IPD 载荷）
 * @param buf 报文首地址
 * @param len 报文长度
 */
static void mqtt_on_packet(uint8_t *buf, uint16_t len)
{
    char    topic_c[96];
    char    payload_c[192];
    uint8_t type;

    if (len == 0)
        return;

    buf[len] = 0;                        /* 补零，便于字符串处理 / cJSON 解析 */

    type = MQTT_UnPacketRecv(buf);

    switch (type)
    {
    case MQTT_PKT_CONNACK:
        if (MQTT_UnPacketConnectAck(buf) == 0)
        {
            s_mqtt_conn   = 1;
            s_got_connack = 1;
        }
        else
        {
            /* 返回码非 0：1 协议错误 / 2 非法 clientid / 3 服务器不可用
             *             4 用户名或密码错误 / 5 未授权 */
            ESP_DBG("[MQTT] CONNACK rejected, code=%d\r\n",
                    (int)MQTT_UnPacketConnectAck(buf));
        }
        break;

    case MQTT_PKT_SUBACK:
        if (MQTT_UnPacketSubscribe(buf) == 0)
        {
            s_got_suback = 1;
            ESP_DBG("[MQTT] SUBACK ok\r\n");
        }
        break;

    case MQTT_PKT_PUBLISH:
    {
        /* UnPacketPublish 会分配出 topic/payload 副本，用完必须 free */
        int8   *topic   = NULL;
        int8   *payload = NULL;
        uint16  topic_len = 0, payload_len = 0;
        uint8   qos = 0;
        uint16  pkt_id = 0;

        if (MQTT_UnPacketPublish(buf, &topic, &topic_len,
                                 &payload, &payload_len,
                                 &qos, &pkt_id) == 0)
        {
            ESP_DBG("[MQTT] PUBLISH qos=%u topic_len=%u payload_len=%u\r\n",
                    (unsigned)qos, (unsigned)topic_len, (unsigned)payload_len);

            if (topic && payload && s_cb)
            {
                /* ★ 先拷到本函数栈上再回调。
                 *   回调里 app_wifi 会同步发应答（cmd_ack → esp8266_mqtt_publish
                 *   → at_cmd → esp8266_process 重入）。若不拷贝，
                 *   重入时 s_rx 会被下一条报文覆盖，回调手上的指针就悬空了。 */
                strncpy(topic_c, (const char *)topic, sizeof(topic_c) - 1);
                topic_c[sizeof(topic_c) - 1] = 0;
                strncpy(payload_c, (const char *)payload, sizeof(payload_c) - 1);
                payload_c[sizeof(payload_c) - 1] = 0;

                s_cb(topic_c, payload_c);
            }
        }

        if (topic)   MQTT_FreeBuffer(topic);
        if (payload) MQTT_FreeBuffer(payload);
        break;
    }

    case MQTT_PKT_PUBACK:
        (void)MQTT_UnPacketPublishAck(buf);
        break;

    case MQTT_PKT_PINGRESP:
        ESP_DBG("[MQTT] PINGRESP\r\n");
        break;

    default:
        ESP_DBG("[MQTT] unhandled pkt type=%u\r\n", (unsigned)type);
        break;
    }
}

/* ================= 行解析（AT 应答） ================= */

static void handle_line(char *line)
{
    ESP_DBG("[ESP] %s\r\n", line);      /* 原样回显，看清卡在哪一句 */

    /* expect 判定必须是独立的 if，不能并进下面的 else-if 链：
     * at_cmd("AT","OK",tmo) 的应答恰好是 "OK"，若串成 else-if，
     * 命中 "OK" 分支后链就结束，s_got_expect 永远为 0 → 假超时。 */
    if (s_expect[0] && strstr(line, s_expect))
        s_got_expect = 1;

    if (strcmp(line, "OK") == 0)
        s_got_ok = 1;
    else if (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0 ||
             strncmp(line, "busy", 4) == 0)
        s_got_err = 1;
    else if (strcmp(line, "CLOSED") == 0 ||
             strncmp(line, "CONNECT FAIL", 12) == 0)
    {
        /* ★ TCP 链路已断：OneNET 侧超时踢连接、路由器断网、模块自己复位，
         *   模块都会吐一行 "CLOSED"（或 "CONNECT FAIL"）。
         *
         *   旧实现用 AT+MQTT* 时，模块会主动上报 +MQTTDISCONNECTED；
         *   现在 MQTT 是 STM32 自己封的，模块不知道链路语义，只报 TCP 层状态，
         *   所以必须在这里手动清掉在线标志。
         *   不清的话 app_wifi 的 STEP_RUN 会一直以为在线（假在线）：
         *   不再重连，属性上报全部静默丢失。 */
        s_mqtt_conn = 0;
        s_tcp_open  = 0;
        ESP_DBG("[TCP] link CLOSED, mark offline\r\n");
    }
    else if (strncmp(line, "WIFI DISCONNECT", 15) == 0)
    {
        /* WiFi 掉线，TCP 必然一起断 */
        s_mqtt_conn = 0;
        s_tcp_open  = 0;
    }
}

/**
 * @brief 从 USART1 环形缓冲取字节：AT 应答按行、+IPD 载荷按长度
 *        所有字节消费都从这里走，收发两不误
 */
void esp8266_process(void)
{
    uint8_t b;

    while (usart_read_byte(USART1, &b))
    {
        /* ---------------- 二进制态：正在收 +IPD 载荷 ---------------- */
        if (s_rx_state == RX_IPD_DATA)
        {
            if (s_ipd_got < MQTT_RX_MAX)
                s_rx[s_ipd_got] = b;          /* 超长部分丢弃，只留前 512 */
            s_ipd_got++;

            if (s_ipd_got >= s_ipd_need)
            {
                uint16_t n = (s_ipd_got > MQTT_RX_MAX) ? MQTT_RX_MAX : s_ipd_got;
                s_rx_state = RX_IDLE;
                s_ipd_got  = 0;
                s_ipd_need = 0;
                mqtt_on_packet(s_rx, n);
            }
            continue;
        }

        /* ---------------- 文本态：收 AT 应答 ---------------- */

        /* '>' 是 AT+CIPSEND 的提示符，**不带换行**，按行解析永远等不到，
         * 必须在这里按单字节识别 */
        if (b == '>' && s_expect[0] == '>')
        {
            s_got_expect = 1;
            s_pos = 0;
            continue;
        }

        if (b == '\r')
            continue;

        if (b == '\n')
        {
            if (s_pos > 0)
            {
                s_line[s_pos] = 0;
                s_pos = 0;
                handle_line(s_line);
            }
            continue;
        }

        if (s_pos < AT_LINE_MAX - 1)
            s_line[s_pos++] = (char)b;
        else
            s_pos = 0;                        /* 行太长：丢弃，防溢出 */

        /* 检测 "+IPD,<len>:" 头，命中后切二进制态。
         * 兼容多连接写法 "+IPD,<len>,<link>:" —— 只取第一个数字 */
        if (s_pos >= 5 && memcmp(s_line, "+IPD,", 5) == 0 && b == ':')
        {
            uint16_t n = 0;
            char    *p = s_line + 5;

            while (*p >= '0' && *p <= '9')
            {
                n = (uint16_t)(n * 10 + (uint16_t)(*p - '0'));
                p++;
            }

            s_pos      = 0;
            s_ipd_got  = 0;
            s_ipd_need = n;
            s_rx_state = (n > 0) ? RX_IPD_DATA : RX_IDLE;
        }
    }
}

/* ================= AT 指令同步收发 ================= */

/**
 * @brief 发送 AT 指令并等待应答
 * @param cmd    完整指令（不含 \r\n）
 * @param expect 期待的关键字（NULL 表示等 "OK"）
 * @param tmo    超时 ms
 */
static uint8_t at_cmd(const char *cmd, const char *expect, uint32_t tmo)
{
    uint32_t t0;
    uint8_t  ret;

    s_got_ok = 0; s_got_err = 0; s_got_expect = 0;
    if (expect)
    {
        strncpy(s_expect, expect, sizeof(s_expect) - 1);
        s_expect[sizeof(s_expect) - 1] = 0;
    }
    else
    {
        s_expect[0] = 0;
    }

    /* 发送前清掉上一轮残留应答，否则会出现"假成功"：
     * 例如 AT+CWJAP 结尾那个 OK 留在缓冲里，紧接着的指令会立刻把它
     * 当成自己的应答（2ms 就返回 OK，指令其实还没发完）。
     * 重入调用（下行回调里再发指令）不能清，否则丢掉正在收的后续报文。 */
    if (s_at_depth == 0)
        usart_rx_clear(USART1);
    s_at_depth++;

#if ESP8266_DEBUG_PRINT
    {
        char     head[64];
        uint16_t n = 0;
        while (cmd[n] && n < 56) { head[n] = cmd[n]; n++; }
        if (cmd[n]) { head[n++] = '.'; head[n++] = '.'; head[n++] = '.'; }
        head[n] = 0;
        ESP_DBG("[AT>] %s\r\n", head);// 打印发送指令
    }
#endif

    usart_send_string(USART1, cmd);// 发送指令
    usart_send_string(USART1, "\r\n");// 发送换行符

    t0  = millis();
    ret = 0;
    while ((millis() - t0) < tmo)
    {
        esp8266_process();// 处理接收数据
        if (s_got_err) { ret = 0; break; }
        if (expect ? s_got_expect : s_got_ok) { ret = 1; break; }
    }

    s_at_depth--;

    ESP_DBG("[AT<] %s (%lums)\r\n",
            ret ? "OK" : (s_got_err ? "ERROR" : "TIMEOUT"),
            (unsigned long)(millis() - t0));// 打印应答
    return ret;
}

/**
 * @brief 只等待接收、不发送指令
 *        用于 AT+CIPSEND 这类"先收提示符、再发裸数据"的两段式交互
 */
static uint8_t at_wait(const char *expect, uint32_t tmo)
{
    uint32_t t0;

    s_got_ok = 0; s_got_err = 0; s_got_expect = 0;
    if (expect)
    {
        strncpy(s_expect, expect, sizeof(s_expect) - 1);
        s_expect[sizeof(s_expect) - 1] = 0;
    }
    else
    {
        s_expect[0] = 0;
    }

    t0 = millis();
    while ((millis() - t0) < tmo)
    {
        esp8266_process();
        if (s_got_err)
            return 0;
        if (expect ? s_got_expect : s_got_ok)
            return 1;
    }
    return 0;
}

/* ================= TCP 透传 ================= */

/**
 * @brief 发送一段裸字节（TCP 透传）
 *        流程：AT+CIPSEND=<len> → 等 '>' → 原样发 <len> 字节 → 等 SEND OK
 */
static uint8_t tcp_send(const uint8_t *buf, uint16_t len)
{
    char cmd[32];

    if (!s_tcp_open || len == 0)
        return 0;

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);
    if (!at_cmd(cmd, ">", 3000))
    {
        ESP_DBG("[TCP] no '>' prompt\r\n");
        return 0;
    }

    usart_send_bytes(USART1, buf, len);       /* 裸数据，不加引号/换行 */

    if (!at_wait("SEND OK", 5000))
    {
        ESP_DBG("[TCP] SEND failed\r\n");
        return 0;
    }
    return 1;
}

/* ================= 对外接口 ================= */

void esp8266_set_msg_cb(esp8266_msg_cb_t cb) { s_cb = cb; }

/** @brief AT 握手：确认模块在线（顺便丢弃上电杂音） */
uint8_t esp8266_check(void)
{
    delay_ms(100);
    usart_rx_clear(USART1);
    if (!at_cmd("AT", "OK", 1000))
        return 0;

    /* 顺便读固件版本，确认模块活着。失败不影响握手结果，不检查返回值。
     * 老固件（v0.40.0.0 / SDK 1.3.0）与新固件（ESP-AT v2.x）都能正常走 TCP 方案。 */
    at_cmd("AT+GMR", "OK", 1500);
    return 1;
}

/** @brief 连接 WiFi 路由器（最长等 20 秒） */
uint8_t esp8266_join_ap(void)
{
    char cmd[96];

    /* 已是 Station 模式则回 OK；重复设置也安全 */
    at_cmd("AT+CWMODE=1", "OK", 3000);

    /* 开启 Station DHCP。个别老固件没有这条指令会回 ERROR，
     * 但 DHCP 默认就是开的，所以忽略结果即可。 */
    at_cmd("AT+CWDHCP=1,1", "OK", 3000);

   // 恢复路径新增：已关联 AP（有 IP）就直接返回成功。
    if (at_cmd("AT+CWJAP?", "+CWJAP:", 3000))
    {
        ESP_DBG("[WIFI] already associated (skip CWJAP)\r\n");
        return 1;
    }
    // 连接 AP
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"",
             ESP8266_WIFI_SSID, ESP8266_WIFI_PASS);
    ESP_DBG("[WIFI] join AP ssid=\"%s\" ...\r\n", ESP8266_WIFI_SSID);

    if (!at_cmd(cmd, "WIFI GOT IP", 20000))
    {
        ESP_DBG("[WIFI] join AP FAILED (pwd / weak signal / 5G band / power)\r\n");
        return 0;
    }
    ESP_DBG("[WIFI] GOT IP\r\n");
    return 1;
}

/**
 * @brief 连接 OneNET：开 TCP + 发 MQTT CONNECT + 等 CONNACK
 *        鉴权：HMAC-SHA1 设备密钥 token（username=ProductID, clientId=DeviceName）
 */
uint8_t esp8266_mqtt_connect(void)
{
    char     cmd[192];
    char     token[200];
    uint32_t et;
    uint32_t t0;
    onenet_cred_t cred;
    MQTT_PACKET_STRUCTURE pkt = { NULL, 0, 0, 0 };// MQTT 报文结构体

    /* ---- 1. 算 OneNET token（et 必须是真实 Unix 时间） ---- */
    cred.product_id    = ONENET_PRODUCT_ID;
    cred.device_name   = ONENET_DEVICE_NAME;
    cred.device_secret = ONENET_DEVICE_SECRET;
    {
        DS3231_Time now;
        uint32_t    now_unix;

        DS3231_GetTime(&now);
        now_unix = DS3231_GetUnix(&now);
        et = now_unix + ONENET_TOKEN_LIFETIME_S;

        /* ★ 排错关键：et 应是约 17.8 亿。若打出的是几万/几百万，
         *   说明时钟基准错了（软 RTC 固件久未重烧会落后），服务器会判 token 过期。 */
        ESP_DBG("[MQTT] now=%lu, et=%lu\r\n",
                (unsigned long)now_unix, (unsigned long)et);
    }
    if (!onenet_calc_token(&cred, et, token, sizeof(token)))// 计算 token
    {
        ESP_DBG("[MQTT] token calc FAILED\r\n");
        return 0;
    }
    ESP_DBG("[MQTT] token len=%u res=products/%s/devices/%s\r\n",
            (unsigned)strlen(token), ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);

    /* ---- 2. 开 TCP 连接（若已开先关，避免 ALREADY CONNECTED） ---- */
    /* ★ 2026-09-14 改为无条件先 CIPCLOSE：模块不随 MCU 复位，
     *   上一轮固件留下的 TCP 连接可能还挂在模块里（MCU 侧 s_tcp_open
     *   重启后归 0，永远不会发 CIPCLOSE）→ 单连接槽位被占，
     *   CIPSTART/CIPMUX 全部秒回 ERROR（实测：CIPSTATUS 里能看到那条
     *   僵尸连接 "TCP,183.230.40.96,1883"）。无连接时 CIPCLOSE 回
     *   ERROR，忽略即可。 */
    (void)at_cmd("AT+CIPCLOSE", "OK", 2000);// 关闭 TCP 连接
    s_tcp_open = 0;

    /* 单连接模式（ESP-AT v2.x 在有连接时该命令会 ERROR，故放在 CIPCLOSE 之后） */
    (void)at_cmd("AT+CIPMUX=0", "OK", 3000);// 单连接模式

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d",
             ESP8266_MQTT_SERVER, ESP8266_MQTT_PORT);//组包
    ESP_DBG("[TCP] connect %s:%d ...\r\n", ESP8266_MQTT_SERVER, ESP8266_MQTT_PORT);
    if (!at_cmd(cmd, "CONNECT", 15000))// 开 TCP 连接
    {
        ESP_DBG("[TCP] CIPSTART failed\r\n");
        memset(token, 0, sizeof(token));
        return 0;
    }
    s_tcp_open = 1;
    ESP_DBG("[TCP] connected\r\n");

    /* ---- 3. 封 MQTT CONNECT 报文并透传出去 ---- */
    s_mqtt_conn   = 0;
    s_got_connack = 0;
    if (MQTT_PacketConnect(ONENET_PRODUCT_ID, token, ONENET_DEVICE_NAME,
                           256, 1, MQTT_QOS_LEVEL0,
                           NULL, NULL, 0, &pkt) != 0)// 组包
    {
        ESP_DBG("[MQTT] packet build FAILED\r\n");
        memset(token, 0, sizeof(token));
        return 0;
    }
    ESP_DBG("[MQTT] CONNECT %lu bytes\r\n", (unsigned long)pkt._len);// 打印 CONNECT 报文长度

    if (!tcp_send(pkt._data, (uint16_t)pkt._len))// 发送 CONNECT 报文
    {
        MQTT_DeleteBuffer(&pkt);// 删除 MQTT 报文缓冲区
        s_tcp_open = 0;
        memset(token, 0, sizeof(token));
        return 0;
    }
    MQTT_DeleteBuffer(&pkt);

    memset(token, 0, sizeof(token));   /* 密码用完即清 */

    /* ---- 4. 等 CONNACK（在 mqtt_on_packet 里置 s_got_connack） ---- */
    ESP_DBG("[MQTT] wait CONNACK ...\r\n");
    t0 = millis();
    while ((millis() - t0) < 8000)
    {
        esp8266_process();// 处理 TCP 数据包
        if (s_got_connack)// 若收到 CONNACK 报文
        {
            ESP_DBG("[MQTT] CONNECTED\r\n");
            return 1;
        }
        delay_ms(10);
    }
    ESP_DBG("[MQTT] wait CONNACK timeout - check token/triad/time\r\n");
    return 0;
}

/**
 * @brief 订阅下行主题
 *
 * ★ 两个要点（2026-09-11 定案）：
 *
 * ① 不再订阅 $sys/{pid}/{dn}/cmd/#。
 *    本产品是【物模型】产品，用设备凭证直连 broker 实测 SUBACK：
 *      thing/property/set        -> 0x00 接受
 *      thing/service/+/invoke    -> 0x00 接受
 *      cmd/#                     -> 0x80 ★拒绝★
 *      cmd/request/+             -> 0x80 ★拒绝★
 *    ⇒ /datapoint/synccmds 永久报 10500 "device not subscribed"。
 *    下行改走【属性设置】thing/property/set（本产品可写属性只有 door_status）。
 *
 * ② 改成「一次只订阅一个主题」，堵住 SUBACK 被掩盖的坑。
 *    MqttKit 的 MQTT_UnPacketSubscribe() 只读 rev_data[4]，即**第 1 个主题**
 *    的返回码。一次订阅 N 个主题时，第 2..N 个主题即使全部 0x80（失败）
 *    也会被吞掉，函数照样返回"成功"。
 *    之前正是踩在这里：cmd/# 订阅失败被掩盖 → 设备显示"在线"、平台却认为
 *    没订阅命令 → APP 下发一直 10500，而且日志上一片"订阅成功"，极难定位。
 *    一次一个主题后，rev_data[4] 就是该主题自己的返回码，不再有掩盖。
 */
uint8_t esp8266_mqtt_subscribe(void)
{
    /* 逐条订阅：物模型属性设置（APP 控制走这条） + 服务调用（预留） */
    static const char *const fmt[] = {
        ONENET_TOPIC_PROP_SET,
        ONENET_TOPIC_SRV_INVOKE,
    };// 订阅主题
    char        t[96];
    const int8 *one[1];
    MQTT_PACKET_STRUCTURE pkt = { NULL, 0, 0, 0 };
    uint8_t     i;
    uint32_t    t0;

    if (!s_mqtt_conn)
        return 0;

    for (i = 0; i < (uint8_t)(sizeof(fmt) / sizeof(fmt[0])); i++)
    {
        snprintf(t, sizeof(t), fmt[i], ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);// 格式化主题
        one[0] = t;

        s_got_suback = 0;
        if (MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL1,
                                 one, 1, &pkt) != 0)// 组包
        {
            ESP_DBG("[MQTT] SUBSCRIBE build FAILED\r\n");
            return 0;
        }
        if (!tcp_send(pkt._data, (uint16_t)pkt._len))
        {
            MQTT_DeleteBuffer(&pkt);
            return 0;
        }
        /* DeleteBuffer 会把 _data/_len/_size/_memFlag 全部复位，下一轮可安全复用 */
        MQTT_DeleteBuffer(&pkt);// 删除 MQTT 报文缓冲区

        /* 等 SUBACK；平台偶尔不回也不影响后续发布，超时按失败让上层重试 */
        t0 = millis();
        while ((millis() - t0) < 5000)
        {
            esp8266_process();// 处理 TCP 数据包
            if (s_got_suback)// 若收到 SUBACK 报文
                break;
            delay_ms(10);
        }
        if (!s_got_suback)// 若未收到 SUBACK 报文
        {
            ESP_DBG("[MQTT] SUBACK timeout: %s\r\n", t);
            return 0;
        }
        ESP_DBG("[MQTT] sub ok: %s\r\n", t);
    }
    ESP_DBG("[MQTT] subscribed (all)\r\n");
    return 1;
}

/**
 * @brief 发布消息（QoS0，topic/payload 均为明文，无需转义）
 */
uint8_t esp8266_mqtt_publish(const char *topic, const char *payload)
{
    MQTT_PACKET_STRUCTURE pkt = { NULL, 0, 0, 0 };
    uint8_t ok;

    if (!s_mqtt_conn || !s_tcp_open)
        return 0;

    if (MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, payload,
                           (uint32_t)strlen(payload),
                           MQTT_QOS_LEVEL0, 0, 1, &pkt) != 0)
        return 0;

    ok = tcp_send(pkt._data, (uint16_t)pkt._len);
    MQTT_DeleteBuffer(&pkt);
    return ok;
}

uint8_t esp8266_mqtt_connected(void) { return s_mqtt_conn; }

void esp8266_mqtt_disconnect(void)
{
    MQTT_PACKET_STRUCTURE pkt = { NULL, 0, 0, 0 };

    if (s_tcp_open)
    {
        if (MQTT_PacketDisConnect(&pkt) == 0)
        {
            tcp_send(pkt._data, (uint16_t)pkt._len);
            MQTT_DeleteBuffer(&pkt);
        }
        at_cmd("AT+CIPCLOSE", "OK", 2000);
    }
    s_mqtt_conn = 0;
    s_tcp_open  = 0;
}

/* 对外导出 OneNET 三元组（只读） */
const onenet_cred_t *esp8266_onenet_cred(void)
{
    static const onenet_cred_t c = {
        ONENET_PRODUCT_ID,
        ONENET_DEVICE_NAME,
        ONENET_DEVICE_SECRET,
    };
    return &c;
}
