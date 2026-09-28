# SmartLock_App —— HBuilder X 打包的安卓 APP

uni-app + Vue 3 项目。对接 OneNET 物联网平台 + STM32 智能门锁。

## 功能列表

| 页面 | 功能 |
|------|------|
| **首页** | 设备状态卡片（门/锁/报警），30s 自动刷新 |
| **远程开锁** | 一键大圆按钮开锁；进阶上锁/解锁锁定 |
| **临时密码** | 4 种模式（5min / 1h / 24h / 一次性），自动生成 + 下发 |
| **开锁日志** | 拉取 OneNET 设备日志，3 分类过滤 |
| **配置** | 设备三元组展示 + 推送开关 |

## 工程结构

```
SmartLock_App/
├── App.vue                  # 根组件 + 全局样式
├── main.js                  # Vue 3 入口
├── pages.json               # 路由 + tabBar 配置
├── manifest.json            # Android/iOS 配置 (包名、权限、图标)
├── pages/                   # 5 个页面 (Vue 单文件组件)
│   ├── index/index.vue
│   ├── unlock/unlock.vue
│   ├── tempPwd/tempPwd.vue
│   ├── logs/logs.vue
│   └── config/config.vue
├── api/                     # 业务层
│   ├── index.js             # OneNET HTTP API 封装
│   ├── sha1.js              # HMAC-SHA1 + Base64 (纯 JS 实现)
│   └── config.js            # 三件套 + 业务配置
├── common/                  # 通用工具
│   └── format.js            # 时间格式化
└── static/icons/            # tabBar 图标 (8 个 81×81 PNG)
```

## 一键上手步骤

### 第 1 步：导入工程到 HBuilder X

1. 打开 **HBuilder X**
2. **文件 → 导入 → 从本地目录导入**
3. 选择 `C:\Users\71563\Desktop\智能门锁\SmartLock_App\`
4. 选择 **Vue 3 + Vite** 的 uni-app 项目模式导入

### 第 2 步：内置浏览器跑通

按 **Ctrl+R**（运行 → 运行到浏览器 → Chrome）：

```
✓ 主页加载
✓ 顶部 tabBar 出现 4 个 tab
✓ 主页显示"已加载"+ "智能门锁"标题
```

如果浏览器里看到 "配置缺失"，说明 `api/config.js` 没填好——但我们已经填过了，不会触发。

### 第 3 步：自定义 App 图标（可选）

1. 准备 **1024×1024 PNG**（或更高分辨率）
2. 在 HBuilder X 中：**manifest.json → App 图标 → 上传**
3. 保存后 HBuilder X 自动重新生成各个分辨率的图标

### 第 4 步：打包为 Android APK

**4.1 安装打包环境（首次需要）**

1. **HBuilder X → 工具 → 插件安装**：
   - 需要 "Android 离线打包支持" 插件
   - 第一次打包时 HBuilder X 会引导下载 Android SDK

2. **安装 Java**（打包工具链依赖）：
   ```
   https://adoptium.net/  下载 JDK 17 LTS
   安装后 cmd 里验证 java -version
   ```

**4.2 配置 keystore 签名**

manifest.json → App SDK 配置 → Android：
- 应用包名：`com.smartlock.app`（可改成你自己的）
- 签名证书：选"使用自有证书"或"使用公共测试证书"
  - **公共测试证书**：HBuilder X 自带，无需设置（仅用于内部测试）
  - **自有证书**：自己生成 `.keystore` 文件，路径填"证书别名 + 证书密码"

**4.3 开始打包**

1. **发行 → 原生 App-云打包**（推荐：上传到 DCloud 云端打包，无需本地 Android SDK）
2. 选择 **Android**，勾选"打正式包"或"打测试包"
3. 点击 **打包** → 等待 2-5 分钟
4. 打包完成 → 自动下载 APK

> **提示**：如果选"本地打包"，需要装 5GB+ 的 Android SDK；云端打包只需要 HBuilder X + 微信扫码登录 DCloud。

### 第 5 步：安装到手机

1. 把下载的 `.apk` 传手机（微信/U盘/邮件都行）
2. 手机"设置 → 安全 → 安装未知应用" → 允许此来源
3. 点开 APK 文件安装
4. 打开"智能门锁" APP

### 第 6 步：实机调试

打开 HBuilder X：**运行 → 运行到手机或模拟器 → Android App-基座**

> **真机调试模式**：HBuilder X 提供一个 "HBuilder 基座" 应用（已自带 JS 引擎），你先装上这个基座 APP，然后通过它打开你的项目，**改代码即时生效**（无需重打包）。

---

## ⚠️ 调试清单

| 问题 | 排查 |
|------|------|
| 主页 `[OK] 配置加载完毕` 不打印 | 浏览器 Console 看红字；常见是 `api/config.js` 路径错 |
| 主页报 HTTP 401 | accessKey 错；对照 OneNET 控制台"产品 → AccessKey" |
| HTTP 400 device not found | productId 或 deviceName 错 |
| 编译报 "vite 找不到" | HBuilder X 自带 vite，需选 Vue 3 + Vite 模板导入 |
| 打包下载失败 | 重试，或换"本地打包" |
| tabBar 图标不显示 | 检查 `static/icons/` 下 PNG 是否存在；用户可替换成自己的 |
| 报警推送 | 本 APP 内不需 OneNET 推送，由 OneNET 规则引擎发到你的微信服务号 |

## 真机调试详细

### 通过基座调试（推荐）

1. HBuilder X → 运行 → 运行到手机或模拟器 → Android App-基座
2. 显示二维码 + USB 提示
3. **手机端**：扫描二维码，下载"标准基座"APP（首次会下载 30MB 的真机调试包）
4. 安装"标准基座"
5. **电脑端** USB 数据线连手机，开启 USB 调试（手机开发者选项）
6. HBuilder X 自动识别 → 启动基座 APP → 打开你的项目
7. **修改任意源码 → 保存 → 基座 APP 自动热更新**（无需重打包）
8. 按 HBuilder X 终端提示的"真机运行日志"看输出

### 通过"无线 ADB"调试

1. 手机开"开发者选项 → 无线调试"
2. cmd：`adb connect 192.168.x.x:5555`
3. HBuilder X 自动识别无线连接

---

## 关键设计说明

### 为什么不依赖云函数？

早期设计依赖微信云函数做后端，但 **微信云开发需要已认证小程序账号**（300元/年）。改成 uni-app 后，前端直接调 OneNET HTTP API，**少了中间层**：
- ✅ 无需第三方账号认证
- ✅ 部署更简单（一次打包就能跑）
- ⚠️ access_key 会出现在 APK 里（可被反编译）

**生产环境建议**：access_key 通过自建后端服务器转发，不要直接给前端。详见 [SmartLock_MiniApp/README.md - 安全章节]。

### 替换 tabBar 图标

占位图标是 81×81 的纯色方块。把 `static/icons/` 下的 4 个 PNG 替换成自己的（同样尺寸即可）：

```
home.png   ← 首页未选中（灰）
home_s.png ← 首页选中（蓝 1976d2）
unlock.png / unlock_s.png ← 开锁
pwd.png / pwd_s.png      ← 临时密码
log.png / log_s.png      ← 日志
```

推荐在线生成：https://icomoon.io/app/#/select

### 配置包名（在 manifest.json）

```json
"app-plus": {
  "distribute": {
    "android": {
      "packagename": "com.YOURNAME.smartlock"  ← 改成你自己的（必须以字母开头）
    }
  }
}
```

---

## 后续增强方向

| 方向 | 实现 |
|------|------|
| 多家庭支持 | 在 config.js 加一个 homeId 字段，配合多 productId 切换 |
| BLE 近距离开门 | 引入 uni-app BLE API（Android 4.3+）|
| 自定义后端中转 | 把 api/index.js 里的 OneNET URL 换成自己的 server URL |
| APK 加固 | 启用微信加固 / 360 加固，防止 accessKey 反编译 |
| 签名证书 | 上线前务必换正式证书，避免使用公共测试证书 |

## License

MIT
