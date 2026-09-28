#ifndef __IMU_H
#define	__IMU_H


#include "ALL_DATA.h"

#define DEG2RAD  0.017453293f //度转弧度π/180 
#define RAD2DEG  57.29578f    //弧度转度180/π 

typedef struct {
	float yaw;
	float pitch;
	float roll;
	float yaw_mag; //单独由磁力计的出来的角度
	float Cos_Roll;
	float Sin_Roll;
	float Cos_Pitch;
	float Sin_Pitch;
	float Cos_Yaw;
	float Sin_Yaw;
}_st_IMU;



	typedef struct V{
				float x;
				float y;
				float z;
				} MPUDA;

extern float yaw_control;
extern float Yaw_Correct;
extern _st_IMU IMU;
extern float GetAccz(void);
extern void GetAngle(const _st_Mpu *pMpu,_st_AngE *pAngE, float dt);
extern	 MPUDA  Gravity, Acc, Gyro,AccGravity;

#endif
