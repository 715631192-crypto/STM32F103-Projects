#ifndef __DHT11_H_
#define __DHT11_H_

uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);
uint8_t DHT11_Init(void);
#endif // __DHT11_H_
