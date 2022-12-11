#pragma once
#include <string>
#include "bsplimits.h"
#include "bsptypes.h"

#pragma pack(push, 1)

COLOR3 operator*(COLOR3 v, float f);
bool operator==(COLOR3 c1, COLOR3 c2);

COLOR4 operator*(COLOR4 v, float f);
bool operator==(COLOR4 c1, COLOR4 c2);

struct WADHEADER
{
	char szMagic[4];    // should be WAD2/WAD3
	int nDir;			// number of directory entries
	int nDirOffset;		// offset into directories
};

struct WADDIRENTRY
{
	int nFilePos;				 // offset in WAD
	int nDiskSize;				 // size in file
	int nSize;					 // uncompressed size
	char nType;					 // type of entry
	bool bCompression;           // 0 if none
	short nDummy;				 // not used
	char szName[MAXTEXTURENAME]; // must be null terminated
};

struct WADTEX
{
	char szName[MAXTEXTURENAME];
	unsigned int nWidth, nHeight;
	unsigned int nOffsets[MIPLEVELS];
	unsigned char* data; // all mip-maps and pallete
	WADTEX()
	{
		szName[0] = '\0';
		data = NULL;
		nWidth = nHeight = 0;
		nOffsets[0] = nOffsets[1] = nOffsets[2] = nOffsets[3] = 0;
	}
	WADTEX(BSPMIPTEX* tex)
	{
		snprintf(szName, MAXTEXTURENAME, "%s", tex->szName);

		nWidth = tex->nWidth;
		nHeight = tex->nHeight;
		for (int i = 0; i < MIPLEVELS; i++)
			nOffsets[i] = tex->nOffsets[i];
		data = (unsigned char*)(((unsigned char*)tex) + tex->nOffsets[0]);
	}
};

class Wad
{
public:
	std::string filename = std::string();

	unsigned char* filedata = NULL;
	int fileLen = 0;

	WADHEADER header = WADHEADER();

	std::vector<WADDIRENTRY> dirEntries = std::vector<WADDIRENTRY>();

	Wad(const std::string& file);
	Wad(void);

	~Wad(void);

	bool readInfo(bool allowempty = false);

	bool hasTexture(int dirIndex);
	bool hasTexture(const std::string& name);

	bool write(const std::string& filename, std::vector<WADTEX*> textures);
	bool write(WADTEX** textures, size_t numTex);
	bool write(std::vector<WADTEX*> textures);

	WADTEX* readTexture(int dirIndex);
	WADTEX* readTexture(const std::string& texname);
};

WADTEX* create_wadtex(const char* name, COLOR3* data, int width, int height);
COLOR3* ConvertWadTexToRGB(WADTEX* wadTex);

#pragma pack(pop)