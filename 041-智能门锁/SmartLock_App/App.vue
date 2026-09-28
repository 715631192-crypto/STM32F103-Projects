<script>
/**
 * 根组件 (uni-app 风格)
 * 全局生命周期入口，可在此做：初始化、检查密钥、监听应用前后台切换
 */
import cfg from './api/config.js';

export default {
  onLaunch() {
    console.log('[OK] SmartLock 启动');
    console.log('     productId :', cfg.productId);
    console.log('     deviceName:', cfg.deviceName);
    if (!cfg.productId || !cfg.deviceName || !cfg.accessKey) {
      uni.showModal({
        title: '配置缺失',
        content: '请检查 api/config.js 中三件套是否填写完整'
      });
    }
  },
  onShow() {
    console.log('[App.onShow]');
  },
  onHide() {
    console.log('[App.onHide]');
  },
  onError(err) {
    console.error('[App.onError]', err);
  }
};
</script>

<style>
/* 全局样式：所有 vue 页面可继承 */
page {
  --primary:        #1976d2;
  --primary-dark:   #0d47a1;
  --primary-light:  #bbdefb;
  --success:        #43a047;
  --warning:        #fb8c00;
  --danger:         #e53935;
  --bg:             #f5f5f5;
  --card:           #ffffff;
  --text:           #212121;
  --text-secondary: #757575;
  --divider:        #e0e0e0;
  background: var(--bg);
}

/* === 通用卡片 === */
.card {
  background: var(--card);
  border-radius: 16rpx;
  padding: 32rpx;
  margin: 24rpx;
  box-shadow: 0 2rpx 8rpx rgba(0, 0, 0, 0.06);
}
.card-title {
  font-size: 32rpx; font-weight: 600;
  color: var(--text); margin-bottom: 16rpx;
}
.card-subtitle {
  font-size: 24rpx; color: var(--text-secondary);
  margin-bottom: 24rpx;
}

/* === 通用按钮 === */
.btn {
  display: inline-block; padding: 24rpx 48rpx;
  border-radius: 12rpx; font-size: 32rpx;
  font-weight: 500; text-align: center;
  border: none; line-height: 1.5;
}
.btn::after { border: none; }
.btn-primary  { background: var(--primary); color: white; }
.btn-success  { background: var(--success); color: white; }
.btn-danger   { background: var(--danger);  color: white; }
.btn-warning  { background: var(--warning); color: white; }
.btn-block    { display: block; width: 100%; box-sizing: border-box; }
.btn-lg       { padding: 32rpx 48rpx; font-size: 36rpx; }

/* === 状态点 === */
.status-dot { width: 16rpx; height: 16rpx; border-radius: 50%; display: inline-block; margin-right: 12rpx; vertical-align: middle; }
.status-online  { background: var(--success); animation: pulse 2s infinite; }
.status-offline { background: var(--text-secondary); }
.status-warning { background: var(--warning); animation: pulse 1s infinite; }
.status-danger  { background: var(--danger); animation: pulse 0.5s infinite; }
@keyframes pulse {
  0% { opacity: 1; } 50% { opacity: 0.4; } 100% { opacity: 1; }
}

/* === 标签徽章 === */
.badge {
  display: inline-block; padding: 6rpx 16rpx;
  border-radius: 24rpx; font-size: 22rpx;
  background: var(--primary-light); color: var(--primary-dark);
  margin-right: 12rpx;
}

/* === 行布局 === */
.row { display: flex; align-items: center; padding: 16rpx 0; }
.row-between { display: flex; align-items: center; justify-content: space-between; padding: 16rpx 0; }
.row .label { color: var(--text-secondary); font-size: 28rpx; }
.row .value { color: var(--text); font-size: 28rpx; font-weight: 500; }
</style>
