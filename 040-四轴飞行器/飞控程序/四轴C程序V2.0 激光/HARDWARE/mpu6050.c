
#include "mpu6050.h"
#include "ALL_DATA.h"
#include "ANO_Data_Transfer.h"
#include "I2C.h"
#include "LED.h"
#include "filter.h"
#include "kalman.h"
#include "myMath.h"
#include <string.h>

#define SMPLRT_DIV 0x19   // 陀螺仪采样率，典型值：0x07(125Hz)
#define CONFIGL 0x1A      // 低通滤波频率，典型值：0x06(5Hz)
#define GYRO_CONFIG 0x1B  // 陀螺仪自检及测量范围，典型值：0x18(不自检，2000deg/s)
#define ACCEL_CONFIG 0x1C // 加速计自检、测量范围及高通滤波频率，典型值：0x01(不自检，2G，5Hz)
#define ACCEL_ADDRESS 0x3B
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H 0x41
#define TEMP_OUT_L 0x42
#define GYRO_XOUT_H 0x43
#define GYRO_ADDRESS 0x43
#define GYRO_XOUT_L 0x44
#define GYRO_YOUT_H 0x45
#define GYRO_YOUT_L 0x46
#define GYRO_ZOUT_H 0x47
#define GYRO_ZOUT_L 0x48
#define PWR_MGMT_1 0x6B // 电源管理，典型值：0x00(正常启用)
#define WHO_AM_I 0x75   // IIC地址寄存器(默认数值0x68，只读)
#define MPU6050_PRODUCT_ID 0x68
#define MPU6052C_PRODUCT_ID 0x72

// #define   MPU6050_is_DRY()      GPIO_ReadOutBit(HT_GPIOC, GPIO_PIN_0)//IRQ主机数据输入
#ifdef USE_I2C_HARDWARE

#define MPU6050_ADDRESS 0xD0 // 0x68
#else
#define MPU6050_ADDRESS 0xD0 // IIC写入时的地址字节数据，+1为读取
#endif

int16_t MpuOffset[6] = {0};

static volatile int16_t *pMpu = (int16_t *)&MPU6050;

/****************************************************************************************
 *@brief
 *@brief
 *@param[in]
 *****************************************************************************************/
int8_t mpu6050_rest(void)
{
    if (IIC_Write_One_Byte(MPU6050_ADDRESS, PWR_MGMT_1, 0x80) == FAILED)
        return FAILED; // 复位
    delay_ms(20);
    return SUCCESS;
}
/****************************************************************************************
 *@brief
 *@brief
 *@param[in]
 *****************************************************************************************/
int8_t MpuInit(void) // 初始化
{
    uint8_t date = SUCCESS;

    bLED_L(); // 前灯亮
    aLED_L(); // 前灯亮
    fLED_H(); // 后灯灭
    hLED_H(); // 后灯灭

    do
    {
        date = IIC_Write_One_Byte(MPU6050_ADDRESS, PWR_MGMT_1, 0x80); // 复位
        delay_ms(30);
        date += IIC_Write_One_Byte(MPU6050_ADDRESS, SMPLRT_DIV, 0x02);   // 陀螺仪采样率，0x00(500Hz)
        date += IIC_Write_One_Byte(MPU6050_ADDRESS, PWR_MGMT_1, 0x03);   // 设置设备时钟源，陀螺仪Z轴
        date += IIC_Write_One_Byte(MPU6050_ADDRESS, CONFIGL, 0x03);      // 低通滤波频率，0x03(42Hz)
        date += IIC_Write_One_Byte(MPU6050_ADDRESS, GYRO_CONFIG, 0x18);  // 满量程+-2000deg/s
        date += IIC_Write_One_Byte(MPU6050_ADDRESS, ACCEL_CONFIG, 0x09); //+-4G
        MPU_Err = 1;
    } while (date != SUCCESS);
    date = IIC_Read_One_Byte(MPU6050_ADDRESS, 0x75); // 读取IIC地址寄存器
    if (date != MPU6050_PRODUCT_ID)
        return FAILED;
    else
        MpuGetOffset(); // 校准
    MPU_Err = 0;
    return SUCCESS;
}
/****************************************************************************************
 *@brief
 *@brief
 *@param[in]
 *****************************************************************************************/

#define Acc_Read() IIC_read_Bytes(MPU6050_ADDRESS, 0X3B, buffer, 6)      // 读取加速度计的值
#define Gyro_Read() IIC_read_Bytes(MPU6050_ADDRESS, 0x43, &buffer[6], 6) // 读取陀螺仪的值

void MpuGetData(void) // 读取陀螺仪数据加滤波
{
    uint8_t i;
    uint8_t buffer[12];

    Acc_Read();
    Gyro_Read();

    for (i = 0; i < 6; i++)
    {
        // 将高字节与低字节组合成 16 位有符号原始值，再减去校准偏移
        pMpu[i] = (((int16_t)buffer[i << 1] << 8) | buffer[(i << 1) + 1]) - MpuOffset[i]; // 校准偏移

        if (i < 3) // 对加速度计值滤波
        {
            static struct _1_ekf_filter ekf[3] = {{0.02, 0, 0, 0, 0.001, 0.543}, {0.02, 0, 0, 0, 0.001, 0.543}, {0.02, 0, 0, 0, 0.001, 0.543}};
            kalman_1(&ekf[i], (float)pMpu[i]); // 一维卡尔曼
            pMpu[i] = (int16_t)ekf[i].out;
        }

        if (i > 2) // 对陀螺仪数据滤波
        {
            uint8_t k = i - 3;
            const float factor = 0.15f; // 低通滤波系数
            static float tBuff[3];

            pMpu[i] = tBuff[k] = tBuff[k] * (1 - factor) + pMpu[i] * factor;
        }
    }
}

/****************************************************************************************
 *@brief   get mpu offset
 *@brief   initial and cmd call this
 *@param[in]
 *****************************************************************************************/
void MpuGetOffset(void) // 校准
{
    int32_t buffer[6] = {0};
    int16_t i;
    uint8_t k = 30;
    const int8_t MAX_GYRO_QUIET = 5;
    const int8_t MIN_GYRO_QUIET = -5;
    /*           wait for calm down    	                                                          */
    int16_t LastGyro[3] = {0};
    int16_t ErrorGyro[3];
    /*           set offset initial to zero    		*/

    memset(MpuOffset, 0, 12);
    MpuOffset[2] = 8192; // set offset from the 8192

    TIM_ITConfig( // 使能或者失能指定的TIM中断
        TIM3,     // TIM2
        TIM_IT_Update,
        DISABLE // 使能
    );
    while (k--)
    {
        do
        {
            delay_ms(10);
            MpuGetData();
            for (i = 0; i < 3; i++)
            {
                ErrorGyro[i] = pMpu[i + 3] - LastGyro[i];
                LastGyro[i] = pMpu[i + 3];
            }
        } while ((ErrorGyro[0] > MAX_GYRO_QUIET) || (ErrorGyro[0] < MIN_GYRO_QUIET) || (ErrorGyro[1] > MAX_GYRO_QUIET) || (ErrorGyro[1] < MIN_GYRO_QUIET) || (ErrorGyro[2] > MAX_GYRO_QUIET) || (ErrorGyro[2] < MIN_GYRO_QUIET));
    }

    /*           throw first 100  group data and make 256 group average as offset                    */
    for (i = 0; i < 356; i++)
    {
        MpuGetData();
        if (100 <= i)
        {
            uint8_t k;
            for (k = 0; k < 6; k++)
            {
                buffer[k] += pMpu[k];
            }
        }
    }

    for (i = 0; i < 6; i++)
    {
        MpuOffset[i] = buffer[i] >> 8;
    }
    TIM_ITConfig( // 使能或者失能指定的TIM中断
        TIM3,     // TIM2
        TIM_IT_Update,
        ENABLE // 使能
    );
}
/**************************************END OF FILE*************************************/
