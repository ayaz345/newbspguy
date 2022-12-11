#include "util.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <string.h>
#include "Wad.h"
#include <cfloat> 
#include <stdarg.h>
#ifdef WIN32
#include <Windows.h>
#include <Shlobj.h>
#else 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <stdio.h>
#ifdef WIN32
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif
#include <set>

bool DebugKeyPressed = false;
unsigned int g_frame_counter = 0;
double g_time = 0.0;

ProgressMeter g_progress;
int g_render_flags;
std::vector<std::string> g_log_buffer;
std::mutex g_log_mutex;

static char log_line[4096];

void logf(const char* format, ...)
{
	g_log_mutex.lock();

	va_list vl;
	va_start(vl, format);
	vsnprintf(log_line, 4096, format, vl);
	va_end(vl);

	printf("%s", log_line);
	g_log_buffer.push_back(log_line);

#ifndef NDEBUG
	std::ofstream outfile("log.txt", std::ios_base::app);
	outfile << log_line;
#endif

	g_log_mutex.unlock();
}

void debugf(const char* format, ...)
{
	if (!g_verbose)
	{
		return;
	}

	g_log_mutex.lock();

	va_list vl;
	va_start(vl, format);
	vsnprintf(log_line, 4096, format, vl);
	va_end(vl);

	printf("%s", log_line);
	g_log_buffer.push_back(log_line);

	g_log_mutex.unlock();
}

bool fileExists(const std::string& fileName)
{
	return fs::exists(fileName) && !fs::is_directory(fileName);
}

char* loadFile(const std::string& fileName, int& length)
{
	if (!fileExists(fileName))
		return NULL;
	std::ifstream fin(fileName.c_str(), std::ios::binary);
	long long begin = fin.tellg();
	fin.seekg(0, std::ios::end);
	unsigned int size = (unsigned int)((int)fin.tellg() - begin);
	char* buffer = new char[size];
	fin.seekg(0, std::ios::beg);
	fin.read(buffer, size);
	fin.close();
	length = (int)size; // surely models will never exceed 2 GB
	return buffer;
}

bool writeFile(const std::string& fileName, const char* data, int len)
{
	std::ofstream file(fileName, std::ios::trunc | std::ios::binary);
	if (!file.is_open() || len <= 0)
	{
		return false;
	}
	file.write(data, len);
	return true;
}

bool writeFile(const std::string& fileName, const std::string& data)
{
	std::ofstream file(fileName, std::ios::trunc | std::ios::binary);
	if (!file.is_open() || !data.size())
	{
		return false;
	}
	file.write(data.c_str(), strlen(data));
	return true;
}


bool removeFile(const std::string& fileName)
{
	return fs::exists(fileName) && fs::remove(fileName);
}

void copyFile(const std::string& fileName, const std::string& fileName2)
{
	if (fileExists(fileName2))
		return;
	if (!fileExists(fileName))
		return;
	int length = 0;
	char* oldFileData = loadFile(fileName, length);
	writeFile(fileName2, oldFileData, length);
	delete[] oldFileData;
}

std::streampos fileSize(const std::string& filePath)
{

	std::streampos fsize = 0;
	std::ifstream file(filePath, std::ios::binary);

	fsize = file.tellg();
	file.seekg(0, std::ios::end);
	fsize = file.tellg() - fsize;
	file.close();

	return fsize;
}

std::vector<std::string> splitString(std::string s, const std::string& delimitter)
{
	std::vector<std::string> split;
	if (s.empty() || delimitter.empty())
		return split;

	size_t delimitLen = delimitter.length();

	while (s.size())
	{
		size_t searchOffset = 0;
		while (searchOffset < s.size())
		{
			size_t delimitPos = s.find(delimitter, searchOffset);

			if (delimitPos == std::string::npos)
			{
				split.push_back(s);
				return split;
			}

			if (delimitPos != 0)
				split.emplace_back(s.substr(0, delimitPos));

			s = s.substr(delimitPos + delimitLen);
		}
	}

	return split;
}

std::vector<std::string> splitStringIgnoringQuotes(std::string s, const std::string& delimitter)
{
	std::vector<std::string> split;
	if (s.empty() || delimitter.empty())
		return split;

	size_t delimitLen = delimitter.length();

	while (s.size())
	{
		bool foundUnquotedDelimitter = false;
		size_t searchOffset = 0;
		while (!foundUnquotedDelimitter && searchOffset < s.size())
		{
			size_t delimitPos = s.find(delimitter, searchOffset);

			if (delimitPos == std::string::npos || delimitPos > s.size())
			{
				split.push_back(s);
				return split;
			}
			size_t quoteCount = 0;
			for (int i = 0; i < delimitPos; i++)
			{
				quoteCount += s[i] == '"';
			}

			if (quoteCount % 2 == 1)
			{
				searchOffset = delimitPos + 1;
				continue;
			}
			if (delimitPos != 0)
				split.emplace_back(s.substr(0, delimitPos));

			s = s.substr(delimitPos + delimitLen);
			foundUnquotedDelimitter = true;
		}

		if (!foundUnquotedDelimitter)
		{
			break;
		}

	}

	return split;
}


std::string basename(const std::string& path)
{
	size_t lastSlash = path.find_last_of("\\/");
	if (lastSlash != std::string::npos)
	{
		return path.substr(lastSlash + 1);
	}
	return path;
}

std::string stripExt(const std::string& path)
{
	size_t lastDot = path.find_last_of('.');
	if (lastDot != std::string::npos)
	{
		return path.substr(0, lastDot);
	}
	return path;
}

bool isNumeric(const std::string& s)
{
	std::string::const_iterator it = s.begin();

	while (it != s.end() && isdigit(*it))
		++it;

	return !s.empty() && it == s.end();
}

std::string toLowerCase(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
				   [](unsigned char c){ return (unsigned char)std::tolower(c); }
	);
	return s;
}

std::string trimSpaces(std::string s)
{
	// Remove white space indents
	size_t lineStart = s.find_first_not_of(" \t\n\r");
	if (lineStart == std::string::npos)
		return "";

	// Remove spaces after the last character
	size_t lineEnd = s.find_last_not_of(" \t\n\r");
	if (lineEnd != std::string::npos && lineEnd < s.length() - 1)
		s = s.substr(lineStart, (lineEnd + 1) - lineStart);
	else
		s = s.substr(lineStart);

	return s;
}

int getBspTextureSize(BSPMIPTEX* bspTexture)
{
	int sz = sizeof(BSPMIPTEX);
	if (bspTexture->nOffsets[0] > 0)
	{
		sz += 256 * 3 + 4; // pallette + padding

		for (int i = 0; i < MIPLEVELS; i++)
		{
			sz += (bspTexture->nWidth >> i) * (bspTexture->nHeight >> i);
		}
	}
	return sz;
}

float clamp(float val, float min, float max)
{
	if (val > max)
	{
		return max;
	}
	else if (val < min)
	{
		return min;
	}
	return val;
}

vec3 parseVector(const std::string& s)
{
	vec3 v;
	std::vector<std::string> parts = splitString(s, " ");

	if (parts.size() != 3)
	{
		logf("Not enough coordinates in std::vector %s. Size:%u\n", s.c_str(), (unsigned int)parts.size());
		return v;
	}

	v.x = (float)atof(parts[0].c_str());
	v.y = (float)atof(parts[1].c_str());
	v.z = (float)atof(parts[2].c_str());

	return v;
}

bool IsEntNotSupportAngles(std::string& entname)
{
	if (entname == "func_wall" ||
		entname == "func_wall_toggle" ||
		entname == "func_illusionary" ||
		entname == "spark_shower" ||
		entname == "func_plat" ||
		entname == "func_door" ||
		entname == "momentary_door" ||
		entname == "func_water" ||
		entname == "func_conveyor" ||
		entname == "func_rot_button" ||
		entname == "func_button" ||
		entname == "env_blood" ||
		entname == "gibshooter" ||
		entname == "trigger" ||
		entname == "trigger_monsterjump" ||
		entname == "trigger_hurt" ||
		entname == "trigger_multiple" ||
		entname == "trigger_push" ||
		entname == "trigger_teleport" ||
		entname == "func_bomb_target" ||
		entname == "func_hostage_rescue" ||
		entname == "func_vip_safetyzone" ||
		entname == "func_escapezone" ||
		entname == "trigger_autosave" ||
		entname == "trigger_endsection" ||
		entname == "trigger_gravity" ||
		entname == "env_snow" ||
		entname == "func_snow" ||
		entname == "env_rain" ||
		entname == "func_rain")
		return true;
	return false;
}

COLOR3 operator*(COLOR3 c, float scale)
{
	c.r = (unsigned char)(c.r * scale);
	c.g = (unsigned char)(c.g * scale);
	c.b = (unsigned char)(c.b * scale);
	return c;
}

bool operator==(COLOR3 c1, COLOR3 c2)
{
	return c1.r == c2.r && c1.g == c2.g && c1.b == c2.b;
}

COLOR4 operator*(COLOR4 c, float scale)
{
	c.r = (unsigned char)(c.r * scale);
	c.g = (unsigned char)(c.g * scale);
	c.b = (unsigned char)(c.b * scale);
	return c;
}

bool operator==(COLOR4 c1, COLOR4 c2)
{
	return c1.r == c2.r && c1.g == c2.g && c1.b == c2.b && c1.a == c2.a;
}

bool pickAABB(vec3 start, vec3 rayDir, vec3 mins, vec3 maxs, float& bestDist)
{
/*
Fast Ray-Box Intersection
by Andrew Woo
from "Graphics Gems", Academic Press, 1990
https://web.archive.org/web/20090803054252/http://tog.acm.org/resources/GraphicsGems/gems/RayBox.c
*/

	bool inside = true;
	char quadrant[3];
	int i;
	int whichPlane;
	float maxT[3];
	float candidatePlane[3];

	float* origin = (float*)&start;
	float* dir = (float*)&rayDir;
	float* minB = (float*)&mins;
	float* maxB = (float*)&maxs;
	float coord[3];

	const char RIGHT = 0;
	const char LEFT = 1;
	const char MIDDLE = 2;

	/* Find candidate planes; this loop can be avoided if
	rays cast all from the eye(assume perpsective view) */
	for (i = 0; i < 3; i++)
	{
		if (origin[i] < minB[i])
		{
			quadrant[i] = LEFT;
			candidatePlane[i] = minB[i];
			inside = false;
		}
		else if (origin[i] > maxB[i])
		{
			quadrant[i] = RIGHT;
			candidatePlane[i] = maxB[i];
			inside = false;
		}
		else
		{
			quadrant[i] = MIDDLE;
		}
	}

	/* Ray origin inside bounding box */
	if (inside)
	{
		return false;
	}

	/* Calculate T distances to candidate planes */
	for (i = 0; i < 3; i++)
	{
		if (quadrant[i] != MIDDLE && abs(dir[i]) >= EPSILON)
			maxT[i] = (candidatePlane[i] - origin[i]) / dir[i];
		else
			maxT[i] = -1.0f;
	}

	/* Get largest of the maxT's for final choice of intersection */
	whichPlane = 0;
	for (i = 1; i < 3; i++)
	{
		if (maxT[whichPlane] < maxT[i])
			whichPlane = i;
	}

	/* Check final candidate actually inside box */
	if (maxT[whichPlane] < 0.0f)
		return false;
	for (i = 0; i < 3; i++)
	{
		if (whichPlane != i)
		{
			coord[i] = origin[i] + maxT[whichPlane] * dir[i];
			if (coord[i] < minB[i] || coord[i] > maxB[i])
				return false;
		}
		else
		{
			coord[i] = candidatePlane[i];
		}
	}
	/* ray hits box */

	vec3 intersectPoint(coord[0], coord[1], coord[2]);
	float dist = (intersectPoint - start).length();

	if (dist < bestDist)
	{
		bestDist = dist;
		return true;
	}

	return false;
}

bool rayPlaneIntersect(const vec3& start, const vec3& dir, const vec3& normal, float fdist, float& intersectDist)
{
	float dot = dotProduct(dir, normal);

	// don't select backfaces or parallel faces
	if (abs(dot) < EPSILON)
	{
		return false;
	}
	intersectDist = dotProduct((normal * fdist) - start, normal) / dot;

	if (intersectDist < 0.f)
	{
		return false; // intersection behind ray
	}

	return true;
}

float getDistAlongAxis(const vec3& axis, const vec3& p)
{
	return dotProduct(axis, p) / sqrt(dotProduct(axis, axis));
}

bool getPlaneFromVerts(const std::vector<vec3>& verts, vec3& outNormal, float& outDist)
{
	const float tolerance = 0.00001f; // normals more different than this = non-planar face

	size_t numVerts = verts.size();
	for (size_t i = 0; i < numVerts; i++)
	{
		vec3 v0 = verts[(i + 0) % numVerts];
		vec3 v1 = verts[(i + 1) % numVerts];
		vec3 v2 = verts[(i + 2) % numVerts];

		vec3 ba = v1 - v0;
		vec3 cb = v2 - v1;

		vec3 normal = crossProduct(ba, cb).normalize(1.0f);

		if (i == 0)
		{
			outNormal = normal;
		}
		else
		{
			float dot = dotProduct(outNormal, normal);
			if (abs(dot) < 1.0f - tolerance)
			{
				return false; // non-planar face
			}
		}
	}

	outDist = getDistAlongAxis(outNormal, verts[0]);
	return true;
}

vec2 getCenter(std::vector<vec2>& verts)
{
	vec2 maxs = vec2(-FLT_MAX_COORD, -FLT_MAX_COORD);
	vec2 mins = vec2(FLT_MAX_COORD, FLT_MAX_COORD);

	for (int i = 0; i < verts.size(); i++)
	{
		expandBoundingBox(verts[i], mins, maxs);
	}

	return mins + (maxs - mins) * 0.5f;
}

vec3 getCenter(std::vector<vec3>& verts)
{
	vec3 maxs = vec3(-FLT_MAX_COORD, -FLT_MAX_COORD, -FLT_MAX_COORD);
	vec3 mins = vec3(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);

	for (int i = 0; i < verts.size(); i++)
	{
		expandBoundingBox(verts[i], mins, maxs);
	}

	return mins + (maxs - mins) * 0.5f;
}


vec3 getCenter(const vec3& maxs, const vec3& mins)
{
	return mins + (maxs - mins) * 0.5f;
}

void getBoundingBox(const std::vector<vec3>& verts, vec3& mins, vec3& maxs)
{
	maxs = vec3(-FLT_MAX_COORD, -FLT_MAX_COORD, -FLT_MAX_COORD);
	mins = vec3(FLT_MAX_COORD, FLT_MAX_COORD, FLT_MAX_COORD);

	for (int i = 0; i < verts.size(); i++)
	{
		expandBoundingBox(verts[i], mins, maxs);
	}
}

void expandBoundingBox(const vec3& v, vec3& mins, vec3& maxs)
{
	if (v.x > maxs.x) maxs.x = v.x;
	if (v.y > maxs.y) maxs.y = v.y;
	if (v.z > maxs.z) maxs.z = v.z;

	if (v.x < mins.x) mins.x = v.x;
	if (v.y < mins.y) mins.y = v.y;
	if (v.z < mins.z) mins.z = v.z;
}

void expandBoundingBox(const vec2& v, vec2& mins, vec2& maxs)
{
	if (v.x > maxs.x) maxs.x = v.x;
	if (v.y > maxs.y) maxs.y = v.y;

	if (v.x < mins.x) mins.x = v.x;
	if (v.y < mins.y) mins.y = v.y;
}

std::vector<vec3> getPlaneIntersectVerts(std::vector<BSPPLANE>& planes)
{
	std::vector<vec3> intersectVerts;

	// https://math.stackexchange.com/questions/1883835/get-list-of-vertices-from-list-of-planes
	size_t numPlanes = planes.size();

	if (numPlanes < 2)
		return intersectVerts;

	for (size_t i = 0; i < numPlanes - 2; i++)
	{
		for (size_t j = i + 1; j < numPlanes - 1; j++)
		{
			for (size_t k = j + 1; k < numPlanes; k++)
			{
				vec3& n0 = planes[i].vNormal;
				vec3& n1 = planes[j].vNormal;
				vec3& n2 = planes[k].vNormal;
				float d0 = planes[i].fDist;
				float d1 = planes[j].fDist;
				float d2 = planes[k].fDist;

				float t = n0.x * (n1.y * n2.z - n1.z * n2.y) +
					n0.y * (n1.z * n2.x - n1.x * n2.z) +
					n0.z * (n1.x * n2.y - n1.y * n2.x);

				if (abs(t) < EPSILON)
				{
					continue;
				}

				// don't use crossProduct because it's less accurate
				//vec3 v = crossProduct(n1, n2)*d0 + crossProduct(n0, n2)*d1 + crossProduct(n0, n1)*d2;
				vec3 v(
					(d0 * (n1.z * n2.y - n1.y * n2.z) + d1 * (n0.y * n2.z - n0.z * n2.y) + d2 * (n0.z * n1.y - n0.y * n1.z)) / -t,
					(d0 * (n1.x * n2.z - n1.z * n2.x) + d1 * (n0.z * n2.x - n0.x * n2.z) + d2 * (n0.x * n1.z - n0.z * n1.x)) / -t,
					(d0 * (n1.y * n2.x - n1.x * n2.y) + d1 * (n0.x * n2.y - n0.y * n2.x) + d2 * (n0.y * n1.x - n0.x * n1.y)) / -t
				);

				bool validVertex = true;

				for (int m = 0; m < numPlanes; m++)
				{
					BSPPLANE& pm = planes[m];
					if (m != i && m != j && m != k && dotProduct(v, pm.vNormal) < pm.fDist + EPSILON)
					{
						validVertex = false;
						break;
					}
				}

				if (validVertex)
				{
					intersectVerts.push_back(v);
				}
			}
		}
	}

	return intersectVerts;
}

bool vertsAllOnOneSide(std::vector<vec3>& verts, BSPPLANE& plane)
{
// check that all verts are on one side of the plane.
	int planeSide = 0;
	for (int k = 0; k < verts.size(); k++)
	{
		float d = dotProduct(verts[k], plane.vNormal) - plane.fDist;
		if (d < -EPSILON)
		{
			if (planeSide == 1)
			{
				return false;
			}
			planeSide = -1;
		}
		if (d >= EPSILON)
		{
			if (planeSide == -1)
			{
				return false;
			}
			planeSide = 1;
		}
	}

	return true;
}

std::vector<vec3> getTriangularVerts(std::vector<vec3>& verts)
{
	int i0 = 0;
	int i1 = -1;
	int i2 = -1;

	int count = 1;
	for (int i = 1; i < verts.size() && count < 3; i++)
	{
		if (verts[i] != verts[i0])
		{
			i1 = i;
			break;
		}
		count++;
	}

	if (i1 == -1)
	{
//logf("Only 1 unique vert!\n");
		return std::vector<vec3>();
	}

	for (int i = 1; i < verts.size(); i++)
	{
		if (i == i1)
			continue;

		if (verts[i] != verts[i0] && verts[i] != verts[i1])
		{
			vec3 ab = (verts[i1] - verts[i0]).normalize();
			vec3 ac = (verts[i] - verts[i0]).normalize();
			if (abs(dotProduct(ab, ac) - 1.0) < EPSILON)
			{
				continue;
			}

			i2 = i;
			break;
		}
	}

	if (i2 == -1)
	{
//logf("All verts are colinear!\n");
		return std::vector<vec3>();
	}

	return {verts[i0], verts[i1], verts[i2]};
}

vec3 getNormalFromVerts(std::vector<vec3>& verts)
{
	std::vector<vec3> triangularVerts = getTriangularVerts(verts);

	if (triangularVerts.empty())
		return vec3();

	vec3 e1 = (triangularVerts[1] - triangularVerts[0]).normalize();
	vec3 e2 = (triangularVerts[2] - triangularVerts[0]).normalize();
	vec3 vertsNormal = crossProduct(e1, e2).normalize();

	return vertsNormal;
}

std::vector<vec2> localizeVerts(std::vector<vec3>& verts)
{
	std::vector<vec3> triangularVerts = getTriangularVerts(verts);

	if (triangularVerts.empty())
	{
		return std::vector<vec2>();
	}

	vec3 e1 = (triangularVerts[1] - triangularVerts[0]).normalize();
	vec3 e2 = (triangularVerts[2] - triangularVerts[0]).normalize();

	vec3 plane_z = crossProduct(e1, e2).normalize();
	vec3 plane_x = e1;
	vec3 plane_y = crossProduct(plane_z, plane_x).normalize();

	mat4x4 worldToLocal = worldToLocalTransform(plane_x, plane_y, plane_z);

	std::vector<vec2> localVerts(verts.size());
	for (int e = 0; e < verts.size(); e++)
	{
		localVerts[e] = (worldToLocal * vec4(verts[e], 1)).xy();
	}

	return localVerts;
}

std::vector<int> getSortedPlanarVertOrder(std::vector<vec3>& verts)
{

	std::vector<vec2> localVerts = localizeVerts(verts);
	if (localVerts.empty())
	{
		return std::vector<int>();
	}

	vec2 center = getCenter(localVerts);
	std::vector<int> orderedVerts;
	std::vector<int> remainingVerts;

	for (int i = 0; i < localVerts.size(); i++)
	{
		remainingVerts.push_back(i);
	}

	orderedVerts.push_back(remainingVerts[0]);
	vec2 lastVert = localVerts[0];
	remainingVerts.erase(remainingVerts.begin() + 0);
	localVerts.erase(localVerts.begin() + 0);
	for (size_t k = 0, sz = remainingVerts.size(); k < sz; k++)
	{
		size_t bestIdx = 0;
		float bestAngle = FLT_MAX_COORD;

		for (size_t i = 0; i < remainingVerts.size(); i++)
		{
			vec2 a = lastVert;
			vec2 b = localVerts[i];
			float a1 = atan2(a.x - center.x, a.y - center.y);
			float a2 = atan2(b.x - center.x, b.y - center.y);
			float angle = a2 - a1;
			if (angle < 0)
				angle += PI * 2;

			if (angle < bestAngle)
			{
				bestAngle = angle;
				bestIdx = i;
			}
		}

		lastVert = localVerts[bestIdx];
		orderedVerts.push_back(remainingVerts[bestIdx]);
		remainingVerts.erase(remainingVerts.begin() + bestIdx);
		localVerts.erase(localVerts.begin() + bestIdx);
	}

	return orderedVerts;
}

std::vector<vec3> getSortedPlanarVerts(std::vector<vec3>& verts)
{
	std::vector<vec3> outVerts;
	std::vector<int> vertOrder = getSortedPlanarVertOrder(verts);
	if (vertOrder.empty())
	{
		return outVerts;
	}
	outVerts.resize(vertOrder.size());
	for (int i = 0; i < vertOrder.size(); i++)
	{
		outVerts[i] = verts[vertOrder[i]];
	}
	return outVerts;
}

bool pointInsidePolygon(std::vector<vec2>& poly, vec2 p)
{
// https://stackoverflow.com/a/34689268
	bool inside = true;
	float lastd = 0;
	for (int i = 0; i < poly.size(); i++)
	{
		vec2& v1 = poly[i];
		vec2& v2 = poly[(i + 1) % poly.size()];

		if (abs(v1.x - p.x) < EPSILON && abs(v1.y - p.y) < EPSILON)
		{
			break; // on edge = inside
		}

		float d = (p.x - v1.x) * (v2.y - v1.y) - (p.y - v1.y) * (v2.x - v1.x);

		if ((d < 0 && lastd > 0) || (d > 0 && lastd < 0))
		{
// point is outside of this edge
			inside = false;
			break;
		}
		lastd = d;
	}
	return inside;
}

#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define NO_COMPRESION 0
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0

void WriteBMP(const std::string& fileName, unsigned char* pixels, int width, int height, int bytesPerPixel)
{
	FILE* outputFile = NULL;
	fopen_s(&outputFile, fileName.c_str(), "wb");
	if (!outputFile)
	{
		logf("Can't write to output file!\n");
		return;
	}
	//*****HEADER************//
	const char* BM = "BM";
	fwrite(&BM[0], 1, 1, outputFile);
	fwrite(&BM[1], 1, 1, outputFile);
	int paddedRowSize = (int)(4 * ceil((float)width / 4.0f)) * bytesPerPixel;
	int fileSize = paddedRowSize * height + HEADER_SIZE + INFO_HEADER_SIZE;
	fwrite(&fileSize, 4, 1, outputFile);
	int reserved = 0x0000;
	fwrite(&reserved, 4, 1, outputFile);
	int dataOffset = HEADER_SIZE + INFO_HEADER_SIZE;
	fwrite(&dataOffset, 4, 1, outputFile);

	//*******INFO*HEADER******//
	int infoHeaderSize = INFO_HEADER_SIZE;
	fwrite(&infoHeaderSize, 4, 1, outputFile);
	fwrite(&width, 4, 1, outputFile);
	fwrite(&height, 4, 1, outputFile);
	short planes = 1; //always 1
	fwrite(&planes, 2, 1, outputFile);
	short bitsPerPixel = (short)(bytesPerPixel * 8);
	fwrite(&bitsPerPixel, 2, 1, outputFile);
	//write compression
	int compression = NO_COMPRESION;
	fwrite(&compression, 4, 1, outputFile);
	//write image size(in bytes)
	int imageSize = width * height * bytesPerPixel;
	fwrite(&imageSize, 4, 1, outputFile);
	int resolutionX = 11811; //300 dpi
	int resolutionY = 11811; //300 dpi
	fwrite(&resolutionX, 4, 1, outputFile);
	fwrite(&resolutionY, 4, 1, outputFile);
	int colorsUsed = MAX_NUMBER_OF_COLORS;
	fwrite(&colorsUsed, 4, 1, outputFile);
	int importantColors = ALL_COLORS_REQUIRED;
	fwrite(&importantColors, 4, 1, outputFile);
	int i = 0;
	int unpaddedRowSize = width * bytesPerPixel;
	for (i = 0; i < height; i++)
	{
		int pixelOffset = ((height - i) - 1) * unpaddedRowSize;
		fwrite(&pixels[pixelOffset], 1, paddedRowSize, outputFile);
	}
	fclose(outputFile);
}


bool dirExists(const std::string& dirName)
{
	return fs::exists(dirName) && fs::is_directory(dirName);
}

#ifndef WIN32
// mkdir_p for linux from https://gist.github.com/ChisholmKyle/0cbedcd3e64132243a39
int mkdir_p(const char* dir, const mode_t mode)
{
	const int PATH_MAX_STRING_SIZE = 256;
	char tmp[PATH_MAX_STRING_SIZE];
	char* p = NULL;
	struct stat sb;
	size_t len;

	/* copy path */
	len = strnlen(dir, PATH_MAX_STRING_SIZE);
	if (len == 0 || len == PATH_MAX_STRING_SIZE)
	{
		return -1;
	}
	memcpy(tmp, dir, len);
	tmp[len] = '\0';

	/* remove trailing slash */
	if (tmp[len - 1] == '/')
	{
		tmp[len - 1] = '\0';
	}

	/* check if path exists and is a directory */
	if (stat(tmp, &sb) == 0)
	{
		if (S_ISDIR(sb.st_mode))
		{
			return 0;
		}
	}

	/* recursive mkdir */
	for (p = tmp + 1; *p; p++)
	{
		if (*p == '/')
		{
			*p = 0;
			/* test path */
			if (stat(tmp, &sb) != 0)
			{
/* path does not exist - create directory */
				if (mkdir(tmp, mode) < 0)
				{
					return -1;
				}
			}
			else if (!S_ISDIR(sb.st_mode))
			{
/* not a directory */
				return -1;
			}
			*p = '/';
		}
	}
	/* test path */
	if (stat(tmp, &sb) != 0)
	{
/* path does not exist - create directory */
		if (mkdir(tmp, mode) < 0)
		{
			return -1;
		}
	}
	else if (!S_ISDIR(sb.st_mode))
	{
/* not a directory */
		return -1;
	}
	return 0;
}
#endif 

bool createDir(const std::string& dirName)
{
	std::string fixDirName = dirName.c_str();
	fixupPath(fixDirName, FIXUPPATH_SLASH::FIXUPPATH_SLASH_SKIP, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
	if (dirExists(fixDirName))
		return true;
	fs::create_directories(fixDirName);
	if (dirExists(fixDirName))
		return true;
	return false;
}

void removeDir(const std::string& dirName)
{
	std::string fixDirName = dirName.c_str();
	fixupPath(fixDirName, FIXUPPATH_SLASH::FIXUPPATH_SLASH_SKIP, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
	std::error_code e;
	fs::remove_all(fixDirName, e);
}


void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}
void fixupPath(char* path, FIXUPPATH_SLASH startslash, FIXUPPATH_SLASH endslash)
{
	std::string tmpPath = path;
	fixupPath(tmpPath, startslash, endslash);
	memcpy(path, &tmpPath[0], tmpPath.size() + 1);
}

void fixupPath(std::string& path, FIXUPPATH_SLASH startslash, FIXUPPATH_SLASH endslash)
{
	if (path.empty())
		return;
	replaceAll(path, "\"", "");
	replaceAll(path, "\'", "");
#ifdef WIN32
	replaceAll(path, "/", "\\");
	replaceAll(path, "\\\\", "\\");
#else
	replaceAll(path, "\\", "/");
	replaceAll(path, "//", "/");
#endif
	if (startslash == FIXUPPATH_SLASH::FIXUPPATH_SLASH_CREATE)
	{
		if (path[0] != '\\' && path[0] != '/')
		{
#ifdef WIN32
			path = "\\" + path;
#else 
			path = "/" + path;
#endif
		}
	}
	else if (startslash == FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE)
	{
		if (path[0] == '\\' || path[0] == '/')
		{
			path.erase(path.begin());
		}
	}

	if (endslash == FIXUPPATH_SLASH::FIXUPPATH_SLASH_CREATE)
	{
		if (path.empty() || (path.back() != '\\' && path.back() != '/'))
		{
#ifdef WIN32
			path = path + "\\";
#else 
			path = path + "/";
#endif
		}
	}
	else if (endslash == FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE)
	{
		if (path.empty())
			return;

		if (path.back() == '\\' || path.back() == '/')
		{
			path.pop_back();
		}
	}

#ifdef WIN32
	replaceAll(path, "/", "\\");
	replaceAll(path, "\\\\", "\\");
#else
	replaceAll(path, "\\", "/");
	replaceAll(path, "//", "/");
#endif
}

std::string GetCurrentWorkingDir()
{
#ifdef WIN32
	return fs::current_path().string() + "\\";
#else 
	return fs::current_path().string() + "/";
#endif
}

#ifdef WIN32
void print_color(int colors)
{
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	colors = colors ? colors : (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	SetConsoleTextAttribute(console, (WORD)colors);
}

std::string getConfigDir()
{
	char path[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path);
	return std::string(path) + "\\AppData\\Roaming\\bspguy\\";
}
#else 
void print_color(int colors)
{
	if (!colors)
	{
		logf("\x1B[0m");
		return;
	}
	const char* mode = colors & PRINT_BRIGHT ? "1" : "0";
	const char* color = "37";
	switch (colors & ~PRINT_BRIGHT)
	{
		case PRINT_RED:								color = "31"; break;
		case PRINT_GREEN:							color = "32"; break;
		case PRINT_RED | PRINT_GREEN:				color = "33"; break;
		case PRINT_BLUE:							color = "34"; break;
		case PRINT_RED | PRINT_BLUE:				color = "35"; break;
		case PRINT_GREEN | PRINT_BLUE:				color = "36"; break;
		case PRINT_GREEN | PRINT_BLUE | PRINT_RED:	color = "36"; break;
	}
	logf("\x1B[%s;%sm", mode, color);
}

std::string getConfigDir()
{
	return std::string("") + getenv("HOME") + "/.config/bspguy/";
}
#endif


bool VectorCompare(vec3 v1, vec3 v2)
{
	int		i;

	for (i = 0; i < 3; i++)
		if (abs(v1[i] - v2[i]) > EPSILON)
			return false;

	return true;
}



void AngleQuaternion(const vec3& angles, vec4& quaternion)
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;

	// FIXME: rescale the inputs to 1/2 angle
	angle = angles[2] * 0.5f;
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[1] * 0.5f;
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[0] * 0.5f;
	sr = sin(angle);
	cr = cos(angle);

	quaternion[0] = sr * cp * cy - cr * sp * sy; // X
	quaternion[1] = cr * sp * cy + sr * cp * sy; // Y
	quaternion[2] = cr * cp * sy - sr * sp * cy; // Z
	quaternion[3] = cr * cp * cy + sr * sp * sy; // W
}

void QuaternionSlerp(const vec4& p, vec4 q, float t, vec4& qt)
{
	int i;
	float omega, cosom, sinom, sclp, sclq;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;
	for (i = 0; i < 4; i++)
	{
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}
	if (a > b)
	{
		for (i = 0; i < 4; i++)
		{
			q[i] = -q[i];
		}
	}

	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if ((1.0f + cosom) > 0.00000001)
	{
		if ((1.0f - cosom) > 0.00000001)
		{
			omega = acos(cosom);
			sinom = sin(omega);
			sclp = sin((1.0f - t) * omega) / sinom;
			sclq = sin(t * omega) / sinom;
		}
		else
		{
			sclp = 1.0f - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++)
		{
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else
	{
		qt[0] = -p[1];
		qt[1] = p[0];
		qt[2] = -p[3];
		qt[3] = p[2];
		sclp = sin((1.0f - t) * 0.5f * PI);
		sclq = sin(t * 0.5f * PI);
		for (i = 0; i < 3; i++)
		{
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}



void QuaternionMatrix(const vec4& quaternion, float(*matrix)[4])
{
	matrix[0][0] = 1.0f - 2.0f * quaternion[1] * quaternion[1] - 2.0f * quaternion[2] * quaternion[2];
	matrix[1][0] = 2.0f * quaternion[0] * quaternion[1] + 2.0f * quaternion[3] * quaternion[2];
	matrix[2][0] = 2.0f * quaternion[0] * quaternion[2] - 2.0f * quaternion[3] * quaternion[1];

	matrix[0][1] = 2.0f * quaternion[0] * quaternion[1] - 2.0f * quaternion[3] * quaternion[2];
	matrix[1][1] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[2] * quaternion[2];
	matrix[2][1] = 2.0f * quaternion[1] * quaternion[2] + 2.0f * quaternion[3] * quaternion[0];

	matrix[0][2] = 2.0f * quaternion[0] * quaternion[2] + 2.0f * quaternion[3] * quaternion[1];
	matrix[1][2] = 2.0f * quaternion[1] * quaternion[2] - 2.0f * quaternion[3] * quaternion[0];
	matrix[2][2] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[1] * quaternion[1];
}



void R_ConcatTransforms(const float in1[3][4], const float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
		in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
		in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
		in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
		in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
		in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
		in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
		in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
		in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
		in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
		in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
		in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
		in1[2][2] * in2[2][3] + in1[2][3];
}

void VectorScale(vec3 v, float scale, vec3& out)
{
	out[0] = v[0] * scale;
	out[1] = v[1] * scale;
	out[2] = v[2] * scale;
}

float VectorNormalize(vec3 v)
{
	int		i;
	float	length;

	if (abs(v[1] - 0.000215956) < 0.0001)
		i = 1;

	length = 0;
	for (i = 0; i < 3; i++)
		length += v[i] * v[i];
	length = sqrt(length);
	if (abs(length) < EPSILON)
		return 0;

	for (i = 0; i < 3; i++)
		v[i] /= length;

	return length;
}


void mCrossProduct(vec3 v1, vec3 v2, vec3& cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

void VectorIRotate(const vec3& in1, const float in2[3][4], vec3& out)
{
	out[0] = in1[0] * in2[0][0] + in1[1] * in2[1][0] + in1[2] * in2[2][0];
	out[1] = in1[0] * in2[0][1] + in1[1] * in2[1][1] + in1[2] * in2[2][1];
	out[2] = in1[0] * in2[0][2] + in1[1] * in2[1][2] + in1[2] * in2[2][2];
}

void VectorTransform(const vec3& in1, const float in2[3][4], vec3& out)
{
	out[0] = mDotProduct(in1, in2[0]) + in2[0][3];
	out[1] = mDotProduct(in1, in2[1]) + in2[1][3];
	out[2] = mDotProduct(in1, in2[2]) + in2[2][3];
}

int TextureAxisFromPlane(const BSPPLANE& pln, vec3& xv, vec3& yv)
{
	int             bestaxis;
	float           dot, best;
	int             i;

	best = 0;
	bestaxis = 0;

	for (i = 0; i < 6; i++)
	{
		dot = dotProduct(pln.vNormal, s_baseaxis[i * 3]);
		if (dot > best)
		{
			best = dot;
			bestaxis = i;
		}
	}

	xv = s_baseaxis[bestaxis * 3 + 1];
	yv = s_baseaxis[bestaxis * 3 + 2];

	return bestaxis;
}

float AngleFromTextureAxis(vec3 axis, bool x, int type)
{
	float retval = 0.0f;

	if (type < 2)
	{
		if (x)
		{
			return -1.f * atan2(axis.y, axis.x) * (180.f / PI);
		}
		else
		{
			return atan2(axis.x, axis.y) * (180.f / PI);
		}
	}


	if (type < 4)
	{
		if (x)
		{
			return -1.f * atan2(axis.z, axis.y) * (180.f / PI);
		}
		else
		{
			return atan2(axis.y, axis.z) * (180.f / PI);
		}
	}

	if (type < 6)
	{
		if (x)
		{
			return -1.f * atan2(axis.z, axis.x) * (180.f / PI);
		}
		else
		{
			return atan2(axis.x, axis.z) * (180.f / PI);
		}
	}


	return retval;
}


vec3 AxisFromTextureAngle(float angle, bool x, int type)
{
	vec3 retval = vec3();


	if (type < 2)
	{
		if (x)
		{
			retval.y = -1.f * sin(angle / 180.f * PI);
			retval.x = cos(angle / 180.f * PI);
		}
		else
		{
			retval.x = -1.f * sin((angle + 180.f) / 180.f * PI);
			retval.y = -1.f * cos((angle + 180.f) / 180.f * PI);
		}
		return retval;
	}

	if (type < 4)
	{
		if (x)
		{
			retval.z = -1.f * sin(angle / 180.f * PI);
			retval.y = cos(angle / 180.f * PI);
		}
		else
		{
			retval.y = -1.f * sin((angle + 180.f) / 180.f * PI);
			retval.z = -1.f * cos((angle + 180.f) / 180.f * PI);
		}
		return retval;
	}


	if (type < 6)
	{
		if (x)
		{
			retval.z = -1.f * sin(angle / 180.f * PI);
			retval.x = cos(angle / 180.f * PI);
		}
		else
		{
			retval.x = -1.f * sin((angle + 180.f) / 180.f * PI);
			retval.z = -1.f * cos((angle + 180.f) / 180.f * PI);
		}
		return retval;
	}

	return retval;
}

// For issue when string.size > 0 but string length is zero ("\0\0\0" string for example)
size_t strlen(std::string str)
{
	return str.size() ? strlen(str.c_str()) : 0;
}

int ColorDistance(COLOR3 color, COLOR3 other)
{
	int dist_b = abs(other.b - color.b);
	int dist_g = abs(other.g - color.g);
	int dist_r = abs(other.r - color.r);
	return dist_b + dist_g + dist_r;
}

bool Is256Colors(COLOR3* image, int size)
{
	int colorCount = 0;
	COLOR3 palette[256];
	memset(&palette, 0, sizeof(COLOR3) * 256);
	for (int y = 0; y < size / 2; y++)
	{
		int paletteIdx = -1;
		for (int k = 0; k < colorCount; k++)
		{
			if (image[y] == palette[k])
			{
				paletteIdx = k;
				break;
			}
		}
		if (paletteIdx == -1)
		{
			if (colorCount >= 256)
			{
				return false;
			}
			palette[colorCount] = image[y];
			paletteIdx = colorCount;
			colorCount++;
		}
	}
	return true;
}

void SimpeColorReduce(COLOR3* image, int size)
{
	std::vector<COLOR3> colorset;
	for (int i = 255; i > 0; i--)
	{
		colorset.push_back(COLOR3(i, i, i));
	}

	for (int i = 0; i < size; i++)
	{
		for (auto& color : colorset)
		{
			if (ColorDistance(image[i], color) <= 3)
			{
				image[i] = color;
			}
		}
	}
}