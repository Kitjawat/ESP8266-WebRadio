#include "webserver.h"
#include "serv-fs.h"

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
		while (i<2);
		if (i >=2) { /*free(string);*/ return oldstr;}
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

ICACHE_FLASH_ATTR struct servFile* findFile(char* name)
{
	struct servFile* f = (struct servFile*)&indexFile;
	while(1)
	{
		if(strcmp(f->name, name) == 0) return f;
		else f = f->next;
		if(f == NULL) return NULL;
	}
}

ICACHE_FLASH_ATTR void serveFile(char* name, int conn)
{
#define PART 2048
	int length;
	int progress,part,gpart;
	char buf[140];
	char *content;
	struct servFile* f = findFile(name);
//	printf ("Heap size: %d\n",xPortGetFreeHeapSize( ));
	gpart = PART;
	if(f != NULL)
	{
		length = f->size;
		content = f->content;
		progress = 0;
	}
	else length = 0;
	if(length > 0)
	{
		char *con = NULL;
		do {
		con = (char*)malloc((gpart+1)*sizeof(char));
		gpart /=2;
		} while ((con == NULL)&&(gpart >=32));
		if(con == NULL)
		{
				sprintf(buf, "HTTP/1.1 500 Internal Server Error\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", (f!=NULL ? f->type : "text/plain"), 0);
				write(conn, buf, strlen(buf));
				printf("serveFile malloc error\n");
				return ;
		}	
//		printf("serveFile socket:%d,  %s. Length: %d  sliced in %d\n",conn,name,length,gpart);		
		sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: keep-alive\r\n\r\n", (f!=NULL ? f->type : "text/plain"), length);
//		sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", (f!=NULL ? f->type : "text/plain"), length);
		write(conn, buf, strlen(buf));

		progress = length;
		part = gpart;
		if (progress <= part) part = progress;

		while (progress > 0) {
			flashRead(con, (uint32_t)content, part);
			write(conn, con, part);
			content += part;
			progress -= part;
			if (progress <= part) part = progress;
		} 

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
			if (strstr(t, "%2F")!=NULL) t = str_replace(t, "%2F", "/", strlen(t)); 
			return t;
		}
	} else return NULL;
}

ICACHE_FLASH_ATTR void respOk(int conn)
{
		char resp[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK";
		write(conn, resp, strlen(resp));
}

// treat the received message
void websockehandle(int socket, wsopcode_t opcode, uint8_t * payload, size_t length)
{
	bool changed = false;
	char* answer;
	struct device_settings *device;
	printf("ws received: %s\n",payload);
	char* vol = getParameterFromResponse("wsvol=", payload, length);
		device = getDeviceSettings();
		changed = false;
		if(vol) {
			VS1053_SetVolume(254-atoi(vol));
			if (device->vol != (254-atoi(vol))){ device->vol = (254-atoi(vol)); changed = true;}
		}
		if (changed) saveDeviceSettings(device);
		answer = malloc(16);
		if (answer != NULL)
		{	
			sprintf(answer,"{\"wsvol\":\"%s\"}",vol);
			websocketlimitedbroadcast(socket,answer, strlen(answer));
			free(answer);
		} else printf("ws malloc fails\n");
		if (vol) free(vol);
		free(device);	
}
ICACHE_FLASH_ATTR void playStation(char* id) {
	struct shoutcast_info* si;
	si = getStation(atoi(id));
	if(si != NULL &&si->domain && si->file) {
		int i;
		vTaskDelay(5);
		clientDisconnect();
		while(clientIsConnected()) {vTaskDelay(5);}
		clientSetURL(si->domain);
		clientSetPath(si->file);
		clientSetPort(si->port);
		clientConnect();
		for (i = 0;i<50;i++)
		{
		  if (clientIsConnected()) break;
		  vTaskDelay(4);
		}
	}
	free(si);
}

ICACHE_FLASH_ATTR void handlePOST(char* name, char* data, int data_size, int conn) {
//	printf("HandlePost %s\n",name);
	char* head = NULL;
	bool changed = false;
	struct device_settings *device;
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
				for (i = 0;i<50;i++)
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
			device = getDeviceSettings();
			changed = false;
			if(vol) {
				VS1053_SetVolume(254-atoi(vol));
				if (device->vol != (254-atoi(vol))){ device->vol = (254-atoi(vol)); changed = true;}
				free(vol);
			}
			if (changed) saveDeviceSettings(device);
			free(device);		}
	} else if(strcmp(name, "/sound") == 0) {
		if(data_size > 0) {
			char* bass = getParameterFromResponse("bass=", data, data_size);
			char* treble = getParameterFromResponse("treble=", data, data_size);
			char* bassfreq = getParameterFromResponse("bassfreq=", data, data_size);
			char* treblefreq = getParameterFromResponse("treblefreq=", data, data_size);
			char* spacial = getParameterFromResponse("spacial=", data, data_size);
			device = getDeviceSettings();
			changed = false;
			if(bass) {
				VS1053_SetBass(atoi(bass));
				if (device->bass != atoi(bass)){ device->bass = atoi(bass); changed = true;}
				free(bass);
			}
			if(treble) {
				VS1053_SetTreble(atoi(treble));
				if (device->treble != atoi(treble)){ device->treble = atoi(treble); changed = true;}
				free(treble);
			}
			if(bassfreq) {
				VS1053_SetBassFreq(atoi(bassfreq));
				if (device->freqbass != atoi(bassfreq)){ device->freqbass = atoi(bassfreq); changed = true;}
				free(bassfreq);
			}
			if(treblefreq) {
				VS1053_SetTrebleFreq(atoi(treblefreq));
				if (device->freqtreble != atoi(treblefreq)){ device->freqtreble = atoi(treblefreq); changed = true;}
				free(treblefreq);
			}
			if(spacial) {
				VS1053_SetSpatial(atoi(spacial));
				if (device->spacial != atoi(spacial)){ device->spacial = atoi(spacial); changed = true;}
				free(spacial);
			}
			if (changed) saveDeviceSettings(device);
			free(device);
		}
	} else if(strcmp(name, "/getStation") == 0) {
		if(data_size > 0) {
			char* id = getParameterFromResponse("idgp=", data, data_size);
			if ((id) && (atoi(id) >=0) && (atoi(id) < 192)) {
				char ibuf [6];	
				char *buf;
				int i;
				for(i = 0; i<sizeof(ibuf); i++) ibuf[i] = 0;
				struct shoutcast_info* si;
				si = getStation(atoi(id));
				if (strlen(si->domain) > sizeof(si->domain)) si->domain[sizeof(si->domain)-1] = 0; //truncate if any (rom crash)
				if (strlen(si->file) > sizeof(si->file)) si->file[sizeof(si->file)-1] = 0; //truncate if any (rom crash)
				if (strlen(si->name) > sizeof(si->name)) si->name[sizeof(si->name)-1] = 0; //truncate if any (rom crash)
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
			} else printf("getstation, no id or Wrong id %d\n",atoi(id));
		}
	} else if(strcmp(name, "/setStation") == 0) {
		if(data_size > 0) {
			char* id = getParameterFromResponse("id=", data, data_size);
			char* url = getParameterFromResponse("url=", data, data_size);
			char* file = getParameterFromResponse("file=", data, data_size);
			char* name = getParameterFromResponse("name=", data, data_size);
			char* port = getParameterFromResponse("port=", data, data_size);
			if(id && url && file && name && port) {
				struct shoutcast_info *si = malloc(sizeof(struct shoutcast_info));
				if ((si != NULL) && (atoi(id) >=0) && (atoi(id) < 192))
				{	
					char* bsi = (char*)si;
					int i; for (i=0;i< sizeof(struct shoutcast_info);i++) bsi[i]=0; //clean 
					if (strlen(url) > sizeof(si->domain)) url[sizeof(si->domain)-1] = 0; //truncate if any
					strcpy(si->domain, url);
					if (strlen(file) > sizeof(si->file)) url[sizeof(si->file)-1] = 0; //truncate if any
					strcpy(si->file, file);
					if (strlen(name) > sizeof(si->name)) url[sizeof(si->name)-1] = 0; //truncate if any
					strcpy(si->name, name);
					si->port = atoi(port);
					saveStation(si, atoi(id));
					free(si);
				} else printf("setStation SI malloc failed or illegal id %d\n",atoi(id));
			} 
			free(port);
			free(name);
			free(file);
			free(url);
			free(id);
		}
	} else if(strcmp(name, "/play") == 0) {
		if(data_size > 0) {
			char* id = getParameterFromResponse("id=", data, data_size);
			if(id != NULL) {
				playStation(id);
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
	} else if(strcmp(name, "/icy") == 0)	
	{	
//		printf("icy vol \n");
		char vol[5]; sprintf(vol,"%d",(254-VS1053_GetVolume()));
		char treble[5]; sprintf(treble,"%d",VS1053_GetTreble());
		char bass[5]; sprintf(bass,"%d",VS1053_GetBass());
		char tfreq[5]; sprintf(tfreq,"%d",VS1053_GetTrebleFreq());
		char bfreq[5]; sprintf(bfreq,"%d",VS1053_GetBassFreq());
		char spac[5]; sprintf(spac,"%d",VS1053_GetSpatial());
		
		struct icyHeader *header = clientGetHeader();
//		printf("icy start header %x\n",header);
		char* not2;
		not2 = header->members.single.notice2;
		if (not2 ==NULL) not2=header->members.single.audioinfo;
		if ((header->members.single.notice2 != NULL)&(strlen(header->members.single.notice2)==0)) not2=header->members.single.audioinfo;
		int json_length ;
		json_length =144+
		((header->members.single.description ==NULL)?0:strlen(header->members.single.description)) +
		((header->members.single.name ==NULL)?0:strlen(header->members.single.name)) +
		((header->members.single.bitrate ==NULL)?0:strlen(header->members.single.bitrate)) +
		((header->members.single.url ==NULL)?0:strlen(header->members.single.url))+ 
		((header->members.single.notice1 ==NULL)?0:strlen(header->members.single.notice1))+
		((not2 ==NULL)?0:strlen(not2))+
		((header->members.single.genre ==NULL)?0:strlen(header->members.single.genre))+
		((header->members.single.metadata ==NULL)?0:strlen(header->members.single.metadata))
		+	strlen(vol) +strlen(treble)+strlen(bass)+strlen(tfreq)+strlen(bfreq)+strlen(spac)
		;
//		printf("icy start header %x  len:%d vollen:%d vol:%s\n",header,json_length,strlen(vol),vol);
		
		char *buf = malloc( json_length + 75);
		if (buf == NULL)
		{	
			printf("post icy malloc fails\n");
			respOk(conn);
		}
		else {				
			sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type:application/json\r\nContent-Length:%d\r\n\r\n{\"descr\":\"%s\",\"name\":\"%s\",\"bitr\":\"%s\",\"url1\":\"%s\",\"not1\":\"%s\",\"not2\":\"%s\",\"genre\":\"%s\",\"meta\":\"%s\",\"vol\":\"%s\",\"treb\":\"%s\",\"bass\":\"%s\",\"tfreq\":\"%s\",\"bfreq\":\"%s\",\"spac\":\"%s\"}",
			json_length,
			(header->members.single.description ==NULL)?"":header->members.single.description,
			(header->members.single.name ==NULL)?"":header->members.single.name,
			(header->members.single.bitrate ==NULL)?"":header->members.single.bitrate,
			(header->members.single.url ==NULL)?"":header->members.single.url,
			(header->members.single.notice1 ==NULL)?"":header->members.single.notice1,
			(not2 ==NULL)?"":not2 ,
			(header->members.single.genre ==NULL)?"":header->members.single.genre,
			(header->members.single.metadata ==NULL)?"":header->members.single.metadata,			
			vol,treble,bass,tfreq,bfreq,spac);
//			printf("buf: %s\n",buf);
			write(conn, buf, strlen(buf));
			free(buf);
		}
		return;
	} else if(strcmp(name, "/wifi") == 0)	
	{
		bool val = false;
		char tmpip[16],tmpmsk[16],tmpgw[16];
		struct device_settings *device;
		device = getDeviceSettings();
		
		uint8_t a,b,c,d;
				
		if(data_size > 0) {
			char* valid = getParameterFromResponse("valid=", data, data_size);
			if(valid != NULL) if (strcmp(valid,"1")==0) val = true;
			char* ssid = getParameterFromResponse("ssid=", data, data_size);
			char* pasw = getParameterFromResponse("pasw=", data, data_size);
			char* aip = getParameterFromResponse("ip=", data, data_size);
			char* amsk = getParameterFromResponse("msk=", data, data_size);
			char* agw = getParameterFromResponse("gw=", data, data_size);
			char* adhcp = getParameterFromResponse("dhcp=", data, data_size);
//			printf("wifi received  valid:%s,val:%d, ssid:%s, pasw:%s, aip:%s, amsk:%s, agw:%s, adhcp:%s \n",valid,val,ssid,pasw,aip,amsk,agw,adhcp);
			if (val) {
				ip_addr_t val;
				ipaddr_aton(aip, &val);
				memcpy(device->ipAddr,&val,sizeof(uint32_t));
				ipaddr_aton(amsk, &val);
				memcpy(device->mask,&val,sizeof(uint32_t));
				ipaddr_aton(agw, &val);
				memcpy(device->gate,&val,sizeof(uint32_t));
				if (adhcp!= NULL) if (strlen(adhcp)!=0) if (strcmp(adhcp,"true")==0)device->dhcpEn = 1; else device->dhcpEn = 0;
				strcpy(device->ssid,(ssid==NULL)?"":ssid);
				strcpy(device->pass,(pasw==NULL)?"":pasw);
				saveDeviceSettings(device);		
			}
			device = getDeviceSettings();
			int json_length ;
			json_length =56+
			strlen(device->ssid) +
			strlen(device->pass) +
			sprintf(tmpip,"%d.%d.%d.%d",device->ipAddr[0], device->ipAddr[1],device->ipAddr[2], device->ipAddr[3])+
			sprintf(tmpmsk,"%d.%d.%d.%d",device->mask[0], device->mask[1],device->mask[2], device->mask[3])+
			sprintf(tmpgw,"%d.%d.%d.%d",device->gate[0], device->gate[1],device->gate[2], device->gate[3])+
			sprintf(adhcp,"%d",device->dhcpEn);
//			printf("wifi3 received  ssid:%s, pasw:%s, aip:%s, amsk:%s, agw:%s, adhcp:%s \n",ssid,pasw,aip,amsk,agw,adhcp);

			char *buf = malloc( json_length + 75);			
			if (buf == NULL)
			{	
				printf("post wifi malloc fails\n");
				respOk(conn);
			}
			else {				
				sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type:application/json\r\nContent-Length:%d\r\n\r\n{\"ssid\":\"%s\",\"pasw\":\"%s\",\"ip\":\"%s\",\"msk\":\"%s\",\"gw\":\"%s\",\"dhcp\":\"%s\"}",
				json_length,
				device->ssid,device->pass,tmpip,tmpmsk,tmpgw,adhcp	);
//				printf("wifi Buf: %s\n",buf);
				write(conn, buf, strlen(buf));
				free(buf);
			}
			if (ssid) free(ssid); if (pasw) free(pasw); if (aip) free(aip);if (amsk) free(amsk);if (agw) free(agw);
			if (valid) free(valid);if (adhcp) free(adhcp);
		}	
		free(device);
		if (val){
		vTaskDelay(200);		
		system_restart_enhance(SYS_BOOT_NORMAL_BIN, system_get_userbin_addr());	
		}	
		
		return;
	} else if(strcmp(name, "/clear") == 0)	
	{
		eeEraseStations();	//clear all stations
	}
	respOk(conn);
}

ICACHE_FLASH_ATTR bool httpServerHandleConnection(int conn, char* buf, uint16_t buflen) {
	char* c;
	char* d;
//	printf ("Heap size: %d\n",xPortGetFreeHeapSize( ));
	if( (c = strstr(buf, "GET ")) != NULL)
	{
//		printf("GET socket:%d ",conn);
		if( ((d = strstr(buf,"Connection:")) !=NULL)&& ((d = strstr(d," Upgrade")) != NULL))
		{  // a websocket request
			// prepare the parameter of the websocket task
			printf("websocket request\n");
			struct websocketparam* pvParams = malloc(sizeof(struct websocketparam));
			char* pbuf = malloc(buflen+1);
			if (pbuf == NULL)
			{
				char resp[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
				write(conn, resp, strlen(resp));
				return true;
			}	
			memcpy(pbuf,buf,buflen);
			pvParams->socket = conn;
			pvParams->buf = pbuf;
			pvParams->len = buflen;
			printf("GET websocket\n");
			if (xTaskCreate( websocketTask,"t11",512,(void *) pvParams,2, NULL )!= pdPASS) close (conn);			
			return false;
		} else{
			char fname[32];
			uint8_t i;
			for(i=0; i<32; i++) fname[i] = 0;
			c += 4;
			char* c_end = strstr(c, " ");
			if(c_end == NULL) return;
			uint8_t len = c_end-c;
			if(len > 32) return;
			strncpy(fname, c, len);
//			printf("GET in  socket:%d file:%s\n",conn,fname);
			serveFile(fname, conn);
//			printf("GET end socket:%d file:%s\n",conn,fname);
		}
	} else if( (c = strstr(buf, "POST ")) != NULL) {
//		printf("POST socket: %d\n",conn);
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
	return true;
}



	xSemaphoreHandle semclient = NULL ;
ICACHE_FLASH_ATTR void serverclientTask(void *pvParams) {

	struct timeval timeout;      
    timeout.tv_sec = 5000; // bug *1000 for seconds
    timeout.tv_usec = 0;
	int recbytes =0;
	int  client_sock =  (int)pvParams;
    char *buf = (char *)zalloc(1024);
	bool result = true;
//	printf("Client entry  socket:%x \n",client_sock);
	if (buf != NULL)
	{
		if (setsockopt (client_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
				printf("setsockopt failed\n");
		while (((recbytes = read(client_sock , buf, 1023)) > 0)) { // For now we assume max. 1023 bytes for request
			if (recbytes < 0) {
				if (errno != EAGAIN )
				{
					printf ("Socket %d read fails %d\n",client_sock, errno);
					vTaskDelay(10);	
					break;
				} else printf("try again\n");
			}	
			char* bend = NULL;
			do {
				bend = strstr(buf, "\r\n\r\n");
				if (bend != NULL) 
				{	
					bend += 4;
//					printf("Server: header len : %d\n",bend - buf);
					if ((recbytes == (bend-buf))  && (strstr(buf,"POST")) ) //rest of post
					{
//						printf ("Server: received more:%d bytes, %s\n", recbytes, bend);
						recbytes += read(client_sock , bend, 100);
					}
				} 
				else { 
//					printf ("Server: received more before end %d bytes, %s\n", recbytes, buf);
					vTaskDelay(1);
					recbytes += read(client_sock , buf+recbytes, 1023-recbytes);
				} //until "\r\n\r\n"
			} while (bend == NULL);
			result = httpServerHandleConnection(client_sock, buf, recbytes);
			if (!result) 
			{
				break;
			}	
		}
		free(buf);
	}
	if (result)
	{
//		shutdown(client_sock,SHUT_RDWR);
//		vTaskDelay(10);
		close(client_sock);
	}
	xSemaphoreGive(semclient);	
//	printf("Client exit socket:%d result %d \n",client_sock,result);
	vTaskDelete( NULL );	
}	
ICACHE_FLASH_ATTR void serverTask(void *pvParams) {
	struct sockaddr_in server_addr, client_addr;
	int server_sock, client_sock;
	portBASE_TYPE xReturned;
	socklen_t sin_size;
    semclient = xSemaphoreCreateCounting( 6,6); 
	websocketinit();
	
	while (1) {
        bzero(&server_addr, sizeof(struct sockaddr_in));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(80);

        do {		
            if (-1 == (server_sock = socket(AF_INET, SOCK_STREAM, 0))) {
				printf ("Socket fails %d\n", errno);
				vTaskDelay(10);	
                break;
            }

            if (-1 == bind(server_sock, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr))) {
				printf ("Bind fails %d\n", errno);
				close(server_sock);
				vTaskDelay(100);	
                break;
            }

            if (-1 == listen(server_sock, 5)) {
				printf ("Listen fails %d\n",errno);
				close(server_sock);
				vTaskDelay(100);	
                break;
            }

            sin_size = sizeof(client_addr);
            while(1) {
                if ((client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
					printf ("Accept fails %d\n",errno);
					vTaskDelay(100);					
                } else
				{
					while (1) 
					{
//						printf ("Accept socket %d\n",client_sock);
						if (xSemaphoreTake(semclient,400)){ 
							xReturned = xTaskCreate( serverclientTask,
							"t10",
							400,
							(void *) client_sock,
							2, 
							NULL );
							if (xReturned != pdPASS) 
							{ char buf[120];
								sprintf(buf, "HTTP/1.1 429 	Too Many Requests\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n");
								write(client_sock, buf, strlen(buf));
								xSemaphoreGive(semclient);
								printf("xTaskCreate failed %d\n",xReturned);
								close (client_sock);
							} 
							else break;
						} else {vTaskDelay(10);printf("no room for client\n");}
					}
				}			
            }
        } while (0);
    }
}
