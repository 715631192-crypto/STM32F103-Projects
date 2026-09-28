

## dui-line-chart 移动端简单折线图
> **组件名：dui-line-chart**

一个移动端简单好用的、好看的折线图，支持负数


### 基本用法

```html
<template>
	<div style="height: 350px">
		<dui-line-chart :option="option"></dui-line-chart>
	</div>
</template>
```
```js
const option = {
	title: {
		text: '每周销量报表',	 //主标题
		subText: '这里是副标题',  //副标题
		textStyle: {				
			color: '#ff007f',	  //主标题颜色
			fontSize: 16,		  //主标题字体大小
			textAlign: 'left'     //主标题水平对齐（可选值：'left'、'center'、'right')
		},
		subTextStyle: {
			color: '#7d7d7d',     //副标题颜色
			fontSize: 12,         //副标题字体大小
			textAlign: 'left'     //副标题水平对齐（可选值：'left'、'center'、'right')
		}
	},
	tooltip: {
		show: true, 			  //是否显示提示框组件
	},
	xAxis: {
		show: true, 			  //是否显示x轴
		data: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'] //x轴分类
	},
	yAxis: {
		show: true, 			  //是否显示y轴
	},
	series: [
		{
			name: '张同学',		  //折线系列名称，用于tooltip的显示
			data: [119, 20, -9, 21, 59, 11, 227],//折线图数据
			type: 'line',
			lineStyle:{
				color:"#1164ff",  //折线颜色
				width:2			  //折线大小
			}
		},
		{
			name: '冀同学',
			data: [219, -5, -55, -27, -21, -123, 527],
			type: 'line',
		},
	]
};
```

###注意
可不要用该组件展示太多数据哦，毕竟移动端


