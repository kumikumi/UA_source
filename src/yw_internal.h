#ifndef  YWINT_H_INCLUDED
#define YWINT_H_INCLUDED

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
    cellArea *p_cell;
    int dword4;
    int dword8;
};


int yw_InitNetwork(NC_STACK_ypaworld *yw);




int loadTOD(NC_STACK_ypaworld *yw, const char *fname);
int writeTOD(NC_STACK_ypaworld *yw, const char *fname, int tod);

void RenderBriefingObject(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc, int a5);
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

void DeleteDirContents(const std::string &path);





int yw_write_callSign(NC_STACK_ypaworld *yw, const char *filename, const char *callsign);
int yw_write_user(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_input(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_sound(FSMgr::FileHandle *fil, UserData *usr);
int yw_write_video(FSMgr::FileHandle *fil, UserData *usr);
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
    int field_914 = 0;
    int buttonsY = 0;
    int pressedBtns = 0;
    int field_920 = 0;
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

class tehMap : public GuiBase
{
public:
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

void UpdateGunList(NC_STACK_ypaworld *yw);
void UpdateVehicleList(NC_STACK_ypaworld *yw);
void UpdateBuildingList(NC_STACK_ypaworld *yw);


void DrawGameMessages(NC_STACK_ypaworld *yw);


typedef void ( *wis_color_func)(NC_STACK_ypaworld *, float, float, float, float, SDL_Color *, SDL_Color *);

int DrawTechUpgrade(NC_STACK_ypaworld *yw);
void DrawHud(NC_STACK_ypaworld *yw);
void yw_RenderHUDTarget(NC_STACK_ypaworld *yw, THudState *wis);

void yw_RenderCursorOverUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);


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
void recorder_update_time(NC_STACK_ypaworld *yw, int dtime);
void recorder_write_frame(NC_STACK_ypaworld *yw);

int yw_write_levelnum(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
void yw_write_ownermap(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
void yw_write_buildmap(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
void yw_write_energymap(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int yw_write_units(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int yw_write_wunderinfo(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int yw_write_kwfactor(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int yw_write_globals(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int yw_write_superbomb(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int yw_write_history(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);
int yw_write_masks(NC_STACK_ypaworld *yw, FSMgr::FileHandle *fil);



NC_STACK_bitmap * loadDisk_screen(NC_STACK_ypaworld *yw);
void draw_splashScreen(NC_STACK_ypaworld *yw, NC_STACK_bitmap *splashScreen);

void yw_RenderVector2D(NC_STACK_ypaworld *yw, UAskeleton::Data *wire, float a3, float a4, float a5, float a6, float a7, float a8, float a9, float a10, SDL_Color coloooor, wis_color_func color_func, wis_color_func color_func2, bool aspectCorrection = false);
void yw_debriefUpdate(NC_STACK_ypaworld *yw, TInputState *inpt);

int ConfirmDialogGetState();
void TakeScreenshot(NC_STACK_ypaworld *yw);

int recorder_create_camera(NC_STACK_ypaworld *yw);
int recorder_go_to_frame(NC_STACK_ypaworld *yw, TGameRecorder *rcrd, int wanted_frame_id);
int recorder_open_replay(TGameRecorder *rcrd);
void recorder_stoprec(NC_STACK_ypaworld *yw);

void AdvanceReplay(NC_STACK_ypaworld *yw, TGameRecorder *rcrd, int a3);

void AddChatMessage(NC_STACK_ypaworld *yw, const std::string &a1, const std::string &a2);


void PlayChatVoiceSample(NC_STACK_ypaworld *yw, int arg);
void GetNetGemProtoIds(NC_STACK_ypaworld *yw, TMapGem *gemProt, int *a3, int *a4);

void yw_NetMsgHndlLoop(NC_STACK_ypaworld *yw);
void yw_NetDrawStats(NC_STACK_ypaworld *yw);
#endif
