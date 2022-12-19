#include <GL/glew.h>
#include "Wad.h"
#include "Texture.h"
#include "lodepng.h"
#include "util.h"
#include "Renderer.h"

Texture::Texture(GLsizei _width, GLsizei _height, const char* name)
{
	this->width = _width;
	this->height = _height;
	this->nearFilter = GL_LINEAR;
	this->farFilter = GL_LINEAR;
	this->data = new unsigned char[(unsigned int)(width * height) * sizeof(COLOR3)];
	this->id = this->format = this->iformat = 0;
	snprintf(texName, 64, "%s", name);
	if (g_settings.verboseLogs)
		logf("Texture: %s %d/%d is loaded.", name, width, height);
	this->transparentMode = IsTextureTransparent(name) ? 1 : 0;
	if (name && name[0] == '{')
	{
		this->transparentMode = 2;
	}
}

Texture::Texture(GLsizei _width, GLsizei _height, unsigned char* data, const char* name)
{
	this->width = _width;
	this->height = _height;
	this->nearFilter = GL_LINEAR;
	this->farFilter = GL_LINEAR;
	this->data = data;
	this->id = this->format = this->iformat = 0;
	snprintf(texName, 64, "%s", name);
	if (g_settings.verboseLogs)
		logf("Texture2 : %s %d/%dis loaded.", name, width, height);
	this->transparentMode = IsTextureTransparent(name) ? 1 : 0;
	if (name && name[0] == '{')
	{
		this->transparentMode = 2;
	}
}

Texture::~Texture()
{
	if (uploaded)
		glDeleteTextures(1, &id);
	delete[] data;
}

void Texture::upload(int _format, bool lightmap)
{
	if (uploaded)
	{
		glDeleteTextures(1, &id);
	}
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id); // Binds this texture handle so we can load the data into it

	// Set up filters and wrap mode
	if (lightmap)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->nearFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, this->nearFilter);
	}

	if (_format == GL_RGB)
	{
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}

	//glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	// TODO: load mipmaps from BSP/WAD

	glTexImage2D(GL_TEXTURE_2D, 0, _format, width, height, 0, _format, GL_UNSIGNED_BYTE, data);

	if (g_settings.verboseLogs)
		logf("Load texture %s with %d/%d size\n", texName, width, height);
	//glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB, width, height);
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);

	uploaded = true;
}

void Texture::bind(GLuint texnum)
{
	glActiveTexture(GL_TEXTURE0 + texnum);
	glBindTexture(GL_TEXTURE_2D, id);
}

bool IsTextureTransparent(const char* texname)
{
	if (!texname)
		return false;
	for (auto const& s : g_settings.transparentTextures)
	{
		if (strcasecmp(s.c_str(), texname) == 0)
			return true;
	}
	return false;
}