#define GLOBAL
// Gives Windows GUI programs their WinMain entry point, as SDL2main did.
#include <SDL3/SDL_main.h>
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
#include "obj3d.h"


int tmpSettingsSaved = 0;
int lastReplayIndex = 0;
int replayStarted = 0;
std::string buildDate;

int tform_inited = 0;
int audio_inited = 0;
int input_inited = 0;

enum GAME_SCREEN_MODE {
    GAME_SCREEN_MODE_UNKNOWN = 0,
    GAME_SCREEN_MODE_MENU = 1,
    GAME_SCREEN_MODE_GAME = 2,
    GAME_SCREEN_MODE_REPLAY = 3
};

GAME_SCREEN_MODE GameScreenMode;
UserData userdata;

static Benchmark benchmark; // -bench

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

            // A level is not drawn before its first step
            if ( ypaworld->_framesElapsed == 0 )
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

        if ( tmpSettingsSaved || levelInfo.State == TLevelInfo::STATE_ABORTED )
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

        userdata.lastInputEvent = 0;
        userdata.GameIsOver = true;

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

        if ( !ypaworld->LoadGame( fmt::sprintf("save:%s/%d.rst", userdata.UserName, levelInfo.LevelID) ) )
            ypa_log_out("Warning, load error\n");

        Input::Engine.QueryInput(&input_states);
    }
    break;

    case TLevelInfo::STATE_SAVE:
    {
        if ( !ypaworld->SaveGame( fmt::sprintf("save:%s/%d.sgm", userdata.UserName, 0) ) )
            ypa_log_out("Warning, Save error\n");

        Input::Engine.QueryInput(&input_states);
    }
    break;

    case TLevelInfo::STATE_LOAD:
    {
        ypaworld->DeleteLevel();

        if ( !ypaworld->LoadGame( fmt::sprintf("save:%s/%d.sgm", userdata.UserName, 0) ) )
            ypa_log_out("Warning, load error\n");

        Input::Engine.QueryInput(&input_states);
    }
    break;

    default:
        break;
    }

    return 1;
}

std::string PickReplayFile(uint32_t seed)
{
    if ( userdata.snaps.empty() )
        return std::string();

    int replayIndex = seed % userdata.snaps.size();

    // TODO(bug): lastReplayIndex starts at 0, so with several replays the first
    // pick can never be replay 0.
    if ( lastReplayIndex == replayIndex )
    {
        if ( replayIndex )
        {
            replayIndex = replayIndex - 1;
        }
        else if ( (userdata.snaps.size() - 1) > 0 )
        {
            replayIndex = 1;
        }
    }

    lastReplayIndex = replayIndex;
    return userdata.snaps[replayIndex];
}

int RunReplay(TUpdateArgs *arg)
{
    // TODO(cleanup): replayStarted is set before replay mode starts and never
    // cleared, so it is always true here.
    if ( (userdata.p_YW->_replayPlayer->frame_count - 3) <= userdata.p_YW->_replayPlayer->frame_id  &&  replayStarted )
    {
        ypaworld->StopReplay();

        if ( !ypaworld->StartReplay( PickReplayFile(arg->TimeStamp) ) )
        {
            GameScreenMode = GAME_SCREEN_MODE_MENU;

            Input::Engine.QueryInput(&input_states);

            return 0;
        }

        replayStarted = 1;
        GameScreenMode = GAME_SCREEN_MODE_REPLAY;
    }

    ypaworld->PlayReplayFrame(arg);

    TReplayControl replayControl;
    replayControl.command = 0;
    replayControl.frame = 0;

    int cont_play = 1;

    if ( arg->Input->KbdLastHit == Input::KC_N )
    {
        replayControl.command = 4;
    }
    else if ( arg->Input->KbdLastHit == Input::KC_P )
    {
        replayControl.command = 2;
    }
    else if ( arg->Input->KbdLastHit == Input::KC_R )
    {
        replayControl.command = 3;
    }
    else if ( arg->Input->KbdLastHit == Input::KC_S )
    {
        replayControl.command = 1;
    }
    else if ( arg->Input->KbdLastHit == Input::KC_V )
    {
        replayControl.frame = -10;
        replayControl.command = 7;
    }
    else if ( arg->Input->KbdLastHit == Input::KC_B )
    {
        replayControl.command = 5;
    }
    else if ( arg->Input->KbdLastHit == Input::KC_M )
    {
        replayControl.frame = 10;
        replayControl.command = 7;
    }
    else if ( arg->Input->KbdLastHit == Input::KC_SPACE || arg->Input->KbdLastHit == Input::KC_ESCAPE )
    {
        cont_play = 0;
    }

    if ( replayControl.command )
        ypaworld->ControlReplay(&replayControl);

    return cont_play;
}

int ProcessReplayFrame()
{
    if ( !RunReplay(&world_update_arg) )
    {
        ypaworld->StopReplay();

        if ( tmpSettingsSaved )
        {
            if ( !ypaworld->LoadSettings("settings.tmp",
                                         userdata.UserName,
                                         (World::SDF_BUDDY | World::SDF_PROTO | World::SDF_USER),
                                         false) )
                return 0;

            tmpSettingsSaved = 0;
        }

        GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;
        world_update_arg.TimeStamp = 0;
        userdata.lastInputEvent = 0;

        if ( !ypaworld->OpenGameShell() )
        {
            ypa_log_out("GameShell-Error!!!\n");
            ypaworld->DeinitGameShell();

            return 0;
        }

        userdata.p_YW->_levelInfo.State = TLevelInfo::STATE_MENU;

        GameScreenMode = GAME_SCREEN_MODE_MENU;

        input_states = TInputState();

        Input::Engine.QueryInput(&input_states);
    }
    return 1;
}

int ProcessMenuFrame()
{
    userdata.GlobalTime = world_update_arg.TimeStamp;
    userdata.DTime = world_update_arg.DTime;
    userdata.Input = &input_states;

    ypaworld->ProcessGameShell();
    
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
        loadRequest.mode = 0;

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

        if ( !ypaworld->LoadGame( fmt::sprintf("save:%s/%d.sgm", userdata.UserName, 0) ) )
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
        loadRequest.mode = 0;

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
    else if ( userdata.envAction.action == EnvAction::ACTION_REPLAY )
    {
        std::string repname = PickReplayFile(world_update_arg.TimeStamp);
        if ( repname.empty() )
        {
            userdata.lastInputEvent = world_update_arg.TimeStamp;
            return 1;
        }

        if ( !userdata.SaveBuildProtoState() )
            return 0;

        tmpSettingsSaved = 1;

        ypaworld->CloseGameShell();

        GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

        if ( ypaworld->StartReplay(repname) )
        {
            replayStarted = 1;
            GameScreenMode = GAME_SCREEN_MODE_REPLAY;
        }
        else
        {
            ypa_log_out("Sorry, unable to init player!\n");
            world_update_arg.TimeStamp = 0;
            userdata.lastInputEvent = 0;

            if ( !ypaworld->OpenGameShell() )
            {
                ypa_log_out("GameShell-Error!!!\n");
                ypaworld->DeinitGameShell();
                return 0;
            }

            GameScreenMode = GAME_SCREEN_MODE_MENU;
        }

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
    else if ( GameScreenMode == GAME_SCREEN_MODE_REPLAY )
    {
        return ProcessReplayFrame();
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

    audio_inited = SFXEngine::SFXe.init();
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

    // The benchmark goes straight to its level
    return ypaworld->LoadSettings(settingsFileName,
                                  userdata.UserName,
                                  World::SDF_ALL,
                                  true, !benchmark.IsActive()) != 0;
}

void ReadSnapsDir()
{
    FSMgr::DirIter dir = uaOpenDir("env:snaps/");

    if ( dir )
    {
        FSMgr::iNode *entr;
        while ( dir.getNext(&entr) )
        {
            if ( entr->getType() == FSMgr::iNode::NTYPE_FILE && !StriCmp(entr->getName().substr(0, 4), "demo") )
                userdata.snaps.push_back( fmt::sprintf("env:snaps/%s", entr->getName()) );
        }
    }
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
    ReadSnapsDir();

    return 1;
}

void HandleMods()
{
    std::string modname;
    
    std::vector<std::string> &cmdl = System::GetCmdLineArray();
    int32_t i = System::FindCmdLineArg("-mod");
    if (i >= 0 && i + 1 < (int32_t)cmdl.size())
        modname = cmdl[i + 1];
    
    if (modname.empty())
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
        std::vector<std::string> &cmdl = System::GetCmdLineArray();
        int32_t i = System::FindCmdLineArg("-env");
        if (i >= 0 && i + 1 < (int32_t)cmdl.size())
            Common::Env.SetPrefix("env", cmdl[i + 1]);

        if ( InitWorldAndShell() )
            return 1;
        deinit_globl_engines();
    }

    return 0;
}

uint32_t maxTicks = 1000/60; // init on 60FPS

// Leaves the menus for the benchmark's level, started as new. A level that
// is not completed saves nothing to the player's profile; only the loading
// screen's tip of the day moves on, as with any level started.
static bool StartBenchmarkLevel()
{
    // Fullscreen: the display's resolution every time, and frame times that
    // vary much less than in a window
    ypaworld->SetWindowed(false);

    ypaworld->CloseGameShell();
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

    // The same level whoever's profile it is
    ypaworld->_levelInfo.Buddies.clear();

    TLevelLoadRequest loadRequest;
    loadRequest.lvlID = benchmark.GetLevel();
    loadRequest.mode = 0;

    if ( !ypaworld->CreateLevel(&loadRequest) )
    {
        ypa_log_out("Benchmark: could not start level %d\n", loadRequest.lvlID);
        return false;
    }

    GameScreenMode = GAME_SCREEN_MODE_GAME;
    Input::Engine.QueryInput(&input_states);
    return true;
}

int main(int argc, char *argv[])
{
    for(int i = 0; i < argc; ++i)
        System::AddCmdLine( std::string(argv[i]) );
    
    System::IniConf::Init();
    FSMgr::iDir::setBaseDir("");
    
    HandleMods();
    
    System::IniConf::ReadFromNucleusIni();
    
    if (System::IniConf::DevMode.Get<bool>() || System::FindCmdLineArg("-devmode") >= 0)
        World::AllowMods(true);        

    benchmark.ReadCommandLine();
    
    System::Init();
    
    GFX::Engine.Init();
    System::Movie.Init();

    if ( !InitGame() )
        return 0;

    System::IniConf::ReadFromNucleusIni();

    if ( benchmark.IsActive() && !StartBenchmarkLevel() )
    {
        ShutdownGame();
        GFX::Engine.Deinit();
        System::Deinit();
        return 1;
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
