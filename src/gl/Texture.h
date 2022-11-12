#pragma once
#include <GL/glew.h>
#include "util.h"

class Texture
{
public:
	unsigned int id; // OpenGL texture ID
	GLsizei height, width;
	int nearFilter;
	int farFilter;
	unsigned int format; // format of the data
	unsigned int iformat; // format of the data when uploaded to GL
	char texName[64];

	Texture(GLsizei width, GLsizei height, const char * name);
	Texture(GLsizei width, GLsizei height, unsigned char* data, const char* name);
	~Texture();

	// upload the texture with the specified settings
	void upload(int format, bool lightmap = false);

	// use this texture for rendering
	void bind(GLuint texnum);

	unsigned char* data; // RGB(A) data

	bool uploaded = false;
};