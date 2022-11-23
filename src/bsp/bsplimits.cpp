#include "bsplimits.h"


unsigned int MAX_MAP_COORD=32767; // stuff breaks past this point

float FLT_MAX_COORD=32767.f;
float FLT_MIN_COORD=-32767.f;

unsigned int MAX_MAP_MODELS=4096;
unsigned int MAX_MAP_NODES=32768;
unsigned int MAX_MAP_CLIPNODES=32767;
unsigned int MAX_MAP_LEAVES=65536;
unsigned int MAX_MAP_TEXDATA=0;
unsigned int MAX_MAP_VISDATA=64 * ( 1024 * 1024 ); // 64 MB
unsigned int MAX_MAP_ENTS=8192;
unsigned int MAX_MAP_SURFEDGES=512000;
unsigned int MAX_MAP_EDGES=256000;
unsigned int MAX_MAP_TEXTURES=4096;
unsigned int MAX_MAP_LIGHTDATA=64 * ( 1024 * 1024 ); // 64 MB
unsigned int MAX_TEXTURE_DIMENSION=1024;
unsigned int MAX_TEXTURE_SIZE=((MAX_TEXTURE_DIMENSION * MAX_TEXTURE_DIMENSION * 2 * 3) / 2);

unsigned int MAX_KEY_LEN=256; // not sure if this includes the null char
unsigned int MAX_VAL_LEN=4096; // not sure if this includes the null char

void ResetBspLimits()
{
	MAX_MAP_COORD=32767; // stuff breaks past this point (leafs signed short mins/maxs breaks)

	FLT_MAX_COORD=32767.f;
	FLT_MIN_COORD=-32767.f;

	MAX_MAP_MODELS=4096;
	MAX_MAP_NODES=32768;
	MAX_MAP_CLIPNODES=32767;
	MAX_MAP_LEAVES=65536;
	MAX_MAP_TEXDATA=0;
	MAX_MAP_VISDATA=64 * ( 1024 * 1024 ); // 64 MB
	MAX_MAP_ENTS=8192;
	MAX_MAP_SURFEDGES=512000;
	MAX_MAP_EDGES=256000;
	MAX_MAP_TEXTURES=4096;
	MAX_MAP_LIGHTDATA=64 * ( 1024 * 1024 ); // 64 MB
	MAX_TEXTURE_DIMENSION=1024;
	MAX_TEXTURE_SIZE=((MAX_TEXTURE_DIMENSION * MAX_TEXTURE_DIMENSION * 2 * 3) / 2);

	MAX_KEY_LEN=256;
	MAX_VAL_LEN=4096;
}