#include <string.h>
#include <algorithm>
#include "BspRenderer.h"
#include "VertexBuffer.h"
#include "primitives.h"
#include "rad.h"
#include "vis.h"
#include "lodepng.h"
#include "Renderer.h"
#include "Clipper.h"
#include "Command.h"
#include "icons/missing.h"


BspRenderer::BspRenderer(Bsp* _map, ShaderProgram* _bspShader, ShaderProgram* _fullBrightBspShader,
						 ShaderProgram* _colorShader, PointEntRenderer* _pointEntRenderer)
{
	this->map = _map;
	this->map->setBspRender(this);
	this->bspShader = _bspShader;
	this->fullBrightBspShader = _fullBrightBspShader;
	this->colorShader = _colorShader;
	this->pointEntRenderer = _pointEntRenderer;


	debugEntOffset = vec3();
	debugClipnodeVis = false;
	debugLeafIdx = std::vector<LeafDebug>();

	renderEnts = NULL;
	renderModels = NULL;
	faceMaths = NULL;

	whiteTex = new Texture(1, 1, "white");
	greyTex = new Texture(1, 1, "grey");
	redTex = new Texture(1, 1, "red");
	yellowTex = new Texture(1, 1, "yellow");
	blackTex = new Texture(1, 1, "black");
	blueTex = new Texture(1, 1, "blue");

	*((COLOR3*)(whiteTex->data)) = {255, 255, 255};
	*((COLOR3*)(redTex->data)) = {110, 0, 0};
	*((COLOR3*)(yellowTex->data)) = {255, 255, 0};
	*((COLOR3*)(greyTex->data)) = {64, 64, 64};
	*((COLOR3*)(blackTex->data)) = {0, 0, 0};
	*((COLOR3*)(blueTex->data)) = {0, 0, 200};

	whiteTex->upload(GL_RGB);
	redTex->upload(GL_RGB);
	yellowTex->upload(GL_RGB);
	greyTex->upload(GL_RGB);
	blackTex->upload(GL_RGB);
	blueTex->upload(GL_RGB);

	unsigned char* img_dat = NULL;
	unsigned int w, h;
	lodepng_decode24(&img_dat, &w, &h, missing_dat, sizeof(missing_dat));
	missingTex = new Texture(w, h, img_dat, "missing");
	missingTex->upload(GL_RGB);

	//loadTextures();
	//loadLightmaps();
	calcFaceMaths();
	preRenderFaces();
	preRenderEnts();
	if (bspShader)
	{
		bspShader->bind();

		unsigned int sTexId = glGetUniformLocation(bspShader->ID, "sTex");
		glUniform1i(sTexId, 0);
		for (int s = 0; s < MAXLIGHTMAPS; s++)
		{
			unsigned int sLightmapTexIds = glGetUniformLocation(bspShader->ID, ("sLightmapTex" + std::to_string(s)).c_str());

			// assign lightmap texture units (skips the normal texture unit)
			glUniform1i(sLightmapTexIds, s + 1);
		}
	}
	if (fullBrightBspShader)
	{
		fullBrightBspShader->bind();

		unsigned int sTexId2 = glGetUniformLocation(fullBrightBspShader->ID, "sTex");
		glUniform1i(sTexId2, 0);
	}
	if (colorShader)
	{
		colorShaderMultId = glGetUniformLocation(colorShader->ID, "colorMult");
		numRenderClipnodes = map->modelCount;
		lightmapFuture = std::async(std::launch::async, &BspRenderer::loadLightmaps, this);
		texturesFuture = std::async(std::launch::async, &BspRenderer::loadTextures, this);
		clipnodesFuture = std::async(std::launch::async, &BspRenderer::loadClipnodes, this);
	}
	else
	{
		loadTextures();
	}
	// cache ent targets so first selection doesn't lag
	for (int i = 0; i < map->ents.size(); i++)
	{
		map->ents[i]->getTargets();
	}

	memset(&undoLumpState, 0, sizeof(LumpState));

	undoEntityState = std::map<int, Entity>();
}

void BspRenderer::loadTextures()
{
	for (int i = 0; i < wads.size(); i++)
	{
		delete wads[i];
	}
	wads.clear();

	std::vector<std::string> wadNames;

	bool foundInfoDecals = false;
	bool foundDecalWad = false;

	for (int i = 0; i < map->ents.size(); i++)
	{
		if (map->ents[i]->keyvalues["classname"] == "worldspawn")
		{
			wadNames = splitString(map->ents[i]->keyvalues["wad"], ";");

			for (int k = 0; k < wadNames.size(); k++)
			{
				wadNames[k] = basename(wadNames[k]);
				if (toLowerCase(wadNames[k]) == "decals.wad")
					foundDecalWad = true;
			}

			if (g_settings.stripWad)
			{
				std::string newWadString = "";

				for (int k = 0; k < wadNames.size(); k++)
				{
					newWadString += wadNames[k] + ";";
				}
				map->ents[i]->setOrAddKeyvalue("wad", newWadString);
			}
		}
		if (map->ents[i]->keyvalues["classname"] == "infodecal")
		{
			foundInfoDecals = true;
		}
	}

	std::vector<std::string> tryPaths{};
	tryPaths.push_back(GetCurrentDir());
	if (GetCurrentDir() != g_config_dir)
		tryPaths.push_back(g_config_dir);

	for (auto& path : g_settings.resPaths)
	{
		if (path.enabled)
			tryPaths.push_back(path.path);
	}

	if (foundInfoDecals && !foundDecalWad)
	{
		wadNames.push_back("decals.wad");
	}

	for (int i = 0; i < wadNames.size(); i++)
	{
		std::string path = std::string();
		for (int k = 0; k < tryPaths.size(); k++)
		{
			std::string tryPath = tryPaths[k] + wadNames[i];
			if (!fileExists(tryPath))
				tryPath = g_settings.gamedir + tryPaths[k] + wadNames[i];
			if (fileExists(tryPath))
			{
				path = std::move(tryPath);
				break;
			}
		}

		if (path.empty())
		{
			logf("Missing WAD: {}\n", wadNames[i]);
			continue;
		}

		logf("Loading WAD {}\n", path);
		Wad* wad = new Wad(path);
		if (wad->readInfo())
			wads.push_back(wad);
		else
		{
			logf("Unreadable WAD file {}\n", path);
			delete wad;
		}
	}

	int wadTexCount = 0;
	int missingCount = 0;
	int embedCount = 0;

	glTexturesSwap = new Texture * [map->textureCount];
	for (int i = 0; i < map->textureCount; i++)
	{
		int texOffset = ((int*)map->textures)[i + 1];
		if (texOffset == -1)
		{
			glTexturesSwap[i] = missingTex;
			continue;
		}
		BSPMIPTEX* tex = ((BSPMIPTEX*)(map->textures + texOffset));
		COLOR3* imageData = NULL;
		WADTEX* wadTex = NULL;
		if (tex->nOffsets[0] <= 0)
		{
			bool foundInWad = false;
			for (int k = 0; k < wads.size(); k++)
			{
				if (wads[k]->hasTexture(tex->szName))
				{
					foundInWad = true;
					wadTex = wads[k]->readTexture(tex->szName);
					imageData = ConvertWadTexToRGB(wadTex);
					wadTexCount++;
					break;
				}
			}

			if (!foundInWad)
			{
				glTexturesSwap[i] = missingTex;
				missingCount++;
				continue;
			}
		}
		else
		{
			imageData = ConvertMipTexToRGB(tex);
			embedCount++;
		}
		if (wadTex)
			glTexturesSwap[i] = new Texture(wadTex->nWidth, wadTex->nHeight, (unsigned char*)imageData, wadTex->szName);
		else
			glTexturesSwap[i] = new Texture(tex->nWidth, tex->nHeight, (unsigned char*)imageData, tex->szName);

		if (wadTex)
			delete wadTex;
	}

	if (wadTexCount)
		logf("Loaded {} wad textures\n", wadTexCount);
	if (embedCount)
		logf("Loaded {} embedded textures\n", embedCount);
	if (missingCount)
		logf("{} missing textures\n", missingCount);
}

void BspRenderer::reload()
{
	updateLightmapInfos();
	calcFaceMaths();
	preRenderFaces();
	preRenderEnts();
	reloadTextures();
	reloadLightmaps();
	reloadClipnodes();
}

void BspRenderer::reloadTextures()
{
	texturesLoaded = false;
	texturesFuture = std::async(std::launch::async, &BspRenderer::loadTextures, this);
}

void BspRenderer::reloadLightmaps()
{
	lightmapsGenerated = false;
	lightmapsUploaded = false;
	deleteLightmapTextures();
	if (lightmaps)
	{
		delete[] lightmaps;
	}
	lightmapFuture = std::async(std::launch::async, &BspRenderer::loadLightmaps, this);
}

void BspRenderer::reloadClipnodes()
{
	clipnodesLoaded = false;
	clipnodeLeafCount = 0;

	deleteRenderClipnodes();

	clipnodesFuture = std::async(std::launch::async, &BspRenderer::loadClipnodes, this);
}

void BspRenderer::addClipnodeModel(int modelIdx)
{
	if (modelIdx < 0)
		return;
	RenderClipnodes* newRenderClipnodes = new RenderClipnodes[numRenderClipnodes + 1];
	for (int i = 0; i < numRenderClipnodes; i++)
	{
		newRenderClipnodes[i] = renderClipnodes[i];
	}
	memset(&newRenderClipnodes[numRenderClipnodes], 0, sizeof(RenderClipnodes));
	numRenderClipnodes++;
	renderClipnodes = newRenderClipnodes;

	generateClipnodeBuffer(modelIdx);
}

void BspRenderer::updateModelShaders()
{
	ShaderProgram* activeShader = (g_render_flags & RENDER_LIGHTMAPS) ? bspShader : fullBrightBspShader;

	for (int i = 0; i < numRenderModels; i++)
	{
		RenderModel& model = renderModels[i];
		for (int k = 0; k < model.groupCount; k++)
		{
			model.renderGroups[k].buffer->setShader(activeShader, true);
			model.renderGroups[k].wireframeBuffer->setShader(activeShader, true);
		}
	}
}

void BspRenderer::loadLightmaps()
{
	std::vector<LightmapNode*> atlases;
	std::vector<Texture*> atlasTextures;
	atlases.push_back(new LightmapNode(0, 0, LIGHTMAP_ATLAS_SIZE, LIGHTMAP_ATLAS_SIZE));
	atlasTextures.push_back(new Texture(LIGHTMAP_ATLAS_SIZE, LIGHTMAP_ATLAS_SIZE, "LIGHTMAP"));
	memset(atlasTextures[0]->data, 0, LIGHTMAP_ATLAS_SIZE * LIGHTMAP_ATLAS_SIZE * sizeof(COLOR3));

	numRenderLightmapInfos = map->faceCount;
	lightmaps = new LightmapInfo[map->faceCount];
	memset(lightmaps, 0, map->faceCount * sizeof(LightmapInfo));

	logf("Calculating lightmaps\n");

	int lightmapCount = 0;
	int atlasId = 0;
	for (int i = 0; i < map->faceCount; i++)
	{
		BSPFACE& face = map->faces[i];
		BSPTEXTUREINFO& texinfo = map->texinfos[face.iTextureInfo];

		if (face.nLightmapOffset < 0 || (texinfo.nFlags & TEX_SPECIAL) || face.nLightmapOffset >= map->bsp_header.lump[LUMP_LIGHTING].nLength)
			continue;

		int size[2];
		int imins[2];
		int imaxs[2];
		GetFaceLightmapSize(map, i, size);
		GetFaceExtents(map, i, imins, imaxs);

		LightmapInfo& info = lightmaps[i];
		info.w = size[0];
		info.h = size[1];
		info.midTexU = (float)(size[0]) / 2.0f;
		info.midTexV = (float)(size[1]) / 2.0f;

		// TODO: float mins/maxs not needed?
		info.midPolyU = (imins[0] + imaxs[0]) * 16 / 2.0f;
		info.midPolyV = (imins[1] + imaxs[1]) * 16 / 2.0f;

		for (int s = 0; s < MAXLIGHTMAPS; s++)
		{
			if (face.nStyles[s] == 255)
				continue;

			// TODO: Try fitting in earlier atlases before using the latest one
			if (!atlases[atlasId]->insert(info.w, info.h, info.x[s], info.y[s]))
			{
				atlases.push_back(new LightmapNode(0, 0, LIGHTMAP_ATLAS_SIZE, LIGHTMAP_ATLAS_SIZE));
				atlasTextures.push_back(new Texture(LIGHTMAP_ATLAS_SIZE, LIGHTMAP_ATLAS_SIZE, "LIGHTMAP"));
				atlasId++;
				memset(atlasTextures[atlasId]->data, 0, LIGHTMAP_ATLAS_SIZE * LIGHTMAP_ATLAS_SIZE * sizeof(COLOR3));

				if (!atlases[atlasId]->insert(info.w, info.h, info.x[s], info.y[s]))
				{
					logf("Lightmap too big for atlas size ( {}x{} but allowed {}x{} )!\n", info.w, info.h, LIGHTMAP_ATLAS_SIZE, LIGHTMAP_ATLAS_SIZE);
					continue;
				}
			}

			lightmapCount++;

			info.atlasId[s] = atlasId;

			// copy lightmap data into atlas
			int lightmapSz = info.w * info.h * sizeof(COLOR3);
			int offset = face.nLightmapOffset + s * lightmapSz;
			COLOR3* lightSrc = (COLOR3*)(map->lightdata + offset);
			COLOR3* lightDst = (COLOR3*)(atlasTextures[atlasId]->data);
			for (int y = 0; y < info.h; y++)
			{
				for (int x = 0; x < info.w; x++)
				{
					int src = y * info.w + x;
					int dst = (info.y[s] + y) * LIGHTMAP_ATLAS_SIZE + info.x[s] + x;
					if (offset + src * sizeof(COLOR3) < map->lightDataLength)
					{
						lightDst[dst] = lightSrc[src];
						//lightDst[dst] = getLightMapRGB(lightSrc[src], face.nStyles[s]);
					}
					else
					{
						bool checkers = x % 2 == 0 != y % 2 == 0;
						lightDst[dst] = {(unsigned char)(checkers ? 255 : 0), 0, (unsigned char)(checkers ? 255 : 0)};
					}
				}
			}
		}
	}

	glLightmapTextures = new Texture * [atlasTextures.size()];
	for (unsigned int i = 0; i < atlasTextures.size(); i++)
	{
		glLightmapTextures[i] = atlasTextures[i];
		delete atlases[i];
	}

	numLightmapAtlases = atlasTextures.size();

	//lodepng_encode24_file("atlas.png", atlasTextures[0]->data, LIGHTMAP_ATLAS_SIZE, LIGHTMAP_ATLAS_SIZE);
	logf("Fit {} lightmaps into {} atlases\n", lightmapCount, atlasId + 1);
}

void BspRenderer::updateLightmapInfos()
{

	if (numRenderLightmapInfos == map->faceCount)
	{
		return;
	}
	if (map->faceCount < numRenderLightmapInfos)
	{
		logf("TODO: Recalculate lightmaps when faces deleted\n");
		return;
	}

	// assumes new faces have no light data
	int addedFaces = map->faceCount - numRenderLightmapInfos;

	LightmapInfo* newLightmaps = new LightmapInfo[map->faceCount];
	memcpy(newLightmaps, lightmaps, numRenderLightmapInfos * sizeof(LightmapInfo));
	memset(newLightmaps + numRenderLightmapInfos, 0, addedFaces * sizeof(LightmapInfo));

	delete[] lightmaps;
	lightmaps = newLightmaps;
	numRenderLightmapInfos = map->faceCount;
}

void BspRenderer::preRenderFaces()
{
	deleteRenderFaces();

	genRenderFaces(numRenderModels);

	for (int i = 0; i < numRenderModels; i++)
	{
		RenderModel& model = renderModels[i];
		for (int k = 0; k < model.groupCount; k++)
		{
			model.renderGroups[k].buffer->bindAttributes(true);
			model.renderGroups[k].wireframeBuffer->bindAttributes(true);
			model.renderGroups[k].buffer->upload();
			model.renderGroups[k].wireframeBuffer->upload();
		}
	}
}

void BspRenderer::genRenderFaces(int& renderModelCount)
{
	renderModels = new RenderModel[map->modelCount];
	memset(renderModels, 0, sizeof(RenderModel) * map->modelCount);
	renderModelCount = map->modelCount;

	int worldRenderGroups = 0;
	int modelRenderGroups = 0;

	for (int m = 0; m < map->modelCount; m++)
	{
		int groupCount = refreshModel(m, false);
		if (m == 0)
			worldRenderGroups += groupCount;
		else
			modelRenderGroups += groupCount;
	}

	logf("Created {} solid render groups ({} world, {} entity)\n",
		 worldRenderGroups + modelRenderGroups,
		 worldRenderGroups,
		 modelRenderGroups);
}

void BspRenderer::deleteRenderModel(RenderModel* renderModel)
{
	if (!renderModel || !renderModel->renderGroups || !renderModel->renderFaces)
	{
		return;
	}
	for (int k = 0; k < renderModel->groupCount; k++)
	{
		RenderGroup& group = renderModel->renderGroups[k];

		delete[] group.verts;
		delete[] group.wireframeVerts;

		delete group.buffer;
		delete group.wireframeBuffer;
	}

	delete[] renderModel->renderGroups;
	delete[] renderModel->renderFaces;

	renderModel->renderGroups = NULL;
	renderModel->renderFaces = NULL;
}

void BspRenderer::deleteRenderClipnodes()
{
	if (renderClipnodes)
	{
		for (int i = 0; i < numRenderClipnodes; i++)
		{
			deleteRenderModelClipnodes(&renderClipnodes[i]);
		}
		delete[] renderClipnodes;
	}

	renderClipnodes = NULL;
}

void BspRenderer::deleteRenderModelClipnodes(RenderClipnodes* renderClip)
{
	for (int i = 0; i < MAX_MAP_HULLS; i++)
	{
		if (renderClip->clipnodeBuffer[i])
		{
			delete renderClip->clipnodeBuffer[i];
			delete renderClip->wireframeClipnodeBuffer[i];
		}
		renderClip->clipnodeBuffer[i] = NULL;
		renderClip->wireframeClipnodeBuffer[i] = NULL;
	}
}

void BspRenderer::deleteRenderFaces()
{
	if (renderModels)
	{
		for (int i = 0; i < numRenderModels; i++)
		{
			deleteRenderModel(&renderModels[i]);
		}
		delete[] renderModels;
	}

	renderModels = NULL;
}

void BspRenderer::deleteTextures()
{
	if (glTextures)
	{
		for (int i = 0; i < numLoadedTextures; i++)
		{
			if (glTextures[i] != missingTex)
			{
				delete glTextures[i];
				glTextures[i] = missingTex;
			}
		}
		delete[] glTextures;
	}

	glTextures = NULL;
}

void BspRenderer::deleteLightmapTextures()
{
	if (glLightmapTextures)
	{
		for (int i = 0; i < numLightmapAtlases; i++)
		{
			if (glLightmapTextures[i])
			{
				delete glLightmapTextures[i];
				glLightmapTextures[i] = NULL;
			}
		}
		delete[] glLightmapTextures;
	}

	glLightmapTextures = NULL;
}

void BspRenderer::deleteFaceMaths()
{
	if (faceMaths)
	{
		delete[] faceMaths;
	}

	faceMaths = NULL;
}

int BspRenderer::refreshModel(int modelIdx, bool refreshClipnodes, bool noTriangulate)
{
	if (modelIdx < 0)
		return 0;

	BSPMODEL& model = map->models[modelIdx];
	RenderModel* renderModel = &renderModels[modelIdx];

	deleteRenderModel(renderModel);

	renderModel->renderFaces = new RenderFace[model.nFaces];

	std::vector<RenderGroup> renderGroups;
	std::vector<std::vector<lightmapVert>> renderGroupVerts;
	std::vector<std::vector<lightmapVert>> renderGroupWireframeVerts;

	ShaderProgram* activeShader = (g_render_flags & RENDER_LIGHTMAPS) ? bspShader : fullBrightBspShader;

	for (int i = 0; i < model.nFaces; i++)
	{
		int faceIdx = model.iFirstFace + i;
		BSPFACE& face = map->faces[faceIdx];
		BSPTEXTUREINFO& texinfo = map->texinfos[face.iTextureInfo];
		BSPMIPTEX* tex = NULL;

		int texWidth, texHeight;
		int texOffset = ((int*)map->textures)[texinfo.iMiptex + 1];
		if (texOffset != -1 && texinfo.iMiptex != -1)
		{
			tex = ((BSPMIPTEX*)(map->textures + texOffset));
			texWidth = tex->nWidth;
			texHeight = tex->nHeight;
		}
		else
		{
	  // missing texture
			texWidth = 16;
			texHeight = 16;
		}


		LightmapInfo* lmap = lightmapsGenerated ? &lightmaps[faceIdx] : NULL;

		lightmapVert* verts = new lightmapVert[face.nEdges];
		int vertCount = face.nEdges;
		Texture* lightmapAtlas[MAXLIGHTMAPS]{NULL};

		float lw = 0;
		float lh = 0;
		if (lightmapsGenerated)
		{
			lw = (float)lmap->w / (float)LIGHTMAP_ATLAS_SIZE;
			lh = (float)lmap->h / (float)LIGHTMAP_ATLAS_SIZE;
		}

		bool isSpecial = texinfo.nFlags & TEX_SPECIAL;
		bool hasLighting = face.nStyles[0] != 255 && face.nLightmapOffset >= 0 && !isSpecial;
		for (int s = 0; s < MAXLIGHTMAPS; s++)
		{
			lightmapAtlas[s] = lightmapsGenerated ? glLightmapTextures[lmap->atlasId[s]] : NULL;
		}

		if (isSpecial)
		{
			lightmapAtlas[0] = whiteTex;
		}

		int entIdx = map->get_ent_from_model(modelIdx);
		Entity* ent = entIdx >= 0 ? map->ents[entIdx] : NULL;

		bool isOpacity = isSpecial || (tex && IsTextureTransparent(tex->szName)) || (ent && ent->hasKey("classname") && g_app->isEntTransparent(ent->keyvalues["classname"].c_str()));

		float opacity = isOpacity ? 0.50f : 1.0f;


		if (ent)
		{
			if (ent->rendermode != kRenderNormal)
			{
				opacity = ent->renderamt / 255.f;
				if (opacity > 0.8f && isOpacity)
					opacity = 0.8f;
				else if (opacity < 0.2f)
					opacity = 0.2f;
			}
		}

		for (int e = 0; e < face.nEdges; e++)
		{
			int edgeIdx = map->surfedges[face.iFirstEdge + e];
			BSPEDGE& edge = map->edges[abs(edgeIdx)];
			int vertIdx = edgeIdx < 0 ? edge.iVertex[1] : edge.iVertex[0];

			vec3& vert = map->verts[vertIdx];
			verts[e].pos = vert.flip();

			verts[e].r = 1.0f;
			if (ent)
			{
				verts[e].g = 1.0f + abs((float)ent->rendermode);
			}
			else
			{
				verts[e].g = 1.0f;
			}
			verts[e].b = 1.0f;
			verts[e].a = opacity;

			// texture coords
			float tw = 1.0f / (float)texWidth;
			float th = 1.0f / (float)texHeight;
			float fU = dotProduct(texinfo.vS, vert) + texinfo.shiftS;
			float fV = dotProduct(texinfo.vT, vert) + texinfo.shiftT;
			verts[e].u = fU * tw;
			verts[e].v = fV * th;

			// lightmap texture coords
			if (hasLighting && lightmapsGenerated)
			{
				float fLightMapU = lmap->midTexU + (fU - lmap->midPolyU) / 16.0f;
				float fLightMapV = lmap->midTexV + (fV - lmap->midPolyV) / 16.0f;

				float uu = (fLightMapU / (float)lmap->w) * lw;
				float vv = (fLightMapV / (float)lmap->h) * lh;

				float pixelStep = 1.0f / (float)LIGHTMAP_ATLAS_SIZE;

				for (int s = 0; s < MAXLIGHTMAPS; s++)
				{
					verts[e].luv[s][0] = uu + lmap->x[s] * pixelStep;
					verts[e].luv[s][1] = vv + lmap->y[s] * pixelStep;
				}
			}
			// set lightmap scales
			for (int s = 0; s < MAXLIGHTMAPS; s++)
			{
				verts[e].luv[s][2] = (hasLighting && face.nStyles[s] != 255) ? 1.0f : 0.0f;
				if (isSpecial && s == 0)
				{
					verts[e].luv[s][2] = 1.0f;
				}
			}
		}

		int idx = 0;


		int wireframeVertCount = face.nEdges * 2;
		lightmapVert* wireframeVerts = new lightmapVert[wireframeVertCount];

		for (int k = 0; k < face.nEdges && (k + 1) % face.nEdges < face.nEdges; k++)
		{
			wireframeVerts[idx++] = verts[k];
			wireframeVerts[idx++] = verts[(k + 1) % face.nEdges];
		}

		for (int k = 0; k < wireframeVertCount; k++)
		{
			wireframeVerts[k].luv[0][2] = 1.0f;
			wireframeVerts[k].luv[1][2] = 0.0f;
			wireframeVerts[k].luv[2][2] = 0.0f;
			wireframeVerts[k].luv[3][2] = 0.0f;
			wireframeVerts[k].r = 1.0f;
			wireframeVerts[k].g = 1.0f;
			wireframeVerts[k].b = 1.0f;
			wireframeVerts[k].a = 1.0f;
		}

		if (!noTriangulate)
		{
			idx = 0;
			// convert TRIANGLE_FAN verts to TRIANGLES so multiple faces can be drawn in a single draw call
			int newCount = face.nEdges + std::max(0, face.nEdges - 3) * 2;
			lightmapVert* newVerts = new lightmapVert[newCount];

			for (int k = 2; k < face.nEdges; k++)
			{
				newVerts[idx++] = verts[0];
				newVerts[idx++] = verts[k - 1];
				newVerts[idx++] = verts[k];
			}

			delete[] verts;
			verts = newVerts;
			vertCount = newCount;
		}


		// add face to a render group (faces that share that same textures and opacity flag)
		bool isTransparent = opacity < 1.0f || (tex && tex->szName[0] == '{');
		int groupIdx = -1;
		for (int k = 0; k < renderGroups.size(); k++)
		{
			if (texinfo.iMiptex == -1)
				continue;
			bool textureMatch = !texturesLoaded || renderGroups[k].texture == glTextures[texinfo.iMiptex];
			if (textureMatch && renderGroups[k].transparent == isTransparent)
			{
				bool allMatch = true;
				for (int s = 0; s < MAXLIGHTMAPS; s++)
				{
					if (renderGroups[k].lightmapAtlas[s] != lightmapAtlas[s])
					{
						allMatch = false;
						break;
					}
				}
				if (allMatch)
				{
					groupIdx = k;
					break;
				}
			}
		}

		// add the verts to a new group if no existing one share the same properties
		if (groupIdx == -1)
		{
			RenderGroup newGroup = RenderGroup();
			newGroup.vertCount = 0;
			newGroup.verts = NULL;
			newGroup.transparent = isTransparent;
			newGroup.special = isSpecial;
			newGroup.texture = texturesLoaded && texinfo.iMiptex != -1 ? glTextures[texinfo.iMiptex] : greyTex;
			for (int s = 0; s < MAXLIGHTMAPS; s++)
			{
				newGroup.lightmapAtlas[s] = lightmapAtlas[s];
			}
			groupIdx = (int)renderGroups.size();
			renderGroups.push_back(newGroup);
			renderGroupVerts.emplace_back(std::vector<lightmapVert>());
			renderGroupWireframeVerts.emplace_back(std::vector<lightmapVert>());
		}

		renderModel->renderFaces[i].group = groupIdx;
		renderModel->renderFaces[i].vertOffset = (int)renderGroupVerts[groupIdx].size();
		renderModel->renderFaces[i].vertCount = vertCount;

		renderGroupVerts[groupIdx].insert(renderGroupVerts[groupIdx].end(), verts, verts + vertCount);
		renderGroupWireframeVerts[groupIdx].insert(renderGroupWireframeVerts[groupIdx].end(), wireframeVerts, wireframeVerts + wireframeVertCount);

		delete[] verts;
		delete[] wireframeVerts;
	}

	renderModel->renderGroups = new RenderGroup[renderGroups.size()];
	renderModel->groupCount = (int)renderGroups.size();

	for (int i = 0; i < renderModel->groupCount; i++)
	{
		renderGroups[i].verts = new lightmapVert[renderGroupVerts[i].size()];
		renderGroups[i].vertCount = (int)renderGroupVerts[i].size();
		memcpy(renderGroups[i].verts, &renderGroupVerts[i][0], renderGroups[i].vertCount * sizeof(lightmapVert));

		renderGroups[i].wireframeVerts = new lightmapVert[renderGroupWireframeVerts[i].size()];
		renderGroups[i].wireframeVertCount = (int)renderGroupWireframeVerts[i].size();
		memcpy(renderGroups[i].wireframeVerts, &renderGroupWireframeVerts[i][0], renderGroups[i].wireframeVertCount * sizeof(lightmapVert));

		auto tmpBuf = renderGroups[i].buffer = new VertexBuffer(activeShader, 0, GL_TRIANGLES);
		tmpBuf->addAttribute(TEX_2F, "vTex");
		tmpBuf->addAttribute(3, GL_FLOAT, 0, "vLightmapTex0");
		tmpBuf->addAttribute(3, GL_FLOAT, 0, "vLightmapTex1");
		tmpBuf->addAttribute(3, GL_FLOAT, 0, "vLightmapTex2");
		tmpBuf->addAttribute(3, GL_FLOAT, 0, "vLightmapTex3");
		tmpBuf->addAttribute(4, GL_FLOAT, 0, "vColor");
		tmpBuf->addAttribute(POS_3F, "vPosition");
		tmpBuf->setData(renderGroups[i].verts, renderGroups[i].vertCount);

		auto tmpWireBuff = renderGroups[i].wireframeBuffer = new VertexBuffer(activeShader, 0, GL_LINES);
		tmpWireBuff->addAttribute(TEX_2F, "vTex");
		tmpWireBuff->addAttribute(3, GL_FLOAT, 0, "vLightmapTex0");
		tmpWireBuff->addAttribute(3, GL_FLOAT, 0, "vLightmapTex1");
		tmpWireBuff->addAttribute(3, GL_FLOAT, 0, "vLightmapTex2");
		tmpWireBuff->addAttribute(3, GL_FLOAT, 0, "vLightmapTex3");
		tmpWireBuff->addAttribute(4, GL_FLOAT, 0, "vColor");
		tmpWireBuff->addAttribute(POS_3F, "vPosition");
		tmpWireBuff->setData(renderGroups[i].wireframeVerts, renderGroups[i].wireframeVertCount);

		renderModel->renderGroups[i] = renderGroups[i];
	}

	for (int i = 0; i < model.nFaces; i++)
	{
		refreshFace(model.iFirstFace + i);
	}

	if (refreshClipnodes)
		generateClipnodeBuffer(modelIdx);

	return renderModel->groupCount;
}

bool BspRenderer::refreshModelClipnodes(int modelIdx)
{
	if (!clipnodesLoaded)
	{
		return false;
	}
	if (modelIdx < 0 || modelIdx >= numRenderClipnodes)
	{
		logf("Bad model idx\n");
		return false;
	}

	deleteRenderModelClipnodes(&renderClipnodes[modelIdx]);
	generateClipnodeBuffer(modelIdx);
	return true;
}

void BspRenderer::loadClipnodes()
{
	numRenderClipnodes = map->modelCount;
	renderClipnodes = new RenderClipnodes[numRenderClipnodes];

	for (int i = 0; i < numRenderClipnodes; i++)
		renderClipnodes[i] = RenderClipnodes();

	if (map)
	{
		for (int i = 0; i < numRenderClipnodes; i++)
		{
			for (int hull = 0; hull < MAX_MAP_HULLS; hull++)
			{
				generateClipnodeBufferForHull(i, hull);
			}
		}
	}
	debugClipnodeVis = false;
}

void BspRenderer::generateClipnodeBufferForHull(int modelIdx, int hullId)
{
	if (debugClipnodeVis && debugLeafIdx.empty())
		return;

	BSPMODEL& model = map->models[modelIdx];
	RenderClipnodes* renderClip = &renderClipnodes[modelIdx];

	Clipper clipper;

	vec3 min = vec3(model.nMins.x, model.nMins.y, model.nMins.z);
	vec3 max = vec3(model.nMaxs.x, model.nMaxs.y, model.nMaxs.z);

	if (renderClip->clipnodeBuffer[hullId])
		delete renderClip->clipnodeBuffer[hullId];
	if (renderClip->wireframeClipnodeBuffer[hullId])
		delete renderClip->wireframeClipnodeBuffer[hullId];

	renderClip->clipnodeBuffer[hullId] = NULL;
	renderClip->wireframeClipnodeBuffer[hullId] = NULL;

	std::vector<NodeVolumeCuts> solidNodes = map->get_model_leaf_volume_cuts(modelIdx, hullId);

	std::vector<CMesh> meshes;
	for (int k = 0; k < solidNodes.size(); k++)
	{
		meshes.emplace_back(clipper.clip(solidNodes[k].cuts));
		clipnodeLeafCount++;
	}

	static COLOR4 hullColors[] = {
		COLOR4(255, 255, 255, 128),
		COLOR4(96, 255, 255, 128),
		COLOR4(255, 96, 255, 128),
		COLOR4(255, 255, 96, 128),
	};
	COLOR4 color = hullColors[hullId];

	std::vector<cVert> allVerts;
	std::vector<cVert> wireframeVerts;
	std::vector<FaceMath> tfaceMaths;

	for (int m = 0; m < meshes.size(); m++)
	{
		CMesh& mesh = meshes[m];

		for (int n = 0; n < mesh.faces.size(); n++)
		{
			if (!mesh.faces[n].visible)
			{
				continue;
			}
			std::set<int> uniqueFaceVerts;

			for (int k = 0; k < mesh.faces[n].edges.size(); k++)
			{
				for (int v = 0; v < 2; v++)
				{
					int vertIdx = mesh.edges[mesh.faces[n].edges[k]].verts[v];
					if (!mesh.verts[vertIdx].visible || uniqueFaceVerts.count(vertIdx))
					{
						continue;
					}
					uniqueFaceVerts.insert(vertIdx);
				}
			}

			std::vector<vec3> faceVerts;
			for (auto vertIdx : uniqueFaceVerts)
			{
				faceVerts.push_back(mesh.verts[vertIdx].pos);
			}

			faceVerts = getSortedPlanarVerts(faceVerts);

			if (faceVerts.size() < 3)
			{
				// logf("Degenerate clipnode face discarded\n");
				continue;
			}

			vec3 normal = getNormalFromVerts(faceVerts);


			if (dotProduct(mesh.faces[n].normal, normal) > 0)
			{
				reverse(faceVerts.begin(), faceVerts.end());
				normal = normal.invert();
			}

			// calculations for face picking
			FaceMath faceMath;
			faceMath.normal = mesh.faces[n].normal;
			faceMath.fdist = getDistAlongAxis(mesh.faces[n].normal, faceVerts[0]);

			vec3 v0 = faceVerts[0];
			vec3 v1;
			bool found = false;
			for (int c = 1; c < faceVerts.size(); c++)
			{
				if (faceVerts[c] != v0)
				{
					v1 = faceVerts[c];
					found = true;
					break;
				}
			}
			if (!found)
			{
				logf("Failed to find non-duplicate vert for clipnode face\n");
			}

			vec3 plane_z = mesh.faces[n].normal;
			vec3 plane_x = (v1 - v0).normalize();
			vec3 plane_y = crossProduct(plane_z, plane_x).normalize();

			faceMath.worldToLocal = worldToLocalTransform(plane_x, plane_y, plane_z);

			faceMath.localVerts = std::vector<vec2>(faceVerts.size());
			for (int k = 0; k < faceVerts.size(); k++)
			{
				faceMath.localVerts[k] = (faceMath.worldToLocal * vec4(faceVerts[k], 1)).xy();
			}

			tfaceMaths.push_back(faceMath);
		// create the verts for rendering
			for (int c = 0; c < faceVerts.size(); c++)
			{
				faceVerts[c] = faceVerts[c].flip();
			}

			COLOR4 wireframeColor = {0, 0, 0, 255};
			for (int k = 0; k < faceVerts.size(); k++)
			{
				wireframeVerts.emplace_back(cVert(faceVerts[k], wireframeColor));
				wireframeVerts.emplace_back(cVert(faceVerts[(k + 1) % faceVerts.size()], wireframeColor));
			}

			vec3 lightDir = vec3(1, 1, -1).normalize();
			float dot = (dotProduct(normal, lightDir) + 1) / 2.0f;
			if (dot > 0.5f)
			{
				dot = dot * dot;
			}

			COLOR4 faceColor = color * (dot);

			bool isVisibled = false;
			if (debugClipnodeVis)
			{
				int faceIdx = map->getFaceFromPlane(mesh.faces[n].planeIdx);
				auto faceLeafs = map->getFaceLeafs(faceIdx);

				if (mesh.faces[n].planeIdx >= 0)
				{
					for (auto leafIdx : debugLeafIdx)
					{
						if (leafIdx.leafIdx < 0)
						{
							continue;
						}

						for (auto fLeaf : faceLeafs)
						{
							if (fLeaf < 0)
								continue;
							if (CHECKVISBIT(leafIdx.leafVIS, fLeaf))
							{
								isVisibled = true;
								break;
							}
						}

						if (isVisibled)
							break;
					}
				}
			}

			if (isVisibled)
			{
				faceColor = COLOR4(255, 0, 0, 150);
			}

			// convert from TRIANGLE_FAN style verts to TRIANGLES
			for (int k = 2; k < faceVerts.size(); k++)
			{
				allVerts.emplace_back(cVert(faceVerts[0], faceColor));
				allVerts.emplace_back(cVert(faceVerts[k - 1], faceColor));
				allVerts.emplace_back(cVert(faceVerts[k], faceColor));
			}

		}
	}

	cVert* output = new cVert[allVerts.size()];
	for (int c = 0; c < allVerts.size(); c++)
	{
		output[c] = allVerts[c];
	}

	cVert* wireOutput = new cVert[wireframeVerts.size()];
	for (int c = 0; c < wireframeVerts.size(); c++)
	{
		wireOutput[c] = wireframeVerts[c];
	}

	renderClip->faceMaths[hullId].clear();

	if (allVerts.empty() || wireframeVerts.empty())
	{
		return;
	}

	renderClip->clipnodeBuffer[hullId] = new VertexBuffer(colorShader, COLOR_4B | POS_3F, output, (GLsizei)allVerts.size(), GL_TRIANGLES);
	renderClip->clipnodeBuffer[hullId]->ownData = true;

	renderClip->wireframeClipnodeBuffer[hullId] = new VertexBuffer(colorShader, COLOR_4B | POS_3F, wireOutput, (GLsizei)wireframeVerts.size(), GL_LINES);
	renderClip->wireframeClipnodeBuffer[hullId]->ownData = true;

	renderClip->faceMaths[hullId] = std::move(tfaceMaths);
}

void BspRenderer::generateClipnodeBuffer(int modelIdx)
{
	if (!map)
		return;

	for (int i = 0; i < MAX_MAP_HULLS; i++)
	{
		generateClipnodeBufferForHull(modelIdx, i);
	}
}

void BspRenderer::updateClipnodeOpacity(unsigned char newValue)
{
	for (int i = 0; i < numRenderClipnodes; i++)
	{
		for (int k = 0; k < MAX_MAP_HULLS; k++)
		{
			VertexBuffer* clipBuf = renderClipnodes[i].clipnodeBuffer[k];
			if (clipBuf && clipBuf->data && clipBuf->numVerts > 0 && clipBuf->attribs.size())
			{
				cVert* vertData = (cVert*)clipBuf->data;
				for (int v = 0; v < clipBuf->numVerts; v++)
				{
					vertData[v].c.a = newValue;
				}
				renderClipnodes[i].clipnodeBuffer[k]->upload();
			}
		}
	}
}

void BspRenderer::preRenderEnts()
{
	if (renderEnts)
	{
		delete[] renderEnts;
	}
	renderEnts = new RenderEnt[map->ents.size()];

	numPointEnts = 0;

	for (int i = 1; i < map->ents.size(); i++)
	{
		numPointEnts += !map->ents[i]->isBspModel();
	}

	for (int i = 0; i < map->ents.size(); i++)
	{
		refreshEnt(i);
	}
}

void BspRenderer::refreshPointEnt(int entIdx)
{
	int skipIdx = 0;

	if (entIdx == 0)
		return;

	// skip worldspawn
	for (size_t i = 1, sz = map->ents.size(); i < sz; i++)
	{
		if (renderEnts[i].modelIdx >= 0)
			continue;

		if (i == entIdx)
		{
			break;
		}

		skipIdx++;
	}

	if (skipIdx >= numPointEnts)
	{
		logf("Failed to update point ent\n");
		return;
	}
}

void BspRenderer::setRenderAngles(int entIdx, vec3 angles)
{
	if (!map->ents[entIdx]->hasKey("classname"))
	{
		renderEnts[entIdx].modelMatAngles.rotateY((angles.y * (PI / 180.0f)));
		renderEnts[entIdx].modelMatAngles.rotateZ(-(angles.x * (PI / 180.0f)));
		renderEnts[entIdx].modelMatAngles.rotateX((angles.z * (PI / 180.0f)));

		renderEnts[entIdx].modelMatAngles2.rotateY((angles.y * (PI / 180.0f)));
		renderEnts[entIdx].modelMatAngles2.rotateZ(-(angles.x * (PI / 180.0f)));
		renderEnts[entIdx].modelMatAngles2.rotateX((angles.z * (PI / 180.0f)));

		renderEnts[entIdx].needAngles = false;
	}
	else
	{
		std::string entClassName = map->ents[entIdx]->keyvalues["classname"];
		// based at cs 1.6 gamedll
		if (entClassName == "func_breakable")
		{
			renderEnts[entIdx].angles.y = 0.0f;
			renderEnts[entIdx].modelMatAngles.rotateY(0.0f);
			renderEnts[entIdx].modelMatAngles.rotateZ(-(angles.x * (PI / 180.0f)));
			renderEnts[entIdx].modelMatAngles.rotateX((angles.z * (PI / 180.0f)));

			renderEnts[entIdx].modelMatAngles2.rotateY(0.0f);
			renderEnts[entIdx].modelMatAngles2.rotateZ(-(angles.x * (PI / 180.0f)));
			renderEnts[entIdx].modelMatAngles2.rotateX((angles.z * (PI / 180.0f)));
		}
		else if (IsEntNotSupportAngles(entClassName))
		{
			renderEnts[entIdx].angles = vec3();
		}
		else if (entClassName == "env_sprite")
		{
			if (abs(angles.y) >= EPSILON && abs(angles.z) < EPSILON)
			{
				renderEnts[entIdx].angles.z = 0.0f;
				renderEnts[entIdx].modelMatAngles.rotateY(0.0);
				renderEnts[entIdx].modelMatAngles.rotateZ(-(angles.x * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles.rotateX((angles.y * (PI / 180.0f)));

				renderEnts[entIdx].modelMatAngles2.rotateY(0.0);
				renderEnts[entIdx].modelMatAngles2.rotateZ(-(angles.x * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles2.rotateX((angles.y * (PI / 180.0f)));
			}
			else
			{
				renderEnts[entIdx].modelMatAngles.rotateY((angles.y * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles.rotateZ(-(angles.x * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles.rotateX((angles.z * (PI / 180.0f)));

				renderEnts[entIdx].modelMatAngles2.rotateY((angles.y * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles2.rotateZ(-(angles.x * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles2.rotateX((angles.z * (PI / 180.0f)));
			}
		}
		else
		{
			bool foundAngles = false;
			for (const auto& prefix : g_settings.entsNegativePitchPrefix)
			{
				if (entClassName.starts_with(prefix))
				{
					renderEnts[entIdx].modelMatAngles.rotateY((angles.y * (PI / 180.0f)));
					renderEnts[entIdx].modelMatAngles.rotateZ((angles.x * (PI / 180.0f)));
					renderEnts[entIdx].modelMatAngles.rotateX((angles.z * (PI / 180.0f)));

					renderEnts[entIdx].modelMatAngles2.rotateY((angles.y * (PI / 180.0f)));
					renderEnts[entIdx].modelMatAngles2.rotateZ((angles.x * (PI / 180.0f)));
					renderEnts[entIdx].modelMatAngles2.rotateX((angles.z * (PI / 180.0f)));
					foundAngles = true;
					break;
				}
			}
			if (!foundAngles)
			{
				renderEnts[entIdx].modelMatAngles.rotateY((angles.y * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles.rotateZ(-(angles.x * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles.rotateX((angles.z * (PI / 180.0f)));


				renderEnts[entIdx].modelMatAngles2.rotateY((angles.y * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles2.rotateZ(-(angles.x * (PI / 180.0f)));
				renderEnts[entIdx].modelMatAngles2.rotateX((angles.z * (PI / 180.0f)));
			}
		}
	}

	if (renderEnts[entIdx].angles != vec3())
	{
		renderEnts[entIdx].needAngles = true;
	}
}

void BspRenderer::refreshEnt(int entIdx)
{
	if (entIdx < 0 || !pointEntRenderer)
		return;
	Entity* ent = map->ents[entIdx];
	BSPMODEL mdl = map->models[ent->getBspModelIdx() > 0 ? ent->getBspModelIdx() : 0];
	renderEnts[entIdx].modelIdx = ent->getBspModelIdx();
	renderEnts[entIdx].modelMatAngles.loadIdentity();
	renderEnts[entIdx].modelMatAngles2.loadIdentity();
	renderEnts[entIdx].modelMatOrigin.loadIdentity();
	renderEnts[entIdx].offset = vec3();
	renderEnts[entIdx].angles = vec3();
	renderEnts[entIdx].needAngles = false;
	renderEnts[entIdx].pointEntCube = pointEntRenderer->getEntCube(ent);
	renderEnts[entIdx].hide = ent->hide;
	bool setAngles = false;

	if (ent->hasKey("origin"))
	{
		vec3 origin = parseVector(ent->keyvalues["origin"]);
		renderEnts[entIdx].modelMatAngles.translate(origin.x, origin.z, -origin.y);
		renderEnts[entIdx].modelMatAngles2 = renderEnts[entIdx].modelMatOrigin = renderEnts[entIdx].modelMatAngles;
		renderEnts[entIdx].offset = origin;
	}

	if (!ent->isBspModel())
	{
		if (ent->hasKey("model") || g_app->fgd)
		{
			std::string modelpath = std::string();

			if (ent->hasKey("model") && ent->keyvalues["model"].size())
			{
				modelpath = ent->keyvalues["model"];
			}

			if (g_app->fgd && modelpath.empty())
			{
				FgdClass* fgdClass = g_app->fgd->getFgdClass(ent->keyvalues["classname"]);
				if (fgdClass && !fgdClass->model.empty())
				{
					modelpath = fgdClass->model;
				}
			}

			if (renderEnts[entIdx].mdlFileName.size() && !modelpath.size() || renderEnts[entIdx].mdlFileName != modelpath)
			{
				renderEnts[entIdx].mdlFileName = modelpath;
				std::string lowerpath = toLowerCase(modelpath);
				std::string newModelPath;
				if (lowerpath.ends_with(".mdl"))
				{
					if (FindPathInAssets(modelpath, newModelPath))
					{
						if (renderEnts[entIdx].mdl)
						{
							delete renderEnts[entIdx].mdl;
							renderEnts[entIdx].mdl = NULL;
						}
						renderEnts[entIdx].mdl = new StudioModel(newModelPath);
						renderEnts[entIdx].mdl->UpdateModelMeshList();
					}
					else
					{
						if (modelpath.size())
							FindPathInAssets(modelpath, newModelPath, true);
						if (renderEnts[entIdx].mdl)
						{
							delete renderEnts[entIdx].mdl;
							renderEnts[entIdx].mdl = NULL;
						}
					}
				}
				else
				{
					if (renderEnts[entIdx].mdl)
					{
						delete renderEnts[entIdx].mdl;
						renderEnts[entIdx].mdl = NULL;
					}
				}
			}
		}
	}

	for (unsigned int i = 0; i < ent->keyOrder.size(); i++)
	{
		if (ent->keyOrder[i] == "angles")
		{
			setAngles = true;
			renderEnts[entIdx].angles = parseVector(ent->keyvalues["angles"]);
		}
		if (ent->keyOrder[i] == "angle")
		{
			setAngles = true;
			float y = (float)atof(ent->keyvalues["angle"].c_str());

			if (y >= 0.0f)
			{
				renderEnts[entIdx].angles.y = y;
			}
			else if ((int)y == -1)
			{
				renderEnts[entIdx].angles.x = -90.0f;
				renderEnts[entIdx].angles.y = 0.0f;
				renderEnts[entIdx].angles.z = 0.0f;
			}
			else if ((int)y <= -2)
			{
				renderEnts[entIdx].angles.x = 90.0f;
				renderEnts[entIdx].angles.y = 0.0f;
				renderEnts[entIdx].angles.z = 0.0f;
			}

		}
	}

	if (ent->hasKey("sequence") || g_app->fgd)
	{
		int sequence = 0;
		if (ent->hasKey("sequence") && isNumeric(ent->keyvalues["sequence"]))
		{
			sequence = atoi(ent->keyvalues["sequence"].c_str());
		}
		if (sequence <= 0 && g_app->fgd)
		{
			FgdClass* fgdClass = g_app->fgd->getFgdClass(ent->keyvalues["classname"]);
			if (fgdClass)
			{
				sequence = fgdClass->modelSequence;
			}
		}

		if (renderEnts[entIdx].mdl)
		{
			renderEnts[entIdx].mdl->SetSequence(sequence);
		}
	}

	if (ent->hasKey("skin") || g_app->fgd)
	{
		int skin = 0;
		if (ent->hasKey("skin") && isNumeric(ent->keyvalues["skin"]))
		{
			skin = atoi(ent->keyvalues["skin"].c_str());
		}
		if (skin <= 0 && g_app->fgd)
		{
			FgdClass* fgdClass = g_app->fgd->getFgdClass(ent->keyvalues["classname"]);
			if (fgdClass)
			{
				skin = fgdClass->modelSkin;
			}
		}

		if (renderEnts[entIdx].mdl)
		{
			renderEnts[entIdx].mdl->SetSkin(skin);
		}
	}

	if (ent->hasKey("body") || g_app->fgd)
	{
		int body = 0;
		if (ent->hasKey("body") && isNumeric(ent->keyvalues["body"]))
		{
			body = atoi(ent->keyvalues["body"].c_str());
		}
		if (body == 0 && g_app->fgd)
		{
			FgdClass* fgdClass = g_app->fgd->getFgdClass(ent->keyvalues["classname"]);
			if (fgdClass)
			{
				body = fgdClass->modelBody;
			}
		}

		if (renderEnts[entIdx].mdl && renderEnts[entIdx].mdl->m_pstudiohdr)
		{
			auto* pbodypart = (mstudiobodyparts_t*)((unsigned char*)renderEnts[entIdx].mdl->m_pstudiohdr + renderEnts[entIdx].mdl->m_pstudiohdr->bodypartindex);
			for (int bg = 0; bg < renderEnts[entIdx].mdl->m_pstudiohdr->numbodyparts; bg++)
			{
				renderEnts[entIdx].mdl->SetBodygroup(bg, body % pbodypart->nummodels);
				body /= pbodypart->nummodels;
				pbodypart++;
			}

		}
	}


	if (setAngles)
	{
		setRenderAngles(entIdx, renderEnts[entIdx].angles);
	}
}

void BspRenderer::calcFaceMaths()
{
	deleteFaceMaths();

	numFaceMaths = map->faceCount;
	faceMaths = new FaceMath[map->faceCount];

	vec3 world_x = vec3(1, 0, 0);
	vec3 world_y = vec3(0, 1, 0);
	vec3 world_z = vec3(0, 0, 1);

	for (int i = 0; i < map->faceCount; i++)
	{
		refreshFace(i);
	}
}

void BspRenderer::refreshFace(int faceIdx)
{
	const vec3 world_x = vec3(1, 0, 0);
	const vec3 world_y = vec3(0, 1, 0);
	const vec3 world_z = vec3(0, 0, 1);

	FaceMath& faceMath = faceMaths[faceIdx];
	BSPFACE& face = map->faces[faceIdx];
	BSPPLANE& plane = map->planes[face.iPlane];
	vec3 planeNormal = face.nPlaneSide ? plane.vNormal * -1 : plane.vNormal;
	float fDist = face.nPlaneSide ? -plane.fDist : plane.fDist;

	faceMath.normal = planeNormal;
	faceMath.fdist = fDist;

	std::vector<vec3> allVerts(face.nEdges);
	vec3 v1;
	for (int e = 0; e < face.nEdges; e++)
	{
		int edgeIdx = map->surfedges[face.iFirstEdge + e];
		BSPEDGE& edge = map->edges[abs(edgeIdx)];
		int vertIdx = edgeIdx < 0 ? edge.iVertex[1] : edge.iVertex[0];
		allVerts[e] = map->verts[vertIdx];

		// 2 verts can share the same position on a face, so need to find one that isn't shared (aomdc_1intro)
		if (e > 0 && allVerts[e] != allVerts[0])
		{
			v1 = allVerts[e];
		}
	}
	if (allVerts.size() == 0)
	{
		allVerts.emplace_back(vec3());
	}
	vec3 plane_x = (v1 - allVerts[0]).normalize(1.0f);
	vec3 plane_y = crossProduct(planeNormal, plane_x).normalize(1.0f);
	vec3 plane_z = planeNormal;

	faceMath.worldToLocal = worldToLocalTransform(plane_x, plane_y, plane_z);

	faceMath.localVerts = std::vector<vec2>(allVerts.size());
	for (int i = 0; i < allVerts.size(); i++)
	{
		faceMath.localVerts[i] = (faceMath.worldToLocal * vec4(allVerts[i], 1)).xy();
	}
}

BspRenderer::~BspRenderer()
{
	clearUndoCommands();
	clearRedoCommands();

	if (lightmapFuture.valid() && lightmapFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready ||
		texturesFuture.valid() && texturesFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready ||
		clipnodesFuture.valid() && clipnodesFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
	{
		logf("ERROR: Deleted bsp renderer while it was loading\n");
	}

	for (int i = 0; i < wads.size(); i++)
	{
		delete wads[i];
	}
	wads.clear();

	if (lightmaps)
	{
		delete[] lightmaps;
	}
	if (renderEnts)
	{
		delete[] renderEnts;
	}

	deleteTextures();
	deleteLightmapTextures();
	deleteRenderFaces();
	deleteRenderClipnodes();
	deleteFaceMaths();

	// TODO: share these with all renderers
	delete whiteTex;
	delete redTex;
	delete yellowTex;
	delete greyTex;
	delete blackTex;
	delete blueTex;
	delete missingTex;
	map->setBspRender(NULL);
	delete map;
	map = NULL;

}

void BspRenderer::reuploadTextures()
{
	deleteTextures();

	loadTextures();

	glTextures = glTexturesSwap;

	for (int i = 0; i < map->textureCount; i++)
	{
		glTextures[i]->upload(GL_RGB);
	}

	numLoadedTextures = map->textureCount;

	texturesLoaded = true;

	preRenderFaces();

	needReloadDebugTextures = true;
}

void BspRenderer::delayLoadData()
{
	if (!lightmapsUploaded && lightmapFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		for (int i = 0; i < numLightmapAtlases; i++)
		{
			glLightmapTextures[i]->upload(GL_RGB);
		}

		lightmapsGenerated = true;

		preRenderFaces();

		lightmapsUploaded = true;
	}

	if (!texturesLoaded && texturesFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		reuploadTextures();
	}

	if (!clipnodesLoaded && clipnodesFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		for (int i = 0; i < numRenderClipnodes; i++)
		{
			RenderClipnodes& clip = renderClipnodes[i];
			for (int k = 0; k < MAX_MAP_HULLS; k++)
			{
				if (clip.clipnodeBuffer[k])
				{
					clip.clipnodeBuffer[k]->bindAttributes(true);
					clip.clipnodeBuffer[k]->upload();
				}
			}
		}

		clipnodesLoaded = true;
		logf("Loaded {} clipnode leaves\n", clipnodeLeafCount);
		updateClipnodeOpacity((g_render_flags & RENDER_TRANSPARENT) ? 128 : 255);
	}
}

bool BspRenderer::isFinishedLoading()
{
	return lightmapsUploaded && texturesLoaded && clipnodesLoaded;
}

void BspRenderer::highlightFace(int faceIdx, bool highlight, COLOR4 color, bool useColor, bool reupload)
{
	RenderFace* rface;
	RenderGroup* rgroup;
	if (!getRenderPointers(faceIdx, &rface, &rgroup))
	{
		logf("Bad face index\n");
		return;
	}

	float r, g, b;
	r = g = b = 1.0f;

	if (highlight)
	{
		r = 0.86f;
		g = 0;
		b = 0;
	}

	if (useColor)
	{
		r = color.r / 255.0f;
		g = color.g / 255.0f;
		b = color.b / 255.0f;
	}

	for (int i = 0; i < rface->vertCount; i++)
	{
		rgroup->verts[rface->vertOffset + i].r = r;
		rgroup->verts[rface->vertOffset + i].g = g;
		rgroup->verts[rface->vertOffset + i].b = b;
	}
	if (reupload)
		rgroup->buffer->upload();
}

void BspRenderer::updateFaceUVs(int faceIdx)
{
	RenderFace* rface;
	RenderGroup* rgroup;
	if (!getRenderPointers(faceIdx, &rface, &rgroup))
	{
		logf("Bad face index\n");
		return;
	}

	BSPFACE& face = map->faces[faceIdx];
	BSPTEXTUREINFO& texinfo = map->texinfos[face.iTextureInfo];
	int texOffset = ((int*)map->textures)[texinfo.iMiptex + 1];
	if (texOffset != -1 && texinfo.iMiptex != -1)
	{
		BSPMIPTEX& tex = *((BSPMIPTEX*)(map->textures + texOffset));

		for (int i = 0; i < rface->vertCount; i++)
		{
			lightmapVert& vert = rgroup->verts[rface->vertOffset + i];
			vec3 pos = vert.pos.flipUV();

			float tw = 1.0f / (float)tex.nWidth;
			float th = 1.0f / (float)tex.nHeight;
			float fU = dotProduct(texinfo.vS, pos) + texinfo.shiftS;
			float fV = dotProduct(texinfo.vT, pos) + texinfo.shiftT;
			vert.u = fU * tw;
			vert.v = fV * th;
		}
	}

	rgroup->buffer->upload();
}

bool BspRenderer::getRenderPointers(int faceIdx, RenderFace** renderFace, RenderGroup** renderGroup)
{
	int modelIdx = map->get_model_from_face(faceIdx);

	if (modelIdx == -1)
	{
		return false;
	}

	int relativeFaceIdx = faceIdx - map->models[modelIdx].iFirstFace;
	*renderFace = &renderModels[modelIdx].renderFaces[relativeFaceIdx];
	*renderGroup = &renderModels[modelIdx].renderGroups[(*renderFace)->group];

	return true;
}

unsigned int BspRenderer::getFaceTextureId(int faceIdx)
{
	BSPFACE& face = map->faces[faceIdx];
	BSPTEXTUREINFO& texinfo = map->texinfos[face.iTextureInfo];
	if (texinfo.iMiptex == -1)
		return 0;
	return glTextures[texinfo.iMiptex]->id;
}


void BspRenderer::render(std::vector<int> highlightEnts, bool highlightAlwaysOnTop, int clipnodeHull)
{
	ShaderProgram* activeShader; vec3 renderOffset;
	mapOffset = map->ents.size() ? map->ents[0]->getOrigin() : vec3();
	renderOffset = mapOffset.flip();

	activeShader = (g_render_flags & RENDER_LIGHTMAPS) ? bspShader : fullBrightBspShader;

	activeShader->bind();
	activeShader->modelMat->loadIdentity();
	activeShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);
	activeShader->updateMatrixes();

	//for (size_t i = 0; i < map->ents.size(); i++)
	//{
	//	if (renderEnts[i].mdl && renderEnts[i].mdl->mdl_mesh_groups.size())
	//	{
	//		renderEnts[i].mdl->AdvanceFrame(g_app->curTime - g_app->oldTime);
	//	}
	//}

	// draw highlighted ent first so other ent edges don't overlap the highlighted edges
	if (highlightEnts.size() && !highlightAlwaysOnTop)
	{
		activeShader->bind();

		for (int highlightEnt : highlightEnts)
		{
			if (highlightEnt > 0 && renderEnts[highlightEnt].modelIdx >= 0 && renderEnts[highlightEnt].modelIdx < map->modelCount)
			{
				if (renderEnts[highlightEnt].hide)
					continue;
				activeShader->pushMatrix(MAT_MODEL);
				*activeShader->modelMat = renderEnts[highlightEnt].modelMatAngles;
				activeShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);
				activeShader->updateMatrixes();

				drawModel(&renderEnts[highlightEnt], false, true, true);
				drawModel(&renderEnts[highlightEnt], true, true, true);

				activeShader->popMatrix(MAT_MODEL);
			}
		}
	}

	for (int pass = 0; pass < 2; pass++)
	{
		bool drawTransparentFaces = pass == 1;

		drawModel(0, drawTransparentFaces, false, false);

		for (int i = 0, sz = (int)map->ents.size(); i < sz; i++)
		{
			if (renderEnts[i].modelIdx >= 0 && renderEnts[i].modelIdx < map->modelCount)
			{
				if (renderEnts[i].hide)
					continue;
				activeShader->pushMatrix(MAT_MODEL);
				*activeShader->modelMat = renderEnts[i].modelMatAngles;
				activeShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);
				activeShader->updateMatrixes();

				drawModel(&renderEnts[i], drawTransparentFaces, g_app->pickInfo.IsSelectedEnt(i), false);

				activeShader->popMatrix(MAT_MODEL);
			}
		}

		if ((g_render_flags & RENDER_POINT_ENTS) && pass == 0)
		{
			drawPointEntities(highlightEnts);
			activeShader->bind();
		}
	}

	if (clipnodesLoaded)
	{
		colorShader->bind();

		if (g_render_flags & RENDER_WORLD_CLIPNODES && clipnodeHull != -1)
		{
			drawModelClipnodes(0, false, clipnodeHull);
		}

		if (g_render_flags & RENDER_ENT_CLIPNODES)
		{
			for (int i = 0, sz = (int)map->ents.size(); i < sz; i++)
			{
				if (renderEnts[i].modelIdx >= 0 && renderEnts[i].modelIdx < map->modelCount)
				{
					if (renderEnts[i].hide)
						continue;
					if (clipnodeHull == -1 && renderModels[renderEnts[i].modelIdx].groupCount > 0)
					{
						continue; // skip rendering for models that have faces, if in auto mode
					}
					colorShader->pushMatrix(MAT_MODEL);
					*colorShader->modelMat = renderEnts[i].modelMatAngles;
					colorShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);
					colorShader->updateMatrixes();

					bool hightlighted = g_app->pickInfo.IsSelectedEnt(i);

					if (hightlighted)
					{
						glUniform4f(colorShaderMultId, 1, 0.25f, 0.25f, 1);
					}

					drawModelClipnodes(renderEnts[i].modelIdx, false, clipnodeHull);

					if (hightlighted)
					{
						glUniform4f(colorShaderMultId, 1, 1, 1, 1);
					}

					colorShader->popMatrix(MAT_MODEL);
				}
			}
		}
	}

	if (highlightEnts.size() && highlightAlwaysOnTop)
	{
		activeShader->bind();

		for (int highlightEnt : highlightEnts)
		{
			if (highlightEnt > 0 && renderEnts[highlightEnt].modelIdx >= 0 && renderEnts[highlightEnt].modelIdx < map->modelCount)
			{
				if (renderEnts[highlightEnt].hide)
					continue;
				activeShader->pushMatrix(MAT_MODEL);
				*activeShader->modelMat = renderEnts[highlightEnt].modelMatAngles;
				activeShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);
				activeShader->updateMatrixes();

				glDisable(GL_DEPTH_TEST);
				drawModel(&renderEnts[highlightEnt], false, true, true);
				drawModel(&renderEnts[highlightEnt], true, true, true);
				glEnable(GL_DEPTH_TEST);

				activeShader->popMatrix(MAT_MODEL);
			}
		}
	}

	delayLoadData();
}

void BspRenderer::drawModel(RenderEnt* ent, bool transparent, bool highlight, bool edgesOnly)
{
	ShaderProgram* activeShader; vec3 renderOffset;
	mapOffset = map->ents.size() ? map->ents[0]->getOrigin() : vec3();
	renderOffset = mapOffset.flip();

	activeShader = (g_render_flags & RENDER_LIGHTMAPS) ? bspShader : fullBrightBspShader;

	int modelIdx = ent ? ent->modelIdx : 0;

	if (modelIdx < 0 || modelIdx >= numRenderModels)
	{
		return;
	}

	if (edgesOnly)
	{
		for (int i = 0; i < renderModels[modelIdx].groupCount; i++)
		{
			RenderGroup& rgroup = renderModels[modelIdx].renderGroups[i];

			if (highlight)
				yellowTex->bind(0);
			else
			{
				if (modelIdx > 0)
					blueTex->bind(0);
				else
					greyTex->bind(0);
			}

			whiteTex->bind(1);

			rgroup.wireframeBuffer->drawFull();
		}
		return;
	}

	for (int i = 0; i < renderModels[modelIdx].groupCount; i++)
	{
		RenderGroup& rgroup = renderModels[modelIdx].renderGroups[i];

		if (rgroup.transparent != transparent)
			continue;

		if (rgroup.special)
		{
			if (modelIdx == 0 && !(g_render_flags & RENDER_SPECIAL))
			{
				continue;
			}
			else if (modelIdx != 0 && !(g_render_flags & RENDER_SPECIAL_ENTS))
			{
				continue;
			}
		}
		else if (modelIdx != 0 && !(g_render_flags & RENDER_ENTS))
		{
			continue;
		}

		if (ent && ent->needAngles)
		{
			activeShader->pushMatrix(MAT_MODEL);
			*activeShader->modelMat = ent->modelMatOrigin;
			activeShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);
			activeShader->updateMatrixes();
			yellowTex->bind(0);
			greyTex->bind(1);
			rgroup.wireframeBuffer->drawFull();
			activeShader->popMatrix(MAT_MODEL);
		}

		if (highlight || (g_render_flags & RENDER_WIREFRAME))
		{
			if (highlight)
				yellowTex->bind(0);
			else
			{
				if (modelIdx > 0)
					blueTex->bind(0);
				else
					greyTex->bind(0);
			}
			whiteTex->bind(1);

			rgroup.wireframeBuffer->drawFull();
		}


		if (texturesLoaded && g_render_flags & RENDER_TEXTURES)
		{
			rgroup.texture->bind(0);
		}
		else
		{
			whiteTex->bind(0);
		}

		if (g_render_flags & RENDER_LIGHTMAPS)
		{
			for (int s = 0; s < MAXLIGHTMAPS; s++)
			{
				if (highlight)
				{
					redTex->bind(s + 1);
				}
				else if (lightmapsUploaded)
				{
					if (showLightFlag != -1)
					{
						if (showLightFlag == s)
						{
							blackTex->bind(s + 1);
							continue;
						}
					}
					rgroup.lightmapAtlas[s]->bind(s + 1);
				}
				else
				{
					if (s == 0)
					{
						greyTex->bind(s + 1);
					}
					else
					{
						blackTex->bind(s + 1);
					}
				}
			}
		}
		/*if (!ent)
		{
			//test
			tempmodel->mdl_meshes[0][0].tex->bind(0);
			whiteTex->bind(1);
			whiteTex->bind(2);
			whiteTex->bind(3);
			whiteTex->bind(4);
			tempmodelBuff->drawFull();
		}
		else*/
		rgroup.buffer->drawFull();

		if (ent)
		{
			for (int s = 0; s < MAXLIGHTMAPS; s++)
			{
				whiteTex->bind(s + 1);
			}

			activeShader->pushMatrix(MAT_MODEL);
			*activeShader->modelMat = ent->modelMatOrigin;
			activeShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);
			activeShader->updateMatrixes();
			glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_SRC_COLOR);
			rgroup.buffer->drawFull();
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			activeShader->popMatrix(MAT_MODEL);
		}
	}
}

void BspRenderer::drawModelClipnodes(int modelIdx, bool highlight, int hullIdx)
{
	RenderClipnodes& clip = renderClipnodes[modelIdx];

	if (hullIdx == -1)
	{
		hullIdx = getBestClipnodeHull(modelIdx);
		if (hullIdx == -1)
		{
			return; // nothing can be drawn
		}
	}

	if (clip.clipnodeBuffer[hullIdx])
	{
		clip.clipnodeBuffer[hullIdx]->drawFull();
		clip.wireframeClipnodeBuffer[hullIdx]->drawFull();
	}
}

void BspRenderer::drawPointEntities(std::vector<int> highlightEnts)
{
	ShaderProgram* activeShader; vec3 renderOffset;
	mapOffset = map->ents.size() ? map->ents[0]->getOrigin() : vec3();
	renderOffset = mapOffset.flip();
	activeShader = (g_render_flags & RENDER_LIGHTMAPS) ? bspShader : fullBrightBspShader;

	// skip worldspawn
	colorShader->pushMatrix(MAT_MODEL);
	fullBrightBspShader->pushMatrix(MAT_MODEL);

	for (int i = 1, sz = (int)map->ents.size(); i < sz; i++)
	{
		if (renderEnts[i].modelIdx >= 0)
			continue;
		if (renderEnts[i].hide)
			continue;

		if (g_app->pickInfo.IsSelectedEnt(i))
		{
			if ((g_render_flags & RENDER_MODELS) && renderEnts[i].mdl && renderEnts[i].mdl->mdl_mesh_groups.size())
			{
				fullBrightBspShader->bind();

				*fullBrightBspShader->modelMat = renderEnts[i].modelMatAngles;
				fullBrightBspShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);

				fullBrightBspShader->updateMatrixes();

				renderEnts[i].mdl->DrawModel();

				colorShader->bind();

				*colorShader->modelMat = renderEnts[i].modelMatAngles;
				colorShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);

				colorShader->updateMatrixes();

				renderEnts[i].pointEntCube->wireframeBuffer->drawFull();
			}
			else
			{
				colorShader->bind();

				*colorShader->modelMat = renderEnts[i].modelMatAngles;
				colorShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);

				colorShader->updateMatrixes();

				renderEnts[i].pointEntCube->selectBuffer->drawFull();
				renderEnts[i].pointEntCube->wireframeBuffer->drawFull();
			}
		}
		else
		{
			if ((g_render_flags & RENDER_MODELS) && renderEnts[i].mdl && renderEnts[i].mdl->mdl_mesh_groups.size())
			{
				*fullBrightBspShader->modelMat = renderEnts[i].modelMatAngles;
				fullBrightBspShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);

				fullBrightBspShader->updateMatrixes();
				renderEnts[i].mdl->DrawModel();
			}
			else
			{
				colorShader->bind();

				*colorShader->modelMat = renderEnts[i].modelMatAngles;
				colorShader->modelMat->translate(renderOffset.x, renderOffset.y, renderOffset.z);

				colorShader->updateMatrixes();

				renderEnts[i].pointEntCube->buffer->drawFull();
			}
			//renderEnts[i].pointEntCube->wireframeBuffer->drawFull();
		}
	}

	colorShader->popMatrix(MAT_MODEL);
}

bool BspRenderer::pickPoly(vec3 start, const vec3& dir, int hullIdx, PickInfo& tempPickInfo, Bsp** tmpMap)
{
	bool foundBetterPick = false;

	if (!map || map->ents.empty())
	{
		return foundBetterPick;
	}

	int sz = (int)map->ents.size();

	start -= mapOffset;

	if (pickModelPoly(start, dir, vec3(), 0, hullIdx, tempPickInfo))
	{
		if (*tmpMap || *tmpMap == map)
		{
			tempPickInfo.selectedEnts.clear();
			tempPickInfo.selectedEnts.push_back(0);
			*tmpMap = map;
			foundBetterPick = true;
		}
	}

	for (int i = 0; i < sz; i++)
	{
		if (renderEnts[i].hide)
			continue;
		if (renderEnts[i].modelIdx >= 0 && renderEnts[i].modelIdx < map->modelCount)
		{
			bool isSpecial = false;
			for (int k = 0; k < renderModels[renderEnts[i].modelIdx].groupCount; k++)
			{
				if (renderModels[renderEnts[i].modelIdx].renderGroups[k].special)
				{
					isSpecial = true;
					break;
				}
			}

			if (isSpecial && !(g_render_flags & RENDER_SPECIAL_ENTS))
			{
				continue;
			}
			else if (!isSpecial && !(g_render_flags & RENDER_ENTS))
			{
				continue;
			}

			if (pickModelPoly(start, dir, renderEnts[i].offset, renderEnts[i].modelIdx, hullIdx, tempPickInfo))
			{
				if (!*tmpMap || *tmpMap == map)
				{
					tempPickInfo.selectedEnts.clear();
					tempPickInfo.selectedEnts.push_back(i);
					*tmpMap = map;
					foundBetterPick = true;
				}
			}
		}
		else if (i > 0 && g_render_flags & RENDER_POINT_ENTS)
		{
			vec3 mins = renderEnts[i].offset + renderEnts[i].pointEntCube->mins;
			vec3 maxs = renderEnts[i].offset + renderEnts[i].pointEntCube->maxs;
			if (pickAABB(start, dir, mins, maxs, tempPickInfo.bestDist))
			{
				if (!*tmpMap || *tmpMap == map)
				{
					tempPickInfo.selectedEnts.clear();
					tempPickInfo.selectedEnts.push_back(i);
					*tmpMap = map;
					foundBetterPick = true;
				}
			};
		}
	}

	return foundBetterPick;
}

bool BspRenderer::pickModelPoly(vec3 start, const vec3& dir, vec3 offset, int modelIdx, int hullIdx, PickInfo& tempPickInfo)
{
	if (map->modelCount <= 0)
		return false;

	BSPMODEL& model = map->models[modelIdx];

	start -= offset;

	bool foundBetterPick = false;
	bool skipSpecial = !(g_render_flags & RENDER_SPECIAL);

	for (int k = 0; k < model.nFaces; k++)
	{
		FaceMath& faceMath = faceMaths[model.iFirstFace + k];
		BSPFACE& face = map->faces[model.iFirstFace + k];

		if (skipSpecial && modelIdx == 0)
		{
			BSPTEXTUREINFO& info = map->texinfos[face.iTextureInfo];
			if (info.nFlags & TEX_SPECIAL)
			{
				continue;
			}
		}

		float t = tempPickInfo.bestDist;
		if (pickFaceMath(start, dir, faceMath, t))
		{
			foundBetterPick = true;
			tempPickInfo.bestDist = t;
			tempPickInfo.selectedFaces.clear();
			tempPickInfo.selectedFaces.push_back(model.iFirstFace + k);
		}
	}

	bool selectWorldClips = modelIdx == 0 && (g_render_flags & RENDER_WORLD_CLIPNODES) && hullIdx != -1;
	bool selectEntClips = modelIdx > 0 && (g_render_flags & RENDER_ENT_CLIPNODES);

	if (hullIdx == -1 && renderModels[modelIdx].groupCount == 0)
	{
// clipnodes are visible for this model because it has no faces
		hullIdx = getBestClipnodeHull(modelIdx);
	}

	if (clipnodesLoaded && (selectWorldClips || selectEntClips) && hullIdx != -1)
	{
		for (int i = 0; i < renderClipnodes[modelIdx].faceMaths[hullIdx].size(); i++)
		{
			FaceMath& faceMath = renderClipnodes[modelIdx].faceMaths[hullIdx][i];

			float t = tempPickInfo.bestDist;
			if (pickFaceMath(start, dir, faceMath, t))
			{
				foundBetterPick = true;
				tempPickInfo.bestDist = t;
				tempPickInfo.selectedFaces.clear();
			}
		}
	}

	return foundBetterPick;
}

bool BspRenderer::pickFaceMath(const vec3& start, const vec3& dir, FaceMath& faceMath, float& bestDist)
{
	float dot = dotProduct(dir, faceMath.normal);
	if (dot >= 0)
	{
		return false; // don't select backfaces or parallel faces
	}

	float t = dotProduct((faceMath.normal * faceMath.fdist) - start, faceMath.normal) / dot;
	if (t < EPSILON || t >= bestDist)
	{
		return false; // intersection behind camera, or not a better pick
	}

	// transform intersection point to the plane's coordinate system
	vec3 intersection = start + dir * t;
	vec2 localRayPoint = (faceMath.worldToLocal * vec4(intersection, 1)).xy();

	// check if point is inside the polygon using the plane's 2D coordinate system
	if (!pointInsidePolygon(faceMath.localVerts, localRayPoint))
	{
		return false;
	}

	bestDist = t;
	g_app->debugVec0 = intersection;

	return true;
}

int BspRenderer::getBestClipnodeHull(int modelIdx)
{
	if (!clipnodesLoaded)
	{
		return -1;
	}

	RenderClipnodes& clip = renderClipnodes[modelIdx];

	// prefer hull that most closely matches the object size from a player's perspective
	if (clip.clipnodeBuffer[0])
	{
		return 0;
	}
	else if (clip.clipnodeBuffer[3])
	{
		return 3;
	}
	else if (clip.clipnodeBuffer[1])
	{
		return 1;
	}
	else if (clip.clipnodeBuffer[2])
	{
		return 2;
	}

	return -1;
}


void BspRenderer::updateEntityState(int entIdx)
{
	if (entIdx < 0)
		return;

	undoEntityState[entIdx] = *map->ents[entIdx];
}

void BspRenderer::saveLumpState(int targetLumps, bool deleteOldState)
{
	if (deleteOldState)
	{
		for (int i = 0; i < HEADER_LUMPS; i++)
		{
			if (undoLumpState.lumps[i])
				delete[] undoLumpState.lumps[i];
		}
	}

	undoLumpState = map->duplicate_lumps(targetLumps);
}

void BspRenderer::pushEntityUndoState(const std::string& actionDesc, int entIdx)
{
	if (entIdx < 0)
	{
		logf("Invalid entity undo state push[No ent id]\n");
		return;
	}

	Entity* ent = map->ents[entIdx];

	if (!ent)
	{
		logf("Invalid entity undo state push[No ent]\n");
		return;
	}

	bool anythingToUndo = true;
	if (undoEntityState[entIdx].keyOrder.size() == ent->keyOrder.size())
	{
		bool keyvaluesDifferent = false;
		for (int i = 0; i < undoEntityState[entIdx].keyOrder.size(); i++)
		{
			std::string oldKey = undoEntityState[entIdx].keyOrder[i];
			std::string newKey = ent->keyOrder[i];
			if (oldKey != newKey)
			{
				keyvaluesDifferent = true;
				break;
			}
			std::string oldVal = undoEntityState[entIdx].keyvalues[oldKey];
			std::string newVal = ent->keyvalues[oldKey];
			if (oldVal != newVal)
			{
				keyvaluesDifferent = true;
				break;
			}
		}

		anythingToUndo = keyvaluesDifferent;
	}

	if (!anythingToUndo)
	{
		logf("Invalid entity undo state push[No changes]\n");
		return; // nothing to undo
	}

	pushUndoCommand(new EditEntityCommand(actionDesc, entIdx, undoEntityState[entIdx], *ent));
	updateEntityState(entIdx);
}

void BspRenderer::pushModelUndoState(const std::string& actionDesc, unsigned int targetLumps)
{
	if (!map)
	{
		logf("Impossible, no map\n");
		return;
	}

	int entIdx = g_app->pickInfo.GetSelectedEnt();
	if (entIdx < 0 && g_app->pickInfo.selectedFaces.size())
	{
		int modelIdx = map->get_model_from_face(g_app->pickInfo.selectedFaces[0]);
		entIdx = map->get_ent_from_model(modelIdx);
	}
	if (entIdx < 0)
		entIdx = 0;

	LumpState newLumps = map->duplicate_lumps(targetLumps);

	bool differences[HEADER_LUMPS] = {false};

	bool anyDifference = false;
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (newLumps.lumps[i] && undoLumpState.lumps[i])
		{
			if (newLumps.lumpLen[i] != undoLumpState.lumpLen[i] || memcmp(newLumps.lumps[i], undoLumpState.lumps[i], newLumps.lumpLen[i]) != 0)
			{
				anyDifference = true;
				differences[i] = true;
			}
		}
	}

	if (!anyDifference)
	{
		logf("No differences detected\n");
		return;
	}

	// delete lumps that have no differences to save space
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		if (!differences[i])
		{
			delete[] undoLumpState.lumps[i];
			delete[] newLumps.lumps[i];
			undoLumpState.lumps[i] = newLumps.lumps[i] = NULL;
			undoLumpState.lumpLen[i] = newLumps.lumpLen[i] = 0;
		}
	}

	EditBspModelCommand* editCommand = new EditBspModelCommand(actionDesc, entIdx, undoLumpState, newLumps, undoEntityState[entIdx].getOrigin());
	pushUndoCommand(editCommand);
	saveLumpState(0xffffffff, false);

	// entity origin edits also update the ent origin (TODO: this breaks when moving + scaling something)
	updateEntityState(entIdx);
}

void BspRenderer::pushUndoCommand(Command* cmd)
{
	undoHistory.push_back(cmd);
	clearRedoCommands();

	while (!undoHistory.empty() && undoHistory.size() > g_settings.undoLevels)
	{
		delete undoHistory[0];
		undoHistory.erase(undoHistory.begin());
	}

	calcUndoMemoryUsage();
}

void BspRenderer::undo()
{
	if (undoHistory.empty())
	{
		return;
	}

	Command* undoCommand = undoHistory[undoHistory.size() - 1];
	if (!undoCommand->allowedDuringLoad && g_app->isLoading)
	{
		logf("Can't undo {} while map is loading!\n", undoCommand->desc);
		return;
	}

	undoCommand->undo();
	undoHistory.pop_back();
	redoHistory.push_back(undoCommand);
	g_app->updateEnts();
}

void BspRenderer::redo()
{
	if (redoHistory.empty())
	{
		return;
	}

	Command* redoCommand = redoHistory[redoHistory.size() - 1];
	if (!redoCommand->allowedDuringLoad && g_app->isLoading)
	{
		logf("Can't redo {} while map is loading!\n", redoCommand->desc);
		return;
	}

	redoCommand->execute();
	redoHistory.pop_back();
	undoHistory.push_back(redoCommand);
	g_app->updateEnts();
}

void BspRenderer::clearUndoCommands()
{
	for (int i = 0; i < undoHistory.size(); i++)
	{
		delete undoHistory[i];
	}

	undoHistory.clear();
	calcUndoMemoryUsage();
}

void BspRenderer::clearRedoCommands()
{
	for (int i = 0; i < redoHistory.size(); i++)
	{
		delete redoHistory[i];
	}

	redoHistory.clear();
	calcUndoMemoryUsage();
}

void BspRenderer::calcUndoMemoryUsage()
{
	undoMemoryUsage = (undoHistory.size() + redoHistory.size()) * sizeof(Command*);

	for (int i = 0; i < undoHistory.size(); i++)
	{
		undoMemoryUsage += undoHistory[i]->memoryUsage();
	}
	for (int i = 0; i < redoHistory.size(); i++)
	{
		undoMemoryUsage += redoHistory[i]->memoryUsage();
	}
}

PickInfo::PickInfo()
{
	selectedEnts.clear();
	selectedFaces.clear();
	bestDist = 0.0f;
}

int PickInfo::GetSelectedEnt()
{
	if (selectedEnts.size())
		return selectedEnts[0];
	return -1;
}

void PickInfo::AddSelectedEnt(int entIdx)
{
	selectedEnts.push_back(entIdx);
	pickCount++;
}

void PickInfo::SetSelectedEnt(int entIdx)
{
	selectedEnts.clear();
	AddSelectedEnt(entIdx);
}

void PickInfo::DelSelectedEnt(int entIdx)
{
	if (IsSelectedEnt(entIdx))
	{
		pickCount++;
		selectedEnts.erase(std::find(selectedEnts.begin(), selectedEnts.end(), entIdx));
	}
}

bool PickInfo::IsSelectedEnt(int entIdx)
{
	return std::find(selectedEnts.begin(), selectedEnts.end(), entIdx) != selectedEnts.end();
}
