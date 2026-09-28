
#include <string.h>
#include "filter.h"
#include <math.h>
#include "myMath.h"
 

/*=================================== ================================================================*/
/*====================================================================================================*
**函数 : 中值滤波
**功能 : 
**输入 : 
**輸出 : None
**备注 : None
**====================================================================================================*/
/*====================================================================================================*/
int16_t MovMiddle(int16_t input)
{	
	uint8_t i,j;
	const uint8_t MOV_MIDDLE_NUM = 5;
	static int16_t middle[5]={0};
	int16_t middle_t[5];
//	MOV_MIDDLE_NUM = pidHeightRate.ki;
	for(i=1;i<MOV_MIDDLE_NUM;i++)
	{
		 middle[i-1] =  middle[i];
	}
	middle[MOV_MIDDLE_NUM-1] = input;
	memcpy(middle_t,middle,MOV_MIDDLE_NUM*sizeof(uint32_t));
	for(i=0;i<MOV_MIDDLE_NUM-1;i++)
	{
		for(j=i+1;j<MOV_MIDDLE_NUM;j++)
		{
			if(middle_t[i] > middle_t[j])
			{
				middle_t[i] ^= middle_t[j];
				middle_t[j] ^= middle_t[i];
				middle_t[i] ^= middle_t[j];
			}
		}
	}
	return middle_t[(MOV_MIDDLE_NUM+1)>>1];
}	
/*=================================== ================================================================*/
/*====================================================================================================*
**函数 : 抗干扰型滑动均值滤波
**功能 : 每次采样到一个新数据放入队列，对N个数据进行算术平均运算
**输入 : 
**輸出 : None
**备注 : None
**====================================================================================================*/
/*====================================================================================================*/
uint16_t AntiPulse_MovingAverage_Filter(MovAverage *_MovAverage)
{
		uint8_t i;	
		uint32_t sum=0;
		uint16_t max=0;
		uint16_t min=0xffff;
	
			_MovAverage->average[_MovAverage->cnt] = _MovAverage->input;	
			_MovAverage->cnt++;			
			if(_MovAverage->cnt==_MovAverage->max_cnt)
			{
				_MovAverage->cnt=0;
			}	
			for(i=0;i<_MovAverage->max_cnt;i++)
			{
					if(_MovAverage->average[i]>max)
							max = _MovAverage->average[i];
					else if(_MovAverage->average[i]<min)
							min = _MovAverage->average[i];
					sum += _MovAverage->average[i];
			}
		return ((sum-max-min)/(_MovAverage->max_cnt-2));                                    
}

uint16_t MovingAverage_Filter(MovAverage *_MovAverage)
{
		uint8_t i;	
		uint32_t sum=0;

			_MovAverage->average[_MovAverage->cnt] = _MovAverage->input;	
			_MovAverage->cnt++;			
			if(_MovAverage->cnt==_MovAverage->max_cnt)
			{
				_MovAverage->cnt=0;
			}	
			for(i=0;i<_MovAverage->max_cnt;i++)
			{
					sum += _MovAverage->average[i];
			}
		return (sum/_MovAverage->max_cnt);                                    
}

/*====================================================================================================*/
/*====================================================================================================*
** 函数名称: IIR_I_Filter
** 功能描述: IIR直接I型滤波器
** 输    入: InData 为当前数据
**           *x     储存未滤波的数据
**           *y     储存滤波后的数据
**           *b     储存系数b
**           *a     储存系数a
**           nb     数组*b的长度
**           na     数组*a的长度
**           LpfFactor
** 输    出: OutData         
** 说    明: 无
** 函数原型: y(n) = b0*x(n) + b1*x(n-1) + b2*x(n-2) -
                    a1*y(n-1) - a2*y(n-2)
**====================================================================================================*/
/*====================================================================================================*/
float IIR_I_Filter(float InputData, float *x, float *y,  const float *b, uint8_t nb, const float *a, uint8_t na)
{
  float z1,z2=0;
  int16_t i;
	
  for(i=nb-1; i>0; i--)
  {
    x[i]=x[i-1];
		y[i]=y[i-1];
  }
  x[0] = InputData;
	z1 = x[0] * b[0];
  for(i=1; i<nb; i++)
  {
    z1 += x[i]*b[i];
		z2 += y[i]*a[i];
  }
  y[0] = z1 - z2; 
  return y[0];
}
/*====================================================================================================*/
/*====================================================================================================*
**函数 : LPF_1st
**功能 : 一阶滞后滤波
**输入 :  
**輸出 : None
**备注 : None
**====================================================================================================*/
/*====================================================================================================*/
//model 1:
float LPF_1_Filter_1(Filter_LPF_1 *LPF_1)
{
	return LPF_1->old_data * (1 - LPF_1->factor) + LPF_1->new_data *  LPF_1->factor;
}
//model 2:
//_LPF_1->factor = cut_frequent
float LPF_1_Filter_2(Filter_LPF_1 *LPF_1,float dt)
{
	 return LPF_1->old_data + (dt /( 1 / ( 2 * PI * LPF_1->factor ) + dt)) * (LPF_1->new_data - LPF_1->old_data);    
}
//---------------------------
// 一阶离散低通滤波器  type frequent.
// Examples for _filter:
//#define  _filter   7.9577e-3  // 由 "1 / ( 2 * PI * f_cut )"这个公式计算得来; 
// f_cut = 10 Hz -> _filter = 15.9155e-3
// f_cut = 15 Hz -> _filter = 10.6103e-3
// f_cut = 20 Hz -> _filter =  7.9577e-3
// f_cut = 25 Hz -> _filter =  6.3662e-3
// f_cut = 30 Hz -> _filter =  5.3052e-3
//======================================================================================================

/*====================================================================================================*/
/*====================================================================================================*
**函数 : Moving_Median 
**功能 : 中位值滤波法
**輸出 : None
**备注 : None
**====================================================================================================*/
/*====================================================================================================*/
#define MED_WIDTH_NUM 11
#define MED_FIL_ITEM  4

float med_filter_tmp[MED_FIL_ITEM][MED_WIDTH_NUM ];
float med_filter_out[MED_FIL_ITEM];

uint8_t med_fil_cnt[MED_FIL_ITEM];

float Moving_Median(uint8_t item,uint8_t width_num,float in)
{
	uint8_t i,j;
	float t;
	float tmp[MED_WIDTH_NUM];
	
	if(item >= MED_FIL_ITEM || width_num >= MED_WIDTH_NUM )
	{
		return 0;
	}
	else
	{
		if( ++med_fil_cnt[item] >= width_num )	
		{
			med_fil_cnt[item] = 0;
		}
		
		med_filter_tmp[item][ med_fil_cnt[item] ] = in;
		
		for(i=0;i<width_num;i++)
		{
			tmp[i] = med_filter_tmp[item][i];
		}
		
		for(i=0;i<width_num-1;i++)
		{
			for(j=0;j<(width_num-1-i);j++)
			{
				if(tmp[j] > tmp[j+1])
				{
					t = tmp[j];
					tmp[j] = tmp[j+1];
					tmp[j+1] = t;
				}
			}
		}		
		return ( tmp[(width_num/2)] );
	}
}

// #define WIDTH_NUM 101
// #define FIL_ITEM  10

 void Moving_Average(float moavarray[],u16 len ,u16 *fil_cnt,float in,float *out)
{
	u16 width_num;
	float last;
 
	width_num = len ;
	
	if( ++*fil_cnt >= width_num )	
	{
		*fil_cnt = 0; //now
	}
	
	last = moavarray[ *fil_cnt ];
	
	moavarray[ *fil_cnt ] = in;
	
	*out += ( in - ( last  ) )/(float)( width_num ) ;
	*out += 0.00001f *LIMIT((in - *out),-1,1);  //????????
	
}  
/******************* (C) COPYRIGHT 2012 WildFire Team *****END OF FILE************/
