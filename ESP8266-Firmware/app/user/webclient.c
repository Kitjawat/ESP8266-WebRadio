#include "webclient.h"
#include "webserver.h"

#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/netdb.h"

#include "esp_common.h"

#include "freertos/semphr.h"

#include "vs1053.h"

struct icyHeader header = {NULL, NULL, NULL, NULL, 0};
char *clientURL = NULL;
char *clientPath = NULL;
uint16_t clientPort = 80;

struct hostent *server;

static const char* icyHeaders[] = { "icy-name:", "icy-notice1:", "icy-notice2:",  "icy-url:", "icy-genre:", "icy-br:", "icy-metaint:" };

static enum clientStatus cstatus;
static uint32_t metacount = 0;
static uint16_t metasize = 0;

xSemaphoreHandle sConnect, sConnected, sDisconnect;

static uint8_t connect = 0, playing = 0;


/* TODO:
	- METADATA HANDLING
	- IP SETTINGS
	- VS1053 - DELAY USING vTaskDelay
*/


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
	uint16_t s = size, i = 0;
	for(i=0; i<s; i++) {
		if(getBufferFree() == 0) return i;
		buffer[wptr] = data[i];
		if(bempty) bempty = 0;
		wptr++;
		if(wptr == BUFFER_SIZE) wptr = 0;
	}
	return s;
}

ICACHE_FLASH_ATTR uint16_t bufferRead(uint8_t *data, uint16_t size) {
	uint16_t s = size, i = 0;
	if(s > getBufferFilled()) s = getBufferFilled();
	for (i = 0; i < s; i++) {
		if(getBufferFilled() == 0) return i;
		data[i] = buffer[rptr];
		rptr++;
		if(rptr == BUFFER_SIZE) rptr = 0;
		if(rptr == wptr) bempty = 1;
	}
	return s;
}

ICACHE_FLASH_ATTR void bufferReset() {
	wptr = 0;
	rptr = 0;
	bempty = 1;
}

///////////////

ICACHE_FLASH_ATTR void clientInit() {
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

ICACHE_FLASH_ATTR void clientParsePlaylist(char* s)
{
  char* str = strstr(s,"http://");
  char path[100];
  char url[100]; 
  char port[16] = "80";
  int i = 0; int j = 0;

  if (str != NULL)
  {
  str +=7; //skip http
  while ((str[i] != '/')&&(str[i] != ':')) {url[j] = str[i]; i++ ;j++;}
  url[j] = 0;
  j = 0;
  if (str[i] == ':')
  {
    i++;
    while (str[i] != '/') {port[j] = str[i]; i++ ;j++;}
  }
  j = 0;
  while ((str[i] != 0x0d)&&(str[i] != 0)&&(str[i] != '"')) {path[j] = str[i]; i++; j++;}
  path[j] = 0;
  printf("url: %s, path: %s, port: %s\n",url,path,port);
  clientSetURL(url);
  clientSetPath(path);
  clientSetPort(atoi(port));
  }

}

ICACHE_FLASH_ATTR void clientParseHeader(char* s)
{
	// icy-notice1 icy-notice2 icy-name icy-genre icy-url icy-br
	uint8_t header_num;
	for(header_num=0; header_num<ICY_HEADERS_COUNT; header_num++) {
		if(header_num != 6) if(header.members.mArr[header_num] != NULL) {
			free(header.members.mArr[header_num]);
			header.members.mArr[header_num] = NULL;
		}
	}
	for(header_num=0; header_num<ICY_HEADERS_COUNT; header_num++)
	{
		char *t;
		t = strstr(s, icyHeaders[header_num]);
		if( t != NULL )
		{
			t += strlen(icyHeaders[header_num]);
			char *t_end = strstr(t, "\r\n");
			if(t_end != NULL)
			{
				uint16_t len = t_end - t;
				if(header_num != 6) // Text header field
				{
					if(header.members.mArr[header_num] != NULL) free(header.members.mArr[header_num]);
					header.members.mArr[header_num] = (char*)malloc((len+1)*sizeof(char));
					if(header.members.mArr[header_num] != NULL)
					{
						int i;
						for(i = 0; i<len+1; i++) header.members.mArr[header_num][i] = 0;
						strncpy(header.members.mArr[header_num], t, len);
					}
				}
				else // Numerical header field
				{
					char *buf = (char*) malloc((len+1)*sizeof(char));
					if(buf != NULL)
					{
						int i;
						for(i = 0; i<len+1; i++) buf[i] = 0;
						strncpy(buf, t, len);
						header.members.single.metaint = atoi(buf);
						free(buf);
					}
				}
			}
		}
	}
}

ICACHE_FLASH_ATTR uint16_t clientProcessMetadata(char* s, uint16_t size)
{
	uint16_t processed = 0;
	if(metasize == 0) { metasize = s[0]*16; processed = 1; }
	if(metasize == 0) return 1; // THERE IS NO METADATA

	if(processed == 1) // BEGINNING OF NEW METADATA; PREPARE MEMORY SPACE
	{
		if(header.members.single.metadata != NULL) free(header.members.single.metadata);
		header.members.single.metadata = (char*) malloc((metasize+1) * sizeof(char));
		if(header.members.single.metadata == NULL)
		{
			cstatus = C_DATA;
			return metasize;
		}
		int i;
		for(i=0; i<metasize+1; i++) header.members.single.metadata[i] = 0;
	}
	uint16_t startpos = 0;
	while(header.members.single.metadata[startpos] != 0) startpos++; // FIND ENDING OF METADATA
	if((size-processed) >= metasize)
	{
		int i;
		for(i=0; i<metasize; i++) header.members.single.metadata[startpos+i] = s[processed+i];
		processed += metasize;
	}
	else
	{
		int i;
		for(i=0; i<(size-processed); i++) header.members.single.metadata[startpos+i] = s[processed+i];
		processed += (size-processed);
		metasize -= (size-processed);
	}
	if(metasize == 0) {
		cstatus = C_DATA; // METADATA READ - BACK TO STREAM DATA
		// DEBUG
		printf("\n");
		printf(header.members.single.metadata);
	}
	return processed;
}

ICACHE_FLASH_ATTR void clientSetURL(char* url)
{
	int l = strlen(url)+1;
	if(clientURL != NULL) free(clientURL);
	clientURL = (char*) malloc(l*sizeof(char));
	if(clientURL != NULL) strcpy(clientURL, url);
	printf("\n##CLI.URLSET#\n");
}

ICACHE_FLASH_ATTR void clientSetPath(char* path)
{
	int l = strlen(path)+1;
	if(clientPath != NULL) free(clientPath);
	clientPath = (char*) malloc(l*sizeof(char));
	if(clientPath != NULL) strcpy(clientPath, path);
	printf("\n##CLI.PATHSET#\n");
}

ICACHE_FLASH_ATTR void clientSetPort(uint16_t port)
{
	clientPort = port;
	printf("\n##CLI.PORTSET#\n");
}

ICACHE_FLASH_ATTR void clientConnect()
{
	cstatus = C_HEADER;
	metacount = 0;
	metasize = 0;

	//if(netconn_gethostbyname(clientURL, &ipAddress) == ERR_OK) {
	if(server) free(server);
	if((server = (struct hostent*)gethostbyname(clientURL))) {
		xSemaphoreGive(sConnect);
		//connect = 1; // todo: semafor!!!
	} else {
		clientDisconnect();
	}
}

ICACHE_FLASH_ATTR void clientDisconnect()
{
	//connect = 0;
	xSemaphoreGive(sDisconnect);
	printf("\n##CLI.STOPPED#\n");
}

ICACHE_FLASH_ATTR void clientReceiveCallback(void *arg, char *pdata, unsigned short len)
{
	/* TODO:
		- What if header is in more than 1 data part?
		- Metadata processing
		- Buffer underflow handling (?)
	*/
	static int metad = -1;
	uint16_t l ;
	char *t1;

	switch (cstatus)
	{
	case C_PLAYLIST:
	    printf("Byte_list = %s\n",pdata);
		clientDisconnect();
        clientParsePlaylist(pdata);
//		clientConnect();
    break;
	case C_HEADER:
	    printf("Byte_read = %s\n",pdata);
		t1 = strstr(pdata, "302 Found"); 
		if (t1 != NULL) { // moved
		  clientDisconnect();
		  clientParsePlaylist(pdata);
		  cstatus = C_PLAYLIST;
		}
		else {
		  clientParseHeader(pdata);
		  if(header.members.single.metaint > 0) metad = header.members.single.metaint;
		  t1 = strstr(pdata, "\r\n\r\n"); // END OF HEADER
		  if(t1 != NULL) {
			//processed = t1-pdata + 4;
			cstatus = C_DATA;
			int newlen = len - (t1-pdata) - 4;
			if(newlen > 0) clientReceiveCallback(NULL, t1+4, newlen);
		  }
		}
	break;
	default:
		l = 0;
		char* buf = pdata;
		/*if(len > metad) {
			int l = pdata[metad+1]*16;
			printf("%d\n", l);
			int rest = len - (&(pdata[metad+1])-pdata) -l;
			metad = header.members.single.metaint - rest;
		} else metad -= len;*/
		do {
			if(getBufferFree() < len) vTaskDelay(1);
			else l = bufferWrite(buf, len);
		} while(l < len);

		if(!playing && getBufferFree() < BUFFER_SIZE/2) {
			playing=1;
		}
	break;	
    }
}

ICACHE_FLASH_ATTR void vsTask(void *pvParams) {
	uint8_t b[1024];

	while(1) {
		if(playing) {
			uint16_t size = bufferRead(b, 1024), s = 0;
			while(s < size) {
				s += VS1053_SendMusicBytes(b+s, size-s);
				vTaskDelay(2);
			}
		} else vTaskDelay(10);
	}
}

ICACHE_FLASH_ATTR void clientTask(void *pvParams) {
	int sockfd, bytes_read;
	struct sockaddr_in dest;
	uint8_t buffer[1024];

	while(1) {
		xSemaphoreGive(sConnected);

		if(xSemaphoreTake(sConnect, portMAX_DELAY)) {

			xSemaphoreTake(sDisconnect, 0);

			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if(sockfd >= 0) printf("Socket created\n");
			bzero(&dest, sizeof(dest));
			dest.sin_family = AF_INET;
			dest.sin_port = htons(clientPort);
			dest.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)(server -> h_addr_list[0])));

			/*---Connect to server---*/
			if(connect(sockfd, (struct sockaddr*)&dest, sizeof(dest)) >= 0) 
			{
				printf("Socket connected\n");
				bzero(buffer, sizeof(buffer));
				
				char *t0 = strstr(clientPath, ".m3u");
				if (t0 != NULL)  // a playlist asked
				{
				  cstatus = C_PLAYLIST;
				  sprintf(buffer, "GET %s HTTP/1.1\r\nHOST: %s\r\n\r\n", clientPath,clientURL); //ask the playlist
			    } else 
				{
				  t0 = strstr(clientPath, ".pls");
				  if (t0 != NULL)  // a playlist asked
				  {
					cstatus = C_PLAYLIST;
					sprintf(buffer, "GET %s HTTP/1.1\r\nHOST: %s\r\n\r\n", clientPath,clientURL); //ask the playlist
				  }
				  else sprintf(buffer, "GET %s HTTP/1.1\r\nHOST: %s\r\nicy-metadata:0\r\n\r\n", clientPath,clientURL); //jpc HTTP 1.1
				}
				printf(buffer);
				send(sockfd, buffer, strlen(buffer), 0);

				do
				{
				    vTaskDelay(1); //jpc
					bzero(buffer, sizeof(buffer));
					bytes_read = recv(sockfd, buffer, sizeof(buffer), 0);
					if ( bytes_read > 0 ) {
						clientReceiveCallback(NULL, buffer, bytes_read);
					}
					if(xSemaphoreTake(sDisconnect, 0)) break;
					xSemaphoreTake(sConnected, 0);
					vTaskDelay(1);
				}
				while ( bytes_read > 0 );
			} else printf("Socket fails to connect\n");
			/*---Clean up---*/
			if (bytes_read == 0 ) clientDisconnect(); //jpc
			playing = 0;
			bufferReset();
			close(sockfd);
			printf("Socket closed\n");
			if (cstatus == C_PLAYLIST) 			
			{
			  clientConnect();
			   printf("pass %d\n",5);
			}
		}
	}
}
