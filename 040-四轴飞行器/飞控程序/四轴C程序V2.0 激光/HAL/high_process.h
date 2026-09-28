#ifndef __HIGH_PROCESS__H
#define __HIGH_PROCESS__H

















typedef struct{
	float High;//处理后的高度
	float Speed;//高度
	float  sutgbl;
}_st_Height;

extern _st_Height Height;


void Strapdown_INS_High(float high);
#endif



