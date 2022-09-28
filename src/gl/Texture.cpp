#include <GL/glew.h>
#include "Wad.h"
#include "Texture.h"
#include "lodepng.h"
#include "util.h"

Texture::Texture(GLsizei _width, GLsizei _height) {
	this->width = _width;
	this->height = _height;
	this->nearFilter = GL_LINEAR;
	this->farFilter = GL_LINEAR;
	this->data = new unsigned char[(unsigned int)(width * height) * sizeof(COLOR3)];
	this->id = this->format = this->iformat = 0;
}

Texture::Texture(GLsizei _width, GLsizei _height, unsigned char* data)
{
	this->width = _width;
	this->height = _height;
	this->nearFilter = GL_LINEAR;
	this->farFilter = GL_LINEAR;
	this->data = data;
	this->id = this->format = this->iformat = 0;
}

Texture::~Texture()
{
	if (uploaded)
		glDeleteTextures(1, &id);
	delete[] data;
}

void Texture::upload(int format, bool lightmap)
{
	if (uploaded) {
		return;
		//glDeleteTextures(1, &id);
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->farFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, this->nearFilter);
	}

	if (format == GL_RGB)
	{
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}

	//glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

	// TODO: load mipmaps from BSP/WAD

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);


	//glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB, width, height);
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);

	uploaded = true;
}

GLuint lasttex = 0xFFFFFFFF; 
unsigned int lastid = 0xFFFFFFFF;

void Texture::bind(GLuint texnum)
{
	if (lasttex == texnum && lastid == id)
	{
		return;
	}
	glActiveTexture(GL_TEXTURE0 + texnum);
	glBindTexture(GL_TEXTURE_2D,id);
	//glBindTextureUnit(texnum, id);
	//glBindImageTexture(texnum, id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGB);
	lasttex = texnum;
	lastid = id;
}
