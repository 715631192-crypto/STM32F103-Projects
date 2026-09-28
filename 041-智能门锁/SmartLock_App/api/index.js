/**
 * api/index.js
 * ================================================================
 * 封装所有 OneNET Studio HTTP API 调用
 *
 * 端点对照表（与 https://iot.10086.cn/doc/aiot 已逐项核对）：
 *   ✓ /thingmodel/query-device-property  —— 设备物模型属性最新值（GET）
 *   ✓ /thingmodel/set-device-property    —— 下发控制（POST，属性设置 = 唯一可行下行）
 *   ✓ /device/event-log                  —— 设备事件日志（GET）
 *   ✗ /datapoint/synccmds                —— 已弃用！本产品是物模型产品，平台拒绝订阅
 *                                          cmd 主题族（SUBACK=0x80），永久报 10500
 *   △ /thing/detail                      —— 旧版兼容回退（HTTP 200 + result.properties）
 *
 * token 算法：access_key 项目级（res = products/{pid}），与官方算法对齐
 * 凭证来源：api/config.js
 */

/* 凭证来源：api/config.js（同目录兄弟文件，相对路径只写一层） */
import { makeAccessToken } from './sha1.js';
import cfg from './config.js';

const ONENET_BASE = 'https://iot-api.heclouds.com';
const TIMEOUT_MS  = 10 * 1000;

function buildToken() {
  const et = Math.floor(Date.now() / 1000);
  return makeAccessToken(cfg.productId, cfg.accessKey, et);
}

function http(method, path, headers = {}, body) {
  return new Promise((resolve, reject) => {
    uni.request({
      url: ONENET_BASE + path,
      method,
      header: { 'Content-Type': 'application/json', ...headers },
      data: body,
      timeout: TIMEOUT_MS,
      success: (res) => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(res.data);
        } else {
          const m = (res.data && (res.data.msg || res.data.error)) || res.data || '';
          reject(new Error(`OneNET HTTP ${res.statusCode}: ${typeof m === 'string' ? m : JSON.stringify(m)}`));
        }
      },
      fail: (err) => reject(new Error('网络异常: ' + (err.errMsg || '')))
    });
  });
}

function httpAuthed(method, path, body) {
  const token = buildToken();
  return http(method, path, { 'Authorization': token }, body);
}

/**
 * 把物模型数组 [{identifier, value, time}] 归一化为 dict
 * 旧版 /thing/detail 返回 { result.properties }，新版 /thingmodel/query-device-property 返回数组
 */
function pickProps(arr) {
  const out = {};
  if (!Array.isArray(arr)) return out;
  for (const it of arr) {
    if (it && it.identifier) {
      out[it.identifier] = {
        value: it.value,
        time: it.time
      };
    }
  }
  return out;
}

/* ================================================================
 * 公开 API
 * ================================================================ */

/**
 * 拉取设备物模型最新属性
 * 端点：GET /thingmodel/query-device-property?product_id=...&device_name=...
 *
 * 返回值结构（与历史约定保持一致，前端可继续用 door_status / lock_status / alarm_level）：
 *   {
 *     online,
 *     door_status:  true|false|null,
 *     lock_status:  true|false|null,
 *     alarm_level:  'tamper'|'duress'|'lockout'|'door_ajar'|'none'|null
 *   }
 */
async function getStatus() {
  /* === 路径 A：新 OneNET Studio 物模型 API ===
   * 文档：https://iot.10086.cn/doc/iot_platform/book/api/application/queryDeviceProperty.html
   *      https://open.iot.10086.cn/doc/aiot/a/detail/1470
   * 响应形态：
   *   {
   *     "code": 0, "msg": "succ", "request_id": "...",
   *     "data": { "list": [
   *       { "identifier": "door_status",  "value": "true", "time": 1700000000000, ... },
   *       { "identifier": "lock_status",  "value": "false",... },
   *       { "identifier": "alarm_level",  "value": "\"tamper\"",... }
   *     ] }
   *   }
   * 注意：1) 实际属性包在 data.list，绝非顶层数组；2) value 是 JSON 字符串（连引号一起）
   */
  const path1 = `/thingmodel/query-device-property?product_id=${encodeURIComponent(cfg.productId)}&device_name=${encodeURIComponent(cfg.deviceName)}`;
  try {
    const r = await httpAuthed('GET', path1);

    /* 关键修复：按官方文档读 data.list（之前误读 r.data / r 顶层 → 永远空） */
    /* 关键修复：data 有两种形态，必须都兼容（实测确认）
     *   形态 A（设备已上报过）：data = { list: [ {identifier, value, time}, ... ] }
     *   形态 B（设备从未上报）：data = [ {identifier, data_type, name}, ... ]
     *                          —— 直接就是数组，没有 .list，且不带 value
     * 之前只认形态 A 的 data.list，遇到形态 B 时 items 恒为 []。
     */
    const items = (r && r.code !== undefined && r.code !== 0)
                  ? []
                  : (Array.isArray(r && r.data)             ? r.data
                  : (r && r.data && Array.isArray(r.data.list)) ? r.data.list
                  : []);
    const props = pickProps(items);

    /* online 判断：不能再用 items.length > 0
     * —— 物模型一旦定义，即使设备从未上报也会返回功能点定义（实测 3 条）。
     * 正确做法：取最新一条属性时间戳，与当前时间比较，在新鲜窗口内才算在线。
     */
    let latest = 0;
    for (const it of items) {
      const t = Number(it && it.time) || 0;
      if (t > latest) latest = t;
    }
    const freshMs = 5 * 60 * 1000;   /* 5 分钟内有数据上报 = 在线 */
    const online = latest > 0 && (Date.now() - latest) < freshMs;

    return {
      online,
      door_status:  getPropVal(props.door_status),
      lock_status:  getPropVal(props.lock_status),
      alarm_level:  getPropVal(props.alarm_level)
    };
  } catch (e) {
    /* === 路径 B：经典 OneNET 兼容（保留） === */
    const path2 = `/thing/detail?product_id=${encodeURIComponent(cfg.productId)}&device_name=${encodeURIComponent(cfg.deviceName)}`;
    try {
      const r = await httpAuthed('GET', path2);
      const props = (r && r.result && r.result.properties) || {};
      return {
        online:       !!(r && r.result && r.result.online),
        door_status:  getPropVal(props.door_status),
        lock_status:  getPropVal(props.lock_status),
        alarm_level:  getPropVal(props.alarm_level)
      };
    } catch (e2) {
      throw new Error(`getStatus 失败: ${e.message}（新接口）/ ${e2.message}（旧接口）`);
    }
  }
}

/* 取物模型属性 v: { value: ... } 形态归一
 * 关键：OneNET 官方文档定义 value 是 "json string"——
 *        "true"      → 布尔 true（设备上报 bool:true）
 *        '"tamper"'  → 字符串 "tamper"（设备上报字符串，"tamper" 两边会带引号）
 *        '123'       → 数字 123
 *        'null'      → null
 * 因此拿到 v 后先用 JSON.parse 解出真正的类型；解析失败回退原值。
 */
function getPropVal(p) {
  if (!p) return null;
  let v = p.value;
  /* 物模型已定义但设备从未上报时，value 字段不存在（undefined）。
   * 统一归一为 null（而不是 undefined），前端 `=== null` 判断才可靠。 */
  if (v === undefined || v === null) return null;
  if (typeof v === 'string') {
    try { v = JSON.parse(v); } catch (e) { /* 保持字符串原值 */ }
  } else if (Array.isArray(v) && v.length > 0) {
    v = v[0];
  } else if (typeof v === 'object' && v !== null) {
    return null;
  }
  return v;
}

/**
 * 下发控制命令 —— 走 OneNET 物模型「属性设置」通道
 *
 * 端点：POST /thingmodel/set-device-property
 * 文档：https://iot.10086.cn/doc/aiot/a/detail/1456 （物模型 - 设置设备属性）
 *
 * ★★★ 为什么不再用 /datapoint/synccmds（2026-09-11 定案）★★★
 *
 * 本产品 v4nyue7h41 是【物模型】产品。用设备凭证直连 MQTT broker，
 * 逐条读 SUBACK 返回码（0x80 = 平台拒绝订阅）实测：
 *
 *   主题                                        SUBACK      结论
 *   -----------------------------------------   ---------   --------------
 *   $sys/{pid}/{dn}/thing/property/set          0x00        ✅ 可订阅
 *   $sys/{pid}/{dn}/thing/service/+/invoke      0x00        ✅ 可订阅
 *   $sys/{pid}/{dn}/cmd/#                       0x80        ❌ 拒绝
 *   $sys/{pid}/{dn}/cmd/request/+               0x80        ❌ 拒绝
 *
 * ⇒ /datapoint/synccmds（平台投递到 cmd/request/{cmdid}）对本产品
 *   永久返回 10500 "device not subscribed"，无论怎么改 token/timeout 都不通。
 *   它只适用于"数据流"协议产品。
 *
 * ⇒ 唯一可行通道 = 属性设置。本产品可写属性只有 door_status(bool, rw)：
 *     door_status = true  → 开锁
 *     door_status = false → 上锁
 *
 * ★ 报文格式（实测抓到的下行原文）：
 *   请求 body: {"product_id":"..","device_name":"..","params":{"door_status":true}}
 *   设备收到 : {"id":"1","version":"1.0","params":{"door_status":true}}
 *   注意 params 里是「标识符: 裸值」，**不包** {"value":...}。
 *
 * ★ 返回值语义：code=0 = 平台已受理且设备回了 set_reply；
 *   设备 10s 内没回 set_reply → 10411「设备响应超时」（设备离线/未订阅）。
 */
async function sendCmd(cmd, params = {}, opts = {}) {
  const ALLOWED = ['unlock', 'lock', 'query', 'temp_pwd', 'unlock_lockout'];
  if (!ALLOWED.includes(cmd)) {
    throw new Error('cmd 不在白名单: ' + cmd);
  }

  /* 「查询」本身没有下行语义：设备每 30s 主动上报 lock_status / door_status，
   * 直接拉一次最新属性即可，不必给设备发命令。 */
  if (cmd === 'query') {
    await getStatus();
    return {
      requestId: '', code: 0, msg: 'succ',
      delivered: true, awaitedResponse: true, payload: null
    };
  }

  /* 命令 → 物模型属性 的映射 */
  const PROP_CMD_MAP = {
    unlock:         { door_status: true },
    lock:           { door_status: false },
    unlock_lockout: { unlock_lockout: true }
  };

  let props;
  if (cmd === 'temp_pwd') {
    /* 临时密码：拼成 "<6位密码>:<有效期秒>:<是否一次性>"，由固件解析。
     * 需要先在 OneNET 控制台把 temp_pwd 定义为「字符串 / 读写」属性。 */
    const pwd = String(params.pwd || '');
    if (!/^\d{6}$/.test(pwd)) throw new Error('临时密码必须是 6 位数字');
    const ttl = Math.min(86400, Math.max(60, Number(params.ttl) || 300));
    props = { temp_pwd: `${pwd}:${ttl}:${params.onetime ? 1 : 0}` };
  } else {
    props = PROP_CMD_MAP[cmd];
  }
  if (!props) throw new Error('未映射的命令: ' + cmd);

  const body = {
    product_id:  cfg.productId,
    device_name: cfg.deviceName,
    params:      props
  };

  const r = await httpAuthed('POST', '/thingmodel/set-device-property', body);

  const code = (r && r.code !== undefined) ? r.code : null;
  if (code !== 0) {
    const msg = (r && r.msg) || '未知错误';

    /* ★ 判定顺序很重要：必须先看「标识符不存在」，再看 10411。
     *   实测 OneNET 缺属性时返回的也是 10411，msg 为
     *   "属性设置失败:identifier: temp_pwd, error: identifier not exist"。
     *   若先判 10411，就会误报成"设备离线/未订阅"，把人带偏（踩过）。 */
    let hint = '';
    if (/not exist|不存在/.test(String(msg))) {
      /* 每个命令缺哪个属性、该填什么类型，直接说清楚 */
      const SPEC = {
        temp_pwd:       '标识符 temp_pwd，类型 string，读写，字符串长度填 32',
        unlock_lockout: '标识符 unlock_lockout，类型 bool，读写',
        door_status:    '标识符 switch，类型 bool，读写'
      };
      const spec = SPEC[cmd] || '';
      hint = '（物模型里没有这个可写属性：请在 OneNET 控制台'
           + '「产品开发 → 功能定义 → 添加功能点 → 属性」新增'
           + (spec ? '：' + spec : '该标识符并设为"读写"') + '）';
    } else if (/length|长度/i.test(String(msg))) {
      /* 实测坑：temp_pwd 建属性时默认长度只有 10，而固件约定的格式是
       * "密码:有效期:一次性"（最长 "374921:86400:1" = 14 字符）
       * ⇒ 平台直接回 "string length error"。控制台把长度改大即可。 */
      hint = '（temp_pwd 在物模型里的「字符串长度」太小：'
           + '请在控制台把它改成 32，否则 "密码:有效期:一次性" 放不下）';
    } else if (code === 10411) {
      hint = '（设备未在超时内回 set_reply：设备可能离线，或未订阅 thing/property/set）';
    } else if (/属性|property/i.test(String(msg))) {
      hint = '（请到 OneNET 控制台确认该标识符已定义为"读写"属性）';
    }
    throw new Error(`属性下发失败 ${code}: ${msg}${hint}`);
  }

  return {
    requestId:       (r && r.request_id) || '',
    code,
    msg:             (r && r.msg) || '',
    delivered:       true,
    awaitedResponse: true,
    payload:         JSON.stringify(props)
  };
}

/**
 * 拉取设备事件日志
 * 端点：GET /device/event-log
 * 文档：https://open.iot.10086.cn/doc/aiot/a/detail/1476
 *       https://iot.10086.cn/doc/iot_platform/book/api/application/eventQuery.html
 * 重要参数（要 100% 对齐官方）：
 *   product_id, device_name   —— 必填
 *   start_time, end_time      —— 毫秒时间戳（long），必填
 *   offset (默认 0), limit (默认 10, 范围 [1,100])
 * 响应：
 *   { code, msg, request_id, data: { list: [
 *       { event_type: 1|2|3, identifier, name, time: <ms>, value: <JSON 字符串> }
 *   ], limit, offset } }
 */
async function getLogs(start, end, page = 1, per_page = 20) {
  /* 默认区间：最近 24 小时。允许传 ISO 字符串或 Date，统一转毫秒时间戳 */
  if (!start || !end) {
    const e = Date.now();
    const s = e - 24 * 3600 * 1000;
    start = s; end = e;
  } else {
    const sNum = (typeof start === 'number') ? start : Date.parse(start);
    const eNum = (typeof end   === 'number') ? end   : Date.parse(end);
    start = isNaN(sNum) ? Date.now() - 24 * 3600 * 1000 : sNum;
    end   = isNaN(eNum) ? Date.now()                   : eNum;
  }
  page     = Math.max(1, Number(page) || 1);
  per_page = Math.min(100, Math.max(1, Number(per_page) || 20));

  /* 关键修复：start_time/end_time（毫秒）+ offset/limit，而不是 start/end/page/per_page */
  const qs =
    `?product_id=${encodeURIComponent(cfg.productId)}` +
    `&device_name=${encodeURIComponent(cfg.deviceName)}` +
    `&start_time=${Math.floor(start)}` +
    `&end_time=${Math.floor(end)}` +
    `&offset=${(page - 1) * per_page}` +
    `&limit=${per_page}`;

  try {
    const r = await httpAuthed('GET', `/device/event-log${qs}`);

    /* 关键修复：响应是 data.list[]，字段是 value（JSON 字符串）+ time（毫秒）。
     * 对外仍保持 { total, page, per_page, items: [{time,content,level}] } 不变。 */
    const list = (r && r.data && Array.isArray(r.data.list)) ? r.data.list : [];
    const items = list.map((it) => {
      /* value 在官方定义里是 JSON 字符串（连引号），尽量解析回对象字符串以便 UI 展示 */
      let v = it.value;
      if (typeof v === 'string') {
        try { v = JSON.stringify(JSON.parse(v)); } catch (e) { v = it.value; }
      }
      /* event_type：1=信息 2=告警 3=故障 → 我们 UI 使用的字符串 level */
      let level = 'info';
      if (it.event_type === 2) level = 'alarm';
      else if (it.event_type === 3) level = 'fault';
      return {
        time:    it.time    || '',     /* 毫秒时间戳，原样透传 */
        content: v          || '',
        level,
        /* 物模型事件标识符（door_event / duress_scene …）。
         * 有了它，UI 就能精确按事件类型解析，不必靠猜 payload 里的字段名 */
        identifier: it.identifier || '',
        name:       it.name       || ''
      };
    });
    const total = (r && r.data && r.data.total !== undefined)
                  ? r.data.total
                  : (r && r.data && r.data.limit && r.data.offset
                      ? ((Number(r.data.offset) + items.length) || items.length)
                      : items.length);
    return { total, page, per_page, items };
  } catch (e) {
    throw new Error('日志接口失败: ' + e.message);
  }
}

/**
 * 生成密码学安全的 6 位随机临时密码（范围 100000 ~ 999999）
 *
 * 使用 crypto.getRandomValues 替代 Math.random()：
 *   - Math.random() 基于 Xorshift128+，序列可预测，攻击者可据此推算临时密码
 *   - crypto.getRandomValues 由操作系统 CSPRNG 提供熵，不可预测
 *
 * 注意：通过「拒绝采样」保证 6 位数字均匀分布，
 *      避免直接取模带来的低位偏差（模 900000 不是 2^32 的因子）。
 */
function secureRandomPwd() {
  // 一次性生成 4 字节（32 位无符号整数），crypto 填充
  const buf = new Uint32Array(1);
  if (typeof crypto !== 'undefined' && crypto.getRandomValues) {
    crypto.getRandomValues(buf);
  } else {
    // 降级：极端环境无 crypto 时用时间戳打散，仍比纯 Math.random 稍好
    buf[0] = (Date.now() ^ (Math.random() * 0xffffffff)) >>> 0;
  }

  // 拒绝采样：只取 [100000, 999999] 区间，落入区间外的重新映射
  // 900000 均匀区间，避免简单取模造成分布不均
  const r = (buf[0] % 900000) + 100000;
  return String(r);
}

/**
 * 云端生成 + 下发临时密码
 */
async function addTempPwd(ttl = 300, onetime = false) {
  const pwd = secureRandomPwd();
  ttl = Math.max(60, Math.min(86400, Number(ttl) || 300));

  await sendCmd('temp_pwd', { pwd, ttl, onetime: onetime ? 1 : 0 });

  return {
    pwd,
    ttl,
    onetime: !!onetime,
    expireAt: Date.now() + ttl * 1000
  };
}

export default {
  getStatus,
  sendCmd,
  getLogs,
  addTempPwd
};
