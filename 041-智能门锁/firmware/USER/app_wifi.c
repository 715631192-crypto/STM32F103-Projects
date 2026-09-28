#include "app_wifi.h"
#include "app_common.h"
#include "app_alarm.h"
#include "app_auth.h"
#include "app_lock.h"
#include "app_log.h"
#include "app_ui.h"
#include "esp8266.h"
#include "cJSON.h"
#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* ---------------- 连接状态机 ----------------
 * STEP_AT(初始化 AT) → STEP_CHECK(检查模块) → STEP_JOIN(连接 WiFi) → STEP_MQTT(连接 MQTT) → STEP_SUB(订阅主题) → STEP_RUN(正常运行)
 * 每步失败都会退回重试（间隔 RETRY_MS），联网断开自动重连 */
enum { STEP_AT = 0, STEP_CHECK, STEP_JOIN, STEP_MQTT, STEP_SUB, STEP_RUN };

/* 状态名（仅用于串口排错输出） */
static const char *const s_step_name[] =
    { "AT", "CHECK", "JOIN", "MQTT", "SUB", "RUN" };

/*
 * 失败退避：两次联网尝试之间至少间隔这么久。
 * 原来失败后下一轮 100ms 就再来一次（app_wifi_process 的调用周期），
 * 会把 ESP8266 活活打进 wdt 复位死循环（日志里 rst cause:4 + Fatal exception）。
 */
#define WIFI_RETRY_MS  5000

/*
 * AT 握手最小间隔。
 * esp8266_check() 内部是"delay 100ms + AT"，模块正常时一次约 110ms，
 * STEP_CHECK 又按 100ms 周期调用，节奏刚好；但模块回 ERROR/"busy" 时
 * at_cmd 会**秒返回**（实测 4~6ms），于是变成 ~10Hz 疯狂刷 AT
 * （日志里 2 秒打出 20 条 [AT>] AT），只会把模块越搞越乱。
 * 加最小间隔后最多每秒试探一次。
 */
#define WIFI_AT_MIN_GAP_MS  1000

static uint8_t  s_step = STEP_AT;// 当前状态
static uint32_t s_step_t = 0;         /* 本状态开始时刻 */
static uint32_t s_last_beat = 0;      /* 上次心跳时刻   */
static uint32_t s_msg_id   = 0;       /* OneNET 消息 ID 自增 */
static uint32_t s_last_at_try = 0;    /* 上次 AT 握手时刻（限流用） */
static uint8_t  s_conn_fail = 0;      /* MQTT/SUB 连续失败计数：
                                       * ≥3 次回退 STEP_JOIN 重新走 CWJAP。
                                       * 场景：ESP8266 wdt 崩溃重启后丢了 IP，
                                       * CIPSTART 只会回 "no ip"，若永远停在
                                       * STEP_MQTT 重试就永远离线（实测 16:42 日志）。*/

/* 报警类型 → OneNET 物模型字符串（属性标识符：alarm_level）
 * 注意：返回值带 \" 转义，是已 JSON 序列化的字符串字面量，
 *       publish_dp 直接把它当作 JSON 字符串值嵌入即可 */
static const char *alarm_level_str(uint8_t type)
{
    switch (type)
    {
    case ALARM_TAMPER:     return "\"tamper\"";
    case ALARM_DURESS:     return "\"duress\"";       /* 最高级别：胁迫 */
    case ALARM_LOCKOUT:    return "\"lockout\"";
    case ALARM_DOOR_AJAR:  return "\"door_ajar\"";
    default:               return "\"none\"";
    }
}

/* ---------------- 下行消息处理 ---------------- */

/**
 * @brief 命令族应答：$sys/{pid}/{dn}/cmd/response/{cmdid}
 *
 * OneNET 的 /datapoint/synccmds 是「同步」API：
 *   平台把命令投递到 $sys/{pid}/{dn}/cmd/request/{cmdid}，
 *   设备需往      $sys/{pid}/{dn}/cmd/response/{cmdid} 回一条消息。
 * 若不应答，APP 会一直阻塞到 timeout（5~30 秒）才返回超时错误，
 * 虽然命令其实已经执行了，但用户体验很差且无法得知执行结果。
 *
 * 注意：本函数在 MQTT 收包回调中同步发布，内部会阻塞等待 TCP 发送完成
 *      （正常几十毫秒）。esp8266.c 已把回调用的 topic/payload 拷到栈上，
 *      保证回调内重入 esp8266_process() 不会踩到正在接收的报文缓冲。
 *
 * @param topic   收到的下行 topic（含 cmdid）
 * @param result  应答内容（简短字符串，如 "ok" / "locked" / "bad_pwd"）
 */
static void cmd_ack(const char *topic, const char *result)
{
    const char *slash;
    char        resp_topic[128];

    /* 只处理 cmd/request/{cmdid} 这一种主题，其它主题不做应答 */
    if (!topic) return;
    slash = strstr(topic, "/cmd/request/");
    if (!slash) return;

    slash += strlen("/cmd/request/");      /* 指向 cmdid 起始 */
    if (*slash == '\0') return;            /* 空 cmdid，忽略 */

    snprintf(resp_topic, sizeof(resp_topic), ONENET_TOPIC_CMD_RESP,
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME, slash);
    esp8266_mqtt_publish(resp_topic, result);
}

/**
 * @brief 属性设置应答：$sys/{pid}/{dn}/thing/property/set_reply
 *
 * OneNET Studio 控制台/APP 下发属性设置后，设备必须回一条 set_reply，
 * 否则界面上会一直显示"下发中"直到超时。
 * 报文格式：{"id":"<原样回带>","code":200,"msg":"success"}
 *
 * @param id    下行报文里的 id 字段（原样回带，便于平台配对）
 * @param code  200=成功；403=锁定期拒绝；400=报文非法
 * @param msg   简短说明
 */
static void prop_reply(const char *id, int code, const char *msg)
{
    char topic[96];
    char payload[128];

    snprintf(topic, sizeof(topic), ONENET_TOPIC_PROP_REPLY,
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%s\",\"code\":%d,\"msg\":\"%s\"}",
             (id && id[0]) ? id : "0", code, msg);
    esp8266_mqtt_publish(topic, payload);
}

/**
 * @brief MQTT 下行消息回调（esp8266 驱动解包出一条 PUBLISH 后调用）
 *
 * ★ 2026-09-11 定案：本产品是【物模型】产品，cmd 主题族被平台拒绝订阅
 *   （实测 SUBACK=0x80），所以 /datapoint/synccmds 永久 10500，走不通。
 *   ⇒ 真正生效的下行只有【分支 A：物模型属性设置】。
 *
 *  分支 A：「物模型属性设置」（唯一可行通道）
 *   主题 $sys/{pid}/{dn}/thing/property/set
 *   报文 {"id":"1","version":"1.0","params":{"door_status":true}}
 *   ★ 注意：params 里是「标识符: 裸值」，**不包** {"value":...}
 *
 *   本产品可写属性只有 door_status(bool, rw)，语义映射：
 *     door_status = true   → 开锁
 *     door_status = false  → 上锁
 *   另外两条只读属性（lock_status / alarm_level）由设备上报，不可下发。
 *
 *   为日后扩展，这里还认这几个属性（需先在 OneNET 控制台加为"读写"）：
 *     switch         bool   开锁/上锁（加了这个就不用 door_status 顶替）
 *     unlock_lockout bool   true=解除错误锁定
 *     temp_pwd       string 临时密码，"6位数字" 或 "6位数字:有效期秒:是否一次性"
 *
 *   执行完必须回 set_reply，否则 APP 端 set-device-property 会等到 10411 超时。
 *
 *  分支 B：「命令族 cmd」（本产品不可用，保留仅为兼容"数据流"产品）
 *   主题 $sys/{pid}/{dn}/cmd/request/{cmdid}
 *   报文 {"cmd":"unlock"|"lock"|"query"|"temp_pwd"|"unlock_lockout"}
 *   本产品 cmd/# 订阅被平台拒绝（0x80），此分支实际不会触发。
 *
 * 两条都用 cJSON 解析，统一在函数末尾 cJSON_Delete。
 */
static void on_mqtt_msg(const char *topic, const char *payload)
{
    cJSON        *root    = NULL;
    cJSON        *item    = NULL;
    const char   *cmd_str = NULL;
    const char   *pwd_str = NULL;
    char          pwd[12];
    char          id_buf[24];
    const char   *id_str  = "0";           /* 下行报文的 id，回执时原样带回 */
    int32_t       ttl     = 300;
    int32_t       onetime = 1;
    const char   *ack     = "ok";          /* 命令应答内容，出错时改写 */

    if (!topic || !payload) return;

    /* 解析 JSON 报文：cJSON_Parse 必须配对 cJSON_Delete，否则会内存泄漏 */
    root = cJSON_Parse(payload);
    if (!root) return;

    /* 先取 id —— 两种下行都要回执，必须原样带回去 */
    item = cJSON_GetObjectItem(root, "id");
    if (item)
    {
        if (cJSON_IsString(item) && item->valuestring)
            id_str = item->valuestring;
        else if (cJSON_IsNumber(item))
        {
            snprintf(id_buf, sizeof(id_buf), "%d", item->valueint);
            id_str = id_buf;
        }
    }

    /* ================= 分支 A：物模型属性设置（唯一可行下行） ================= */
    if (strstr(topic, "/thing/property/set") != NULL)
    {
        int         code   = 200;
        const char *msg    = "success";
        cJSON      *params = cJSON_GetObjectItem(root, "params");

        if (params)
        {
            /* 本产品可写属性 = door_status（bool, rw）；
             * switch 是留给"日后新增专用开关属性"的别名，先查 switch 再查 door_status。 */
            cJSON *sw = cJSON_GetObjectItem(params, "switch");
            if (!sw) sw = cJSON_GetObjectItem(params, "door_status");
            /* 兼容控制台/模拟器可能发来的 {"door_status":{"value":true}} 包装形态 */
            if (sw && cJSON_IsObject(sw)) sw = cJSON_GetObjectItem(sw, "value");

            if (sw && cJSON_IsBool(sw))
            {
                if (cJSON_IsTrue(sw))
                {
                    /* 锁定期内 app_auth_remote_unlock() 会直接 return，如实回执 */
                    if (app_auth_is_locked()) { code = 403; msg = "locked"; }
                    else                      { app_auth_remote_unlock(); }
                }
                else
                {
                    app_auth_remote_lock();
                }
            }
            else
            {
                /* 非开关量：继续看"解除锁定"与"临时密码"两个可扩展属性 */
                cJSON *lk = cJSON_GetObjectItem(params, "unlock_lockout");
                cJSON *tp = cJSON_GetObjectItem(params, "temp_pwd");

                if (lk && cJSON_IsBool(lk) && cJSON_IsTrue(lk))
                {
                    app_auth_remote_unlock_lockout();          /* 解除错误锁定 */
                }
                else if (tp && cJSON_IsString(tp) && tp->valuestring)
                {
                    /* 临时密码字符串："6位数字" 或 "6位数字:有效期秒:是否一次性"
                     * 例 "374921" / "374921:600:1"；省略后两段时默认 300 秒、一次性 */
                    char    buf[32];
                    char   *p1;
                    char   *p2;
                    char   *p3;
                    int     i;
                    int32_t tv = 300;
                    int32_t on = 1;

                    strncpy(buf, tp->valuestring, sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';

                    p1 = buf;                                  /* 密码段 */
                    p2 = strchr(buf, ':');
                    if (p2)
                    {
                        *p2++ = '\0';
                        /* 有效期：手写十进制解析，避免引入 atoi */
                        tv = 0;
                        for (i = 0; p2[i] >= '0' && p2[i] <= '9'; i++)
                            tv = tv * 10 + (p2[i] - '0');
                        p3 = strchr(p2, ':');
                        if (p3) on = (p3[1] == '1') ? 1 : 0;
                    }

                    /* 密码必须是 6 位纯数字。
                     * ★ 注意：这里不能 goto cleanup —— cleanup 走的是 cmd_ack()，
                     *   它只认 /cmd/request/ 主题，对本分支不会回 set_reply，
                     *   APP 会一直等到 10411 超时。必须就地改 code/msg 走 prop_reply。 */
                    for (i = 0; i < 6; i++)
                        if (p1[i] < '0' || p1[i] > '9') break;

                    if (i < 6 || p1[6] != '\0')
                    {
                        code = 400; msg = "bad_pwd";
                    }
                    else
                    {
                        /* 范围钳制：60 秒 ~ 24 小时，防负数/超大值强转溢出 */
                        if (tv < 60)    tv = 60;
                        if (tv > 86400) tv = 86400;
                        on = (on == 1) ? 1 : 0;

                        if (app_auth_add_temp_pwd(p1, (uint8_t)on, (uint32_t)tv))
                        {
                            app_log_add(AUTH_METHOD_REMOTE, 0, LOG_RESULT_OK);
                            code = 200; msg = "success";
                        }
                        else
                        {
                            code = 400; msg = "full";          /* 临时密码表已满 */
                        }
                    }
                }
                else
                {
                    code = 400; msg = "bad_params";            /* 属性缺失或类型不对 */
                }
            }
        }
        else
        {
            code = 400; msg = "no_params";
        }

        prop_reply(id_str, code, msg);
        cJSON_Delete(root);
        return;
    }

    /* ================= 分支 B：命令族（APP synccmds） ================= */

    /* 取出 cmd 字段（字符串） */
    item = cJSON_GetObjectItem(root, "cmd");
    if (item && cJSON_IsString(item) && item->valuestring)
        cmd_str = item->valuestring;
    if (!cmd_str) goto cleanup;

    if (strcmp(cmd_str, "unlock") == 0)
    {
        /* 锁定期内 app_auth_remote_unlock() 会直接 return，这里如实告知平台 */
        ack = app_auth_is_locked() ? "locked" : "ok";
        app_auth_remote_unlock();               /* 远程开锁（锁定态拒绝） */
    }
    else if (strcmp(cmd_str, "lock") == 0)
    {
        app_auth_remote_lock();                 /* 远程上锁 */
    }
    else if (strcmp(cmd_str, "unlock_lockout") == 0)
    {
        app_auth_remote_unlock_lockout();       /* 解除错误锁定 */
    }
    else if (strcmp(cmd_str, "query") == 0)
    {
        app_wifi_push_status();                 /* 汇报当前状态 */
    }
    else if (strcmp(cmd_str, "temp_pwd") == 0)
    {
        /* 云端下发临时密码：pwd + 有效期/一次性标志 */
        item = cJSON_GetObjectItem(root, "pwd");
        if (item && cJSON_IsString(item) && item->valuestring
            && strlen(item->valuestring) == 6)
        {
            int i;
            /* valuestring 是 cJSON 内部指针；先拷贝到本函数的栈缓冲区，
             * 后续若在 cleanup 前使用务必基于 pwd_str 拷贝的值 */
            strncpy(pwd, item->valuestring, sizeof(pwd) - 1);
            pwd[sizeof(pwd) - 1] = '\0';

            /* 密码非法：goto cleanup 前先标记失败应答 */
            for (i = 0; i < 6; i++)
                if (pwd[i] < '0' || pwd[i] > '9')
                {
                    ack = "bad_pwd";
                    goto cleanup;
                }
            pwd_str = pwd;

            item = cJSON_GetObjectItem(root, "ttl");
            if (item && cJSON_IsNumber(item)) ttl = item->valueint;
            item = cJSON_GetObjectItem(root, "onetime");
            if (item && cJSON_IsNumber(item)) onetime = item->valueint;

            /* 范围钳制：ttl 限定在 60~86400 秒（1 分钟 ~ 24 小时），
             * 防止负数/超大值在强转 uint32_t 时溢出成巨大有效期（越权） */
            if (ttl < 60)      ttl = 60;
            if (ttl > 86400)   ttl = 86400;
            /* onetime 归一化为 0/1，其它值一律按非一次性处理 */
            onetime = (onetime == 1) ? 1 : 0;

            if (app_auth_add_temp_pwd(pwd_str, (uint8_t)onetime,
                                      (uint32_t)ttl))
            {
                app_log_add(AUTH_METHOD_REMOTE, 0, LOG_RESULT_OK);
                ack = "ok";
            }
            else
            {
                ack = "full";        /* 临时密码表已满（最多 4 条） */
            }
        }
        else
        {
            ack = "bad_pwd";         /* pwd 缺失或长度不是 6 */
        }
    }
    else
    {
        ack = "unknown_cmd";         /* 不在白名单的命令 */
    }

cleanup:
    /* 命令执行完毕 → 回执给平台（APP 端 synccmds 才能立即返回，而不是等超时） */
    cmd_ack(topic, ack);
    cJSON_Delete(root);
}

/* ---------------- 物模型上行封装 ---------------- */

/**
 * @brief 发布物模型属性上报（OneNET Studio 规范格式）
 *
 * 生成完整 JSON 报文：
 *   {"id":"<msg_id>","version":"1.0","params":{"<key>":{"value":<value>}}}
 *
 * 主题：$sys/{pid}/{dn}/thing/property/post
 * 例：{"id":"1","version":"1.0","params":{"door_status":{"value":true}}}
 *
 * 与旧实现的区别（已对齐参考工程 037-DHT11）：
 *   旧：$sys/{pid}/{dn}/dp/post/json + {"id":"n","dp":{"k":[{"v":v}]}}
 *   新：$sys/{pid}/{dn}/thing/property/post + params.value 形式
 *   —— 新格式下 APP 端 /thingmodel/query-device-property 可直接读到这些属性，
 *      不必再回退到旧的 /thing/detail 兼容接口。
 *
 * @param key    属性标识符（如 "door_status"）
 * @param value  已格式化的 JSON 值字符串（如 "true"、"\"tamper\""、数字串）
 */
static void publish_dp(const char *key, const char *value)
{
    char payload[176];
    char topic[96];

    /* 一次性生成完整 JSON，避免拼装过程中漏掉开头的 { / "params": */
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"%s\":{\"value\":%s}}}",
             (unsigned long)(++s_msg_id), key, value);

    snprintf(topic, sizeof(topic), ONENET_TOPIC_PROP_POST,
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    esp8266_mqtt_publish(topic, payload);
}

/* ---------------- 对外接口 ---------------- */

void app_wifi_init(void)
{
    /*
     * ★ 必须先接管 cJSON 的内存分配，否则云端下行命令全部解析失败。
     *
     * cJSON 默认的 global_hooks 指向 libc 的 malloc/free（cJSON.c 里
     * #define internal_malloc malloc）。但本工程的启动文件
     * startup_stm32f103xb.s 里 Heap_Size = 0x0（"不用 malloc，heap = 0"），
     * 且 STM32F103C8.sct 也没有 ARM_LIB_HEAP 段 —— microlib 的堆大小是 0，
     * malloc() 永远返回 NULL。
     * 结果：cJSON_Parse() 每层对象都分配失败 → 直接返回 NULL →
     *       on_mqtt_msg() 开头就 return，APP 下发的命令一条都执行不了。
     *
     * 这里把钩子改接到 FreeRTOS 的 heap_4（configTOTAL_HEAP_SIZE = 4KB，
     * 见 firmware/stm32/include/FreeRTOSConfig.h：本工程任务栈全部静态分配，
     * 这块堆只服务 cJSON / MqttKit / esp8266 下行解包的瞬时缓冲，
     * 20KB SRAM 装不下参考工程那样的 16KB），不额外占用静态 RAM，且线程安全。
     */
    {
        static cJSON_Hooks s_json_hooks;
        s_json_hooks.malloc_fn = pvPortMalloc;
        s_json_hooks.free_fn   = vPortFree;
        cJSON_InitHooks(&s_json_hooks);
    }

    esp8266_set_msg_cb(on_mqtt_msg);
    s_step = STEP_AT;
    s_step_t = millis();
    s_msg_id = 0;

    /* 打印编译进固件的联网参数，确认不是配错了三元组/路由器 */
    printf("[WIFI] ssid=\"%s\" mqtt=%s:%d\r\n",
           ESP8266_WIFI_SSID, ESP8266_MQTT_SERVER, ESP8266_MQTT_PORT);
    printf("[WIFI] pid=%s dn=%s\r\n", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
}

/** @brief MQTT 是否在线 */
uint8_t app_wifi_online(void)
{
    return (s_step == STEP_RUN) && esp8266_mqtt_connected();
}

/**
 * @brief 通用发布：把 payload 发到 $sys/{pid}/{dn}/<sub_topic>
 *
 * ⚠️ 只用于**自定义主题**。物模型事件**绝不能**用这个函数自己拼
 *    `thing/event/{event_id}/post` —— 物模型事件的正确主题是
 *    `thing/event/post`（事件名在 payload 的 params 里），发错了平台**静默丢弃**。
 *    上报事件请用 app_wifi_push_door_event() / app_wifi_push_duress_event()。
 */
void app_wifi_publish(const char *sub_topic, const char *payload)
{
    char topic[96];
    if (!app_wifi_online()) return;
    snprintf(topic, sizeof(topic), "$sys/%s/%s/%s",
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME, sub_topic);
    esp8266_mqtt_publish(topic, payload);
}

/** @brief 推送报警 → 物模型属性 alarm_level */
void app_wifi_push_alarm(uint8_t alarm_type)
{
    if (!app_wifi_online()) return;
    publish_dp("alarm_level", alarm_level_str(alarm_type));
}

/** @brief 推送门状态 → 物模型属性 door_status（true=open） */
void app_wifi_push_door(uint8_t door_open)
{
    if (!app_wifi_online()) return;
    publish_dp("door_status", door_open ? "true" : "false");
}

/** @brief 推送锁状态 → 物模型属性 lock_status（true=已开） */
void app_wifi_push_status(void)
{
    if (!app_wifi_online()) return;
    publish_dp("lock_status", app_lock_is_open() ? "true" : "false");
}

/* ⚠️ 原 app_wifi_push_light() 已删除（2026-09-11）：
 *    它 publish_dp("last_event", "\"light_on\"")，而物模型里**没有 last_event
 *    这个属性** ⇒ 每开一次锁都会收到平台回执 `2306 identifier not exist:
 *    identifier:last_event`，是一条注定失败的无效上行，纯浪费 MQTT 流量。
 *    若日后要做"开锁开灯"，应先在控制台把该属性加进物模型，再加回来。 */

/* ---------------- 物模型事件上行（APP「日志」页的数据源） ----------------
 *
 * 背景：APP 的日志页读的是 OneNET /device/event-log（设备事件日志）。
 * 固件以前只发属性（property/post），一条事件都没发过 ⇒ 该接口永远
 * {"list":null}，APP 上就是「开锁日志无信息」。
 *
 * 本产品物模型里有两个现成的事件（均已实测可上报）：
 *   door_event   —— 「门事件」，eventType=info，输出参数是一个叫 value 的 struct：
 *                   { type(string) 动作类型, method(int32 0~5) 认证方式,
 *                     since(int64) 毫秒戳, reason(string) 关门原因 }
 *   duress_scene —— 「胁迫场景」，eventType=alert，输出参数 type(string)/ts(int64)
 *
 * ★ 报文形状必须严格按物模型输出参数来（实测）：
 *     door_event   → params.door_event.value.value = {...}   （嵌套两层）
 *     duress_scene → params.duress_scene.value      = {...}  （只嵌一层）
 *   形状不对平台会回 2308/2409，且 id 必须是纯数字（否则 2405）。
 */

/** @brief 发送一条已拼好的事件 payload（只负责补主题，省栈） */
static void send_event_payload(const char *payload)
{
    char topic[96];
    snprintf(topic, sizeof(topic), ONENET_TOPIC_EVENT_POST,
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    esp8266_mqtt_publish(topic, payload);
}

/**
 * @brief 上报「门事件」door_event
 * @param type   命令类型，"unlock"（开锁）/ "lock"（上锁）
 * @param method 认证方式（0~5，见 app_common.h 的 AUTH_METHOD_*）
 *
 * ⚠️ method 必须 ≤ 5：物模型里 method 的取值区间是 0~5，
 *    而 AUTH_METHOD_ADMIN = 6 —— 所以本函数**不要**用来上报管理员进菜单。
 */
void app_wifi_push_door_event(const char *type, uint8_t method)
{
    char payload[192];

    if (!app_wifi_online()) return;

    /* since 是 int64 毫秒戳。★ 不能写 app_now_unix() * 1000UL：
     * 本平台 unsigned long 只有 32 位，1.789e9 × 1000 ≈ 1.79e12 会直接回绕
     * （实测上报值恒在 2.39e9 附近 ≈ 1970-01-28，时间完全不可信）。
     * 反正 DS3231 只有秒级分辨率，这里用 "%lu000" 把秒文本补成毫秒 ——
     * 纯字符串拼接，不做 64 位运算，也就不会溢出。 */
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"door_event\":"
             "{\"value\":{\"value\":{\"type\":\"%s\",\"method\":%u,"
             "\"since\":%lu000,\"reason\":\"\"}}}}}",
             (unsigned long)(++s_msg_id), type, (unsigned)method,
             (unsigned long)app_now_unix());
    send_event_payload(payload);
}

/** @brief 上报「胁迫场景」duress_scene（type 固定 "duress"） */
void app_wifi_push_duress_event(void)
{
    char payload[160];

    if (!app_wifi_online()) return;

    /* ts 同样是 int64 毫秒戳，处理方式与 door_event 的 since 一致（见那里的注释） */
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{\"duress_scene\":"
             "{\"value\":{\"type\":\"duress\",\"ts\":%lu000}}}}",
             (unsigned long)(++s_msg_id),
             (unsigned long)app_now_unix());
    send_event_payload(payload);
}

/**
 * @brief 联网状态机（100ms 调用）
 *        AT 指令为阻塞等待，因此只在门已上锁的空闲时段推进联网流程，
 *        避免影响正常操作（产品化建议放入独立 RTOS 任务彻底非阻塞）。
 */
void app_wifi_process(void)
{
    switch (s_step)
    {
        // 上电延时，等模块启动3000ms
    case STEP_AT:                                
        if ((millis() - s_step_t) > 3000)
        {
            printf("[WIFI] %s -> %s\r\n", s_step_name[s_step], s_step_name[STEP_CHECK]);
            s_step = STEP_CHECK;
            s_step_t = millis();
        }
        break;
        // AT 同步
    case STEP_CHECK:                    
        if ((millis() - s_last_at_try) < WIFI_AT_MIN_GAP_MS)// 限流：模块回 ERROR/"busy" 时 at_cmd 秒返回，不限流会以 ~10Hz 刷 AT
            break;
        s_last_at_try = millis();

        if (esp8266_check())// 检查模块是否已连接 WiFi
        {
            printf("[WIFI] %s -> %s (AT ok)\r\n", s_step_name[s_step], s_step_name[STEP_JOIN]);
            s_step = STEP_JOIN;
        }
        else if ((millis() - s_step_t) > 5000)
        {
            printf("[WIFI] %s timeout: no AT reply (check pwr/baud/TX-RX)\r\n",
                   s_step_name[s_step]);// 模块未回复 AT，检查电源、波特率、TX/RX 线是否正常
            s_step_t = millis();
        }
        break;
        // 连 WiFi
    case STEP_JOIN:  
        if (app_lock_is_open())// 门已上锁，拒绝连接
            break;
        if (esp8266_join_ap())// 连接成功
        {
            printf("[WIFI] %s -> %s (WiFi ok)\r\n", s_step_name[s_step], s_step_name[STEP_MQTT]);// 连接成功，进入 MQTT 状态
            s_step = STEP_MQTT;
            s_step_t = millis();
        }
        else if ((millis() - s_step_t) > 15000)
        printf("[WIFI] %s timeout: cannot join router\r\n", s_step_name[s_step]);// 
        {
            s_step_t = millis();
        }
        break;
/* 连 MQTT 服务器（含 token 鉴权）*/
    case STEP_MQTT:                                
        if ((millis() - s_step_t) < WIFI_RETRY_MS) /* 退避：不猛打模块 */
            break;
        s_step_t = millis();
        if (esp8266_mqtt_connect())// 连接成功
        {
            s_conn_fail = 0;
            printf("[WIFI] %s -> %s (MQTT ok)\r\n", s_step_name[s_step], s_step_name[STEP_SUB]);
            s_step = STEP_SUB;
        }
        else
        {
            s_conn_fail++;
            printf("[WIFI] %s failed, retry in %lums (see [MQTT] log)\r\n",
                   s_step_name[s_step], (unsigned long)WIFI_RETRY_MS);
            if (s_conn_fail >= 3)
            {
                /* 连续 3 次连不上（典型："no ip"）⇒ ESP 多半崩溃重启丢了 WiFi
                 * 关联，回 JOIN 重新走 CWMODE/CWDHCP/CWJAP 把关联捞回来 */
                s_conn_fail = 0;
                printf("[WIFI] MQTT failed x3 -> back to %s (re-run CWJAP)\r\n",
                       s_step_name[STEP_JOIN]);
                s_step = STEP_JOIN;
                s_step_t = millis();
            }
        }
        break;
 /* 订阅下行主题 */
    case STEP_SUB:                                
        if ((millis() - s_step_t) < WIFI_RETRY_MS) /* 同上，退避 */
            break;
        s_step_t = millis();
        if (esp8266_mqtt_subscribe())// 订阅成功
        {
            s_conn_fail = 0;
            s_step = STEP_RUN;
            s_last_beat = millis();
            printf("[WIFI] %s -> %s (subscribed, ONLINE)\r\n",
                   s_step_name[STEP_SUB], s_step_name[STEP_RUN]);
            app_wifi_push_status();// 推送 WiFi 状态
            app_ui_notify(UI_EVENT_WIFI);// 通知 UI 更新 WiFi 状态
        }
        else
        {
            /* SUB 连续失败 = TCP 链路已死（如 ESP 中途又被 wdt 打崩），
             * 与 MQTT 同策略：3 次后回 JOIN 重建 WiFi 关联 */
            s_conn_fail++;
            printf("[WIFI] %s failed, retry in %lums\r\n",
                   s_step_name[s_step], (unsigned long)WIFI_RETRY_MS);
            if (s_conn_fail >= 3)
            {
                s_conn_fail = 0;
                printf("[WIFI] SUB failed x3 -> back to %s (re-run CWJAP)\r\n",
                       s_step_name[STEP_JOIN]);
                s_step = STEP_JOIN;
                s_step_t = millis();
            }
        }
        break;
// 在线：心跳 + 断线重连
    case STEP_RUN:                                
        esp8266_process();
        if (!esp8266_mqtt_connected())
        {
            printf("[WIFI] %s -> %s (MQTT lost, reconnect)\r\n",
                   s_step_name[s_step], s_step_name[STEP_MQTT]);
            s_step = STEP_MQTT;
            s_step_t = millis();
            app_ui_notify(UI_EVENT_WIFI);
            break;
        }
        if ((millis() - s_last_beat) > 30000)      /* 30 秒心跳 */
        {
            s_last_beat = millis();
            app_wifi_push_status();
        }
        break;

    default:
        s_step = STEP_CHECK;
        break;
    }
}
