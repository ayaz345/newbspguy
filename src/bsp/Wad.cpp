#include <iostream>
#include <fstream>
#include <string.h>
#include "Wad.h"
#include "util.h"
#include "Renderer.h"

Wad::Wad(void)
{
	dirEntries.clear();
	if (filedata)
		delete[] filedata;
	filedata = NULL;
}

Wad::Wad(const std::string& file)
{
	this->filename = file;
	dirEntries.clear();
	if (filedata)
		delete[] filedata;
	filedata = NULL;
}

Wad::~Wad(void)
{
	dirEntries.clear();
	if (filedata)
		delete[] filedata;
	filedata = NULL;
}

void W_CleanupName(const char* in, char* out)
{
	int			i;
	int			c;

	for (i = 0; i < MAXTEXTURENAME; i++) {
		c = in[i];
		if (!c)
			break;

		if (c >= 'A' && c <= 'Z')
			c += ('a' - 'A');
		out[i] = c;
	}

	for (; i < MAXTEXTURENAME; i++)
		out[i] = 0;
}


bool Wad::readInfo()
{
	std::string file = filename;

	if (!fileExists(file))
	{
		logf("%s does not exist!\n", filename.c_str());
		return false;
	}

	filedata = (unsigned char*)loadFile(file, fileLen);

	if (!filedata)
	{
		logf("%s does not exist!\n", filename.c_str());
		return false;
	}

	if (fileLen < sizeof(WADHEADER))
	{
		delete[] filedata;
		filedata = NULL;
		logf("%s is not wad file[small]!\n", filename.c_str());
		return false;
	}

	int offset = 0;

	memcpy((char*)&header, &filedata[offset], sizeof(WADHEADER));

	if (std::string(header.szMagic).find("WAD3") != 0)
	{
		delete[] filedata;
		filedata = NULL;
		logf("%s is not wad file[invalid header]!\n", filename.c_str());
		return false;
	}

	if (header.nDirOffset >= (int)fileLen)
	{
		delete[] filedata;
		filedata = NULL;
		logf("%s is not wad file[buffer overrun]!\n", filename.c_str());
		return false;
	}

	//
	// WAD DIRECTORY ENTRIES
	//
	offset = header.nDirOffset;

	dirEntries.clear();

	usableTextures = false;

	//logf("D %d %d\n", header.nDirOffset, header.nDir);

	for (int i = 0; i < header.nDir; i++)
	{
		WADDIRENTRY tmpWadEntry = WADDIRENTRY();

		if (offset + sizeof(WADDIRENTRY) > fileLen)
		{
			logf("Unexpected end of WAD\n");
			return false;
		}

		memcpy((char*)&tmpWadEntry, &filedata[offset], sizeof(WADDIRENTRY));
		offset += sizeof(WADDIRENTRY);

		W_CleanupName(tmpWadEntry.szName, tmpWadEntry.szName);

		dirEntries.push_back(tmpWadEntry);

		if (dirEntries[i].nType == 0x43) usableTextures = true;
	}


	if (!usableTextures)
	{
		logf("Info: %s contains no regular textures\n", basename(filename).c_str());
		if (!dirEntries.size())
			return false;
	}

	return true;
}

bool Wad::hasTexture(const std::string& texname)
{
	for (int d = 0; d < header.nDir; d++)
		if (strcasecmp(texname.c_str(), dirEntries[d].szName) == 0)
			return true;
	return false;
}

bool Wad::hasTexture(int dirIndex)
{
	if (dirIndex < 0 || dirIndex >= dirEntries.size())
	{
		return false;
	}
	return true;
}

WADTEX* Wad::readTexture(int dirIndex, int* texturetype)
{
	if (dirIndex < 0 || dirIndex >= dirEntries.size())
	{
		logf("invalid wad directory index\n");
		return NULL;
	}
	//if (cache != NULL)
		//return cache[dirIndex];
	std::string name = std::string(dirEntries[dirIndex].szName);
	return readTexture(name, texturetype);
}

WADTEX* Wad::readTexture(const std::string& texname, int* texturetype)
{
	int idx = -1;
	for (int d = 0; d < header.nDir; d++)
	{
		if (strcasecmp(texname.c_str(), dirEntries[d].szName) == 0)
		{
			idx = d;
			break;
		}
	}

	if (idx < 0)
	{
		return NULL;
	}

	if (dirEntries[idx].bCompression)
	{
		logf("OMG texture is compressed. I'm too scared to load it :<\n");
		return NULL;
	}

	int offset = dirEntries[idx].nFilePos;

	if (texturetype)
	{
		*texturetype = dirEntries[idx].nType;
	}

	BSPMIPTEX mtex = BSPMIPTEX();
	memcpy((char*)&mtex, &filedata[offset], sizeof(BSPMIPTEX));
	offset += sizeof(BSPMIPTEX);
	if (g_settings.verboseLogs)
		logf("Load wad BSPMIPTEX name %s size %d/%d\n", mtex.szName, mtex.nWidth, mtex.nHeight);
	int w = mtex.nWidth;
	int h = mtex.nHeight;
	int sz = w * h;	   // miptex 0
	int sz2 = sz / 4;  // miptex 1
	int sz3 = sz2 / 4; // miptex 2
	int sz4 = sz3 / 4; // miptex 3
	int szAll = sz + sz2 + sz3 + sz4 + 2 + 256 * 3 + 2;

	unsigned char* data = new unsigned char[szAll];

	memcpy(data, &filedata[offset], szAll);

	WADTEX* tex = new WADTEX();
	memcpy(tex->szName, mtex.szName, MAXTEXTURENAME);
	for (int i = 0; i < MIPLEVELS; i++)
		tex->nOffsets[i] = mtex.nOffsets[i];
	tex->nWidth = mtex.nWidth;
	tex->nHeight = mtex.nHeight;
	tex->data = data;
	tex->needclean = true;
	if (g_settings.verboseLogs)
		logf("Return WADTEX name %s size %d/%d\n", tex->szName, tex->nWidth, tex->nHeight);
	return tex;
}

bool Wad::write(WADTEX** textures, size_t numTex)
{
	std::vector<WADTEX*> textList = std::vector<WADTEX*>(&textures[0], &textures[numTex]);
	return write(filename, textList);
}

bool Wad::write(std::vector<WADTEX*> textures)
{
	return write(filename, textures);
}

bool Wad::write(const std::string& _filename, std::vector<WADTEX*> textures)
{
	this->filename = _filename;

	std::ofstream myFile(filename, std::ios::trunc | std::ios::binary);

	header.szMagic[0] = 'W';
	header.szMagic[1] = 'A';
	header.szMagic[2] = 'D';
	header.szMagic[3] = '3';
	header.nDir = (int)textures.size();

	size_t tSize = sizeof(BSPMIPTEX) * textures.size();
	for (size_t i = 0; i < textures.size(); i++)
	{
		int w = textures[i]->nWidth;
		int h = textures[i]->nHeight;
		int sz = w * h;	   // miptex 0
		int sz2 = sz / 4;  // miptex 1
		int sz3 = sz2 / 4; // miptex 2
		int sz4 = sz3 / 4; // miptex 3
		int szAll = sz + sz2 + sz3 + sz4 + 2 + 256 * 3 + 2;
		tSize += szAll;
	}

	if (tSize)
	{
		header.nDirOffset = (int)(sizeof(WADHEADER) + tSize);
		myFile.write((char*)&header, sizeof(WADHEADER));

		for (size_t i = 0; i < textures.size(); i++)
		{
			BSPMIPTEX miptex = BSPMIPTEX();
			memcpy(miptex.szName, textures[i]->szName, MAXTEXTURENAME);

			int w = textures[i]->nWidth;
			int h = textures[i]->nHeight;
			int sz = w * h;	   // miptex 0
			int sz2 = sz / 4;  // miptex 1
			int sz3 = sz2 / 4; // miptex 2
			int sz4 = sz3 / 4; // miptex 3
			int szAll = sz + sz2 + sz3 + sz4 + 2 + 256 * 3 + 2;
			miptex.nWidth = w;
			miptex.nHeight = h;
			miptex.nOffsets[0] = sizeof(BSPMIPTEX);
			miptex.nOffsets[1] = sizeof(BSPMIPTEX) + sz;
			miptex.nOffsets[2] = sizeof(BSPMIPTEX) + sz + sz2;
			miptex.nOffsets[3] = sizeof(BSPMIPTEX) + sz + sz2 + sz3;

			myFile.write((char*)&miptex, sizeof(BSPMIPTEX));
			myFile.write((char*)textures[i]->data, szAll);
		}

		int offset = sizeof(WADHEADER);
		for (size_t i = 0; i < textures.size(); i++)
		{
			WADDIRENTRY entry = WADDIRENTRY();
			entry.nFilePos = offset;
			int w = textures[i]->nWidth;
			int h = textures[i]->nHeight;
			int sz = w * h;	   // miptex 0
			int sz2 = sz / 4;  // miptex 1
			int sz3 = sz2 / 4; // miptex 2
			int sz4 = sz3 / 4; // miptex 3
			int szAll = sz + sz2 + sz3 + sz4 + 2 + 256 * 3 + 2;
			entry.nDiskSize = szAll + sizeof(BSPMIPTEX);
			entry.nSize = szAll + sizeof(BSPMIPTEX);
			entry.nType = 0x43; // Texture
			entry.bCompression = false;
			entry.nDummy = 0;

			for (int k = 0; k < MAXTEXTURENAME; k++)
				memcpy(entry.szName, textures[i]->szName, MAXTEXTURENAME);
			offset += szAll + sizeof(BSPMIPTEX);

			myFile.write((char*)&entry, sizeof(WADDIRENTRY));
		}
	}
	else
	{
		header.nDirOffset = 0;
		myFile.write((char*)&header, sizeof(WADHEADER));
	}

	myFile.close();

	return true;
}

WADTEX* create_wadtex(const char* name, COLOR3* rgbdata, int width, int height)
{
	COLOR3 palette[256];
	memset(&palette, 0, sizeof(COLOR3) * 256);
	unsigned char* mip[MIPLEVELS] = {NULL};

	COLOR3* src = rgbdata;
	int colorCount = 0;

	// create pallete and full-rez mipmap
	mip[0] = new unsigned char[width * height];

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			int paletteIdx = -1;
			for (int k = 0; k < colorCount; k++)
			{
				if (*src == palette[k])
				{
					paletteIdx = k;
					break;
				}
			}
			if (paletteIdx == -1)
			{
				if (colorCount >= 256)
				{
					logf("Too many colors\n");
					delete[] mip[0];
					return NULL;
				}
				palette[colorCount] = *src;
				paletteIdx = colorCount;
				colorCount++;
			}

			mip[0][y * width + x] = (unsigned char)paletteIdx;
			src++;
		}
	}

	int texDataSize = width * height + sizeof(COLOR3) * 256;

	// generate mipmaps
	for (int i = 1; i < MIPLEVELS; i++)
	{
		int div = 1 << i;
		int mipWidth = width / div;
		int mipHeight = height / div;
		texDataSize += mipWidth * height;
		mip[i] = new unsigned char[mipWidth * mipHeight];

		src = rgbdata;
		for (int y = 0; y < mipHeight; y++)
		{
			for (int x = 0; x < mipWidth; x++)
			{
				int paletteIdx = -1;
				for (int k = 0; k < colorCount; k++)
				{
					if (*src == palette[k])
					{
						paletteIdx = k;
						break;
					}
				}

				mip[i][y * mipWidth + x] = (unsigned char)paletteIdx;
				src += div;
			}
		}
	}

	size_t newTexLumpSize = sizeof(BSPMIPTEX) + texDataSize;
	unsigned char* newTexData = new unsigned char[newTexLumpSize];
	memset(newTexData, 0, sizeof(newTexLumpSize));

	WADTEX* newMipTex = new WADTEX();
	newMipTex->nWidth = width;
	newMipTex->nHeight = height;

	memcpy(newMipTex->szName, name, MAXTEXTURENAME);

	newMipTex->nOffsets[0] = sizeof(BSPMIPTEX);
	newMipTex->nOffsets[1] = newMipTex->nOffsets[0] + width * height;
	newMipTex->nOffsets[2] = newMipTex->nOffsets[1] + (width >> 1) * (height >> 1);
	newMipTex->nOffsets[3] = newMipTex->nOffsets[2] + (width >> 2) * (height >> 2);

	unsigned char* palleteOffset = newTexData + newMipTex->nOffsets[3] + (width >> 3) * (height >> 3) + 2;

	memcpy(newTexData + newMipTex->nOffsets[0], mip[0], width * height);
	memcpy(newTexData + newMipTex->nOffsets[1], mip[1], (width >> 1) * (height >> 1));
	memcpy(newTexData + newMipTex->nOffsets[2], mip[2], (width >> 2) * (height >> 2));
	memcpy(newTexData + newMipTex->nOffsets[3], mip[3], (width >> 3) * (height >> 3));
	memcpy(palleteOffset, palette, sizeof(COLOR3) * 256);

	newMipTex->data = newTexData + sizeof(BSPMIPTEX);
	newMipTex->needclean = true;

	return newMipTex;
}

COLOR3* ConvertWadTexToRGB(WADTEX* wadTex)
{
	if (g_settings.verboseLogs)
		logf("Convert WADTEX to RGB name %s size %d/%d\n", wadTex->szName, wadTex->nWidth, wadTex->nHeight);
	int lastMipSize = (wadTex->nWidth / 8) * (wadTex->nHeight / 8);

	COLOR3* palette = (COLOR3*)(wadTex->data + wadTex->nOffsets[3] + lastMipSize + 2 - 40);
	unsigned char* src = wadTex->data;

	COLOR3* imageData = new COLOR3[wadTex->nWidth * wadTex->nHeight];

	int sz = wadTex->nWidth * wadTex->nHeight;

	for (int k = 0; k < sz; k++)
	{
		imageData[k] = palette[src[k]];
	}

	if (g_settings.verboseLogs)
		logf("Converted WADTEX to RGB name %s size %d/%d\n", wadTex->szName, wadTex->nWidth, wadTex->nHeight);
	return imageData;
}
COLOR3* ConvertMipTexToRGB(BSPMIPTEX* tex)
{
	if (g_settings.verboseLogs)
		logf("Convert BSPMIPTEX to RGB name %s size %d/%d\n", tex->szName, tex->nWidth, tex->nHeight);
	int lastMipSize = (tex->nWidth / 8) * (tex->nHeight / 8);

	COLOR3* palette = (COLOR3*)(((unsigned char*)tex) + tex->nOffsets[3] + lastMipSize + 2);
	unsigned char* src = (unsigned char*)(((unsigned char*)tex) + tex->nOffsets[0]);

	COLOR3* imageData = new COLOR3[tex->nWidth * tex->nHeight];

	int sz = tex->nWidth * tex->nHeight;

	for (int k = 0; k < sz; k++)
	{
		imageData[k] = palette[src[k]];
	}

	if (g_settings.verboseLogs)
		logf("Converted BSPMIPTEX to RGB name %s size %d/%d\n", tex->szName, tex->nWidth, tex->nHeight);
	return imageData;
}