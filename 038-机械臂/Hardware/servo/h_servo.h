#ifndef __H_SERVO_H
#define __H_SERVO_H

#include "main.h"

// 0号舵机定义
#define SERVO0_PIN GPIO_Pin_3
#define SERVO0_GPIO_PORT GPIOB
#define SERVO0_GPIO_CLK RCC_APB2Periph_GPIOB

// 1号舵机定义
#define SERVO1_PIN GPIO_Pin_8
#define SERVO1_GPIO_PORT GPIOB
#define SERVO1_GPIO_CLK RCC_APB2Periph_GPIOB

// 2号舵机定义
#define SERVO2_PIN GPIO_Pin_9
#define SERVO2_GPIO_PORT GPIOB
#define SERVO2_GPIO_CLK RCC_APB2Periph_GPIOB

// 3号舵机定义
#define SERVO3_PIN GPIO_Pin_6
#define SERVO3_GPIO_PORT GPIOB
#define SERVO3_GPIO_CLK RCC_APB2Periph_GPIOB

// 4号舵机定义
#define SERVO4_PIN GPIO_Pin_7
#define SERVO4_GPIO_PORT GPIOB
#define SERVO4_GPIO_CLK RCC_APB2Periph_GPIOB

// 5号舵机定义
#define SERVO5_PIN GPIO_Pin_4
#define SERVO5_GPIO_PORT GPIOB
#define SERVO5_GPIO_CLK RCC_APB2Periph_GPIOB

/* 控制舵机引脚输出的宏 */
#define SERVO0_PIN_SET(level) GPIO_WriteBit(SERVO0_GPIO_PORT, SERVO0_PIN, level)
#define SERVO1_PIN_SET(level) GPIO_WriteBit(SERVO1_GPIO_PORT, SERVO1_PIN, level)
#define SERVO2_PIN_SET(level) GPIO_WriteBit(SERVO2_GPIO_PORT, SERVO2_PIN, level)
#define SERVO3_PIN_SET(level) GPIO_WriteBit(SERVO3_GPIO_PORT, SERVO3_PIN, level)
#define SERVO4_PIN_SET(level) GPIO_WriteBit(SERVO4_GPIO_PORT, SERVO4_PIN, level)
#define SERVO5_PIN_SET(level) GPIO_WriteBit(SERVO5_GPIO_PORT, SERVO5_PIN, level)

/* 舵机数量 */
#define DJ_NUM 8  //为8是因为定时器中断计算pwm周期需要

/* 舵机结构体 */
typedef struct {
    uint16_t aim;   // 执行目标
    uint16_t time;  // 执行时间
    float cur;      // 当前值
    float inc;      // 增量
} servo_t;

extern servo_t duoji_doing[DJ_NUM]; //舵机结构体数组

/********舵机相关函数声明********/
void servo_init(void); //舵机引脚初始化
void servo_pin_set(u8 index, BitAction level); //设置舵机引脚电平
void duoji_doing_set(u8 index, int aim, int time); //设置舵机参数
void servo_inc_offset(u8 index); //设置舵机每次增加的偏移量

#endif //__H_SERVO_H
