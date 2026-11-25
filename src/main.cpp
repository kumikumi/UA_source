#define GLOBAL
// Gives Windows GUI programs their WinMain entry point, as SDL2main did.
#include <SDL3/SDL_main.h>
#include <cstdlib>
#include "system/fsmgr.h"
#include "system/system.h"
#include "env.h"

#include "includes.h"
#include "system/gfx.h"
#include "system/inpt.h"
#include "system/perf.h"
#include "winp.h"
#include "wintimer.h"

#include "ade.h"
#include "area.h"
#include "amesh.h"
#include "bitmap.h"
#include "bmpAnm.h"
#include "base.h"
#include "ilbm.h"
#include "particle.h"
#include "embed.h"
#include "network.h"
#include "windp.h"
#include "ypabact.h"
#include "ypatank.h"
#include "ypacar.h"
#include "ypaflyer.h"
#include "yparobo.h"
#include "ypaufo.h"
#include "3ds.h"
#include "image.h"


#include "font.h"
#include "yw.h"
#include "benchmark.h"

#include "button.h"



#include "system/movie.h"
#include "system/inivals.h"
#include "system/cmdline.h"
#include "obj3d.h"


int tmpSettingsSaved = 0;
std::string buildDate;

int tform_inited = 0;
int audio_inited = 0;
int input_inited = 0;

enum GAME_SCREEN_MODE {
    GAME_SCREEN_MODE_UNKNOWN = 0,
    GAME_SCREEN_MODE_MENU = 1,
    GAME_SCREEN_MODE_GAME = 2
};

GAME_SCREEN_MODE GameScreenMode;
UserData userdata;

static Benchmark benchmark; // --bench

// --headless: nothing shown, heard or read from the controls, and the
// level run as fast as the machine can. --time-limit in timer ticks, 0 for
// none.
static bool headless = false;
static int32_t headlessTimeLimit = 0;

static std::string recordFile; // --record
static bool recordFull = false; // --record-full
static uint32_t checkResumeStep = 0; // --check-resume

static bool StartDirectLevel();

// The game advances in fixed steps, so how units behave does not depend on
// the frame rate. 60 steps per second is the rate the port ran at while it
// was capped to 60 fps. The original game had no fixed step: each frame
// advanced it by the time the frame took, plus one tick, at whatever frame
// rate the machine managed.
static const int32_t SimStepsPerSecond = 60;

// Step length in timer ticks (1/1024 s): 17 at 60 steps per second.
static const int32_t SimStepTicks = 1024 / SimStepsPerSecond;

// At most this many steps per frame. A machine that cannot keep up makes the
// game run slower, instead of frames growing longer as they catch up.
static const int32_t SimMaxStepsPerFrame = 4;

static int32_t simPendingTicks = 0;   // Time not simulated yet
static TInputState lastStepInput;      // Input of the last simulation step

// Drops the one-time events of an input state, leaving what is held down.
static void ClearInputEvents(TInputState *inpt)
{
    inpt->KbdLastHit = Input::KC_NONE;
    inpt->HotKeyID = -1;
    inpt->chr = 0;
    inpt->ClickInf.flag &= ~(TClickBoxInf::FLAG_LM_DOWN | TClickBoxInf::FLAG_LM_UP |
                             TClickBoxInf::FLAG_RM_DOWN | TClickBoxInf::FLAG_RM_UP |
                             TClickBoxInf::FLAG_MM_DOWN | TClickBoxInf::FLAG_MM_UP |
                             TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_UP |
                             TClickBoxInf::FLAG_DBL_CLICK);
}

// Runs the simulation steps that are due and draws a frame. frameTicks is the
// real time since the last frame. timeFrozen holds the game nearly still, as
// while waiting for network players.
int ProcessGameplayFrame(int32_t frameTicks, bool timeFrozen)
{
    bool draw = true;
    bool stepped = false;
    TInputState renderInput;

    // How far into the last step to draw. Frames that do not step normally
    // (pause, waiting for the network) draw where the last step ended.
    float stepFraction = 1.0;

    if ( benchmark.IsFrozen() )
    {
        // The same frame again: no time passes and the game does not step
        frameTicks = 0;
    }
    else if ( ypaworld->_gamePaused )
    {
        // The paused screen keeps drawing, and time runs for its blinking.
        // A key ends the pause; it does not also act in the game.
        input_states = TInputState();
        Input::Engine.QueryEvents(&input_states);
        input_states.Period = frameTicks;

        world_update_arg.DTime = frameTicks;
        world_update_arg.TimeStamp += frameTicks;
        world_update_arg.Input = &input_states;

        draw = false;
        simPendingTicks = 0;

        if ( !ypaworld->PausedFrame(&world_update_arg) )
            draw = true; // The pause ended, draw the game again
    }
    else
    {
        int32_t stepTicks = SimStepTicks;

        if ( timeFrozen )
        {
            // TODO(multiplayer): Network games have not been played since the
            // simulation moved to fixed steps. Check this freeze while waiting
            // for players, and that remote units still move smoothly between
            // network updates.

            // One step of 2 ticks per frame, as in the original: the network
            // keeps running while the game stands nearly still.
            stepTicks = 2;
            simPendingTicks = stepTicks;
        }
        else
        {
            // Screenshot sequences advance the game 40 ticks per picture
            if ( ypaworld->_screenShotSeq )
                frameTicks = 40;

            simPendingTicks += frameTicks;

            if ( simPendingTicks > SimMaxStepsPerFrame * SimStepTicks )
                simPendingTicks = SimMaxStepsPerFrame * SimStepTicks;

            // A level is not drawn before its first step, nor before a step
            // has set the view point again after it was restored
            if ( ypaworld->_framesElapsed == 0 )
                simPendingTicks = SimStepTicks;
            else if ( !TF::Engine.GetViewPoint() && simPendingTicks < SimStepTicks )
                simPendingTicks = SimStepTicks;
        }

        while ( simPendingTicks >= stepTicks )
        {
            simPendingTicks -= stepTicks;

            input_states = TInputState();
            Input::Engine.QueryEvents(&input_states);
            input_states.Period = stepTicks;

            // Only the first step of a frame can have new key presses and
            // clicks; the frame's own keys (F9, F10, screenshots) see those.
            if ( !stepped )
                renderInput = input_states;

            stepped = true;

            world_update_arg.DTime = stepTicks;
            world_update_arg.TimeStamp += stepTicks;
            world_update_arg.Input = &input_states;

            draw = ypaworld->SimulationStep(&world_update_arg);

            lastStepInput = input_states;

            // Stop at a pause, or when the level ends, restarts, saves or loads
            // (handled below). STATE_PAUSED is the in-game menu: it runs on.
            int state = ypaworld->GetLevelInfo().State;

            if ( !draw || ypaworld->_gamePaused ||
                 (state != TLevelInfo::STATE_PLAYING && state != TLevelInfo::STATE_PAUSED) )
            {
                simPendingTicks = 0;
                break;
            }
        }

        // The time left over is how far into the next step real time is, so
        // draw that far between the last two steps. This shows the game up to
        // one step late, in exchange for smooth motion at any frame rate.
        if ( !timeFrozen )
            stepFraction = (float)simPendingTicks / (float)stepTicks;
    }

    if ( draw )
    {
        if ( !stepped )
        {
            renderInput = lastStepInput;
            ClearInputEvents(&renderInput);
        }

        TUpdateArgs renderArg = world_update_arg;
        renderArg.DTime = frameTicks;
        renderArg.Input = &renderInput;

        GFX::Engine.SetAnimationTime(world_update_arg.TimeStamp - (1.0 - stepFraction) * SimStepTicks);

        ypaworld->RenderFrame(&renderArg, stepFraction);

        if ( benchmark.IsActive() && !benchmark.FrameDrawn(ypaworld) )
            return 0; // Done, quit
    }

    const TLevelInfo &levelInfo = ypaworld->GetLevelInfo();

    switch( levelInfo.State )
    {
    case TLevelInfo::STATE_COMPLETED:
    case TLevelInfo::STATE_ABORTED:
    {
        ypaworld->DeleteLevel();

        if ( (tmpSettingsSaved || levelInfo.State == TLevelInfo::STATE_ABORTED) && !userdata.DirectStart )
        {
            if ( !ypaworld->LoadSettings("settings.tmp", 
                                         userdata.UserName,
                                         (World::SDF_BUDDY | World::SDF_PROTO | World::SDF_USER),
                                         false))
                return 0;

            tmpSettingsSaved = 0;
        }

        int keepRunning = 1;

        if ( userdata.remoteMode )
            keepRunning = 0;

        if ( userdata.EnvMode == ENVMODE_NETPLAY )
        {
            userdata.returnToTitle = true;
            userdata.EnvMode = ENVMODE_SINGLEPLAY;
        }
        else
        {
            userdata.returnToTitle = false;
        }

        GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;
        world_update_arg.TimeStamp = 0;

        userdata.GameIsOver = true;

        // Started from the command line: the campaign screen, for the
        // debriefing, after which the game quits
        if ( userdata.DirectStart )
            userdata.EnvMode = ENVMODE_SINGLEPLAY;

        if ( ypaworld->OpenGameShell() )
        {
            GameScreenMode = GAME_SCREEN_MODE_MENU;
            Input::Engine.QueryInput(&input_states);

            if (!keepRunning)
                return 0;
        }
        else
        {
            ypa_log_out("GameShell-Error!!!\n");
            ypaworld->DeinitGameShell();
            return 0;
        }
    }
    break;

    case TLevelInfo::STATE_RESTART:
    {
        ypaworld->DeleteLevel();

        if ( userdata.DirectStart )
        {
            if ( !StartDirectLevel() )
                return 0;
        }
        else if ( !ypaworld->RestartLevel() )
            ypa_log_out("Warning, could not start the level again\n");

        Input::Engine.QueryInput(&input_states);
    }
    break;

    case TLevelInfo::STATE_SAVE:
    {
        if ( !ypaworld->SaveGame( UserData::SaveGamePath(userdata.UserName) ) )
            ypa_log_out("Warning, Save error\n");

        Input::Engine.QueryInput(&input_states);
    }
    break;

    case TLevelInfo::STATE_LOAD:
    {
        ypaworld->DeleteLevel();

        if ( !ypaworld->LoadGame( UserData::SaveGamePath(userdata.UserName) ) )
            ypa_log_out("Warning, load error\n");

        Input::Engine.QueryInput(&input_states);
    }
    break;

    case TLevelInfo::STATE_REWIND:
    {
        ypaworld->RewindLevel();
        Input::Engine.QueryInput(&input_states);
    }
    break;

    default:
        break;
    }

    return 1;
}

int ProcessMenuFrame()
{
    userdata.GlobalTime = world_update_arg.TimeStamp;
    userdata.DTime = world_update_arg.DTime;
    userdata.Input = &input_states;

    ypaworld->ProcessGameShell();

    // Started from the command line, the shell only comes after the level:
    // the game quits where it would go back to the level map
    if ( userdata.DirectStart && ypaworld->GetLevelInfo().State == TLevelInfo::STATE_MENU )
        return 0;
    
    if ( userdata.envAction.action == EnvAction::ACTION_QUIT )
        return 0;
    else if ( userdata.envAction.action == EnvAction::ACTION_PLAY )
    {
        userdata.SaveSettings();
        ypaworld->CloseGameShell();

        GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

        if ( !userdata.SaveBuildProtoState() )
            return 0;

        TLevelLoadRequest loadRequest;
        loadRequest.lvlID = userdata.envAction.params[0];

        if ( !ypaworld->AdvancedCreateLevel(&loadRequest) )
        {
            ypa_log_out("Sorry, unable to init this level!\n");

            ypaworld->DeinitGameShell();
            return 0;
        }
        GameScreenMode = GAME_SCREEN_MODE_GAME;
        Input::Engine.QueryInput(&input_states);
    }
    else if ( userdata.envAction.action == EnvAction::ACTION_LOAD )
    {
        GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

        const TLevelInfo &levelInfo = ypaworld->GetLevelInfo();

        userdata.SaveSettings();
        ypaworld->CloseGameShell();

        if ( !userdata.SaveBuildProtoState() )
            return 0;

        if ( !ypaworld->LoadGame( UserData::SaveGamePath(userdata.UserName) ) )
        {
            ypa_log_out("Error while loading level (level %d, User %s\n", levelInfo.LevelID, userdata.UserName.c_str());

            ypaworld->DeinitGameShell();
            return 0;
        }
        GameScreenMode = GAME_SCREEN_MODE_GAME;
        Input::Engine.QueryInput(&input_states);
    }
    else if ( userdata.envAction.action == EnvAction::ACTION_NETPLAY )
    {
        userdata.SaveSettings();

        if ( !userdata.SaveBuildProtoState() )
            return 0;

        tmpSettingsSaved = 1;

        ypaworld->CloseGameShell();

        TLevelLoadRequest loadRequest;
        loadRequest.lvlID = userdata.envAction.params[0];

        GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

        if ( !ypaworld->CreateNetLevel(&loadRequest) )
        {
            ypa_log_out("Sorry, unable to init this level for network!\n");
            ypaworld->DeinitGameShell();

            return 0;
        }

        GameScreenMode = GAME_SCREEN_MODE_GAME;
        Input::Engine.QueryInput(&input_states);
    }

    return 1;
}


int ProcessNextFrame()
{
    ypaworld->UpdateScreenSize();

    // Gameplay reads input and advances time once per simulation step, in
    // ProcessGameplayFrame, so here it only takes the time since the last frame
    bool gameplay = ( GameScreenMode == GAME_SCREEN_MODE_GAME );

    input_states = TInputState();

    if ( gameplay )
        input_states.Period = Input::Engine.QueryTimer();
    else
        Input::Engine.QueryInput(&input_states);

    int32_t frameTicks = input_states.Period;
    bool timeFrozen = userdata.ResetInputPeriod;

    if ( userdata.ResetInputPeriod )
    {
        input_states.Period = 1;
        userdata.ResetInputPeriod = false;
    }

    input_states.Period++;
    
    world_update_arg.Input = &input_states;

    if ( !gameplay )
    {
        world_update_arg.DTime = input_states.Period;
        world_update_arg.TimeStamp += input_states.Period;
    }
    

    // If mouse captured, enable releative mouse control
    if (ypaworld->_mouseGrabbed)
        System::SetReleativeMouse(true);
    else
        System::SetReleativeMouse(false);

    System::SetTextInput( GameScreenMode == GAME_SCREEN_MODE_MENU && userdata.IsTypingText() );
    
    if ( GameScreenMode == GAME_SCREEN_MODE_MENU )
    {
        return ProcessMenuFrame();
    }
    else if ( GameScreenMode == GAME_SCREEN_MODE_GAME )
    {
        return ProcessGameplayFrame(frameTicks, timeFrozen);
    }
    return 1;
}

int init_classesLists_and_variables()
{
    ypa_log__ypa_general_log();
    init_d3dlog();
    init_dinputlog();

    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_nucleus>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_rsrc>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_bitmap>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_skeleton>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ilbm>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_sklt>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ade>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_area>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_base>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_bmpanim>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_amesh>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_particle>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_embed>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_idev>() );
    //Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_input>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_itimer>() );
    //Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_iwimp>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_sample>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_wav>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_button>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_network>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_winp>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_wintimer>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_windp>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypaworld>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypabact>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypatank>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_yparobo>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypamissile>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypaflyer>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypacar>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypaufo>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypagun>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_3ds>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_image>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_Obj3D>() );

    return 1;
}

void deinit_globl_engines()
{
    if ( tform_inited )
        TF::Engine.Deinit();
    if ( input_inited )
        Input::Engine.Deinit();
    if ( audio_inited )
        SFXEngine::SFXe.deinit();

    ypa_log_out("Nucleus shutdown:\n");
}

int InitEngines()
{
    ypaworld = 0;
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;
    userdata.clear();
    input_states = TInputState();
    world_update_arg = TUpdateArgs();

    if ( !init_classesLists_and_variables() )
    {
        LogStartupError("Couldn't open Nucleus!");
        deinit_globl_engines();
        return 0;
    }

    Common::Env.AddGlobalIniKey("gfx.display  = win3d.class");
    Common::Env.AddGlobalIniKey("gfx.display2 = windd.class");
    Common::Env.AddGlobalIniKey("gfx.engine     = gfx.engine");
    Common::Env.AddGlobalIniKey("tform.engine   = tform_ng.engine");
    Common::Env.AddGlobalIniKey("input.engine   = input.engine");
    Common::Env.AddGlobalIniKey("input.wimp     = winp");
    Common::Env.AddGlobalIniKey("input.keyboard = winp");
    Common::Env.AddGlobalIniKey("input.slider[10] = winp:mousex");
    Common::Env.AddGlobalIniKey("input.slider[11] = winp:mousey");
    Common::Env.AddGlobalIniKey("input.slider[12] = winp:joyx winp:joyrudder");
    Common::Env.AddGlobalIniKey("input.slider[13] = winp:joyy");
    Common::Env.AddGlobalIniKey("input.slider[14] = winp:joythrottle");
    Common::Env.AddGlobalIniKey("input.slider[15] = winp:joyhatx");
    Common::Env.AddGlobalIniKey("input.slider[16] = winp:joyhaty");
    Common::Env.AddGlobalIniKey("input.button[16] = winp:joyb0");
    Common::Env.AddGlobalIniKey("input.button[17] = winp:joyb1");
    Common::Env.AddGlobalIniKey("input.button[18] = winp:joyb2");
    Common::Env.AddGlobalIniKey("input.button[19] = winp:joyb3");
    Common::Env.AddGlobalIniKey("input.button[20] = winp:joyb4");
    Common::Env.AddGlobalIniKey("input.button[21] = winp:joyb5");
    Common::Env.AddGlobalIniKey("input.button[22] = winp:joyb6");
    Common::Env.AddGlobalIniKey("input.button[23] = winp:joyb7");

    audio_inited = SFXEngine::SFXe.init(!headless);
    input_inited = Input::Engine.Init();
    tform_inited = TF::Engine.Init();

    if ( !audio_inited )
    {
        LogStartupError("Couldn't open audio engine!");
        deinit_globl_engines();
        return 0;
    }

    if ( !tform_inited )
    {
        LogStartupError("Couldn't open tform engine!");
        deinit_globl_engines();
        return 0;
    }
    if ( !input_inited )
    {
        LogStartupError("Couldn't open input engine!");
        deinit_globl_engines();
        return 0;
    }

    return 1;
}

int yw_initGameWithSettings()
{
    FSMgr::FileHandle *user_def = uaOpenFileAlloc("env:user.def", "r");

    std::string settingsFileName;

    if ( user_def )
    {
        std::string line;
        user_def->ReadLine(&line);

        settingsFileName = fmt::sprintf("save:%s/user.txt", line);

        FSMgr::FileHandle *user_txt = uaOpenFileAlloc(settingsFileName, "r");

        if ( user_txt )
        {
            delete user_txt;

            userdata.UserName = line;
            settingsFileName = fmt::sprintf("%s/user.txt", line);
        }
        else
        {
            ypa_log_out("Warning: default user file doesn't exist (%s)\n", settingsFileName.c_str());
            settingsFileName = fmt::sprintf("sdu7/user.txt");
            userdata.UserName = "SDU7";
        }

        delete user_def;
    }
    else
    {
        settingsFileName = fmt::sprintf("sdu7/user.txt");
        userdata.UserName = "SDU7";
        ypa_log_out("Warning: No default user set\n");
    }

    userdata.diskListActiveElement = -1;

    int profileIndex = 1;
    for ( ProfileList::iterator it = userdata.profiles.begin(); it != userdata.profiles.end(); it++ )
    {
        if ( !StriCmp(it->name, userdata.UserName) )
        {
            userdata.diskListActiveElement = profileIndex;
            break;
        }

        profileIndex++;
    }

    // Straight into a level from the command line: no intro
    return ypaworld->LoadSettings(settingsFileName,
                                  userdata.UserName,
                                  World::SDF_ALL,
                                  true, !userdata.DirectStart) != 0;
}

void ShutdownGame()
{
    if ( ypaworld )
    {
        if ( GameScreenMode == GAME_SCREEN_MODE_GAME )
        {
            ypaworld->DeleteLevel();
            ypaworld->DeinitGameShell();
        }
        else if ( GameScreenMode == GAME_SCREEN_MODE_MENU )
        {
            userdata.SaveSettings();
            ypaworld->CloseGameShell();
            ypaworld->DeinitGameShell();
        }

        ypaworld->Delete();
    }

    deinit_globl_engines();
}

int InitWorldAndShell()
{
    buildDate = "Jul 09 1998  23:52:47";
//    strcpy(buildDate, __DATE__);
//    strcat(buildDate, " ");
//    strcat(buildDate, __TIME__);

    ypaworld = Nucleus::CInit<NC_STACK_ypaworld>( { {NC_STACK_ypaworld::YW_ATT_BUILD_DATE, std::string(buildDate)} } );
    
    if ( !ypaworld )
    {
        ypa_log_out("Unable to init ypaworld.class\n");
        return 0;
    }

    if ( !ypaworld->InitGameShell(&userdata) )
    {
        ypa_log_out("Unable to init shell structure\n");
        return 0;
    }

    if ( !yw_initGameWithSettings() )
    {
        ypa_log_out("Unable to init game with default settings\n");
        return 0;
    }

    if ( !userdata.HasInited && !ypaworld->OpenGameShell())
    {
        ypa_log_out("Error: Unable to open Gameshell\n");
        return 0;
    }

    GameScreenMode = GAME_SCREEN_MODE_MENU;

    return 1;
}

void HandleMods()
{
    std::string modname;
    
    if (!System::CmdLine::Get("mod", &modname) || modname.empty())
        return;
    
    FSMgr::iNode *node = FSMgr::iDir::findNode("mods");
    
    if (!node || node->getType() != FSMgr::iNode::NTYPE_DIR)
        return;
    
    node->Detach();
    
    FSMgr::iNode *imod = ((FSMgr::iDir *)node)->getNode(modname);
    
    if (!imod || imod->getType() != FSMgr::iNode::NTYPE_DIR)
    {
        delete node;
        return;
    }
    
    imod->Detach();
    delete node;
    
    FSMgr::iDir *mod = dynamic_cast<FSMgr::iDir *>(imod);
    if (!mod)
    {
        delete imod;
        return;
    }

    // Detach original basepath "save" and "env" dirs
    FSMgr::iDir::GetRoot()->Detach("save");
    FSMgr::iDir::GetRoot()->Detach("env");
    
    // Make sure save and env created in mod dir and complete replace it
    FSMgr::iDir::GetRoot()->addNode( mod->MakeDir("save") );
    FSMgr::iDir::GetRoot()->addNode( mod->MakeDir("env") );
    
    FSMgr::iDir::GetRoot()->Override( mod );
    
    delete mod;
    
    World::AllowMods(true);
}

int InitGame()
{
    if ( InitEngines() )
    {
        std::string envDir;
        if (System::CmdLine::Get("env", &envDir))
            Common::Env.SetPrefix("env", envDir);

        if ( InitWorldAndShell() )
            return 1;
        deinit_globl_engines();
    }

    return 0;
}

uint32_t maxTicks = 1000/60; // init on 60FPS

// The level the command line starts the game in: --level, --load or the
// benchmark's. Nothing of it is written to the player's profile.
struct DirectStart
{
    int Level = 0;        // Started as new
    std::string SaveFile; // Or from this saved game or recording
    bool HasStep = false; // --step: from after this step of the recording
    uint32_t Step = 0;
};

static DirectStart directStart;

// Started from a recording: the step it went on from, and the recording's
// start and steps up to there, for a recording that goes on from it
static bool resumedFromRecording = false;
static uint32_t resumedStep = 0;
static World::RecordedPoint resumedStart, resumedPoint;

// False, after saying why, when the command line starts a level in more
// than one way, or names no level
static bool ReadDirectStart()
{
    std::string value;
    int ways = 0;

    if ( System::CmdLine::Get("level", &value) )
    {
        char *end = NULL;
        long level = std::strtol(value.c_str(), &end, 10);

        if ( value.empty() || *end || level < 1 || level > 255 )
        {
            System::CmdLine::PrintError("--level " + value + ": not a level number");
            return false;
        }

        directStart.Level = level;
        ways++;
    }

    if ( System::CmdLine::Get("load", &directStart.SaveFile) )
    {
        if ( directStart.SaveFile.empty() )
        {
            System::CmdLine::PrintError("--load needs FILE");
            return false;
        }

        ways++;
    }

    if ( System::CmdLine::Get("step", &value) )
    {
        char *end = NULL;
        long step = std::strtol(value.c_str(), &end, 10);

        if ( value.empty() || *end || step < 0 )
        {
            System::CmdLine::PrintError("--step " + value + ": not a step");
            return false;
        }

        if ( directStart.SaveFile.empty() )
        {
            System::CmdLine::PrintError("--step needs --load of a recording");
            return false;
        }

        directStart.HasStep = true;
        directStart.Step = step;
    }

    if ( benchmark.IsActive() )
    {
        directStart.Level = benchmark.GetLevel();
        ways++;
    }

    if ( ways > 1 )
    {
        System::CmdLine::PrintError("--level, --load and --bench each start a level; give one");
        return false;
    }

    userdata.DirectStart = (ways == 1);

    headless = System::CmdLine::Has("headless");

    if ( headless && (!userdata.DirectStart || benchmark.IsActive()) )
    {
        System::CmdLine::PrintError("--headless needs --level or --load");
        return false;
    }

    if ( System::CmdLine::Get("time-limit", &value) )
    {
        char *end = NULL;
        long seconds = std::strtol(value.c_str(), &end, 10);

        if ( value.empty() || *end || seconds < 1 || seconds > 24 * 3600 )
        {
            System::CmdLine::PrintError("--time-limit " + value + ": not a number of seconds");
            return false;
        }

        if ( !headless )
        {
            System::CmdLine::PrintError("--time-limit needs --headless");
            return false;
        }

        headlessTimeLimit = seconds * 1024; // Timer ticks
    }

    if ( System::CmdLine::Get("check-resume", &value) )
    {
        char *end = NULL;
        long step = std::strtol(value.c_str(), &end, 10);

        if ( value.empty() || *end || step < 1 )
        {
            System::CmdLine::PrintError("--check-resume " + value + ": not a step");
            return false;
        }

        if ( !headless )
        {
            System::CmdLine::PrintError("--check-resume needs --headless");
            return false;
        }

        checkResumeStep = step;
    }

    if ( System::CmdLine::Get("record", &recordFile) )
    {
        if ( recordFile.empty() )
        {
            System::CmdLine::PrintError("--record needs FILE");
            return false;
        }

        if ( !userdata.DirectStart || benchmark.IsActive() )
        {
            System::CmdLine::PrintError("--record needs --level or --load");
            return false;
        }

        if ( checkResumeStep )
        {
            System::CmdLine::PrintError("--record and --check-resume don't go together");
            return false;
        }
    }

    if ( System::CmdLine::Has("record-full") )
    {
        if ( recordFile.empty() )
        {
            System::CmdLine::PrintError("--record-full needs --record");
            return false;
        }

        recordFull = true;
    }

    return true;
}

// Where a level started from the command line is recorded when --record
// doesn't say
static std::string TemporaryRecordingPath()
{
    const char *dir = getenv("TMPDIR");

    if ( !dir )
        dir = getenv("TEMP");

    if ( !dir )
        dir = getenv("TMP");

    return std::string(dir ? dir : "/tmp") + "/ua_source_level.rec";
}

// Whether the file is a recording, as saved games are
static bool IsRecordingFile(const std::string &path)
{
    FSMgr::FileHandle file(path, "rb");
    char head[12];

    return file.OK() && file.read(head, 12) == 12 && !memcmp(head, "FORM", 4) && !memcmp(head + 8, "RPLY", 4);
}

// Whether the level or saved game the command line names is there
static bool CheckDirectStart()
{
    if ( !directStart.SaveFile.empty() )
    {
        if ( !IsRecordingFile(directStart.SaveFile) )
        {
            System::CmdLine::PrintError(directStart.SaveFile + " is no saved game or recording");
            return false;
        }

        return true;
    }

    const auto &regions = ypaworld->_globalMapRegions.MapRegions;

    if ( directStart.Level < 1 || directStart.Level >= (int)regions.size() || regions[directStart.Level].Status == TMapRegionInfo::STATUS_NONE )
    {
        System::CmdLine::PrintError(fmt::sprintf("there is no level %d", directStart.Level));
        return false;
    }

    if ( regions[directStart.Level].Status == TMapRegionInfo::STATUS_NETWORK )
    {
        System::CmdLine::PrintError(fmt::sprintf("level %d is for network games", directStart.Level));
        return false;
    }

    return true;
}

// Starts the level of a recording as it was after the step --step names, or
// after the last it can go on from
static bool StartFromRecording(const std::string &path)
{
    World::Recording recording;

    if ( !recording.Open(path) )
        return false;

    uint32_t step = directStart.HasStep ? directStart.Step : recording.LastResumable(recording.Steps());

    if ( step > recording.Steps() )
    {
        ypa_log_out("The recording %s has %d steps, not %d\n", path.c_str(), recording.Steps(), step);
        return false;
    }

    if ( !recording.IsResumable(step) )
    {
        ypa_log_out("The level can't go on from step %d of %s, only from one every %d steps: the one before is %d\n",
                    step, path.c_str(), (int)World::Recorder::BlockSteps, recording.LastResumable(step));
        return false;
    }

    if ( !recording.Read(0, resumedStart, true) || !recording.Read(step, resumedPoint, true) )
    {
        ypa_log_out("The recording %s can't be read up to step %d\n", path.c_str(), step);
        return false;
    }

    if ( !World::State::Restore(ypaworld, resumedPoint.View, resumedPoint.Internal, true) )
        return false;

    resumedFromRecording = true;
    resumedStep = step;
    return true;
}

// Leaves the menus for the level the command line names, and starts it again
// for Restart
static bool StartDirectLevel()
{
    ypaworld->CloseGameShell();
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

    bool started;

    if ( !directStart.SaveFile.empty() )
    {
        started = StartFromRecording(directStart.SaveFile);
    }
    else
    {
        // The same level whoever's profile it is
        ypaworld->_levelInfo.Buddies.clear();

        TLevelLoadRequest loadRequest;
        loadRequest.lvlID = directStart.Level;

        started = ypaworld->CreateLevel(&loadRequest) != 0;
    }

    if ( !started )
    {
        ypa_log_out("Could not start the level the command line names\n");
        return false;
    }

    // What the debriefing compares the end of the level with, as when the
    // level map starts a level. A recording's level has its start's.
    if ( !resumedFromRecording )
    {
        ypaworld->_lvlPrimevalTypeMap = ypaworld->_lvlTypeMap;
        ypaworld->_lvlPrimevalOwnMap = ypaworld->_lvlOwnMap;
    }

    // In a window, the debriefing after a win has the level's events from
    // its recording: unless --record names one, it goes to a temporary file,
    // not the profile
    if ( !headless && recordFile.empty() && !ypaworld->_recorder.Start(ypaworld, TemporaryRecordingPath(), "the command line") )
        ypa_log_out("The level isn't recorded, so its debriefing has no events\n");

    GameScreenMode = GAME_SCREEN_MODE_GAME;

    if ( !headless )
        Input::Engine.QueryInput(&input_states);

    return true;
}

// How a headless run has ended, or empty while it goes on
static std::string HeadlessOutcome(int32_t startTime)
{
    int state = ypaworld->GetLevelInfo().State;

    if ( state == TLevelInfo::STATE_COMPLETED )
        return "won";

    if ( state == TLevelInfo::STATE_ABORTED )
        return "aborted";

    // Nobody is there to leave the level, so these end it
    if ( ypaworld->_userRobo->_status == BACT_STATUS_DEAD )
        return "lost, the player's host station destroyed";

    bool enemyLeft = false;
    for ( NC_STACK_ypabact *unit : ypaworld->_unitsList )
    {
        if ( unit != ypaworld->_userRobo && unit->_bact_type == BACT_TYPES_ROBO && unit->_status != BACT_STATUS_DEAD )
            enemyLeft = true;
    }

    if ( !enemyLeft )
        return "won, no enemy host station left";

    if ( headlessTimeLimit && ypaworld->_timeStamp - startTime >= headlessTimeLimit )
        return "stopped at the time limit";

    return std::string();
}

// One headless step, with an empty input
static void HeadlessStep()
{
    input_states = TInputState();
    input_states.Period = SimStepTicks;

    world_update_arg.Input = &input_states;
    world_update_arg.DTime = SimStepTicks;
    world_update_arg.TimeStamp += SimStepTicks;

    ypaworld->SimulationStep(&world_update_arg);
}

// --check-resume: keeps the state, plays on, starts again from what was kept
// and plays the same steps again; each step's state must come out the same.
// Returns the report.
static std::string CheckResume(int32_t startTime, uint32_t step)
{
    const uint32_t CheckSteps = 600;
    const int level = ypaworld->GetLevelInfo().LevelID;

    World::State::Image kept = World::State::Capture(ypaworld);
    const uint32_t keptTimeStamp = world_update_arg.TimeStamp;

    // The steps as they went, packed: there can be many
    std::vector<std::string> played;
    for (uint32_t i = 0; i < CheckSteps && HeadlessOutcome(startTime).empty(); i++)
    {
        HeadlessStep();
        played.push_back(World::State::Pack(World::State::Capture(ypaworld)));
    }

    ypaworld->DeleteLevel();

    if ( !World::State::Restore(ypaworld, kept) )
        return fmt::sprintf("Level %d at step %d: can't start again from the kept state\n", level, step);

    world_update_arg.TimeStamp = keptTimeStamp;

    World::State::Schema schema = World::State::MakeSchema(ypaworld);

    std::string diff = World::State::Diff(schema, kept, World::State::Capture(ypaworld));
    if ( !diff.empty() )
        return fmt::sprintf("Level %d at step %d: the state read back isn't the one kept:\n%s", level, step, diff);

    for (size_t i = 0; i < played.size(); i++)
    {
        HeadlessStep();

        World::State::Image now = World::State::Capture(ypaworld);
        World::State::Image then;
        World::State::Unpack(played[i], then);

        if ( !(now.Objects == then.Objects) )
            return fmt::sprintf("Level %d at step %d: step %d after it differs (as played / started again):\n%s",
                                level, step, (int)i + 1, World::State::Diff(schema, then, now));
    }

    return fmt::sprintf("Level %d at step %d: the same for %d steps after it\n", level, step, (int)played.size());
}

// Runs the level in steps, back to back, with an empty input, until it ends
static void RunHeadless()
{
    const int32_t startTime = ypaworld->_timeStamp; // Late for a saved game
    const uint64_t startTicks = SDL_GetTicksNS();
    uint32_t steps = 0;
    std::string outcome;

    while ( outcome.empty() )
    {
        HeadlessStep();
        steps++;

        if ( steps == checkResumeStep )
        {
            std::string report = CheckResume(startTime, steps);
            fputs(report.c_str(), stdout);
            fflush(stdout);
            return;
        }

        outcome = HeadlessOutcome(startTime);
    }

    double seconds = (SDL_GetTicksNS() - startTicks) / 1e9;
    int32_t gameTime = (ypaworld->_timeStamp - startTime) / 1024;

    std::string report = fmt::sprintf("Level %d %s after %d:%02d of game time: %d steps in %.1f s, %.0f times real time\n",
                                      ypaworld->GetLevelInfo().LevelID, outcome, gameTime / 60, gameTime % 60, steps, seconds,
                                      seconds > 0.0 ? gameTime / seconds : 0.0);
    fputs(report.c_str(), stdout);
    fflush(stdout); // Before anything at exit can end the program
    ypa_log_out("%s", report.c_str());
}

int main(int argc, char *argv[])
{
    switch ( System::CmdLine::Parse(argc, argv) )
    {
    case System::CmdLine::PARSE_HELP:
        return 0;

    case System::CmdLine::PARSE_ERROR:
        return 2;

    default:
        break;
    }
    
    System::IniConf::Init();
    FSMgr::iDir::setBaseDir("");
    
    HandleMods();
    
    System::IniConf::ReadFromNucleusIni();
    
    if (System::IniConf::DevMode.Get<bool>() || System::CmdLine::Has("devmode"))
        World::AllowMods(true);        

    benchmark.ReadCommandLine();

    if ( !ReadDirectStart() )
        return 2;
    
    System::Init(headless);
    
    GFX::Engine.Init(headless);

    if ( !headless )
        System::Movie.Init();

    if ( !InitGame() )
        return 0;

    System::IniConf::ReadFromNucleusIni();

    if ( userdata.DirectStart )
    {
        // Headless, the simulation steps leave out the controls and the
        // in-game interface. Else the benchmark runs fullscreen: the
        // display's resolution every time, and frame times that vary much
        // less than in a window. A level goes as the profile has it, which
        // the intro would have set.
        if ( headless )
            ypaworld->setYW_dontRender(true);
        else
            ypaworld->SetWindowed( !benchmark.IsActive() && userdata.IsWindowedFlag() );

        if ( !CheckDirectStart() || !StartDirectLevel() )
        {
            ShutdownGame();
            GFX::Engine.Deinit();
            System::Deinit();
            return 1;
        }

        // Only the first start: a restart ends the recording, as it ends the
        // level
        if ( !recordFile.empty() )
        {
            std::string from = directStart.SaveFile.empty() ? fmt::sprintf("level %d", directStart.Level) : directStart.SaveFile;
            bool recording;

            ypaworld->_recorder.SetFull(recordFull);

            if ( resumedFromRecording )
            {
                from += fmt::sprintf(" at step %d", resumedStep);
                recording = ypaworld->_recorder.StartFrom(ypaworld, recordFile, from, resumedStart, resumedPoint);
            }
            else
            {
                recording = ypaworld->_recorder.Start(ypaworld, recordFile, from);
            }

            if ( !recording )
            {
                System::CmdLine::PrintError("can't write " + recordFile);
                ShutdownGame();
                GFX::Engine.Deinit();
                System::Deinit();
                return 1;
            }
        }

        if ( headless )
        {
            RunHeadless();

            ShutdownGame();
            GFX::Engine.Deinit();
            System::Deinit();
            return 0;
        }
    }

    uint32_t ticks = 0;

//    int fps = 0;
//    uint32_t fpstick = SDL_GetTicks() + 1000;

    while ( true )
    {

        if (GFX::Engine.FpsMaxTicks == 0)
        {
            if ( !ProcessNextFrame() )
                break;

            if ( System::ProcessEvents() )
                break;
        }
        else
        {
            uint32_t curTick = SDL_GetTicks();

            if (curTick >= ticks)
            {
                if ( !ProcessNextFrame() )
                    break;

                if ( System::ProcessEvents() )
                    break;

                ticks = curTick;

                uint32_t diffTick = SDL_GetTicks() - curTick;

                if (diffTick < GFX::Engine.FpsMaxTicks)
                {
                    uint16_t delay = GFX::Engine.FpsMaxTicks - diffTick;
                    ticks += delay;

                    System::PerfStats::Timer perf(System::PerfStats::SEC_IDLE);
                    SDL_Delay(delay);
                }
            }
            else
            {
                System::PerfStats::Timer perf(System::PerfStats::SEC_IDLE);
                SDL_Delay(1);
            }
        }

//        fps++;
//        if (SDL_GetTicks() > fpstick)
//        {
//            printf("fps %d\n", fps);
//            fpstick = SDL_GetTicks() + 1000;
//            fps = 0;
//        }

    }
    
    ShutdownGame();
    
    GFX::Engine.Deinit();
    System::Deinit();

    return 0;
}
