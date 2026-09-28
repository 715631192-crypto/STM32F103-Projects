<template>
	<view class="gauge-box" :style="{'background-color': bgColor}">
		<canvas :canvas-id="config.id" :style="{'width' : _width + 'px','height' : _width + 'px'}"></canvas>
		<view class="gauge-btn" v-if="_status" :class="{'gauge-btn-active':_dStatus}">{{_dStatus ? '在线' : '离线'}}</view>
	</view>
</template>

<script>
const mainDefault = {
	bgColor:'rgba(255,255,255,1)',
	startAngle: 0.75,
	endAngle: 0.25,
	width: uni.upx2px(350),
	status: 0,//设备是否在线
	padding: 10,
	min: 0,
	max: 100,
	value: 0,
	unit: false,
	name: '图例标题',
	detail:{},
	axisTick:[]
};
const detailDefault = {//标题、数值、单位设置
	title: {//name字体位置设置
		offsetCenter:[0,uni.upx2px(-40)],//距离圆心直径偏移
		color:'#888888',
		fontSize: uni.upx2px(24),
		fontWeight:'normal',
		textAlign:'center'
	},
	value: {
		color:'#323232',
		fontSize: uni.upx2px(48),
		fontWeight:'bolder',
		offsetCenter:[0,uni.upx2px(40)],//距离圆心直径偏移
		textAlign:'center'
	},
	unit: {
		color:'#323232',
		fontSize: uni.upx2px(22),
		fontWeight:'normal',
		offsetCenter:[0,uni.upx2px(40)],//距离圆心直径偏移
		textAlign:'center'
	}
};
const axisTickDefault = {//轴刻度线
	width: uni.upx2px(25),//轴长
	number: 6,//轴数量（相当于几等分）
	color: '#369c96,#d5d5d5',//轴颜色(第一个值--指针之前的颜色，之后的颜色)
	subNumber: 10,//一个大刻度分成几个小刻度
	subWidth:  uni.upx2px(25),
	subHeight:  1,
	padding: uni.upx2px(10)//刻度距离边距
};
const axisTickDefaultSmall = {//轴刻度线(里圈)
	width: uni.upx2px(10),//轴长
	number: 6,//轴数量（相当于几等分）
	color: '#d5d5d5',//轴颜色(第一个值--指针之前的颜色，之后的颜色)
	subNumber: 10,//大刻度之间分成几个小刻度
	subWidth:  uni.upx2px(6),
	subHeight:  1,
	padding: uni.upx2px(25)//刻度距离边距
};
export default {
	name:'bestGauge',
	data:function(){
		return {
			gaugeOption:{}
		}
	},
	props:{
		config:{
			type: Object,
			default:() => {
				return {};
			}
		},
		bgColor:{
			type: String,
			default:'#fff'
		}
	},
	computed:{
		_width:function(){
			return this.config.width || mainDefault.width
		},
		_status:function(){
			return String(this.config.status) !== 'false' ? true : false;
		},
		_dStatus:function(){
			return String(this.config.status) === 'true' || this.config.status === 1 ? 1 : 0;
		}
	},
	watch:{
		config:{
			handler(newVal, oldVal) {
				this.initCharts();
			},
			deep: true
		}
	},
	mounted:function(){
		this.initCharts();
	},
	methods: {
		initCharts:function(){
			// 自动填充数据
			var gaugeOption = this.fillInData();
			var uChartsGauge = null;
			uChartsGauge = uni.createCanvasContext(gaugeOption.id, this);
			uChartsGauge.fillStyle = gaugeOption.bgColor;
			uChartsGauge.fillRect(0,0,gaugeOption.width,gaugeOption.width);
			uChartsGauge.save();
			this.drawGauge(gaugeOption,uChartsGauge);
		},
		drawGauge:function(options,ctx){
			var centerPosition = {
				x: options.width / 2,
				y: options.width / 2
			};
			// 总角度数（为什么要加1）假装下图是个圆  0.75PI ~ 0.25PI 实际间隔1.5PI
					//   1.5PI
					//     ——
			  // 1.0PI	|	   |  0PI
					//     ——
					//   0.5PI
			var totalAngle = options.startAngle - options.endAngle + 1;
			// 计算半径
			var radius = Math.min(centerPosition.x, centerPosition.y);

			//画刻度线
			for(let idx = 0,len = options.axisTick.length;idx < len;idx++){
				var gaugeOption = options.axisTick[idx];
				radius -= (gaugeOption.padding + gaugeOption.width / 2);
				var criticalPoint = Math.floor(options.value / (options.max / gaugeOption.number));//大刻度临界点
				var subCriticalPoint = Math.floor(options.value / (options.max / gaugeOption.subNumber / gaugeOption.number));//小刻度临界点
				if(options.value * 1 === 0){
					criticalPoint = -1;//防止0的时候被上颜色
					subCriticalPoint = -1;
				}
				var colors = gaugeOption.color.split(',');
				colors[1] = colors[1] || colors[0];
				var splitAngle = totalAngle / gaugeOption.number;
				var childAngle = totalAngle / gaugeOption.number / gaugeOption.subNumber;
				var startX = -radius - gaugeOption.width * 0.5;
				var endX = -radius - gaugeOption.width * 0.5 + gaugeOption.width;
				var childEndX = -radius - gaugeOption.width * 0.5 + gaugeOption.subWidth;

				// 画大刻度
				let maxScaleData = {
					...centerPosition,
					startX,
					endX,
					splitAngle,
					criticalPoint,
					colors,
					startAngle: options.startAngle,
					width: gaugeOption.subHeight,
					number: gaugeOption.number,
				}
				this.drawScale(ctx, maxScaleData);

				// 画小刻度
				let minScaleData = {
					...centerPosition,
					startX,
					colors,
					criticalPoint: subCriticalPoint,
					endX: childEndX,
					splitAngle:childAngle,
					startAngle: options.startAngle,
					width: gaugeOption.subHeight,
					number: gaugeOption.number * gaugeOption.subNumber,
				}
				this.drawScale(ctx, minScaleData);
			}
			// 标题
			var titleObj = {
				...options.detail.title,
				...centerPosition,
				name: options.name
			}
			this.drawText(ctx,titleObj);

			// 数值
			var _fillText = (options.value * 1) === 0 ? 0 : (options.value * 1).toFixed(1);
			var valueObj = {
				...options.detail.value,
				...centerPosition,
				name: _fillText
			}
			this.drawText(ctx,valueObj);

			// 单位
			if(String(options.unit) !== 'false'){
				let unitObj = {
					...options.detail.unit,
					...centerPosition,
					name: options.unit
				}
				var valueLength = (options.value * 1).toFixed(1).length;
				var _oftX = ((valueLength - 1.5) / 2) * valueObj.fontSize;//需要保证单位在数值后面
				unitObj.offsetCenter[0] += _oftX;
				this.drawText(ctx,unitObj);
			}

			ctx.draw();
		},
		// 画字
		drawText:function(ctx,data){
			let { fontSize, fontWeight, color, textAlign, offsetCenter, x, y, name } = data;
			// 画标题
			ctx.beginPath();
			// 设置字体
			ctx.font = fontWeight + " " +fontSize + "px MicrosoftYaHei";
			// 设置颜色
			ctx.fillStyle = color;
			// 设置水平对齐方式
			ctx.textAlign = textAlign;
			// 设置中心点
			ctx.translate(x, y);
			// 绘制文字（参数：要写的字，x坐标，y坐标）
			ctx.fillText(name,...offsetCenter);
			ctx.closePath();
			ctx.stroke();
			ctx.restore();
			ctx.save();
		},
		// 画刻度
		drawScale:function(ctx, data){
			let { number, criticalPoint, startX, endX, splitAngle, width, x, y, startAngle,colors } = data;
			// 设置中心点
			ctx.translate(x, y);
			ctx.rotate((startAngle - 1) * Math.PI);
			for (let i = 0; i <= number; i++) {
				ctx.beginPath();
				ctx.setStrokeStyle(i <= criticalPoint ? colors[0] : colors[1]);
				ctx.setLineWidth(width);
				ctx.moveTo(startX, 0);
				ctx.lineTo(endX, 0);
				ctx.stroke();
				ctx.rotate(splitAngle * Math.PI);
			}
			// 清除状态
			ctx.restore();
			ctx.save();
		},
		fillInData:function(){
			var gaugeOption = this.deepClone(mainDefault, this.config);
			for(var _k in detailDefault){
				gaugeOption.detail[_k] = this.deepClone(detailDefault[_k], gaugeOption.detail[_k])
			}
			if(gaugeOption.axisTickLength === undefined || gaugeOption.axisTickLength === null){
				if(!(gaugeOption.axisTick instanceof Array)){
					gaugeOption.axisTick = [gaugeOption.axisTick]
				}
			}else{
				gaugeOption.axisTick = gaugeOption.axisTick || [];
			}
			var axisTickLength = gaugeOption.axisTickLength || gaugeOption.axisTick.length || 1;
			var axisTick = [].concat(gaugeOption.axisTick);
			for(var idx = 0,len = axisTickLength;idx < len;idx++){
				var defaultData = idx == 0 ? axisTickDefault : axisTickDefaultSmall;
				axisTick[idx] = axisTick[idx] || {};
				if(idx > 0 && !axisTick[idx].padding){
					axisTick[idx].padding = +axisTick[idx -1].width;
				}
				axisTick[idx] = this.deepClone(defaultData, axisTick[idx]);
			}
			gaugeOption.axisTick = axisTick
			return gaugeOption;
		},
		deepClone:function(source,target){//待优化代码
			var _obj = {};
			source = source || {};
			target = target || {};
			Object.assign(_obj, source, target);
			return JSON.parse(JSON.stringify(_obj));
		}
	}
};
</script>

<style>
	.gauge-box {
		position: relative;
		width: 100%;
		height: 100%;
		display: flex;
		align-items: center;
		justify-content: center;
		text-align: center;
	}
	.gauge-box canvas{
		width: 100%;
		margin: 0 auto;
		height: auto;
		display: block;
	}
	.gauge-btn{
		position: absolute;
		bottom: 60upx;
		left: 50%;
		width: 90upx;
		margin-left: -45upx;
		border-radius: 17upx;
		border: solid 1px #d5d5d5;
		color: #adadad;
		font-size: 22upx;
		line-height: 24upx;
		color: #adadad;
		padding: 4upx 0 4upx 14upx;
		text-align: center;
		box-sizing: border-box;
	}
	.gauge-btn:before{
		position: absolute;
		left: 15upx;
		width: 8upx;
		height: 8upx;
		top: 50%;
		margin-top: -4upx;
		border-radius: 100%;
		content: '';
		background-color: #909090;
	}
	.gauge-btn.gauge-btn-active{
		color: #323232;
		border-color: #43bb6b;
	}
	.gauge-btn.gauge-btn-active:before{
		background-color: #43bb6b;
	}
</style>
