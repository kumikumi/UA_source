#ifndef YW_H_INCLUDED
#define YW_H_INCLUDED

#include <string.h>
#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

#include "types.h"

#include "common/plane.h"

#include "system/gfx.h"
#include "system/inpt.h"
#include "system/sound.h"
#include "bitmap.h"
#include "ilbm.h"
#include "base.h"
#include "sklt.h"
#include "wav.h"
#include "ypabact.h"
#include "ypamissile.h"
#include "ypagun.h"

#include "lstvw.h"
#include "listnode.h"
#include "yw_viewdist.h"
#include "yw_replayui.h"

#include "world/consts.h"
#include "world/protos.h"
#include "world/tools.h"
#include "world/parsers.h"
#include "world/nparticle.h"
#include "world/player.h"


#include "world/tutorialhints.h"
#include "world/recorder.h"
#include "world/state.h"
#include "world/frame.h"
#include "world/playback.h"
#include "locale/locale.h"




class NC_STACK_ypaworld;
class NC_STACK_button;
class NC_STACK_windp;

struct TLego;
struct TSubSectorDesc;
struct TSectorDesc;
struct map_event;
struct uamessage_base;
struct uamessage_vhclData;
struct uamessage_vhclDataI;


struct TSectorCollision;

namespace UIWidgets {
    enum MAIN_MENU_WIDGET_IDS{
        BTN_SAVE_LOAD = 1001,
        BTN_INPUT_SETTINGS = 1003,
        BTN_OPTIONS = 1004,
        BTN_QUIT = 1007,
        BTN_LANGUAGE = 1008,
        BTN_MULTIPLAYER = 1016,
        BTN_REPLAYS = 1017,
        BTN_SINGLE_PLAYER = 1018
    };
    enum MAIN_MENU_EVENT_IDS{
        ALL_DOWN = 1251,
        BTN_SINGLE_PLAYER_UP = 1024,
        BTN_MULTIPLAYER_UP = 1022,
        BTN_INPUT_SETTINGS_UP = 1007,
        BTN_OPTIONS_UP = 1005,
        BTN_SAVE_LOAD_UP = 1001,
        BTN_LANGUAGE_UP = 1011,
        BTN_REPLAYS_UP = 1025,
        BTN_QUIT_UP = 1013
    };
    enum NETWORK_MENU_WIDGET_IDS{
        TXT_MENU_TITLE = 1204,
        TXT_MENU_DESCR_LINE1 = 1222,
        TXT_MENU_DESCR_LINE2 = 1223,
        BTN_CREATE_SESSTION = 1202,
        TXTBOX = 1200
    };

}

namespace World
{
   
enum
{
    FRACTION_MAXCOUNT = 8,
};
    
enum BLIST_TYPES
{
    BLIST_UNITS = 1, // Used in world for store all bacts
    BLIST_CACHE = 2, // Used in world for store cached dead bacts
    BLIST_KIDS  = 3, // Used in bact class for store kid units
};

enum OWNERID
{
    OWNER_NOCHANGE  = -2,
    OWNER_RECALC    = -1,
    OWNER_UNKNOW    = 0,
    OWNER_0         = 0,
    OWNER_1         = 1,
    OWNER_RESIST    = OWNER_1,
    OWNER_RESIST_BIT = (1 << OWNER_RESIST),
    OWNER_2         = 2,
    OWNER_SULG      = OWNER_2,
    OWNER_SULG_BIT  = (1 << OWNER_SULG),
    OWNER_3         = 3,
    OWNER_MYKO      = OWNER_3,
    OWNER_MYKO_BIT  = (1 << OWNER_MYKO),
    OWNER_4         = 4,
    OWNER_TAER      = OWNER_4,
    OWNER_TAER_BIT  = (1 << OWNER_TAER),
    OWNER_5         = 5,
    OWNER_BLACK     = OWNER_5,
    OWNER_BLACK_BIT  = (1 << OWNER_BLACK),
    OWNER_6         = 6,
    OWNER_GHOR      = OWNER_6,
    OWNER_GHOR_BIT  = (1 << OWNER_GHOR),
    OWNER_7         = 7,
};

enum COLOR_IDS
{
	COLOR_OWNER_0 = 0,
	COLOR_OWNER_1 = 1,
	COLOR_OWNER_2 = 2,
	COLOR_OWNER_3 = 3,
	COLOR_OWNER_4 = 4,
	COLOR_OWNER_5 = 5,
	COLOR_OWNER_6 = 6,
	COLOR_OWNER_7 = 7,

	COLOR_MAP_DIRECTION 	= 8,
	COLOR_MAP_PRIMTARGET 	= 9,
	COLOR_MAP_SECTARGET 	= 10,
	COLOR_MAP_COMMANDER 	= 11,
	COLOR_MAP_DRAGBOX 		= 12,
	COLOR_MAP_VIEWER 		= 13,

	COLOR_HUD_WEAPON_0 			= 20,
	COLOR_HUD_COMPASS_CMDVEC_0 	= 21,
	COLOR_HUD_COMPASS_PRIMTGT_0 = 22,
	COLOR_HUD_COMPASS_LOCKTGT_0 = 23,
	COLOR_HUD_COMPASS_0 		= 24,
	COLOR_HUD_VEHICLE_0 		= 25,
	COLOR_HUD_VISOR_MG_0 		= 26,
	COLOR_HUD_VISOR_LOCKED_0 	= 27,
	COLOR_HUD_VISOR_AUTONOM_0 	= 28,

	COLOR_HUD_WEAPON_1 			= 29,
	COLOR_HUD_COMPASS_CMDVEC_1 	= 30,
	COLOR_HUD_COMPASS_PRIMTGT_1 = 31,
	COLOR_HUD_COMPASS_LOCKTGT_1 = 32,
	COLOR_HUD_COMPASS_1 		= 33,
	COLOR_HUD_VEHICLE_1 		= 34,
	COLOR_HUD_VISOR_MG_1 		= 35,
	COLOR_HUD_VISOR_LOCKED_1 	= 36,
	COLOR_HUD_VISOR_AUTONOM_1 	= 37,

	COLOR_BRIEF_LINES 	= 50,

	COLOR_TEXT_DEFAULT 		= 60,
	COLOR_TEXT_LIST 		= 61,
	COLOR_TEXT_LIST_SEL		= 62,
	COLOR_TEXT_TOOLTIP		= 63,
	COLOR_TEXT_MESSAGE		= 64,
	COLOR_TEXT_HUD			= 65,
	COLOR_TEXT_BRIEFING		= 66,
	COLOR_TEXT_DEBRIEFING	= 67,
	COLOR_TEXT_BUTTON		= 68,

	COLOR_MAX_NUMBER		= 70,
};

enum MOVIE_IDS
{
	MOVIE_INTRO		= 0,
	MOVIE_LOSE		= 1,
	MOVIE_WIN		= 2,
	MOVIE_USER		= 3,
	MOVIE_KYT		= 4,
	MOVIE_TAER		= 5,
	MOVIE_MYK		= 6,
	MOVIE_SULG		= 7,
	MOVIE_BLACK		= 8,

	MOVIE_MAX_NUMBER = 9
};

enum PREFERENCES_FLAGS
{
    PREF_JOYDISABLE     =  (1 << 0), //    4,
    PREF_FFDISABLE      =  (1 << 1), //    8,
    PREF_ENEMYINDICATOR =  (1 << 2), // 0x20,
    PREF_CDMUSICDISABLE =  (1 << 4), // 0x100,
    PREF_ALTJOYSTICK    =  (1 << 5), // 0x200,
};

enum INPUT_BIND
{
    INPUT_BIND_TYPE_BUTTON        = 1,
    INPUT_BIND_TYPE_SLIDER        = 2,
    INPUT_BIND_TYPE_HOTKEY        = 3,

    INPUT_BIND_PAUSE      = 1,
    INPUT_BIND_QUIT       = 2,
    INPUT_BIND_DRIVE_DIR  = 3,
    INPUT_BIND_DRIVE_SPEED= 4,
    INPUT_BIND_GUN_HEIGHT = 5,
    INPUT_BIND_FLY_HEIGHT = 6,
    INPUT_BIND_FLY_SPEED  = 7,
    INPUT_BIND_FLY_DIR    = 8,
    INPUT_BIND_BRAKE      = 9,
    INPUT_BIND_FIRE       = 10,
    INPUT_BIND_CAMFIRE    = 11,
    INPUT_BIND_GUN        = 12,
    INPUT_BIND_SET_COMM   = 13,
    INPUT_BIND_HUD        = 14,
    INPUT_BIND_AUTOPILOT  = 15,
    INPUT_BIND_ORDER      = 16,
    INPUT_BIND_NEW        = 17,
    INPUT_BIND_ADD        = 18,
    INPUT_BIND_SQ_MANAGE  = 19,
    INPUT_BIND_AGGR_1     = 20,
    INPUT_BIND_AGGR_2     = 21,
    INPUT_BIND_AGGR_3     = 22,
    INPUT_BIND_AGGR_4     = 23,
    INPUT_BIND_AGGR_5     = 24,
    INPUT_BIND_MAP        = 25,
    INPUT_BIND_WAPOINT    = 26,
    INPUT_BIND_LANDLAYER  = 27,
    INPUT_BIND_OWNER      = 28,
    INPUT_BIND_HEIGHT     = 29,
    INPUT_BIND_MINIMAP    = 30,
    INPUT_BIND_LOCKVIEW   = 31,
    INPUT_BIND_ZOOMIN     = 32,
    INPUT_BIND_ZOOMOUT    = 33,
    INPUT_BIND_LOG_WND    = 34,
    INPUT_BIND_CONTROL    = 35,
    INPUT_BIND_LAST_SEAT  = 36,
    INPUT_BIND_ATTACK     = 37,
    INPUT_BIND_TO_HOST    = 38,
    INPUT_BIND_TO_COMM    = 39,
    INPUT_BIND_NEXT_UNIT  = 40,
    INPUT_BIND_NEXT_COMM  = 41,
    INPUT_BIND_LAST_MSG   = 42,
    INPUT_BIND_TO_ALL     = 43,
    INPUT_BIND_HELP       = 44,
    INPUT_BIND_ANALYZER   = 45,
    INPUT_BIND_REWIND     = 46,

    INPUT_BIND_MAX        = 47,
};

enum SOUND_ID
{
    SOUND_ID_VOLUME = 0,
    SOUND_ID_RIGHT  = 1,
    SOUND_ID_LEFT   = 2,
    SOUND_ID_BUTTON = 3,
    SOUND_ID_QUIT   = 4,
    SOUND_ID_SLIDER = 5,
    SOUND_ID_WELCOME = 6,
    SOUND_ID_MENUOPEN = 7,
    SOUND_ID_OVERLEVEL = 8,
    SOUND_ID_LEVELSEL = 9,
    SOUND_ID_TEXTAPPEAR = 10,
    SOUND_ID_OBJAPPEAR = 11,
    SOUND_ID_SECTCONQ = 12,
    SOUND_ID_VHCLDESTR = 13,
    SOUND_ID_BLDGCONQ = 14,
    SOUND_ID_TIMERCOUNT = 15,

    SOUND_ID_SELECT = 16,
    SOUND_ID_ERROR  = 17,
    SOUND_ID_ATTEN  = 18,
    SOUND_ID_SECRET = 19,
    SOUND_ID_PLASMA = 20,
    
    SOUND_ID_CHAT = 21,
    
    SOUND_ID_MAX
};

enum GFX_FLAG
{
    GFX_FLAG_SKYRENDER          = (1 << 1),
    GFX_FLAG_WINDOWED           = (1 << 5),
    GFX_FLAG_VSYNC              = (1 << 6),
};

enum SAVE_DATA_FLAG
{
    SDF_USER     = (1 << 0),
    SDF_INPUT    = (1 << 1),
    SDF_VIDEO    = (1 << 2),
    SDF_SOUND    = (1 << 3),
    SDF_SCORE    = (1 << 4),
    SDF_SHELL    = (1 << 5),
    SDF_PROTO    = (1 << 6),
    SDF_BUDDY    = (1 << 7),
    SDF_GAME     = (1 << 8), // The AI timeline
    
    SDF_ALL      = (SDF_USER | SDF_INPUT | SDF_VIDEO | SDF_SOUND | SDF_SCORE | SDF_SHELL | SDF_PROTO | SDF_BUDDY | SDF_GAME),
};

enum SOUND_FLAGS
{
    SF_INVERTLR  = (1 << 0),
    SF_CDSOUND   = (1 << 4),
};

enum UPGRADE_TYPES
{
    UPGRADE_NONE       = 0,
    UPGRADE_WEAPON     = 1,
    UPGRADE_ARMOR      = 2,
    UPGRADE_VEHICLE    = 3,
    UPGRADE_BUILDING   = 4,
    UPGRADE_RADARE     = 5,
    UPGRADE_BLDVHCL    = 6,
    UPGRADE_GENERIC    = 7,
};



class AssignParser : public ScriptParser::DataHandler
{
public:
	virtual int Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2);
	virtual bool IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt) { return !StriCmp(word, "begin_assign"); };

	static ScriptParser::DataHandler *MakeParser() {return new AssignParser;};
};


enum DOACTION
{
    DOACTION_0 = 0,
    DOACTION_1,
    DOACTION_2,
    DOACTION_ADD_UNIT1,
    DOACTION_ADD_UNIT2,
    DOACTION_5,
    DOACTION_6,
    DOACTION_7,
    DOACTION_8,
    DOACTION_9,
    DOACTION_10,
    DOACTION_WP_BEGIN,
    DOACTION_WP_ADD,
    DOACTION_WP_CYCLE,
    DOACTION_19
};




namespace Log
{
enum LOGMSG
{
    LOGMSG_UPGRADE_WEAPON   = 25,
    LOGMSG_UPGRADE_ARMOR    = 26,
    LOGMSG_UPGRADE_VEHICLE  = 27,
    LOGMSG_UPGRADE_BUILDING = 28,
    LOGMSG_UPGRADE_RADARE   = 78,
    LOGMSG_UPGRADE_BLDVHCL  = 79,
    LOGMSG_UPGRADE_GENERIC  = 80,
};

inline uint32_t GetUpgradeLogID(uint8_t upg)
{
    switch ( upg )
    {
    case UPGRADE_WEAPON:    return LOGMSG_UPGRADE_WEAPON;
    case UPGRADE_ARMOR:     return LOGMSG_UPGRADE_ARMOR;
    case UPGRADE_VEHICLE:   return LOGMSG_UPGRADE_VEHICLE;
    case UPGRADE_BUILDING:  return LOGMSG_UPGRADE_BUILDING;
    case UPGRADE_RADARE:    return LOGMSG_UPGRADE_RADARE;
    case UPGRADE_BLDVHCL:   return LOGMSG_UPGRADE_BLDVHCL;
    default:                return UPGRADE_GENERIC;
    }
}
}

}



#include "world/history.h"







struct ProfilesNode
{
    int totalElapsedTime = 0;
    uint8_t fraction = 0;
    std::string name;
};

typedef std::list<ProfilesNode> ProfileList;




enum
{
    ENVMODE_TITLE = 1,
    ENVMODE_INPUT = 2,
    ENVMODE_SETTINGS = 3,
    ENVMODE_TUTORIAL = 4,
    ENVMODE_SINGLEPLAY = 5,
    ENVMODE_NETPLAY = 6,
    ENVMODE_SELLOCALE = 7,
    ENVMODE_ABOUT = 8,
    ENVMODE_SELPLAYER = 9,
    ENVMODE_REPLAYS = 10
};

struct EnvAction
{
    enum
    {
        ACTION_NONE     = 0,
        ACTION_QUIT     = 1,
        ACTION_PLAY     = 2,
        ACTION_LOAD     = 3,
        ACTION_NETPLAY  = 4,
        ACTION_CONTINUE = 5, // The level left unfinished (UnfinishedLevel)
        ACTION_VIEW_REPLAY = 6 // UserData::replayToView, in the replay viewer
    };

    int8_t action = ACTION_NONE;
    int32_t params[5] = {0};
};

// A replay kept in the profile's replays folder, as the replays page lists
// it
struct TReplayFile
{
    std::string Path;
    std::string Name;     // Of the file
    std::string Date;     // When it was saved, as its name has it
    int Level = 0;
    std::string LevelDescription; // As RecordedLevel gives it
    int32_t Duration = 0; // Game time, in 1/1024 s
    uint64_t Size = 0;    // Of the file, in bytes
};

struct TReplayPreview;

class UserData
{
friend class World::Parsers::ShellSoundParser;

public:
    enum
    {
        NETSCREEN_MODE_SELECT = 0,
        NETSCREEN_SESSION_SELECT = 1, // Only on dedicated server
        NETSCREEN_ENTER_NAME  = 2,
        NETSCREEN_ENTER_IP = 5,
        NETSCREEN_CHOOSE_MAP  = 3,
        NETSCREEN_INSESSION = 4,
    };
    
    enum
    {
        ICHG_JOYSTICK       = (1 << 0),
        ICHG_FORCEFEEDBACK  = (1 << 1),
        ICHG_ALTJOYSTICK    = (1 << 2)
    };

public:
    struct TInputConf
    {
        uint8_t Type = 0;
        int16_t KeyID = 0;
        int16_t PKeyCode = 0;
        int16_t NKeyCode = 0;
        int16_t PKeyCodeBkp = 0;
        int16_t NKeyCodeBkp = 0;
        int16_t PKeyCodeDef = 0;
        int16_t NKeyCodeDef = 0;
        uint8_t SetFlags = 0; // Flags what key is setted (first/second)
        
        TInputConf() = default;        
        TInputConf(uint8_t type, int16_t keyID, int16_t pKey, int16_t nKey = 0)
        : Type(type), KeyID(keyID), PKeyCode(pKey), NKeyCode(nKey)
        , PKeyCodeBkp(pKey), NKeyCodeBkp(nKey), PKeyCodeDef(pKey), NKeyCodeDef(nKey)
        {};
        
        enum {
            IF_FIRST  = (1 << 0),
            IF_SECOND = (1 << 1)
        };
    };

public:
    bool HasInited = false;
    bool IgnoreScoreSaving = false;
    bool GameIsOver = false; // If it's opened after complete game

    // Started straight into a level from the command line (--level, --load,
    // --bench): nothing is written to the player's profile, the exit menu
    // has no Save or Load, and the game quits after the level
    bool DirectStart = false;

    bool ResetInputPeriod = false;
    std::string UserName;

    NC_STACK_ypaworld *p_YW = NULL;
    TInputState *Input = NULL;
    int32_t DTime     = 0;
    uint32_t GlobalTime = 0;

    int EnvMode = 0; 
    bool EnvModeChanged = false;

    bool returnToTitle;

    TSndCarrier samples1_info;
    std::array<NC_STACK_sample *, World::SOUND_ID_MAX> samples1 = Common::ArrayInit<NC_STACK_sample *, World::SOUND_ID_MAX>(NULL);
    
    NC_STACK_button *sub_bar_button;

    NC_STACK_button *titel_button;
    NC_STACK_button *button_input_button;
    GuiList input_listview;
    int32_t inpListActiveElement;
    bool confFirstKey; // What key we will config for sliders (first or second)
    bool joystickEnabled;
    bool confJoystickEnabled; // Apply this to joystickEnabled on press "OK"
    bool altJoystickEnabled;
    bool confAltJoystickEnabled; // Apply this to altJoystickEnabled on press "OK"
    bool confFFEnabled;
    bool keyCatchMode; // mode for catch input to configure key
    uint8_t inputChangedParts;
    
    NC_STACK_button *video_button;
    GuiList d3d_listvw;
    
    std::string win3d_guid;
    std::string win3d_name;
    std::string conf3DGuid;
    std::string conf3DName;
    
    int16_t fxnumber;
    int16_t confFxNumber;
    uint8_t GFXFlags;
    uint8_t confGFXFlags;

    
    uint8_t soundFlags;
    uint8_t confSoundFlags;
    
    int16_t soundVolume;
    int16_t confSoundVolume;
    
    int16_t musicVolume;
    int16_t confMusicVolume;
    
    bool enemyIndicator;
    bool confEnemyIndicator;

    // The options the port adds: the engines and the world hold these
    // settings; these are the settings screen's choices until OK.
    bool confFiltering;
    bool confMovies;
    int confMaxFps;
    int confSoundChannels;
    int32_t confAITimeline;

    // _settingsChangeOptions bits of the port's options
    enum
    {
        OPT_CHG_FILTERING  = 0x4000,
        OPT_CHG_MOVIES     = 0x8000,
        OPT_CHG_MAXFPS     = 0x20000,
        OPT_CHG_CHANNELS   = 0x40000,
        OPT_CHG_AITIMELINE = 0x80000,
    };

    // The frame rate limit slider's steps, as gfx.maxfps: 0 follows the
    // display and -1 sets no limit
    static constexpr std::array<int, 7> FpsLimits {{0, 30, 60, 90, 120, 240, -1}};

    int _settingsChangeOptions;
    
    NC_STACK_button *disk_button;
    GuiList disk_listvw;
    int32_t diskListActiveElement;
    
    std::string userNameDir;
    int16_t userNameDirCursor;
    
    int16_t diskScreenMode;
    
    ProfileList profiles;

    uint8_t savedDataFlags; // SDF flags - data types loaded from settings

    bool diskEnterFromMapSelect;
    
    NC_STACK_button *locale_button;
    Engine::StringList lang_dlls;
    GuiList local_listvw;
    std::string *default_lang_dll;
    std::string *prev_lang;
    
    NC_STACK_button *about_button;
    uint32_t aboutDlgLastKeyTime;
    int8_t aboutDlgKeyCount;

    NC_STACK_button *network_button;

    // The replays page (yw_replays.cpp): the profile's replays, newest
    // first, the one chosen, and its map, drawn in an area of the page
    NC_STACK_button *replays_button = NULL;
    GuiList replays_listvw;
    std::vector<TReplayFile> replays;
    int replaysSelected = 0;
    TReplayPreview *replayPreview = NULL;
    std::string replayPreviewOf; // The replay it was made for, or couldn't be
    Common::Rect replaysMapArea;

    // The replay the viewer is to show (EnvAction::ACTION_VIEW_REPLAY), and
    // whether the debriefing it was started from shows again after
    std::string replayToView;
    bool replayFromDebriefing = false;
    GuiList network_listvw;

    int16_t netListY;

    int16_t netSelMode;
    int16_t netSel;
    int nInputMode;
    std::string netName;

    int16_t netNameCurPos;
    int16_t netLevelID;
    std::string netLevelName;

    std::string netPlayerName;
    uint32_t netCRC;
    int32_t takTime;
    uint8_t netPlayerOwner;
    uint8_t FreeFraction;
    uint8_t SelectedFraction;
    
    bool netGameCanStart;
    bool isHost;
    bool modemAskSession;

    uint32_t msgcount;
    bool isWelcmd;
    bool rdyStart;
    bool remoteMode;
    bool disconnected;
    bool blocked;
    
    int32_t problemTimer;

    std::deque<std::string> msgBuffers;
    std::string lastSender;

    struct TMapDescription
    {
        int id = 0;
        std::string pstring;
        
        TMapDescription() = default;
        TMapDescription(int __id, const std::string &__pstring)
        : id(__id), pstring(__pstring) {};
        
        static bool compare(const TMapDescription &lhs, const TMapDescription &rhs)
        {
            return StriCmp(lhs.pstring, rhs.pstring) > 0;
        }
    };
    
    std::vector< TMapDescription > mapDescriptions;

    bool noSent;
    bool sentAQ; // sended annonce of quit
    
    uint8_t netProblemOwner;
    std::string netProblemName;
    int32_t netProblem;
    int32_t netAllOk;
    int32_t update_time_norm;
    int32_t flush_time_norm;
    int32_t kickTime;
    int32_t latencyCheck;
    int32_t netProblemCountDown;
    int32_t netAllOkCountDown;
    uint32_t deadCheckTime;
    int32_t sendScoreCountDown;
    
    struct TNetPlayerLobbyData
    {
        std::string Name;
        std::string Msg;
        uint8_t  NetFraction = 0;
        bool     IsTrouble   = false;
        uint8_t  Owner       = 0;
        bool     Ready       = false;
        bool     Welcomed    = false;
        int32_t  UpdateCountDown = 0;
        uint32_t DataChecksum   = 0;
    };

    struct TNetPlayerData
    {
        std::string Name;
        uint8_t  Owner       = 0;
        bool     Ready       = false;
        uint8_t DestroyFlags = 0;
        uint8_t Status       = 0;
        bool ConnectionProblem = false;
        uint32_t LastMessageTime = 0;
        uint32_t TimeStamp   = 0;
        uint32_t MessageCount = 0;
        int32_t ProblemCountDown = 0;
        int32_t Latency      = 0;
    };
    
    
    std::array<TNetPlayerData, World::CVFractionsCount> netPlayers;
    std::array<TNetPlayerLobbyData, World::CVMaxNetPlayers> lobbyPlayers;
    
    
    NC_STACK_button *confirm_button;
    
    uint8_t confirmMode;
    // Kept so the dialog can be shown again after the shell is laid out anew.
    std::string confirmText1;
    std::string confirmText2;
    int confirmSingleButton = 0;

    EnvAction envAction;

    std::array<TInputConf, World::INPUT_BIND_MAX>  InputConfig;
    std::array<std::string, World::INPUT_BIND_MAX> InputConfigTitle;

    /* SGM save exist
       0 - not checked
       1 - exist
       2 - not exist, but checked*/
    int8_t sgmSaveExist = 0; 

    // The level UnfinishedLevel found, 0 for none, -1 not looked for yet
    int continueLevel = -1;
    
    int16_t shelltrack;
    int16_t missiontrack;
    int16_t loadingtrack;
    int16_t debriefingtrack;
    
    struct TAudioTrackDelays
    {
        int32_t min_delay = 0;
        int32_t max_delay = 0;
    };
    
    TAudioTrackDelays shelltrack__adv;
    TAudioTrackDelays missiontrack__adv;
    TAudioTrackDelays loadingtrack__adv;
    TAudioTrackDelays debriefingtrack__adv;
    
    uint32_t netsend_count;
    uint32_t netrecv_count;
    uint32_t netrecv_time;
    uint32_t netsend_speed;
    uint32_t netrecv_speed;
    uint32_t netrecv_bpsMin;
    uint32_t netrecv_bpsMax;
    uint32_t netrecv_bpsAvr;
    uint32_t netsend_bpsMin;
    uint32_t netsend_bpsMax;
    uint32_t netsend_bpsAvr;
    uint32_t net_gcnt;
    uint32_t net_packet_size;
    uint32_t net_packet_min;
    uint32_t net_packet_max;
    uint32_t net_packet_cnt;
    uint32_t net_packet_avr;

public:
    void clear();

    void SaveProfile();
    void CreateNewProfile();
    void ApplyOptions();
    void ShowSaveLoadMenu();
    void ShowInputSettings();
    void ShowLanguageMenu();
    void yw_returnToTitle();
    void InputPageCancel();
    void InputConfCancel();
    void CancelOptions();
    // Show the port's options as they are set, and take them as choices
    void SetEngineOptionWidgets();
    static int FpsLimitStep(int fps); // The slider step nearest to it
    static std::string FpsLimitText(int fps);
    static std::string AITimelineText(int32_t ms);
    void LoadProfile();
    void DeleteProfile();
    void ApplySelectedLanguage();
    void ExitFromLanguageMenu();

    void GameShellUiHandleInput();
    void NetCallsignChosen();
    bool StartNetworkGame(int hostLevel, const std::string &address, const std::string &callsign);
    void ConnectToServer(std::string connStr);
    void ShowToolTip(int id);
    void ExitFromNetworkToMain();
    void yw_NetPrintStartInfo();

    void AfterMapChoose();
    void yw_NetOKProvider();
    void yw_JoinNetGame();
    void JoinLobbyLessGame();
    void LogNetStatistics();
    void yw_FractionInit();
    void yw_netcleanup();
    void StartNetGame();
    void HideConfirmDialog();
    void ShowOptionsMenu();
    void GameShellUiOpenNetwork();
    int IsSaveGameOutdated();
    void ShowConfirmDialog(int mode, const std::string &txt1, const std::string &txt2, int singleButton);
    void ReloadAllInputs();
    void InputConfCopyToBackup();
    void InputConfigRestoreDefault();
    void  UpdateSelected3DDevFromList();
    void CloseSaveLoadMenu();
    void ShowAbout();
    
    bool SaveBuildProtoState();
    void SaveSettings();
    

    bool  ShellSoundsLoad();

    static int InputIndexFromConfig(uint32_t type, uint32_t index);
    // The profile's saved game, a recording of the level up to when it was
    // saved
    static std::string SaveGamePath(const std::string &username);
    static bool HasSaveGame(const std::string &username);

    // The recording of the level the profile plays in the campaign, and
    // that level when it was left unfinished: Continue goes on with it
    static std::string SessionPath(const std::string &username);
    int UnfinishedLevel();
    void ContinueGame();
    void LookForSavedGamesAgain();

    // The profile's replays, and the page that lists them
    static std::string ReplaysPath(const std::string &username);
    void FindReplays();
    void ShowReplays();
    void CloseReplays();
    void ReplaysUiHandleInput();
    void ViewSelectedReplay();
    void DeleteSelectedReplay();
    void LoadReplayPreview();
    void FreeReplayPreview();
    void SaveDebriefReplay();
    
    bool IsWindowedFlag()
    {
        return (GFXFlags & World::GFX_FLAG_WINDOWED) != 0;
    }

    // The player name on the load/save screen, or the name, address or chat
    // line on the network screens, takes typed characters.
    bool IsTypingText() const
    {
        return (EnvMode == ENVMODE_SELPLAYER && diskScreenMode) ||
               (EnvMode == ENVMODE_NETPLAY && nInputMode);
    }

    bool IsVsyncFlag()
    {
        return (GFXFlags & World::GFX_FLAG_VSYNC) != 0;
    }

protected:
    bool LoadSample(int sampleID, const std::string &file);
};

enum CELL_PFLAGS
{
    CELL_PFLAGS_IN_CLST = 1, // Sector in close list
    CELL_PFLAGS_IN_OLST = 2  // Sector in open list
};

struct cellArea
{    
    int32_t Id = 0;
    Common::Point CellId;

    float addit_cost = 0.0; // Additional cost for ground units
    uint8_t pf_flags = 0; // Pathfind flags
    float cost_to_this = 0.0;
    float cost_to_target = 0.0;
    cellArea *pf_treeup = NULL;
 
    enum CF
    {
        // Not playable sector, used as border
        CF_BORDER = (1 << 0),
    };
    
    uint8_t Flags = 0;

    uint8_t owner = 0;
    int32_t type_id = 0; // Index in array
    uint8_t SectorType = 0; // Complex (3x3) or simple
    int32_t energy_power = 0; // Cell electric power
    Common::PlaneArray<int16_t, 3, 3> buildings_health = Common::PlaneArray<int16_t, 3, 3>::ArrayInit(0);
    uint8_t view_mask = 0; // Who can view this sector (mask)
    uint8_t UnhideMask = 0; // Who can unhide other fractions (mask)
    
    enum PTYPE
    {
        PT_NONE            = 0,  // Oridnary sector 
        PT_CONSTRUCTING    = 1,  // Building construct process 
        PT_POWERSTATION    = 2,
        PT_BUILDINGS       = 3,
        PT_TECHUPGRADE     = 4,
        PT_GATECLOSED      = 5,
        PT_GATEOPENED      = 6,
        PT_TECHDEACTIVE    = 7,
        PT_STOUDSON        = 8
    };
    uint8_t PurposeType = PT_NONE;
    int32_t PurposeIndex = 0;
    
    World::RefBactList unitsList; // Units in this sector
    vec3d CenterPos;
    float height = 0.0;
    float averg_height = 0.0;
    
    cellArea() : unitsList(this, NC_STACK_ypabact::GetCellRefNode) {};
    
    bool IsCanSee(int owant) const
    {
        return (ViewMask(owant) & view_mask) != 0;
    }
    
    void AddToViewMask(int ownr)
    {
        view_mask |= ViewMask(ownr);
    }
    
    void DelFromViewMask(int ownr)
    {
        view_mask &= ~ViewMask(ownr);
    }
    
    inline static int32_t ViewMask(int ownr) { return (1 << ownr); }
    
    bool IsUnhideFor(int owant) const { return (ViewMask(owant) & UnhideMask) != 0; }
    
    void AddUnhideMask(int ownr) { UnhideMask |= ViewMask(ownr); }
    
    void DelUnhideMask(int ownr) { UnhideMask &= ~ViewMask(ownr); }
    
    
    int GetEnergy()
    {
        if ( SectorType == 1 )
            return buildings_health.At(0, 0);

        int e = 0;
        for (auto hlth : buildings_health)
            e += hlth;
        
        return e;
    }
    
    inline bool IsBorder() const
    {
        return (Flags & CF_BORDER) != 0;
    }
    
    inline bool IsGamePlaySector() const
    {
        return (Flags & CF_BORDER) == 0;
    }
};

struct TCellFillerCh : TObjectCache
{
    int8_t Id1 = -1;
    int8_t Id2 = -1;
    float  Heights[4] = {0.};
    
    void FreeVBO();
    void MakeVBO();
    
    ~TCellFillerCh();
};

struct TMapKeySector
{
    Common::Point CellId;
    cellArea *PCell = NULL;
};

struct TMapGate
{
    cellArea *PCell = NULL;
    Common::Point CellId;
    int ClosedBldID = 0;
    int OpenBldID = 0;
    std::vector<int> PassToLevels;
    std::vector<TMapKeySector> KeySectors;
    int MbStatus = 0;
};

struct MapSquad
{
    bool Useable;
    int Owner;
    int VhclID;
    int Count;
    float X;
    float Z;
    int MbStatus;

    MapSquad()
    {
        clear();
    }

    void clear()
    {
        Useable = false;
        Owner = 0;
        VhclID = 0;
        Count = 0;
        X = 0.;
        Z = 0.;
        MbStatus = 0;
    }
};

struct MapRobo
{
    int16_t Owner;
    int16_t VhclID;
    vec3d Pos;
    int Energy;
    int ConBudget;
    int RadBudget;
    int PowBudget;
    int DefBudget;
    int ReloadConst;
    int SafBudget;
    int RecBudget;
    int CplBudget;
    int RobBudget;
    int MbStatus;
    int16_t ViewAngle;
    int SafDelay;
    int PowDelay;
    int RadDelay;
    int CplDelay;
    int DefDelay;
    int ConDelay;
    int RecDelay;
    int RobDelay;

    MapRobo()
    {
        Owner = 0;
        VhclID = 0;
        Pos = vec3d();
        Energy = 0;
        ConBudget = 0;
        RadBudget = 0;
        PowBudget = 0;
        DefBudget = 0;
        ReloadConst = 0;
        SafBudget = 0;
        RecBudget = 0;
        CplBudget = 0;
        RobBudget = 0;
        MbStatus = 0;
        ViewAngle = 0;
        SafDelay = 0;
        PowDelay = 0;
        RadDelay = 0;
        CplDelay = 0;
        DefDelay = 0;
        ConDelay = 0;
        RecDelay = 0;
        RobDelay = 0;
    }
};

struct TMapGem
{
    int16_t BuildingID = 0;
    Common::Point CellId;
    int MbStatus = 0;
    std::string ScriptFile;
    std::string MsgDefault;
    int16_t NwVprotoNum1 = 0;
    int16_t NwVprotoNum2 = 0;
    int16_t NwVprotoNum3 = 0;
    int16_t NwVprotoNum4 = 0;
    int16_t NwBprotoNum1 = 0;
    int16_t NwBprotoNum2 = 0;
    int16_t NwBprotoNum3 = 0;
    int16_t NwBprotoNum4 = 0;
    Engine::StringList ActionsList;
    int Type = 0;
};

struct TBkgPicInfo
{
    Common::Point Size;
    std::string PicName;
};

struct TLevelDescription
{
    enum BITS
    {
        BIT_SET = (1 << 0),
        BIT_SKY = (1 << 1),
        BIT_TYP = (1 << 2),
        BIT_OWN = (1 << 3),
        BIT_HGT = (1 << 4),
        BIT_BLG = (1 << 5),

        // A level needs no host stations: factions can do without
        BIT_ALL = (BIT_SET | BIT_SKY | BIT_TYP | BIT_OWN | BIT_HGT | BIT_BLG),
    };
    
    uint32_t ReadedPartsBits = 0;
    int32_t SetID = 0;
    int32_t EventLoopID = 0;
    Common::Point MapSize;
    bool SlowConnection = false;
    std::string SkyStr;
    std::string TypStr;
    std::string OwnStr;
    std::string HgtStr;
    std::string BlgStr;
    std::vector<MapRobo> Robos;
    std::vector<MapSquad> Squads;
    std::array<std::string, 8> Palettes;
    std::vector<TBkgPicInfo> Mbmaps;
    std::vector<TBkgPicInfo> Dbmaps;

    // The faction the player plays: begin_level's player_owner, or the
    // first host station's; 0 when neither says
    int32_t PlayerOwner = 0;
    
    bool IsOk() const { return (ReadedPartsBits & BIT_ALL) == BIT_ALL; }
};

struct TMapBuddy
{
    int32_t CommandID = 0;
    int16_t Type = 0;
    int32_t Energy = 0;

    TMapBuddy() = default;
    
    TMapBuddy( int32_t cmdID, int16_t tp, int32_t e)
    : CommandID(cmdID), Type(tp), Energy(e)
    {}
};

struct TMapSuperItem
{
    enum
    {
        TYPE_BOMB = 1,
        TYPE_WAVE = 2,
        
        STATE_INACTIVE = 0,
        STATE_ACTIVE   = 1,
        STATE_STOPPED  = 2,
        STATE_TRIGGED  = 3,
    };
    
    int Type = 0;
    int State = STATE_INACTIVE;
    int32_t TimerValue = 0;
    cellArea *PCell = NULL;
    Common::Point CellId;
    int32_t InactiveBldID = 0;
    int32_t ActiveBldID = 0;
    int32_t TriggerBldID = 0;
    std::vector<TMapKeySector> KeySectors;
    int MbStatus = 0;
    int32_t ActiveTime = 0;
    int32_t TriggerTime = 0;
    uint32_t ActivateOwner = 0;
    int32_t CountDown = 0;
    int32_t LastTenSec = 0;
    int32_t LastSec = 0;
    int32_t CurrentRadius = 0; // Current radius of the propagation wave
    int32_t LastRadius = 0;
};

struct TLevelInfo
{
    enum STATE
    {
        STATE_PLAYING    = 0,
        STATE_COMPLETED  = 1,
        STATE_ABORTED    = 2,
        STATE_PAUSED     = 3,
        STATE_RESTART    = 4,
        STATE_BRIEFING   = 5,
        STATE_SAVE       = 6,
        STATE_LOAD       = 7,
        STATE_MENU       = 8,
        STATE_DEBRIEFING = 9,
        STATE_REWIND     = 10,
    };
    
    std::string MapName;
    int State = STATE_PLAYING;
    int LevelID = 0;
    int GateCompleteID = 0; // Index of gate through level complete
    int MusicTrack = 0;
    int MusicTrackMinDelay = 0;
    int MusicTrackMaxDelay = 0;
    int OwnerMask = 0;
    int UserMask = 0;

    std::vector<TMapBuddy> Buddies;
    std::vector<TMapGate> Gates;
    std::vector<TMapSuperItem> SuperItems;
    std::array<int, 8> JodieFoster = Common::ArrayInit<int, 8>(0);
    std::string MovieStr;
    std::string MovieWinStr;
    std::string MovieLoseStr;
};


struct TBriefObject
{
    enum
    {
        TYPE_NONE    = 0,
        TYPE_SECTOR  = 1,
        TYPE_VEHICLE = 2
    };
    
    vec2d Pos;
    int16_t ObjType = TYPE_NONE;
    int16_t ID = 0;
    int TileSet = 0;
    int TileID = 0;
    int Color = 0;
    std::string Title;
    
    NC_STACK_base::Instance *VP = NULL; // Must not be copied

    TBriefObject() = default;
    
    TBriefObject(int16_t tp, int16_t oid, float sx, float sy, int tset, int tid, int clr, const std::string &ttl)
    {
    	Pos.x = sx;
        Pos.y = sy;
        ObjType = tp;
        ID = oid;
        TileSet = tset;
        TileID = tid;
        Color = clr;
        Title = ttl;
    }
    
    TBriefObject(const TBriefObject &b)
    {
    	Pos = b.Pos;
        ObjType = b.ObjType;
        ID = b.ID;
        TileSet = b.TileSet;
        TileID = b.TileID;
        Color = b.Color;
        Title = b.Title;
    }
    
    TBriefObject(TBriefObject &&b) = default; //Pointer also can be moved
    
    ~TBriefObject()
    {
        Common::DeleteAndNull(&VP);
    }
    
    TBriefObject &operator=(const TBriefObject &b)
    {
        Pos = b.Pos;
        ObjType = b.ObjType;
        ID = b.ID;
        TileSet = b.TileSet;
        TileID = b.TileID;
        Color = b.Color;
        Title = b.Title;
        Common::DeleteAndNull(&VP);
        return *this;
    }

    bool operator==(const TBriefObject &b) const
    {
        return ObjType == b.ObjType && ID == b.ID && Pos == b.Pos;
    }
    
    operator bool() const
    {
        return ObjType != TYPE_NONE;
    }
};

struct TBriefengScreen
{
    enum
    {
        STAGE_NONE        = 0,
        STAGE_PLAYLEVEL   = 1,
        STAGE_CANCEL      = 2,
        
        STAGE_LOADED      = 4,
        STAGE_SCALING     = 5,
        STAGE_SCALEEND    = 6,
        
        STAGE_PLAYER_ST   = 7,
        STAGE_PLAYER_RN   = 8,
        STAGE_PLAYER_END  = 9,
        
        STAGE_KEYS_ST     = 10,
        STAGE_KEYS_RN     = 11,
        STAGE_KEYS_END    = 12,
        
        STAGE_TECH_ST     = 13,
        STAGE_TECH_RN     = 14,
        STAGE_TECH_END    = 15,
        
        STAGE_ENMHS_ST    = 16,
        STAGE_ENMHS_RN    = 17,
        STAGE_ENMHS_END   = 18,
        
        STAGE_ENMFRC_ST   = 19,
        STAGE_ENMFRC_RN   = 20,
        STAGE_ENMFRC_END  = 21,
        
        STAGE_BUDDY_ST    = 22,
        STAGE_BUDDY_RN    = 23,
        STAGE_BUDDY_END   = 24,
        
        STAGE_GATE_ST     = 25,
        STAGE_GATE_RN     = 26,
        STAGE_GATE_END    = 27,
        
        STAGE_MOVIE       = 28,
        
        
        // TimerStatus
        TIMER_NORMAL      = 0,
        TIMER_STOP        = 1,
        TIMER_FAST        = 2,
        TIMER_RESTART     = 3,
    };
    
    NC_STACK_bitmap *MbmapImg = NULL;
    NC_STACK_bitmap *BriefingMapImg = NULL;
    TLevelDescription Desc;
    int Stage = STAGE_NONE;
    int TimerStatus = TIMER_NORMAL;
    int32_t ActiveElementID = 0; // In current heap
    int32_t ElementsCount = 0;
    int32_t StartTime = 0;
    int32_t CurrTime = 0;
    int32_t TextTime = 0;
    int32_t PreTextTime = 0;
    std::string ObjDescription;
    std::string BriefingText;
    
    GFX::TBitmapBlitParams MapBlitParams;
    Common::FRect MapBlitStart;
    Common::FRect MapBlitEnd;

    int32_t SelectedObjID = 0;
    
    bool AddObjectsFlag = false;
    TBriefObject ViewingObject;
    Common::FRect ViewingObjectRect;
    int32_t ViewingObjectAngle = 0;
    uint32_t ViewingObjectStartTime = 0;
    
    std::vector<TBriefObject> Objects;
    
    baseRender_msg ObjRenderParams;

    bool ZoomFromGate = false;

    // The debriefing comes back as it ends, as from watching the replay
    bool Instant = false;

    Common::Point MapSize; // Of the level's map the debriefing shows
    
    std::array<NC_STACK_sklt *, 4> VectorGfx = Common::ArrayInit<NC_STACK_sklt *, 4>(NULL);
    Common::PlaneBytes OwnMap;
    Common::PlaneBytes TypMap;
    //int _owner;
    uint32_t ReplayedTime = 0;   // The level's game time the replay of its history is at
    size_t EventsReplayed = 0;  // Of the replay's events, those counted and heard
    std::array<World::TPlayerStatus, World::CVFractionsCount> StatsGlobal;
    std::array<World::TPlayerStatus, World::CVFractionsCount> StatsIngame;
    std::string MovieStr;
    std::vector<World::History::Event> Upgrades;
    
    ~TBriefengScreen()
    {
        UnloadRes();
    }
    
    void UnloadRes()
    {
        if ( MbmapImg )
        {
            MbmapImg->Delete();
            MbmapImg = NULL;
        }

        if ( BriefingMapImg )
        {
            BriefingMapImg->Delete();
            BriefingMapImg = NULL;
        }
        
        for ( NC_STACK_sklt* &gfx : VectorGfx )
        {
            if (gfx)
            {
                gfx->Delete();
                gfx = NULL;
            }
        }
        
        OwnMap.Clear();

        TypMap.Clear();
    }

    void Clear()
    {
        UnloadRes();

        Desc = TLevelDescription();
        Stage = STAGE_NONE;
        TimerStatus = TIMER_NORMAL;
        ActiveElementID = 0;
        ElementsCount = 0;
        StartTime = 0;
        CurrTime = 0;
        TextTime = 0;
        PreTextTime = 0;
        ObjDescription.clear();
        BriefingText.clear();
        
        ViewingObject = TBriefObject();
        
        ViewingObjectRect = Common::FRect();
        ViewingObjectAngle = 0;
        ViewingObjectStartTime = 0;
        
        MapBlitParams = GFX::TBitmapBlitParams();
        MapBlitStart = Common::FRect();
        MapBlitEnd = Common::FRect();
        AddObjectsFlag = false;
        SelectedObjID = 0;

        Objects.clear();

        ObjRenderParams = baseRender_msg();
        ZoomFromGate = false;
        Instant = false;
        MapSize = Common::Point();

        //_owner = 0;
        ReplayedTime = 0;
        EventsReplayed = 0;

        StatsGlobal.fill( World::TPlayerStatus() );
        StatsIngame.fill( World::TPlayerStatus() );

        MovieStr.clear();
        Upgrades.clear();
    }
};

struct TMapRegionInfo
{
    enum
    {
        STATUS_NONE      = 0,
        STATUS_DISABLED  = 1,
        STATUS_ENABLED   = 2,
        STATUS_COMPLETED = 3,
        STATUS_NETWORK   = 4
    };
    int Status = 0;
    std::string MapDirectory;
    std::string MapName;
    Common::FRect Rect;
    uint32_t RoboCount = 0;
    uint32_t FractionsBits = 0;
    Common::Point MapSize;
    bool SlowConnection = false;
    
    inline bool IsFraction(int32_t id) const { return (FractionsBits & (1 << id)) != 0;}
};

struct TMapRegionsNet
{
    enum
    {
        MAX_NUM = 4
    };

    int NumSets = 0;
    std::array<TBkgPicInfo, MAX_NUM> background_map;
    std::array<TBkgPicInfo, MAX_NUM> rollover_map;
    std::array<TBkgPicInfo, MAX_NUM> finished_map;
    std::array<TBkgPicInfo, MAX_NUM> enabled_map;
    std::array<TBkgPicInfo, MAX_NUM> mask_map;
    std::array<TBkgPicInfo, MAX_NUM> tut_background_map;
    std::array<TBkgPicInfo, MAX_NUM> tut_rollover_map;
    std::array<TBkgPicInfo, MAX_NUM> tut_mask_map;
    std::array<TBkgPicInfo, MAX_NUM> menu_map;
    std::array<TBkgPicInfo, MAX_NUM> input_map;
    std::array<TBkgPicInfo, MAX_NUM> settings_map;
    std::array<TBkgPicInfo, MAX_NUM> network_map;
    std::array<TBkgPicInfo, MAX_NUM> locale_map;
    std::array<TBkgPicInfo, MAX_NUM> save_map;
    std::array<TBkgPicInfo, MAX_NUM> about_map;
    std::array<TBkgPicInfo, MAX_NUM> help_map;
    std::array<TBkgPicInfo, MAX_NUM> brief_map;
    std::array<TBkgPicInfo, MAX_NUM> debrief_map;
    
    std::array<TMapRegionInfo, 256> MapRegions;
    NC_STACK_bitmap *MenuImage = NULL;
    NC_STACK_bitmap *MaskImage = NULL;
    NC_STACK_bitmap *RolloverImage = NULL;
    NC_STACK_bitmap *FinishedImage = NULL;
    NC_STACK_bitmap *EnabledImage = NULL;
    
    size_t SelectedRegion = 0;
    
    ~TMapRegionsNet()
    {
        UnloadImages();
    }
    
    void UnloadImages();
};


struct THudState
{
    int visible = 0;
    NC_STACK_sklt *sklts[14] = {NULL};
    UAskeleton::Data *sklts_intern[14] = {NULL};
    int color_fx = 0;
    int switch_time = 0;
    int target_time = 0;
    uint32_t target = 0;          // What the sight last locked on to, by id
    int pulse_period = 0;
    float pulse_phase = 0.0;
    float bar_width = 0.0;
    float info_height = 0.0;
    float line_height = 0.0;
    int label_width = 0;
    int value_width = 0;
    int bar_segments = 0;
    float cl1_r = 0.0;
    float cl1_g = 0.0;
    float cl1_b = 0.0;
    float cl2_r = 0.0;
    float cl2_g = 0.0;
    float cl2_b = 0.0;
};


// What the HUD shows of the unit looked from: the player's, as the game
// has it (HudSubjectOf), or one of a replay's frame
struct THudSubject
{
    int Vehicle = 0;
    vec3d Position;
    mat3x3 Rotation = mat3x3::Ident(); // Of the view: the compass turns with it
    int Energy = 0;
    int EnergyMax = 1;
    int Shield = 0;
    int ReloadPercent = 100;           // Of its weapon
    bool HasParent = false;            // Its host station, or its commander
    vec3d ParentPos;
    bool HasTarget = false;            // Its squad's target, when it leads one
    vec3d TargetPos;
    uint8_t SectorOwner = 0;           // Of the sector it's in
    THudVisor Visor;                   // Its sights
    uint32_t VisorTarget = 0;          // What they lock on to, by id; 0 for none
    vec3d VisorTargetPos;
};

struct TMFWinStatus
{
    bool Valid = false;
    bool IsOpen = false;
    Common::PointRect Rect;
    std::array<int32_t, 8> Data = Common::ArrayInit<int32_t, 8>(0);
};



struct TSingleVoiceMessage
{
    int Priority = -1;
    TSndCarrier Carrier;
    NC_STACK_sample *Sample = NULL;
    NC_STACK_ypabact *Unit = NULL;
    
    TSingleVoiceMessage()
    {
        Carrier.Resize(1);
    }
    
    ~TSingleVoiceMessage()
    {
        SFXEngine::SFXe.StopCarrier(&Carrier);
        
        if (Sample)
            Sample->Delete();
        
    }
    
    void Reset()
    {
        SFXEngine::SFXe.StopCarrier(&Carrier);
        
        if (Sample)
            Sample->Delete();
        
        Carrier.Resize(1);
        Priority = -1;
        Sample = NULL;
        Unit = NULL;
    }
};

struct TPowerStationInfo
{
    Common::Point CellId; 
    cellArea *pCell = NULL;
    int32_t Power = 0;
    int32_t EffectivePower = 0;
    
    operator Common::Point() { return CellId; }
};

struct EnergyAccum
{
    uint8_t Owner = 0;
    int32_t Energy = 0;
};

struct TLego
{
    NC_STACK_base *Base = NULL;
    NC_STACK_skeleton *CollisionSkelet = NULL;
    NC_STACK_skeleton *UseCollisionSkelet = NULL;
    uint8_t Shield = 0; // Percents
    uint8_t GUIElementID = 0;
    
    /* Explode object and position */
    struct ExFX
    {
        uint8_t Index = 0;
        uint8_t ObjectID = 0;
        vec3d Position;
    };

    std::vector<ExFX> Explosions;
};

struct TSubSectorDesc
{
    int32_t StartHealth = 0;
    std::array<uint8_t, 4> HPModels = Common::ArrayInit<uint8_t, 4>(0); //Building health models 0 - 100%hp, 3 - 0%hp
};

struct TSectorDesc
{
    uint8_t SectorType = 0;
    uint8_t SurfaceType = 0;
    uint8_t GUIElementID = 0;
    Common::PlaneArray<TSubSectorDesc *, 3, 3> SubSectors = Common::PlaneArray<TSubSectorDesc *, 3, 3>::ArrayInit(0);
};






struct TLevelLoadRequest
{
    int lvlID;
};

struct TUnitSpawn
{
    size_t vehicle_id;
    vec3d pos;
};

struct TSectorInfo
{
    float pos_x;
    float pos_z;
    Common::Point CellId;
    cellArea *pcell;
};

struct TRayIntersection
{
    vec3d stPos;
    vec3d vect;
    bool isect;
    float tVal;
    vec3d isectPos;
    int polyID;
    UAskeleton::Data *skel;
    int flags;

    TRayIntersection()
    {
        isect = false;
        tVal = 0.0;
        polyID = 0;
        skel = NULL;
        flags = 0;
    }
};

struct TSphereContact
{
    vec3d pos1;
    vec3d normal;
};

struct TSphereCollisionQuery
{
    vec3d pos;
    vec3d dir;
    float radius;
    TSphereContact *collisions;
    int coll_max;
    int coll_count;
    int flags;
};

struct TFFeedbackEffect
{
    int effects_type;
    float strength;
    float pos_x;
    float pos_z;
};

struct TBuildingPlacement
{
    int owner;
    int blg_ID;
    int instant;
    Common::Point CellId;
    int x;
    int y;
    int flags;
};

struct TSectorDamage
{
    vec3d pos;
    int energy;
    int OwnerID;
    NC_STACK_ypabact *unit;
};

struct TRoboMessage;

struct TInfoMessage
{
    NC_STACK_ypabact *unit;
    int Priority;
    std::string txt;
    int MsgID;
};

struct TReloadRatios
{
    int owner;
    float ratioClamped;
    float ratioPositive;
};

struct TRoboDestroyed
{
    NC_STACK_ypabact *bact;
    int killerOwner;
};

struct TRenderingSector;

// The last of a message a host station reported
struct TLastRoboMessage
{
    int param1 = 0;
    int param2 = 0;
    int param3 = 0;
    int lastTime = 0;
};

// How far the world is drawn around the viewer. Any of the limits can be
// lifted: SectorRadius with ALL_SECTORS, the distances with UNLIMITED.
struct TViewDistance
{
    static constexpr int32_t ALL_SECTORS = INT32_MAX;
    static constexpr float UNLIMITED = std::numeric_limits<float>::infinity();

    int32_t SectorRadius = 4;    // Sectors drawn around the viewer's, in steps along the grid
    float ClipDistance = 3500.0; // Objects wholly further ahead than this are not drawn
    float FogStart = 1200.0;     // Objects fade to black between the fog distances
    float FogEnd = 2100.0;       // UNLIMITED: no fog

    // Objects fade out between these distances over the ground, showing the
    // sky behind them, so that the end of the view doesn't cut them off.
    float FadeOutStart = 3000.0;
    float FadeOutEnd = 3500.0;   // UNLIMITED: no fading
};

class NC_STACK_ypaworld: public NC_STACK_nucleus //NC_STACK_base
{
friend class UserData;
friend class World::Parsers::UserParser;
friend class World::Parsers::VhclProtoParser;
friend class World::Parsers::WeaponProtoParser;
friend class World::Parsers::BuildProtoParser;
friend class World::Parsers::LevelStatusParser;

public:
    enum
    {
        NUM_BUILD_PROTO = 128,
        NUM_VHCL_PROTO  = 256,
        NUM_WEAPON_PROTO = 128,
        NUM_ROBO_PROTO = 16,   //reserve
        
    };

public:
    struct TConstructInfo
    {
        Common::Point CellID;
        int Time = 0;
        int EndTime = 0;
        int Owner = 0;
        uint8_t BuildID = 0;
    };
    
    struct TNetGameEvent
    {
        int8_t EventType = 0;
        uint32_t TimeStamp = 0;
        std::string PlayerName;
        std::string Player2Name;
    };

public:
    virtual size_t Init(IDVList &stak);
    virtual size_t Deinit();
    bool PausedFrame(TUpdateArgs *arg);
    bool SimulationStep(TUpdateArgs *arg);
    void RenderFrame(TUpdateArgs *arg, float stepFraction);

    // Whether a step has left a frame to draw since the level was set up
    bool ReadyToDraw() const { return !_frames.Empty(); }

    // The world is drawn from this camera instead of the player's, as the
    // benchmark does: where it is and how it's turned
    void OverrideCamera(const vec3d &pos, const mat3x3 &rotation);
    virtual void DamageSectorBuilding(TSectorDamage *arg);
    virtual size_t GetSectorInfo(TSectorInfo *arg);
    // A player plays a faction now: its host stations follow its players'
    // orders instead of its AI. The player's orders go to its first host
    // station, if it has one.
    void JoinFaction(World::Player *player, uint8_t owner);

    // A player stops playing their faction, and lets go of what they
    // controlled. When no one plays it any more, its AI does.
    void LeaveFaction(World::Player *player);

    bool FactionHasPlayers(uint8_t owner) const;

    // Someone at another machine plays a faction from now on: a new player
    // of it, named so, in its first host station (riding along when someone
    // else controls it) or, with none, its first squad. NULL when the
    // faction has no units to start in.
    World::Player *AddFactionPlayer(const std::string &name, uint8_t owner);

    // A player who isn't at this machine is gone; their faction's AI plays
    // it again when no one else does
    void RemovePlayer(World::Player *player);

    // The commander of a faction's first squad still there, guns' aside;
    // NULL for none
    NC_STACK_ypabact *FirstSquad(uint8_t owner);

    // A host station is destroyed and another of its faction goes on: the
    // players whose orders went to it go to that one, and those who were
    // in it get in that one
    void HostStationTakesOver(NC_STACK_ypabact *lost, NC_STACK_ypabact *heir);

    // A faction has any number of host stations, none included. Its squads
    // and guns are under them; a faction with none has them at the top
    // level, where its host stations would be.

    // The first of a faction's host stations still going; NULL when it has
    // none
    NC_STACK_yparobo *FirstHostStation(uint8_t owner);

    // A new squad of a faction, under one of its host stations or, with
    // none, at the top level. Its commander is the first vehicle.
    bool MakeSquad(uint8_t owner, NC_STACK_yparobo *host, const std::vector<int> &vehicles, vec3d pos, bool usable);

    // A unit leads a squad of its own now: under its host station, or at
    // the top level when it has none
    void AddCommander(NC_STACK_ypabact *unit);

    // The factions with units left, host stations included, as bits (1 <<
    // owner). A faction is defeated when it has none left: one with host
    // stations when the last of them falls and takes the rest along, one
    // without when its last unit is destroyed.
    uint32_t FactionsWithUnits();

    // The local player's faction has no units left; false when they play
    // none
    bool LocalPlayerDefeated();

    // Each commander of a faction's squads, dead or not, building guns' and
    // host stations' own included: those under its host stations, and
    // those at the top level of a faction without one
    template <typename Func>
    void ForEachCommander(int owner, Func func)
    {
        for ( NC_STACK_ypabact *unit : _unitsList )
        {
            if ( unit->_owner != owner )
                continue;

            if ( unit->_bact_type == BACT_TYPES_ROBO )
            {
                for ( NC_STACK_ypabact *commander : unit->_kidList )
                    func(commander);
            }
            else if ( unit->IsCommander() )
            {
                func(unit);
            }
        }
    }

    // A player's order to one of their faction's squads (move, attack,
    // waypoints), which needs no host station
    void OrderSquad(uint8_t owner, TUserOrder *order);

    // A unit by its id: a host station, a squad's or one of its units, a
    // gun; NULL when there's none
    NC_STACK_ypabact *FindUnit(uint32_t id);

    // What the game's interface reads of the level: the world itself, or
    // the frames of a game played elsewhere
    const MapScene &UiScene();
    const MapScene *_uiScene = NULL; // NULL for the world
    const MapScene &WorldScene();

    // The local player's order from the interface: carried out now, or sent
    // where the game is played (_orderSink)
    void IssueOrder(const World::PlayerOrder &order);
    std::function<void(const World::PlayerOrder &)> _orderSink;

    // Carries out a player's order, if what it names is there and theirs to
    // order: one to their host station or a squad goes there in this step's
    // orders (Player::Order), the rest is done now. False when refused.
    bool ApplyPlayerOrder(World::Player *player, const World::PlayerOrder &order);

    // What a faction pays for a unit or a building of the prototype (bought
    // with DOACTION_ADD_UNIT1/2 or DOACTION_6); with more units than the
    // network game's limit, more
    int OrderCost(uint8_t owner, int action, int proto);
    float UnitLimitCostFactor(uint8_t owner);

    // What beaming a host station costs, from where it is to a point of a
    // sector
    int BeamCost(const NC_STACK_ypabact *host, const cellArea &sector, const vec3d &to);

    // A squad, by its commander: too weak to fight on, it retreats to its
    // faction's nearest power station or host station it can get to, or
    // with none, fights on; recovered, it stops retreating, where it is if
    // that's in its faction's power. Says whether it fights or retreats.
    enum { SQUAD_FIGHTING, SQUAD_RETREATING };
    int CheckSquadRetreat(NC_STACK_ypabact *commander);

    // Where a squad retreats to: its faction's nearest power station (its
    // sector's middle) or host station (then also host) that it can get to,
    // of the few nearest. False when there's none.
    bool FindRetreat(NC_STACK_ypabact *commander, vec3d *pos, NC_STACK_ypabact **host);

    // What a faction's units say to the player here (TRoboMessage ids), into
    // the message log and out loud; some are left out when said just before.
    // Returns whether it was said.
    int PlaceMessage(TRoboMessage *msg);

    // A player looks from a unit now: their camera and HUD go there
    void SetPlayerUnit(World::Player *player, NC_STACK_ypabact *unit);

    // A player's orders go to a host station now, of the faction they play
    // from then on
    void SetPlayerHostStation(World::Player *player, NC_STACK_ypabact *host);

    // The player whose controls move a unit; NULL when none's do
    World::Player *ControllerOf(const NC_STACK_ypabact *unit) const;

    // A player looking from a unit, other than one; NULL when there's none
    World::Player *PlayerLookingFrom(const NC_STACK_ypabact *unit, const World::Player *other = NULL) const;
    virtual void AddToUnitsList(NC_STACK_ypabact *bact);
    virtual void IntersectRay(TRayIntersection *arg);
    virtual void FindSphereCollisions(TSphereCollisionQuery *arg);
    virtual void GuiWinAttach(GuiBase *lstvw);
    virtual void GuiWinDetach(GuiBase *lstvw);
    virtual void MoveToDeadCache(NC_STACK_ypabact *bacto);
    virtual size_t IsNearViewer(NC_STACK_ypabact *bact);
    virtual NC_STACK_ypabact * CreateVehicle(TUnitSpawn *vhcl_id);
    virtual NC_STACK_ypamissile * CreateWeapon(TUnitSpawn *arg);
    virtual size_t CreateBuilding(TBuildingPlacement *arg);
    virtual void IntersectLongRay(TRayIntersection *arg);
    virtual void DeleteLevel();
    void FreeLevel();
    void FreeUnits();
    bool FreeSomeUnits(size_t count);
    void ResetControls();
    virtual void SetHudVisor(THudVisor *arg);
    virtual bool InitGameShell(UserData *usr);
    virtual void DeinitGameShell();
    virtual bool CreateTitleControls();
    virtual bool CreateSubBarControls();
    virtual bool CreateConfirmControls();
    virtual bool CreateInputControls();
    virtual bool CreateVideoControls();
    virtual bool CreateDiskControls();
    virtual bool CreateLocaleControls();
    virtual bool CreateAboutControls();
    virtual bool CreateNetworkControls();
    virtual bool OpenGameShell();
    virtual void CloseGameShell();
    virtual void ProcessGameShell();
    virtual void ShowMessage(TInfoMessage *arg);
    virtual size_t CreateLevel(TLevelLoadRequest *arg);
    virtual size_t LoadLanguage(const std::string &langname);
    virtual void UpdateGameShell();
    virtual size_t BeamUnitThroughGate(NC_STACK_ypabact *pbact);
    virtual size_t LoadGame(const std::string &saveFile);
    virtual size_t SaveGame(const std::string &saveFile);

    // The level being played in the campaign is recorded there; Restart
    // starts it again from there; a won level's end is kept to play it again
    std::string SessionPath() const;
    bool RestartLevel();
    bool RewindLevel();
    void PlayRewind(double realSteps);
    bool EndRewind();
    bool IsRewinding() const { return _rewinding; }
    void DrawRewindNotice();
    void SaveWonState();

    // The replay viewer: a recording played back in its level, which is set
    // up from the recording's first keyframe, drawn from its frames and never
    // stepped (yw_replay.cpp)
    bool StartReplay(const std::string &path, uint32_t startStep = 0);

    // Or a game as it's played elsewhere, live (Net::ViewClient), shown a
    // little behind its latest step
    bool StartWatching(std::unique_ptr<World::FrameSource> source, int level, const std::string &levelDescription);
    void ReplayFrame(TUpdateArgs *arg);
    bool LeavingReplay() const { return _replayLeave; }

    // Playing the game watched live with the game's interface, which then
    // takes the input a step at a time (PlayStep), as in a level
    bool PlayingLive() const { return _replay && _replayPlaying; }
    void PlayStep(TInputState *inpt);
    void EndReplay();
    bool StartViewer(std::unique_ptr<World::FrameSource> source, int level, const std::string &levelDescription,
                     uint32_t startStep);

    // The world's own state, field by field: see World::State
    void VisitState(World::State::Visitor &v);

    // For World::State::Restore: the level's fixed data (ground, prototypes,
    // the definitions of gates and bombs) without its units; a unit of a
    // class, made as yw_createUnit does, or not counted (MakeUnitObject);
    // a unit's models, sounds and effects from its prototype; and what is
    // worked out from the state once it is read. Of the level a description
    // is of, as ReadLevelDescription and RecordedLevel give them.
    bool LoadLevelData(int levelId, const std::string &description);
    NC_STACK_ypabact *NewUnitObject(int type);
    NC_STACK_ypabact *MakeUnitObject(int type);
    void GiveUnitAssets(NC_STACK_ypabact *unit);
    void FinishRestore(bool inPlace);
    void ClearForRestore();
    virtual bool SaveSettings(UserData *usr, const std::string &fileName, uint32_t sdfMask);
    virtual size_t LoadSettings(const std::string &fileName, const std::string &userName, uint32_t sdfMask, bool updateGameShell, bool playIntro = false);
    virtual bool ReloadInput(size_t id);
    void SetWindowed(bool windowed);

    // Follows the window size: call once per frame, outside BeginFrame/EndFrame.
    void UpdateScreenSize();
    void LoadScreenFonts();
    bool CreateGameShellControls();
    void DeleteGameShellControls();
    void ShowGameShellPage();
    void RelayoutGameShell();
    void RelayoutGameGui();
    void RelayoutReplay();
    virtual size_t ReloadLanguage();
    virtual void GetReloadRatios(TReloadRatios *arg);

    // What a faction gets of the power under its host stations, unclamped:
    // its sectors against the power of its power stations
    static float ReloadRatio(int sectors, int stationPower);
    virtual void HandleRoboDestroyed(TRoboDestroyed *arg);
    virtual size_t CreateNetLevel(TLevelLoadRequest *arg);
    virtual void FFeedback_Play(TFFeedbackEffect *arg);
    virtual bool NetSendMessage(uamessage_base *data, size_t dataSize, const std::string &recvID, bool garantee);
    virtual bool NetBroadcastMessage(uamessage_base *data, size_t dataSize, bool garantee);

    virtual size_t AdvancedCreateLevel(TLevelLoadRequest *arg);
    void ScoreEvent(const World::History::Event &event);
    virtual void OpenHelpURL(const void *arg);

    NC_STACK_ypaworld();
    virtual ~NC_STACK_ypaworld() {};
    
    virtual const std::string ClassName() const {
        return __ClassName;
    };


    enum YW_ATT
    {
        YW_ATT_MAPMAX_X = 0x80002000,
        YW_ATT_MAPMAX_Y = 0x80002001,
        YW_ATT_MAPSIZE_X = 0x80002002,
        YW_ATT_MAPSIZE_Y = 0x80002003,
//        YW_ATT_SECTORSIZE_X = 0x80002004,
//        YW_ATT_SECTORSIZE_Y = 0x80002005,
        YW_ATT_SKYVISLIMIT = 0x80002009,
        YW_ATT_SKYFADELENGTH = 0x8000200A,
        YW_ATT_SKYHEIGHT = 0x8000200B,
        YW_ATT_SKYRENDER = 0x8000200C,
        YW_ATT_DOENERGYRECALC = 0x8000200D,
        YW_ATT_USERHOST = 0x80002010,
        YW_ATT_USERVEHICLE = 0x80002011,
        YW_ATT_WPNPROTOS = 0x80002012,
        YW_ATT_BUILDPROTOS = 0x80002013,
        YW_ATT_VHCLPROTOS = 0x80002014,
        YW_ATT_LVLFINISHED = 0x80002015,
        YW_ATT_SCREEN_W = 0x80002016,
        YW_ATT_SCREEN_H = 0x80002017,
        YW_ATT_LOCALE_STRINGS = 0x80002018,
        YW_ATT_LVL_INFO = 0x8000201A,
        YW_ATT_DESTROY_FX = 0x8000201B,
        YW_ATT_PNET = 0x8000201C,
        YW_ATT_BUILD_DATE = 0x8000201D,     //std::string
        YW_ATT_DONT_RENDER = 0x8000201E,
        YW_ATT_INVULNERABLE = 0x8000201F
    };

    virtual void setYW_skyVisLimit(int);
    virtual void setYW_skyFadeLength(int);
    virtual void setYW_skyHeight(int);
    virtual void setYW_skyRender(int);
    virtual void setYW_doEnergyRecalc(int);
    virtual void setYW_screenW(int);
    virtual void setYW_screenH(int);
    virtual void setYW_dontRender(bool);

    virtual int getYW_mapSizeX();
    virtual int getYW_mapSizeY();
    
    Common::Point GetMapSize();
    
    virtual int getYW_skyHeight();
    virtual int getYW_skyRender();
    virtual int getYW_doEnergyRecalc();
    World::Player *LocalPlayer() const { return _localPlayer; }

    // A new player, playing nothing yet
    World::Player *AddPlayer();
    World::Player *FindPlayer(uint32_t id) const;

    // Restoring: the players a state has, by id, made anew with nothing
    // set; then which of them is at this machine (none: a new one, who
    // plays nothing)
    void MakePlayers(const std::vector<uint32_t> &ids);
    void SetLocalPlayer(uint32_t id);

    // The sector the local player sees from: their unit's, or the camera's
    // when they look from none
    Common::Point ViewerCell() const;

    // The faction the local player plays; -1, no faction's, when none
    int LocalOwner() const { return _localPlayer->Owner ? _localPlayer->Owner : -1; }

    // The local player's host station, and the unit they look from
    NC_STACK_ypabact *UserRobo() const { return _localPlayer->HostStation; }
    NC_STACK_ypabact *UserUnit() const { return _localPlayer->Unit; }
    virtual std::vector<World::TWeapProto> &GetWeaponsProtos() { return _weaponProtos; };
    virtual std::vector<World::TBuildingProto> &GetBuildProtos() { return _buildProtos; };
    virtual std::vector<World::TVhclProto> &GetVhclProtos() { return _vhclProtos; };
    virtual std::vector<World::TRoboProto> &GetRoboProtos() { return _roboProtos; };
    virtual int getYW_lvlFinished();
    virtual int getYW_screenW();
    virtual int getYW_screenH();
    virtual TLevelInfo &GetLevelInfo();
    virtual int getYW_destroyFX();
    virtual NC_STACK_windp *getYW_pNET();
    virtual int getYW_invulnerable();

protected:
    int LevelCommonLoader(TLevelDescription *mapp, int levelID, const std::string &description);
    void FFeedback_Init();
    void FFeedback_StopAll();
    void FFeedback_VehicleChanged();
    void FFeedback_Update();

    void GUI_Close();



    void GameShellUiOpenNetwork(); // On main menu "Multiplayer" press
    void GameShellBkgProcess();
    void GameShellBlitBkg(NC_STACK_bitmap *bitm);
    void GameShellInitBkgMode(int mode);
    bool GameShellInitBkg();

    int  InputConfigLoadDefault();

    static uint32_t PointToUint32(const Common::Point &point) 
    { return (0xFFFF & point.y) << 16 | (0xFFFF & point.x); }

public:
    void GuiWinToFront(GuiBase *);
    void GuiWinOpen(GuiBase *);
    void GuiWinClose(GuiBase *);
    void HandleGuiWindows(TInputState *inpt);
    void DrawGuiWindows();


    SDL_Color GetColor(int color_id);
    bool ParseColorString(int color_id, const std::string &color_string);



    // The view distance is the default until it is set here, and again
    // after ResetViewDistance.
    TViewDistance GetViewDistance() const;
    TViewDistance GetDefaultViewDistance() const; // The original's far view
    void SetViewDistance(const TViewDistance &view);
    void ResetViewDistance();
    bool IsViewDistanceSet() const { return _viewDistanceSet; }
    int32_t GetMapSectorRadius() const; // Enough sectors to draw the whole map from anywhere on it
    float GetMapDiagonal() const;       // Nothing on the map is further away

    // Enemy host stations wait after each decision before making another
    // of its kind, the longer the less effort they put in it: up to the
    // timeline, in milliseconds. Host stations take it when they are made.
    void SetAITimeline(int32_t timeline) { _aiTimeline = timeline; }
    int32_t GetAITimeline() const { return _aiTimeline; }

    void SetMoviesEnabled(bool enabled) { _moviesEnabled = enabled; }
    bool IsMoviesEnabled() const { return _moviesEnabled; }
    
    int TestVehicle(int protoID, int job);


//protected:
    void PlayGameMovie(const std::string &movie_fname);
    bool LoadProtosScript(const std::string &filename);
    bool ParseLevelHeader(TLevelDescription *mapp, const std::string &fname);
    void UpdateCampaignScreen();
    bool InitDebrief();
    bool LoadDebriefMap(TBriefengScreen *brf, int levelId, const std::string &description);
    void KeepDebriefing();
    bool ResumeDebriefing();
    bool CreateReplaysControls();
    void DeleteReplaysControls();
    void FreeDebrief();
    int ParseDebriefLevelDesc(TLevelDescription *mapproto, int levelId, const std::string &description);
    int ParseLevelDescription(TLevelDescription *mapp, int levelId, const std::string &description);
    std::string ReadLevelDescription(int levelId) const;
    std::string RecordedLevel(const World::Recording &recording) const;
    bool InitBriefing(int lvlid);
    void FreeBriefing();
    void FreeBriefDataSet();
    int CreateBriefingTextList();
    void RelayoutBriefingTextList();
    void yw_ActivateWunderstein(cellArea *cell, int gemid);
    void yw_InitTechUpgradeBuildings();
    
    void DoSectorsEnergyRecalc();
    void RecalcSectorsPowerForPS(const TPowerStationInfo &ps);
    
    void ActivateSuperItem(int id, int resume);
    void StopSuperItem(int id);
    void TriggerSuperItem(int id);
    int ParseSettingsFile(const std::string &fname, uint32_t sdfMask);
    void listSaveDir(const std::string &saveDir);
    bool InitMapRegionsNet();
    int yw_ParseWorldIni(const std::string &filename);
    bool ScanLevelFile(const std::string &fname, bool multiplayer);
    int ScanLevelDir(bool multiplayer);
    int yw_ScanLevels();
    void EnableLevelPasses();
    int load_fonts_and_icons();
    int yw_LoadSet(int setID);
    
    void FreeLegos();
    void FreeFillers();
    
    void DrawMapRegionsTutorial();
    void DrawMapRegions();
    
    bool CanRewind() const;
    void AddFrame();
    uint32_t FramesKeptFrom() const;
    bool MakeFramesBack(double ms, uint32_t wantFrom);
    bool FrameAt(double step, World::Frame &out) const;
    void PlaceDrawnCamera(const World::Frame &frame);
    void RenderGame(TUpdateArgs *arg, const World::Frame &frame, bool withInterface, bool particles);
    void CheckSectorVisibility(Common::Point sect, TRenderingSector *sct, const World::Frame &frame, baseRender_msg *rndrParams);
    void RenderSector(TRenderingSector *sct, const World::Frame &frame, baseRender_msg *rndrParams);
    void RenderUnit(const World::Frame &frame, const World::Frame::Unit &unit, baseRender_msg *arg);
    void yw_renderSky(const vec3d &viewerPos, baseRender_msg *rndr_params);
    void RenderSuperItems(const World::Frame &frame, const Common::Point &viewerSector, baseRender_msg *arg);
    void RenderSuperWave(vec2d pos, vec2d fromPos, const Common::Point &viewerSector, baseRender_msg *arg);
    void RenderFillers(baseRender_msg *arg, int diameter);
    NC_STACK_base *UnitModel(uint8_t type, uint8_t vehicle, uint8_t look) const;
    void FreeDrawings();
    
    void PrepareFiller(cellArea *sct, cellArea *sct2, int32_t type, int32_t type2, float startCornerHeight, float endCornerHeight, bool vertical, TCellFillerCh *out, bool force = false);
    void PrepareAllFillers();
    
    bool IsVisibleMapPos(vec2d pos, const Common::Point &viewerSector);
    
    //PRT - Position Robo Target
    
    void yw_processVhclDataMsgs(uamessage_vhclDataI *msg, NC_STACK_ypabact *host_node);
    NC_STACK_ypabact * yw_getHostByOwner(uint8_t owner);
    void yw_netApplyVhclDataI(NC_STACK_ypabact *bact, uamessage_vhclDataI *dat, int id, uint32_t timestamp);
    void NetRemove(NC_STACK_ypabact *bct);
    void NetReleaseMissiles(NC_STACK_ypabact *bact);
    void DetachFromRoboGunSlot(NC_STACK_ypabact *bct);
    void BriefingSetObject(const TBriefObject &obj, bool doAdd);
    
    
    static TileMap * yw_LoadFont(const std::string &fontname);
    static TileMap * yw_LoadTileSet(const std::string &bitmap, Common::Point chrSz, Common::Point delta, Common::Point cr, Common::Point offset);
    NC_STACK_ypabact * FindBactByCmdOwn(uint32_t commandID, char owner);
    
    
    bool yw_createRobos(const std::vector<MapRobo> &Robos, int playerOwner);
    bool yw_NetSetHostStations(const std::vector<MapRobo> &Robos);
    void yw_InitSquads(const std::vector<MapSquad> &squads);
    
    
    NC_STACK_ypabact *yw_createUnit(int model_id);
    void SetVehicleAssets(NC_STACK_ypabact *unit, World::TVhclProto &vhcl);
    void SetWeaponAssets(NC_STACK_ypamissile *missile, World::TWeapProto &wproto);
    void PlaceBuilding(const Common::Point &cellId, int ownerid2, int blg_id, int skipGuns);
    void SetupPowerStationInfo(cellArea *cell, int power);
    void ResetAccumMap();
    
    void CellSetOwner(cellArea *cell, uint8_t owner);
    
    void DestroyAllGunsInSector(cellArea *cell);
    bool BuildingConstructBegin(cellArea *cell, uint8_t buildingID, int owner, int cTime);
    void BuildingConstructUpdate(int dtime);
    bool IsAnyBuildingProcess(int owner) const;
    
    void debug_info_draw(TInputState *inpt);
    void DrawPerfOverlay(TInputState *inpt);
    void debug_count_units();
    void ProfileCalcValues();
    
    
    void SendCRC(int lvlid);
    
    
    void ScoreKill(NC_STACK_ypabact *bact);
    
    
    int IsCheatKeyHit(TInputState *struc, int kkode);
    
    
    void CountUnitsPerOwner();
    
    void KillUnitsInSuperBombRadius(uint8_t activate, float centerX, float centerZ, float radius);
    void UpdateSuperBombShockwave(int id);
    void UpdateTriggeredSuperItem(int id);
    void UpdateStoppedSuperItem(int id);
    void UpdateActiveSuperItem(int id);
    void UpdateSuperItems();
    void DeactivateSuperItem(int id);
    bool IsSuperItemLostBy(int id, int owner);
    bool IsSuperItemHeldBy(int id, int owner);
    bool OwnerHasHostStation(int owner);
    
    void UpdateCommanderList();

    // The player's unit that reported last, in the last 10 s; false for
    // none
    bool GetLastMsgSender(MapScene::Unit &out);
    
    void HandleGuiInput(TInputState *inpt);
    void ConfirmDialogHandleInput(TInputState *inpt);
    void ConfirmDialogLayoutItems(int pressedFlags);
    void SquadManagerShowStatusTooltip(const MapScene::Unit &unit);
    bool SquadManagerIsDragValid();
    
    void yw_MouseSelect(TInputState *arg);
    void ComputeMouseSelectFlags(TInputState *arg);
    
    void yw_MAP_MouseSelect(TClickBoxInf *winp);
    
    void UpdateMouseVector(TClickBoxInf *winp);
    void yw_3D_MouseSelect(TClickBoxInf *winp);
    
    void HandleWorldMouse(TInputState *arg);
    void UpdateActionEnergyCost();
    float GetUnitLimitCostFactor();
    
    int SquadManagerBeginDrag(TClickBoxInf *winpt);
    void yw_SMAN_MouseSelect(TClickBoxInf *winp);
    void SquadManager_InputHandle(TInputState *inpt);
    bool SquadManagerPickUnit(int sqid, int btnX, MapScene::Unit &out);
    
    
    void SetShowingTooltip(int32_t id)
    {
        if ( id > _toolTipId ) // avoid flickr of tooltips
        {
            _toolTipHotKeyId = -1;
            _toolTipId = id;
        }
    }
    
    void SetShowingTooltipWithHotkey(int32_t id, int32_t hotkey)
    {
        if ( id > _toolTipId ) // avoid flickr of tooltips
        {
            _toolTipHotKeyId = hotkey;
            _toolTipId = id;
        }
    }
    
    // Before the interface handles input, which sets the tooltip to show
    void ClearTooltip()
    {
        _toolTipHotKeyId = -1;
        _toolTipId = 0;
    }

    std::string GetTooltipString() const
    {
        return Locale::Text::Tooltip(_toolTipId);
    }
    
    void PrepareUserInput(TInputState *inpt);
    void TrackFireButton(TInputState *inpt);
    void TakeDialogKeys(TInputState *inpt);
    void ExecuteMouseAction(int arg);
    
    
    bool CheckUnitDoubleClick(TClickBoxInf *winp);
    int HandleWaypointMode(TInputState *arg, int action);
    int UserActBeamCheck();
    int UserActBuildCheck();
    int UserActCreateCheck(TClickBoxInf *winp);

    // Whether a host station can build in a sector for that much: 0 when it
    // can, else why not (as UserActBuildCheck's results). As a scene has
    // it: what the interface shows, or (WorldScene) the world.
    int CheckBuild(const MapScene &scene, const MapScene::Unit &robo, const Common::Point &sector, int cost);

    // Whether a host station can beam to a point on the ground of a sector:
    // 0 when it can, the point then moved to where it would hover, else why
    // not (as UserActBeamCheck's results). Once it's known, what it costs.
    int CheckBeam(const MapScene &scene, const MapScene::Unit &robo, const Common::Point &sector, vec3d *point, int *cost);

    // The unit nearest along a ray, but those two (0 for none), as the
    // mouse picks units; false for none. Its distance less its radius.
    bool UnitOnRay(const MapScene &scene, const vec3d &pos, const vec3d &vect, uint32_t ignore, uint32_t ignoreToo,
                   MapScene::Unit &hit, float &distance);

    // A sector's index, as orders have it; -1 for none
    int SectorIndex(const Common::Point &sector) const { return IsSector(sector) ? sector.y * _mapSize.x + sector.x : -1; }
    void ExitMenuHandleInput(TInputState *inpt);
    
    int yw_MouseFindCreationPoint(TClickBoxInf *winp);
    
    void OpenConfirmDialog(const std::string &text, GuiBase *lstvw);
    void NetSendExitMsg(uint8_t normExit);
    void ConfirmOpenHelp(const std::string &url);
    
    void LoadKeyNames();
    
    bool GetDefaultCmdr(MapScene::Unit &out);
    bool GetNextCmdrByCmdId(uint32_t cmdrId, MapScene::Unit &out);
    bool GetNextSquad(MapScene::Unit &out);
    
    
    void LinkGatesAndSuperItems();
    void InitGates();
    void UpdatePowerEnergy();
    void PowerStationErase(cellArea *cell);
    void CellSetNewOwner(cellArea *cell, NC_STACK_ypabact *attacker, int newOwner);
    void CellCheckHealth(cellArea *cell, int newOwner, NC_STACK_ypabact *attacker);
    void InitBuddies();
    void StartPlayerInSquad(int playerOwner);
    void InitSuperItems();
    bool LoadBlgMap(const std::string &mapName);
    bool LoadHightMap(const std::string &mapName);
    bool LoadOwnerMap(const std::string &mapName);
    bool LoadTypeMap(const std::string &mapName);
    
    void PlayIntroMovie();    
    
    void SituationAnalyzer();
    
    cellArea *GetSector(int32_t x, int32_t y);
    cellArea *GetSector(const Common::Point &sec);
    cellArea *GetSector(size_t id);
    cellArea &SectorAt(int32_t x, int32_t y);
    cellArea &SectorAt(const Common::Point &sec);
    cellArea &SectorAt(size_t id);
    
    Common::PlaneVector<cellArea> &Sectors();
    
    void SetMapSize(const Common::Point &sz);
    bool IsGamePlaySector(const Common::Point &sz) const;
    bool IsSector(const Common::Point &sz) const;
    bool IsSectorBorder(const Common::Point &sz, int border) const;
    int SectorBeamEnergy(const Common::Point &sector);
    TSectorCollision GetSectorCollision(int _dx, int _dz, int _dxx, int _dzz, int flags);
    void FitFillerCollisionHeights(TSectorCollision &arg);
    void IntersectRaySkeleton(TRayIntersection *arg, const TSectorCollision &loc);
    void CollideSphereWithSkeleton(TSphereCollisionQuery *arg, const TSectorCollision &sectorColl);    
    
    void VoiceMessageUpdate();
    void VoiceMessagePlayFile(const std::string &flname, NC_STACK_ypabact *unit, int priority);
    void VoiceMessagePlayMsg(NC_STACK_ypabact *unit, int priority, int msgID);
    void VoiceMessageCalcPositionToUnit();
    
    bool ProtosInit();
    void ProtosFreeSounds();
    
    
    void SetCmdrIdToSelect(int32_t id) { _cmdrIdToSelect = id; };
        
    World::ParticleSystem &ParticleSystem() { return _particles; };
    
    int32_t GetLegoBld(const cellArea *cell, int bldX, int bldY);
    int32_t GetLegoBld(const Common::Point &cell, int bldX, int bldY);
    int32_t DrawnSectorType(const World::Frame::Sector &sector, const World::Frame::Construction *construction) const;
    
    void ClearOverrideModels();
    void LoadOverrideModels();
    
    std::string GetVehicleName(uint32_t id) const
    {
        return Locale::Text::VehicleName(id, _vhclProtos[ id ].name);
    }
    
    std::string GetVehicleName(const World::TVhclProto &proto) const
    {
        if (proto.Index != -1)
            return Locale::Text::VehicleName(proto.Index, proto.name);
        
        return proto.name;
    }
    
    std::string GetBuildingName(uint32_t id, bool net = false) const
    {
        if (net)
            return Locale::Text::NetBuildingName(id, _buildProtos[ id ].Name);
        else
            return Locale::Text::BuildingName(id, _buildProtos[ id ].Name);
    }
    
    std::string GetBuildingName(const World::TBuildingProto &proto, bool net = false) const
    {
        if (proto.Index != -1)
        {
            if (net)
                return Locale::Text::NetBuildingName(proto.Index, proto.Name);
            else
                return Locale::Text::BuildingName(proto.Index, proto.Name);
        }
        
        return proto.Name;
    }
    
    std::string GetLevelName(uint32_t id) const
    {
        return Locale::Text::LevelName(id, _globalMapRegions.MapRegions[ id ].MapName);
    }
    
    std::string GetLevelName(const TLevelInfo &lvl) const
    {
        return Locale::Text::LevelName(lvl.LevelID, lvl.MapName);
    }
    
    bool IsHidden(uint32_t owner) const
    {
        return (_hiddenFractions & (1 << owner)) != 0;
    }
    
    int8_t GetPlayerOwner() const { return _playerOwner; }

public:
    //Data
    static constexpr const char * __ClassName = "ypaworld.class";
  
    
    
    UserData *_GameShell = NULL;
    
    Common::Point _mapSize;

    Common::PlaneVector<cellArea> _cells;
    Common::PlaneVector<TCellFillerCh> _cellsVFCache;
    Common::PlaneVector<TCellFillerCh> _cellsHFCache;

    vec2d _mapLength;

    Common::PlaneVector<EnergyAccum> _energyAccumMap;
    std::map<int32_t, TPowerStationInfo> _powerStations;
    
    int32_t _nextPSForUpdate = 0;
    int32_t _setId = 0;
    
    NC_STACK_base *_setData = NULL;
    World::RefBactList _unitsList;
    World::RefBactList _deadCacheList;

    // The ids the next new unit and the next new squad get, counted from the
    // level's start
    uint32_t _nextUnitId = World::CVFirstUnitId;
    int32_t _nextCommandId = 1;
    std::vector<NC_STACK_base *> _vhclModels;
    std::array<TLego, 256> _legoArray; 
    std::array<TSubSectorDesc, 256> _subSectorArray;
    std::array<TSectorDesc, 256> _secTypeArray;  
    
    std::vector<World::TVhclProto> _vhclProtos;
    std::vector<World::TWeapProto> _weaponProtos;
    std::vector<World::TBuildingProto> _buildProtos;
    std::vector<World::TRoboProto> _roboProtos;
    
    std::list<NC_STACK_base *> _overrideModels;
    
    std::map<int32_t, TConstructInfo> _inBuildProcess; // Buildings in creation process
    std::array<int16_t, 256> _buildHealthModelId = Common::ArrayInit<int16_t, 256>(0);
    Common::PlaneArray<uint8_t, 64, 64> _sqrtTable = Common::PlaneArray<uint8_t, 64, 64>::ArrayInit(0);
    
    vec3d _viewerPosition;
    mat3x3 _viewerRotation;
    
    NC_STACK_base *_skyObject  = NULL;
    
    int32_t _renderSectors = 9; // Render distance in sectors, across the viewer's
    
    NC_STACK_base *_beeBox = NULL;
    NC_STACK_skeleton *_colsubSkeleton  = NULL;
    NC_STACK_skeleton *_colcompSkeleton  = NULL;
    NC_STACK_bitmap *_tracyBitmap  = NULL;
    NC_STACK_bitmap *_shadermpBitmap  = NULL;
    
    bool _doNotRender = false;
    
    Common::PlaneArray<NC_STACK_base *, 6, 6> _fillersHorizontal = Common::PlaneArray<NC_STACK_base *, 6, 6>::ArrayInit(NULL);
    Common::PlaneArray<NC_STACK_base *, 6, 6> _fillersVertical = Common::PlaneArray<NC_STACK_base *, 6, 6>::ArrayInit(NULL);
    
    NC_STACK_skeleton *_fillerSide = NULL;
    NC_STACK_skeleton *_fillerCross = NULL;
    
    int32_t _normalVizLimit = 2100;  // Objects' own fog: black from here
    int32_t _normalFadeLength = 900; // over this much before it
    bool _viewDistanceSet = false;
    TViewDistance _viewDistance; // Used once _viewDistanceSet
    ViewDistanceWindow _viewDistanceWindow; // Opened with F10, see DrawPerfOverlay
    int32_t _skyVizLimit = 0;
    int32_t _skyFadeLength = 0;
    int32_t _skyHeight = 0;
    bool _skyRender = true;
    bool _doEnergyRecalc = true; // Update powerstations energy give

    int32_t audio_volume = 0;
    
    bool _hasSGMSave = false;
    bool _canRestart = false;
    
    bool _gamePaused = false;
    uint32_t _gamePausedTimeStamp = 0;
    
    int32_t _timeStamp = 0;
    int32_t _frameTime = 0;
    uint32_t _framesElapsed = 0;

    // What the last steps left to draw, and what makes it
    World::Frames _frames;
    std::unique_ptr<World::FrameMaker> _frameMaker;

    // The frames of steps before those kept, being made from the recording;
    // and where they ended when that failed, not to try again
    std::unique_ptr<World::FramesFromRecording> _framesBack;
    uint32_t _framesBackFailedTo = UINT32_MAX;

    // What drawing a unit's models keeps from one frame to the next: their
    // particles' emitters
    struct TUnitDrawing
    {
        std::unique_ptr<NC_STACK_base::Instance> Model;
        std::unique_ptr<NC_STACK_base::Instance> Extras[3];
    };

    std::unordered_map<uint32_t, TUnitDrawing> _unitDrawings; // By unit id
    std::vector<std::unique_ptr<NC_STACK_base::Instance>> _buildingDrawings; // 9 a sector

    // The units of the frame drawn, by sector: the first's index in each
    // sector, and the next's after each, -1 for none
    std::vector<int32_t> _drawnUnitFirst, _drawnUnitNext;

    // How the sound shook the camera in the last step: only the picture
    // shakes
    mat3x3 _cameraShake = mat3x3::Ident();

    // The camera the world is drawn from, and the one the benchmark puts in
    // place of the player's
    TF::TForm3D _drawnCamera;
    bool _cameraOverridden = false;
    vec3d _cameraOverridePos;
    mat3x3 _cameraOverrideRot;
    
    std::string _buildDate; 
    
    bool _playerHSDestroyed = false;
    
    bool _fireBtnIsDown = false; // true - fire btn is down, contigues
    bool _fireBtnDownHappen = false; // true happen on down, single
    bool _guiLoaded = false;
    
    std::array<TileMap *, 92> _guiTiles = Common::ArrayInit<TileMap *, 92>(NULL);
    
    GuiBaseList _guiActive;
    
    Common::Point _screenSize;

    bool _guiDragging = false;
    GuiBase *_guiDragElement = NULL;
    Common::Point _guiDragPos;
    bool _guiDragDefaultMouse = false;

    bool _mouseGrabbed = false; // Grab mouse for unit steer-turn
    
    int32_t _toolTipId = 0;
    int32_t _toolTipHotKeyId = -1; // Used to display hotkey name
    
    std::array<SDL_Color, World::COLOR_MAX_NUMBER> _iniColors;
    
    uint32_t _msgTimestampGates = 0;
    uint32_t _msgTimestampHSReturn = 0;
    uint32_t _msgTimestampEnemySector = 0;
    uint32_t _msgTimestampPSUnderAtk = 0;

    // What the player was last told of each message, and when they were
    // last told of finding each owner's host station, to not repeat them
    std::array<TLastRoboMessage, 100> _lastRoboMessages;
    std::array<int32_t, World::CVFractionsCount> _lastEnemyHostFoundMsgTime;
    
    uint32_t _vehicleTakenControlTimestamp = 0;
    uint32_t _vehicleTakenCommandId = 0;

    bool _invulnerable = false;
    
    int32_t _fontH = 0;
    int32_t _fontDefCloseW = 0;
    int32_t _fontVBScrollW = 0;
    int32_t _fontHScrollH = 0;
    int32_t _fontBorderW = 0;
    int32_t _fontBorderH = 2;
    int32_t _fontVScrollH = 0;
    
    int32_t _iconOrderW = 0;
    int32_t _iconOrderH = 0;
    int32_t _iconHelpW = 0;
    int32_t _iconHelpH = 0;
    
    int32_t _upScreenBorder = 0;
    int32_t _downScreenBorder = 0; 
    
    uint32_t _guiActFlags = 0;
    uint32_t _doAction = World::DOACTION_0;
    
    Common::Point _cellOnMouse = Common::Point(-1, -1); // The sector under the pointer
    vec3d _cellMouseIsectPos; // intersect position in cell

    vec3d _mouseVector; // normalized mouse cursor vector
    
    MapScene::Unit _bactOnMouse; // The unit under the pointer
    float _mouseSelDistance = 0.0;
    Common::Point _prevMousePos;
    MapScene::Unit _bactPrevClicked; // used for dblclick
    int _actionEnergyCost = 0; // Of what a click would order now
    vec3d _beamGroundPos;      // Where on the ground a beam was asked for
    
    bool _makingWaypointsMode = false;
    int32_t _waypointCount = 0;
    vec3d _firstWaypointPosition;

    bool _mouseCursorHidden = false;
    Common::Point _mouseCursorHidePos;
    
    update_msg _updateMessage;
    
    int8_t _showDebugMode = 0; // debug info draw modes
    bool _showPerfOverlay = false; // FPS and frame timings, toggled with F10
    
    int32_t _polysCount = 0;
    int32_t _polysDraw = 0;
    uint16_t _FPS = 0;
    
    bool _playerInHSGun = false;
    uint8_t _ownerOldCellUserUnit = 0; // Used to check entering enemy sector
    
    // Everyone playing, and the one at this machine among them
    std::vector<std::unique_ptr<World::Player>> _players;
    World::Player *_localPlayer = NULL;
    uint32_t _nextPlayerId = 1;
    
    std::array<uint32_t, 8> _countSectorsPerOwner = Common::ArrayInit<uint32_t, 8>(0);
    std::array<uint32_t, 8>  _countUnitsPerOwner = Common::ArrayInit<uint32_t, 8>(0);
    std::array<float, 8> _reloadRatioClamped = Common::ArrayInit<float, 8>(0.0); // clamped to 0.0..1.0
    std::array<float, 8> _reloadRatioPositive = Common::ArrayInit<float, 8>(0.0); // 0 and positive
    
    // The player's squads, by their commanders, in the order of their
    // command ids
    std::vector<MapScene::Unit> _cmdrsRemap;
    uint32_t _activeCmdrID = 0; // active CMDR ID
    int32_t _activeCmdrRemapIndex = 0; // index in _cmdrsRemap
    int32_t _activeCmdrKidsCount = 0; // count of alive units in active squad
    int32_t _cmdrIdToSelect = -1;
    
    uint32_t _prevUnitId = 0;
    
    MapScene::Unit _lastMsgSender; // Id 0 for none
    
    int32_t _screenShotCount = 0;
    
    bool _screenShotSeq = false;
    int32_t _screenShotSeqId = 0; //Screenshoting sequence id
    int32_t _screenShotSeqFrame = 0; //Screenshoting frame
    
    World::Recorder _recorder; // --record
    bool _rewindKeyHeld = false; // Since it went back: not to go on going back

    // Going back through the frames of the steps the level was rewound over,
    // drawing the step at _rewindPosition, down to _rewindTarget, where the
    // level is then restored to its state then, as the recording kept it
    bool _rewinding = false;
    double _rewindPosition = 0.0;
    uint32_t _rewindTarget = 0;
    std::unique_ptr<World::State::InPlaceRestore> _rewindRestore;

    double _drawnStep = 0.0; // The step drawn last, or how far between two

    // The replay being watched (StartReplay): the step drawn or how far
    // between two, whether it's paused, how fast it plays (an index of
    // ReplaySpeeds), whether the timeline is being dragged and whether it's
    // to be left
    std::unique_ptr<World::FrameSource> _replay;
    double _replayPosition = 0.0;
    bool _replayPaused = false;
    int _replaySpeed = 0;
    bool _replayDragging = false;
    bool _replayLeave = false;
    bool _replayLiveEnded = false; // Watched live, and nothing more will come

    // How it's looked at, the windows of its factions by owner and its
    // map, and whether they and the HUD are shown
    ReplayCamera _replayCamera;
    std::array<ReplayFactionWindow, 8> _replayFactions;
    ReplayMapScene _replayScene;   // Of the frame drawn, for the game's map
    uint32_t _replayHovered = 0;   // The unit under the pointer
    uint32_t _replayPressed = 0;   // And when the button went down
    bool _replayWindowsShown = true;
    bool _replayHudShown = true;
    int32_t _replayClock = 0; // Real time the viewer has run, for what blinks
    Common::Point _replayBorders; // The windows' upper and lower borders before

    void HandleReplayInput(TInputState *inpt);
    void ToggleReplayPause();
    void DrawReplayControls();

    void SetReplayCameraMode(ReplayCamera::Mode mode, const World::Frame *frame);
    void FollowReplayPlayer(uint32_t player);
    void LookFromUnit(uint32_t unit);
    bool LookFromNextUnit(const World::Frame &frame);
    void HandleReplayWindows(TInputState *inpt, const World::Frame &frame);
    uint32_t ReplayUnitInWorld(const Common::Point &mouse, const World::Frame &frame) const;
    void FlyReplayCamera(TInputState *inpt);
    void PlaceReplayCamera(World::Frame &frame);
    uint32_t ReplayViewedUnit(const World::Frame &frame) const;
    void UpdateReplayWindows(const World::Frame &frame);
    void DrawReplayWindows();
    void DrawReplayHud(const World::Frame &frame);
    void CloseReplayWindows();
    void OpenReplayWindows();
    World::Frame _replayFrame; // The last drawn, for the input of the next

    // Playing a game watched live (FrameSource::Player): the game's own
    // interface, on the frames shown, its orders sent to the game
    bool _replayPlaying = false;
    bool _playStepped = false;     // Its interface stepped since it began
    PlayMapScene _playScene;
    uint32_t _playUnit = 0;        // The player's unit in the frame before
    bool _playSelectNewSquad = false; // A squad was ordered: select it once it comes
    std::vector<uint32_t> _playSquadsBefore;

    void StartPlaying();
    void StopPlaying();
    void PlayInterfaceStep(TInputState *inpt);
    void PlayFromFrame(const World::Frame &frame);

    // Whether the level has stepped since it was set up: the interface shows
    // it as a step leaves it
    bool _stepped = false;
    
    THudVisor _guiVisor;
    
    std::vector<TMapGem> _techUpgrades; // tech upgrades in level
    int32_t _upgradeId = 0;
    uint32_t _upgradeTimeStamp = 0;
    int32_t _upgradeVehicleId = 0;
    int32_t _upgradeWeaponId = 0;
    int32_t _upgradeBuildId = 0;

    TMapRegionsNet _globalMapRegions; // level selector
    TLevelInfo _levelInfo; // current level information

    // The description of the level set up last, which its recording keeps:
    // the text of its file, read by the script parsers
    std::string _levelDescription;
    
    TBriefengScreen _briefScreen;
    
    // The last level's history, played from its recording by the debriefing
    // and counted by the score
    World::History::Replay _history;

    // The recording the debriefing shows, empty when there's none; its size,
    // and whether it was kept as a replay
    std::string _debriefRecording;
    uint64_t _debriefRecordingSize = 0;
    bool _debriefReplaySaved = false;

    // What the debriefing shows of the world, kept while its replay is
    // watched (KeepDebriefing), as setting the replay's level up reads over
    // it
    struct TKeptDebriefing
    {
        World::History::Replay History;
        Common::PlaneBytes PrimevalTypeMap, PrimevalOwnMap;
        int32_t LevelId = 0;
        std::string LevelDescription;
        int8_t PlayerOwner = 0;
        int OwnerMask = 0, UserMask = 0;
    };

    std::unique_ptr<TKeptDebriefing> _keptDebriefing;
    
    int32_t _stoudsonWaveVehicleId = 0;
    int32_t _stoudsonCenterVehicleId = 0; // Not used :-/
    
    bool _prepareDebrief = false; // prepare debrief and do it? or exit without
    bool _gameWasNetGame = false;
    uint8_t _userOwnerIdWasInNetGame = 0;
    
    Common::PlaneBytes _lvlTypeMap;
    Common::PlaneBytes _lvlOwnMap;
    Common::PlaneBytes _lvlBuildingsMap;
    Common::PlaneBytes _lvlHeightMap;

    // copy of map datas on level start, for debrief
    Common::PlaneBytes _lvlPrimevalTypeMap;
    Common::PlaneBytes _lvlPrimevalOwnMap;
    
    
    THudState _hud;
    
    bool _shellConfIsParsed = false;
    uint32_t  _preferences = 0;
    TMFWinStatus _roboMapStatus;
    TMFWinStatus _roboFinderStatus;
    TMFWinStatus _vhclMapStatus;
    TMFWinStatus _vhclFinderStatus;
    
    int32_t _fxLimit = 0;
    
    std::array<int32_t, World::CVFractionsCount> _dbgSquadCounter = Common::ArrayInit<int32_t, World::CVFractionsCount>(0);
    std::array<int32_t, World::CVFractionsCount> _dbgVehicleCounter = Common::ArrayInit<int32_t, World::CVFractionsCount>(0);
    std::array<int32_t, World::CVFractionsCount> _dbgFlakCounter = Common::ArrayInit<int32_t, World::CVFractionsCount>(0);
    std::array<int32_t, World::CVFractionsCount> _dbgRoboCounter = Common::ArrayInit<int32_t, World::CVFractionsCount>(0);
    std::array<int32_t, World::CVFractionsCount> _dbgWeaponCounter = Common::ArrayInit<int32_t, World::CVFractionsCount>(0);
    
    int32_t _dbgTotalSquadCount = 0;
    int32_t _dbgTotalSquadCountMax = 0;
    int32_t _dbgTotalVehicleCount = 0;
    int32_t _dbgTotalVehicleCountMax = 0;
    int32_t _dbgTotalFlakCount = 0;
    int32_t _dbgTotalFlakCountMax = 0;
    int32_t _dbgTotalRoboCount = 0;
    int32_t _dbgTotalRoboCountMax = 0;
    int32_t _dbgTotalWeaponCount = 0;
    int32_t _dbgTotalWeaponCountMax = 0;
    
    uint32_t _ffTimeStamp = 0;
    int32_t _ffEffectType = 0;
    float _ffPeriod = 0.0;
    float _ffMagnitude = 0.0;
    
    NC_STACK_windp *_netDriver = NULL;
    uint32_t _netUpdateTimeStamp = 0;
    bool _isNetGame = false;
    bool _isNetGameStarted = false;
    int32_t _netFlushTimer = 0;

    bool _netInfoOverkill = false;

    int32_t _netStartTimer = 0;
    
    std::string _netTcpAddress; // your tcp address string (for printing)

    bool _netExclusiveGem = false;

    int32_t _aiTimeline = 600000;  // game.timeline until the profile sets it
    bool _moviesEnabled = true;    // gfx.movie_player until the profile sets it
    
    bool _netChatSystem = false;

    
    enum PFID
    {
        PFID_FPS        = 0,
        PFID_FRAMETIME  = 1,
        PFID_MARKTIME   = 2, // time elapsed for change view mask of sectors
        PFID_GUITIME    = 3,
        PFID_UPDATETIME = 4,
        PFID_RENDERTIME = 5,
        PFID_NETTIME    = 6,
        PFID_POLYGONS   = 7,
        PFID_NEWGUITIME = 8,
        
        PFID_MAX        = 9
    };
    
    int32_t _profileFramesCount = 0;   
    std::array<uint32_t, PFID_MAX> _profileVals = Common::ArrayInit<uint32_t, PFID_MAX>(0);
    std::array<uint32_t, PFID_MAX> _profileMax = Common::ArrayInit<uint32_t, PFID_MAX>(0);
    std::array<uint32_t, PFID_MAX> _profileMin = Common::ArrayInit<uint32_t, PFID_MAX>(0);
    std::array<uint32_t, PFID_MAX> _profileTotal = Common::ArrayInit<uint32_t, PFID_MAX>(0);
    
    std::array<World::TPlayerStatus, World::CVFractionsCount> _playersStats;
    std::array<World::TPlayerStatus, World::CVFractionsCount> _gameplayStats;
    
    int32_t _maxRoboEnergy = 0;
    int32_t _maxReloadConst = 0;
    
    TSingleVoiceMessage _voiceMessage;
    
    bool _joyIgnoreX = false;
    bool _joyIgnoreY = false;
    bool _joyIgnoreZ = false;
    
    float _oldJoyX = 0.0; // input slider 12
    float _oldJoyY = 0.0; // input slider 13
    float _oldJoyZ = 0.0; // input slider 14
    float _oldJoyHatX = 0.0; // input slider 15
    float _oldJoyHatY = 0.0; // input slider 16
    bool _joyKnown = false;  // The old ones were read
    
    std::array<std::string, World::MOVIE_MAX_NUMBER> _movies;
    
    int8_t _firstContactFaction = 0;
    
    std::string _helpURL;
    std::string _helpSavedURL; 
    
    float _maxImpulse = 0.0;
    

    TNetGameEvent _netEvent;
    
    
    float _vehicleSectorRatio = 0.0; // not used
    uint32_t _defaultUnitLimit = 0;
    uint32_t _defaultUnitLimitArg = 0;
    uint32_t _defaultUnitLimitType = 0;
    uint32_t _levelUnitLimit = 0;
    uint32_t _levelUnitLimitArg = 0;
    uint32_t _levelUnitLimitType = 0;
    
    int16_t _kbdLastKeyHit = Input::KC_NONE;
    int32_t _tipOfDayId = 0;
    int32_t _beamEnergyStart = 0;
    int32_t _beamEnergyAdd = 0;
    int32_t _beamEnergyCapacity = 0;
    int32_t _beamEnergyCurrent = 0;
    
    bool _easyCheatKeys = false;

    int8_t _playerOwner = 0;
        
    std::string _initScriptFilePath;

    /* Allow to build multiple buildings at the same time 
       Can be set in
       world: misc scope  - "multi_building" 
       level: level scope  - "multi_building" */
    bool _allowMultiBuildWorld = false;
    bool _allowMultiBuildLevel = false;
    
    /* Fix original bug */
    bool _fixWeaponRadius = false;
    
    /* */
    uint32_t _worldHiddenFractions = World::OWNER_BLACK_BIT;
    uint32_t _hiddenFractions = World::OWNER_BLACK_BIT;

protected:

    /** On saved game load variables **/
    bool _extraViewEnable = false; // If you seat in robo gun
    int _extraViewNumber = -1; // robo gun index
    
    World::ParticleSystem _particles;

    World::TutorialHints _tutorialHints;
};

#endif
