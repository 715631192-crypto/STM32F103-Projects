#ifndef __APP_AUTH_H
#define __APP_AUTH_H

#include <stdint.h>
#include "app_common.h"

/*
 * 统一身份认证接口层（本项目安全体系的核心）
 * ================================================================
 * 所有开锁入口（密码/指纹/IC卡/TOTP/远程）都汇聚到这里，
 * 由它统一完成：验证 → 组合认证状态机 → 错误计数与锁定
 *             → 开锁动作 → 黑匣子日志 → 联动/报警
 *
 * 安全设计要点：
 *   1. 主密码与胁迫密码只存 SHA-1 摘要，Flash 里没有明文；
 *   2. 虚位密码：正确密码是输入串的任意 6 位连续子串即可通过（防偷窥）；
 *   3. 胁迫密码 = 主密码末位 +1；胁迫指纹 = AS608_DURESS_PAGE(49 号页)。
 *      用它们开锁：门正常打开（不激怒歹徒），但后台静默推送最高级别报警；
 *      另外用户还能在"密码管理"里自行添加 role=胁迫 的密码，效果相同；
 *   4. 连续 3 次失败 → 锁定 60 秒（锁定状态落盘，断电重启无效绕过）；
 *   5. 组合模式：可要求"两种不同方式"在 30 秒内先后通过才开锁。
 *
 * 密码体系（三条来源，认证时按此顺序命中）：
 *   ① SysParams.main_pwd_hash   出厂/修改的主密码     → 可进菜单 + 开锁
 *   ② SysParams.duress_pwd_hash 主密码末位+1 推导     → 开锁 + 静默报警
 *   ③ PwdEntry s_pwds[] (本表)   用户添加，带角色：
 *        管理员(1) 可进菜单 + 开锁 ／ 普通(2) 只开锁 ／ 胁迫(3) 开锁+静默报警
 */

#define AUTH_PWD_MAX_LEN     20    /* 虚位密码最长输入位数        */
#define AUTH_FAIL_LIMIT       3    /* 连续失败锁定阈值            */
#define AUTH_LOCKOUT_SECONDS 60    /* 锁定时长(秒)                */
#define AUTH_COMBO_WINDOW_S   30    /* 组合验证时间窗(秒)          */
#define LOCKOUT_COOLDOWN_S    60    /* 远程解锁锁定的最短冷却期(秒) */

#define CARD_MAX              50   /* 最多注册 50 张卡            */
#define TEMPPWD_MAX            4   /* 最多 4 条有效临时密码       */
#define PWD_MAX                6   /* 最多额外添加 6 条密码
                                    * ★ 数量受 RAM 限制：本表在 RAM 里
                                    *   有镜像（88B 出头），而
                                    *   RW_IRAM1 只剩约 280 字节。
                                    *   想加条数先确认 RAM 够不够。 */

/* ---- 密码角色（添加密码时指定，决定"能不能进菜单"和"要不要报警"） ----
 *   1 = 管理员：可进菜单做管理，也能开锁
 *   2 = 普通  ：只能开锁
 *   3 = 胁迫  ：开锁 + 静默最高级别报警（门照开，不露声色）
 *   另有两条"内置"凭据不在本条表里，见 SysParams：
 *     main_pwd_hash   —— 出厂/修改的主密码，等同管理员
 *     duress_pwd_hash —— 由主密码末位+1 推导出来的内置胁迫密码
 */
#define PWD_ROLE_ADMIN        1
#define PWD_ROLE_NORMAL       2
#define PWD_ROLE_DURESS       3

/* 系统参数（Flash 0x000000，上电加载到 RAM） */
typedef struct
{
    uint32_t magic;               /* 0x314B4C53 "SLK1" = 参数有效 */
    uint8_t  main_pwd_hash[20];   /* SHA1(主密码)                 */
    uint8_t  duress_pwd_hash[20]; /* SHA1(胁迫密码)               */
    char     totp_key_b32[20];    /* TOTP 密钥(Base32,与手机一致) */
    uint8_t  totp_en;             /* TOTP 开关                    */
    char     periodic_pwd[6];     /* 周期密码(明文,见README说明)  */
    uint8_t  periodic_en;
    uint8_t  periodic_weekday;    /* 1=周一 ... 7=周日            */
    uint8_t  periodic_h1;         /* 生效起始小时                 */
    uint8_t  periodic_h2;         /* 生效结束小时                 */
    uint8_t  auto_relock_s;       /* 开锁后 N 秒自动上锁          */
    uint8_t  combo_mode;          /* 0=单因素 1=双因素            */
    uint16_t finger_next_page;    /* 下一个空闲指纹页             */
    uint8_t  card_next_slot;      /* 下一个空闲卡槽               */
    uint8_t  fail_count;          /* 连续失败次数(持久化)         */
    uint32_t lockout_until;       /* 锁定截止 Unix 时间, 0=未锁   */
    uint8_t  rsv[20];             /* 预留对齐                     */
} SysParams;                      /* 共 128 字节                  */

/* 卡片表条目（Flash 0x001000，8B/条） */
typedef struct
{
    uint8_t uid[4];               /* MIFARE 卡 4 字节 UID         */
    uint8_t used;                 /* 1=占用 0=空                  */
    uint8_t rsv;
} CardEntry;

/* 临时密码条目（Flash 0x002000，16B/条） */
typedef struct
{
    char     pwd[6];              /* 6 位数字                     */
    uint8_t  onetime;             /* 1=一次性 0=限时              */
    uint8_t  used;                /* 1=已用掉(一次性)             */
    uint16_t rsv;
    uint32_t expire;              /* 截止 Unix 时间               */
} TempPwd;

/* 密码表条目（Flash 0x004000，22B/条）
 * ------------------------------------------------------------------
 * 只存 SHA-1 摘要，Flash 里没有明文 —— 与主密码同一套安全策略。
 * 6 条共 132 字节，上电加载到 RAM 镜像 s_pwds[]。 */
typedef struct
{
    uint8_t hash[20];             /* SHA1(6 位密码)               */
    uint8_t role;                 /* PWD_ROLE_ADMIN/NORMAL/DURESS */
    uint8_t used;                 /* 1=占用 0=空                  */
} PwdEntry;                       /* 22 字节                      */

void      app_auth_init(void);
SysParams *app_auth_params(void);
uint8_t   app_auth_save(void);              /* 参数/卡表/临时密码/密码表落盘 */

/* ---- 认证入口（驱动层/UI 调用，内部完成全部联动） ---- */
void app_auth_verify_password(const char *digits, uint8_t len);
void app_auth_verify_card(const uint8_t uid[4]);
void app_auth_verify_finger(uint16_t page_id);
void app_auth_remote_unlock(void);
void app_auth_remote_lock(void);
void app_auth_remote_unlock_lockout(void);  /* 云端解除错误锁定 */

uint8_t app_auth_check_admin(const char *digits, uint8_t len); /* 管理员校验 */
uint8_t app_auth_set_password(const char *new_pwd6);           /* 修改主密码 */
uint8_t app_auth_add_card(const uint8_t uid[4]);               /* 注册卡片   */
uint8_t app_auth_add_temp_pwd(const char *pwd6, uint8_t onetime, uint32_t ttl_s);

/* ---- 密码管理（0x004000 密码表） ---- */
uint8_t app_auth_add_password(const char *pwd6, uint8_t role); /* 添加一条密码 */
/* 删除密码：返回 1=已删除  2=命中的是内置凭据(不可删)  0=没找到 */
uint8_t app_auth_del_password(const char *pwd6);
uint8_t app_auth_pwd_count(void);                              /* 已添加条数   */

uint8_t app_auth_is_locked(void);

#endif
