/**
 * api/totp.js
 * ================================================================
 * TOTP 离线动态口令（RFC 6238 / HMAC-SHA1）—— 与固件逐字节对齐
 *
 * 固件侧实现：`MIDDLEWARE/totp.c` 的 totp_now() / base32_decode()；
 * 校验入口  ：`USER/app_auth.c` 密码校验第 5 步（`s_p.totp_en && len == 6`）。
 *
 * 算法：
 *   t     = floor(unix / 30)            ← 30 秒一个时间片
 *   msg   = t 的 8 字节大端
 *   mac   = HMAC-SHA1(base32decode(key), msg)    20 字节
 *   off   = mac[19] & 0x0F              ← 动态截断偏移
 *   code  = ((mac[off]&0x7F)<<24 | mac[off+1]<<16 | mac[off+2]<<8 | mac[off+3]) % 10^6
 *
 * ★ 完全不需要联网 —— 手机本地算，锁本地验。
 *   固件容忍前后各 1 个时间片（TOTP_WINDOW = 1），即口令实际可用约 90 秒。
 */

import { hmacSha1 } from './sha1.js';

const TOTP_STEP = 30;      /* 必须与固件 TOTP_STEP_SECONDS 一致 */

/**
 * Base32 解码（RFC 4648：A~Z + 2~7）
 * 与固件 base32_decode() 行为一致：小写也接受、'=' 视为结束、空格忽略、
 * 遇非法字符返回空（表示密钥不可用）。
 */
function base32Decode(s) {
  let acc = 0;
  let bits = 0;
  const out = [];

  const str = String(s || '');
  for (let i = 0; i < str.length; i++) {
    const ch = str[i];
    let v;
    if (ch >= 'A' && ch <= 'Z')      v = ch.charCodeAt(0) - 65;
    else if (ch >= 'a' && ch <= 'z') v = ch.charCodeAt(0) - 97;
    else if (ch >= '2' && ch <= '7') v = ch.charCodeAt(0) - 50 + 26;
    else if (ch === '=')             break;      /* 填充符：结束 */
    else if (ch === ' ')             continue;   /* 空格：跳过  */
    else                             return new Uint8Array(0);

    acc = ((acc << 5) | v) >>> 0;
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      out.push((acc >>> bits) & 0xFF);
    }
  }
  return new Uint8Array(out);
}

/**
 * 计算指定时刻的 6 位动态口令
 * @param {string} b32Key  Base32 密钥（与固件 totp_key_b32 相同）
 * @param {number} unixSeconds Unix 秒时间戳
 * @returns {string} 6 位数字串；密钥非法时返回空串
 */
function totpCode(b32Key, unixSeconds) {
  const key = base32Decode(b32Key);
  if (!key.length) return '';

  /* t → 8 字节大端 */
  const msg = new Uint8Array(8);
  let t = Math.floor(Number(unixSeconds) / TOTP_STEP);
  for (let i = 7; i >= 0; i--) {
    msg[i] = t & 0xFF;
    t = Math.floor(t / 256);
  }

  const mac = new Uint8Array(hmacSha1(key, msg));
  const off = mac[19] & 0x0F;
  const code = (((mac[off] & 0x7F) << 24) |
                (mac[off + 1] << 16) |
                (mac[off + 2] << 8) |
                 mac[off + 3]) >>> 0;

  /* 不用 padStart，避免个别环境的 polyfill 差异 */
  let s = String(code % 1000000);
  while (s.length < 6) s = '0' + s;
  return s;
}

/** 当前时间片剩余秒数（用于倒计时显示） */
function totpSecondsLeft(unixSeconds) {
  return TOTP_STEP - (Math.floor(Number(unixSeconds)) % TOTP_STEP);
}

export {
  base32Decode,
  totpCode,
  totpSecondsLeft,
  TOTP_STEP
};
