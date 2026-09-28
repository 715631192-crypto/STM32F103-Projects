#include "pid.h"
#include "myMath.h"	

/**************************************************************
 * PID 参数复位（清零积分项/上次误差/输出/偏移）
 * @param[in] 
 * @param[out] 
 * @return     
 ***************************************************************/	
void pidRest(PidObject **pid,const uint8_t len)
{
	uint8_t i;
	for(i=0;i<len;i++)
	{
	  	pid[i]->integ = 0;
	    pid[i]->prevError = 0;
	    pid[i]->out = 0;
		pid[i]->offset = 0;
	}
}

/**************************************************************
 * Update the PID parameters.
 *
 * @param[in] pid         A pointer to the pid object.
 * @param[in] measured    The measured value
 * @param[in] updateError Set to TRUE if error should be calculated.
 *                        Set to False if pidSetError() has been used.
 * @return PID algorithm output
 ***************************************************************/	
void pidUpdate(PidObject* pid,const float dt)
{
	 float error;// 误差
	 float deriv;// 微分项
	 error = pid->desired - pid->measured;// 计算误差 = 期望值 - 测量值
	 pid->integ += error * dt;// 积分项累加（误差×dt）
	 deriv = (error - pid->prevError) / dt;// 计算微分（误差变化率）
	 pid->out = pid->kp * error + pid->ki * pid->integ + pid->kd * deriv;// 位置PID输出
	 pid->prevError = error;// 更新上次误差为当前误差
		
}

/**************************************************************
 *  CascadePID
 * @param[in] 
 * @param[out] 
 * @return     
 ***************************************************************/	
void CascadePID(PidObject* pidRate,PidObject* pidAngE,const float dt)  // 串级PID（双环）
{	 
	 pidUpdate(pidRate,dt);// 更新内环（角速度）PID
	 pidRate->desired = pidAngE->out;
	 pidUpdate(pidAngE,dt);// 更新外环（角度）PID
	 
}


/*******************************END*********************************/



