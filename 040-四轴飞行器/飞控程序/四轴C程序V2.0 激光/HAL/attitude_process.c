#include "math.h"
#include "ALL_DATA.h"
#include "attitude_process.h"
#include "kalman.h"
#include "Filter.h"
#include "IMU.h"
_st_Attitude Attitude;
/*****************************************************************************************

				加速度滤波处理
	



*****************************************************************************************/


//-----Butterworth变量-----//
Butter_Parameter Butter_80HZ_Parameter_Acce={
  //200hz---80hz
1,     1.14298050254,   0.4128015980962,
0.638945525159,    1.277891050318,    0.638945525159
};

Butter_Parameter Butter_60HZ_Parameter_Acce={
  //200hz---60hz
1,   0.3695273773512,   0.1958157126558,
0.3913357725018,   0.7826715450035,   0.3913357725018
};

Butter_Parameter Butter_50HZ_Parameter_Acce={
  //200hz---50hz
1,-1.300707181133e-16,   0.1715728752538,
0.2065720838261,   0.4131441676523,   0.2065720838261,
};


Butter_Parameter Butter_30HZ_Parameter_Acce={
  //200hz---30hz
1,  -0.7477891782585,    0.272214937925,
0.1311064399166,   0.2622128798333,   0.1311064399166
};
Butter_Parameter Butter_20HZ_Parameter_Acce={
  //200hz---20hz
  1,    -1.14298050254,   0.4128015980962,
  0.06745527388907,   0.1349105477781,  0.06745527388907
};
Butter_Parameter Butter_15HZ_Parameter_Acce={
  //200hz---15hz
  1,   -1.348967745253,   0.5139818942197,
  0.04125353724172,  0.08250707448344,  0.04125353724172
};

Butter_Parameter Butter_10HZ_Parameter_Acce={
  //200hz---10hz
  1,   -1.561018075801,   0.6413515380576,
  0.02008336556421,  0.04016673112842,  0.02008336556421};
Butter_Parameter Butter_5HZ_Parameter_Acce={
  //200hz---5hz
  1,   -1.778631777825,   0.8008026466657,
  0.005542717210281,  0.01108543442056, 0.005542717210281
};

Butter_Parameter Butter_2HZ_Parameter_Acce={
  //200hz---2hz
  1,   -1.911197067426,   0.9149758348014,
  0.0009446918438402,  0.00188938368768,0.0009446918438402
};
Butter_Parameter Butter_1HZ_Parameter_Acce={
  //200hz---1hz
  1,   -1.955578240315,   0.9565436765112,
  0.000241359049042, 0.000482718098084, 0.000241359049042
};


float LPButterworth(float curr_input,Butter_BufferData *Buffer,Butter_Parameter *Parameter) //低通滤波器
{
        /* 加速度计Butterworth滤波 */
	/* 获取最新x(n) */
        static int LPB_Cnt=0;
        Buffer->Input_Butter[2]=curr_input;
        if(LPB_Cnt>=100)
        {
	/* Butterworth滤波 */
        Buffer->Output_Butter[2]=
         Parameter->b[0] * Buffer->Input_Butter[2]
        +Parameter->b[1] * Buffer->Input_Butter[1]
	+Parameter->b[2] * Buffer->Input_Butter[0]
        -Parameter->a[1] * Buffer->Output_Butter[1]
        -Parameter->a[2] * Buffer->Output_Butter[0];
        }
        else
        {
          Buffer->Output_Butter[2]=Buffer->Input_Butter[2];
          LPB_Cnt++;
        }
	/* x(n) 序列保存 */
        Buffer->Input_Butter[0]=Buffer->Input_Butter[1];
        Buffer->Input_Butter[1]=Buffer->Input_Butter[2];
	/* y(n) 序列保存 */
        Buffer->Output_Butter[0]=Buffer->Output_Butter[1];
        Buffer->Output_Butter[1]=Buffer->Output_Butter[2];


        return Buffer->Output_Butter[2];
}


Butter_BufferData Butter_Buffer_Correct[3];


Butter_BufferData Butter_Buffer[3];
Butter_BufferData Butter_Buffer_Feedback[3];
float Butter_parameter[2][3]={
  //200hz---21hz
        0.07319880848434,   0.1463976169687,  0.07319880848434,
        1,   -1.102497726129,   0.3952929600662
};

#define GRAVITY_MSS     9.80665f
#define AcceMax_1G      4096.0f
#define One_G_TO_Accel  AcceMax_1G/GRAVITY_MSS
#define AcceMax     4096.0f  //   4096
#define AcceGravity 9.80f

float Acce_Control[3]={0};

/*****************************************************************************************
	   用于内环PID加速度环的加速度处理
*****************************************************************************************/
void Acce_Control_Filter(void)
{
    float Acce_Control_Feedback[3]={0};
/**********************惯导加速度LPF_21hz**************************/ 
	//用于控制的加速度
   Attitude.accX_control=LPButterworth(Attitude.accX_origion,
                    &Butter_Buffer[0],&Butter_30HZ_Parameter_Acce);
   Attitude.accY_control=LPButterworth(Attitude.accY_origion
                    ,&Butter_Buffer[1],&Butter_30HZ_Parameter_Acce);
   Attitude.accZ_control=LPButterworth(Attitude.accZ_origion
                    ,&Butter_Buffer[2],&Butter_30HZ_Parameter_Acce);

/**********************加速度反馈量LPF_2hz***************************/
		
   Acce_Control_Feedback[0]=LPButterworth(Attitude.accX_origion,
                    &Butter_Buffer_Feedback[0],&Butter_5HZ_Parameter_Acce);
   Acce_Control_Feedback[1]=LPButterworth(Attitude.accY_origion
                    ,&Butter_Buffer_Feedback[1],&Butter_5HZ_Parameter_Acce);
   Acce_Control_Feedback[2]=LPButterworth(Attitude.accZ_origion
                    ,&Butter_Buffer_Feedback[2],&Butter_5HZ_Parameter_Acce);
//以下三句用于GPS和气压计加速度环，气压计使用到三环控制，最内环为加速度，GPS只有双环未上加速度环，预留
   //投影在垂直海拔上的加速度值	
		Attitude.acc_high_feedback= 
                      -IMU.Sin_Roll* Acce_Control_Feedback[0]
                        + IMU.Sin_Pitch *IMU.Cos_Roll * Acce_Control_Feedback[1]
                           + IMU.Cos_Pitch * IMU.Cos_Roll * Acce_Control_Feedback[2];
//   //投影在东西的加速度值	
//		Attitude.acc_pitch_feedback=
//                      IMU.Cos_Yaw* IMU.Cos_Roll * Acce_Control_Feedback[0]
//                        +(IMU.Sin_Pitch*IMU.Sin_Roll*IMU.Cos_Yaw-IMU.Cos_Pitch * IMU.Sin_Yaw) *Acce_Control_Feedback[1]
//                          +(IMU.Sin_Pitch * IMU.Sin_Yaw+IMU.Cos_Pitch * IMU.Sin_Roll * IMU.Cos_Yaw) * Acce_Control_Feedback[2];
//		//投影在南北的加速度值
//		Attitude.acc_roll_feedback=
//                      IMU.Sin_Yaw* IMU.Cos_Roll * Acce_Control_Feedback[0]
//                        +(IMU.Sin_Pitch * IMU.Sin_Roll * IMU.Sin_Yaw +IMU.Cos_Pitch * IMU.Cos_Yaw) * Acce_Control_Feedback[1]
//                          + (IMU.Cos_Pitch * IMU.Sin_Roll * IMU.Sin_Yaw - IMU.Sin_Pitch * IMU.Cos_Yaw) * Acce_Control_Feedback[2];
   Attitude.acc_high_feedback*=AcceGravity/AcceMax;
   Attitude.acc_high_feedback-=AcceGravity;
	 Attitude.acc_high_feedback*=100;//加速度cm/s^2
   Attitude.acc_pitch_feedback*=AcceGravity/AcceMax;
   Attitude.acc_pitch_feedback*=100;//加速度cm/s^2
   Attitude.acc_roll_feedback*=AcceGravity/AcceMax;
   Attitude.acc_roll_feedback*=100;//加速度cm/s^2
}


struct _ButterWorth2d_Acc_Tag
{
	int16_t input[3];
	int16_t output[3];
};

struct _ButterWorth2d_Acc_Tag accButter[3] =
{
	/* input[3] output[3] */
	{0, 0, 0, 0, 0, 0},		// X-axis
	{0, 0, 0, 0, 0, 0},		// Y-axis
	{0, 0, 0, 0, 0, 0},		// Z-axis
};

struct _ButterWorth2d_Acc_Tag gyroButter[3] =
{
	/* input[3] output[3] */
	{0, 0, 0, 0, 0, 0},		// X-axis
	{0, 0, 0, 0, 0, 0},		// Y-axis
	{0, 0, 0, 0, 0, 0},		// Z-axis
};
float K[3]={1.0,1.0,1.0};//默认标度误差
float B[3]={0,0,0};//默认零位误差


/*****************************************************************************************
	   用于GPS和气压计融合的加速度
*****************************************************************************************/
void ACC_IMU_Filter(int16_t ax,int16_t ay,int16_t az)
{
	
		const static float b_acc[3] ={0.1311064399166,   0.2622128798333,   0.1311064399166};
	const static float a_acc[3] ={1,  -0.7477891782585,    0.272214937925};
	float accelFilter[3];
	uint8_t axis;
////------------未经过椭球校正的原始数据，用于六面加速度矫正---------------------------------------------
          Attitude.accX_correct=(int16_t)(LPButterworth(ax,
                    &Butter_Buffer_Correct[0],&Butter_1HZ_Parameter_Acce));
					 Attitude.accY_correct=(int16_t)(LPButterworth(ay
														,&Butter_Buffer_Correct[1],&Butter_1HZ_Parameter_Acce));
					 Attitude.accZ_correct=(int16_t)(LPButterworth(az
														,&Butter_Buffer_Correct[2],&Butter_1HZ_Parameter_Acce));

//---------------------------//经过椭球校正后的三轴加速度量--------------------------------------------
        Attitude.accX_origion=K[0]*ax-B[0]*One_G_TO_Accel; //六面校准得出的K、B值在这里用到
        Attitude.accY_origion=K[1]*ay-B[1]*One_G_TO_Accel;
        Attitude.accZ_origion=K[2]*az-B[2]*One_G_TO_Accel;
		
//--------------------------为姿态解算IMU的加速度做准备-------------------------------------------------

//加速度滤波
#ifndef Butterworth
	//200_30z

	/* 加速度计Butterworth滤波 */
	/* 获取最新x(n) */
			accButter[0].input[2] =(int16_t)(Attitude.accX_origion);
        accButter[1].input[2] =(int16_t)(Attitude.accY_origion);
        accButter[2].input[2] =(int16_t)(Attitude.accZ_origion);
	/* Butterworth滤波 */

	for (axis = 0; axis < 3; axis++)
	{
	accButter[axis].output[2] =
                 (int16_t)(b_acc[0] * accButter[axis].input[2]
                  + b_acc[1] * accButter[axis].input[1]
                    + b_acc[2] * accButter[axis].input[0]
                      - a_acc[1] * accButter[axis].output[1]
                        - a_acc[2] * accButter[axis].output[0]);
		accelFilter[axis] = accButter[axis].output[2];
	}
	for ( axis = 0; axis < 3; axis++)
	{
		/* x(n) 序列保存 */
		accButter[axis].input[0] = accButter[axis].input[1];
		accButter[axis].input[1] = accButter[axis].input[2];
		/* y(n) 序列保存 */
		accButter[axis].output[0] = accButter[axis].output[1];
		accButter[axis].output[1] = accButter[axis].output[2];
	}
       Attitude.accX_IMU=accelFilter[0];
       Attitude.accY_IMU=accelFilter[1];
       Attitude.accZ_IMU=accelFilter[2];
	
#else	
			static struct _1_ekf_filter ekf[3] = {{0.02,0,0,0,0.001,0.543},{0.02,0,0,0,0.001,0.543},{0.02,0,0,0,0.001,0.543}};	
		kalman_1(&ekf[0],Attitude.accX_origion);  //一维卡尔曼
		kalman_1(&ekf[1],Attitude.accY_origion);  //一维卡尔曼		
		kalman_1(&ekf[2],Attitude.accZ_origion);  //一维卡尔曼
				

	 Attitude.accX_IMU=ekf[0].out;
	 Attitude.accY_IMU=ekf[1].out;
	 Attitude.accZ_IMU=ekf[2].out;				
	
#endif	
}

/*****************************************************************************************
	   用于GPS和气压计融合的加速度
*****************************************************************************************/
void  SINS_Prepare(void)
{
			Acce_Control_Filter();
      /*Z-Y-X欧拉角转方向余弦矩阵
        //Pitch Roll  Yaw 分别对应Φ θ Ψ

             X轴旋转矩阵
             R（Φ）
        {1      0        0    }
        {0      cosΦ    sinΦ}
        {0    -sinΦ    cosΦ }

             Y轴旋转矩阵
             R（θ）
        {cosθ     0        -sinθ}
        {0         1        0     }
        {sinθ     0        cosθ}

             Z轴旋转矩阵
             R（θ）
        {cosΨ      sinΨ       0}
        {-sinΨ     cosΨ       0}
        {0          0           1 }

        由Z-Y-X顺规有:
      载体坐标系到导航坐标系下旋转矩阵R(b2n)
      R(b2n) =R(Ψ)^T*R(θ)^T*R(Φ)^T

      R=
        {cosΨ*cosθ     -cosΦ*sinΨ+sinΦ*sinθ*cosΨ        sinΨ*sinΦ+cosΦ*sinθ*cosΨ}
        {cosθ*sinΨ     cosΦ*cosΨ +sinΦ*sinθ*sinΨ       -cosΨ*sinΦ+cosΦ*sinθ*sinΨ}
        {-sinθ          cosθsin Φ                          cosθcosΦ                   }
      */
//以下三句用于GPS和气压计数据融合
			//投影在垂直海拔上的加速度值，用于三阶融合气压计	
      Attitude.acc_yaw_sensor =//投影在垂直
                            -IMU.Sin_Roll* Attitude.accX_control
                              + IMU.Sin_Pitch *IMU.Cos_Roll * Attitude.accY_control
                                 + IMU.Cos_Pitch * IMU.Cos_Roll *Attitude.accZ_control;
			//投影在东西的加速度，用于融合GPS经度
      Attitude.acc_pitch_sensor=
                         IMU.Cos_Yaw*IMU.Cos_Roll * Attitude.accX_control
                              +(IMU.Sin_Pitch*IMU.Sin_Roll*IMU.Cos_Yaw-IMU.Cos_Pitch * IMU.Sin_Yaw) * Attitude.accY_control
                                +(IMU.Sin_Pitch * IMU.Sin_Yaw+IMU.Cos_Pitch * IMU.Sin_Roll * IMU.Cos_Yaw)*Attitude.accZ_control;
			//投影在南北的加速度，用于融合GPS的纬度
      Attitude.acc_roll_sensor=
                         IMU.Sin_Yaw* IMU.Cos_Roll * Attitude.accX_control
                              +(IMU.Sin_Pitch * IMU.Sin_Roll * IMU.Sin_Yaw +IMU.Cos_Pitch * IMU.Cos_Yaw) * Attitude.accY_control
                                + (IMU.Cos_Pitch * IMU.Sin_Roll * IMU.Sin_Yaw - IMU.Sin_Pitch * IMU.Cos_Yaw)*Attitude.accZ_control;

			
      Attitude.acc_yaw_sensor*=AcceGravity/AcceMax;
      Attitude.acc_yaw_sensor-=AcceGravity;//减去重力加速度，可以得到垂直加速度净增量
      Attitude.acc_yaw_sensor*=100;//加速度cm/s^2

      Attitude.acc_pitch_sensor*=AcceGravity/AcceMax;
      Attitude.acc_pitch_sensor*=100;//加速度cm/s^2

      Attitude.acc_roll_sensor*=AcceGravity/AcceMax;
      Attitude.acc_roll_sensor*=100;//加速度cm/s^2

			//计算动态步长，即飞行器的变化幅度，也就是飞行速度
      Attitude.Acceleration_Length=sqrt(Attitude.acc_yaw_sensor*Attitude.acc_yaw_sensor
                               +Attitude.acc_pitch_sensor*Attitude.acc_pitch_sensor
                                 +Attitude.acc_roll_sensor*Attitude.acc_roll_sensor);

   /******************************************************************************/
   //将无人机在导航坐标系下的沿着正东、正北方向的运动加速度旋转到当前航向的运动加速度:机头(俯仰)+横滚

      Attitude.acc_x_earth=Attitude.acc_pitch_sensor;//沿地理坐标系，正东方向运动加速度,单位为CM
      Attitude.acc_y_earth=Attitude.acc_roll_sensor;//沿地理坐标系，正北方向运动加速度,单位为CM

//地理坐标系水平加速度投影到机体坐标系上
      Attitude.acc_x_body=Attitude.acc_x_earth*IMU.Cos_Yaw+Attitude.acc_y_earth*IMU.Sin_Yaw;  //横滚正向运动加速度  X轴正向
      Attitude.acc_y_body=-Attitude.acc_x_earth*IMU.Sin_Yaw+Attitude.acc_y_earth*IMU.Cos_Yaw; //机头正向运动加速度  Y轴正向

}




/*****************************************************************************************
	   角速度滤波
*****************************************************************************************/

Butter_Parameter Gyro_Parameter={
//200hz---30hz
1,  -0.7477891782585,    0.272214937925,
0.1311064399166,   0.2622128798333,   0.1311064399166
};
Butter_BufferData Gyro_BufferData[3];

float GYRO_LPF(float curr_inputer,   //三阶巴特沃斯滤波
               Butter_BufferData *Buffer,
               Butter_Parameter *Parameter)
{
        /* 加速度计Butterworth滤波 */
	/* 获取最新x(n) */
        Buffer->Input_Butter[2]=curr_inputer;
	/* Butterworth滤波 */
        Buffer->Output_Butter[2]=
         Parameter->b[0] * Buffer->Input_Butter[2]
        +Parameter->b[1] * Buffer->Input_Butter[1]
	+Parameter->b[2] * Buffer->Input_Butter[0]
        -Parameter->a[1] * Buffer->Output_Butter[1]
        -Parameter->a[2] * Buffer->Output_Butter[0];
	/* x(n) 序列保存 */
        Buffer->Input_Butter[0]=Buffer->Input_Butter[1];
        Buffer->Input_Butter[1]=Buffer->Input_Butter[2];
	/* y(n) 序列保存 */
        Buffer->Output_Butter[0]=Buffer->Output_Butter[1];
        Buffer->Output_Butter[1]=Buffer->Output_Butter[2];
        return (Buffer->Output_Butter[2]);
}

void GYRO_IMU_Filter(short gx,short gy,short gz)//角速度低通滤波后用于姿态解算
{

        Attitude.gyroX_IMU=GYRO_LPF(gx,
                        &Gyro_BufferData[0],
                        &Gyro_Parameter
                        );
        Attitude.gyroY_IMU=GYRO_LPF(gy,
                        &Gyro_BufferData[1],
                        &Gyro_Parameter
                        );
        Attitude.gyroZ_IMU=GYRO_LPF(gz,
                        &Gyro_BufferData[2],
                        &Gyro_Parameter
                        );
}








