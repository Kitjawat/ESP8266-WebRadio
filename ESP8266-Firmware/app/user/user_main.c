/******************************************************************************
 * Copyright 2015 Piotr Sperka (http://www.piotrsperka.info)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
*******************************************************************************/
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "el_uart.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "interface.h"
#include "webserver.h"
#include "webclient.h"

#include "vs1053.h"

#include "eeprom.h"

	struct station_config config;
	int FlashOn = 5,FlashOff = 5;
	sc_status status = 0;
void cb(sc_status stat, void *pdata)
{
	printf("SmartConfig status received: %d\n",status);
	status = stat;
	if (stat == SC_STATUS_LINK_OVER) if (pdata) printf("SmartConfig: %d:%d:%d:%d\n",((char*) pdata)[0],((char*)pdata)[1],((char*)pdata)[2],((char*)pdata)[3]);
}

void uartInterfaceTask(void *pvParameters) {
	char tmp[64];
	int t = 0;
	for(t = 0; t<64; t++) tmp[t] = 0;
	t = 0;
	uart_rx_init();
	printf("UART READY TO READ\n");
	
	wifi_station_set_auto_connect(false);
	wifi_station_set_hostname("WifiWebRadio");
	while (wifi_station_get_connect_status() != STATION_GOT_IP)
	{	
		status = wifi_station_get_config_default(&config); 
		if (status) 
		{		
			int i = 0;
			FlashOn = FlashOff = 20;
			printf("Config found\n");	
			wifi_station_connect();
			while (wifi_station_get_connect_status() != STATION_GOT_IP) 
			{	
				vTaskDelay(10);// 100 ms
				if (i++ >= 100) break; // 10 seconds
			}	
			if (i >= 100)
			{
				printf("Config not found\nTrying smartconfig");
				FlashOn = FlashOff = 50;
				smartconfig_set_type(SC_TYPE_ESPTOUCH);
				smartconfig_start(cb);
				printf("smartConfig started. Waiting for ios or android 'ESP8266 SmartConfig' application\n");
				i = 0;
				while (status != SC_STATUS_LINK) 
				{	
					vTaskDelay(10); //100 ms
					if (i++ >= 1000) break; // 100 seconds
				}	
				if (i >= 1000)
				{
					FlashOn = 10;FlashOff = 100;
					vTaskDelay(1000);
					printf("Config not found\n");
					printf("Send uart command: wifi.con(\"YOUR_SSID\",\"YOUR_PASSWORD\") [hit ENTER, don't forget about quotation marks!]\n");
					printf("or load 'ESP8266 SmartConfig' application on ios or android\n");
				} else
				{	
					smartconfig_stop();
				}
			} else {break;} // success
		}
	}
	FlashOn = 100;FlashOff = 10;	
	while(1) {
		while(1) {
			char c = uart_getchar();
			if(c == '\r') break;
			if(c == '\n') break;
			tmp[t] = c;
			t++;
			if(t == 64) t = 0;
		}
		checkCommand(t, tmp);
		for(t = 0; t<64; t++) tmp[t] = 0;
		t = 0;
		vTaskDelay(25); // 250ms
	}
}

UART_SetBaudrate(uint8 uart_no, uint32 baud_rate) {
	uart_div_modify(uart_no, UART_CLK_FREQ / baud_rate);
}

void testtask(void* p) {
	gpio16_output_conf();
	while(1) {
		gpio16_output_set(0);
		vTaskDelay(FlashOff);
		gpio16_output_set(1);
		vTaskDelay(FlashOn);
	};
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
    Delay(400);
	UART_SetBaudrate(0,115200);
	wifi_set_opmode(STATION_MODE);
	Delay(500);	
	printf ("Heap size: %d\n",xPortGetFreeHeapSize( ));
	clientInit();
	VS1053_HW_init();
	TCP_WND = 2 * TCP_MSS;

	xTaskCreate(testtask, "t0", 176, NULL, 1, NULL); // DEBUG/TEST
	xTaskCreate(uartInterfaceTask, "t1", 176, NULL, 2, NULL);
	xTaskCreate(serverTask, "t2", 512, NULL, 3, NULL);
	xTaskCreate(clientTask, "t3", 512, NULL, 4, NULL);
	xTaskCreate(vsTask, "t4", 376, NULL, 4, NULL);
}

