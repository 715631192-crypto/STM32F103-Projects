<template>
	<view class="container">
		<view class="header">
			<text class="title">DHT11 温湿度监测</text>
		</view>

		<view class="data-card">
			<view style="display:flex;gap:30rpx;">
				<bestGauge
					class="best"
					style="flex:1;height:320rpx;"
					:config="tempConfig"
				></bestGauge>
				<bestGauge
					class="best"
					style="flex:1;height:320rpx;"
					:config="humiConfig"
				></bestGauge>
			</view>
		</view>

		<!-- 温湿度折线图区域 -->
		<view class="data-card">
			<dui-line-chart :option="chartOption" style="height:350rpx;"></dui-line-chart>
		</view>

		<view class="device-card">
			<view class="device-item">
				<view class="device-header">
					<view class="device-indicator" :class="{ 'on': ledStatus, 'off': !ledStatus }"></view>
					<text class="device-name">💡 LED灯</text>
					<text class="device-state">{{ ledStatus ? '已开启' : '已关闭' }}</text>
				</view>
				<view class="btn-group">
					<button class="btn btn-on" :disabled="requestLoading || ledStatus" @click="setLedStatus(true)">开启</button>
					<button class="btn btn-off" :disabled="requestLoading || !ledStatus" @click="setLedStatus(false)">关闭</button>
				</view>
			</view>

			<view class="device-item">
				<view class="device-header">
					<view class="device-indicator buzzer-indicator" :class="{ 'on': buzzerStatus, 'off': !buzzerStatus }"></view>
					<text class="device-name">🔔 蜂鸣器</text>
					<text class="device-state">{{ buzzerStatus ? '已开启' : '已关闭' }}</text>
				</view>
				<view class="btn-group">
					<button class="btn btn-on" :disabled="requestLoading || buzzerStatus" @click="setBuzzerStatus(true)">开启</button>
					<button class="btn btn-off" :disabled="requestLoading || !buzzerStatus" @click="setBuzzerStatus(false)">关闭</button>
				</view>
			</view>
		</view>

		<!-- 删除手动刷新按钮 -->
	</view>
</template>

<script>
	import duiLineChart from "@/uni_modules/dui-line-chart/components/dui-line-chart/dui-line-chart.vue";
	import bestGauge from "@/components/best-gauge/best-gauge.vue";
	const {createCommonToken} = require("@/static/js/key.js")
	export default {
		components:{
			bestGauge,
			duiLineChart
		},
		data() {
			return {
				author_key: 'MDU3ZDg1NWE2ZTExNDE3YWJhYzY2NzYxZWMyNDJiNTU=',
				version: '2022-05-01',
				user_id: '539993',
				token: '',
				temp: '--',
				humi: '--',
				ledStatus: false,
				buzzerStatus: false,
				requestLoading: false,
				timer: null, // 自动刷新定时器
				// 温度仪表盘配置
				tempConfig:{
					id: 'gauge-canvas-temp',
					value: 0,
					status: true,
					max: 50,
					min: 0,
					name: '温度 ℃'
				},
				// 湿度仪表盘配置
				humiConfig:{
					id: 'gauge-canvas-humi',
					value: 0,
					status: true,
					max: 100,
					min: 0,
					name: '湿度 %'
				},
				// 折线图原始数据缓存
				timeList: [],
				tempList: [],
				humiList: [],
				MAX_RECORD: 60, //最多保存60条记录（1分钟数据）
				// 图表配置
				chartOption: {
					title: {
						text: '温湿度实时曲线',
						subText: '最近一分钟采集记录',
						textStyle: {
							color: '#333',
							fontSize: 16
						},
						subTextStyle: {
							color: '#7d7d7d',
							fontSize: 12
						}
					},
					tooltip: {
						show: true
					},
					xAxis: {
						show: true,
						data: []
					},
					yAxis: {
						show: true
					},
					series: [
						{
							name: '温度 ℃',
							data: [],
							type: 'line',
							lineStyle:{
								color:"#ff6b6b",
								width:2
							}
						},
						{
							name: '湿度 %',
							data: [],
							type: 'line',
							lineStyle:{
								color:"#4dabf7",
								width:2
							}
						}
					]
				}
			}
		},
		onLoad() {
			this.getToken()
			this.$nextTick(()=>{
				this.getDeviceData()
			})
			// 开启5秒自动刷新
			this.timer = setInterval(()=>{
				this.getDeviceData()
			},5000)
		},
		// 页面卸载清除定时器，避免内存泄漏
		onUnload() {
			if(this.timer){
				clearInterval(this.timer)
				this.timer = null
			}
		},
		methods: {
			getToken() {
				const params = {
					author_key: this.author_key,
					version: this.version,
					user_id: this.user_id
				}
				this.token = createCommonToken(params)
			},
			getDeviceData() {
				if (!this.token) {
					console.warn("token为空，禁止发起请求");
					return;
				}
				if (this.requestLoading) return
				this.requestLoading = true
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/query-device-property',
					method: 'GET',
					data: { // 【修复】GET 请求不能用data，改为params
						product_id: 'u5u1bJ8h3C',
						device_name: 'DHT11'
					},
					header: {
						authorization: this.token
					},
					success: (res) => {
						console.log('响应数据：', res.data)
						if (res.data.code == 0) {
							const dataArray = res.data.data
							let newTemp = 0;
							let newHumi = 0;
							dataArray.forEach(item => {
								switch (item.identifier) {
									case 'temp':
										this.temp = item.value
										newTemp = Number(item.value)
										this.tempConfig.value = newTemp
										break
									case 'humi':
										this.humi = item.value
										newHumi = Number(item.value)
										this.humiConfig.value = newHumi
										break
									case 'led':
										this.ledStatus = item.value === 'true'
										break
									case 'buzzer':
										this.buzzerStatus = item.value === 'true'
										break
								}
							})
							// ==========追加折线图数据==========
							const now = new Date();
							const timeStr = `${now.getHours()}:${now.getMinutes()}:${now.getSeconds()}`;
							this.timeList.push(timeStr);
							this.tempList.push(newTemp);
							this.humiList.push(newHumi);
							// 超出最大数量，移除最旧一条
							if(this.timeList.length > this.MAX_RECORD){
								this.timeList.shift();
								this.tempList.shift();
								this.humiList.shift();
							}
							// 更新图表配置
							this.chartOption.xAxis.data = this.timeList;
							this.chartOption.series[0].data = this.tempList;
							this.chartOption.series[1].data = this.humiList;
						} else {
							uni.showToast({
								title: `获取失败：${res.data.msg}`,
								icon: 'none'
							})
						}
					},
					fail: () => {
						uni.showToast({
							title: '网络请求失败',
							icon: 'none'
						})
					},
					complete: () => {
						this.requestLoading = false
					}
				})
			},
			setLedStatus(value) {
				if (!this.token) return;
				if (this.requestLoading) return
				this.requestLoading = true
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/set-device-property',
					method: 'POST',
					data: {
						product_id: 'u5u1bJ8h3C',
						device_name: 'DHT11',
						params: {
							led: value
						}
					},
					header: {
						authorization: this.token
					},
					success: (res) => {
						console.log(res.data)
						if (res.data.code == 0) {
							this.ledStatus = value
							uni.showToast({
								title: value ? 'LED已开启' : 'LED已关闭',
								icon: 'success'
							})
						} else {
							uni.showToast({
								title: `操作失败：${res.data.msg}`,
								icon: 'none'
							})
						}
					},
					fail: () => {
						uni.showToast({
							title: '网络请求失败',
							icon: 'none'
						})
					},
					complete: () => {
						this.requestLoading = false
					}
				})
			},
			setBuzzerStatus(value) {
				if (!this.token) return;
				if (this.requestLoading) return
				this.requestLoading = true
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/set-device-property',
					method: 'POST',
					data: {
						product_id: 'u5u1bJ8h3C',
						device_name: 'DHT11',
						params: {
							buzzer: value
						}
					},
					header: {
						authorization: this.token
					},
					success: (res) => {
						console.log(res.data)
						if (res.data.code == 0) {
							this.buzzerStatus = value
							uni.showToast({
								title: value ? '蜂鸣器已开启' : '蜂鸣器已关闭',
								icon: 'success'
							})
						} else {
							uni.showToast({
								title: `操作失败：${res.data.msg}`,
								icon: 'none'
							})
						}
					},
					fail: () => {
						uni.showToast({
							title: '网络请求失败',
							icon: 'none'
						})
					},
					complete: () => {
						this.requestLoading = false
					}
				})
			}
		}
	}
</script>

<style>
	.container {
		display: flex;
		flex-direction: column;
		align-items: center;
		padding: 40rpx 30rpx;
		background: linear-gradient(180deg, #f0f4ff 0%, #e8eeff 100%);
		min-height: 100vh;
		box-sizing: border-box;
	}

	.header {
		margin-bottom: 40rpx;
	}

	.title {
		font-size: 44rpx;
		font-weight: bold;
		color: #333;
	}

	.data-card {
		width: 100%;
		background: #fff;
		border-radius: 20rpx;
		padding: 30rpx;
		box-shadow: 0 4rpx 20rpx rgba(0, 0, 0, 0.08);
		margin-bottom: 30rpx;
	}

	.device-card {
		width: 100%;
		background: #fff;
		border-radius: 20rpx;
		padding: 30rpx;
		box-shadow: 0 4rpx 20rpx rgba(0, 0, 0, 0.08);
		margin-bottom: 30rpx;
	}

	.device-item {
		padding: 20rpx 0;
		border-bottom: 1rpx solid #f0f0f0;
	}

	.device-item:last-child {
		border-bottom: none;
	}

	.device-header {
		display: flex;
		align-items: center;
		margin-bottom: 24rpx;
	}

	.device-indicator {
		width: 24rpx;
		height: 24rpx;
		border-radius: 50%;
		margin-right: 16rpx;
		box-shadow: 0 0 10rpx rgba(0, 0, 0, 0.2);
	}

	.device-indicator.on {
		background: #51cf66;
		box-shadow: 0 0 20rpx rgba(81, 207, 102, 0.6);
	}

	.device-indicator.off {
		background: #dee2e6;
	}

	.buzzer-indicator.on {
		background: #ffa94d;
		box-shadow: 0 0 20rpx rgba(255, 169, 77, 0.6);
	}

	.device-name {
		font-size: 30rpx;
		color: #333;
		font-weight: 500;
		flex: 1;
	}

	.device-state {
		font-size: 26rpx;
		color: #888;
	}

	.btn-group {
		display: flex;
		gap: 20rpx;
	}

	.btn {
		flex: 1;
		font-size: 28rpx;
		border-radius: 12rpx;
		padding: 20rpx 0;
		border: none;
	}

	.btn-on {
		background: #51cf66;
		color: #fff;
	}

	.btn-on:disabled {
		background: #b2f2bb;
		color: #fff;
	}

	.btn-off {
		background: #ff6b6b;
		color: #fff;
	}

	.btn-off:disabled {
		background: #ffc9c9;
		color: #fff;
	}
</style>
