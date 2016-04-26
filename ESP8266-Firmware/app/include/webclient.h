#include "c_types.h"

#define ICY_HEADERS_COUNT 9
struct icyHeader
{
	union
	{
		struct
		{
			char* name;
			char* notice1;
			char* notice2;
			char* url;
			char* genre;
			char* bitrate;
			char* description;
			char* audioinfo;
			char* metadata;
			int metaint;
		} single;
		char* mArr[ICY_HEADERS_COUNT];
	} members;
};

enum clientStatus { C_HEADER, C_HEADER1,C_METADATA, C_DATA, C_PLAYLIST, C_PLAYLIST1 };

void clientInit();
uint8_t clientIsConnected();

void clientSetURL(char* url);
void clientSetPath(char* path);
void clientSetPort(uint16_t port);
struct icyHeader* clientGetHeader();
void clientConnect();
void clientDisconnect();
void clientTask(void *pvParams);
void vsTask(void *pvParams) ;
