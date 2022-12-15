#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "ShaderProgram.h"
#include "BspRenderer.h"
#include "Fgd.h"
#include <thread>
#include <future>
#include "Command.h"
#include <GLFW/glfw3.h>
#include <GL/glew.h>

#define EDIT_MODEL_LUMPS (PLANES | TEXTURES | VERTICES | NODES | TEXINFO | FACES | LIGHTING | CLIPNODES | LEAVES | EDGES | SURFEDGES | MODELS)

extern std::string g_settings_path;
extern std::string g_config_dir;

class Gui;

enum transform_modes
{
	TRANSFORM_MODE_NONE = -1,
	TRANSFORM_MODE_MOVE,
	TRANSFORM_MODE_SCALE
};

enum transform_targets
{
	TRANSFORM_OBJECT,
	TRANSFORM_VERTEX,
	TRANSFORM_ORIGIN
};

enum pick_modes
{
	PICK_OBJECT,
	PICK_FACE
};

struct TransformAxes
{
	cCube model[6];
	VertexBuffer* buffer;
	vec3 origin;
	vec3 mins[6];
	vec3 maxs[6];
	COLOR4 dimColor[6];
	COLOR4 hoverColor[6];
	int numAxes;
};

struct PathToggleStruct
{
	std::string path;
	bool enabled;

	PathToggleStruct(std::string filePath, bool isEnable)
	{
		path = filePath;
		enabled = isEnable;
	}
};

struct AppSettings
{
	int windowWidth;
	int windowHeight;
	int windowX;
	int windowY;
	int maximized;
	float fontSize;
	std::string gamedir;
	std::string workingdir;
	std::string lastdir;
	bool settingLoaded; // Settings loaded
	int undoLevels;
	bool verboseLogs;

	bool save_windows;
	bool debug_open;
	bool keyvalue_open;
	bool transform_open;
	bool log_open;
	bool settings_open;
	bool limits_open;
	bool entreport_open;
	int settings_tab;

	float fov;
	float zfar;
	float moveSpeed;
	float rotSpeed;
	int render_flags;
	bool vsync;
	bool show_transform_axes;
	bool backUpMap;

	bool preserveCrc32;
	bool autoImportEnt;
	bool sameDirForEnt;

	bool entListReload;
	bool stripWad;

	std::vector<PathToggleStruct> fgdPaths;
	std::vector<PathToggleStruct> resPaths;



	std::vector<std::string> conditionalPointEntTriggers;
	std::vector<std::string> entsThatNeverNeedAnyHulls;
	std::vector<std::string> entsThatNeverNeedCollision;
	std::vector<std::string> passableEnts;
	std::vector<std::string> playerOnlyTriggers;
	std::vector<std::string> monsterOnlyTriggers;
	std::vector<std::string> entsNegativePitchPrefix;

	void loadDefault();
	void load();
	void reset();
	void save();
	void save(std::string path);
};

extern std::string GetWorkDir();

class Renderer;

extern AppSettings g_settings;
extern Renderer* g_app;

extern vec2 mousePos;
extern vec3 cameraOrigin;
extern vec3 cameraAngles;

class Renderer
{
	friend class Gui;
	friend class EditEntityCommand;
	friend class DeleteEntityCommand;
	friend class CreateEntityCommand;
	friend class DuplicateBspModelCommand;
	friend class CreateBspModelCommand;
	friend class EditBspModelCommand;
	friend class CleanMapCommand;
	friend class OptimizeMapCommand;

public:
	std::vector<BspRenderer*> mapRenderers;

	vec3 debugPoint;
	vec3 debugVec0;
	vec3 debugVec1;
	vec3 debugVec2;
	vec3 debugVec3;

	Gui* gui;

	bool hideGui = false;
	bool isModelsReloading = false;

	Renderer();
	~Renderer();

	void addMap(Bsp* map);

	void reloadBspModels();
	void renderLoop();
	void postLoadFgdsAndTextures();
	void postLoadFgds();
	void reloadMaps();
	void clearMaps();
	void saveSettings();
	void loadSettings();

	bool isSleeping = false;
	bool reloading = false;
	bool isLoading = false;
	bool reloadingGameDir = false;

	PickInfo pickInfo = PickInfo();
	BspRenderer* getMapContainingCamera();
	Bsp* getSelectedMap();
	int getSelectedMapId();
	void selectMapId(int id);
	void selectMap(Bsp* map);
	void deselectMap();
	void clearSelection();
	void updateEnts();
private:
	Bsp* SelectedMap = NULL;

	GLFWwindow* window;
	ShaderProgram* bspShader;
	ShaderProgram* fullBrightBspShader;
	ShaderProgram* colorShader;
	PointEntRenderer* pointEntRenderer;
	PointEntRenderer* swapPointEntRenderer = NULL;

	static std::future<void> fgdFuture;

	Fgd* fgd = NULL;

	double oldTime = 0.0;
	double curTime = 0.0;
	vec3 cameraForward;
	vec3 cameraUp;
	vec3 cameraRight;
	bool cameraIsRotating;
	float moveSpeed = 200.0f;
	float fov = 75.0f;
	float zNear = 1.0f;
	float zFar = 262144.0f;
	float rotationSpeed = 5.0f;
	int windowWidth;
	int windowHeight;
	mat4x4 matmodel = mat4x4(), matview = mat4x4(), projection = mat4x4(), modelView = mat4x4(), modelViewProjection = mat4x4();

	vec2 lastMousePos;
	vec2 totalMouseDrag;

	bool movingEnt = false; // grab an ent and move it with the camera
	vec3 grabStartOrigin;
	vec3 grabStartEntOrigin;
	float grabDist;

	TransformAxes moveAxes = TransformAxes();
	TransformAxes scaleAxes = TransformAxes();
	int hoverAxis; // axis being hovered
	int draggingAxis = -1; // axis currently being dragged by the mouse
	bool gridSnappingEnabled = true;
	int gridSnapLevel = 0;
	int transformMode = TRANSFORM_MODE_MOVE;
	int transformTarget = TRANSFORM_OBJECT;
	int pickMode = PICK_OBJECT;
	bool showDragAxes = false;
	bool pickClickHeld = true; // true if the mouse button is still held after picking an object
	vec3 axisDragStart;
	vec3 dragDelta;
	vec3 axisDragEntOriginStart;
	std::vector<ScalableTexinfo> scaleTexinfos; // texture coordinates to scale
	bool textureLock = false;
	bool moveOrigin = true;
	bool invalidSolid = false;
	bool isTransformableSolid = false;
	bool anyEdgeSelected = false;
	bool anyVertSelected = false;


	std::vector<TransformVert> modelVerts; // control points for invisible plane intersection verts in HULL 0
	std::vector<TransformVert> modelFaceVerts; // control points for visible face verts
	std::vector<HullEdge> modelEdges;
	cCube* modelVertCubes = NULL;
	cCube modelOriginCube = cCube();
	VertexBuffer* modelVertBuff = NULL;
	VertexBuffer* modelOriginBuff = NULL;
	bool originSelected = false;
	bool originHovered = false;
	vec3 oldOrigin;
	vec3 transformedOrigin;
	int hoverVert = -1;
	int hoverEdge = -1;
	float vertExtentFactor = 0.01f;
	bool modelUsesSharedStructures = false;
	vec3 selectionSize;

	cVert* line_verts = NULL;
	VertexBuffer* lineBuf = NULL;

	cQuad* plane_verts = NULL;
	VertexBuffer* planeBuf = NULL;

	VertexBuffer* entConnections = NULL;
	VertexBuffer* entConnectionPoints = NULL;

	Entity* copiedEnt = NULL;

	int oldLeftMouse;
	int curLeftMouse;
	int oldRightMouse;
	int curRightMouse;

	int oldScroll;

	bool pressed[GLFW_KEY_LAST];
	bool released[GLFW_KEY_LAST];
	bool oldPressed[GLFW_KEY_LAST];
	bool oldReleased[GLFW_KEY_LAST];

	bool anyCtrlPressed;
	bool anyAltPressed;
	bool anyShiftPressed;

	bool canControl;
	bool oldControl;

	int pickCount = 0; // used to give unique IDs to text inputs so switching ents doesn't update keys accidentally
	int vertPickCount = 0;

	int debugInt = 0;
	int debugIntMax = 0;
	int debugNode = 0;
	int debugNodeMax = 0;
	bool debugClipnodes = false;
	bool debugNodes = false;
	int clipnodeRenderHull = -1;

	vec3 getMoveDir();
	void controls();
	void cameraPickingControls();
	void vertexEditControls();
	void cameraRotationControls();
	void cameraObjectHovering();
	void cameraContextMenus(); // right clicking on ents and things
	void moveGrabbedEnt(); // translates the grabbed ent
	void shortcutControls(); // hotkeys for menus and things
	void globalShortcutControls(); // these work even with the UI selected
	void pickObject(); // select stuff with the mouse
	bool transformAxisControls(); // true if grabbing axes
	void applyTransform(bool forceUpdate = false);
	void setupView();
	void getPickRay(vec3& start, vec3& pickDir);
	void revertInvalidSolid(Bsp* map, int entIdx);

	void drawModelVerts();
	void drawModelOrigin();
	void drawTransformAxes();
	void drawEntConnections();
	void drawLine(const vec3& start, const vec3& end, COLOR4 color);
	void drawPlane(BSPPLANE& plane, COLOR4 color, vec3 offset = vec3());
	void drawClipnodes(Bsp* map, int iNode, int& currentPlane, int activePlane, vec3 offset = vec3());
	void drawNodes(Bsp* map, int iNode, int& currentPlane, int activePlane, vec3 offset = vec3());

	vec3 getEntOrigin(Bsp* map, Entity* ent);
	vec3 getEntOffset(Bsp* map, Entity* ent);

	vec3 getAxisDragPoint(vec3 origin);

	void updateDragAxes(vec3 delta = vec3());
	void updateModelVerts();
	void updateSelectionSize();
	void updateEntConnections();
	void updateEntConnectionPositions(); // only updates positions in the buffer
	bool getModelSolid(std::vector<TransformVert>& hullVerts, Bsp* map, Solid& outSolid); // calculate face vertices from plane intersections
	void moveSelectedVerts(const vec3& delta);
	bool splitModelFace();

	vec3 snapToGrid(const vec3& pos);

	void grabEnt();
	void cutEnt();
	void copyEnt();
	void pasteEnt(bool noModifyOrigin);
	void deleteEnt(int entIdx = 0);
	void deleteEnts();
	void scaleSelectedObject(float x, float y, float z);
	void scaleSelectedObject(vec3 dir, const vec3& fromDir);
	void scaleSelectedVerts(float x, float y, float z);
	vec3 getEdgeControlPoint(std::vector<TransformVert>& hullVerts, HullEdge& edge);
	vec3 getCentroid(std::vector<TransformVert>& hullVerts);
	void deselectObject(); // keep map selected but unselect all objects
	void selectFace(Bsp* map, int face, bool add = false);
	void deselectFaces();
	void selectEnt(Bsp* map, int entIdx, bool add = false);
	void goToEnt(Bsp* map, int entIdx);
	void goToCoords(float x, float y, float z);
	void goToFace(Bsp* map, int faceIdx);
	void ungrabEnt();
	void loadFgds();
};