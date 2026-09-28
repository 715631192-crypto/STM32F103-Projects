#ifndef _ALL_USER_DATA_H_
#define _ALL_USER_DATA_H_

typedef   signed          char int8_t;
typedef   signed short     int int16_t;
typedef   signed           int int32_t;
typedef   signed       long long int64_t;

    /* exact-width unsigned integer types */
typedef unsigned          char uint8_t;
typedef unsigned short     int uint16_t;
typedef unsigned           int uint32_t;
typedef unsigned       long long uint64_t;


#define NULL 0
extern volatile uint32_t SysTick_count;
extern volatile uint8_t spl_flag;
extern volatile uint32_t ST_CpuID;

typedef struct
{
 float Input_Butter[3];
 float Output_Butter[3];
}Butter_BufferData;

typedef struct
{
 const float a[3];
 const float b[3];
}Butter_Parameter;


typedef struct{
	int16_t accX;
	int16_t accY;
	int16_t accZ;
	int16_t gyroX;
	int16_t gyroY;
	int16_t gyroZ;
}_st_Mpu;


typedef struct{
	int16_t magX;
	int16_t magY;
	int16_t magZ;
}_st_Mag;


typedef struct{
	float rate;
	float height;
}High;


typedef struct{
	float roll;
	float pitch;
	float yaw;
}_st_AngE;
 typedef struct
{	
//		 struct{  //角度数据 
//			float roll;
//			float pitch;
//			float yaw;
//		}Angle;
		 struct{  //高度数据
			float rate;
			float bara_height; 
			float ultra_height;
			float ultra_baro_height;
		}High;		 
}_st_FlightData;


typedef struct
{
	uint16_t roll;
	uint16_t pitch;
	uint16_t thr;
	uint16_t yaw;
	uint16_t AUX1;
	uint16_t AUX2;
	uint16_t AUX3;
	uint16_t AUX4;	
	uint16_t AUX5;
	uint16_t AUX6;
	uint16_t AUX7;
}_st_Remote;




typedef volatile struct
{
	float desired;     ///期望
	float offset;      //
	float prevError;    // 上次偏差
	float integ;        //误差积分累加值
	float kp;           //p参数
	float ki;           //i参数
	float kd;           //d参数
	float IntegLimitHigh;       //< integral limit
	float IntegLimitLow;
	float measured;     //pid反馈量
	float out;
	float OutLimitHigh;
	float OutLimitLow;	
	float Control_OutPut;//控制器总输出
	float Last_Control_OutPut;//上次控制器总输出
	float Control_OutPut_Limit;//输出限幅
		/***************************************/
	float Last_FeedBack;//上次反馈值
	float Dis_Err;//微分量
	float Dis_Error_History[5];//历史微分量
	float Err_LPF;
	float Last_Err_LPF;
	float Dis_Err_LPF;

//	int8_t Err_Limit_Flag :1;//偏差限幅标志
//	int8_t Integrate_Limit_Flag :1;//积分限幅标志
//	int8_t Integrate_Separation_Flag :1;//积分分离标志		
  Butter_BufferData Control_Device_LPF_Buffer;//控制器低通输入输出缓冲	
}PidObject;


typedef volatile struct
{
	uint8_t unlock;
	uint32_t  slock_flag;
	uint8_t height_lock:1;
	uint8_t take_off:1;
	uint8_t take_down:1;

}_st_ALL_flag;



typedef volatile struct
{
	uint8_t AccOffset :1;  //校准命令
	uint8_t GyroOffset :1;
	uint8_t MagOffset :1;
	uint8_t six_acc_offset; //六面校准
	enum{ 								 //飞行模式切换命令
			LOCK = 0x00,				//锁定模式
			NORMOL, 		//基本模式		
			HEIGHT,			//定高模式
			Flow_POSITION,   //GPS定点模式
	}FlightMode; //飞行模式
}st_Command;



extern _st_Remote Remote;
extern _st_Mpu MPU6050;
extern _st_Mag AK8975; //保留，需外接磁力计
extern _st_AngE Angle;


extern _st_ALL_flag ALL_flag;


extern	PidObject pidRateX;
extern	PidObject pidRateY;
extern	PidObject pidRateZ;

extern	PidObject pidPitch;
extern	PidObject pidRoll;
extern	PidObject pidYaw;

extern	PidObject pidHeightRate;
extern	PidObject pidHeightHigh;


extern PidObject Flow_PosPid_x; 
extern PidObject Flow_PosPid_y;

extern PidObject Flow_SpeedPid_x;
extern PidObject Flow_SpeedPid_y;

extern _st_FlightData FlightData;
//飞控命令
extern st_Command Command;



void GetLockCode(void);
void pid_param_Init(void);
#endif

