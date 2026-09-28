#ifndef _ESP8266_H_
#define _ESP8266_H_





#define REV_OK		0	
#define REV_WAIT	1	
#define ESP8266_WIFI_INFO		"AT+CWJAP=\"Starry_Wang\",\"88888888\"\r\n"
#define ESP8266_ONENET_INFO    "AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n"      //ESP8266建立TCP连接到OneNet的MQTT服务器



void ESP8266_Init(void);

void ESP8266_Clear(void);

_Bool ESP8266_SendCmd(char *cmd, char *res);

void ESP8266_SendData(unsigned char *data, unsigned short len);

unsigned char *ESP8266_GetIPD(unsigned short timeOut);


#endif
