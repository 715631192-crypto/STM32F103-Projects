<!--
  pages/unlock/unlock.vue
  远程开锁页：大圆按钮 + 进阶操作
-->
<template>
  <view class="page">
    <view class="hero">
      <view :class="['hero-circle', operating ? 'op' : '']" @click="onUnlockTap">
        <text class="hero-icon">{{ operating ? '...' : '🔓' }}</text>
      </view>
      <text class="hero-tip">轻按上方按钮，远程开锁</text>
      <text class="hero-desc">开锁请求通过 OneNET MQTT 下发到设备{{ '\n' }}约 1-3 秒后舵机会动作</text>
    </view>

    <view class="card" v-if="lastAction">
      <view class="card-title">最近操作</view>
      <view class="row-between"><text class="label">动作</text><text class="value">{{ lastAction }}</text></view>
      <view class="row-between"><text class="label">时间</text><text class="value">{{ lastActionAt }}</text></view>
      <view class="row-between" v-if="lastActionId"><text class="label">请求ID</text><text class="value small">{{ lastActionId }}</text></view>
    </view>

    <view class="card">
      <view class="card-title">进阶</view>
      <button class="btn btn-warning btn-block" @click="onLockTap">远程上锁</button>
      <view style="height: 16rpx;" />
      <button class="btn btn-block" @click="onUnlockLockout">解除错误锁定</button>
    </view>

    <view class="card">
      <view class="card-title">安全提示</view>
      <view class="hint-line">① 远程开锁需要设备在线（OneNET MQTT 已连接）</view>
      <view class="hint-line">② 操作日志会同步到云端，黑匣子可审计</view>
      <view class="hint-line">③ 锁定状态（密码错误 3 次后）的设备无法远程开锁</view>
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
      operating:    false,
      lastAction:   '',
      lastActionAt: '',
      lastActionId: ''
    };
  },
  methods: {
    onUnlockTap() {
      if (this.operating) return;
      uni.showModal({
        title: '远程开锁',
        content: '确认要远程打开门锁吗？\n请确保您在安全可信的网络环境。',
        confirmText: '确认开锁',
        cancelText:  '取消',
        confirmColor: '#e53935',
        success: (r) => { if (r.confirm) this.dispatch('unlock', '远程开锁'); }
      });
    },
    async onLockTap() {
      if (this.operating) return;
      await this.dispatch('lock', '远程上锁');
    },
    async onUnlockLockout() {
      if (this.operating) return;
      uni.showModal({
        title: '解除锁定',
        content: '确认解除错误锁定状态吗？（管理员操作）',
        success: (r) => { if (r.confirm) this.dispatch('unlock_lockout', '解除锁定'); }
      });
    },
    async dispatch(cmd, label) {
      this.operating = true;
      uni.showLoading({ title: '下发中...' });

      let ok = false;                        /* 执行结果：true=成功 false=失败 */
      let failMsg = '';
      try {
        const r = await api.sendCmd(cmd);
        ok = true;
        this.lastAction   = label;
        this.lastActionAt = fmt.fmtDate(Date.now());
        this.lastActionId = r.requestId || '';
        uni.vibrateShort({ type: 'medium' });
      } catch (e) {
        /* ★ 必须把 e.message 显示给用户！sendCmd 抛出的 message 里带着
         *   "下一步该做什么"（比如：物模型里缺 unlock_lockout 属性，
         *   要去控制台「产品开发 → 功能定义 → 添加功能点 → 属性」加）。
         *   以前这里只 console.error + toast "下发失败"，用户只看到四个字，
         *   完全不知道是"缺云端属性"还是"设备离线"，只能来问。 */
        failMsg = (e && e.message) ? e.message : String(e);
        console.error('[dispatch]', cmd, e);
      } finally {
        /* ⚠️ 顺序很关键：必须先 hideLoading() 再 showToast()/showModal()！
         *
         * uni-app 的 showLoading 与 showToast 底层共用同一个 toast 控件，
         * 如果在 loading 还没关闭时调用 showToast，toast 会直接「顶掉」
         * loading（此时 loading 已经消失），finally 里随后的 hideLoading()
         * 就找不到对象，控制台会报：
         *     "请注意 showLoading 与 hideLoading 必须配对使用"
         *
         * 所以这里统一在 finally 里：先关 loading，再弹提示。 */
        uni.hideLoading();
        if (ok) {
          uni.showToast({ title: '已下发', icon: 'success' });
        } else {
          /* 失败时用弹窗而不是 toast：toast 显示不下这么长的排查指引 */
          uni.showModal({
            title: label + '失败',
            content: failMsg || '未知错误',
            showCancel: false,
            confirmText: '我知道了'
          });
        }
        this.operating = false;
      }
    }
  }
};
</script>

<style>
.page { padding: 24rpx 0; }

.hero {
  display: flex; flex-direction: column; align-items: center;
  padding: 48rpx 0 32rpx;
}

.hero-circle {
  width: 280rpx; height: 280rpx;
  border-radius: 50%;
  background: linear-gradient(135deg, #43a047 0%, #2e7d32 100%);
  display: flex; align-items: center; justify-content: center;
  box-shadow: 0 8rpx 24rpx rgba(67, 160, 71, 0.36);
  transition: transform .15s;
}
.hero-circle.op {
  background: linear-gradient(135deg, #fb8c00 0%, #e65100 100%);
  box-shadow: 0 8rpx 24rpx rgba(251, 140, 0, 0.36);
  animation: pulse 1s infinite;
}
@keyframes pulse { 0%,100% { transform: scale(1); } 50% { transform: scale(0.95); } }
.hero-circle:active { transform: scale(0.95); }
.hero-icon { font-size: 120rpx; color: white; }

.hero-tip  { margin-top: 24rpx; font-size: 32rpx; color: var(--text); font-weight: 500; }
.hero-desc { margin-top: 8rpx; font-size: 24rpx; color: var(--text-secondary); text-align: center; white-space: pre-line; }

.hint-line { font-size: 24rpx; color: var(--text-secondary); line-height: 1.7; padding: 6rpx 0; }
.value.small { font-size: 22rpx; color: var(--text-secondary); }
</style>
