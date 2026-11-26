#ifndef  YWINT_H_INCLUDED
#define YWINT_H_INCLUDED

#include "yw_mapscene.h"

struct TSectorCollision
{
    NC_STACK_skeleton *sklt = NULL;
    vec3d pos;
    Common::Point Cell;
    int16_t CollisionType = 0;
    int16_t Flags = 0;
};

// For rendering
struct TRenderingSector
{
    cellArea *Cell;                       // Its place and height
    const World::Frame::Sector *Drawn;    // As the frame has it
    const World::Frame::Construction *Construction; // A building being built
    int InMap;   // The sector is on the map
    int Visible; // Its bounding box is in view
};


int yw_InitNetwork(NC_STACK_ypaworld *yw);




int loadTOD(NC_STACK_ypaworld *yw, const char *fname);
int writeTOD(NC_STACK_ypaworld *yw, const char *fname, int tod);

void RenderBriefingObject(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc, int elapsed);
void ypaworld_func158__DrawVehicle(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc);

void set_keys_vals(NC_STACK_ypaworld *yw);
void yw_draw_input_list(NC_STACK_ypaworld *yw, UserData *usr);

int yw_loadSky(NC_STACK_ypaworld *yw, const std::string &skyname);
//void yw_renderSky_test(_NC_STACK_ypaworld *yw, base77Func *rndr_params);



void listLocaleDir(UserData *usr, const char *dirname);



void PlayIntroMovie(NC_STACK_ypaworld *yw);

void CollectNetworkLevels(UserData *usr);





void yw_calcPlayerScore(NC_STACK_ypaworld *yw);


void UpdateBriefingScreen(NC_STACK_ypaworld *yw, UserData *usr, TInputState *inpt);

NC_STACK_base *load_set_base();

int SetupVehicleModels(NC_STACK_ypaworld *yw, NC_STACK_base *base);
int yw_parse_lego(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil, NC_STACK_base *base);
int yw_parse_subSect(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int yw_parse_sektor(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int SetupFillerModels(NC_STACK_ypaworld *yw, NC_STACK_base *base);


void DrawGameShellControls(NC_STACK_ypaworld *yw, UserData *usr);
void DrawReplaysPage(NC_STACK_ypaworld *yw, UserData *usr);
void FormatListItem(NC_STACK_ypaworld *yw, GuiList *lstvw, CmdStream *out, const std::string &txt);
std::string FileSizeText(uint64_t bytes);

void DeleteDirContents(const std::string &path);





int yw_write_callSign(NC_STACK_ypaworld *yw, const char *filename, const char *callsign);
int yw_write_user(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_input(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_sound(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_video(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_game(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_level_status(FSMgr::FileHandle *fil, NC_STACK_ypaworld *yw, int lvlid);
int yw_write_levels_statuses(FSMgr::FileHandle *fil, NC_STACK_ypaworld *yw);
int yw_write_buddies(FSMgr::FileHandle *fil, NC_STACK_ypaworld *yw);
int yw_write_shell(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_item_modifers(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);

void FreeSetData(NC_STACK_ypaworld *yw);





int GUI_Open(NC_STACK_ypaworld *yw);
void splashScreen_OutText(NC_STACK_ypaworld *yw, const std::string &txt, int x, int y);


void RestoreWinStatus(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact1, NC_STACK_ypabact *bact2);







void create_squad_man(NC_STACK_ypaworld *yw);
void DrawGameInterface(NC_STACK_ypaworld *yw);

void draw_tooltip(NC_STACK_ypaworld *yw);

void UpdateCommanderList(NC_STACK_ypaworld *yw);


class squadMan: public GuiList
{
public:
    std::array<NC_STACK_ypabact *, 24> squads;
    int dragFlags;
    int pickCmdrIdx;
    int pickKidIdx;
    int dragCmdrIdx;
    int dragKidIdx;
    NC_STACK_ypabact *dragUnit;
    int16_t dragOffsetX;
    int16_t dragOffsetY;
    int16_t dragIconX;
    int16_t dragIconY;
    int16_t dragStartX;
    int16_t dragStartY;
    int iconWidth;
    int cmdrIconX;
    int kidIconsX;
    int unitCountX;
    int aggrIconsX;
};

class energPanel : public GuiBase
{
public:
    int panelHeight = 0;
    int gaugeWidth = 0;
    int gaugeStartX = 0;
    int gaugeSpacing = 0;
    int iconWidth = 0;
    int barGap = 0;
    int barMaxLen = 0;
};

struct TBuyListEntry
{
    int kind = 0;
    int protoId = 0;
};

class TBottomBar : public GuiBase
{
public:
    int enabledModes = 0;
    int activeMode = 0;
    int barEnabled = 0;
    int inputAttached = 0;
    uint8_t gunIndices[256] = {0};
    uint8_t vhclProtoIds[256] = {0};
    uint8_t buildProtoIds[256] = {0};
    TBuyListEntry buyList[512];
    int gunCount = 0;
    int vhclCount = 0;
    int buildCount = 0;
    int buyListCount = 0;
    int selVhclIdx = 0;
    int selGunIdx = 0;
    int selBuildIdx = 0;
    int selBuyListIdx = 0;
    int groupGap = 0;
    int buyListWidth = 0;
    int createGroupX = 0;
    int windowGroupX = 0;
    int unitGroupX = 0;
    int toolGroupX = 0;
    int buttonsY = 0;
    int pressedBtns = 0;
};

struct TInfoLogMsg
{
    int id = 0;
    int timestamp = 0;
    int displayTime = 0;
    char txt[128] = {0};
};

class yw_infolog : public GuiList
{
public:
    int time_col_width = 0;
    int last_msg_idx = 0;
    int last_msg_prio = 0;
    int oldest_msg_idx = 0;
    int msg_count = 0;
    TInfoLogMsg msgs[64];
    uint32_t last_sender_gid = 0;
    int last_msg_time = 0;
    int last_msg_id = 0;
    int last_msg_lines = 0;
    int last_msg_duration = 0;
};

class tehMap;
extern tehMap robo_map;

class tehMap : public GuiBase
{
public:
    const MapScene *Scene = NULL; // What it shows: the game's, or a replay's
    int16_t hScrollLength;
    int16_t hThumbPos;
    int16_t hThumbSize;
    int16_t vScrollLength;
    int16_t vThumbPos;
    int16_t vThumbSize;
    float viewCenterX;
    float viewCenterZ;
    float unitsPerPixelX;
    float unitsPerPixelY;
    int stateFlags;
    uint8_t layerFlags;
    uint8_t lockViewer;
    uint8_t zoomLevel;
    int32_t MapViewMask;
    int scrollPixelX;
    int scrollPixelY;
    int viewWidth;
    int viewHeight;
    int viewScreenX;
    int viewScreenY;
    int16_t altX;
    int16_t altY;
    int16_t altW;
    int16_t altH;
    int16_t minWidth;
    int16_t minHeight;
    int16_t maxWidth;
    int16_t maxHeight;
    int16_t resizeOffsetX;
    int16_t resizeOffsetY;
    int16_t dragStartX;
    int16_t dragStartY;
    float dragStartCenterX;
    float dragStartCenterZ;
    int minThumbSize;
    int toolBtnWidth;
    int toolBtnHeight;
    int toolBarOffsetX;
    int sizeBtnWidth;
    int borderTop;
    int borderBottom;
    int borderLeft;
    int borderRight;
    int borderTotalW;
    int borderTotalH;
    float radarLeft;
    float radarTop;
    float radarRight;
    float radarBottom;
    float radarRotM00;
    
    CmdStream t1_cmdbuf_1;
    CmdStream t1_cmdbuf_2;
    CmdStream t1_cmdbuf_3; // Sectors, built by the simulation step
    CmdStream overlayCmds; // Units and markers, built while drawing
};

struct debrif_t1
{
    int owner;
    int status;
};


void HandleGuiInput(NC_STACK_ypaworld *yw, TInputState *inpt);
void RoboMapFollowViewer(NC_STACK_ypaworld *yw);
void RoboMapUpdateLayout(NC_STACK_ypaworld *yw);
void RoboMapZoom(NC_STACK_ypaworld *yw, int zoomDir);

void UpdateGunList(NC_STACK_ypaworld *yw);
void UpdateVehicleList(NC_STACK_ypaworld *yw);
void UpdateBuildingList(NC_STACK_ypaworld *yw);


void DrawGameMessages(NC_STACK_ypaworld *yw);

// The game map's art, shared with the replay viewer's map: a layer of tiles
// over the pixels from left, top to right, bottom of a map whose tiles are
// tileSize pixels, its top left at xpos, ypos from the screen's centre,
// each tile's glyph in tilesetId as ffunc gives it for its column and row
typedef int (*mapFunc)(NC_STACK_ypaworld *yw, int x, int y);
void RoboMapLayoutTileLayer(NC_STACK_ypaworld *yw, int tileSize, CmdStream *cmdbuf, int left, int top, int right, int bottom, int tilesetId, mapFunc ffunc, int xpos, int ypos);

// The game's map window, of the game's own scene or another's (MapScene),
// and the scene's unit of one of the game's
void CreateRoboMap(NC_STACK_ypaworld *yw, const MapScene *scene = NULL);
MapScene::Unit LiveMapUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
void RoboMap_InputHandle(NC_STACK_ypaworld *yw, TInputState *inpt);

// Where the pointer is over the map's view, in world x and z, and the unit
// drawn there, the player's first; false for none
vec2d RoboMapWorldAt(const TClickBoxInf *winp);
bool RoboMapUnitAt(NC_STACK_ypaworld *yw, const TClickBoxInf *winp, MapScene::Unit &out);

// The glyph of the buildings layer, four tiles a sector, at a tile of a
// sector of a type and simple type (TSectorDesc::SectorType) whose buildings
// are that healthy (x + 3 * y); between sectors, of where the ground steps
int MapLegoTile(NC_STACK_ypaworld *yw, int x, int y, int32_t typeId, int sectorType, const int16_t *health);

// A unit's glyph on the map at a zoom level: its type's in its owner's
// colour, or its vehicle's type icon with typeIcons
int MapUnitGlyph(NC_STACK_ypaworld *yw, uint8_t type, uint8_t owner, int vehicle, int zoomLevel, bool typeIcons);

// The glyph that shows how much energy a unit has left, a quarter at a time
uint8_t EnergyGlyph(int energy, int energyMax);

// What the energy panel's reload gauge shows of a host station of an owner
// on a sector of some power: whether it fills (1), drains (-1) or neither
// (0), the share of the sector's power the sector's owner gets, how much
// that gives in ten seconds, in hundreds, and the gauge's label
struct TReloadGauge
{
    int State = 0;
    int SectorPower = 0;
    float Ratio = 0.0;
    int Amount = 0;
    std::string Label;
};

TReloadGauge ReloadGauge(uint8_t owner, uint8_t sectorOwner, int sectorPower, float sectorRatio, int absReload);

void LayoutClippedTileString(TileMap *tyle, CmdStream *cur, const std::string &st, int maxWidth, char padChar);

// The squad manager's icons: a vehicle's type, and what a squad is doing
char VehicleTypeIcon(NC_STACK_ypaworld *yw, int vehicle);
char SquadStatusIcon(uint8_t status, int32_t statusFlags, uint8_t primaryType, uint8_t secondaryType);


typedef void ( *wis_color_func)(NC_STACK_ypaworld *, float, float, float, float, SDL_Color *, SDL_Color *);

int DrawTechUpgrade(NC_STACK_ypaworld *yw);
void DrawHud(NC_STACK_ypaworld *yw);
void yw_RenderHUDTarget(NC_STACK_ypaworld *yw, THudState *wis, const THudSubject &unit);

// The HUD of a unit: its compass, sights and bars, as of the HUD's time
// (_timeStamp); and the blinking line that says it or its host station is
// gone
THudSubject HudSubjectOf(NC_STACK_ypaworld *yw);
void DrawHudOf(NC_STACK_ypaworld *yw, CmdStream *buf, const THudSubject &unit);
void DrawHudDestroyed(NC_STACK_ypaworld *yw, CmdStream *buf, bool hostStation);

// How far a weapon has reloaded, in percent, so long after its last shot
// of a salvo so far
int WeaponReloadPercent(const World::TWeapProto &wpn, int sinceShot, int salveCounter);

void LoadHud(NC_STACK_ypaworld *yw);
void FreeHud(NC_STACK_ypaworld *yw);

// Where the HUD's radar is, from the screen's centre, as parts of its half
// width and height
const float HudRadarLeft = 0.45;
const float HudRadarTop = 0;
const float HudRadarRight = 0.95;
const float HudRadarBottom = 0.6;

// The marks over a unit under the pointer: a cursor in its owner's colour,
// and how much energy it has left; and over every enemy unit around the
// unit the player steers, with the enemy indicator preference
void yw_RenderCursorOverUnit(NC_STACK_ypaworld *yw, const MapScene::Unit &bact);
void yw_RenderUnitLifeBar(NC_STACK_ypaworld *yw, CmdStream *cur, const MapScene::Unit &bact);
void yw_RenderOverlayCursors(NC_STACK_ypaworld *yw, CmdStream *cur);


void yw_RenderHUDRadare(NC_STACK_ypaworld *yw);
void DrawGameStatus(NC_STACK_ypaworld *yw);


void UpdateReloadRatios(NC_STACK_ypaworld *yw);

void HandleWorldMouse(NC_STACK_ypaworld *yw, TInputState *arg);




int HandlePause(NC_STACK_ypaworld *yw, TUpdateArgs *arg);
void UpdatePlayerInRoboGun(NC_STACK_ypaworld *yw);
void UpdateSuperItems(NC_STACK_ypaworld *yw);
void UpdateGates(NC_STACK_ypaworld *yw);

void CheckEnemySectorEntered(NC_STACK_ypaworld *yw);


void FFeedback_Update(NC_STACK_ypaworld *yw);
void HandleSnapshotKeys(NC_STACK_ypaworld *yw, TInputState *inpt);




NC_STACK_bitmap * loadDisk_screen(NC_STACK_ypaworld *yw);
void draw_splashScreen(NC_STACK_ypaworld *yw, NC_STACK_bitmap *splashScreen);

void yw_RenderVector2D(NC_STACK_ypaworld *yw, UAskeleton::Data *wire, float posX, float posY, float m00, float m01, float m10, float m11, float scaleX, float scaleY, SDL_Color coloooor, wis_color_func color_func, wis_color_func color_func2, bool aspectCorrection = false);
void yw_debriefUpdate(NC_STACK_ypaworld *yw, TInputState *inpt);
void yw_DebriefRenderSectorsOwners(NC_STACK_ypaworld *yw, TBriefengScreen *brf);
void DebriefPlayHistory(NC_STACK_ypaworld *yw, TBriefengScreen *brf, World::History::Replay &history, uint32_t time, bool heard);

int ConfirmDialogGetState();
void OpenExitTrainingDialog(NC_STACK_ypaworld *yw);
void TakeScreenshot(NC_STACK_ypaworld *yw);



void AddChatMessage(NC_STACK_ypaworld *yw, const std::string &sender, const std::string &text);


void PlayChatVoiceSample(NC_STACK_ypaworld *yw, int arg);
void GetNetGemProtoIds(NC_STACK_ypaworld *yw, TMapGem *gemProt, int *vhclProtoId, int *buildProtoId);

void yw_NetMsgHndlLoop(NC_STACK_ypaworld *yw);
void yw_NetDrawStats(NC_STACK_ypaworld *yw);
#endif
