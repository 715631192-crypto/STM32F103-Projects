<!--
  pages/index/index.vue
  主页：设备状态卡片 + 快捷操作入口
-->
<template>
  <view class="page">

    <!-- 设备状态卡 -->
    <view class="card hero-card">
      <view class="hero-head">
        <view class="row">
          <view :class="['status-dot', online ? 'status-online' : 'status-offline']"></view>
          <text class="hero-name">{{ deviceName }}</text>
        </view>
        <text class="hero-time" v-if="lastUpdateAt">
          更新于 {{ lastUpdateAtStr }}
        </text>
      </view>

      <view class="metric-row">
        <view class="metric">
          <text class="metric-label">门状态</text>
          <view :class="['metric-value', doorOpen ? 'open' : 'closed']">
            <text class="metric-icon">{{ doorOpen ? '🔓' : '🔒' }}</text>
            <text class="metric-text">{{ doorOpen ? '已打开' : '已关闭' }}</text>
          </view>
        </view>
        <view class="metric">
          <text class="metric-label">锁状态</text>
          <view :class="['metric-value', locked ? 'locked' : 'unlocked']">
            <text class="metric-icon">{{ locked ? '🔐' : '🔑' }}</text>
            <text class="metric-text">{{ locked ? '已上锁' : '已开锁' }}</text>
          </view>
        </view>
      </view>

      <view
        v-if="alarmLevel && alarmLevel !== 'none'"
        :class="['alarm-banner', alarmLevel]">
        <view class="alarm-icon">⚠</view>
        <view class="alarm-text">
          <view class="alarm-title">{{ alarmMap[alarmLevel] || '异常' }}</view>
          <view class="alarm-time" v-if="lastAlarmAtStr">{{ lastAlarmAtStr }}</view>
        </view>
      </view>
    </view>

    <!-- 快捷操作 -->
    <view class="card">
      <view class="card-title">快捷操作</view>
      <view class="quick-grid">
        <view class="quick-btn primary" @click="goUnlock">
          <text class="quick-icon">🔓</text>
          <text class="quick-label">远程开锁</text>
        </view>
        <view class="quick-btn warning" @click="goTempPwd">
          <text class="quick-icon">⏱</text>
          <text class="quick-label">临时密码</text>
        </view>
        <view class="quick-btn info" @click="onQuery">
          <text class="quick-icon">📊</text>
          <text class="quick-label">查询状态</text>
        </view>
        <view class="quick-btn" @click="goLogs">
          <text class="quick-icon">📜</text>
          <text class="quick-label">开锁日志</text>
        </view>
      </view>
    </view>

    <!-- 设备说明卡 -->
    <view class="card">
      <view class="card-title">设备信息</view>
      <view class="row-between"><text class="label">设备名</text><text class="value">{{ deviceName }}</text></view>
      <view class="row-between"><text class="label">ProductID</text><text class="value small">{{ productId }}</text></view>
      <view class="row-between"><text class="label">协议</text><text class="value">OneNET MQTT</text></view>
      <view class="row-between"><text class="label">通信</text><text class="value small">USART1 115200 → ESP8266</text></view>
    </view>

    <view style="height: 120rpx;" />
  </view>
</template>

<script>
import api from '../../api/index.js';
import cfg from '../../api/config.js';
import fmt from '../../common/format.js';

export default {
  data() {
    return {
      productId:        cfg.productId,
      deviceName:       cfg.deviceName,
      online:           false,
      doorOpen:         false,
      locked:           true,
      alarmLevel:       'none',
      lastAlarmAt:      0,
      lastAlarmAtStr:   '',
      lastUpdateAt:     0,
      lastUpdateAtStr:  '',
      timer:            null,
      querying:         false,   /* 查询状态命令是否进行中（防连点重入） */

      alarmMap: {
        tamper:    '防撬报警',
        duress:    '🚨 紧急求救',
        lockout:   '错误锁定',
        door_ajar: '门虚掩',
        none:      ''
      }
    };
  },

  onShow() {
    this.refreshStatus();
    if (this.timer) clearInterval(this.timer);
    this.timer = setInterval(() => this.refreshStatus(), cfg.pollIntervalMs);
  },

  onUnload() {
    if (this.timer) clearInterval(this.timer);
  },

  onPullDownRefresh() {
    this.refreshStatus().finally(() => uni.stopPullDownRefresh());
  },

  methods: {
    async refreshStatus() {
      try {
        const data = await api.getStatus();
        const now = Date.now();
        const oldAlarm = this.alarmLevel;

        this.online          = !!data.online;
        this.doorOpen        = data.door_status === true;
        this.locked          = data.lock_status !== true;
        this.alarmLevel      = data.alarm_level || 'none';
        this.lastUpdateAt    = now;
        this.lastUpdateAtStr = fmt.relTime(now);

        if (data.alarm_level && data.alarm_level !== 'none' && data.alarm_level !== oldAlarm) {
          this.lastAlarmAt    = now;
          this.lastAlarmAtStr = fmt.fmtDateShort(now);
          this.handleAlarmBurst(data.alarm_level);
        }
      } catch (e) {
        console.error('[refreshStatus]', e);
      }
    },

    handleAlarmBurst(level) {
      if (level === 'none') return;
      const prefs = uni.getStorageSync('pushPrefs') || {};
      if (prefs.vibrate !== false) uni.vibrateLong();

      uni.showModal({
        title: level === 'duress' ? '🚨 紧急求救' :
               level === 'tamper' ? '防撬报警'  : '门锁异常',
        content: this.alarmMap[level] || '检测到报警事件',
        confirmText: '查看',
        cancelText:  '忽略',
        success: (r) => { if (r.confirm) uni.switchTab({ url: '/pages/logs/logs' }); }
      });
    },

    /* tabBar 页面之间跳转必须用 switchTab，navigateTo 仅用于非 tabBar 页 */
    goUnlock()  { uni.switchTab({ url: '/pages/unlock/unlock'   }); },
    goTempPwd() { uni.switchTab({ url: '/pages/tempPwd/tempPwd' }); },
    goLogs()    { uni.switchTab({ url: '/pages/logs/logs'        }); },

    async onQuery() {
      /* 防重入：连点会导致两次 showLoading 只配一次 hideLoading，
       * 同样会触发 "showLoading 与 hideLoading 必须配对使用" 警告 */
      if (this.querying) return;
      this.querying = true;

      uni.showLoading({ title: '查询中...' });

      let ok = false;                        /* 执行结果：true=成功 false=失败 */
      try {
        await api.sendCmd('query');
        ok = true;
        setTimeout(() => this.refreshStatus(), 2000);
      } catch (e) {
        console.error('[onQuery]', e);
      } finally {
        /* ⚠️ 顺序很关键：必须先 hideLoading() 再 showToast()！
         * uni-app 的 showLoading 与 showToast 共用同一个底层控件，
         * loading 未关就调 showToast 会被 toast 顶掉，
         * 后续的 hideLoading() 找不到对象 → 报"必须配对使用"。 */
        uni.hideLoading();
        /* 注：原注释写"toast 已在 API 弹"，但 api/index.js 并未弹任何 toast，
         * 查询失败时用户完全无感知，这里补上失败提示 */
        uni.showToast({
          title: ok ? '查询已下发' : '查询失败',
          icon:  ok ? 'success' : 'none'
        });
        this.querying = false;
      }
    }
  }
};
</script>

<style>
.page { padding: 24rpx 0 32rpx; }

.hero-card {
  background: linear-gradient(135deg, #1976d2 0%, #1565c0 100%);
  color: white;
  margin: 32rpx 24rpx;
}

.hero-head {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
  margin-bottom: 24rpx;
}

.hero-name { color: white; font-size: 36rpx; font-weight: 600; margin-left: 12rpx; }
.hero-time { color: rgba(255,255,255,0.7); font-size: 22rpx; }

.metric-row { display: flex; gap: 24rpx; margin: 24rpx 0; }

.metric {
  flex: 1;
  background: rgba(255,255,255,0.14);
  border-radius: 16rpx;
  padding: 24rpx;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8rpx;
}

.metric-label { color: rgba(255,255,255,0.8); font-size: 24rpx; }
.metric-icon  { font-size: 56rpx; }
.metric-text  { font-size: 28rpx; font-weight: 500; color: white; }
.metric-value { display: flex; flex-direction: column; align-items: center; gap: 8rpx; }

/* 报警横幅 */
.alarm-banner {
  display: flex; align-items: center;
  background: rgba(255,255,255,0.18);
  border-radius: 12rpx; padding: 16rpx 24rpx; margin-top: 16rpx;
}
.alarm-banner.duress { background: #b71c1c; animation: flash 1s infinite; }
.alarm-banner.tamper  { background: #e65100; animation: flash 1.5s infinite; }
.alarm-banner.lockout { background: #f57c00; }
.alarm-banner.door_ajar { background: #fb8c00; }

@keyframes flash { 0%,100% { opacity: 1; } 50% { opacity: 0.6; } }

.alarm-icon  { font-size: 40rpx; margin-right: 16rpx; }
.alarm-title { color: white; font-size: 28rpx; font-weight: 600; }
.alarm-time  { color: rgba(255,255,255,0.85); font-size: 22rpx; margin-top: 4rpx; }

/* 快捷网格 */
.quick-grid {
  display: grid; grid-template-columns: 1fr 1fr; gap: 16rpx;
}
.quick-btn {
  display: flex; flex-direction: column; align-items: center;
  padding: 32rpx 16rpx; border-radius: 16rpx;
  background: var(--primary-light);
  transition: transform .15s;
}
.quick-btn:active { transform: scale(0.95); }
.quick-btn.primary { background: var(--primary-light); color: var(--primary-dark); }
.quick-btn.warning { background: #ffe0b2; color: #e65100; }
.quick-btn.info    { background: #b2dfdb; color: #00695c; }
.quick-btn         { background: #f5f5f5; color: var(--text); }
.quick-icon  { font-size: 56rpx; margin-bottom: 8rpx; }
.quick-label { font-size: 28rpx; font-weight: 500; }

.value.small { font-size: 24rpx; color: var(--text-secondary); }
</style>
