#include "c_types.h"

//#define CACHE_FLASH __attribute__((section(".irom0.rodata")))

struct servFile
{
	const char name[32];
	const char type[16];
	uint8_t cgi;
	uint16_t size;
	char* content;
	struct servFile *next;
};

//ICACHE_STORE_ATTR ICACHE_RODATA_ATTR

#define ICACHE_STORE_TYPEDEF_ATTR __attribute__((aligned(4),packed))
#define ICACHE_STORE_ATTR __attribute__((aligned(4)))
#define ICACHE_RAM_ATTR __attribute__((section(".iram0.text")))

#include "../../webpage/index"
#include "../../webpage/style"
#include "../../webpage/script"
#include "../../webpage/logo"

const struct servFile stationsFile = {
	"/stations",
	"application/json",
	2, // internally generated
	1, // not 0 unknown now
	0,
	NULL
};
const struct servFile logoFile = {
	"/logo.png",
	"image/png",
	0,
	sizeof(logo_png),
	logo_png,
	(struct servFile*)&stationsFile
};

const struct servFile scriptFile = {
	"/script.js",
	"text/javascript",
	0,
	sizeof(script_js),
	script_js,
	(struct servFile*)&logoFile
};

const struct servFile styleFile = {
	"/style.css",
	"text/css",
	0,
	sizeof(style_css),
	style_css,
	(struct servFile*)&scriptFile
};

const struct servFile indexFile = {
	"/",
	"text/html",
	1,
	sizeof(index_html),
	index_html,
	(struct servFile*)&styleFile
};
