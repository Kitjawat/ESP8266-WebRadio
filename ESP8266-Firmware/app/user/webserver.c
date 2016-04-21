#include "webserver.h"
#include "serv-fs.h"
#include "webclient.h"
#include "vs1053.h"

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "flash.h"
#include "eeprom.h"


/*
ICACHE_FLASH_ATTR char* my_strdup(char* string, int length)
{
  char* newstr = (char*)malloc((length+1)*sizeof(char));
  while (newstr == NULL)
	{
		newstr = (char*)malloc((length+1)*sizeof(char));
        if ( newstr == NULL ){
			int i = 0;
			do { 
			i++;		
			printf ("Heap size: %d\n",xPortGetFreeHeapSize( ));
			vTaskDelay(10);
			printf("strdup malloc fails for %d\n",(length+1)*sizeof(char) );
			}
			while (i<10);
			if (i >=10) {  return NULL;}
		} 		
	}
    int i;
    for(i=0; i<length+1; i++) if(i < length) newstr[i] = string[i]; else newstr[i] = 0;
	return newstr;
}
*/
ICACHE_FLASH_ATTR char* str_replace ( char *string, const char *substr, const char *replacement, int length ){
  char *tok = NULL;
  char *newstr = NULL;
  char *oldstr = NULL;
  /* if either substr or replacement is NULL, duplicate string a let caller handle it */
  if ( substr == NULL ) {
	return string;
  }
  if( replacement == NULL ) replacement = "";
/*  newstr = my_strdup(string, length);
  free(string);*/
  newstr = string;
  while ( (tok = strstr ( newstr, substr ))){
    oldstr = newstr;
	newstr = NULL;
	while (newstr == NULL)
	{
       newstr = malloc ( strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) + 1 );
    /*failed to alloc mem, free old string and return NULL */
      if ( newstr == NULL ){
		int i = 0;
		do { 
        i++;		
		printf ("Heap size: %d\n",xPortGetFreeHeapSize( ));
		vTaskDelay(10);
 	    printf("strreplace malloc fails for %d\n",strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) + 1 );
 		}
		while (i<10);
		if (i >=10) { /*free(string);*/ return oldstr;}
      } 
	}
    memcpy ( newstr, oldstr, tok - oldstr );
    memcpy ( newstr + (tok - oldstr), replacement, strlen ( replacement ) );
    memcpy ( newstr + (tok - oldstr) + strlen( replacement ), tok + strlen ( substr ), strlen ( oldstr ) - strlen ( substr ) - ( tok - oldstr ) );
    memset ( newstr + strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) , 0, 1 );
    free (oldstr);
  }
  return newstr;
}


ICACHE_FLASH_ATTR char* serverParseCGI(char* html, int length)
{
  clientTakesHeader();
  struct icyHeader *header = clientGetHeader();
  char* h = html;
  char buf[15];

  h = str_replace(h, "#ICY-NAME#", header->members.single.name, length);
  h = str_replace(h, "#ICY-DESCRIPTION#", header->members.single.description, strlen(h)); 
  h = str_replace(h, "#ICY-NOTICE1#", header->members.single.notice1, strlen(h));
  taskYIELD();
  if ((header->members.single.notice2 ==NULL)|| strlen(header->members.single.notice2) == 0)
    h = str_replace(h, "#ICY-NOTICE2#", header->members.single.audioinfo, strlen(h));
  else
    h = str_replace(h, "#ICY-NOTICE2#", header->members.single.notice2, strlen(h));
  h = str_replace(h, "#ICY-GENRE#", header->members.single.genre, strlen(h));
  h = str_replace(h, "#ICY-URL#", header->members.single.url, strlen(h));
  h = str_replace(h, "#ICY-BITRATE#", header->members.single.bitrate, strlen(h));
  taskYIELD();
  
  sprintf(buf, "%d", 254-VS1053_GetVolume());
  h = str_replace(h, "#SOUND-VOL#", buf, strlen(h));
  sprintf(buf, "%d", VS1053_GetTreble());
  h = str_replace(h, "#SOUND-TREBLE#", buf, strlen(h));
  sprintf(buf, "%d", VS1053_GetBass());
  h = str_replace(h, "#SOUND-BASS#", buf, strlen(h));
  sprintf(buf, "%d", VS1053_GetTrebleFreq());
  h = str_replace(h, "#SOUND-TREBLE-FREQ#", buf, strlen(h));
  sprintf(buf, "%d", VS1053_GetBassFreq());
  h = str_replace(h, "#SOUND-BASS-FREQ#", buf, strlen(h));
  sprintf(buf, "%d", VS1053_GetSpatial());
  h = str_replace(h, "#SOUND-SPACIAL#", buf, strlen(h));
  clientGivesHeader();
  return h;
}

ICACHE_FLASH_ATTR struct servFile* findFile(char* name)
{
	struct servFile* f = (struct servFile*)&indexFile;
	while(1)
	{
//	printf("findFile %s\n",name);	
		if(strcmp(f->name, name) == 0) return f;
		else f = f->next;
		if(f == NULL) return NULL;
	}
}


ICACHE_FLASH_ATTR void serveFile(char* name, int conn)
{
	int length;
	char buf[140];
	char *content;

	struct servFile* f = findFile(name);
	printf ("Heap size: %d\n",xPortGetFreeHeapSize( ));

	if(f != NULL)
	{
		length = f->size;
		content = f->content;
	}
	else length = 0;
//	printf("serveFile %s. Length: %d\n",name,length);	

	if(length > 0)
	{
		char *con = NULL;
		while(con == NULL)
		{
           con = (char*)malloc(length*sizeof(char));
			if ( con == NULL )
			{
				int i = 0;
				do { 
				i++;		
				printf ("Heap size: %d\n",xPortGetFreeHeapSize( ));
				vTaskDelay(10);
				printf("servfile malloc fails for %d\n",length*sizeof(char) );
				}
				while (i<10);
				if (i >=10) {
					sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", (f!=NULL ? f->type : "text/plain"), 0);
					write(conn, buf, strlen(buf));
					return ;
				}
			}	
		} 			
				
		flashRead(con, (uint32_t)content, length);
		
		if(f->cgi == 1) {
			con = serverParseCGI(con, length);
			length = strlen(con);
		}
		sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", (f!=NULL ? f->type : "text/plain"), length);
		write(conn, buf, strlen(buf));
		write(conn, con, length);
		free(con);
	}
	else
	{
		sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", (f!=NULL ? f->type : "text/plain"), 0);
		write(conn, buf, strlen(buf));
	}
}

ICACHE_FLASH_ATTR char* getParameterFromResponse(char* param, char* data, uint16_t data_length) {
	char* p = strstr(data, param);
	if(p > 0) {
		p += strlen(param);
		char* p_end = strstr(p, "&");
		if(p_end <= 0) p_end = data_length + data;
		if(p_end > 0) {
			char* t = malloc(p_end-p + 1);
			if (t == NULL) { printf("getParameterFromResponse malloc fails\n"); return NULL;}
			int i;
			for(i=0; i<(p_end-p + 1); i++) t[i] = 0;
			strncpy(t, p, p_end-p);
			t = str_replace(t, "%2F", "/", strlen(t));
			return t;
		}
	} else return NULL;
}
ICACHE_FLASH_ATTR void respOk(int conn)
{
		char resp[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK";
		write(conn, resp, strlen(resp));
}
ICACHE_FLASH_ATTR void handlePOST(char* name, char* data, int data_size, int conn) {
//	printf("HandlePost %s\n",name);
	if(strcmp(name, "/instant_play") == 0) {
		if(data_size > 0) {
			char* url = getParameterFromResponse("url=", data, data_size);
			char* path = getParameterFromResponse("path=", data, data_size);
			char* port = getParameterFromResponse("port=", data, data_size);
			int i;
			if(url != NULL && path != NULL && port != NULL) {
				clientDisconnect();
				while(clientIsConnected()) vTaskDelay(5);
				clientSetURL(url);
				clientSetPath(path);
				clientSetPort(atoi(port));
				clientConnect();
				for (i = 0;i<200;i++)
				{
					if (clientIsConnected()) break;
					vTaskDelay(5);
				}
//				while(!clientIsConnected()) vTaskDelay(5);
			} 
			if(url) free(url);
			if(path) free(path);
			if(port) free(port);
		}
	} else if(strcmp(name, "/soundvol") == 0) {
		if(data_size > 0) {
			char* vol = getParameterFromResponse("vol=", data, data_size);
//			printf("/sounvol vol: %s num:%d \n",vol, atoi(vol));
			if(vol) {
				VS1053_SetVolume(254-atoi(vol));
				free(vol);
			}
		}
//		respOk(conn);
	} else if(strcmp(name, "/sound") == 0) {
		if(data_size > 0) {
			char* bass = getParameterFromResponse("bass=", data, data_size);
			char* treble = getParameterFromResponse("treble=", data, data_size);
			char* bassfreq = getParameterFromResponse("bassfreq=", data, data_size);
			char* treblefreq = getParameterFromResponse("treblefreq=", data, data_size);
			char* spacial = getParameterFromResponse("spacial=", data, data_size);
			if(bass) {
				VS1053_SetBass(atoi(bass));
				free(bass);
			}
			if(treble) {
				VS1053_SetTreble(atoi(treble));
				free(treble);
			}
			if(bassfreq) {
				VS1053_SetBassFreq(atoi(bassfreq));
				free(bassfreq);
			}
			if(treblefreq) {
				VS1053_SetTrebleFreq(atoi(treblefreq));
				free(treblefreq);
			}
			if(spacial) {
				VS1053_SetSpatial(atoi(spacial));
				free(spacial);
			}
		}
	} else if(strcmp(name, "/getStation") == 0) {
		if(data_size > 0) {
			char* id = getParameterFromResponse("idgp=", data, data_size);
			if(id) {
				char ibuf [6];	
				char *buf;
				int i;
				for(i = 0; i<sizeof(ibuf); i++) ibuf[i] = 0;
				struct shoutcast_info* si;
				si = getStation(atoi(id));
				sprintf(ibuf, "%d", si->port);
				int json_length = strlen(si->domain) + strlen(si->file) + strlen(si->name) + strlen(ibuf) + 40;
				buf = malloc(json_length + 75);
				if (buf == NULL)
				{	
					printf("getStation malloc fails\n");
					respOk(conn);
				}
				else {				
					for(i = 0; i<sizeof(buf); i++) buf[i] = 0;
					sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n{\"Name\":\"%s\",\"URL\":\"%s\",\"File\":\"%s\",\"Port\":\"%d\"}",
						json_length, si->name, si->domain, si->file, si->port);
					write(conn, buf, strlen(buf));
					free(buf);
				}
				free(si);
				free(id);
				return;
			} 
		}
	}/* else if(strcmp(name, "/getSelIndex") == 0) {
				char*  buf = malloc(150);
				int i;
				for(i = 0; i<sizeof(buf); i++) buf[i] = 0;
				sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n{\"Index\":\"%4d\"}", 16,CurId);
				write(conn, buf, strlen(buf));
				free(buf);

	
	}*/ else if(strcmp(name, "/setStation") == 0) {
		if(data_size > 0) {
			char* id = getParameterFromResponse("id=", data, data_size);
			char* url = getParameterFromResponse("url=", data, data_size);
			char* file = getParameterFromResponse("file=", data, data_size);
			char* name = getParameterFromResponse("name=", data, data_size);
			char* port = getParameterFromResponse("port=", data, data_size);
			if(id && url && file && name && port) {
				struct shoutcast_info *si = malloc(sizeof(struct shoutcast_info));
				strcpy(si->domain, url);
				strcpy(si->file, file);
				strcpy(si->name, name);
				si->port = atoi(port);
				saveStation(si, atoi(id));
				free(si);
			} 
			free(id);
			free(url);
			free(file);
			free(name);
			free(port);
		}
	} else if(strcmp(name, "/play") == 0) {
		if(data_size > 0) {
			char* id = getParameterFromResponse("id=", data, data_size);
			if(id != NULL) {
				struct shoutcast_info* si;
				si = getStation(atoi(id));
/*				CurId = atoi(id);
				printf("CurId set: %s\n", id);
*/				if(si != NULL &&si->domain && si->file) {
					int i;
					vTaskDelay(5);
					clientDisconnect();
					while(clientIsConnected()) {vTaskDelay(5);}
					clientSetURL(si->domain);
					clientSetPath(si->file);
					clientSetPort(si->port);
					clientConnect();
					for (i = 0;i<100;i++)
					{
					  if (clientIsConnected()) break;
					  vTaskDelay(4);
					}
//					while(!clientIsConnected()) vTaskDelay(5);
				}
				free(si);
			}
			if(id) free(id);
		}
	} else if(strcmp(name, "/stop") == 0) {
	    int i;
		if (clientIsConnected())
		{	
			clientDisconnect();
			for (i = 0;i<100;i++)
			{
				if (!clientIsConnected()) break;
				vTaskDelay(4);
			}
//			while(clientIsConnected()) vTaskDelay(5);
		}
	}
	respOk(conn);
}

ICACHE_FLASH_ATTR void httpServerHandleConnection(int conn, char* buf, uint16_t buflen) {
	char *c;
//	printf ("Heap size: %d\n",xPortGetFreeHeapSize( ));

	if( (c = strstr(buf, "GET ")) != NULL)
	{
		char fname[32];
		uint8_t i;
		for(i=0; i<32; i++) fname[i] = 0;
		c += 4;
		char* c_end = strstr(c, " ");
		if(c_end == NULL) return;
		uint8_t len = c_end-c;
		if(len > 32) return;
		strncpy(fname, c, len);
		serveFile(fname, conn);
	} else if( (c = strstr(buf, "POST ")) != NULL) {
		char fname[32];
		uint8_t i;
		for(i=0; i<32; i++) fname[i] = 0;
		c += 5;
		char* c_end = strstr(c, " ");
		if(c_end == NULL) return;
		uint8_t len = c_end-c;
		if(len > 32) return;
		strncpy(fname, c, len);
//		printf("Name: %s\n", fname);
		// DATA
		char* d_start = strstr(buf, "\r\n\r\n");
		if(d_start > 0) {
			d_start += 4;
			uint16_t len = buflen - (d_start-buf);
			handlePOST(fname, d_start, len, conn);
		}
	}
}

ICACHE_FLASH_ATTR void serverTask(void *pvParams) {
	struct sockaddr_in server_addr, client_addr;
	int server_sock, client_sock;
	socklen_t sin_size;
	
	while (1) {
        bzero(&server_addr, sizeof(struct sockaddr_in));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(80);
		
        int recbytes;

        do {
            if (-1 == (server_sock = socket(AF_INET, SOCK_STREAM, 0))) {
				printf ("Socket fails %d\n", errno);
//				user_init(); // restart
                break;
            }

            if (-1 == bind(server_sock, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr))) {
				printf ("Bind fails %d\n", errno);
				close(client_sock);
                break;
            }

            if (-1 == listen(server_sock, 5)) {
				printf ("Listen fails %d\n",errno);
				close(client_sock);
                break;
            }

            sin_size = sizeof(client_addr);
			recbytes = 0;
            while(1) {
                if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
					printf ("Accept fails %d\n",errno);
					close(client_sock);
                    break;;
                }
                char *buf = (char *)zalloc(1024);
				if (buf == NULL) {printf("server zalloc fails\n");	break;}

                while ((recbytes = read(client_sock , buf, 1023)) > 0) { // For now we assume max. 1023 bytes for request
//					printf ("Server: received %d bytes, %s\n", recbytes, buf);
					char* bend = strstr(buf, "\r\n\r\n");
					bend += 4;
//					printf("Server: header len : %d\n",bend-buf);
					if ((recbytes == (bend-buf))&& (strstr(buf,"POST"))) //bug socket
					{
						recbytes += read(client_sock , bend, 100);
//						printf ("Server: received more:%d bytes, %s\n", recbytes, bend);
					}
					httpServerHandleConnection(client_sock, buf, recbytes);
                }
                free(buf);

                if (recbytes == 0) {
                    close(client_sock);
                }
                if (recbytes < 0) {
					printf ("Socket read fails %d\n", errno);
                    close(client_sock);
					break;
                }
            }
        } while (0);
    }
}
