#define GLOBAL
// Gives Windows GUI programs their WinMain entry point, as SDL2main did.
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <cstdlib>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
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
#include "net/viewserver.h"
#include "net/viewclient.h"
#include "obj3d.h"


int tmpSettingsSaved = 0;
std::string buildDate;

// Where a level started from the command line is recorded when --record
// doesn't say, once it is (TemporaryRecordingPath)
static std::string temporaryRecording;

int tform_inited = 0;
int audio_inited = 0;
int input_inited = 0;

enum GAME_SCREEN_MODE {
    GAME_SCREEN_MODE_UNKNOWN = 0,
    GAME_SCREEN_MODE_MENU = 1,
    GAME_SCREEN_MODE_GAME = 2,
    GAME_SCREEN_MODE_REPLAY = 3 // The replay viewer
};

GAME_SCREEN_MODE GameScreenMode;
UserData userdata;

static Benchmark benchmark; // --bench

// --headless: nothing shown, heard or read from the controls, and the
// level run as fast as the machine can. --time-limit in timer ticks, 0 for
// none.
static bool headless = false;
static bool aiOnly = false; // --ai-only
static int32_t headlessTimeLimit = 0;

static std::string recordFile; // --record
static bool recordFull = false; // --record-full
static uint32_t checkResumeStep = 0; // --check-resume

// --stream: others watch the level as it's played, a step at a time
// (Net::ViewServer); streamedStep is the recorder's last step sent
static bool streamLevel = false;
static Net::ViewServer viewServer;
static uint32_t streamedStep = 0;

// --watch: a game played elsewhere, watched in the replay viewer or, with
// --headless, only read
static std::string watchAddress;
static std::string watchName = "watcher";

// After a step: what it changed to those watching (--stream)
static void StreamStep()
{
    if ( !viewServer.IsOpen() || ypaworld->_recorder.Step() == streamedStep || !ypaworld->_recorder.IsRecording() )
        return;

    streamedStep = ypaworld->_recorder.Step();
    viewServer.Step(ypaworld->_levelInfo.LevelID, ypaworld->_levelDescription, ypaworld->_recorder);
}

static bool StartDirectLevel();

// The level the command line starts the game in: --level, --load or the
// benchmark's, or the replay it shows (--replay). Nothing of it is written to the player's profile.
struct DirectStart
{
    int Level = 0;        // Started as new
    std::string SaveFile; // Or from this saved game or recording
    bool HasStep = false; // --step: from after this step of the recording
    uint32_t Step = 0;
    std::string ReplayFile; // --replay: this recording in the replay viewer
};

static DirectStart directStart;

// The network game the command line hosts or joins (--host, --join); the
// lobby on from there is the menus' own
struct NetStart
{
    int HostLevel = 0;
    std::string JoinAddress;
    std::string Callsign;

    bool Active() const { return HostLevel || !JoinAddress.empty(); }
};

static NetStart netStart;

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
// A level that could not be loaded or started again: back to the level map,
// saying so. The level is gone, but loading it may have read some of the
// campaign's state: the profile, saved before, has it as it was. Returns
// what the frame functions do: 0 to quit, when even the menus can't open.
static int BackToLevelMap(bool continuing)
{
    ypaworld->_levelInfo.State = TLevelInfo::STATE_MENU;
    userdata.IgnoreScoreSaving = true;
    tmpSettingsSaved = 0;

    if ( !ypaworld->LoadSettings(fmt::sprintf("%s/user.txt", userdata.UserName), userdata.UserName,
                                 World::SDF_USER | World::SDF_SCORE | World::SDF_BUDDY | World::SDF_PROTO, false) ||
         !ypaworld->OpenGameShell() )
    {
        ypaworld->DeinitGameShell();
        return 0;
    }

    // A level left unfinished that can't be loaded isn't offered again
    if ( continuing )
        userdata.continueLevel = 0;

    userdata.ShowConfirmDialog(4, Locale::Text::Advanced(Locale::ADV_CANTLOAD), Locale::Text::Advanced(Locale::ADV_CANTLOADWHY), 1);

    GameScreenMode = GAME_SCREEN_MODE_MENU;
    world_update_arg.TimeStamp = 0;
    Input::Engine.QueryInput(&input_states);
    return 1;
}

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
    else if ( ypaworld->IsRewinding() )
    {
        // Going back through the last seconds as they were drawn: the game
        // doesn't step, and the controls wait. A frame that took longer than
        // the steps can catch up with, as the first drawing the level back
        // does, goes back as far as a step's time.
        input_states = TInputState();
        Input::Engine.QueryEvents(&input_states);

        if ( frameTicks > SimMaxStepsPerFrame * World::CVStepTicks )
            frameTicks = World::CVStepTicks;

        ypaworld->PlayRewind((double)frameTicks / World::CVStepTicks);

        // Back there, the next step comes at once, to draw on from there
        simPendingTicks = ypaworld->IsRewinding() ? 0 : World::CVStepTicks;
    }
    else
    {
        int32_t stepTicks = World::CVStepTicks;

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

            if ( simPendingTicks > SimMaxStepsPerFrame * World::CVStepTicks )
                simPendingTicks = SimMaxStepsPerFrame * World::CVStepTicks;

            // A level is not drawn before its first step, nor before a step
            // has placed the camera again after it was restored
            if ( ypaworld->_framesElapsed == 0 )
                simPendingTicks = World::CVStepTicks;
            else if ( !ypaworld->ReadyToDraw() && simPendingTicks < World::CVStepTicks )
                simPendingTicks = World::CVStepTicks;
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
            StreamStep();

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
        {
            ypa_log_out("Warning, could not start the level again\n");
            return BackToLevelMap(false);
        }

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
        {
            ypa_log_out("Warning, load error\n");
            return BackToLevelMap(false);
        }

        Input::Engine.QueryInput(&input_states);
    }
    break;

    case TLevelInfo::STATE_REWIND:
        // Going back starts with the next frame
        ypaworld->RewindLevel();
        break;

    default:
        break;
    }

    return 1;
}

// Back from the replay viewer to where it was started from: the debriefing,
// shown as it ends, or the replays page. The campaign's state that setting
// the replay's level up read over is read back, as after a level. Saying so
// when the replay couldn't be watched. Returns what the frame functions do.
static int BackFromReplay(bool failed)
{
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

    if ( !failed )
        ypaworld->EndReplay();

    if ( !userdata.DirectStart &&
         !ypaworld->LoadSettings("settings.tmp", userdata.UserName, World::SDF_BUDDY | World::SDF_PROTO | World::SDF_USER, false) )
        return 0;

    if ( !ypaworld->OpenGameShell() )
    {
        ypaworld->DeinitGameShell();
        return 0;
    }

    if ( userdata.replayFromDebriefing && !ypaworld->ResumeDebriefing() )
        ypa_log_out("Could not show the debriefing again\n");

    if ( failed )
        userdata.ShowConfirmDialog(4, Locale::Text::Advanced(Locale::ADV_CANTOPENREPLAY), "", 1);

    GameScreenMode = GAME_SCREEN_MODE_MENU;
    world_update_arg.TimeStamp = 0;
    Input::Engine.QueryInput(&input_states);
    return 1;
}

// Leaves the menus for the replay viewer (EnvAction::ACTION_VIEW_REPLAY)
static int StartReplayViewer()
{
    if ( !userdata.DirectStart )
    {
        userdata.SaveSettings();

        if ( !userdata.SaveBuildProtoState() )
            return 0;
    }

    if ( userdata.replayFromDebriefing )
        ypaworld->KeepDebriefing();

    ypaworld->CloseGameShell();
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

    if ( !ypaworld->StartReplay(userdata.replayToView) )
    {
        ypa_log_out("Could not watch the replay %s\n", userdata.replayToView.c_str());
        return BackFromReplay(true);
    }

    GameScreenMode = GAME_SCREEN_MODE_REPLAY;
    Input::Engine.QueryInput(&input_states);
    return 1;
}

// Played on by the real time alone, as the game stepped
static int ProcessReplayFrame(int32_t frameTicks)
{
    world_update_arg.DTime = frameTicks;
    ypaworld->ReplayFrame(&world_update_arg);

    // Started from the command line (--replay), the game quits
    if ( ypaworld->LeavingReplay() )
        return (directStart.ReplayFile.empty() && watchAddress.empty()) ? BackFromReplay(false) : 0;

    return 1;
}

// --replay: the recording in the replay viewer, at --step's step
static bool StartDirectReplay()
{
    ypaworld->CloseGameShell();
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

    if ( !ypaworld->StartReplay(directStart.ReplayFile, directStart.HasStep ? directStart.Step : 0) )
    {
        ypa_log_out("Could not watch the replay the command line names\n");
        return false;
    }

    GameScreenMode = GAME_SCREEN_MODE_REPLAY;
    Input::Engine.QueryInput(&input_states);
    return true;
}

// --watch: the game played at the address in the replay viewer, live
static bool StartDirectWatch()
{
    ypaworld->CloseGameShell();
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

    std::unique_ptr<Net::ViewClient> client(new Net::ViewClient);

    if ( !client->Join(watchAddress, watchName, 10000) )
    {
        ypa_log_out("Could not watch the game at %s\n", watchAddress.c_str());
        return false;
    }

    int level = client->Level();
    std::string description = client->LevelDescription();

    if ( !ypaworld->StartWatching(std::move(client), level, description) )
        return false;

    GameScreenMode = GAME_SCREEN_MODE_REPLAY;
    Input::Engine.QueryInput(&input_states);
    return true;
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
    else if ( userdata.envAction.action == EnvAction::ACTION_VIEW_REPLAY )
        return StartReplayViewer();
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
            return BackToLevelMap(false);
        }
        GameScreenMode = GAME_SCREEN_MODE_GAME;
        Input::Engine.QueryInput(&input_states);
    }
    else if ( userdata.envAction.action == EnvAction::ACTION_LOAD || userdata.envAction.action == EnvAction::ACTION_CONTINUE )
    {
        GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

        const TLevelInfo &levelInfo = ypaworld->GetLevelInfo();

        // The saved game, or the level left unfinished, whose recording
        // goes on
        bool continuing = userdata.envAction.action == EnvAction::ACTION_CONTINUE;
        std::string file = continuing ? UserData::SessionPath(userdata.UserName) : UserData::SaveGamePath(userdata.UserName);

        userdata.SaveSettings();
        ypaworld->CloseGameShell();

        if ( !userdata.SaveBuildProtoState() )
            return 0;

        if ( !ypaworld->LoadGame(file) )
        {
            ypa_log_out("Error while loading level (level %d, User %s)\n", levelInfo.LevelID, userdata.UserName.c_str());
            return BackToLevelMap(continuing);
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

    // Those watching come and go between steps too
    viewServer.Service();

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
        return ProcessReplayFrame(frameTicks);
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

    // Straight into a level or a network game from the command line: no
    // intro
    return ypaworld->LoadSettings(settingsFileName,
                                  userdata.UserName,
                                  World::SDF_ALL,
                                  true, !userdata.DirectStart && !netStart.Active()) != 0;
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
        else if ( GameScreenMode == GAME_SCREEN_MODE_REPLAY )
        {
            ypaworld->EndReplay();
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

    // Recorded only for the debriefing, which is over
    if ( !temporaryRecording.empty() )
        std::remove(temporaryRecording.c_str());

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


// Started from a recording: the keyframe it went on from, for a recording
// that goes on from it
static bool resumedFromRecording = false;
static uint32_t resumedStep = 0;
static World::Recording resumedRecording;

// False, after saying why, when --host, --join or --callsign don't go
// together with the rest
static bool ReadNetStart()
{
    std::string value;

    if ( System::CmdLine::Get("host", &value) )
    {
        char *end = NULL;
        long level = std::strtol(value.c_str(), &end, 10);

        if ( value.empty() || *end || level < 1 || level > 255 )
        {
            System::CmdLine::PrintError("--host " + value + ": not a level number");
            return false;
        }

        netStart.HostLevel = level;
    }

    if ( System::CmdLine::Get("join", &netStart.JoinAddress) && netStart.JoinAddress.empty() )
    {
        System::CmdLine::PrintError("--join needs ADDRESS");
        return false;
    }

    if ( netStart.HostLevel && !netStart.JoinAddress.empty() )
    {
        System::CmdLine::PrintError("--host and --join: give one");
        return false;
    }

    if ( System::CmdLine::Get("callsign", &netStart.Callsign) && !netStart.Active() )
    {
        if ( watchAddress.empty() )
        {
            System::CmdLine::PrintError("--callsign needs --host, --join or --watch");
            return false;
        }

        watchName = netStart.Callsign;
    }

    if ( netStart.Active() && (userdata.DirectStart || System::CmdLine::Has("headless")) )
    {
        System::CmdLine::PrintError("--host and --join open the menus; they don't go with --level, "
                                    "--load, --replay, --bench or --headless");
        return false;
    }

    return true;
}

// Hosts or joins the network game the command line asks for. If that fails,
// the menus stay where it failed, after saying why.
static void StartNetGame()
{
    if ( netStart.HostLevel )
    {
        const auto &regions = ypaworld->_globalMapRegions.MapRegions;

        if ( netStart.HostLevel >= (int)regions.size() || regions[netStart.HostLevel].Status != TMapRegionInfo::STATUS_NETWORK )
        {
            System::CmdLine::PrintError(fmt::sprintf("level %d is not a network level", netStart.HostLevel));
            return;
        }
    }

    if ( !userdata.StartNetworkGame(netStart.HostLevel, netStart.JoinAddress, netStart.Callsign) )
    {
        if ( netStart.HostLevel )
            System::CmdLine::PrintError(fmt::sprintf("can't host a game on port %d", Net::DEFAULT_PORT));
        else
            System::CmdLine::PrintError("can't join the game at " + netStart.JoinAddress);
    }
}

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

        if ( directStart.SaveFile.empty() && !System::CmdLine::Has("replay") )
        {
            System::CmdLine::PrintError("--step needs --load or --replay of a recording");
            return false;
        }

        directStart.HasStep = true;
        directStart.Step = step;
    }

    if ( System::CmdLine::Get("replay", &directStart.ReplayFile) )
    {
        if ( directStart.ReplayFile.empty() )
        {
            System::CmdLine::PrintError("--replay needs FILE");
            return false;
        }

        ways++;
    }

    if ( System::CmdLine::Get("watch", &watchAddress) )
    {
        if ( watchAddress.empty() )
        {
            System::CmdLine::PrintError("--watch needs ADDRESS");
            return false;
        }

        ways++;
    }

    if ( benchmark.IsActive() )
    {
        directStart.Level = benchmark.GetLevel();
        ways++;
    }

    if ( ways > 1 )
    {
        System::CmdLine::PrintError("--level, --load, --replay, --watch and --bench each start a level; give one");
        return false;
    }

    userdata.DirectStart = (ways == 1);

    headless = System::CmdLine::Has("headless");

    if ( headless && (!userdata.DirectStart || benchmark.IsActive() || !directStart.ReplayFile.empty()) )
    {
        System::CmdLine::PrintError("--headless needs --level, --load or --watch");
        return false;
    }

    streamLevel = System::CmdLine::Has("stream");

    if ( streamLevel && !directStart.Level && directStart.SaveFile.empty() )
    {
        System::CmdLine::PrintError("--stream needs --level or --load");
        return false;
    }

    if ( !watchAddress.empty() && (System::CmdLine::Has("record") || System::CmdLine::Has("check-resume") ||
                                   System::CmdLine::Has("ai-only") || System::CmdLine::Has("step")) )
    {
        System::CmdLine::PrintError("--watch only goes with --headless, --time-limit and --callsign");
        return false;
    }

    aiOnly = System::CmdLine::Has("ai-only");

    if ( aiOnly && !headless )
    {
        System::CmdLine::PrintError("--ai-only needs --headless");
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

        if ( !userdata.DirectStart || benchmark.IsActive() || !directStart.ReplayFile.empty() )
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
// doesn't say: a file of this process's own, gone when the game quits, so
// that games started at once don't write each other's
static const std::string &TemporaryRecordingPath()
{
    if ( temporaryRecording.empty() )
    {
        const char *dir = getenv("TMPDIR");

        if ( !dir )
            dir = getenv("TEMP");

        if ( !dir )
            dir = getenv("TMP");

#if defined(_WIN32)
        int pid = _getpid();
#else
        int pid = getpid();
#endif

        temporaryRecording = fmt::sprintf("%s/ua_source_level_%d.rec", dir ? dir : "/tmp", pid);
    }

    return temporaryRecording;
}

// Whether the level, saved game or replay the command line names is there
static bool CheckDirectStart()
{
    // What's watched comes from the game watched
    if ( !watchAddress.empty() )
        return true;

    if ( !directStart.SaveFile.empty() || !directStart.ReplayFile.empty() )
    {
        const std::string &file = directStart.SaveFile.empty() ? directStart.ReplayFile : directStart.SaveFile;
        World::Recording recording;

        if ( !recording.Open(file) )
        {
            System::CmdLine::PrintError(file + " is no saved game or recording");
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

// Starts the level of a recording as it was at the keyframe --step names, or
// at its last
static bool StartFromRecording(const std::string &path)
{
    World::Recording &recording = resumedRecording;

    if ( !recording.Open(path) )
        return false;

    uint32_t step = directStart.HasStep ? directStart.Step : recording.Keyframes().back().Step;
    const World::KeyframeAt *at = recording.KeyframeAtOrBefore(step);

    if ( step > recording.LastStep() )
    {
        ypa_log_out("The recording %s ends at step %d, before %d\n", path.c_str(), recording.LastStep(), step);
        return false;
    }

    if ( !at || at->Step != step )
    {
        ypa_log_out("The level can't go on from step %d of %s, only from a keyframe, one every %d steps: the one before is %d\n",
                    step, path.c_str(), (int)World::Recorder::KeyframeSteps, at ? at->Step : 0);
        return false;
    }

    World::State::Image view, internal;

    if ( !recording.ReadKeyframe(step, view, internal) ||
         !World::State::Restore(ypaworld, ypaworld->RecordedLevel(recording), view, internal, true) )
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

    if ( aiOnly )
        ypaworld->LeaveFaction(ypaworld->LocalPlayer());

    // What the debriefing compares the end of the level with, as when the
    // level map starts a level. A recording's level has its start's.
    if ( !resumedFromRecording )
    {
        ypaworld->_lvlPrimevalTypeMap = ypaworld->_lvlTypeMap;
        ypaworld->_lvlPrimevalOwnMap = ypaworld->_lvlOwnMap;
    }

    // In a window, the debriefing after a win has the level's events from
    // its recording: unless --record names one, it goes to a temporary file,
    // not the profile. A recording's level goes on with it.
    if ( !headless && recordFile.empty() )
    {
        bool recording = resumedFromRecording ?
                         ypaworld->_recorder.ContinueFrom(ypaworld, TemporaryRecordingPath(), resumedRecording, resumedStep) :
                         ypaworld->_recorder.Start(ypaworld, TemporaryRecordingPath(), "the command line");

        if ( !recording )
            ypa_log_out("The level isn't recorded, so its debriefing has no events\n");
    }

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

    // Nobody is there to leave the level, so these end it: a faction is
    // out when it has no units left
    uint32_t factionsLeft = ypaworld->FactionsWithUnits();
    int mine = ypaworld->LocalOwner();

    if ( mine > 0 )
    {
        if ( !(factionsLeft & (1u << mine)) )
            return "lost, the player's faction has no units left";

        if ( !(factionsLeft & ~(1u << mine)) )
            return "won, no enemy faction has units left";
    }

    // With no one playing, it's over when one faction is all that's left
    if ( mine <= 0 && !factionsLeft )
        return "over, no faction has units left";

    if ( mine <= 0 && !(factionsLeft & (factionsLeft - 1)) )
    {
        int faction = 0;
        while ( factionsLeft > 1u << faction )
            faction++;

        return fmt::sprintf("over, only faction %d has units left", faction);
    }

    if ( headlessTimeLimit && ypaworld->_timeStamp - startTime >= headlessTimeLimit )
        return "stopped at the time limit";

    return std::string();
}

// One headless step, with an empty input
static void HeadlessStep()
{
    input_states = TInputState();
    input_states.Period = World::CVStepTicks;

    world_update_arg.Input = &input_states;
    world_update_arg.DTime = World::CVStepTicks;
    world_update_arg.TimeStamp += World::CVStepTicks;

    ypaworld->SimulationStep(&world_update_arg);
    StreamStep();
}

// --check-resume: keeps the state, plays on, starts again from what was kept
// and plays the same steps again; each step's state must come out the same.
// Returns the report.
static std::string CheckResume(int32_t startTime, uint32_t step)
{
    const uint32_t CheckSteps = 600;
    const int level = ypaworld->GetLevelInfo().LevelID;
    const std::string description = ypaworld->_levelDescription;

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

    if ( !World::State::Restore(ypaworld, description, kept) )
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
        // Watched, it goes at the game's own speed, keeping in touch
        // between steps
        if ( viewServer.IsOpen() )
        {
            uint64_t due = startTicks + (uint64_t)steps * World::CVStepTicks * 1000000000ull / 1024;

            while ( SDL_GetTicksNS() < due )
            {
                viewServer.Service();
                SDL_DelayNS(1000000);
            }
        }

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

// --watch with --headless: the game watched read as it comes, nothing shown;
// every 600 steps a hash of the view then, to compare with the game's, and
// at the end what came and how fast. --time-limit stops it after that much
// game time.
static void RunHeadlessWatch()
{
    Net::ViewClient client;
    client.KeepViewHashes(600);

    if ( !client.Join(watchAddress, watchName, 10000) )
    {
        printf("Could not watch the game at %s\n", watchAddress.c_str());
        return;
    }

    const uint32_t firstStep = client.LastStep();
    const uint64_t startTicks = SDL_GetTicksNS();
    size_t hashesShown = 0;

    // The longest wait for the next step, as when packets were lost
    uint32_t lastStep = firstStep;
    uint64_t lastStepTicks = startTicks, longestWait = 0;

    printf("Watching level %d from step %u\n", client.Level(), firstStep);
    fflush(stdout);

    while ( client.Update() )
    {
        if ( client.LastStep() != lastStep )
        {
            longestWait = std::max(longestWait, SDL_GetTicksNS() - lastStepTicks);
            lastStep = client.LastStep();
            lastStepTicks = SDL_GetTicksNS();
        }

        for ( ; hashesShown < client.ViewHashes().size(); hashesShown++ )
            printf("step %u view %016llx\n", client.ViewHashes()[hashesShown].first,
                   (unsigned long long)client.ViewHashes()[hashesShown].second);

        fflush(stdout);

        if ( headlessTimeLimit && (int64_t)(client.LastStep() - firstStep) * World::CVStepTicks >= headlessTimeLimit )
            break;

        SDL_DelayNS(1000000);
    }

    double seconds = (SDL_GetTicksNS() - startTicks) / 1e9;
    printf("Watched %u steps in %.1f s: %llu bytes, %.1f KB/s (%.1f KB/s unpacked), the longest wait for a step %.0f ms%s\n",
           client.StepsReceived(), seconds, (unsigned long long)client.BytesReceived(),
           seconds > 0 ? client.BytesReceived() / 1024.0 / seconds : 0.0, seconds > 0 ? client.BytesUnpacked() / 1024.0 / seconds : 0.0,
           longestWait / 1e6, client.Ended() ? ", the game ended" : "");
    fflush(stdout);
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

    if ( !ReadDirectStart() || !ReadNetStart() )
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

        bool started = false;

        if ( CheckDirectStart() )
        {
            if ( !watchAddress.empty() )
                started = headless || StartDirectWatch();
            else
                started = directStart.ReplayFile.empty() ? StartDirectLevel() : StartDirectReplay();
        }

        if ( !started )
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
            bool recording;

            ypaworld->_recorder.SetFull(recordFull);

            if ( resumedFromRecording )
                recording = ypaworld->_recorder.ContinueFrom(ypaworld, recordFile, resumedRecording, resumedStep);
            else
                recording = ypaworld->_recorder.Start(ypaworld, recordFile, fmt::sprintf("level %d", directStart.Level));

            if ( !recording )
            {
                System::CmdLine::PrintError("can't write " + recordFile);
                ShutdownGame();
                GFX::Engine.Deinit();
                System::Deinit();
                return 1;
            }
        }

        // Others watching: what they get is what's recorded
        if ( streamLevel )
        {
            if ( !ypaworld->_recorder.IsRecording() )
                ypaworld->_recorder.Start(ypaworld, TemporaryRecordingPath(), "the command line");

            if ( !viewServer.Open(Net::DEFAULT_PORT, fmt::sprintf("level %d", ypaworld->_levelInfo.LevelID)) )
            {
                System::CmdLine::PrintError(fmt::sprintf("can't let others watch on port %d", (int)Net::DEFAULT_PORT));
                ShutdownGame();
                GFX::Engine.Deinit();
                System::Deinit();
                return 1;
            }

            if ( headless )
            {
                printf("Watchable on port %d\n", (int)Net::DEFAULT_PORT);
                fflush(stdout);
            }
        }

        if ( headless )
        {
            if ( watchAddress.empty() )
                RunHeadless();
            else
                RunHeadlessWatch();

            viewServer.Close();
            ShutdownGame();
            GFX::Engine.Deinit();
            System::Deinit();
            return 0;
        }
    }

    if ( netStart.Active() )
        StartNetGame();

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
    
    viewServer.Close();
    ShutdownGame();
    
    GFX::Engine.Deinit();
    System::Deinit();

    return 0;
}
