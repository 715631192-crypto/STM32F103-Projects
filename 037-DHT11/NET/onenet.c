

// 单片机头文件
#include "stm32f10x.h"

// 网络设备
#include "ESP8266.h"

// 协议文件
#include "onenet.h"
#include "mqttkit.h"

// 算法
#include "base64.h"
#include "hmac_sha1.h"

// 硬件驱动
#include "USART.h"
#include "Delay.h"

// C库
#include <string.h>
#include <stdio.h>
#include "cJSON.h"
#include "LED.h"
#include "BUZZER.h"

#define PROID "u5u1bJ8h3C"

#define ACCESS_KEY "OTlPWWdUMVpIR2VUZXltalNBa2JuZUJUNlZvalFadnE="

#define DEVICE_NAME "DHT11"

char devid[16];

char key[48];

extern unsigned char esp8266_buf[512];
extern uint8_t temp, humi;
extern uint8_t led_state;

/*
************************************************************
*	函数名称：	OTA_UrlEncode
*
*	函数功能：	sign需要进行URL编码
*
*	入口参数：	sign：加密结果
*
*	返回参数：	0-成功	其他-失败
*
*	说明：		+			%2B
*				空格		%20
*				/			%2F
*				?			%3F
*				%			%25
*				#			%23
*				&			%26
*				=			%3D
************************************************************
*/
static unsigned char OTA_UrlEncode(char *sign)
{

	char sign_t[40];
	unsigned char i = 0, j = 0;
	unsigned char sign_len = strlen(sign);

	if (sign == (void *)0 || sign_len < 28)
		return 1;

	for (; i < sign_len; i++)
	{
		sign_t[i] = sign[i];
		sign[i] = 0;
	}
	sign_t[i] = 0;

	for (i = 0, j = 0; i < sign_len; i++)
	{
		switch (sign_t[i])
		{
		case '+':
			strcat(sign + j, "%2B");
			j += 3;
			break;

		case ' ':
			strcat(sign + j, "%20");
			j += 3;
			break;

		case '/':
			strcat(sign + j, "%2F");
			j += 3;
			break;

		case '?':
			strcat(sign + j, "%3F");
			j += 3;
			break;

		case '%':
			strcat(sign + j, "%25");
			j += 3;
			break;

		case '#':
			strcat(sign + j, "%23");
			j += 3;
			break;

		case '&':
			strcat(sign + j, "%26");
			j += 3;
			break;

		case '=':
			strcat(sign + j, "%3D");
			j += 3;
			break;

		default:
			sign[j] = sign_t[i];
			j++;
			break;
		}
	}

	sign[j] = 0;

	return 0;
}

/*
************************************************************
*	函数名称：	OTA_Authorization
*
*	函数功能：	计算Authorization
*
*	入口参数：	ver：参数组版本号，日期格式，目前仅支持格式"2018-10-31"
*				res：产品id
*				et：过期时间，UTC秒值
*				access_key：访问密钥
*				dev_name：设备名
*				authorization_buf：缓存token的指针
*				authorization_buf_len：缓存区长度(字节)
*
*	返回参数：	0-成功	其他-失败
*
*	说明：		当前仅支持sha1
************************************************************
*/
#define METHOD "sha1"
/**
 * @brief  生成OneNET平台MQTT连接鉴权Token(Authorization)
 * @param  ver                 协议版本字符串，例如"2018-10-31"
 * @param  res                 产品ID
 * @param  et                  过期时间戳，Unix时间，必须大于平台基准时间1564562581
 * @param  access_key          设备/产品的Access‑Key字符串（base64格式）
 * @param  dev_name            设备名称，flag=0时传入；flag=1可以传NULL
 * @param  authorization_buf   【输出】存放最终生成的鉴权token字符串缓冲区
 * @param  authorization_buf_len 输出缓冲区总大小，建议≥120字节
 * @param  flag                模式选择：1=产品级token；0=设备级token
 * @retval 0 生成成功；1 入参非法
 * @note   流程：Base64解码access_key → 拼装待签名字符串 → HMAC‑SHA1签名 → Base64编码签名结果 → URL转义 → 拼装最终token
 */
static unsigned char OneNET_Authorization(char *ver, char *res, unsigned int et, char *access_key, char *dev_name,
										  char *authorization_buf, unsigned short authorization_buf_len, _Bool flag)
{

	size_t olen = 0;

	char sign_buf[64];			   // 签名结果：先base64编码，之后做URL编码
	char hmac_sha1_buf[64];		   // HMAC‑SHA1原始二进制签名输出
	char access_key_base64[64];	   // AccessKey做Base64解码之后的真正密钥
	char string_for_signature[72]; // 待签名原始字符串，按照OneNET规定格式拼接

	//----------------------------------------------------参数合法性检查--------------------------------------------------------------------
	// 版本、产品ID、access_key、输出缓冲区不能为空；过期时间不能太早；输出缓冲区不能过小
	if (ver == (void *)0 || res == (void *)0 || et < 1564562581 || access_key == (void *)0 || authorization_buf == (void *)0 || authorization_buf_len < 120)
		return 1;

	//----------------------------------------------------把AccessKey从Base64还原出原始密钥----------------------------------------------------
	memset(access_key_base64, 0, sizeof(access_key_base64));
	BASE64_Decode((unsigned char *)access_key_base64, sizeof(access_key_base64), &olen, (unsigned char *)access_key, strlen(access_key));
	// UsartPrintf(USART_DEBUG, "access_key_base64: %s\r\n", access_key_base64);

	//----------------------------------------------------拼接【待签名的原始字符串】-----------------------------------------------------
	memset(string_for_signature, 0, sizeof(string_for_signature));
	if (flag)
		// 产品级Token格式：过期时间\n请求方法\n资源路径\n版本号
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s\n%s", et, METHOD, res, ver);
	else
		// 设备级Token格式：过期时间\n请求方法\n产品/设备路径\n版本号
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s/devices/%s\n%s", et, METHOD, res, dev_name, ver);
	// UsartPrintf(USART_DEBUG, "string_for_signature: %s\r\n", string_for_signature);

	//----------------------------------------------------HMAC‑SHA1加密签名-------------------------------------------------------------------------
	memset(hmac_sha1_buf, 0, sizeof(hmac_sha1_buf));
	// 密钥：解码后的access_key；待签名数据：上面拼接好的string_for_signature；输出：hmac_sha1_buf
	hmac_sha1((unsigned char *)access_key_base64, strlen(access_key_base64),
			  (unsigned char *)string_for_signature, strlen(string_for_signature),
			  (unsigned char *)hmac_sha1_buf);

	// UsartPrintf(USART_DEBUG, "hmac_sha1_buf: %s\r\n", hmac_sha1_buf);

	//----------------------------------------------------对HMAC‑SHA1输出做Base64编码------------------------------------------------------
	olen = 0;
	memset(sign_buf, 0, sizeof(sign_buf));
	BASE64_Encode((unsigned char *)sign_buf, sizeof(sign_buf), &olen, (unsigned char *)hmac_sha1_buf, strlen(hmac_sha1_buf));

	//----------------------------------------------------Base64结果做URL转义（+ / = 转成url安全字符）---------------------------------------------------
	OTA_UrlEncode(sign_buf);
	// UsartPrintf(USART_DEBUG, "sign_buf: %s\r\n", sign_buf);

	//----------------------------------------------------拼装最终OneNET鉴权Token--------------------------------------------------------------------
	if (flag)
		// 产品token：version&res&et&method&sign
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s&et=%d&method=%s&sign=%s", ver, res, et, METHOD, sign_buf);
	else
		// 设备token：version&res(产品/设备)&et&method&sign
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s%%2Fdevices%%2F%s&et=%d&method=%s&sign=%s", ver, res, dev_name, et, METHOD, sign_buf);
	// UsartPrintf(USART_DEBUG, "Token: %s\r\n", authorization_buf);

	return 0;
}
//==========================================================
//	函数名称：	OneNET_RegisterDevice
//
//	函数功能：	在产品中注册一个设备
//
//	入口参数：	access_key：访问密钥
//				pro_id：产品ID
//				serial：唯一设备号
//				devid：保存返回的devid
//				key：保存返回的key
//
//	返回参数：	0-成功		1-失败
//
//	说明：
//==========================================================
_Bool OneNET_RegisterDevice(void)
{

	_Bool result = 1;
	unsigned short send_len = 11 + strlen(DEVICE_NAME);
	char *send_ptr = NULL, *data_ptr = NULL;

	char authorization_buf[144]; // 加密的key

	send_ptr = malloc(send_len + 240);
	if (send_ptr == NULL)
		return result;

	while (ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80\r\n", "CONNECT"))
		Delay_ms(500);

	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, NULL,
						 authorization_buf, sizeof(authorization_buf), 1);

	snprintf(send_ptr, 240 + send_len, "POST /mqtt/v1/devices/reg HTTP/1.1\r\n"
									   "Authorization:%s\r\n"
									   "Host:ota.heclouds.com\r\n"
									   "Content-Type:application/json\r\n"
									   "Content-Length:%d\r\n\r\n"
									   "{\"name\":\"%s\"}",

			 authorization_buf, 11 + strlen(DEVICE_NAME), DEVICE_NAME);

	ESP8266_SendData((unsigned char *)send_ptr, strlen(send_ptr));

	/*
	{
	  "request_id" : "f55a5a37-36e4-43a6-905c-cc8f958437b0",
	  "code" : "onenet_common_success",
	  "code_no" : "000000",
	  "message" : null,
	  "data" : {
		"device_id" : "589804481",
		"name" : "mcu_id_43057127",

	"pid" : 282932,
		"key" : "indu/peTFlsgQGL060Gp7GhJOn9DnuRecadrybv9/XY="
	  }
	}
	*/

	data_ptr = (char *)ESP8266_GetIPD(250); // 等待平台响应

	if (data_ptr)
	{
		data_ptr = strstr(data_ptr, "device_id");
	}

	if (data_ptr)
	{
		char name[16];
		int pid = 0;

		if (sscanf(data_ptr, "device_id\" : \"%[^\"]\",\r\n\"name\" : \"%[^\"]\",\r\n\r\n\"pid\" : %d,\r\n\"key\" : \"%[^\"]\"", devid, name, &pid, key) == 4)
		{
			UsartPrintf(USART_DEBUG, "create device: %s, %s, %d, %s\r\n", devid, name, pid, key);
			result = 0;
		}
	}

	free(send_ptr);
	ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK");

	return result;
}

//==========================================================
//	函数名称：	OneNet_DevLink
//
//	函数功能：	与onenet创建连接
//
//	入口参数：	无
//
//	返回参数：	0-成功	1-失败
//
//	说明：		与onenet平台建立连接
//==========================================================
_Bool OneNet_DevLink(void)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0}; // 协议包

	unsigned char *dataPtr;

	char authorization_buf[512];//储存token的缓冲区

	_Bool status = 1;
    
	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, DEVICE_NAME,//版本号、产品ID、时间戳、密钥、名称
						 authorization_buf, sizeof(authorization_buf), 0);

	UsartPrintf(USART_DEBUG, "OneNET_DevLink\r\n"
							 "NAME: %s,	PROID: %s,	KEY:%s\r\n",
				DEVICE_NAME, PROID, authorization_buf);
    //创建连接报文
	if (MQTT_PacketConnect(PROID, authorization_buf, DEVICE_NAME, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len); // 上传平台

		dataPtr = ESP8266_GetIPD(250); // 等待平台响应
		if (dataPtr != NULL)
		{
			if (MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch (MQTT_UnPacketConnectAck(dataPtr))
				{
				case 0:
					UsartPrintf(USART_DEBUG, "Tips:	连接成功\r\n");
					status = 0;
					break;

				case 1:
					UsartPrintf(USART_DEBUG, "WARN:	连接失败：协议错误\r\n");
					break;
				case 2:
					UsartPrintf(USART_DEBUG, "WARN:	连接失败:非法的clientid\r\n");
					break;
				case 3:
					UsartPrintf(USART_DEBUG, "WARN:	连接失败：服务器失败\r\n");
					break;
				case 4:
					UsartPrintf(USART_DEBUG, "WARN:	连接失败：用户名或密码错误\r\n");
					break;
				case 5:
					UsartPrintf(USART_DEBUG, "WARN:	连接失败：非法链接\r\n");
					break;

				default:
					UsartPrintf(USART_DEBUG, "ERR:	连接失败：未知错误\r\n");
					break;
				}
			}
		}

		MQTT_DeleteBuffer(&mqttPacket); // 删包
	}
	else
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");

	return status;
}

// unsigned char OneNet_FillBuf(char *buf)
// {

// 	char text[48];

// 	memset(text, 0, sizeof(text));

// 	strcpy(buf, "{\"id\":\",\"params\":{");

// 	memset(text, 0, sizeof(text));
// 	sprintf(text, "\"temp\":{\"value\":%d},", temp);
// 	strcat(buf, text);

// 	memset(text, 0, sizeof(text));
// 	sprintf(text, "\"humi\":{\"value\":%d}", humi);
// 	strcat(buf, text);

// 	// memset(text, 0, sizeof(text));
// 	// sprintf(text, "\"led\":{\"value\":%d}", led_state);
// 	// strcat(buf, text);

// 	strcat(buf, "}}");

// 	return strlen(buf);

// }

unsigned char OneNet_FillBuf(char *buf)
{
	char text[48];

	memset(text, 0, sizeof(text));
	// 补全id字段的正确赋值
	strcpy(buf, "{\"id\":\"123\",\"params\":{");

	memset(text, 0, sizeof(text));
	sprintf(text, "\"temp\":{\"value\":%d},", temp);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"humi\":{\"value\":%d},", humi);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"buzzer\":{\"value\":%s},", buzzer_state ? "true" : "false");
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"led\":{\"value\":%s}", led_state ? "true" : "false");
	strcat(buf, text);

	strcat(buf, "}}");

	return (unsigned char)strlen(buf);
}

/*{
	"id":"123",
	"version":"1.0",
	"params":{
		"humi":{"value":25},
		 "temp":{"value":30}
	 }
}*/

//==========================================================
//	函数名称：	OneNet_SendData
//
//	函数功能：	上传数据到平台
//
//	入口参数：	type：发送数据的格式
//
//	返回参数：	无
//
//	说明：
//==========================================================
void OneNet_SendData(void)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0}; // 协议包

	char buf[256];

	short body_len = 0, i = 0;

	UsartPrintf(USART_DEBUG, "Tips:	OneNet_SendData-MQTT\r\n");

	memset(buf, 0, sizeof(buf));

	body_len = OneNet_FillBuf(buf); // 获取当前需要发送的数据流的总长度

	if (body_len)
	{
		if (MQTT_PacketSaveData(PROID, DEVICE_NAME, body_len, NULL, &mqttPacket) == 0) // 封包
		{
			for (; i < body_len; i++)
				mqttPacket._data[mqttPacket._len++] = buf[i];

			ESP8266_SendData(mqttPacket._data, mqttPacket._len); // 上传数据到平台
			UsartPrintf(USART_DEBUG, "Send %d Bytes\r\n", mqttPacket._len);

			MQTT_DeleteBuffer(&mqttPacket); // 删包
		}
		else
			UsartPrintf(USART_DEBUG, "WARN:	EDP_NewBuffer Failed\r\n");
	}
}

//==========================================================
//	函数名称：	OneNET_Publish
//
//	函数功能：	发布消息
//
//	入口参数：	topic：发布的主题
//				msg：消息内容
//
//	返回参数：	无
//
//	说明：
//==========================================================
void OneNET_Publish(const char *topic, const char *msg)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0}; // 协议包

	UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);

	if (MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg), MQTT_QOS_LEVEL0, 0, 1, &mqtt_packet) == 0)
	{
		ESP8266_SendData(mqtt_packet._data, mqtt_packet._len); // 向平台发送订阅请求

		MQTT_DeleteBuffer(&mqtt_packet); // 删包
	}
}

//==========================================================
//	函数名称：	OneNET_Subscribe
//
//	函数功能：	订阅
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：
//==========================================================
void OneNET_Subscribe(void)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0}; // 协议包

	char topic_buf[56];
	const char *topic = topic_buf;

	// 构建主题
	snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/thing/property/set", PROID, DEVICE_NAME);

	UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topic_buf);

	// 打包订阅消息
	if (MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, &topic, 1, &mqtt_packet) == 0)
	{
		ESP8266_SendData(mqtt_packet._data, mqtt_packet._len); // 向平台发送订阅请求

		MQTT_DeleteBuffer(&mqtt_packet); // 删包
	}
}

//==========================================================
//	函数名称：	OneNet_RevPro
//
//	函数功能：	平台返回数据检测
//
//	入口参数：	dataPtr：平台返回的数据
//
//	返回参数：	无
//
//	说明：
//==========================================================
void OneNet_RevPro(unsigned char *cmd)
{
	char *req_payload = NULL;		// 接收到的消息载荷指针
	char *cmdid_topic = NULL;		// 接收到的消息主题指针

	unsigned short topic_len = 0;	// 主题长度
	unsigned short req_len = 0;		// 载荷长度

	unsigned char qos = 0;			// QoS等级
	static unsigned short pkt_id = 0;	// 数据包ID

	unsigned char type = 0;			// MQTT消息类型

	short result = 0;

	// 解析MQTT数据包，获取消息类型
	type = MQTT_UnPacketRecv(cmd);
	UsartPrintf(USART_DEBUG, "MQTT type=%d\r\n", type);

	// 根据消息类型分别处理
	switch (type)
	{
	case MQTT_PKT_PUBLISH:		// 收到PUBLISH消息（平台下发的属性设置指令）
		// 解包PUBLISH消息，提取主题、载荷等信息
		result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id);

		UsartPrintf(USART_DEBUG, "pub result=%d, topic=%s, len=%d\r\n",
			result, cmdid_topic ? cmdid_topic : "NULL", req_len);

		// 解包成功且载荷不为空
		if (result == 0 && req_payload != NULL)
		{
			UsartPrintf(USART_DEBUG, "payload: %s\r\n", req_payload);

			// 解析JSON格式的载荷数据
			cJSON *raw_json = cJSON_Parse(req_payload);
			UsartPrintf(USART_DEBUG, "json parse: %s\r\n", raw_json ? "OK" : "FAIL");

			char id_str[64] = {0};	// 存储请求ID

			if (raw_json != NULL)
			{
				// 提取"id"字段（请求ID）
				cJSON *id_json = cJSON_GetObjectItem(raw_json, "id");
				if (id_json != NULL)
				{
					if (id_json->type == cJSON_String)		// ID是字符串类型
					{
						strncpy(id_str, id_json->valuestring, sizeof(id_str) - 1);
						UsartPrintf(USART_DEBUG, "id(string)=%s\r\n", id_str);
					}
					else if (id_json->type == cJSON_Number)	// ID是数字类型
					{
						snprintf(id_str, sizeof(id_str), "%d", id_json->valueint);
						UsartPrintf(USART_DEBUG, "id(number)=%s\r\n", id_str);
					}
				}
				else
				{
					UsartPrintf(USART_DEBUG, "id field not found\r\n");
				}

				// 提取"params"字段（设备控制参数）
				cJSON *params_json = cJSON_GetObjectItem(raw_json, "params");
				if (params_json != NULL)
				{
					// 提取LED控制参数
					cJSON *led_json = cJSON_GetObjectItem(params_json, "led");
					// 提取蜂鸣器控制参数
					cJSON *buzzer_json = cJSON_GetObjectItem(params_json, "buzzer");
					
					if (led_json != NULL)
					{
						if (led_json->type == cJSON_True)	// LED开
						{
							UsartPrintf(USART_DEBUG, "LED ON\r\n");
							LED_ON();
						}
						else if (led_json->type == cJSON_False)	// LED关
						{
							UsartPrintf(USART_DEBUG, "LED OFF\r\n");
							LED_OFF();
						}
					}

					if (buzzer_json != NULL)
					{
						if (buzzer_json->type == cJSON_True)	// 蜂鸣器开
						{
							UsartPrintf(USART_DEBUG, "BUZZER ON\r\n");
							BUZZER_ON();
						}
						else if (buzzer_json->type == cJSON_False)	// 蜂鸣器关
						{
							UsartPrintf(USART_DEBUG, "BUZZER OFF\r\n");
							BUZZER_OFF();
						}
					}
				}

				cJSON_Delete(raw_json);	// 释放JSON内存
			}

			// 构造并发送属性设置响应
			{
				char resp_topic[128];		// 响应主题
				char resp_payload[256];		// 响应载荷

				// 构建响应主题：$sys/{产品ID}/{设备名}/thing/property/set_reply
				snprintf(resp_topic, sizeof(resp_topic),
					"$sys/%s/%s/thing/property/set_reply",
					PROID, DEVICE_NAME);

				// 构建响应载荷：包含ID和成功状态码200
				if (id_str[0] != 0)	// 有ID则回传原ID
				{
					snprintf(resp_payload, sizeof(resp_payload),
						"{\"id\":\"%s\",\"code\":200,\"data\":{}}",
						id_str);
				}
				else	// 无ID则只返回状态码
				{
					snprintf(resp_payload, sizeof(resp_payload),
						"{\"code\":200,\"data\":{}}");
				}

				UsartPrintf(USART_DEBUG, "resp topic: %s\r\n", resp_topic);
				UsartPrintf(USART_DEBUG, "resp payload: %s\r\n", resp_payload);
				OneNET_Publish(resp_topic, resp_payload);	// 发送响应给平台
			}
		}
		break;

	case MQTT_PKT_PUBACK:		// 收到PUBACK消息（发布确认）
		if (MQTT_UnPacketPublishAck(cmd) == 0)
			UsartPrintf(USART_DEBUG, "MQTT Publish Send OK\r\n");
		break;

	case MQTT_PKT_SUBACK:		// 收到SUBACK消息（订阅确认）
		if (MQTT_UnPacketSubscribe(cmd) == 0)
			UsartPrintf(USART_DEBUG, "MQTT Subscribe OK\r\n");
		else
			UsartPrintf(USART_DEBUG, "MQTT Subscribe Err\r\n");
		break;

	default:	// 其他未处理的消息类型
		result = -1;
		break;
	}

	ESP8266_Clear();	// 清空ESP8266接收缓冲区

	// 释放动态分配的内存（仅对CMD和PUBLISH类型）
	if (type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}
}