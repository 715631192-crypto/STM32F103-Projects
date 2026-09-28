<!--
  pages/config/config.vue
  配置页：只读展示 + 推送开关
-->
<template>
  <view class="page">
    <view class="card">
      <view class="card-title">OneNET 设备三元组</view>
      <view class="card-subtitle">
        这三项是从 OneNET 控制台获取的设备凭证。其中
        <text class="hl">access_key 不能随便给别人或放在公开网址</text>
        ，本程序把它硬编码在 <code>api/config.js</code> 中。
      </view>

      <view class="form-row">
        <text class="form-label">ProductID</text>
        <input class="form-input" :value="productId" disabled placeholder="例如 v4nyue7h41" />
      </view>
      <view class="form-row">
        <text class="form-label">DeviceName</text>
        <input class="form-input" :value="deviceName" disabled placeholder="例如 smartlock_001" />
      </view>
      <view class="form-row">
        <text class="form-label">AccessKey</text>
        <input class="form-input secret" :value="accessKeyMasked" disabled password type="text" placeholder="已固化在代码中" />
      </view>
    </view>

    <view class="card">
      <view class="card-title">推送偏好</view>
      <view class="row-between">
        <text class="label">接收胁迫报警推送</text>
        <switch :checked="prefs.alarmPush" @change="onAlarmPushChange" color="#e53935" />
      </view>
      <view class="row-between">
        <text class="label">接收防撬报警推送</text>
        <switch :checked="prefs.tamperPush" @change="onTamperPushChange" color="#fb8c00" />
      </view>
      <view class="row-between">
        <text class="label">通知震动</text>
        <switch :checked="prefs.vibrate" @change="onVibrateChange" color="#1976d2" />
      </view>
    </view>

    <view class="card">
      <view class="card-title">关于</view>
      <view class="row-between"><text class="label">应用版本</text><text class="value">v1.0.0</text></view>
      <view class="row-between"><text class="label">对应硬件</text><text class="value">STM32F103C8T6</text></view>
      <view class="row-between"><text class="label">打包工具</text><text class="value">HBuilder X</text></view>
    </view>

    <view style="height: 120rpx;" />
  </view>
</template>

<script>
import cfg from '../../api/config.js';

export default {
  data() {
    return {
      productId:       cfg.productId,
      deviceName:      cfg.deviceName,
      accessKeyMasked: '••••••••••••••••（已固化，不显示）',
      prefs: {
        alarmPush:  true,
        tamperPush: true,
        vibrate:    true
      }
    };
  },

  onLoad() {
    const saved = uni.getStorageSync('pushPrefs');
    if (saved) this.prefs = { ...this.prefs, ...saved };
  },

  methods: {
    onAlarmPushChange(e)  { this.savePref('alarmPush',  e.detail.value); },
    onTamperPushChange(e) { this.savePref('tamperPush', e.detail.value); },
    onVibrateChange(e)    { this.savePref('vibrate',    e.detail.value); },
    savePref(key, value) {
      this.prefs = { ...this.prefs, [key]: value };
      uni.setStorageSync('pushPrefs', this.prefs);
    }
  }
};
</script>

<style>
.page { padding: 16rpx 0 32rpx; }

.form-row {
  display: flex; align-items: center;
  padding: 24rpx 0;
  border-bottom: 1rpx solid var(--divider);
}
.form-row:last-child { border-bottom: none; }

.form-label {
  width: 220rpx; color: var(--text-secondary);
  font-size: 28rpx; flex-shrink: 0;
}

.form-input {
  flex: 1; font-size: 28rpx;
  color: var(--text); text-align: right;
}

.form-input.secret { letter-spacing: 2rpx; }

.hl { color: var(--danger); font-weight: 600; }

.value {
  color: var(--text);
  font-size: 28rpx;
}
</style>
