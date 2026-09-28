<!--
  pages/tempPwd/tempPwd.vue
  临时密码生成页：5 种模式
  （前 4 种走云端下发、需 OneNET 物模型属性；第 5 种「动态口令」本地计算、免联网）
-->
<template>
  <view class="page">
    <view class="card">
      <view class="card-title">生成方式</view>
      <view class="card-subtitle">选择临时密码的有效期和次数限制</view>

      <view class="mode-grid">
        <view
          v-for="m in modes"
          :key="m.id"
          :class="['mode-item', mode === m.id ? 'active' : '']"
          @click="setMode(m.id)">
          <text class="mode-icon">{{ m.icon }}</text>
          <text class="mode-label">{{ m.label }}</text>
          <text class="mode-desc">{{ m.desc }}</text>
        </view>
      </view>
    </view>

    <view class="card" v-if="pwd">
      <view class="card-title">生成的临时密码</view>

      <view class="pwd-box">
        <view
          v-for="(c, i) in pwdArr"
          :key="i"
          class="pwd-cell"
          @click="onCopy">{{ c }}</view>
      </view>

      <view class="meta">
        <view class="row-between"><text class="label">有效期</text><text class="value">{{ validityLabel }}</text></view>
        <view class="row-between" v-if="isTotp"><text class="label">本码剩余</text><text class="value">{{ totpLeft }} 秒后自动刷新</text></view>
        <view class="row-between"><text class="label">{{ isTotp ? '本码失效' : '到期时间' }}</text><text class="value">{{ expireAtStr }}</text></view>
        <view class="row-between"><text class="label">使用次数</text><text class="value">{{ isTotp ? '窗口内可重复' : (onetime ? '仅 1 次' : '多次（直到过期）') }}</text></view>
      </view>

      <view style="height: 24rpx;" />
      <button class="btn btn-primary btn-block" @click="onCopy">📋 复制密码</button>
      <view style="height: 16rpx;" />
      <button class="btn btn-block" @click="onShare">📤 分享给访客</button>
    </view>

    <view class="card" v-if="!pwd">
      <view class="card-subtitle" v-if="mode === 'totp'">动态口令由<b class="hl">本机直接计算</b>，不经过云端、<b class="hl">断网也能用</b>。每 30 秒变化一次，锁端容忍前后各一个时间片，约 90 秒内有效。</view>
      <view class="card-subtitle" v-else>点击下方按钮，云端会生成一个 6 位随机数字密码并下发到设备。设备收到后会立即接收，无需按键激活。</view>
      <button
        class="btn btn-primary btn-lg btn-block"
        :disabled="generating"
        @click="onGenerate">
        {{ generating ? '生成中...' : (mode === 'totp' ? '🔐 生成动态口令' : '🎲 生成临时密码') }}
      </button>
    </view>

    <view class="card">
      <view class="card-title">使用说明</view>
      <view class="hint-line">① 密码仅保存在设备端，不会同步到你的账户</view>
      <view class="hint-line">② 密码<b class="hl">同步传递</b>给访客，请走微信/短信通道</view>
      <view class="hint-line">③ 设备最多可缓存 4 条临时密码，过期或用完后自动让位</view>
      <view class="hint-line">④ 一次性密码用后即焚；限时密码到期自动作废</view>
      <view class="hint-line">⑤ <b class="hl">动态口令</b>走锁端内置算法，不占设备缓存、不依赖网络；锁的「系统设置」页会显示同一口令，可用来核对</view>
    </view>

    <view style="height: 120rpx;" />
  </view>
</template>

<script>
import api from '../../api/index.js';
import cfg from '../../api/config.js';
import fmt from '../../common/format.js';
import { totpCode, totpSecondsLeft } from '../../api/totp.js';

/*
 * 前 4 种是「云端临时密码」：APP 生成 → OneNET 属性下发 → 设备存表（带有效期）。
 *   ⚠️ 需要先在 OneNET 控制台把 temp_pwd 定义为「字符串 / 读写」属性，否则平台会回
 *      identifier not exist。开锁/上锁不受影响（用的是本来就有的 door_status）。
 *
 * 第 5 种是「动态口令」：**本地算，完全不联网**，锁端早已支持（固件 TOTP 出厂默认开启）。
 *   代价是有效期只有 30 秒（固件容忍前后各一片，实际约 90 秒），
 *   做不到"5 分钟 / 24 小时"那种长效密码。
 */
const MODE_MAP = {
  '5min':   { ttl: 300,    onetime: false, label: '5 分钟'                },
  '1h':     { ttl: 3600,   onetime: false, label: '1 小时'                },
  'oneday': { ttl: 86400,  onetime: false, label: '24 小时'               },
  'once':   { ttl: 86400,  onetime: true,  label: '24 小时内仅 1 次'        },
  'totp':   { ttl: 0,      onetime: false, label: '动态口令（30 秒刷新）'    }
};

export default {
  data() {
    return {
      modes: [
        { id: '5min',   icon: '⏱', label: '5 分钟',  desc: '5 分钟内可重复使用'  },
        { id: '1h',     icon: '⏰', label: '1 小时',  desc: '1 小时内可重复使用'  },
        { id: 'oneday', icon: '📅', label: '24 小时', desc: '24 小时内可重复使用' },
        { id: 'once',   icon: '1️⃣', label: '仅使用 1 次', desc: '24 小时内仅 1 次有效' },
        { id: 'totp',   icon: '🔐', label: '动态口令', desc: '30 秒刷新 · 免联网'  }
      ],
      mode:          '5min',
      pwd:           '',
      pwdArr:        [],
      validityLabel: '',
      expireAt:      0,
      expireAtStr:   '',
      onetime:       false,
      generating:    false,
      /* 动态口令专用：是否当前展示的是 TOTP、以及本时间片剩余秒数 */
      isTotp:        false,
      totpLeft:      0,
      /* 最近一次失败原因（含"下一步该做什么"，用弹窗显示给用户看） */
      failMsg:       '',
      /* setInterval 句柄（TOTP 倒计时用），不进模板、只为能 clear */
      totpTimer:     null
    };
  },
  /* 离开页面必须停掉定时器，否则后台空转 */
  onHide()   { this.stopTotpTimer(); },
  onUnload() { this.stopTotpTimer(); },
  methods: {
    setMode(m) {
      this.mode = m;
      this.stopTotpTimer();          /* 切走就停掉动态口令的重算定时器 */
      this.isTotp = false;
      this.failMsg = '';
      uni.vibrateShort({ type: 'light' });
    },

    /* ---------------- 动态口令（TOTP）：本地算，完全不联网 ---------------- */

    /** 重新计算当前时间片的口令并刷新倒计时；成功返回 true */
    genTotp() {
      const now  = Math.floor(Date.now() / 1000);
      const code = totpCode(cfg.totpKey, now);
      if (!code) {
        this.failMsg = 'TOTP 密钥无效，请检查 api/config.js 的 totpKey';
        return false;
      }
      this.pwd           = code;
      this.pwdArr        = code.split('');
      this.isTotp        = true;
      this.onetime       = false;
      this.validityLabel = '动态口令（每 30 秒变化，锁端容忍前后各一片）';
      this.expireAt      = (Math.floor(now / 30) + 1) * 30 * 1000;
      this.expireAtStr   = fmt.fmtDate(this.expireAt);
      this.totpLeft      = totpSecondsLeft(now);
      return true;
    },

    /** 每秒刷新倒计时；跨时间片时自动换成新口令 */
    startTotpTimer() {
      this.stopTotpTimer();
      this.totpTimer = setInterval(() => {
        if (!this.isTotp) return;
        const now  = Math.floor(Date.now() / 1000);
        const left = totpSecondsLeft(now);
        /* 剩余秒数"变大"了 ⇒ 已跨过 30 秒边界，换新口令 */
        if (left > this.totpLeft) this.genTotp();
        else                      this.totpLeft = left;
      }, 1000);
    },

    stopTotpTimer() {
      if (this.totpTimer) {
        clearInterval(this.totpTimer);
        this.totpTimer = null;
      }
    },
    onCellTap() { this.onCopy(); },
    onCopy() {
      if (!this.pwd) return;
      uni.setClipboardData({
        data: this.pwd,
        success: () => {
          uni.showToast({ title: '密码已复制', icon: 'success' });
          uni.vibrateShort({ type: 'light' });
        }
      });
    },
    onShare() {
      uni.setClipboardData({
        data: `临时开门密码：${this.pwd}（${this.validityLabel}）`,
        success: () => uni.showToast({ title: '密码已复制，可粘贴发送', icon: 'none' })
      });
    },
    async onGenerate() {
      if (this.generating) return;
      const cfgMode = MODE_MAP[this.mode];

      this.generating = true;
      this.failMsg = '';
      uni.showLoading({ title: '生成中...' });

      let ok = false;                        /* 执行结果：true=成功 false=失败 */
      try {
        if (this.mode === 'totp') {
          /* ★ 动态口令：本地计算，不下发、不联网 */
          ok = this.genTotp();
          if (ok) this.startTotpTimer();
        } else {
          /* 云端临时密码：APP 生成 → OneNET 属性下发 → 设备存表 */
          const r = await api.addTempPwd(cfgMode.ttl, cfgMode.onetime);

          this.stopTotpTimer();
          this.isTotp        = false;
          this.pwd           = r.pwd;
          this.pwdArr        = r.pwd.split('');
          this.validityLabel = cfgMode.label + (cfgMode.onetime ? '（一次性）' : '');
          this.expireAt      = r.expireAt;
          this.expireAtStr   = fmt.fmtDate(r.expireAt);
          this.onetime       = cfgMode.onetime;
          ok = true;

          /* 最近生成的临时密码写入本地缓存（最多保留 10 条） */
          const recents = uni.getStorageSync('recentTempPwds') || [];
          recents.unshift({
            pwd: r.pwd, ttl: r.ttl, onetime: cfgMode.onetime,
            createdAt: Date.now(), expireAt: r.expireAt
          });
          uni.setStorageSync('recentTempPwds', recents.slice(0, 10));
        }

        if (ok) uni.vibrateShort({ type: 'medium' });
      } catch (e) {
        /* ★ 把"下一步该做什么"展示给用户 —— 只打 console 的话手机上什么也看不到，
         *   用户只会看到"生成失败"，根本不知道要去控制台加属性。 */
        this.failMsg = (e && e.message) ? e.message : String(e);
        console.error('[onGenerate]', e);
      } finally {
        /* ⚠️ 顺序很关键：必须先 hideLoading() 再 showToast() / showModal()！
         * uni-app 的 showLoading 与 showToast 共用同一个底层控件，
         * loading 未关就调 showToast 会被 toast 顶掉，
         * 后续的 hideLoading() 找不到对象 → 报"必须配对使用"。 */
        uni.hideLoading();
        if (ok) {
          uni.showToast({
            title: this.isTotp ? '已生成动态口令' : '已下发到设备',
            icon: 'success'
          });
        } else if (this.failMsg) {
          uni.showModal({
            title: '生成失败',
            content: this.failMsg,
            showCancel: false,
            confirmText: '我知道了'
          });
        }
        this.generating = false;
      }
    }
  }
};
</script>

<style>
.page { padding: 24rpx 0 32rpx; }

.mode-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16rpx; }

.mode-item {
  display: flex; flex-direction: column; align-items: center;
  padding: 32rpx 16rpx;
  border: 2rpx solid var(--divider); border-radius: 16rpx;
  background: white; transition: all .15s;
}
.mode-item.active {
  border-color: var(--primary);
  background: var(--primary-light);
  transform: scale(0.97);
}
.mode-icon  { font-size: 56rpx; margin-bottom: 8rpx; }
.mode-label { font-size: 30rpx; font-weight: 600; color: var(--text); }
.mode-desc  { font-size: 22rpx; color: var(--text-secondary); margin-top: 8rpx; }

.pwd-box { display: flex; justify-content: space-between; margin: 32rpx 0; }

.pwd-cell {
  width: 90rpx; height: 110rpx;
  border: 4rpx solid var(--primary); border-radius: 16rpx;
  font-size: 56rpx; font-weight: 700;
  color: var(--primary-dark); background: var(--primary-light);
  display: flex; align-items: center; justify-content: center;
}

.meta { margin-top: 16rpx; }

.hint-line { font-size: 24rpx; color: var(--text-secondary); line-height: 1.7; padding: 6rpx 0; }
.hl { color: var(--danger); font-weight: 600; }
</style>
