#ifndef __APP_PS2_H_
#define __APP_PS2_H_
#include "main.h"
#define CMD_RETURN_SIZE 1024
#define PS2_LED_RED 0x73
#define PS2_LED_GRN 0x41
#define PSX_BUTTON_NUM 16
#define PS2_MAX_LEN 64

void app_ps2_init(void);
void app_ps2_run(void);
void parse_psx_buf(u8 *buf,u8 mode);


#endif // __APP_PS2_H_