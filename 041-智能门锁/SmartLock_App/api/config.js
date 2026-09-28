/**
 * api/config.js
 * ================================================================
 * OneNET Studio 三件套 + 通用配置
 *
 * 三件套：
 *   - productId  : 产品 ID
 *   - deviceName : 设备名（ClientID）
 *   - accessKey  : 访问密钥
 *
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │ ⚠️ 重要：accessKey 有「三种体系」，res 与 key 必须配套，混用必报 10403 │
 * └──────────────────────────────────────────────────────────────────────┘
 *
 * 本项目使用【方式 2：产品访问】，即 res = products/{productId}
 *
 * ┌────┬──────────────────────┬────────────────────────────────────────────┐
 * │方式│ res                  │ accessKey 获取位置                          │
 * ├────┼──────────────────────┼────────────────────────────────────────────┤
 * │ 1  │ userid/{userid}      │ 主用户 key：DMP 控制台首页右上角 →          │
 * │主用户│                     │ 个人「账号信息」/「访问权限」               │
 * ├────┼──────────────────────┼────────────────────────────────────────────┤
 * │ 2  │ products/{productid} │ ★产品 key（本项目用这个）★                 │
 * │产品 │                      │ 「产品开发」→「产品列表」→ 点「操作」列的  │
 * │    │                      │ 「产品开发」链接 →「产品信息页面」查看      │
 * ├────┼──────────────────────┼────────────────────────────────────────────┤
 * │ 3  │ projects/{projectid} │ 项目 key：「应用开发」→「项目管理」→        │
 * │项目 │                      │ 「进入项目管理」→「项目概况」→「项目信息」 │
 * └────┴──────────────────────┴────────────────────────────────────────────┘
 *
 * 历史坑记录：
 *   曾误把「主用户 accessKey」（在「访问权限」页面，对应 res=userid/{userid}）
 *   填到本文件的 accessKey（这里需要的是「产品 accessKey」），
 *   导致所有 API 返回 code 10403 authentication failed。
 *   症状：端点存在（HTTP 200），但 code 非 0 且 msg 含 "auth"/"authentication"。
 *
 * 算法依据：https://iot.10086.cn/doc/aiot/fuse/detail/1464
 */
export default {
  productId:  'v4nyue7h41',
  deviceName: 'smartlock_001',
  accessKey:  '0WfldyV3r08r1cfLedEHTZ8nY6etoa1vizOPGxsLd5E=',
  /* ↑ 产品 accessKey（产品访问方式，res=products/v4nyue7h41）
   *   实测已通过鉴权：code=0，并成功拉取到物模型功能点
   *   （alarm_level / door_status / lock_status，与固件推送的 key 完全一致） */

  /* ★ TOTP 动态口令密钥（Base32），必须与固件一致。
   * ★ 2026-09-14 修正：本工程固件的种子不是参考工程 app_auth.c 的
   *   DEFAULT_TOTP_KEY("JBSWY3DPEHPK3PXP"，解码仅 7 字节)，而是
   *   smart_lock_board_config.h 的 PROVISION_DEFAULT_TOTP_SECRET =
   *   ASCII "12345678901234567890"（20 字节，RFC 6238 附录 B 测试向量），
   *   其 Base32 编码如下。密钥不一致 = 动态码永远验不过（踩过）。
   *   校验入口 = smart_lock_app.c 的 BOARD_AUTH_TOTP（先按 '*' 再输 6 位）。
   *   改这里的话，固件 PROVISION_DEFAULT_TOTP_SECRET 要同步改。 */
  totpKey: 'GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ',

  /* 业务配置 */
  pollIntervalMs: 30000,
  refreshOnShow:  true,
  vibrateOnAlarm: true
};

