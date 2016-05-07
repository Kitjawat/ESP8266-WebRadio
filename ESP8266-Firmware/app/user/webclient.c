#include "webclient.h"
#include "webserver.h"

#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/netdb.h"

#include "esp_common.h"

#include "freertos/semphr.h"

#include "vs1053.h"
#include "eeprom.h"

static enum clientStatus cstatus;
static uint32_t metacount = 0;
static uint16_t metasize = 0;

xSemaphoreHandle sConnect, sConnected, sDisconnect, sHeader;

static uint8_t connect = 0, playing = 0;


/* TODO:
	- METADATA HANDLING
	- IP SETTINGS
	- VS1053 - DELAY USING vTaskDelay
*/
struct icyHeader header = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL};
int headerlen[ICY_HEADER_COUNT] = {0,0,0,0,0,0,0,0,0,0};

char *metaint = NULL;
char *clientURL = NULL;
char *clientPath = NULL;
uint16_t clientPort = 80;

struct hostent *server;
int rest ;
///////////////
#define BUFFER_SIZE 10240

uint8_t buffer[BUFFER_SIZE];
uint16_t wptr = 0;
uint16_t rptr = 0;
uint8_t bempty = 1;

ICACHE_FLASH_ATTR uint16_t getBufferFree() {
	if(wptr > rptr ) return BUFFER_SIZE - wptr + rptr;
	else if(wptr < rptr) return rptr - wptr;
	else if(bempty) return BUFFER_SIZE; else return 0;
}

ICACHE_FLASH_ATTR uint16_t getBufferFilled() {
	return BUFFER_SIZE - getBufferFree();
}

ICACHE_FLASH_ATTR uint16_t bufferWrite(uint8_t *data, uint16_t size) {
//	uint16_t s = size;
	uint16_t i = 0;
	for(i=0; i<size; i++) {
		if(getBufferFree() == 0) { return i;}
		buffer[wptr++] = data[i];
		if(bempty) bempty = 0;
//		wptr++;
		if(wptr == BUFFER_SIZE) wptr = 0;
	}
	return size;
}

ICACHE_FLASH_ATTR uint16_t bufferRead(uint8_t *data, uint16_t size) {
	uint16_t s = size;
	uint16_t i = 0;
	if(s > getBufferFilled()) s = getBufferFilled();
	for (i = 0; i < s; i++) {
		if((BUFFER_SIZE - getBufferFree()) == 0) { return i;}
		data[i] = buffer[rptr++];
//		rptr++;
		if(rptr == BUFFER_SIZE) rptr = 0;
		if(rptr == wptr) bempty = 1;
	}
	return s;
}

ICACHE_FLASH_ATTR void bufferReset() {
	playing = 0;	
	wptr = 0;
	rptr = 0;
	bempty = 1;
}

///////////////

ICACHE_FLASH_ATTR void clientInit() {
	vSemaphoreCreateBinary(sHeader);
	vSemaphoreCreateBinary(sConnect);
	vSemaphoreCreateBinary(sConnected);
	vSemaphoreCreateBinary(sDisconnect);
	xSemaphoreTake(sConnect, portMAX_DELAY);
	xSemaphoreTake(sConnected, portMAX_DELAY);
	xSemaphoreTake(sDisconnect, portMAX_DELAY);
}

ICACHE_FLASH_ATTR uint8_t clientIsConnected() {
	if(xSemaphoreTake(sConnected, 0)) {
		xSemaphoreGive(sConnected);
		return 0;
	}
	return 1;
}

ICACHE_FLASH_ATTR struct icyHeader* clientGetHeader()
{	
	return &header;
}

	
ICACHE_FLASH_ATTR bool clientParsePlaylist(char* s)
{
  char* str; 
  char path[80] = "/";
  char url[80]; 
  char port[5] = "80";
  int remove;
  int i = 0; int j = 0;
  str = strstr(s,"<location>http://");  //for xspf
  if (str != NULL) remove = 17;
  if (str ==NULL) 
  {	  
	str = strstr(s,"http://");
	if (str != NULL) remove = 7;
  }
  if (str != NULL)
  {
	str += remove; //skip http://
	while ((str[i] != '/')&&(str[i] != ':')&&(str[i] != 0x0a)&&(str[i] != 0x0d)) {url[j] = str[i]; i++ ;j++;}
	url[j] = 0;
	j = 0;
	if (str[i] == ':')  //port
	{
		i++;
		while ((str[i] != '/')&&(str[i] != 0x0a)&&(str[i] != 0x0d)) {port[j] = str[i]; i++ ;j++;}
	}
	j = 0;
	if ((str[i] != 0x0a)&&(str[i] != 0x0d)&&(str[i] != 0)&&(str[i] != '"')&&(str[i] != '<'))
	{	
	  while ((str[i] != 0x0a)&&(str[i] != 0x0d)&&(str[i] != 0)&&(str[i] != '"')&&(str[i] != '<')) {path[j] = str[i]; i++; j++;}
	  path[j] = 0;
	}

	if (strncmp(url,"localhost",9)!=0) clientSetURL(url);
	clientSetPath(path);
	clientSetPort(atoi(port));
	printf("url: %s, path: %s, port: %s\n",url,path,port);
	return true;
  }
  else 
  { 
   cstatus = C_DATA;
   return false;
  }
}
ICACHE_FLASH_ATTR char* stringify(char* str,int *len)
{
		if ((strchr(str,'"') == NULL)&&(strchr(str,'/') == NULL)) return str;
		char* new = malloc(strlen(str)+100);
//		printf("stringify: enter: len:%d\n",*len);
		int i=0 ,j =0;
		for (i = 0;i< strlen(str)+100;i++) new[i] = 0;
		for (i=0;i< strlen(str);i++)
		{
			if (str[i] == '"') {
				new[j++] = '\\';
			}
			if (str[i] == '/') {
				new[j++] = '\\';
			}
			new[j++] =(str)[i] ;
		}
		free(str);
		if (j+1>*len)*len = j+1;
		return new;		
}

ICACHE_FLASH_ATTR void clientSaveMetadata(char* s,int len,bool catenate)
{
	    int oldlen = 0;
		char* t_end = NULL;
		char* t_quote;
		char* t ;
		bool found = false;
		printf("Entry meta s= %s\n",s);
		if (catenate) oldlen = strlen(header.members.mArr[METADATA]);
		t = s;
		t_end = strstr(t,";StreamUrl='");
		if (t_end != NULL) { *t_end = 0;found = true;} 
		t = strstr(t,"StreamTitle='");
		if (t!= NULL) {t += 13;found = true;} else t = s;
		len = strlen(t);
//		printf("Len= %d t= %s\n",len,t);
		if ((t_end != NULL)&&(len >=3)) t_end -= 3;
		else if (len >3) t_end = t+len-3; else t_end = t;
		if (found)
		{	
			t_quote = strstr(t_end,"'");
			if (t_quote !=NULL){ t_end = t_quote; *t_end = 0;}
		} else {t = "";len = 0;}
//		printf("clientsaveMeta t= 0x%x t_end= 0x%x  t=%s\n",t,t_end,t);
		
		s = t;
		if((header.members.mArr[METADATA] != NULL)&&(headerlen[METADATA] < (oldlen+len+1)*sizeof(char))) 
		{	// realloc if new malloc is bigger (avoid heap fragmentation)
//			printf("clientsaveMeta free  %d < %d\n",headerlen[METADATA],(oldlen+len+1)*sizeof(char));
			free(header.members.mArr[METADATA]);
			header.members.mArr[METADATA] = (char*)malloc((oldlen  +len+1)*sizeof(char));
			headerlen[METADATA] = (oldlen +len+1)*sizeof(char);
		} else
		if(header.members.mArr[METADATA] == NULL) {
			header.members.mArr[METADATA] = (char*)malloc((oldlen  +len+1)*sizeof(char));
//			printf("clientsaveMeta malloc len:%d\n",(oldlen  +len+1)*sizeof(char));
			headerlen[METADATA] = (oldlen +len+1)*sizeof(char);
		}
		
		if(header.members.mArr[METADATA] != NULL)
		{
			int i;
			header.members.mArr[METADATA][oldlen +len] = 0;
			strncpy(&(header.members.mArr[METADATA][oldlen]), s,len);
//			printf("metadata before len :%d  addr:0x%x  cont:%s\n",headerlen[METADATA] ,header.members.mArr[METADATA],header.members.mArr[METADATA]);
			header.members.mArr[METADATA] = stringify(header.members.mArr[METADATA],&headerlen[METADATA]);
//			printf("metadata len: %d  addr:0x%x  metadata:%s\n",headerlen[METADATA] ,header.members.mArr[METADATA],header.members.mArr[METADATA]);
		}	
}	
ICACHE_FLASH_ATTR void clearHeaders()
{
	uint8_t header_num;
	for(header_num=0; header_num<ICY_HEADER_COUNT; header_num++) {
		if(header_num != METAINT) if(header.members.mArr[header_num] != NULL) {
			header.members.mArr[header_num][0] = 0;				
		}
	}
	header.members.mArr[METAINT] = 0;
}
	
ICACHE_FLASH_ATTR bool clientSaveOneHeader(char* t, uint16_t len, uint8_t header_num)
{
	if((header.members.mArr[header_num] != NULL)&&(headerlen[header_num] < (len+1)*sizeof(char))) 
	{	// realloc if new malloc is bigger (avoid heap fragmentation)
		free(header.members.mArr[header_num]);
		header.members.mArr[header_num] = NULL;
	}
	if(header.members.mArr[header_num] == NULL) 
//	if(header.members.mArr[header_num] != NULL) free(header.members.mArr[header_num]);
	header.members.mArr[header_num] = (char*)malloc((len+1)*sizeof(char));
	headerlen[header_num] = (len+1)*sizeof(char);
	if(header.members.mArr[header_num] != NULL)
	{
		int i;
		for(i = 0; i<len+1; i++) header.members.mArr[header_num][i] = 0;
		strncpy(header.members.mArr[header_num], t, len);
//		printf("header before addr:0x%x  cont:%s\n",header.members.mArr[header_num],header.members.mArr[header_num]);
		header.members.mArr[header_num] = stringify(header.members.mArr[header_num],&headerlen[header_num]);
//		printf("header after  addr:0x%x  cont:%s\n",header.members.mArr[header_num],header.members.mArr[header_num]);
	}
	return true;
}

	
ICACHE_FLASH_ATTR bool clientParseHeader(char* s)
{
	// icy-notice1 icy-notice2 icy-name icy-genre icy-url icy-br
	uint8_t header_num;
	bool ret = false;
//	printf("ParseHeader: %s\n",s);
	xSemaphoreTake(sHeader,portMAX_DELAY);
	if ((cstatus != C_HEADER1)&& (cstatus != C_PLAYLIST))// not ended. dont clear
	{
		clearHeaders();
	}
	for(header_num=0; header_num<ICY_HEADERS_COUNT; header_num++)
	{
//				printf("icy deb: %d\n",header_num);		
		char *t;
		t = strstr(s, icyHeaders[header_num]);
		if( t != NULL )
		{
			t += strlen(icyHeaders[header_num]);
			char *t_end = strstr(t, "\r\n");
			if(t_end != NULL)
			{
//				printf("icy in: %d\n",header_num);		
				uint16_t len = t_end - t;
				if(header_num != METAINT) // Text header field
				{
					ret = clientSaveOneHeader(t, len, header_num);
				}
				else // Numerical header field
				{
					if ((metaint != NULL) && ( (headerlen[header_num]) < ((len+1)*sizeof(char)) ))
					{
						free (metaint);
						metaint = NULL;
					}
					if (metaint == NULL) { metaint = (char*) malloc((len+1)*sizeof(char));headerlen[header_num]= (len+1)*sizeof(char);}
					if (metaint != NULL)
					{					
						int i;
						for(i = 0; i<len+1; i++) metaint[i] = 0;
						strncpy(metaint, t, len);
						header.members.single.metaint = atoi(metaint);
//						printf("MetaInt= %s, Metaint= %d\n",metaint,header.members.single.metaint);
						ret = true;
					}
//			printf("icy: %s: %d\n",icyHeaders[header_num],header.members.single.metaint);					
				}
			}
		}
	}
	xSemaphoreGive(sHeader);
	return ret;
}

ICACHE_FLASH_ATTR void clientSetURL(char* url)
{
	int l = strlen(url)+1;
	if ((clientURL != NULL)&&((strlen(clientURL)+1) < l*sizeof(char))) {free(clientURL);clientURL = NULL;} //avoid fragmentation
//	if (clientURL != NULL) {free(clientURL);clientURL = NULL;}
	if (clientURL == NULL) clientURL = (char*) malloc(l*sizeof(char));
	if(clientURL != NULL) strcpy(clientURL, url);
	printf("##CLI.URLSET#:%s\n",clientURL);
}

ICACHE_FLASH_ATTR void clientSetPath(char* path)
{
	int l = strlen(path)+1;
	if ((clientPath != NULL)&&((strlen(clientPath)+1) < l*sizeof(char))){free(clientPath); clientPath = NULL;} //avoid fragmentation
//	if(clientPath != NULL) free(clientPath);
	if (clientPath == NULL) clientPath = (char*) malloc(l*sizeof(char));
	if(clientPath != NULL) strcpy(clientPath, path);
	printf("##CLI.PATHSET#:%s\n",clientPath);
}

ICACHE_FLASH_ATTR void clientSetPort(uint16_t port)
{
	clientPort = port;
	printf("##CLI.PORTSET#:%d\n",port);
}

ICACHE_FLASH_ATTR void clientConnect()
{
	cstatus = C_HEADER;
	metacount = 0;
	metasize = 0;
printf("ClientConnect\n");
	//if(netconn_gethostbyname(clientURL, &ipAddress) == ERR_OK) {
	if(server) free(server);
	if((server = (struct hostent*)gethostbyname(clientURL))) {
		xSemaphoreGive(sConnect);
	} else {
		clientDisconnect();
	}
}

ICACHE_FLASH_ATTR void clientDisconnect()
{
	//connect = 0;
	xSemaphoreGive(sDisconnect);
	printf("##CLI.STOPPED#\n");
	clearHeaders();
}

ICACHE_FLASH_ATTR void clientReceiveCallback(char *pdata, unsigned short len)
{
	/* TODO:
		- What if header is in more than 1 data part? // ok now ...
		- Metadata processing // ok
		- Buffer underflow handling (?) ?
	*/
	static int metad ;
	uint16_t l ;
	char* t1;
	static char* head;
	static uint16_t lhead;
	bool  icyfound;

	if (cstatus != C_DATA){printf("cstatus= %d\n",cstatus);  printf("Len=%d, Byte_list = %s\n",len,pdata);}
	switch (cstatus)
	{
	case C_PLAYLIST:
 
        if (!clientParsePlaylist(pdata)) //need more
		  cstatus = C_PLAYLIST1;
		else clientDisconnect();  
    break;
	case C_PLAYLIST1:
       clientDisconnect();		  
        clientParsePlaylist(pdata) ;//more?
		cstatus = C_PLAYLIST;
	break;
	case C_HEADER:
		clearHeaders();
		metad = -1;
		lhead = 0;
		t1 = strstr(pdata, "302 "); 
		if (t1 ==NULL) t1 = strstr(pdata, "301 "); 
		if (t1 != NULL) { // moved to a new address
			if( strcmp(t1,"Found")||strcmp(t1,"Temporarily")||strcmp(t1,"Moved"))
			{
				printf("Header: Moved\n");
				clientDisconnect();
				clientParsePlaylist(pdata);
				cstatus = C_PLAYLIST;				
			}	
			break;
		}
		head = malloc(len);
	case C_HEADER1:  // not ended
		{
			cstatus = C_HEADER1;
			head = realloc(head,lhead+len+1);
			memcpy(head+lhead,pdata,len);
			lhead += len;
			t1 = strstr(head, "\r\n\r\n"); // END OF HEADER
			printf("Header len: %d,  Header: %s\n",lhead,head);
			if ((t1 != NULL) && (lhead >= 4)&&(t1 <= head+lhead-4)) 
			{	
				icyfound = clientParseHeader(head);
				if(header.members.single.metaint > 0) 
				metad = header.members.single.metaint;
//				printf("t1: 0x%x, cstatus: %d, icyfound: %d  metad:%d Metaint:%d\n",t1,cstatus,icyfound,metad,header.members.single.metaint); 
				cstatus = C_DATA;				
				VS1053_flush_cancel(1);
				int newlen = lhead - (t1-head) - 4;
				if(newlen > 0) clientReceiveCallback(t1+4, newlen);
				free(head);
			}
		}
	break;
	default:
//		 buf = pdata;		
// -----------	
		rest = 0;
		if(((header.members.single.metaint != 0)&&(len > metad))) {
			l = pdata[metad]*16;
			rest = len - metad  -l -1;
			if (l !=0)
			{
//				printf("len:%d, metad:%d, l:%d, rest:%d, str: %s\n",len,metad, l,rest,pdata+metad+1 );
				if (rest <0) *(pdata+len) = 0; //truncated
				clientSaveMetadata(pdata+metad+1,l,false);
			}				
			while(getBufferFree() < metad){ vTaskDelay(1); /*printf(":");*/}
				bufferWrite(pdata, metad); 
			metad = header.members.single.metaint - rest ; //until next
			if (rest >0)
			{	
				while(getBufferFree() < rest) {vTaskDelay(1); /*printf(".");*/}
					bufferWrite(pdata+len-rest, rest); 
				rest = 0;
			} 	
		} else 
		{	
	        if (rest <0) 
			{
//				printf("Negative len = %d, metad = %d  rest = %d\n",len,metad,rest);
				clientSaveMetadata(pdata,0-rest,true);
				/*buf =pdata+rest;*/ len +=rest;metad += rest; rest = 0;
			}	
			if (header.members.single.metaint != 0) metad -= len;
//			printf("len = %d, metad = %d  metaint= %d\n",len,metad,header.members.single.metaint);
			while(getBufferFree() < len) {vTaskDelay(1); /*printf("-");*/}
			if (len >0) bufferWrite(pdata+rest, len);	
		}
// ---------------			
		if(!playing && (getBufferFree() < BUFFER_SIZE/3)) {
			playing=1;
			printf("Playing\n");
		}	
    }
}

#define VSTASKBUF	1280  
//1344
ICACHE_FLASH_ATTR void vsTask(void *pvParams) {
	uint8_t b[VSTASKBUF];

	struct device_settings *device;
	register uint16_t size ,s;
	Delay(100);
	VS1053_Start();
	device = getDeviceSettings();
	Delay(300);

	VS1053_SPI_SpeedUp();
	VS1053_SetVolume(device->vol);	
	VS1053_SetTreble(device->treble);
	VS1053_SetBass(device->bass);
	VS1053_SetTrebleFreq(device->freqtreble);
	VS1053_SetBassFreq(device->freqbass);
	VS1053_SetSpatial(device->spacial);
	free(device);	
	while(1) {
		if(playing) {
			size = bufferRead(b, VSTASKBUF), s = 0;
			while(s < size) {
				s += VS1053_SendMusicBytes(b+s, size-s);
			} 
			vTaskDelay(2);
		} else vTaskDelay(10);
	}
}

ICACHE_FLASH_ATTR void clientTask(void *pvParams) {
#define RECEIVE 1440
//1440	for MTU 1500
	int sockfd, bytes_read;
	struct sockaddr_in dest;
//	uint8_t buffer[RECEIVE];
	uint8_t* buffer= malloc(RECEIVE);
	clearHeaders();
	while(1) {
		xSemaphoreGive(sConnected);

		if(xSemaphoreTake(sConnect, portMAX_DELAY)) {

			xSemaphoreTake(sDisconnect, 0);

			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if(sockfd >= 0) ; //printf("WebClient Socket created\n");
			else printf("WebClient Socket creation failed\n");
			bzero(&dest, sizeof(dest));
			dest.sin_family = AF_INET;
			dest.sin_port = htons(clientPort);
			dest.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)(server -> h_addr_list[0])));

			/*---Connect to server---*/
			if(connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) >= 0) 
			{
//				printf("WebClient Socket connected\n");
				bzero(buffer, RECEIVE);
				
				char *t0 = strstr(clientPath, ".m3u");
				if (t0 == NULL)  t0 = strstr(clientPath, ".pls");
				if (t0 == NULL)  t0 = strstr(clientPath, ".xspf");				
				if (t0 != NULL)  // a playlist asked
				{
				  cstatus = C_PLAYLIST;
				  sprintf(buffer, "GET %s HTTP/1.0\r\nHOST: %s\r\n\r\n", clientPath,clientURL); //ask for the playlist
			    } 
				else sprintf(buffer, "GET %s HTTP/1.0\r\nHost: %s\r\nicy-metadata:1\r\n\r\n", clientPath,clientURL); 
//				printf("st:%d, Client Sent:\n%s\n",cstatus,buffer);
				send(sockfd, buffer, strlen(buffer), 0);
				
				xSemaphoreTake(sConnected, 0);

				do
				{
					bytes_read = recv(sockfd, buffer, RECEIVE, 0);
//					printf("s:%d   ", bytes_read);
					if ( bytes_read > 0 ) {
						clientReceiveCallback(buffer, bytes_read);
					}	
					if(xSemaphoreTake(sDisconnect, 0)) break;
				}
				while ( bytes_read > 0 );
			} else printf("WebClient Socket fails to connect %d\n", errno);
			/*---Clean up---*/
			if (bytes_read == 0 ) 
			{
					clientDisconnect(); 
					clientSaveOneHeader("Not Found", 9,METANAME);
			}//jpc
			bufferReset();
			if (playing) VS1053_flush_cancel(2);
			playing = 0;
			VS1053_flush_cancel(0);
			shutdown(sockfd,SHUT_RDWR);
			vTaskDelay(10);	
			close(sockfd);
//			printf("WebClient Socket closed\n");
			if (cstatus == C_PLAYLIST) 			
			{
			  clientConnect();
			}
		}
	}
}
