/**
 * api/sha1.js
 * ================================================================
 * 纯 JS SHA1 + HMAC-SHA1 + Base64 + URL 编码 + OneNET access_token 计算
 *
 * 与微信小程序版 (SmartLock_MiniApp/utils/sha1.js) 算法一致：
 *   sign  = base64(hmac-sha1(accessKey, msg))
 *   其中 accessKey 当 ASCII 字节序列（不 hex 解码）
 *
 * 验证：token 与 Python hmac + base64 实现 byte-for-byte 一致
 */

function sha1(input) {
  let bytes;
  if (typeof input === 'string') {
    bytes = stringToBytes(input);
  } else if (input instanceof ArrayBuffer) {
    bytes = new Uint8Array(input);
  } else if (input && (input.buffer instanceof ArrayBuffer)) {
    bytes = new Uint8Array(input.buffer);
  } else {
    throw new TypeError('sha1: input must be string or Uint8Array');
  }

  let h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
  const origLen = bytes.length;
  const bitLen = origLen * 8;
  const padLen = ((origLen + 9 + 63) & ~63) - origLen;
  const padded = new Uint8Array(origLen + padLen);
  padded.set(bytes);
  padded[origLen] = 0x80;
  const dv = new DataView(padded.buffer);
  dv.setUint32(padded.length - 4, bitLen >>> 0, false);
  dv.setUint32(padded.length - 8, Math.floor(bitLen / 0x100000000), false);

  const w = new Uint32Array(80);
  for (let chunkStart = 0; chunkStart < padded.length; chunkStart += 64) {
    for (let i = 0; i < 16; i++) w[i] = dv.getUint32(chunkStart + i * 4, false);
    for (let i = 16; i < 80; i++) {
      const x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
      w[i] = (((x << 1) | (x >>> 31)) >>> 0);
    }

    let a = h0, b = h1, c = h2, d = h3, e = h4;
    for (let i = 0; i < 80; i++) {
      let f, k;
      if (i < 20)      { f = (b & c) | ((b ^ 0xFFFFFFFF) & d); k = 0x5A827999; }
      else if (i < 40) { f = b ^ c ^ d;                          k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d);        k = 0x8F1BBCDC; }
      else             { f = b ^ c ^ d;                          k = 0xCA62C1D6; }

      const temp = ((((a << 5) | (a >>> 27)) + f + e + k + w[i]) >>> 0);
      e = d; d = c; c = ((b << 30) | (b >>> 2)) >>> 0; b = a; a = temp;
    }

    h0 = (h0 + a) >>> 0; h1 = (h1 + b) >>> 0; h2 = (h2 + c) >>> 0;
    h3 = (h3 + d) >>> 0; h4 = (h4 + e) >>> 0;
  }

  const out = new Uint8Array(20);
  const outDv = new DataView(out.buffer);
  outDv.setUint32(0, h0, false); outDv.setUint32(4, h1, false);
  outDv.setUint32(8, h2, false); outDv.setUint32(12, h3, false);
  outDv.setUint32(16, h4, false);
  return out.buffer;
}

function hmacSha1(key, msg) {
  let keyBytes;
  if (typeof key === 'string') {
    keyBytes = stringToBytes(key);
  } else if (key instanceof ArrayBuffer) {
    keyBytes = new Uint8Array(key);
  } else {
    keyBytes = new Uint8Array(key.buffer);
  }

  if (keyBytes.length > 64) {
    keyBytes = new Uint8Array(sha1(keyBytes));
  }
  if (keyBytes.length < 64) {
    const padded = new Uint8Array(64);
    padded.set(keyBytes);
    keyBytes = padded;
  }

  const ipad = new Uint8Array(64);
  const opad = new Uint8Array(64);
  for (let i = 0; i < 64; i++) {
    ipad[i] = keyBytes[i] ^ 0x36;
    opad[i] = keyBytes[i] ^ 0x5C;
  }

  const msgBytes = typeof msg === 'string' ? stringToBytes(msg) : new Uint8Array(msg);
  const inner = new Uint8Array(64 + msgBytes.length);
  inner.set(ipad);
  inner.set(msgBytes, 64);
  const innerHash = sha1(inner.buffer);

  const outer = new Uint8Array(64 + 20);
  outer.set(opad);
  outer.set(new Uint8Array(innerHash), 64);
  return sha1(outer.buffer);
}

const B64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function base64Encode(buf) {
  const bytes = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  let out = '';
  let i = 0;
  const n = bytes.length;
  for (; i + 2 < n; i += 3) {
    const triplet = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
    out += B64_CHARS[(triplet >> 18) & 0x3F];
    out += B64_CHARS[(triplet >> 12) & 0x3F];
    out += B64_CHARS[(triplet >>  6) & 0x3F];
    out += B64_CHARS[ triplet        & 0x3F];
  }
  if (i < n) {
    const b1 = bytes[i];
    const b2 = (i + 1 < n) ? bytes[i + 1] : 0;
    const triplet = (b1 << 16) | (b2 << 8);
    out += B64_CHARS[(triplet >> 18) & 0x3F];
    out += B64_CHARS[(triplet >> 12) & 0x3F];
    if (i + 1 < n) {
      out += B64_CHARS[(triplet >>  6) & 0x3F];
      out += '=';
    } else {
      out += '==';
    }
  }
  return out;
}

/**
 * 标准 Base64 解码（RFC 4648）
 * 用于 OneNET accessKey 在参与 HMAC 之前先解码
 * 直接调用浏览器内置 atob()（V8 引擎原生支持）
 */
function base64Decode(str) {
  /* atob 接受标准 base64，含 = padding */
  const binary = atob(String(str));
  const out = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) out[i] = binary.charCodeAt(i);
  return out;
}

function urlEncode(s) {
  return encodeURIComponent(s)
    .replace(/!/g,  '%21')
    .replace(/\*/g, '%2A')
    .replace(/'/g,  '%27')
    .replace(/\(/g, '%28')
    .replace(/\)/g, '%29');
}

/**
 * 计算 OneNET 后端 access_token（按官方 2022-05-01 算法对齐）
 * 算法（与官方文档 https://iot.10086.cn/doc/aiot/fuse/detail/1464 一致）：
 *
 *   version = "2022-05-01"            ← 当前 OneNET Studio 标准版本
 *   res     = products/{productId}
 *   msg     = "{et}\nsha1\n{res}\n{version}"
 *   key     = base64_decode(accessKey) ← ★ 必须先解码
 *   sign    = base64(hmac_sha1(key, msg))
 *   token   = "version=...&res={url(res)}&et=...&method=sha1&sign={url(sign)}"
 *
 * @param {string} productId
 * @param {string} accessKey  —— OneNET 给的 base64 字符串，函数内部会解码
 * @param {number} et        Unix 秒时间戳
 * @returns {string}
 */
function makeAccessToken(productId, accessKey, et) {
  const version = '2022-05-01';
  const method  = 'sha1';
  const res     = `products/${productId}`;
  const msg     = `${et}\n${method}\n${res}\n${version}`;

  /* ★ 关键修复：HMAC 前必须 base64-decode access_key ★ */
  const keyRaw = base64Decode(accessKey);
  const mac    = hmacSha1(keyRaw, msg);
  const signRaw = base64Encode(mac);

  return `version=${version}&res=${urlEncode(res)}&et=${et}&method=${method}&sign=${urlEncode(signRaw)}`;
}

function stringToBytes(s) {
  const bytes = [];
  for (let i = 0; i < s.length; i++) {
    let code = s.charCodeAt(i);
    if (code < 0x80) {
      bytes.push(code);
    } else if (code < 0x800) {
      bytes.push(0xC0 | (code >> 6));
      bytes.push(0x80 | (code & 0x3F));
    } else if ((code & 0xFC00) === 0xD800 && i + 1 < s.length) {
      code = ((code & 0x3FF) << 10) | (s.charCodeAt(++i) & 0x3FF);
      code += 0x10000;
      bytes.push(0xF0 | (code >> 18));
      bytes.push(0x80 | ((code >> 12) & 0x3F));
      bytes.push(0x80 | ((code >>  6) & 0x3F));
      bytes.push(0x80 | (code & 0x3F));
    } else {
      bytes.push(0xE0 | (code >> 12));
      bytes.push(0x80 | ((code >>  6) & 0x3F));
      bytes.push(0x80 | (code & 0x3F));
    }
  }
  return new Uint8Array(bytes);
}

export {
  sha1,
  hmacSha1,
  base64Encode,
  base64Decode,
  urlEncode,
  makeAccessToken,
  stringToBytes
};
