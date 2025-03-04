#include "Settings.h"
#include "Renderer.h"
#include "util.h"
#include <iostream>
#include <fstream>
#include <string>

AppSettings g_settings;
std::string g_settings_path = "";
std::string g_config_dir = "";

void AppSettings::loadDefault()
{
	settingLoaded = false;

	windowWidth = 800;
	windowHeight = 600;
	windowX = 0;
#ifdef WIN32
	windowY = 30;
#else
	windowY = 0;
#endif
	maximized = 0;
	fontSize = 22.f;
	gamedir = std::string();
	workingdir = "./bspguy_work/";

	lastdir = "";
	undoLevels = 64;

	verboseLogs = false;
#ifndef NDEBUG
	verboseLogs = true;
#endif
	save_windows = false;
	debug_open = false;
	keyvalue_open = false;
	transform_open = false;
	log_open = false;
	limits_open = false;
	entreport_open = false;
	goto_open = false;

	settings_tab = 0;

	render_flags = g_render_flags = RENDER_TEXTURES | RENDER_LIGHTMAPS | RENDER_SPECIAL
		| RENDER_ENTS | RENDER_SPECIAL_ENTS | RENDER_POINT_ENTS | RENDER_WIREFRAME | RENDER_ENT_CONNECTIONS
		| RENDER_ENT_CLIPNODES | RENDER_MODELS | RENDER_MODELS_ANIMATED;

	vsync = true;
	merge_verts = false;
	mark_unused_texinfos = false;
	start_at_entity = false;
	backUpMap = true;
	preserveCrc32 = false;
	autoImportEnt = false;
	sameDirForEnt = false;

	moveSpeed = 500.0f;
	fov = 75.0f;
	zfar = 262144.0f;
	rotSpeed = 5.0f;

	fgdPaths.clear();
	resPaths.clear();

	conditionalPointEntTriggers.clear();
	entsThatNeverNeedAnyHulls.clear();
	entsThatNeverNeedCollision.clear();
	passableEnts.clear();
	playerOnlyTriggers.clear();
	monsterOnlyTriggers.clear();
	entsNegativePitchPrefix.clear();
	transparentTextures.clear();
	transparentEntities.clear();

	defaultIsEmpty = true;

	entListReload = true;
	stripWad = false;

	ResetBspLimits();
}

void AppSettings::reset()
{
	loadDefault();

	fgdPaths.clear();
	fgdPaths.push_back({ "/moddir/GameDefinitionFile.fgd",true });

	resPaths.clear();
	resPaths.push_back({ "/moddir/",true });
	resPaths.push_back({ "/moddir_addon/",true });

	conditionalPointEntTriggers.clear();
	conditionalPointEntTriggers.push_back("trigger_once");
	conditionalPointEntTriggers.push_back("trigger_multiple");
	conditionalPointEntTriggers.push_back("trigger_counter");
	conditionalPointEntTriggers.push_back("trigger_gravity");
	conditionalPointEntTriggers.push_back("trigger_teleport");

	entsThatNeverNeedAnyHulls.clear();
	entsThatNeverNeedAnyHulls.push_back("env_bubbles");
	entsThatNeverNeedAnyHulls.push_back("func_tankcontrols");
	entsThatNeverNeedAnyHulls.push_back("func_traincontrols");
	entsThatNeverNeedAnyHulls.push_back("func_vehiclecontrols");
	entsThatNeverNeedAnyHulls.push_back("trigger_autosave"); // obsolete in sven
	entsThatNeverNeedAnyHulls.push_back("trigger_endsection"); // obsolete in sven

	entsThatNeverNeedCollision.clear();
	entsThatNeverNeedCollision.push_back("func_illusionary");
	entsThatNeverNeedCollision.push_back("func_mortar_field");

	passableEnts.clear();
	passableEnts.push_back("func_door");
	passableEnts.push_back("func_door_rotating");
	passableEnts.push_back("func_pendulum");
	passableEnts.push_back("func_tracktrain");
	passableEnts.push_back("func_train");
	passableEnts.push_back("func_water");
	passableEnts.push_back("momentary_door");

	playerOnlyTriggers.clear();
	playerOnlyTriggers.push_back("func_ladder");
	playerOnlyTriggers.push_back("game_zone_player");
	playerOnlyTriggers.push_back("player_respawn_zone");
	playerOnlyTriggers.push_back("trigger_cdaudio");
	playerOnlyTriggers.push_back("trigger_changelevel");
	playerOnlyTriggers.push_back("trigger_transition");

	monsterOnlyTriggers.clear();
	monsterOnlyTriggers.push_back("func_monsterclip");
	monsterOnlyTriggers.push_back("trigger_monsterjump");

	entsNegativePitchPrefix.clear();

	entsNegativePitchPrefix.push_back("ammo_");
	entsNegativePitchPrefix.push_back("env_sprite");
	entsNegativePitchPrefix.push_back("cycler");
	entsNegativePitchPrefix.push_back("item_");
	entsNegativePitchPrefix.push_back("monster_");
	entsNegativePitchPrefix.push_back("weaponbox");
	entsNegativePitchPrefix.push_back("worlditems");
	entsNegativePitchPrefix.push_back("xen_");

	transparentTextures.clear();
	transparentTextures.push_back("AAATRIGGER");

	transparentEntities.clear();
	transparentEntities.push_back("func_buyzone");
}

void AppSettings::load()
{
	std::ifstream file(g_settings_path);
	if (!file.is_open())
	{
		logf("No access to settings file {}!\n", g_settings_path);
		reset();
		return;
	}

	int lines_readed = 0;
	std::string line;
	while (getline(file, line))
	{
		if (line.empty())
			continue;

		size_t eq = line.find('=');
		if (eq == std::string::npos)
		{
			continue;
		}
		lines_readed++;

		std::string key = trimSpaces(line.substr(0, eq));
		std::string val = trimSpaces(line.substr(eq + 1));

		if (key == "window_width")
		{
			g_settings.windowWidth = atoi(val.c_str());
		}
		else if (key == "window_height")
		{
			g_settings.windowHeight = atoi(val.c_str());
		}
		else if (key == "window_x")
		{
			g_settings.windowX = atoi(val.c_str());
		}
		else if (key == "window_y")
		{
			g_settings.windowY = atoi(val.c_str());
		}
		else if (key == "window_maximized")
		{
			g_settings.maximized = atoi(val.c_str());
		}
		else if (key == "save_windows")
		{
			g_settings.save_windows = atoi(val.c_str()) != 0;
		}
		else if (key == "debug_open")
		{
			g_settings.debug_open = atoi(val.c_str()) != 0 && save_windows;
		}
		else if (key == "keyvalue_open")
		{
			g_settings.keyvalue_open = atoi(val.c_str()) != 0 && save_windows;
		}
		else if (key == "transform_open")
		{
			g_settings.transform_open = atoi(val.c_str()) != 0 && save_windows;
		}
		else if (key == "log_open")
		{
			g_settings.log_open = atoi(val.c_str()) != 0 && save_windows;
		}
		else if (key == "limits_open")
		{
			g_settings.limits_open = atoi(val.c_str()) != 0 && save_windows;
		}
		else if (key == "entreport_open")
		{
			g_settings.entreport_open = atoi(val.c_str()) != 0 && save_windows;
		}
		else if (key == "texbrowser_open")
		{
			g_settings.texbrowser_open = atoi(val.c_str()) != 0 && save_windows;
		}
		else if (key == "goto_open")
		{
			g_settings.goto_open = atoi(val.c_str()) != 0 && save_windows;
		}
		else if (key == "settings_tab")
		{
			if (save_windows)
				g_settings.settings_tab = atoi(val.c_str());
		}
		else if (key == "vsync")
		{
			g_settings.vsync = atoi(val.c_str()) != 0;
		}
		else if (key == "mark_unused_texinfos")
		{
			g_settings.mark_unused_texinfos = atoi(val.c_str()) != 0;
		}
		else if (key == "merge_verts")
		{
			g_settings.merge_verts = atoi(val.c_str()) != 0;
		}
		else if (key == "start_at_entity")
		{
			g_settings.start_at_entity = atoi(val.c_str()) != 0;
		}
		else if (key == "verbose_logs")
		{
			g_settings.verboseLogs = atoi(val.c_str()) != 0;
#ifndef NDEBUG
			g_settings.verboseLogs = true;
#endif
		}
		else if (key == "fov")
		{
			g_settings.fov = (float)atof(val.c_str());
		}
		else if (key == "zfar")
		{
			g_settings.zfar = (float)atof(val.c_str());
		}
		else if (key == "move_speed")
		{
			g_settings.moveSpeed = (float)atof(val.c_str());
			if (g_settings.moveSpeed < 100)
			{
				logf("Move speed can be 100 - 1000. Replaced to default value.\n");
				g_settings.moveSpeed = 500;
			}
		}
		else if (key == "rot_speed")
		{
			g_settings.rotSpeed = (float)atof(val.c_str());
		}
		else if (key == "renders_flags")
		{
			g_settings.render_flags = atoi(val.c_str());
		}
		else if (key == "font_size")
		{
			g_settings.fontSize = (float)atof(val.c_str());
		}
		else if (key == "undo_levels")
		{
			g_settings.undoLevels = atoi(val.c_str());
		}
		else if (key == "gamedir")
		{
			g_settings.gamedir = val;
		}
		else if (key == "workingdir")
		{
			g_settings.workingdir = val;
		}
		else if (key == "lastdir")
		{
			g_settings.lastdir = val;
		}
		else if (key == "fgd")
		{
			if (val.find('?') == std::string::npos)
				fgdPaths.push_back({ val,true });
			else
			{
				auto vals = splitString(val, "?");
				if (vals.size() == 2)
				{
					fgdPaths.push_back({ vals[1],vals[0] == "enabled" });
				}
			}
		}
		else if (key == "res")
		{
			if (val.find('?') == std::string::npos)
				resPaths.push_back({ val,true });
			else
			{
				auto vals = splitString(val, "?");
				if (vals.size() == 2)
				{
					resPaths.push_back({ vals[1],vals[0] == "enabled" });
				}
			}
		}
		else if (key == "savebackup")
		{
			g_settings.backUpMap = atoi(val.c_str()) != 0;
		}
		else if (key == "save_crc")
		{
			g_settings.preserveCrc32 = atoi(val.c_str()) != 0;
		}
		else if (key == "auto_import_ent")
		{
			g_settings.autoImportEnt = atoi(val.c_str()) != 0;
		}
		else if (key == "same_dir_for_ent")
		{
			g_settings.sameDirForEnt = atoi(val.c_str()) != 0;
		}
		else if (key == "reload_ents_list")
		{
			entListReload = atoi(val.c_str()) != 0;
		}
		else if (key == "strip_wad_path")
		{
			stripWad = atoi(val.c_str()) != 0;
		}
		else if (key == "default_is_empty")
		{
			defaultIsEmpty = atoi(val.c_str()) != 0;
		}
		else if (key == "FLT_MAX_COORD")
		{
			FLT_MAX_COORD = (float)atof(val.c_str());
		}
		else if (key == "MAX_MAP_MODELS")
		{
			MAX_MAP_MODELS = atoi(val.c_str());
		}
		else if (key == "MAX_MAP_NODES")
		{
			MAX_MAP_NODES = atoi(val.c_str());
		}
		else if (key == "MAX_MAP_CLIPNODES")
		{
			MAX_MAP_CLIPNODES = atoi(val.c_str());
		}
		else if (key == "MAX_MAP_LEAVES")
		{
			MAX_MAP_LEAVES = atoi(val.c_str());
		}
		else if (key == "MAX_MAP_VISDATA")
		{
			MAX_MAP_VISDATA = atoi(val.c_str()) * (1024 * 1024);
		}
		else if (key == "MAX_MAP_ENTS")
		{
			MAX_MAP_ENTS = atoi(val.c_str());
		}
		else if (key == "MAX_MAP_SURFEDGES")
		{
			MAX_MAP_SURFEDGES = atoi(val.c_str());
		}
		else if (key == "MAX_MAP_EDGES")
		{
			MAX_MAP_EDGES = atoi(val.c_str());
		}
		else if (key == "MAX_MAP_TEXTURES")
		{
			MAX_MAP_TEXTURES = atoi(val.c_str());
		}
		else if (key == "MAX_MAP_LIGHTDATA")
		{
			MAX_MAP_LIGHTDATA = atoi(val.c_str()) * (1024 * 1024);
		}
		else if (key == "MAX_TEXTURE_DIMENSION")
		{
			MAX_TEXTURE_DIMENSION = atoi(val.c_str());
			MAX_TEXTURE_SIZE = ((MAX_TEXTURE_DIMENSION * MAX_TEXTURE_DIMENSION * 2 * 3) / 2);
		}
		else if (key == "TEXTURE_STEP")
		{
			TEXTURE_STEP = atoi(val.c_str());
		}
		else if (key == "optimizer_cond_ents")
		{
			conditionalPointEntTriggers.push_back(val);
		}
		else if (key == "optimizer_no_hulls_ents")
		{
			entsThatNeverNeedAnyHulls.push_back(val);
		}
		else if (key == "optimizer_no_collision_ents")
		{
			entsThatNeverNeedCollision.push_back(val);
		}
		else if (key == "optimizer_passable_ents")
		{
			passableEnts.push_back(val);
		}
		else if (key == "optimizer_player_hull_ents")
		{
			playerOnlyTriggers.push_back(val);
		}
		else if (key == "optimizer_monster_hull_ents")
		{
			monsterOnlyTriggers.push_back(val);
		}
		else if (key == "negative_pitch_ents")
		{
			entsNegativePitchPrefix.push_back(val);
		}
		else if (key == "transparent_textures")
		{
			transparentTextures.push_back(val);
		}
		else if (key == "transparent_entities")
		{
			transparentEntities.push_back(val);
		}
	}

	if (g_settings.windowY == -32000 &&
		g_settings.windowX == -32000)
	{
		g_settings.windowY = 0;
		g_settings.windowX = 0;
	}


#ifdef WIN32
	// Fix invisibled window header for primary screen.
	if (g_settings.windowY >= 0 && g_settings.windowY < 30)
	{
		g_settings.windowY = 30;
	}
#endif

	// Restore default window height if invalid.
	if (windowHeight <= 0 || windowWidth <= 0)
	{
		windowHeight = 600;
		windowWidth = 800;
	}

	if (lines_readed > 0)
		g_settings.settingLoaded = true;
	else
		logf("Failed to load user config: {}\n", g_settings_path);

	if (defaultIsEmpty && fgdPaths.empty())
	{
		fgdPaths.push_back({ "/moddir/GameDefinitionFile.fgd",true });
	}

	if (defaultIsEmpty && resPaths.empty())
	{
		resPaths.push_back({ "/moddir/",true });
		resPaths.push_back({ "/moddir_addon/",true });
	}

	if (entListReload || defaultIsEmpty)
	{
		if ((defaultIsEmpty && conditionalPointEntTriggers.empty()) || entListReload)
		{
			conditionalPointEntTriggers.clear();
			conditionalPointEntTriggers.push_back("trigger_once");
			conditionalPointEntTriggers.push_back("trigger_multiple");
			conditionalPointEntTriggers.push_back("trigger_counter");
			conditionalPointEntTriggers.push_back("trigger_gravity");
			conditionalPointEntTriggers.push_back("trigger_teleport");
		}
		if ((defaultIsEmpty && entsThatNeverNeedAnyHulls.empty()) || entListReload)
		{
			entsThatNeverNeedAnyHulls.clear();
			entsThatNeverNeedAnyHulls.push_back("env_bubbles");
			entsThatNeverNeedAnyHulls.push_back("func_tankcontrols");
			entsThatNeverNeedAnyHulls.push_back("func_traincontrols");
			entsThatNeverNeedAnyHulls.push_back("func_vehiclecontrols");
			entsThatNeverNeedAnyHulls.push_back("trigger_autosave"); // obsolete in sven
			entsThatNeverNeedAnyHulls.push_back("trigger_endsection"); // obsolete in sven
		}
		if ((defaultIsEmpty && entsThatNeverNeedCollision.empty()) || entListReload)
		{
			entsThatNeverNeedCollision.clear();
			entsThatNeverNeedCollision.push_back("func_illusionary");
			entsThatNeverNeedCollision.push_back("func_mortar_field");
		}
		if ((defaultIsEmpty && passableEnts.empty()) || entListReload)
		{
			passableEnts.clear();
			passableEnts.push_back("func_door");
			passableEnts.push_back("func_door_rotating");
			passableEnts.push_back("func_pendulum");
			passableEnts.push_back("func_tracktrain");
			passableEnts.push_back("func_train");
			passableEnts.push_back("func_water");
			passableEnts.push_back("momentary_door");
		}
		if ((defaultIsEmpty && playerOnlyTriggers.empty()) || entListReload)
		{
			playerOnlyTriggers.clear();
			playerOnlyTriggers.push_back("func_ladder");
			playerOnlyTriggers.push_back("game_zone_player");
			playerOnlyTriggers.push_back("player_respawn_zone");
			playerOnlyTriggers.push_back("trigger_cdaudio");
			playerOnlyTriggers.push_back("trigger_changelevel");
			playerOnlyTriggers.push_back("trigger_transition");
		}
		if ((defaultIsEmpty && monsterOnlyTriggers.empty()) || entListReload)
		{
			monsterOnlyTriggers.clear();
			monsterOnlyTriggers.push_back("func_monsterclip");
			monsterOnlyTriggers.push_back("trigger_monsterjump");
		}
		if ((defaultIsEmpty && entsNegativePitchPrefix.empty()) || entListReload)
		{
			entsNegativePitchPrefix.clear();
			entsNegativePitchPrefix.push_back("ammo_");
			entsNegativePitchPrefix.push_back("cycler");
			entsNegativePitchPrefix.push_back("item_");
			entsNegativePitchPrefix.push_back("monster_");
			entsNegativePitchPrefix.push_back("weaponbox");
			entsNegativePitchPrefix.push_back("worlditems");
			entsNegativePitchPrefix.push_back("xen_");
		}
	}

	if (defaultIsEmpty && transparentTextures.empty())
	{
		transparentTextures.push_back("AAATRIGGER");
	}

	if (defaultIsEmpty && transparentEntities.empty())
	{
		transparentEntities.push_back("func_buyzone");
	}


	FixupAllSystemPaths();

	entListReload = false;
}

void AppSettings::save(std::string path)
{
	std::ostringstream file;

	file << "window_width=" << g_settings.windowWidth << std::endl;
	file << "window_height=" << g_settings.windowHeight << std::endl;
	file << "window_x=" << g_settings.windowX << std::endl;
	file << "window_y=" << g_settings.windowY << std::endl;
	file << "window_maximized=" << g_settings.maximized << std::endl;

	file << "save_windows=" << g_settings.save_windows << std::endl;
	file << "debug_open=" << g_settings.debug_open << std::endl;
	file << "keyvalue_open=" << g_settings.keyvalue_open << std::endl;
	file << "transform_open=" << g_settings.transform_open << std::endl;
	file << "log_open=" << g_settings.log_open << std::endl;
	file << "limits_open=" << g_settings.limits_open << std::endl;
	file << "entreport_open=" << g_settings.entreport_open << std::endl;
	file << "texbrowser_open=" << g_settings.texbrowser_open << std::endl;
	file << "goto_open=" << g_settings.goto_open << std::endl;

	file << "settings_tab=" << g_settings.settings_tab << std::endl;

	file << "gamedir=" << g_settings.gamedir << std::endl;
	file << "workingdir=" << g_settings.workingdir << std::endl;
	file << "lastdir=" << g_settings.lastdir << std::endl;

	for (int i = 0; i < fgdPaths.size(); i++)
	{
		file << "fgd=" << (g_settings.fgdPaths[i].enabled ? "enabled" : "disabled") << "?" << g_settings.fgdPaths[i].path << std::endl;
	}

	for (int i = 0; i < resPaths.size(); i++)
	{
		file << "res=" << (g_settings.resPaths[i].enabled ? "enabled" : "disabled") << "?" << g_settings.resPaths[i].path << std::endl;
	}

	for (int i = 0; i < conditionalPointEntTriggers.size(); i++)
	{
		file << "optimizer_cond_ents=" << conditionalPointEntTriggers[i] << std::endl;
	}

	for (int i = 0; i < entsThatNeverNeedAnyHulls.size(); i++)
	{
		file << "optimizer_no_hulls_ents=" << entsThatNeverNeedAnyHulls[i] << std::endl;
	}

	for (int i = 0; i < entsThatNeverNeedCollision.size(); i++)
	{
		file << "optimizer_no_collision_ents=" << entsThatNeverNeedCollision[i] << std::endl;
	}

	for (int i = 0; i < passableEnts.size(); i++)
	{
		file << "optimizer_passable_ents=" << passableEnts[i] << std::endl;
	}

	for (int i = 0; i < playerOnlyTriggers.size(); i++)
	{
		file << "optimizer_player_hull_ents=" << playerOnlyTriggers[i] << std::endl;
	}

	for (int i = 0; i < monsterOnlyTriggers.size(); i++)
	{
		file << "optimizer_monster_hull_ents=" << monsterOnlyTriggers[i] << std::endl;
	}

	for (int i = 0; i < entsNegativePitchPrefix.size(); i++)
	{
		file << "negative_pitch_ents=" << entsNegativePitchPrefix[i] << std::endl;
	}

	for (int i = 0; i < transparentTextures.size(); i++)
	{
		file << "transparent_textures=" << transparentTextures[i] << std::endl;
	}

	for (int i = 0; i < transparentEntities.size(); i++)
	{
		file << "transparent_entities=" << transparentEntities[i] << std::endl;
	}

	file << "vsync=" << g_settings.vsync << std::endl;
	file << "mark_unused_texinfos=" << g_settings.mark_unused_texinfos << std::endl;
	file << "merge_verts=" << g_settings.merge_verts << std::endl;
	file << "start_at_entity=" << g_settings.start_at_entity << std::endl;
#ifdef NDEBUG
	file << "verbose_logs=" << g_settings.verboseLogs << std::endl;
#endif
	file << "fov=" << g_settings.fov << std::endl;
	file << "zfar=" << g_settings.zfar << std::endl;
	file << "move_speed=" << g_settings.moveSpeed << std::endl;
	file << "rot_speed=" << g_settings.rotSpeed << std::endl;
	file << "renders_flags=" << g_settings.render_flags << std::endl;
	file << "font_size=" << g_settings.fontSize << std::endl;
	file << "undo_levels=" << g_settings.undoLevels << std::endl;
	file << "savebackup=" << g_settings.backUpMap << std::endl;
	file << "save_crc=" << g_settings.preserveCrc32 << std::endl;
	file << "auto_import_ent=" << g_settings.autoImportEnt << std::endl;
	file << "same_dir_for_ent=" << g_settings.sameDirForEnt << std::endl;
	file << "reload_ents_list=" << g_settings.entListReload << std::endl;
	file << "strip_wad_path=" << g_settings.stripWad << std::endl;
	file << "default_is_empty=" << g_settings.defaultIsEmpty << std::endl;

	file << "FLT_MAX_COORD=" << FLT_MAX_COORD << std::endl;
	file << "MAX_MAP_MODELS=" << MAX_MAP_MODELS << std::endl;
	file << "MAX_MAP_NODES=" << MAX_MAP_NODES << std::endl;
	file << "MAX_MAP_CLIPNODES=" << MAX_MAP_CLIPNODES << std::endl;
	file << "MAX_MAP_LEAVES=" << MAX_MAP_LEAVES << std::endl;
	file << "MAX_MAP_VISDATA=" << MAX_MAP_VISDATA / (1024 * 1024) << std::endl;
	file << "MAX_MAP_ENTS=" << MAX_MAP_ENTS << std::endl;
	file << "MAX_MAP_SURFEDGES=" << MAX_MAP_SURFEDGES << std::endl;
	file << "MAX_MAP_EDGES=" << MAX_MAP_EDGES << std::endl;
	file << "MAX_MAP_TEXTURES=" << MAX_MAP_TEXTURES << std::endl;
	file << "MAX_MAP_LIGHTDATA=" << MAX_MAP_LIGHTDATA / (1024 * 1024) << std::endl;
	file << "MAX_TEXTURE_DIMENSION=" << MAX_TEXTURE_DIMENSION << std::endl;
	file << "TEXTURE_STEP=" << TEXTURE_STEP << std::endl;

	file.flush();

	writeFile(g_settings_path, file.str());
}

void AppSettings::save()
{
	if (!dirExists(g_config_dir))
	{
		createDir(g_config_dir);
	}
	g_app->saveSettings();
	save(g_settings_path);
}
