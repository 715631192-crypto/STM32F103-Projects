<!--
  pages/logs/logs.vue
  日志页：拉取最近 24 小时设备日志 + 分类过滤
-->
<template>
  <view class="page">
    <view class="card" v-if="total > 0">
      <view class="card-title">最近 {{ total }} 条记录</view>
      <view class="card-subtitle">时间区间：{{ startStr }} ~ {{ endStr }}</view>

      <view class="filter-row">
        <view
          v-for="f in filters"
          :key="f.id"
          :class="['chip', filter === f.id ? 'active' : '']"
          @click="setFilter(f.id)">{{ f.label }}</view>
      </view>
    </view>

    <view class="card list">
      <view v-if="items.length === 0 && !loading" class="empty">暂无日志</view>
      <view v-for="(it, idx) in items" :key="idx" class="log-row">
        <view :class="['log-dot', it.kind]"></view>
        <view class="log-body">
          <view class="log-title">{{ it.title }}</view>
          <view class="log-desc">{{ it.desc }}</view>
          <view class="log-time">{{ it.timeStr }}</view>
        </view>
      </view>
    </view>

    <view class="more" v-if="items.length > 0">
      <view class="btn btn-sm" v-if="page < maxPage && !noMore" @click="loadMore">
        {{ loading ? '加载中...' : '加载更多' }}
      </view>
      <text class="no-more" v-if="noMore">已经到底了</text>
    </view>

    <view style="height: 120rpx;" />
  </view>
</template>

<script>
import api from '../../api/index.js';
import fmt from '../../common/format.js';

export default {
  data() {
    return {
      items:        [],
      rawItems:     [],
      total:        0,
      page:         1,
      perPage:      20,
      maxPage:      1,
      loading:      false,
      noMore:       false,
      startStr:     '',
      endStr:       '',
      filter:       'all',
      filters: [
        { id: 'all',    label: '全部' },
        { id: 'unlock', label: '开锁' },
        { id: 'alarm',  label: '报警' },
        { id: 'config', label: '配置' }
      ]
    };
  },

  onShow() {
    if (this.rawItems.length === 0) {
      this.loadPage(1, false);
    }
  },

  onPullDownRefresh() {
    this.loadPage(1, false).finally(() => uni.stopPullDownRefresh());
  },

  methods: {
    loadMore() {
      if (this.noMore || this.loading) return;
      this.loadPage(this.page + 1, true);
    },
    setFilter(f) {
      this.filter = f;
      this.applyFilter();
    },
    async loadPage(page, append) {
      if (this.loading) return;
      this.loading = true;

      const end = Date.now();
      const start = end - 24 * 3600 * 1000;

      try {
        const r = await api.getLogs(
          new Date(start).toISOString(),
          new Date(end).toISOString(),
          page,
          this.perPage
        );

        const raw = append
          ? this.rawItems.concat(r.items)
          : r.items;

        this.rawItems = raw;
        this.total    = r.total;
        this.page     = r.page;
        this.maxPage  = Math.ceil(r.total / r.per_page);
        this.noMore   = r.page * r.per_page >= r.total;
        this.startStr = fmt.fmtDate(start);
        this.endStr   = fmt.fmtDate(end);

        this.applyFilter();
      } catch (e) {
        console.error('[loadPage]', e);
      } finally {
        this.loading = false;
      }
    },

    applyFilter() {
      const f = this.filter;
      const items = this.rawItems
        .map((it) => normalize(it))
        .filter((it) => {
          if (f === 'all') return true;
          return it.kind === f;
        });
      this.items = items;
    }
  }
};

/* 工具：OneNET 日志 → 渲染项 */
function normalize(raw) {
  const ts = parseTime(raw.time);
  let content = {};
  /* key 修复：官方 doc #1476 的事件 payload 在字段 value（JSON 字符串）中。
   * 对外兼容老逻辑 —— 如果上层没把 value 解析成字符串，这里再尝试一次。 */
  try { content = JSON.parse(raw.content); } catch (e) {}
  if (!content || typeof content !== 'object') content = { raw: raw.content };

  const ident = raw.identifier || '';
  let kind = 'other';
  let title = '';
  let desc  = '';

  /* ---- ① 固件上报的物模型事件（2026-09-11 新增，日志页的主要数据源）----
   *   door_event    payload = { value: { type, method, since, reason } }
   *   duress_scene  payload = { type:"duress", ts }
   *
   * ⚠️ duress_scene 的 type 就在**顶层**，必须先于下面通用的 content.type
   *    分支判断，否则会被当成"未知自定义事件"归到 other（老代码的坑）。
   */
  if (ident === 'duress_scene' || (content.type === 'duress' && content.ts !== undefined)) {
    kind  = 'alarm';
    title = '🚨 胁迫报警';
    desc  = '使用胁迫凭据开门（门正常打开，后台静默报警）';
  } else if (ident === 'door_event' || (content.value && content.value.type)) {
    const v   = content.value || {};
    const who = authMethodName(v.method);
    if (v.type === 'unlock') {
      kind  = 'unlock';
      title = '🔓 开锁成功';
      desc  = `认证方式：${who}`;
    } else if (v.type === 'lock') {
      kind  = 'unlock';
      title = '🔒 上锁';
      desc  = `触发来源：${who}`;
    } else {
      kind  = 'unlock';
      title = `门事件：${v.type}`;
      desc  = `认证方式：${who}`;
    }
  } else if (content.cmd) {
    kind = 'unlock';
    const m = methodMap(content.cmd, content.from);
    title = m.title; desc = m.desc;
  } else if (content.alarm) {
    kind = 'alarm';
    const m = alarmMap(content.alarm);
    title = m.title; desc = m.desc;
  } else if (content.event) {
    kind = 'config';
    title = '配置变更';
    desc  = `由 ${content.by || '系统'} 触发：${content.event}`;
  } else if (content.type) {
    /* STM32 设备用 {type:"door_open"|"door_close", ...} 自定义上报，
     * 不属于 OneNET 物模型 event 三件套，做归类归到 "unlock" 类 */
    kind = (content.type === 'door_open' || content.type === 'door_close' || content.type.indexOf('unlock') >= 0)
           ? 'unlock' : 'other';
    title = content.type;
    desc  = content.method !== undefined
            ? `method=${content.method}` : (content.since ? `since=${content.since}` : '');
  } else {
    title = '事件';
    desc  = raw.content || '';
  }

  return {
    timeStr: ts ? fmt.fmtDate(ts) : raw.time,
    kind, title, desc, _ts: ts
  };
}

/* 认证方式 → 中文名。必须与固件 app_common.h 的 AUTH_METHOD_* 保持一致，
 * 因为 door_event 上报的 method 就是这套编号。 */
const AUTH_METHOD_NAME = {
  0: '系统自动',
  1: '密码',
  2: 'IC 卡',
  3: '指纹',
  4: '动态口令',
  5: '远程下发',
  6: '管理员'
};

function authMethodName(m) {
  if (m === undefined || m === null) return '未知';
  const n = Number(m);
  if (isNaN(n)) return String(m);
  return AUTH_METHOD_NAME[n] || `方式${n}`;
}

/* 兼容多种时间格式：OneNET /device/event-log 给的是毫秒数字戳，
 * 老逻辑给的是 ISO 字符串；都允许 Date.parse / new Date 用。
 */
function parseTime(t) {
  if (!t && t !== 0) return 0;
  if (typeof t === 'number') return isNaN(t) ? 0 : t;
  if (typeof t === 'string' && /^\d+$/.test(t)) return Number(t);
  const ms = Date.parse(t);
  return isNaN(ms) ? 0 : ms;
}

function methodMap(cmd, from) {
  const src = from ? ` (${from})` : '';
  const tab = {
    unlock:         { title: '远程开锁成功',   desc: '通过 OneNET MQTT 下发命令' + src },
    lock:           { title: '远程上锁',       desc: '锁舌归位' },
    unlock_lockout: { title: '解除错误锁定',   desc: '管理员远程操作' },
    temp_pwd:       { title: '下发临时密码',   desc: '云端注入' },
    query:          { title: '查询状态',       desc: '无实际动作' }
  };
  return tab[cmd] || { title: '开锁指令: ' + cmd, desc: '' };
}

function alarmMap(level) {
  const tab = {
    tamper:    { title: '🔨 防撬报警',   desc: '物理破坏触发' },
    duress:    { title: '🚨 紧急求救',   desc: '胁迫操作' },
    lockout:   { title: '错误锁定',     desc: '密码错误次数过多' },
    door_ajar: { title: '门虚掩',       desc: '门未关紧超过阈值' }
  };
  return tab[level] || { title: '报警: ' + level, desc: '' };
}
</script>

<style>
.page { padding: 24rpx 0 32rpx; }

.filter-row { display: flex; gap: 12rpx; margin-top: 16rpx; }
.chip {
  padding: 12rpx 24rpx; border-radius: 24rpx;
  background: #f5f5f5; color: var(--text-secondary);
  font-size: 24rpx;
}
.chip.active { background: var(--primary); color: white; }

.list { padding: 0 32rpx; }

.empty {
  text-align: center; padding: 80rpx 0;
  color: var(--text-secondary); font-size: 28rpx;
}

.log-row {
  display: flex;
  padding: 24rpx 0;
  border-bottom: 1rpx solid var(--divider);
}
.log-row:last-child { border-bottom: none; }

.log-dot {
  width: 16rpx; height: 16rpx; border-radius: 50%;
  margin: 14rpx 20rpx 0 0; flex-shrink: 0;
}
.log-dot.unlock { background: var(--success); }
.log-dot.alarm  { background: var(--danger); }
.log-dot.config { background: var(--primary); }
.log-dot.other  { background: var(--text-secondary); }

.log-body { flex: 1; min-width: 0; }
.log-title { font-size: 28rpx; color: var(--text); font-weight: 500; }
.log-desc  { font-size: 24rpx; color: var(--text-secondary); margin-top: 4rpx; }
.log-time  { font-size: 22rpx; color: var(--text-secondary); margin-top: 4rpx; }

.more { display: flex; justify-content: center; padding: 32rpx 0; }
.no-more { color: var(--text-secondary); font-size: 24rpx; }
</style>
