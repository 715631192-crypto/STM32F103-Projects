#ifndef __ATTITUDE__H_
#define __ATTITUDE__H_

typedef struct{ 
	 //经过滤波后用于姿态解算的角速度
  	float gyroX_IMU;
  	float gyroY_IMU;
  	float gyroZ_IMU;	
		//滤波后用于校准使用的加速度值
		float accX_correct;
		float accY_correct;
		float accZ_correct;		
		//经过椭球校正后的三轴加速度量
		float accX_origion;
		float accY_origion;
		float accZ_origion;	
//以下数据都经椭球矫正后再处理
//-----------------------------------------//用于传感器 比如GPS，气压计	
		float accX_control;
		float accY_control;
		float accZ_control;		
		//经过滤波用于传感器的控制,比如气压计等等
		float acc_yaw_sensor;
		float acc_pitch_sensor;
		float acc_roll_sensor;				
	//-----------------------------------------//
		float acc_high_feedback;   //直直方向上的加速度用于高度控制PID
		float acc_pitch_feedback;
		float acc_roll_feedback;
	//-----------------------------------------//用于姿态解算的加速度	
		float accX_IMU;
		float accY_IMU;
		float accZ_IMU;
	//-----------------------------------------	
	  float Acceleration_Length;//三维加速度向量模长
		float acc_x_earth; //地理坐标系下的加速度X Y，即经纬度上的加速度
		float acc_y_earth;
		float acc_x_body; //机头标系下的加速度X Y，即横滚角俯仰上的xy
		float acc_y_body;		
		//用于观测
		float acc_yaw;
		float acc_pitch;
		float acc_roll;
}_st_Attitude;

//加速度六面校准椭球矫正参数
extern float K[3];//默认标度误差
extern float B[3];//默认零位误差


extern _st_Attitude  Attitude;

extern void GYRO_IMU_Filter(short ax,short ay,short az);//角速度低通滤波后用于姿态解算
extern void ACC_IMU_Filter(short ax,short ay,short az);
extern void  SINS_Prepare(void);
#endif
