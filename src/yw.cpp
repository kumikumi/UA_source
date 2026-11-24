#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <string>
#include "includes.h"
#include "yw.h"

#include "yw_internal.h"
#include "loaders.h"

#include "button.h"
#include "font.h"
#include "yparobo.h"
#include "windp.h"
#include "yw_net.h"
#include "env.h"
#include "system/inivals.h"
#include "system/perf.h"
#include "locale/locale.h"

extern yw_infolog info_log;

int vertMenuSpace;
int videoMenuRowWidth;
int menuListWidth;
int buttonsSpace;
int checkBoxWidth;

int netMenuWidth;
int subBarButtonWidth;

int scaledFontHeight;
int bottomButtonsY;
int button1LineWidth;
int bottomSecondBtnPosX;
int bottomThirdBtnPosX;

GuiList briefingTextList;

uint32_t bact_id = 0x10000;

// method 169
uint32_t maxLoadedCommandID;

NC_STACK_ypaworld::NC_STACK_ypaworld()
: _unitsList(this, NC_STACK_ypabact::GetKidRefNode, World::BLIST_UNITS)
, _deadCacheList(this, NC_STACK_ypabact::GetKidRefNode, World::BLIST_CACHE)
, _history(4096)
{
}

namespace World
{
int AssignParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
	if ( !StriCmp(p1, "end") )
	{
		return ScriptParser::RESULT_SCOPE_END;
	}
	else if ( !p1.empty() && !p2.empty() )
	{
		Common::Env.SetPrefix(p1, p2);
		ypa_log_out("parsing assign.txt: set assign %s to %s\n", p1.c_str(), p2.c_str());
		return ScriptParser::RESULT_OK;
	}
	else
	{
		return ScriptParser::RESULT_BAD_DATA;
	}

    return ScriptParser::RESULT_UNKNOWN;
}

bool ParseAssignFile(const std::string &file)
{
	ScriptParser::HandlersList hndls{
		AssignParser::MakeParser()
	};

	return ScriptParser::ParseFile(file, hndls, 0);
}

}


bool NC_STACK_ypaworld::LoadProtosScript(const std::string &filename)
{
    std::string buf = Common::Env.SetPrefix("rsrc", "data:");

    ScriptParser::HandlersList parsers {
        new World::Parsers::VhclProtoParser(this),
        new World::Parsers::WeaponProtoParser(this),
        new World::Parsers::BuildProtoParser(this)
    };

    bool res = ScriptParser::ParseFile(filename, parsers, ScriptParser::FLAG_NO_SCOPE_SKIP);
    Common::Env.SetPrefix("rsrc", buf);

    return res;
}

bool NC_STACK_ypaworld::ProtosInit()
{
    _vhclProtos.resize(NUM_VHCL_PROTO);
    _weaponProtos.resize(NUM_WEAPON_PROTO);
    _buildProtos.resize(NUM_BUILD_PROTO);
    
    _roboProtos.reserve(NUM_ROBO_PROTO);
    _roboProtos.resize(1);    

    if ( !LoadProtosScript(_initScriptFilePath) )
        return false;

    return true;
}

int yw_InitSceneRecorder(NC_STACK_ypaworld *yw)
{
    yw->_replayRecorder = new TGameRecorder();
    return yw->_replayRecorder != NULL;
}

void yw_setInitScriptLoc(NC_STACK_ypaworld *yw)
{
    bool ok = false;
    FSMgr::FileHandle *fil = uaOpenFileAlloc("env:startup.def", "r");

    if (fil)
    {
        std::string line;
        if ( fil->ReadLine(&line) )
        {
            size_t en = line.find_first_of("\n;");
            if (en != std::string::npos)
                line.erase(en);

            yw->_initScriptFilePath = line;
            ok = true;
        }


        delete fil;
    }

    if (!ok)
        yw->_initScriptFilePath = "data:scripts/startup.scr";
}

size_t NC_STACK_ypaworld::Init(IDVList &stak)
{
    if ( !NC_STACK_nucleus::Init(stak) )
    {
        ypa_log_out("yw_main.c/OM_NEW: _supermethoda() failed!\n");
        return 0;
    }

    Common::Env.SetPrefix("rsrc", "mc2res");
    Common::Env.SetPrefix("data", "Data");
    Common::Env.SetPrefix("save", "Save");
    Common::Env.SetPrefix("help", "Help");
    Common::Env.SetPrefix("mov", "Data:mov");
    Common::Env.SetPrefix("levels", "Levels");
    Common::Env.SetPrefix("mbpix", "levels:mbpix");
    Common::Env.SetPrefix("locale", "locale");
    Common::Env.SetPrefix("scripts", "data:scripts");

    if ( !World::ParseAssignFile("env:assign.txt") )
        ypa_log_out("Warning, no env:assign.txt script.\n");

    yw_setInitScriptLoc(this);

    Locale::Text::SetLangDefault();
    LoadLanguage("language"); // Load lang strings

//		if ( !make_CD_CHECK(1, 1, v10, v9) )
//		{
//			func1();
//			return NULL;
//		}

    _vhclModels.clear();
    ClearOverrideModels();

    if ( !ProtosInit() )
    {
        ypa_log_out("ERROR: couldn't initialize prototypes.\n");
        Deinit();
        return 0;
    }

    _screenSize = GFX::Engine.GetScreenSize();

    _unitsList.clear();
    _guiActive.clear();
    _deadCacheList.clear();


    _fxLimit = 16;
    _skyVizLimit = stak.Get<int32_t>(YW_ATT_SKYVISLIMIT, 4200);
    _skyFadeLength = stak.Get<int32_t>(YW_ATT_SKYFADELENGTH, 1100);
    _mapSize.x = stak.Get<int32_t>(YW_ATT_MAPMAX_X, 64);
    _mapSize.y = stak.Get<int32_t>(YW_ATT_MAPMAX_Y, 64);
    _skyHeight = stak.Get<int32_t>(YW_ATT_SKYHEIGHT, -550);
    _skyRender = stak.Get<bool>(YW_ATT_SKYRENDER, true);
    _doEnergyRecalc = stak.Get<bool>(YW_ATT_DOENERGYRECALC, true);

    _buildDate = stak.Get<std::string>(YW_ATT_BUILD_DATE, "");
    
    for (char &chr : _buildDate)
    {
        chr = toupper(chr);
        if ( chr < ' ' || chr > 'Z' )
            chr = '*';
    }



    _buildHealthModelId[0] = 3;
    for (int i = 1; i <= 100; i++ )
        _buildHealthModelId[i] = 2;
    for (int i = 101; i <= 200; i++ )
        _buildHealthModelId[i] = 1;
    for (int i = 201; i < 256; i++ )
        _buildHealthModelId[i] = 0;

    for (int j = 0; j < 64; j++)
    {
        for (int i = 0; i < 64; i++)
        {
            _sqrtTable(j,i) = sqrt(j * j + i * i);
        }
    }

    _cells.Clear();
    
    if ( !yw_InitSceneRecorder(this) )
    {
        ypa_log_out("yw_main.c/OM_NEW: init scene recorder failed!\n");
        Deinit();
        return 0;
    }

    if ( !InitMapRegionsNet() )
    {
        ypa_log_out("yw_main.c/OM_NEW: yw_InitLevelNet() failed!\n");
        Deinit();
        return 0;
    }

    if ( !yw_InitNetwork(this) )
    {
        ypa_log_out("yw_main.c/OM_NEW: yw_InitNetwork() failed!\n");
        Deinit();
        return 0;
    }

    _doNotRender = false;

    return 1;
}


size_t NC_STACK_ypaworld::Deinit()
{
    GFX::Engine.SetCursor(0, 0);
    dprintf("MAKE ME %s\n","NC_STACK_ypaworld::Deinit");
    return NC_STACK_nucleus::Deinit();
}

void UpdateViewerPosition(NC_STACK_ypaworld *yw)
{
    if ( yw->_viewerBact->getBACT_extraViewer() )
    {
        NC_STACK_ypabact *viewer = yw->_viewerBact;

        yw->_viewerPosition = viewer->_position + viewer->_rotation.Transpose().Transform(viewer->_viewer_position);

        yw->_viewerRotation = yw->_viewerBact->_viewer_rotation;
    }
    else
    {
        yw->_viewerPosition = yw->_viewerBact->_position;

        yw->_viewerRotation = yw->_viewerBact->_rotation;
    }
}

// While the game is paused: draws the paused screen and returns true, or
// returns false when a key has ended the pause.
bool NC_STACK_ypaworld::PausedFrame(TUpdateArgs *arg)
{
    return HandlePause(this, arg) != 0;
}

// One step of the game: input, interface commands, network and units,
// arg->DTime timer ticks long. Returns false when the frame should not be
// drawn: while paused, or after a network game was aborted.
bool NC_STACK_ypaworld::SimulationStep(TUpdateArgs *arg)
{
    extern GuiList gui_lstvw; //In yw_game_ui.cpp
    extern GuiList lstvw2; //In yw_game_ui.cpp
    extern bool SPEED_DOWN_NET; //In yw_net.cpp

    if ( (gui_lstvw.IsClosed() && lstvw2.IsClosed())
            || (arg->Input->KbdLastHit != Input::KC_RETURN && arg->Input->KbdLastHit != Input::KC_ESCAPE) )
    {
        _kbdLastKeyHit = Input::KC_NONE;
    }
    else
    {
        _kbdLastKeyHit = arg->Input->KbdLastHit;
        arg->Input->KbdLastHit = Input::KC_NONE;
        arg->Input->KbdLastDown = Input::KC_NONE;
        arg->Input->HotKeyID = -1;
    }

    if ( HandlePause(this, arg) )
        return false;

    SaveUnitTransforms();

    uint32_t frameStart = profiler_begin();

    _netChatSystem = false;

    if ( _userUnit )
    {
        if ( _framesElapsed == 1 )
        {
            _viewerPosition = _userUnit->_position;
            _viewerRotation = _userUnit->_rotation;
        }

        vec3d velocity = _userUnit->_fly_dir * _userUnit->_fly_dir_length;

        SFXEngine::SFXe.BeginSoundFrame(arg->DTime, _viewerPosition, velocity, _viewerRotation);
    }

    if ( _framesElapsed == 1 )
    {
        TInfoMessage startMsg;
        startMsg.unit = _userRobo;
        startMsg.Priority = 128;
        startMsg.MsgID = 41;

        ShowMessage(&startMsg);
    }

    UpdateReloadRatios(this);
    UpdatePlayerInRoboGun(this);

    // The interface sets the tooltip again as it handles this step's input;
    // frames drawn until the next step keep showing it.
    ClearTooltip();

    if ( !_doNotRender )
    {
        PrepareUserInput(arg->Input); //Precompute input (add mouse turn)

        TClickBoxInf *winp = &arg->Input->ClickInf;

        if ( _mouseCursorHidden )
        {
            if ( winp->move.ScreenPos != _mouseCursorHidePos )
                _mouseCursorHidden = false;
        }
        else
        {
            if ( arg->Input->KbdLastHit != Input::KC_NONE )
            {
                if ( arg->Input->KbdLastHit != Input::KC_LMB && arg->Input->KbdLastHit != Input::KC_RMB && arg->Input->KbdLastHit != Input::KC_MMB && !(arg->Input->Buttons.Is(4)) )
                {
                    _mouseCursorHidden = true;
                    _mouseCursorHidePos = winp->move.ScreenPos;
                }
            }
        }
    }

    _timeStamp += arg->DTime;
    _frameTime = arg->DTime;
    _framesElapsed++;

    _updateMessage.user_action = World::DOACTION_0;
    _updateMessage.gTime = _timeStamp;
    _updateMessage.frameTime = arg->DTime;
    _updateMessage.units_count = 0;
    _updateMessage.inpt = arg->Input;

    HistoryEventAdd(World::History::Frame(_timeStamp));

    uint32_t netStart = profiler_begin();
    
    if ( _isNetGame )
        yw_NetMsgHndlLoop(this);

    _profileVals[PFID_NETTIME] = profiler_end(netStart);

    if ( _isNetGame && _levelInfo.State == TLevelInfo::STATE_ABORTED )
        return false;

    uint32_t markStart = profiler_begin();

    for (cellArea &cell : _cells)
    {
        cell.view_mask = cellArea::ViewMask(cell.owner);
        cell.UnhideMask = 0;
    }

    for (NC_STACK_ypabact* &unit : _unitsList)
        unit->MarkSectorsForView();

    _profileVals[PFID_MARKTIME] = profiler_end(markStart);

    UpdateGunList(this);
    UpdateVehicleList(this);
    UpdateBuildingList(this);
    UpdateCommanderList();
    UpdateGates(this);
    UpdateSuperItems();
    BuildingConstructUpdate(arg->DTime);

    if ( !_doNotRender )
    {
        uint32_t guiStart = profiler_begin();

        RoboMapFollowViewer(this);
        HandleGuiInput(arg->Input);

        UpdateViewerPosition(this);

        RoboMapUpdateLayout(this);
        HandleWorldMouse(arg->Input);

        _profileVals[PFID_GUITIME] = profiler_end(guiStart);
    }

    if ( _doEnergyRecalc )
        DoSectorsEnergyRecalc();

    if ( _replayRecorder->do_record )
        recorder_update_time(this, arg->DTime);

    // The user's unit sets these again as it updates; frames drawn until the
    // next step show them as they are.
    _guiVisor.mgunSight = 0;
    _guiVisor.weaponSight = 0;
    _guiVisor.target = NULL;

    _ownerOldCellUserUnit = _userUnit->_pSector->owner;

    uint32_t updateStart = profiler_begin();
    uint64_t simStart = System::PerfStats::Now();
    
    // Do user commands before any unit state can be changed
    if (_userRobo)
    {
        if (_userRobo->_bact_type == BACT_TYPES_ROBO)
            ((NC_STACK_yparobo *)_userRobo)->HandleUserCommands(&_updateMessage);
    }

    for ( NC_STACK_ypabact *unit : _unitsList.safe_iter() )
    {
        if (_isNetGame && unit != _userRobo && unit->_bact_type == BACT_TYPES_ROBO)
            unit->NetUpdate(&_updateMessage);
        else
            unit->Update(&_updateMessage);

        _updateMessage.units_count++;
    }

    System::Perf.AddTime(System::PerfStats::SEC_SIM, System::PerfStats::Now() - simStart);
    _profileVals[PFID_UPDATETIME] = profiler_end(updateStart);

    UpdateViewerPosition(this);

    uint32_t flushStart = profiler_begin();

    if ( _isNetGame )
    {
        // TODO(multiplayer): Meant for frames with time frozen, but neither
        // the original (which added a tick to every frame) nor the fixed steps
        // ever give a step of 1 tick. While waiting for players, steps are 2
        // ticks, so the flush timer runs down slowly. Check how often the
        // original flushed then.
        if ( arg->DTime == 1 )
            _netFlushTimer -= 20;
        else
            _netFlushTimer -= arg->DTime;

        if ( _netFlushTimer <= 0 )
        {

            uint32_t sentSize = _netDriver->FlushBroadcastBuffer();

            _GameShell->netsend_count += sentSize;

            if ( !_GameShell->net_packet_min || sentSize < _GameShell->net_packet_min )
                _GameShell->net_packet_min = sentSize;

            if ( sentSize > _GameShell->net_packet_max )
                _GameShell->net_packet_max = sentSize;

            if ( sentSize )
                _GameShell->net_packet_cnt++;

            if ( SPEED_DOWN_NET )
                _netFlushTimer = 1500;
            else
                _netFlushTimer = _GameShell->flush_time_norm;
        }
    }
    _profileVals[PFID_NETTIME] += profiler_end(flushStart);

    if ( _userUnit )
    {
        if ( _GameShell )
        {
            _GameShell->samples1_info.Position = _userUnit->_position;
            SFXEngine::SFXe.UpdateSoundCarrier(&_GameShell->samples1_info);
        }
    }

    _tutorialHints.Update(this);

    if (_script)
        _script->CallUpdate(_timeStamp, arg->DTime);
                
    VoiceMessageUpdate(); // update sound messages

    const mat3x3 &shake = SFXEngine::SFXe.EndSoundFrame();
    TF::TForm3D *viewPoint = TF::Engine.GetViewPoint();

    viewPoint->SclRot = shake * viewPoint->SclRot;

    if ( _replayRecorder->do_record )
        recorder_write_frame();

    CheckEnemySectorEntered(this);

    FFeedback_Update(); // Do vibrate joystick

    _profileVals[PFID_FRAMETIME] = profiler_end(frameStart);

    if ( !_doNotRender )
    {
        if ( _netChatSystem )
        {
            _doNotRender = true;

            dprintf("MAKE ME %s (multiplayer part Messaging)\n", "SimulationStep");

//          v66 = get_lang_string(650, "MESSAGE TO");
//
//          sprintf(&v68, "%s %s", v66, yw->field_762E);
//
//          memset(&dlgBox, 0, 40);
//
//          dlgBox.title = &v68;
//          dlgBox.ok = get_lang_string(2, "OK");
//          dlgBox.cancel = get_lang_string(3, "CANCEL");
//          dlgBox.result = 0;
//          dlgBox.timer_context = yw;
//          dlgBox.maxLen = 64;
//          dlgBox.time = 250;
//          dlgBox.startText = "";
//          dlgBox.timer_func = sub_44674C;
//
//          yw->win3d->ShowTextInputDialog(&dlgBox);
//
            _doNotRender = false;
//
//          if ( dlgBox.result )
//            sub_4D9418(yw, dlgBox.result, yw->field_762E, yw->field_762A);
        }
    }

    if ( !_helpURL.empty() )
    {
        const std::string help_link = _helpURL;
        _helpURL.clear();

        OpenHelpURL(&help_link); //launch online help
    }

    return true;
}

// Units that move further than this in one step, as when beamed, are drawn
// where they ended up instead of in between.
static const double MaxInterpolatedMove = 300.0;

// Whether a transform changed too much in one step to draw it in between: it
// moved too far, or turned more than about 30 degrees, as the camera does
// when the player switches to another unit.
static bool IsStepJump(const vec3d &pos0, const mat3x3 &rot0, const vec3d &pos1, const mat3x3 &rot1)
{
    if ( (pos1 - pos0).length() > MaxInterpolatedMove )
        return true;

    return (rot1.AxisY() - rot0.AxisY()).length() > 0.5 ||
           (rot1.AxisZ() - rot0.AxisZ()).length() > 0.5;
}

// Remembers every unit's transform at the start of a simulation step, so it
// can be drawn between there and where the step leaves it.
void NC_STACK_ypaworld::SaveUnitTransforms()
{
    _simStep++;

    // The camera is a transform of its own, which the viewer unit fills in
    _prevView = TF::Engine.GetViewPoint();

    if ( _prevView )
    {
        _prevViewPos = _prevView->Pos;
        _prevViewSclRot = _prevView->SclRot;
    }

    for (cellArea &cell : _cells)
    {
        for (NC_STACK_ypabact *unit : cell.unitsList)
        {
            unit->_prevTFormPos = unit->_tForm.Pos;
            unit->_prevTFormSclRot = unit->_tForm.SclRot;
            unit->_prevStep = _simStep;
        }
    }
}

// Moves every unit and the camera stepFraction of the way through the last
// step, from where they started it towards where they ended it, until
// RestoreUnitTransforms.
// TODO(bug): HUD markers, map icons and the sky are placed from unit
// positions, not these interpolated transforms, so they can lag by up to
// one step.
void NC_STACK_ypaworld::InterpolateUnitTransforms(float stepFraction)
{
    _steppedTForms.clear();

    auto blend = [this, stepFraction](TF::TForm3D *tf, const vec3d &pos0, const mat3x3 &sclRot0)
    {
        if ( IsStepJump(pos0, sclRot0, tf->Pos, tf->SclRot) )
            return;

        _steppedTForms.push_back({tf, tf->Pos, tf->SclRot});

        // Blending the matrices keeps scale exact. Turns within one step are
        // small enough that the blend stays a rotation.
        tf->Pos = pos0 + (tf->Pos - pos0) * stepFraction;
        tf->SclRot = sclRot0 * (1.0 - stepFraction) + tf->SclRot * stepFraction;
    };

    for (cellArea &cell : _cells)
    {
        for (NC_STACK_ypabact *unit : cell.unitsList)
        {
            if ( unit->_prevStep == _simStep ) // Else it appeared during the step
                blend(&unit->_tForm, unit->_prevTFormPos, unit->_prevTFormSclRot);
        }
    }

    if ( _prevView && _prevView == TF::Engine.GetViewPoint() )
        blend(_prevView, _prevViewPos, _prevViewSclRot);
}

// Puts back the transforms of the last step, for the simulation to go on from.
void NC_STACK_ypaworld::RestoreUnitTransforms()
{
    for (const TSavedTForm &saved : _steppedTForms)
    {
        saved.TForm->Pos = saved.Pos;
        saved.TForm->SclRot = saved.SclRot;
    }

    _steppedTForms.clear();
}

// Draws the world stepFraction of the way through the last simulation step.
// arg->DTime is the time since the last frame, for what animates while
// drawing: particles and the interface.
void NC_STACK_ypaworld::RenderFrame(TUpdateArgs *arg, float stepFraction)
{
    if ( arg->DTime > 0 )
        _FPS = 1024 / arg->DTime;

    _profileVals[PFID_FPS] = _FPS;

    // The interface animates by _frameTime while it is drawn
    _frameTime = arg->DTime;

    if ( !_doNotRender )
    {
        uint32_t renderStart = profiler_begin();

        GFX::Engine.BeginFrame();

        InterpolateUnitTransforms(stepFraction);

        if ( _userUnit->_cellId ) // if cell is not 0,0
        {
            RenderGame(arg, 1);

            if ( _isNetGame )
                yw_NetDrawStats(this);
        }

        RestoreUnitTransforms();
        
        debug_info_draw(arg->Input);
        DrawPerfOverlay(arg->Input);

        GFX::Engine.EndFrame();

        _profileVals[PFID_RENDERTIME] = profiler_end(renderStart);
        _profileVals[PFID_FRAMETIME] += _profileVals[PFID_RENDERTIME];
    }

    HandleSnapshotKeys(this, arg->Input); // Snaps/ start/stop recording

    _profileVals[PFID_POLYGONS] = _polysDraw;

    ProfileCalcValues();
}

void GetNetGemProtoIds(NC_STACK_ypaworld *yw, TMapGem *gemProt, int *vhclProtoId, int *buildProtoId)
{
    switch ( yw->_GameShell->netPlayerOwner )
    {
    case 1:
        *vhclProtoId = gemProt->NwVprotoNum1;
        *buildProtoId = gemProt->NwBprotoNum1;
        break;

    case 6:
        *vhclProtoId = gemProt->NwVprotoNum2;
        *buildProtoId = gemProt->NwBprotoNum2;
        break;

    case 3:
        *vhclProtoId = gemProt->NwVprotoNum3;
        *buildProtoId = gemProt->NwBprotoNum3;
        break;

    case 4:
        *vhclProtoId = gemProt->NwVprotoNum4;
        *buildProtoId = gemProt->NwBprotoNum4;
        break;

    default:
        *vhclProtoId = 0;
        *buildProtoId = 0;
        break;
    }
}

void ActivateNetWunderstein(NC_STACK_ypaworld *yw, cellArea *cell, int gemId)
{
    TMapGem &gem = yw->_techUpgrades[gemId];

    int vhclId, buildId;
    GetNetGemProtoIds(yw, &gem, &vhclId, &buildId);

    yw->_upgradeId = gemId;
    yw->_upgradeTimeStamp = yw->_timeStamp;
    yw->_upgradeVehicleId = vhclId;
    yw->_upgradeWeaponId = 0;
    yw->_upgradeBuildId = buildId;

    if ( vhclId )
    {
        yw->_vhclProtos[vhclId].disable_enable_bitmask = 0;
        yw->_vhclProtos[vhclId].disable_enable_bitmask |= 1 << yw->_GameShell->netPlayerOwner;
    }

    if ( buildId )
    {
        yw->_buildProtos[buildId].EnableMask = 0;
        yw->_buildProtos[buildId].EnableMask |= 1 << yw->_GameShell->netPlayerOwner;
    }

    TInfoMessage msg;
    msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_TECHUP);
    msg.unit = 0;
    msg.Priority = 48;

    if ( gem.Type )
        msg.MsgID = World::Log::GetUpgradeLogID(gem.Type);
    else
        msg.MsgID = 0;

    yw->ShowMessage(&msg);

    if ( yw->_isNetGame && yw->_netExclusiveGem )
    {
        uamessage_upgrade upMsg;
        upMsg.msgID = UAMSG_UPGRADE;
        upMsg.owner = yw->_GameShell->netPlayerOwner;
        upMsg.enable = 1;
        upMsg.upgradeID = gemId;

        yw->NetBroadcastMessage(&upMsg, sizeof(upMsg), true);
    }

    cell->PurposeType = cellArea::PT_TECHDEACTIVE;
}

void RevokeTechUpgrade(NC_STACK_ypaworld *yw, cellArea *cell, int gemId)
{
    TMapGem &gem = yw->_techUpgrades[gemId];

    int vhclId;
    int buildId;

    GetNetGemProtoIds(yw, &gem, &vhclId, &buildId);

    if ( vhclId )
        yw->_vhclProtos[vhclId].disable_enable_bitmask = 0;

    if ( buildId )
        yw->_buildProtos[buildId].EnableMask = 0;

    std::string msgText = Locale::Text::Feedback(Locale::FEEDBACK_TECHDOWN) + gem.MsgDefault;

    TInfoMessage msg;
    msg.unit = 0;
    msg.Priority = 80;
    msg.txt = msgText;
    msg.MsgID = 29;

    yw->ShowMessage(&msg);

    if ( yw->_isNetGame )
    {
        uamessage_upgrade upMsg;
        upMsg.msgID = UAMSG_UPGRADE;
        upMsg.owner = yw->_GameShell->netPlayerOwner;
        upMsg.enable = 0;
        upMsg.upgradeID = gemId;

        yw->NetBroadcastMessage(&upMsg, sizeof(upMsg), true);
    }

    cell->PurposeIndex = 0;
    cell->PurposeType = cellArea::PT_NONE;
}

void NC_STACK_ypaworld::yw_ActivateWunderstein(cellArea *cell, int gemid)
{
    _upgradeVehicleId = 0;
    _upgradeBuildId = 0;
    _upgradeWeaponId = 0;

    _upgradeId = gemid;
    _upgradeTimeStamp = _timeStamp;

    TMapGem &gem = _techUpgrades[gemid];

    if ( !gem.ScriptFile.empty() )
    {
        if ( !LoadProtosScript(gem.ScriptFile) )
            ypa_log_out("yw_ActivateWunderstein: ERROR parsing script %s.\n", gem.ScriptFile.c_str());
    }
    else
    {
        std::string tmp = Common::Env.SetPrefix("rsrc", "data:");

        ScriptParser::HandlersList parsers {
            new World::Parsers::VhclProtoParser(this),
            new World::Parsers::WeaponProtoParser(this),
            new World::Parsers::BuildProtoParser(this)
        };

        ScriptParser::ParseStringList(gem.ActionsList, parsers, ScriptParser::FLAG_NO_SCOPE_SKIP);
        Common::Env.SetPrefix("rsrc", tmp);
    }

    TInfoMessage msg;
    msg.unit = NULL;
    msg.Priority = 48;
    msg.txt = Locale::Text::Feedback(Locale::FEEDBACK_TECHUP);

    if ( gem.Type )
        msg.MsgID = World::Log::GetUpgradeLogID(gem.Type);
    else
        msg.MsgID = 0;

    ShowMessage(&msg);

    cell->PurposeType = cellArea::PT_TECHDEACTIVE;
}

void SpawnBuildingExplosions(NC_STACK_ypaworld *yw, const cellArea &cell, int bldX, int bldY)
{
    Common::Point dist = yw->_viewerBact->_cellId.AbsDistance( cell.CellId );
  
    if ( dist.x + dist.y <= (yw->_renderSectors - 1) / 2 )
    {
        const TLego &lego = yw->_legoArray[  yw->GetLegoBld(&cell, bldX, bldY) ];

        vec3d ttt = World::SectorIDToCenterPos3( cell.CellId );
        ttt.y = cell.height;

        if ( cell.SectorType != 1 )
        {
            ttt.x += (bldX - 1) * 300.0;
            ttt.z += (bldY - 1) * 300.0;
        }
        
        for (const TLego::ExFX &fx : lego.Explosions)
        {
            if ( fx.Index >= yw->_fxLimit )
                break;

            TUnitSpawn spawn;
            spawn.vehicle_id = fx.ObjectID;
            spawn.pos = fx.Position + ttt;

            NC_STACK_ypabact *boom = yw->CreateVehicle(&spawn);

            if ( boom )
            {
                boom->_owner = 0;

                setState_msg deadState;
                deadState.newStatus = BACT_STATUS_DEAD;
                deadState.setFlags = BACT_STFLAG_DEATH1;
                deadState.unsetFlags = 0;
                boom->SetState(&deadState);

                yw->AddToUnitsList(boom);

                TImpulse impulse;
                impulse.pos = spawn.pos;
                impulse.energ = 40000;
                impulse.dir.x = fx.Position.x;
                impulse.dir.y = -150.0;
                impulse.dir.z = fx.Position.z;

                float tmp = impulse.dir.length();

                if ( tmp > 0.1 )
                    impulse.dir /= tmp;

                impulse.force = 30.0;
                impulse.mass = 50.0;

                boom->ApplyImpulse(&impulse);
            }
        }
    }
}

void WarnPowerStationAttacked(NC_STACK_ypaworld *yw, const cellArea &cell, TSectorDamage *arg)
{
    if ( cell.PurposeType == cellArea::PT_POWERSTATION )
    {
        if ( cell.owner == yw->_userRobo->_owner )
        {
            if ( arg->unit )
            {
                if ( yw->_userRobo->_owner != arg->unit->_owner && yw->_timeStamp - yw->_msgTimestampPSUnderAtk > 5000 )
                {
                    yw->_msgTimestampPSUnderAtk = yw->_timeStamp;

                    TInfoMessage msg;
                    msg.unit = NULL;
                    msg.Priority = 77;
                    msg.MsgID = 33;

                    yw->ShowMessage(&msg);
                }
            }
        }
    }
}



void NC_STACK_ypaworld::DamageSectorBuilding(TSectorDamage *arg)
{
    Common::Point sec = World::PositionToSectorID( arg->pos );
    cellArea &cell = _cells.At(sec);

    if ( cell.IsGamePlaySector() && cell.PurposeType != cellArea::PT_CONSTRUCTING )
    {
        int tile = (int)(arg->pos.x / 150.0) % 8;

        int col;

        if ( tile < 3 )
            col = 1;
        else if ( tile < 5)
            col = 2;
        else
            col = 3;

        tile = (int)(-arg->pos.z / 150.0) % 8;

        int row;

        if ( tile < 3 )
            row = 1;
        else if ( tile < 5)
            row = 2;
        else
            row = 3;

        if ( col && row )
        {
            int bldY;
            int bldX;

            if ( cell.SectorType == 1 )
            {
                bldY = 0;
                bldX = 0;
            }
            else
            {
                bldX = col - 1;
                bldY = 2 - (row - 1);
            }

            int oldHealth = cell.buildings_health.At(bldX, bldY);

            int newHealth = oldHealth - arg->energy * (100 - _legoArray[  GetLegoBld(&cell, bldX, bldY)  ].Shield ) / 100 / 400;

            if ( newHealth < 0 )
                newHealth = 0;
            else if ( newHealth > 255 )
                newHealth = 255;

            int oldModel = _buildHealthModelId[oldHealth];
            int newModel = _buildHealthModelId[newHealth];

            if ( oldModel > newModel )
            {
                while ( oldModel > newModel )
                {
                    SpawnBuildingExplosions(this, cell, bldX, bldY);

                    oldModel--;
                }
            }
            else if ( oldModel < newModel )
            {
                while ( oldModel < newModel )
                {
                    SpawnBuildingExplosions(this, cell, bldX, bldY);

                    oldModel++;
                }
            }

            cell.buildings_health.At(bldX, bldY) = newHealth;

            WarnPowerStationAttacked(this, cell, arg);

            CellCheckHealth(&cell, arg->OwnerID, arg->unit);

            if ( cell.PurposeType == cellArea::PT_TECHUPGRADE )
            {
                if ( _userRobo && _userRobo->_owner == cell.owner )
                {
                    if ( _isNetGame )
                        ActivateNetWunderstein(this, &cell, cell.PurposeIndex);
                    else
                        yw_ActivateWunderstein(&cell, cell.PurposeIndex);                  

                    HistoryEventAdd( World::History::Upgrade(sec.x, sec.y, cell.owner, _techUpgrades[ _upgradeId ].Type, _upgradeVehicleId, _upgradeWeaponId, _upgradeBuildId) );
                }
            }
            else if ( cell.PurposeType == cellArea::PT_TECHDEACTIVE )
            {
                if ( _isNetGame )
                {
                    int totalHealth = 0;

                    for (auto hlth : cell.buildings_health)
                        totalHealth += hlth;

                    if ( !totalHealth )
                        RevokeTechUpgrade(this, &cell, cell.PurposeIndex);
                }
            }
        }
    }
}


size_t NC_STACK_ypaworld::GetSectorInfo(TSectorInfo *arg)
{
    arg->CellId = World::PositionToSectorID( arg->pos_x, arg->pos_z );
    
    if ( !IsSector( arg->CellId ) )
    {
        ypa_log_out("YWM_GETSECTORINFO %d %d max: %d %d\n", arg->CellId.x, arg->CellId.y, _mapSize.x, _mapSize.y);
        ypa_log_out("YWM_GETSECTORINFO ausserhalb!!!\n");

        arg->pcell = NULL;

        return 0;
    }

    arg->pcell = &_cells(arg->CellId);
    return 1;
}


void NC_STACK_ypaworld::SetViewerUnit(NC_STACK_ypabact *bact)
{
    _viewerBact = bact;

    setYW_userVehicle(bact);
}


void NC_STACK_ypaworld::AddToUnitsList(NC_STACK_ypabact *bact)
{
    newMaster_msg masterMsg;

    masterMsg.bact = NULL;
    masterMsg.list = &_unitsList;

    bact->SetNewMaster(&masterMsg);
}


void NC_STACK_ypaworld::IntersectRay(TRayIntersection *arg)
{
    arg->tVal = 2.0;
    arg->isect = 0;

    vec3d stpos = arg->stPos;

    float pos_xx = stpos.x + arg->vect.x;
    float pos_zz = stpos.z + arg->vect.z;

    int dx = (stpos.x + 150) / 300;
    int dxx = (pos_xx + 150) / 300;

    int dz = (-stpos.z + 150) / 300;

    int dzz = (-pos_zz + 150) / 300;

    int elems = 0;
    TSectorCollision collisions[4];

    if ( dx == dxx && dz == dzz )
    {
        elems = 1;
        collisions[0] = GetSectorCollision(dx, dz, dx, dz, arg->flags);
    }
    else if ( dx == dxx || dz == dzz )
    {
        elems = 2;
        collisions[0] = GetSectorCollision(dx, dz, dx,  dz,  arg->flags);
        collisions[1] = GetSectorCollision(dx, dz, dxx, dzz, arg->flags);
    }
    else
    {
        elems = 4;
        collisions[0] = GetSectorCollision(dx, dz, dx,  dz,  arg->flags);
        collisions[1] = GetSectorCollision(dx, dz, dx,  dzz, arg->flags);
        collisions[2] = GetSectorCollision(dx, dz, dxx, dz,  arg->flags);
        collisions[3] = GetSectorCollision(dx, dz, dxx, dzz, arg->flags);
    }

    for (int i = 0; i < elems; i++)
    {
        if ( collisions[i].CollisionType )
        {
            if ( collisions[i].CollisionType != 1)
                FitFillerCollisionHeights(collisions[i]);

            arg->stPos = stpos - collisions[i].pos;

            IntersectRaySkeleton(arg, collisions[i]);

            if ( arg->isect )
                break;
        }
    }
}


void NC_STACK_ypaworld::FindSphereCollisions(TSphereCollisionQuery *arg)
{
    arg->coll_count = 0;

    vec3d pos = arg->pos;

    int dxx = (pos.x + 150) / 300;
    int dzz = (-pos.z + 150) / 300;
    int xxmr = (pos.x - arg->radius + 150) / 300;
    int zzmr = (-(pos.z - arg->radius) + 150) / 300;
    int xxpr = (pos.x + arg->radius + 150) / 300;
    int zzpr = (-(pos.z + arg->radius) + 150) / 300;

    TSectorCollision collision;

    for (int i = 0; i < 9; i++)
    {
        collision.CollisionType = 0;

        switch ( i )
        {
        case 0:
            collision = GetSectorCollision(dxx, dzz, dxx, dzz, arg->flags);
            break;

        case 1:
            if ( dxx != xxmr )
                collision = GetSectorCollision(dxx, dzz, xxmr, dzz, arg->flags);
            break;

        case 2:
            if ( dxx != xxpr )
                collision = GetSectorCollision(dxx, dzz, xxpr, dzz, arg->flags);
            break;

        case 3:
            if ( dzz != zzmr )
                collision = GetSectorCollision(dxx, dzz, dxx, zzmr, arg->flags);
            break;

        case 4:
            if ( dzz != zzpr )
                collision = GetSectorCollision(dxx, dzz, dxx, zzpr, arg->flags);
            break;

        case 5:
            if ( dxx != xxmr && dzz != zzmr )
                collision = GetSectorCollision(dxx, dzz, xxmr, zzmr, arg->flags);
            break;

        case 6:
            if ( dxx != xxpr && dzz != zzmr )
                collision = GetSectorCollision(dxx, dzz, xxpr, zzmr, arg->flags);
            break;

        case 7:
            if ( dxx != xxpr && dzz != zzpr )
                collision = GetSectorCollision(dxx, dzz, xxpr, zzpr, arg->flags);
            break;

        case 8:
            if ( dxx != xxmr && dzz != zzpr )
                collision = GetSectorCollision(dxx, dzz, xxmr, zzpr, arg->flags);
            break;
        }

        if ( collision.CollisionType )
        {
            if ( collision.CollisionType != 1 )
                FitFillerCollisionHeights(collision);

            arg->pos = pos - collision.pos;

            CollideSphereWithSkeleton(arg, collision);
        }
    }
}


void NC_STACK_ypaworld::GuiWinAttach(GuiBase *lstvw)
{
    if ( !(lstvw->flags & GuiBase::FLAG_WITH_ICON) )
        lstvw->flags &= ~GuiBase::FLAG_ICONIFED;

    if ( lstvw->flags & GuiBase::FLAG_IN_LIST )
        GuiWinDetach(lstvw);

    lstvw->Attach(_guiActive);

    lstvw->flags |= GuiBase::FLAG_IN_LIST;

    if ( lstvw->flags & GuiBase::FLAG_WITH_ICON )
        lstvw->iconBox.pobject = lstvw;

    lstvw->pobject = lstvw;

    if ( lstvw->flags & GuiBase::FLAG_ICONIFED )
        Input::Engine.AddClickBoxFront(&lstvw->iconBox);
    else if ( lstvw->IsOpen() )
        Input::Engine.AddClickBoxFront(lstvw);
}


void NC_STACK_ypaworld::GuiWinDetach(GuiBase *lstvw)
{
    if ( lstvw->flags & GuiBase::FLAG_IN_LIST )
    {
        lstvw->Detach();

        lstvw->flags &= ~GuiBase::FLAG_IN_LIST;

        if ( lstvw->flags & GuiBase::FLAG_ICONIFED )
            Input::Engine.RemClickBox(&lstvw->iconBox);
        else if ( lstvw->IsOpen() )
            Input::Engine.RemClickBox(lstvw);
    }
}


void NC_STACK_ypaworld::MoveToDeadCache(NC_STACK_ypabact *bacto)
{
    if ( bacto->_bact_type == BACT_TYPES_MISSLE )
    {
        if ( bacto->_primTtype )
            ypa_log_out("OH NO! The DEATH CACHE BUG is back!\n");
    }

    SFXEngine::SFXe.StopCarrier(&bacto->_soundcarrier);

    newMaster_msg cache;
    cache.bact = NULL;
    cache.list = &_deadCacheList;

    bacto->SetNewMaster(&cache);

    TUnitPlacement placement;
    placement.pos.x = 600.0;
    placement.pos.y = -50000.0;
    placement.pos.z = -600.0;
    placement.flags = 2;

    bacto->SetPosition(&placement);

    bacto->_status_flg |= BACT_STFLAG_NORENDER;
}


size_t NC_STACK_ypaworld::IsNearViewer(NC_STACK_ypabact *bact)
{
    if ( _viewerBact )
    {
        Common::Point dist = _viewerBact->_cellId.AbsDistance( bact->_cellId );
        if ( dist.x + dist.y <= (_renderSectors - 1) / 2 )
            return 1;
    }

    for ( NC_STACK_ypabact* &station : _unitsList ) //Robos
    {
        if ( station->_status_flg & BACT_STFLAG_ISVIEW )
        {
            Common::Point dist = station->_cellId.AbsDistance( bact->_cellId );
            if ( dist.x + dist.y <= (_renderSectors - 1) / 2 )
                return 1;
        }

        
        for ( NC_STACK_ypabact* &comm : station->_kidList ) // Squad comms
        {
            if ( comm->_status_flg & BACT_STFLAG_ISVIEW )
            {
                Common::Point dist = comm->_cellId.AbsDistance( bact->_cellId );
                if ( dist.x + dist.y <= (_renderSectors - 1) / 2 )
                    return 1;
            }


            for ( NC_STACK_ypabact* &unit : comm->_kidList ) // Squad units
            {
                if ( unit->_status_flg & BACT_STFLAG_ISVIEW )
                {
                    Common::Point dist = unit->_cellId.AbsDistance( bact->_cellId );
                    if ( dist.x + dist.y <= (_renderSectors - 1) / 2 )
                        return 1;
                }
            }
        }
    }

    return 0;
}


NC_STACK_ypabact * NC_STACK_ypaworld::CreateVehicle(TUnitSpawn *vhcl_id)
{
    if ( vhcl_id->vehicle_id >= _vhclProtos.size() )
        return NULL;

    World::TVhclProto &vhcl = _vhclProtos[vhcl_id->vehicle_id];

    NC_STACK_ypabact *bacto = yw_createUnit(vhcl.model_id);

    if ( bacto )
    {
        bacto->_energy = vhcl.energy;
        bacto->_energy_max = vhcl.energy;
        bacto->_shield = vhcl.shield;
        bacto->_mass = vhcl.mass;
        bacto->_force = vhcl.force;
        bacto->_maxrot = vhcl.maxrot;
        bacto->_height = vhcl.height;
        bacto->_radius = vhcl.radius;
        bacto->_viewer_radius = vhcl.vwr_radius;
        bacto->_overeof = vhcl.overeof;
        bacto->_viewer_overeof = vhcl.vwr_overeof;
        bacto->_airconst = vhcl.airconst;
        bacto->_airconst_static = vhcl.airconst;
        bacto->_adist_sector = vhcl.adist_sector;
        bacto->_adist_bact = vhcl.adist_bact;
        bacto->_sdist_sector = vhcl.sdist_sector;
        bacto->_sdist_bact = vhcl.sdist_bact;
        bacto->_radar = vhcl.radar;
        bacto->_gun_radius = vhcl.gun_radius;
        bacto->_gun_power = vhcl.gun_power;
        bacto->_pitch_max = vhcl.max_pitch;
        bacto->_vehicleID = vhcl_id->vehicle_id;
        bacto->_weapon = vhcl.weapon;

        if ( vhcl.weapon == -1 )
            bacto->_weapon_flags = 0;
        else
            bacto->_weapon_flags = _weaponProtos.at( vhcl.weapon )._weaponFlags;

        bacto->_mgun = vhcl.mgun;
        bacto->_fire_pos.x = vhcl.fire_x;
        bacto->_fire_pos.y = vhcl.fire_y;
        bacto->_fire_pos.z = vhcl.fire_z;
        bacto->_gun_angle = vhcl.gun_angle;
        bacto->_gun_angle_user = vhcl.gun_angle;
        bacto->_num_weapons = vhcl.num_weapons;
        bacto->_kill_after_shot = vhcl.kill_after_shot;
        bacto->_vp_normal = _vhclModels.at( vhcl.vp_normal );
        bacto->_vp_fire = _vhclModels.at( vhcl.vp_fire );
        bacto->_vp_dead = _vhclModels.at( vhcl.vp_dead );
        bacto->_vp_wait = _vhclModels.at( vhcl.vp_wait );
        bacto->_vp_megadeth = _vhclModels.at( vhcl.vp_megadeth );
        bacto->_vp_genesis = _vhclModels.at( vhcl.vp_genesis );

        bacto->_destroyFX = vhcl.dest_fx;
        bacto->_extDestroyFX = vhcl.ExtDestroyFX;

        for (NC_STACK_base *& vp_fx : bacto->_vp_fx_models)
            vp_fx = NULL;

        bacto->_scale_start = vhcl.scale_fx_p0;
        bacto->_scale_speed = vhcl.scale_fx_p1;
        bacto->_scale_accel = vhcl.scale_fx_p2;
        bacto->_scale_duration = vhcl.scale_fx_p3;
        bacto->_scale_pos = 0;
        
        bacto->_hidden = vhcl.hidden;
        bacto->_unhideRadar = vhcl.unhideRadar;

        for (int i = 0; vhcl.scale_fx_pXX[ i ]; i++ )
        {
            bacto->_vp_fx_models[i] = _vhclModels.at( vhcl.scale_fx_pXX[ i ] );

            bacto->_status_flg |= BACT_STFLAG_SEFFECT;
        }

        bacto->_soundcarrier.Resize(vhcl.sndFX.size());

        for (World::TVhclSound &sfx : vhcl.sndFX)
            sfx.LoadSamples();

        for (size_t i = 0; i < vhcl.sndFX.size(); i++)
        {
            TSoundSource *smpl_inf = &bacto->_soundcarrier.Sounds[ i ];

            smpl_inf->Volume = vhcl.sndFX[i].volume;
            smpl_inf->Pitch = vhcl.sndFX[i].pitch;

            if ( World::TVhclProto::IsLoopingSnd(i) )
                smpl_inf->SetLoop(true);

            if ( vhcl.sndFX[i].MainSample.Sample )
                smpl_inf->PSample = vhcl.sndFX[i].MainSample.Sample->GetSampleData();
            else
                smpl_inf->PSample = 0;

            if ( vhcl.sndFX[i].sndPrm.slot )
            {
                smpl_inf->PPFx = &vhcl.sndFX[i].sndPrm;
                smpl_inf->SetPFx(true);
            }
            else
            {
                smpl_inf->SetPFx(false);
            }

            if ( vhcl.sndFX[i].sndPrm_shk.slot )
            {
                smpl_inf->PShkFx = &vhcl.sndFX[i].sndPrm_shk;
                smpl_inf->SetShk(true);
            }
            else
            {
                smpl_inf->SetShk(false);
            }

            if ( !vhcl.sndFX[i].extS.empty() )
            {
                smpl_inf->PFragments = &vhcl.sndFX[i].extS; //CHECK IT
                smpl_inf->SetFragmented(true);
            }
            else
            {
                smpl_inf->SetFragmented(false);
            }
        }

        bacto->_pitch = bacto->_soundcarrier.Sounds[0].Pitch;
        bacto->_volume = bacto->_soundcarrier.Sounds[0].Volume;

        bacto->SetParameters(vhcl.initParams);

        TUnitPlacement placement;
        placement.pos = vhcl_id->pos;
        placement.flags = 0;
        bacto->SetPosition(&placement);

        setState_msg stateMsg;
        stateMsg.newStatus = BACT_STATUS_NORMAL;
        stateMsg.setFlags = 0;
        stateMsg.unsetFlags = 0;
        bacto->SetStateInternal(&stateMsg);
    }

    return bacto;
}


NC_STACK_ypamissile * NC_STACK_ypaworld::CreateWeapon(TUnitSpawn *arg)
{
    if ( arg->vehicle_id >= _weaponProtos.size() )
        return NULL;

    World::TWeapProto &wproto = _weaponProtos.at(arg->vehicle_id);

    if ( !(wproto._weaponFlags & 1) )
        return NULL;

    NC_STACK_ypamissile *wobj = dynamic_cast<NC_STACK_ypamissile *>( yw_createUnit(wproto.unitID) );

    if ( !wobj )
        return NULL;

    wobj->_energy = wproto.energy;
    wobj->_energy_max = wproto.energy;
    wobj->_shield = 0;
    wobj->_mass = wproto.mass;
    wobj->_force = wproto.force;
    wobj->_maxrot = wproto.maxrot;
    wobj->_height = wproto.heightStd;
    wobj->_radius = wproto.radius;
    wobj->_viewer_radius = wproto.vwr_radius;
    wobj->_overeof = wproto.overeof;
    wobj->_viewer_overeof = wproto.vwr_overeof;
    wobj->_airconst = wproto.airconst;
    wobj->_airconst_static = wproto.airconst;
    wobj->_adist_sector = wproto.adistSector;
    wobj->_adist_bact = wproto.adistBact;
    wobj->_vehicleID = arg->vehicle_id;
    wobj->_weapon = 0;

    wobj->_vp_normal =   _vhclModels.at(wproto.vp_normal);
    wobj->_vp_fire =     _vhclModels.at(wproto.vp_fire);
    wobj->_vp_dead =     _vhclModels.at(wproto.vp_dead);
    wobj->_vp_wait =     _vhclModels.at(wproto.vp_wait);
    wobj->_vp_megadeth = _vhclModels.at(wproto.vp_megadeth);
    wobj->_vp_genesis =  _vhclModels.at(wproto.vp_genesis);

    wobj->_destroyFX = wproto.dfx;
    wobj->_extDestroyFX = wproto.ExtDestroyFX;

    int missileType;

    switch(wproto._weaponFlags)
    {
    case 1:
        missileType = NC_STACK_ypamissile::MISL_BOMB;
        break;

    case 7:
        missileType = NC_STACK_ypamissile::MISL_TARGETED;
        break;

    case 11:
        missileType = NC_STACK_ypamissile::MISL_OBSAVOID;
        break;

    case 17:
        missileType = NC_STACK_ypamissile::MISL_GRENADE;
        break;

    default:
        missileType = NC_STACK_ypamissile::MISL_DIRECT;
        break;
    }

    wobj->SetLifeTime(wproto.life_time);
    wobj->SetDelay(wproto.delay_time);
    wobj->SetDriveTime(wproto.drive_time);
    wobj->SetMissileType(missileType);
    wobj->SetPowerHeli(wproto.energy_heli * 1000.0);
    wobj->SetPowerTank(wproto.energy_tank * 1000.0);
    wobj->SetPowerFlyer(wproto.energy_flyer * 1000.0);
    wobj->SetPowerRobo(wproto.energy_robo * 1000.0);

    /* Original bug caused by mixing vararg and float values
       that does not passed as 32-bit float value and
       instead it was passed as 64-bit floats, so it's
       break TAG-Val array alignment.*/
    
    if (_fixWeaponRadius)
    {
        wobj->SetRadiusHeli(wproto.radius_heli);
        wobj->SetRadiusTank(wproto.radius_tank);
        wobj->SetRadiusFlyer(wproto.radius_flyer);
        wobj->SetRadiusRobo(wproto.radius_robo);
    }
    else
    {
        wobj->SetRadiusHeli(0.0);
        wobj->SetRadiusTank(0.0);
        wobj->SetRadiusFlyer(0.0);
        wobj->SetRadiusRobo(0.0);
    }

    wobj->_soundcarrier.Resize(wproto.sndFXes.size());

    for (World::TVhclSound &sfx : wproto.sndFXes)
        sfx.LoadSamples();

    for (size_t i = 0; i < wproto.sndFXes.size(); i++)
    {
        TSoundSource *snd = &wobj->_soundcarrier.Sounds[i];

        snd->Volume = wproto.sndFXes[i].volume;
        snd->Pitch = wproto.sndFXes[i].pitch;

        if ( i == 0 )
            snd->SetLoop(true);

        if ( wproto.sndFXes[i].MainSample.Sample )
            snd->PSample = wproto.sndFXes[i].MainSample.Sample->GetSampleData();
        else
            snd->PSample = 0;

        if ( wproto.sndFXes[i].sndPrm.slot )
        {
            snd->SetPFx(true);
            snd->PPFx = &wproto.sndFXes[i].sndPrm;
        }
        else
        {
            snd->SetPFx(false);
        }

        if ( wproto.sndFXes[i].sndPrm_shk.slot )
        {
            snd->SetShk(true);
            snd->PShkFx = &wproto.sndFXes[i].sndPrm_shk;
        }
        else
        {
            snd->SetShk(false);
        }

        if ( !wproto.sndFXes[i].extS.empty() )
        {
            snd->SetFragmented(true);
            snd->PFragments = &wproto.sndFXes[i].extS;
        }
        else
        {
            snd->SetFragmented(false);
        }
    }

    wobj->SetParameters(wproto.initParams);

    TUnitPlacement placement;

    placement.pos = arg->pos;
    placement.flags = 1;

    wobj->SetPosition(&placement);

    setState_msg stateMsg;

    stateMsg.setFlags = 0;
    stateMsg.unsetFlags = 0;
    stateMsg.newStatus = BACT_STATUS_NORMAL;

    wobj->SetStateInternal(&stateMsg);

    return wobj;
}


size_t NC_STACK_ypaworld::CreateBuilding(TBuildingPlacement *arg)
{
    if (  !arg->instant 
       && !_allowMultiBuildLevel 
       && IsAnyBuildingProcess(arg->owner))
            return false;
        
    cellArea &cell = _cells(arg->CellId);

    bool UserInSec = false;

    for ( NC_STACK_ypabact* &node : cell.unitsList )
    {
        
        if ( _userUnit == node || node->_bact_type == BACT_TYPES_ROBO)
        {
            UserInSec = true;
            break;
        }
    }

    if ( _userUnit  &&  &cell == _userUnit->_pSector )
        UserInSec = true;

    if (cell.IsBorder())
        return 0;
    
    if ( cell.PurposeType == cellArea::PT_CONSTRUCTING )
        return 0;
    else if ( UserInSec  && !arg->instant )
        return 0;
    else if ( cell.PurposeType == cellArea::PT_POWERSTATION )
        PowerStationErase(&cell);
    else if ( (cell.PurposeType == cellArea::PT_TECHUPGRADE || cell.PurposeType == cellArea::PT_GATECLOSED || cell.PurposeType == cellArea::PT_GATEOPENED) && !arg->instant )
        return 0;
    else if ( cell.PurposeType == cellArea::PT_TECHDEACTIVE && _isNetGame )
        return 0;

    if ( arg->instant )
    {
        PlaceBuilding(arg->CellId, arg->owner, arg->blg_ID, arg->flags & 1);
    }
    else
    {        
        DestroyAllGunsInSector(&cell);

        if ( !BuildingConstructBegin(&cell, arg->blg_ID, arg->owner, World::CVBuildConstructTime) )
            return 0;
    }

    return 1;
}


void NC_STACK_ypaworld::IntersectLongRay(TRayIntersection *arg)
{
    arg->tVal = 2.0;
    arg->isect = 0;

    vec3d stpos = arg->stPos;

    int fixX = (stpos.x + 150.0) / 75.0 * 16384.0;
    int fixZ = (stpos.z - 150.0) / 75.0 * 16384.0;

    float absDx = fabs(arg->vect.x);
    float absDz = fabs(arg->vect.z);

    int stepZ;
    int stepX;
    float remaining;

    if ( absDx != 0.0 || absDz != 0.0 )
    {
        if ( absDx <= absDz )
        {
            stepX = (absDx / absDz * 16384.0);

            if ( arg->vect.x < 0.0 )
                stepX = -stepX;

            if ( arg->vect.z >= 0.0 )
                stepZ = 16384;
            else
                stepZ = -16384;

            remaining = absDz;
        }
        else
        {
            if ( arg->vect.x >= 0.0 )
                stepX = 16384;
            else
                stepX = -16384;

            stepZ = (absDz / absDx * 16384.0);

            if ( arg->vect.z < 0.0 )
                stepZ = -stepZ;

            remaining = absDx;
        }
    }
    else
    {
        stepZ = 0;
        remaining = 0.0;
        stepX = 0;
    }

    int startSecX = fixX >> 16;
    int startSecY = -fixZ >> 16;

    TSectorCollision collision;
    int secX, secY;

    do // Don't like this :E
    {
        secX = fixX >> 16;
        secY = -fixZ >> 16;

        collision = GetSectorCollision(startSecX, startSecY, secX, secY, arg->flags);

        if ( collision.CollisionType )
        {
            if ( collision.CollisionType != 1 )
                FitFillerCollisionHeights(collision);

            arg->stPos = stpos - collision.pos;

            IntersectRaySkeleton(arg, collision);

            if ( arg->isect )
                return;
        }

        do // Don't like this :E
        {
            remaining -= 75.0;
            fixX += stepX;
            fixZ += stepZ;
        }
        while ( fixX >> 16 == secX && -fixZ >> 16 == secY && remaining > 0.0 );

    }
    while( remaining > 0.0 );

    int endSecX = ((int)((arg->vect.x + stpos.x + 150.0) / 75.0 * 16384.0)) >> 16;
    // TODO(bug): -endSecZ is -(z >> 16), but secY above is (-fixZ) >> 16. They differ
    // by one unless z is on a sector edge, so the extra test below can hit
    // the wrong sector or skip the real end sector.
    int endSecZ = ((int)((arg->vect.z + stpos.z - 150.0) / 75.0 * 16384.0)) >> 16;

    if ( endSecX != secX || -endSecZ != secY )
    {
        collision = GetSectorCollision(startSecX, startSecY, endSecX, -endSecZ, arg->flags);

        if ( collision.CollisionType )
        {
            if ( collision.CollisionType != 1 )
                FitFillerCollisionHeights(collision);

            arg->stPos = stpos - collision.pos;

            IntersectRaySkeleton(arg, collision);
        }
    }
}


void NC_STACK_ypaworld::FindUnitOnRay(TUnitRayQuery *arg)
{
    arg->hitUnit = NULL;

    int fixX = arg->pos.x / 300.0 * 16384.0;
    int fixZ = arg->pos.z / 300.0 * 16384.0;

    float absDx = fabs(arg->vect.x);
    float absDz = fabs(arg->vect.z);

    arg->distance = arg->vect.length();

    if ( arg->distance <= 0.0 )
        return;

    int stepX, stepZ, remaining;

    if ( absDx != 0.0 || absDz != 0.0 )
    {
        if ( absDx <= absDz )
        {
            stepX = (absDx * 16384.0 / absDz);

            if ( arg->vect.x < 0.0 )
                stepX = -stepX;

            if ( arg->vect.z >= 0.0 )
                stepZ = 16384;
            else
                stepZ = -16384;

            remaining = absDz;
        }
        else
        {
            if ( arg->vect.x >= 0.0 )
                stepX = 16384;
            else
                stepX = -16384;

            stepZ = (absDz * 16384.0 / absDx);

            if ( arg->vect.z < 0.0 )
                stepZ = -stepZ;

            remaining = absDx;
        }
    }
    else
    {
        stepZ = 0;
        remaining = 0;
        stepX = 0;
    }

    vec3d dir = arg->vect / arg->distance;

    while ( remaining >= 0 )
    {
        int secY = -fixZ >> 16;
        int secX = fixX >> 16;

        if ( IsSector( {secX, secY} ) )
        {
            for ( NC_STACK_ypabact* &sect_bacts : _cells(secX, secY).unitsList )
            {
                if ( sect_bacts != arg->unit && sect_bacts->_status != BACT_STATUS_DEAD )
                {
                    if ( !(arg->unit == _userUnit && _playerInHSGun) || sect_bacts != _userRobo )
                    {
                        vec3d toUnit = sect_bacts->_position - arg->pos;
                        vec3d cross = dir * toUnit;

                        if ( cross.length() < sect_bacts->_radius )
                        {
                            float dist = toUnit.length();

                            if ( dir.dot(toUnit) / dist > 0.0 )
                            {
                                if ( arg->distance > dist - sect_bacts->_radius )
                                {
                                    arg->hitUnit = sect_bacts;
                                    arg->distance = dist - sect_bacts->_radius;
                                }
                            }
                        }
                    }
                }
            }
        }

        if ( arg->hitUnit )
            break;

        do
        {
            fixX += stepX;
            fixZ += stepZ;
            remaining -= 300;
        }
        while ( (fixX >> 16) == secX && (-fixZ >> 16) == secY && remaining >= 0 );
    }
}


void NC_STACK_ypaworld::DeleteLevel()
{
    EnableLevelPasses();

    if ( _levelInfo.State == TLevelInfo::STATE_COMPLETED )
    {
        _prepareDebrief = true;
        if ( _firstContactFaction )
            _levelInfo.JodieFoster[ _firstContactFaction ] = 1;
    }
    else
    {
        _prepareDebrief = false;
    }

    if ( _levelInfo.State == TLevelInfo::STATE_COMPLETED )
    {
        if ( _GameShell )
        {
            FSMgr::FileHandle *fil = uaOpenFileAlloc(fmt::sprintf("save:%s/sgisold.txt", _GameShell->UserName), "w");

            if ( fil )
                delete fil;

            SaveSettings(_GameShell, fmt::sprintf("%s/user.txt", _GameShell->UserName), World::SDF_ALL);

            fil = uaOpenFileAlloc("env:user.def", "w");

            if ( fil )
            {
                fil->puts(_GameShell->UserName);
                delete fil;
            }
        }
    }

    if ( _isNetGame )
    {
        if ( !_GameShell->sentAQ )
            NetSendExitMsg(0);

        _GameShell->LogNetStatistics();
        _GameShell->yw_netcleanup();

        _prepareDebrief = true;
        _gameWasNetGame = true;

        if ( _userUnit )
            _userOwnerIdWasInNetGame = _userUnit->_owner;
    }
    else
    {
        _userOwnerIdWasInNetGame = 0;
        _gameWasNetGame = false;
    }

    if ( _replayRecorder->do_record )
        recorder_stoprec(this);

    _screenShotSeq = false;
    _replayRecorder->do_record = 0;

    NC_STACK_bitmap *disk = loadDisk_screen(this);

    if ( disk )
    {
        draw_splashScreen(this, disk);
        disk->Delete();
    }

    //ypaworld_func151__sub5(this); Free map events
    _voiceMessage.Reset();

    SFXEngine::SFXe.setMasterVolume(audio_volume);

    GUI_Close();

    if ( _skyObject )
    {
        _skyObject->Delete();
        _skyObject = NULL;
    }

    int plowner;
    if ( _userRobo )
        plowner = _userRobo->_owner;
    else
        plowner = 0;

    while ( !_deadCacheList.empty() )
    {
        NC_STACK_ypabact *bct = _deadCacheList.front();

        if ( _gameWasNetGame )
        {
            if ( plowner != bct->_owner && bct->_bact_type == BACT_TYPES_ROBO )
                NetRemove(bct);
        }

        bct->Delete();
    }

    while ( !_unitsList.empty() )
    {
        NC_STACK_ypabact *bct = _unitsList.front();

        if ( _gameWasNetGame )
        {
            if ( plowner != bct->_owner && bct->_bact_type == BACT_TYPES_ROBO )
                NetRemove(bct);
        }

        bct->Delete();
    }

    // NetRemove can fill deadcache, so clean it again
    while ( !_deadCacheList.empty() )
        _deadCacheList.front()->Delete();

    // A replay's camera was one of the units. The next level's units set the
    // view point again in its first step, after SaveUnitTransforms.
    TF::Engine.SetViewPoint(NULL);
    _prevView = NULL;

    ProtosFreeSounds();

    FreeSetData(this);

    _powerStations.clear();
    _energyAccumMap.Clear();

    _lvlTypeMap.Clear();
    _lvlOwnMap.Clear();
    _lvlBuildingsMap.Clear();
    _lvlHeightMap.Clear();

    if ( _GameShell )
        _GameShell->samples1_info.Position = vec3d();

    if ( _gameWasNetGame )
    {
        LoadProtosScript(_initScriptFilePath);
    }
}


void NC_STACK_ypaworld::SetHudVisor(THudVisor *arg)
{
    _guiVisor = *arg;
}

void UserData::ReloadAllInputs()
{
    int savedActive = inpListActiveElement;

    for (int i = 0; i <= 48; i++)
        Input::Engine.SetHotKey(i, "nop");

    for (int i = 1; i <= 45; i++)
    {
        inpListActiveElement = i;
        p_YW->ReloadInput(i);
    }

    inpListActiveElement = savedActive;
}


bool NC_STACK_ypaworld::InitGameShell(UserData *usr)
{
    _GameShell = usr;
    usr->p_YW = this;

    _levelInfo.State = TLevelInfo::STATE_MENU;
    usr->EnvMode = ENVMODE_TITLE;

    System::IniConf::ReadFromNucleusIni();

    _netExclusiveGem = System::IniConf::NetGameExclusiveGem.Get<bool>();

    // Defaults for profiles that don't set these
    SetAITimeline(System::IniConf::GameTimeLine.Get<int>());
    SetMoviesEnabled(System::IniConf::GfxMoviePlayer.Get<bool>());

    usr->profiles.clear();
    usr->lang_dlls.clear();

    LoadKeyNames();

    listSaveDir("save:");
    listLocaleDir(usr, "locale:");


    usr->userNameDir = "NEWUSER";
    usr->userNameDirCursor = usr->userNameDir.size();

    usr->IgnoreScoreSaving = true;
    usr->diskListActiveElement = 0;
    usr->inpListActiveElement = 1;   

    usr->samples1_info.Clear();    
    usr->samples1_info.Resize(World::SOUND_ID_MAX);
    
//    for (TSoundSource &snd : usr->samples1_info.Sounds)
//    {
//        snd.Volume = 127;
//        snd.Pitch = 0;
//    }

    if ( !usr->ShellSoundsLoad() )
    {
        ypa_log_out("Error: Unable to load from Shell.ini\n");
        return false;
    }
    
    usr->InputConfig[World::INPUT_BIND_DRIVE_DIR]   = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 3,  Input::KC_RIGHT, Input::KC_LEFT);
    usr->InputConfig[World::INPUT_BIND_DRIVE_SPEED] = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 4,  Input::KC_UP, Input::KC_DOWN);
    usr->InputConfig[World::INPUT_BIND_FLY_DIR]     = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 0,  Input::KC_RIGHT, Input::KC_LEFT);
    usr->InputConfig[World::INPUT_BIND_FLY_HEIGHT]  = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 1,  Input::KC_UP, Input::KC_DOWN);
    usr->InputConfig[World::INPUT_BIND_FLY_SPEED]   = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 2,  Input::KC_CTRL, Input::KC_SHIFT);
    usr->InputConfig[World::INPUT_BIND_GUN_HEIGHT]  = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 5,  Input::KC_A, Input::KC_Y);
    usr->InputConfig[World::INPUT_BIND_FIRE]        = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 0,  Input::KC_SPACE);
    usr->InputConfig[World::INPUT_BIND_CAMFIRE]     = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 1,  Input::KC_TAB);
    usr->InputConfig[World::INPUT_BIND_GUN]         = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 2,  Input::KC_RETURN);
    usr->InputConfig[World::INPUT_BIND_BRAKE]       = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 3,  Input::KC_NUM0);
    usr->InputConfig[World::INPUT_BIND_HUD]         = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 25, Input::KC_V);
    usr->InputConfig[World::INPUT_BIND_NEW]         = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 2,  Input::KC_N);
    usr->InputConfig[World::INPUT_BIND_ADD]         = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 3,  Input::KC_A);
    usr->InputConfig[World::INPUT_BIND_ORDER]       = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 0,  Input::KC_O);
    usr->InputConfig[World::INPUT_BIND_ATTACK]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 1,  Input::KC_SPACE);
    usr->InputConfig[World::INPUT_BIND_CONTROL]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 4,  Input::KC_C);
    usr->InputConfig[World::INPUT_BIND_AUTOPILOT]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 7,  Input::KC_G);
    usr->InputConfig[World::INPUT_BIND_MAP]         = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 8,  Input::KC_M);
    usr->InputConfig[World::INPUT_BIND_SQ_MANAGE]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 9,  Input::KC_F);
    usr->InputConfig[World::INPUT_BIND_LANDLAYER]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 10, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_OWNER]       = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 11, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_HEIGHT]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 12, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_LOCKVIEW]    = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 14, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_ZOOMIN]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 16, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_ZOOMOUT]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 17, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_MINIMAP]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 18, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_NEXT_COMM]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 20, Input::KC_F1);
    usr->InputConfig[World::INPUT_BIND_TO_HOST]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 21, Input::KC_F2);
    usr->InputConfig[World::INPUT_BIND_NEXT_UNIT]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 22, Input::KC_F3);
    usr->InputConfig[World::INPUT_BIND_TO_COMM]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 23, Input::KC_F4);
    usr->InputConfig[World::INPUT_BIND_QUIT]        = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 24, Input::KC_ESCAPE);
    usr->InputConfig[World::INPUT_BIND_LOG_WND]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 27, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_LAST_MSG]    = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 31, Input::KC_BACKSPACE);
    usr->InputConfig[World::INPUT_BIND_PAUSE]       = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 32, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_TO_ALL]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 37, Input::KC_NUM5);
    usr->InputConfig[World::INPUT_BIND_AGGR_1]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 38, Input::KC_1);
    usr->InputConfig[World::INPUT_BIND_AGGR_2]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 39, Input::KC_2);
    usr->InputConfig[World::INPUT_BIND_AGGR_3]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 40, Input::KC_3);
    usr->InputConfig[World::INPUT_BIND_AGGR_4]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 41, Input::KC_4);
    usr->InputConfig[World::INPUT_BIND_AGGR_5]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 42, Input::KC_5);
    usr->InputConfig[World::INPUT_BIND_WAPOINT]     = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 4,  Input::KC_SHIFT);
    usr->InputConfig[World::INPUT_BIND_HELP]        = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 43, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_LAST_SEAT]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 44, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_SET_COMM]    = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 45, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_ANALYZER]    = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 46, Input::KC_NONE);

    usr->ReloadAllInputs();

    TRemoteStartInfo remoteStart;

    if (!_netDriver->GetRemoteStart(&remoteStart) )
    {
        ypa_log_out("Error while remote start check\n");
        return  false;
    }

    // TODO(multiplayer): GetRemoteStart never fills callSIGN, so the name is
    // empty. It also never sets isClient and isHoster together, so the
    // isHoster branch below can't run and a remote-started host never enters
    // remote mode.
    if ( remoteStart.isClient )
    {
        _GameShell->netPlayerName = remoteStart.callSIGN;

        if ( remoteStart.isHoster )
            _GameShell->isHost = true;
        else
            _GameShell->isHost = false;

        _GameShell->remoteMode = true;

        usr->netLevelID = 0;
        usr->EnvMode = ENVMODE_NETPLAY;

        if ( usr->isHost )
        {
            usr->lobbyPlayers[ _netDriver->GetMyIndex() ].Ready = true;
            usr->rdyStart = true;
            usr->netSelMode = UserData::NETSCREEN_CHOOSE_MAP;
        }
        else
        {
            usr->netSelMode = UserData::NETSCREEN_INSESSION;
        }

        for(uint32_t i = 0; i < _netDriver->GetPlayerCount(); i++)
            _GameShell->lobbyPlayers[i].Name = _netDriver->GetPlayerName(i);

        usr->update_time_norm = 400;
        usr->flush_time_norm = 400;
    }
    else
    {
        _GameShell->remoteMode = false;
    }

    return true;
}


void NC_STACK_ypaworld::DeinitGameShell()
{
    _GameShell->yw_netcleanup();

    _GameShell->profiles.clear();

    _GameShell->lang_dlls.clear();

    SFXEngine::SFXe.StopPlayingSounds();

    for (NC_STACK_sample * &smpl : _GameShell->samples1)
    {
        if (smpl)
        {
            smpl->Delete();
            smpl = NULL;
        }
    }
    
    SFXEngine::SFXe.StopCarrier(&_GameShell->samples1_info);
}


void TMapRegionsNet::UnloadImages()
{
    if ( MenuImage )
    {
        MenuImage->Delete();
        MenuImage = NULL;
    }
    if ( RolloverImage )
    {
        RolloverImage->Delete();
        RolloverImage = NULL;
    }
    if ( FinishedImage )
    {
        FinishedImage->Delete();
        FinishedImage = NULL;
    }
    if ( EnabledImage )
    {
        EnabledImage->Delete();
        EnabledImage = NULL;
    }
    if ( MaskImage )
    {
        MaskImage->Delete();
        MaskImage = NULL;
    }
}


void LoadGameShellBkgImages(NC_STACK_ypaworld *yw, int mode)
{
    int loadOk = 1;

    if ( yw->_globalMapRegions.NumSets )
    {
        std::string oldRsrc = Common::Env.SetPrefix("rsrc", "levels:");

        int bestSet = 0;
        int bestDiff = 65535;
        for (int i = 0; i < yw->_globalMapRegions.NumSets; i++)
        {

            int xx = (yw->_globalMapRegions.background_map[i].Size.x - yw->_screenSize.x);
            int yy = (yw->_globalMapRegions.background_map[i].Size.y - yw->_screenSize.y);

            int sq = sqrt(xx * xx + yy * yy);

            if (sq < bestDiff)
            {
                bestSet = i;
                bestDiff = sq;
            }
        }

        std::string menu_map;
        std::string rollover_map;
        std::string mask_map;
        std::string finished_map;
        std::string enabled_map;

        NC_STACK_bitmap *ilbm_menu_map  = NULL;
        NC_STACK_bitmap *ilbm_rollover_map = NULL;
        NC_STACK_bitmap *ilbm_mask_map = NULL;
        NC_STACK_bitmap *ilbm_finished_map = NULL;
        NC_STACK_bitmap *ilbm_enabled_map = NULL;

        switch ( mode )
        {
        case ENVMODE_TITLE:
        case ENVMODE_INPUT:
        case ENVMODE_SETTINGS:
        case ENVMODE_NETPLAY:
        case ENVMODE_SELLOCALE:
        case ENVMODE_ABOUT:
        case ENVMODE_SELPLAYER:
        case ENVMODE_HELP:
            menu_map  = yw->_globalMapRegions.menu_map[bestSet].PicName;
            rollover_map = yw->_globalMapRegions.settings_map[bestSet].PicName;
            break;
        case ENVMODE_TUTORIAL:
            menu_map  = yw->_globalMapRegions.tut_background_map[bestSet].PicName;
            mask_map = yw->_globalMapRegions.tut_mask_map[bestSet].PicName;
            rollover_map = yw->_globalMapRegions.tut_rollover_map[bestSet].PicName;
            break;
        case ENVMODE_SINGLEPLAY:
            menu_map  = yw->_globalMapRegions.background_map[bestSet].PicName;
            rollover_map = yw->_globalMapRegions.rollover_map[bestSet].PicName;
            finished_map = yw->_globalMapRegions.finished_map[bestSet].PicName;
            mask_map = yw->_globalMapRegions.mask_map[bestSet].PicName;
            enabled_map = yw->_globalMapRegions.enabled_map[bestSet].PicName;
            break;
        default:
            break;
        }

        if ( !menu_map.empty() )
        {
            ilbm_menu_map = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, menu_map},
                {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1} } );
            if ( !ilbm_menu_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", menu_map.c_str());
                loadOk = 0;
            }
        }

        if ( !rollover_map.empty() )
        {
            ilbm_rollover_map = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, rollover_map},
                {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1} });
            if ( !ilbm_rollover_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", rollover_map.c_str());
                loadOk = 0;
            }
        }

        if ( !finished_map.empty() )
        {
            ilbm_finished_map = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, finished_map},
                {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1} });
            if ( !ilbm_finished_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", finished_map.c_str());
                loadOk = 0;
            }
        }

        if ( !enabled_map.empty() )
        {
            ilbm_enabled_map = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, enabled_map},
                {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1} });
            if ( !ilbm_enabled_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", enabled_map.c_str());
                loadOk = 0;
            }
        }
        if ( !mask_map.empty() )
        {
            ilbm_mask_map = Utils::ProxyLoadImage({{NC_STACK_rsrc::RSRC_ATT_NAME, mask_map}});
            if ( !ilbm_mask_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", mask_map.c_str());
                loadOk = 0;
            }
        }

        Common::Env.SetPrefix("rsrc", oldRsrc);

        if ( !loadOk )
        {
            if ( ilbm_menu_map )
            {
                ilbm_menu_map->Delete();
                ilbm_menu_map = NULL;
            }
            if ( ilbm_rollover_map )
            {
                ilbm_rollover_map->Delete();
                ilbm_rollover_map = NULL;
            }
            if ( ilbm_finished_map )
            {
                ilbm_finished_map->Delete();
                ilbm_finished_map = NULL;
            }
            if ( ilbm_enabled_map )
            {
                ilbm_enabled_map->Delete();
                ilbm_enabled_map = NULL;
            }
            if ( ilbm_mask_map )
            {
                ilbm_mask_map->Delete();
                ilbm_mask_map = NULL;
            }
        }
        yw->_globalMapRegions.UnloadImages();
        yw->_globalMapRegions.MenuImage = ilbm_menu_map;
        yw->_globalMapRegions.MaskImage = ilbm_mask_map;
        yw->_globalMapRegions.RolloverImage = ilbm_rollover_map;
        yw->_globalMapRegions.FinishedImage = ilbm_finished_map;
        yw->_globalMapRegions.EnabledImage = ilbm_enabled_map;
    }
}

void CalcMapRegionRects(NC_STACK_ypaworld *yw)
{
    std::array<Common::Rect, 256> regions;

    if ( yw->_globalMapRegions.MaskImage )
    {
        for (int i = 0; i < 256; i++)
        {
            regions[i].left = 10000;
            regions[i].top = 10000;
            regions[i].right = -10000;
            regions[i].bottom = -10000;
        }

        ResBitmap *bitm = yw->_globalMapRegions.MaskImage->GetBitmap();

        SDL_LockSurface(bitm->swTex);
        for (int y = 0; y < bitm->height; y++ )
        {
            uint8_t *ln = ((uint8_t *)bitm->swTex->pixels + y * bitm->swTex->pitch);

            for (int x = 0; x < bitm->width; x++)
            {
                Common::Rect &rgn = regions.at( ln[x] );

                if ( x < rgn.left )
                    rgn.left = x;

                if ( x > rgn.right )
                    rgn.right = x;

                if ( y < rgn.top )
                    rgn.top = y;

                if ( y > rgn.bottom )
                    rgn.bottom = y;
            }
        }

        for (int i = 0; i < 256; i++)
        {
            TMapRegionInfo &minf = yw->_globalMapRegions.MapRegions[i];

            if ( minf.Status != TMapRegionInfo::STATUS_NONE && minf.Status != TMapRegionInfo::STATUS_NETWORK && regions.at(i).IsValid() )
            {
                minf.Rect.left = 2.0 * ((float)(regions[i].left) / (float)bitm->width) + -1.0;
                minf.Rect.right = 2.0 * ((float)(regions[i].right) / (float)bitm->width) + -1.0;
                minf.Rect.top = 2.0 * ((float)(regions[i].top) / (float)bitm->height) + -1.0;
                minf.Rect.bottom = 2.0 * ((float)(regions[i].bottom) / (float)bitm->height) + -1.0;
            }
            else
                minf.Rect = Common::FRect();
        }
        
        SDL_UnlockSurface(bitm->swTex);
    }
}

void NC_STACK_ypaworld::GameShellInitBkgMode(int mode)
{
    LoadGameShellBkgImages(this, mode);
    if ( mode == ENVMODE_TUTORIAL || mode == ENVMODE_SINGLEPLAY )
    {
        _firstContactFaction = 0;
        _briefScreen.Stage = TBriefengScreen::STAGE_NONE;
        _globalMapRegions.SelectedRegion = 0;

        CalcMapRegionRects(this);

        _tipOfDayId = loadTOD(this, "tod.def");

        int nextTipId = _tipOfDayId + 1;

        if ( (nextTipId + 2490) > 2512 )
            nextTipId = 0;
        writeTOD(this, "tod.def", nextTipId);
    }
}

bool NC_STACK_ypaworld::GameShellInitBkg()
{
    GFX::Engine.SetClipCentered(Common::Rect (-(_screenSize.x / 2), -(_screenSize.y / 2), _screenSize.x / 2, _screenSize.y / 2) );
    GameShellInitBkgMode(_GameShell->EnvMode);
    return true;
}

//Controls creation methods
bool NC_STACK_ypaworld::CreateTitleControls(){
    _GameShell->titel_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)0},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_screenSize.y} } );
    if ( !_GameShell->titel_button )
    {
        ypa_log_out("Unable to create Titel-Button-Object\n");
        return false;
    }

    int allAdded = 0;
    NC_STACK_button::WidgetDesc btn_64arg;

    btn_64arg.tileset_down = 19;
    btn_64arg.tileset_up = 18;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    btn_64arg.tileset_disabled = 30;
    btn_64arg.xpos = _screenSize.x * 0.3328125;
    btn_64arg.ypos = _screenSize.y * 0.2291666666666666;
    btn_64arg.width = _screenSize.x / 3;
    btn_64arg.caption = Locale::Text::Title(Locale::TITLE_GAME);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
    btn_64arg.pressedCode = 0;
    btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SINGLE_PLAYER;
    btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_SINGLE_PLAYER_UP;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->titel_button->Add(&btn_64arg) )
    {
        btn_64arg.ypos = _screenSize.y * 0.3083333333333334;
        btn_64arg.caption = Locale::Text::Title(Locale::TITLE_NETWORK);
        btn_64arg.caption2.clear();
        btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_MULTIPLAYER_UP;
        btn_64arg.pressedCode = 0;
        btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
        btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER;

        if ( _GameShell->titel_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = _screenSize.x * 0.3328125;
            btn_64arg.ypos = _screenSize.y * 0.4333333333333334;
            btn_64arg.width = _screenSize.x / 3;
            btn_64arg.caption = Locale::Text::Title(Locale::TITLE_INPUT);
            btn_64arg.caption2.clear();
            btn_64arg.pressedCode = 0;
            btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
            btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS;
            btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_INPUT_SETTINGS_UP;

            if ( _GameShell->titel_button->Add(&btn_64arg) )
            {
                btn_64arg.ypos = _screenSize.y * 0.5125;
                btn_64arg.caption = Locale::Text::Title(Locale::TITLE_SETTINGS);
                btn_64arg.caption2.clear();
                btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_OPTIONS_UP;
                btn_64arg.pressedCode = 0;
                btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_OPTIONS;

                if ( _GameShell->titel_button->Add(&btn_64arg) )
                {
                    btn_64arg.ypos = _screenSize.y * 0.5916666666666667;
                    btn_64arg.caption = Locale::Text::Title(Locale::TITLE_PROFILE);
                    btn_64arg.caption2.clear();
                    btn_64arg.pressedCode = 0;
                    btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                    btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_SAVE_LOAD_UP;
                    btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SAVE_LOAD;

                    if ( _GameShell->titel_button->Add(&btn_64arg) )
                    {
                        btn_64arg.xpos = _screenSize.x * 0.890625;
                        btn_64arg.ypos = _screenSize.y * 0.9583333333333334;
                        btn_64arg.width = _screenSize.x * 0.1;
                        btn_64arg.caption = Locale::Text::Title(Locale::TITLE_LOCALE);
                        btn_64arg.caption2.clear();
                        btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_LANGUAGE_UP;
                        btn_64arg.pressedCode = 0;
                        btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                        btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE;

                        if ( _GameShell->titel_button->Add(&btn_64arg) )
                        {
                            btn_64arg.xpos = _screenSize.x * 0.3328125;
                            btn_64arg.ypos = _screenSize.y * 0.7166666666666667;
                            btn_64arg.width = _screenSize.x / 3;
                            btn_64arg.caption = Locale::Text::Title(Locale::TITLE_HELP);
                            btn_64arg.caption2.clear();
                            btn_64arg.pressedCode = 0;
                            btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                            btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_HELP;
                            btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_HELP_UP;

                            if ( _GameShell->titel_button->Add(&btn_64arg) )
                            {
                                btn_64arg.ypos = _screenSize.y * 0.7958333333333333;
                                btn_64arg.caption = Locale::Text::Title(Locale::TITLE_QUIT);
                                btn_64arg.caption2.clear();
                                btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_QUIT_UP;
                                btn_64arg.pressedCode = 0;
                                btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                                btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_QUIT;

                                if ( _GameShell->titel_button->Add(&btn_64arg) )
                                    allAdded = 1;
                            }
                        }
                    }
                }
            }
        }
    }

    if ( !allAdded )
    {
        ypa_log_out("Unable to add button to Titel\n");
        return false;
    }

    NC_STACK_button::WidgetState btnState;

    if ( _GameShell->lang_dlls.size() <= 1 )
    {
        btnState.state = 0;
        btnState.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE;
        _GameShell->titel_button->Disable(&btnState);
    }

    _GameShell->titel_button->HideScreen();
    return true;
}
bool NC_STACK_ypaworld::CreateSubBarControls(){
    int allAdded = 0;
    NC_STACK_button::WidgetDesc btn_64arg;
    subBarButtonWidth = _screenSize.x / 4 - 20;

    _GameShell->sub_bar_button = Nucleus::CInit<NC_STACK_button>({ 
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)(_screenSize.y - _fontH)},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_fontH}});

    if ( !_GameShell->sub_bar_button )
    {
        ypa_log_out("Unable to create Button-Object\n");
        return false;
    }

    allAdded = 0;

    btn_64arg.tileset_down = 19;
    btn_64arg.tileset_disabled = 30;
    btn_64arg.ypos = 0;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    btn_64arg.tileset_up = 18;
    btn_64arg.xpos = subBarButtonWidth + buttonsSpace;
    btn_64arg.width = subBarButtonWidth;
    btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_REWIND);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 1251;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1011;
    btn_64arg.upCode = 1016;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 2 * (buttonsSpace + subBarButtonWidth);
        btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_STEPFWD);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1020;
        btn_64arg.pressedCode = 1018;
        btn_64arg.button_id = 1013;

        if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_START);
            btn_64arg.caption2.clear();
            btn_64arg.upCode = 1019;
            btn_64arg.pressedCode = 0;
            btn_64arg.downCode = 1251;
            btn_64arg.button_id = 1014;

            if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = (_screenSize.x - 3 * subBarButtonWidth - 2 * buttonsSpace);
                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_GOTOLDSV);
                btn_64arg.caption2.clear();
                btn_64arg.pressedCode = 0;
                btn_64arg.downCode = 1251;
                btn_64arg.button_id = 1020;
                btn_64arg.upCode = 1026;

                if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                {
                    btn_64arg.xpos = (_screenSize.x - 2 * subBarButtonWidth - buttonsSpace);
                    btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_LOAD);
                    btn_64arg.caption2.clear();
                    btn_64arg.upCode = 1021;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.downCode = 1251;
                    btn_64arg.button_id = 1015;

                    if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                    {
                        btn_64arg.xpos = _screenSize.x - subBarButtonWidth;
                        btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_GOBACK);
                        btn_64arg.caption2.clear();
                        btn_64arg.pressedCode = 0;
                        btn_64arg.downCode = 1251;
                        btn_64arg.button_id = 1019;
                        btn_64arg.upCode = 1013;

                        if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                        {
                            allAdded = 1;
                        }
                    }
                }
            }
        }
    }
    if ( !allAdded )
    {
        ypa_log_out("Unable to add button to sub-bar\n");
        return false;
    }
    NC_STACK_button::WidgetState btnState;
    if ( _GameShell->sgmSaveExist != 1 )
    {
        btnState.butID = 1015;
        btnState.state = 0;
        _GameShell->sub_bar_button->Disable(&btnState);
    }

    btnState.state = 0;
    btnState.butID = 1014;
    _GameShell->sub_bar_button->Disable(&btnState);

    btnState.butID = 1013;
    _GameShell->sub_bar_button->Disable(&btnState);

    btnState.butID = 1011;
    _GameShell->sub_bar_button->Disable(&btnState);

    _GameShell->sub_bar_button->HideScreen();
    return true;
}
bool NC_STACK_ypaworld::CreateConfirmControls()
{

    _GameShell->confirm_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)0},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_screenSize.y}} );
    if ( !_GameShell->confirm_button )
    {
        ypa_log_out("Unable to create Confirm-Button-Object\n");
        return false;
    }

    NC_STACK_button::WidgetDesc btn_64arg;
    btn_64arg.tileset_up = 18;
    btn_64arg.tileset_down = 19;
    btn_64arg.tileset_disabled = 30;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    btn_64arg.xpos = _screenSize.x * 0.25;
    btn_64arg.ypos = _screenSize.y * 0.53125;
    btn_64arg.width = _screenSize.x * 0.125;
    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
    btn_64arg.caption2.clear();
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.upCode = 1350;
    btn_64arg.downCode = 1251;
    btn_64arg.button_id = 1300;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->confirm_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = _screenSize.x * 0.625;
        btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
        btn_64arg.upCode = 1351;
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1301;
        btn_64arg.downCode = 1251;
        btn_64arg.pressedCode = 0;

        if ( _GameShell->confirm_button->Add(&btn_64arg) )
        {
            btn_64arg.tileset_down = 16;
            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.tileset_up = 16;
            btn_64arg.tileset_disabled = 16;
            btn_64arg.xpos = _screenSize.x * 0.25;
            btn_64arg.ypos = _screenSize.y * 0.4375;
            btn_64arg.caption = " ";
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.upCode = 0;
            btn_64arg.pressedCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
            btn_64arg.button_id = 1302;
            btn_64arg.width = _screenSize.x * 0.5;
            btn_64arg.txt_r = _iniColors[60].r;
            btn_64arg.txt_g = _iniColors[60].g;
            btn_64arg.txt_b = _iniColors[60].b;

            if ( _GameShell->confirm_button->Add(&btn_64arg) )
            {
                btn_64arg.button_id = 1303;
                btn_64arg.ypos = _screenSize.y * 0.46875;
                btn_64arg.caption = " ";
                btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                btn_64arg.caption2.clear();

                _GameShell->confirm_button->Add(&btn_64arg);
            }
        }
    }
    NC_STACK_button::WidgetState btnState;
    btnState.state = 0;
    btnState.butID = 1300;
    _GameShell->confirm_button->Disable(&btnState);

    btnState.butID = 1301;
    _GameShell->confirm_button->Disable(&btnState);

    _GameShell->confirm_button->HideScreen();
    return true;
}
bool NC_STACK_ypaworld::CreateInputControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    
    GuiList::tInit args;
    args.resizeable = false;
    args.numEntries = 45;
    args.shownEntries = 8;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 8;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = menuListWidth;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    if ( !_GameShell->input_listview.Init(this, args) )
    {
        ypa_log_out("Unable to create Input-ListView\n");
        return false;
    }

    _GameShell->input_listview.x = posLeftPaddingX;
    _GameShell->input_listview.y = scaledFontHeight + (vertMenuSpace + _fontH) * 4;

    _GameShell->button_input_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}});
    if ( !_GameShell->button_input_button )
    {
        ypa_log_out("Unable to create Input-Button\n");
        return false;
    }
    int allAdded = 0;
    NC_STACK_button::WidgetDesc btn_64arg;

    allAdded = 0;
    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.tileset_disabled = 16;
    btn_64arg.xpos = 0;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.ypos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TITLE);
    btn_64arg.downCode = 0;
    btn_64arg.caption2.clear();
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.button_id = 1057;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->button_input_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT2);
        btn_64arg.caption2.clear();
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1058;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->button_input_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.pressedCode = 0;
            btn_64arg.button_id = 1059;

            if ( _GameShell->button_input_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (vertMenuSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.pressedCode = 0;
                btn_64arg.button_id = 1060;

                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 19;
                    btn_64arg.tileset_disabled = 30;
                    btn_64arg.tileset_up = 18;
                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                    btn_64arg.xpos = menuWidth / 6;
                    btn_64arg.caption = "g";
                    btn_64arg.caption2 = "g";
                    btn_64arg.upCode = 1051;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.flags = 0;
                    btn_64arg.ypos = 6 * vertMenuSpace + 14 * _fontH;
                    btn_64arg.width = checkBoxWidth;
                    btn_64arg.downCode = 1050;
                    btn_64arg.button_id = 1050;

                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                    {
                        btn_64arg.tileset_down = 16;
                        btn_64arg.tileset_up = 16;
                        btn_64arg.tileset_disabled = 16;
                        btn_64arg.xpos = (checkBoxWidth + buttonsSpace + menuWidth / 6);
                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                        btn_64arg.width = (menuWidth / 2 - buttonsSpace);
                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_JOYSTICK);
                        btn_64arg.button_id = 2;
                        btn_64arg.caption2.clear();
                        btn_64arg.downCode = 0;
                        btn_64arg.upCode = 0;
                        btn_64arg.pressedCode = 0;
                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                        btn_64arg.txt_r = _iniColors[60].r;
                        btn_64arg.txt_g = _iniColors[60].g;
                        btn_64arg.txt_b = _iniColors[60].b;

                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                        {
                            btn_64arg.tileset_down = 19;
                            btn_64arg.tileset_disabled = 30;
                            btn_64arg.tileset_up = 18;
                            btn_64arg.caption = "g";
                            btn_64arg.caption2 = "g";
                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                            btn_64arg.xpos = buttonsSpace + (menuWidth / 2);
                            btn_64arg.width = checkBoxWidth;
                            btn_64arg.downCode = 1058;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.button_id = 1061;
                            btn_64arg.upCode = 1059;
                            btn_64arg.flags = 0;

                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
                            {
                                btn_64arg.tileset_down = 16;
                                btn_64arg.tileset_up = 16;
                                btn_64arg.tileset_disabled = 16;
                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                btn_64arg.xpos = (checkBoxWidth + (menuWidth / 2) + 2 * buttonsSpace);
                                btn_64arg.width = ((menuWidth / 2) - buttonsSpace);
                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_ALTJOYMODEL);
                                btn_64arg.caption2.clear();
                                btn_64arg.downCode = 0;
                                btn_64arg.upCode = 0;
                                btn_64arg.pressedCode = 0;
                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                btn_64arg.button_id = 2;
                                btn_64arg.txt_r = _iniColors[60].r;
                                btn_64arg.txt_g = _iniColors[60].g;
                                btn_64arg.txt_b = _iniColors[60].b;

                                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.tileset_down = 19;
                                    btn_64arg.tileset_disabled = 30;
                                    btn_64arg.tileset_up = 18;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                    btn_64arg.xpos = menuWidth / 3;
                                    btn_64arg.caption = "g";
                                    btn_64arg.caption2 = "g";
                                    btn_64arg.upCode = 1055;
                                    btn_64arg.button_id = 1055;
                                    btn_64arg.ypos = 7 * vertMenuSpace + (15 * _fontH);
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.width = checkBoxWidth;
                                    btn_64arg.flags = 0;
                                    btn_64arg.downCode = 1056;

                                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.tileset_down = 16;
                                        btn_64arg.tileset_up = 16;
                                        btn_64arg.tileset_disabled = 16;
                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                        btn_64arg.xpos = (checkBoxWidth + (menuWidth / 3) + buttonsSpace);
                                        btn_64arg.width = menuWidth / 2;
                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_FF);
                                        btn_64arg.button_id = 2;
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 0;
                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                        btn_64arg.upCode = 0;
                                        btn_64arg.pressedCode = 0;

                                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.tileset_down = 19;
                                            btn_64arg.tileset_up = 18;
                                            btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                            btn_64arg.tileset_disabled = 30;
                                            btn_64arg.xpos = menuWidth / 6;
                                            btn_64arg.ypos = 5 * vertMenuSpace + 13 * _fontH;
                                            btn_64arg.width = (menuWidth / 3 - buttonsSpace);
                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_DELETE);
                                            btn_64arg.downCode = 1251;
                                            btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                            btn_64arg.caption2.clear();
                                            btn_64arg.pressedCode = 0;
                                            btn_64arg.upCode = 1057;
                                            btn_64arg.button_id = 1056;
                                            btn_64arg.txt_r = _iniColors[68].r;
                                            btn_64arg.txt_g = _iniColors[68].g;
                                            btn_64arg.txt_b = _iniColors[68].b;

                                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.xpos = buttonsSpace + menuWidth / 2;
                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_RESETDEF);
                                                btn_64arg.caption2.clear();
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.upCode = 1053;
                                                btn_64arg.button_id = 1053;

                                                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.xpos = 0;
                                                    btn_64arg.ypos = bottomButtonsY;
                                                    btn_64arg.width = button1LineWidth;
                                                    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                                    btn_64arg.caption2.clear();
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.button_id = 1051;
                                                    btn_64arg.upCode = 1052;
                                                    btn_64arg.downCode = 1251;

                                                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.xpos = bottomThirdBtnPosX;
                                                        btn_64arg.ypos = bottomButtonsY;
                                                        btn_64arg.width = button1LineWidth;
                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                                        btn_64arg.upCode = 1250;
                                                        btn_64arg.caption2.clear();
                                                        btn_64arg.button_id = 1052;
                                                        btn_64arg.pressedCode = 0;

                                                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.xpos = bottomSecondBtnPosX;
                                                            btn_64arg.ypos = bottomButtonsY;
                                                            btn_64arg.width = button1LineWidth;
                                                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                            btn_64arg.upCode = 1054;
                                                            btn_64arg.button_id = 1054;
                                                            btn_64arg.caption2.clear();
                                                            btn_64arg.pressedCode = 0;

                                                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                            {
                                                                allAdded = 1;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if ( !allAdded )
    {
        ypa_log_out("Unable to add input-button\n");
        return false;
    }

    _GameShell->button_input_button->HideScreen();
    return true;
}
bool NC_STACK_ypaworld::CreateVideoControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    
    int devIndex = 0;
    int curDevIndex = 0;

    const std::vector<GFX::TGFXDeviceInfo> &devices = GFX::Engine.GetDevices();
    
    for ( const GFX::TGFXDeviceInfo &dev : devices )
    {
        if ( dev.isChosen )
        {
            _GameShell->win3d_guid = dev.guid;
            _GameShell->win3d_name = dev.Caption();

            curDevIndex = devIndex;
            break;
        }
        devIndex++;
    }

    int rowWidth = menuWidth - 3 * buttonsSpace - _fontVBScrollW;
    int devListWidth = (menuWidth - 3 * buttonsSpace - _fontVBScrollW) * 0.6;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = devices.size();
    args.shownEntries = 4;
    args.firstShownEntry = 0;
    args.selectedEntry = curDevIndex;
    args.maxShownEntries = 4;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = devListWidth;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = true;
    args.keyboardInput = true;

    if ( !_GameShell->d3d_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create D3D-Menu\n");
        return false;
    }

    _GameShell->video_button = Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}});

    if ( !_GameShell->video_button )
    {
        ypa_log_out("Unable to create Video-Button\n");
        return false;
    }

    int labelWidth = rowWidth * 0.4;
    int devListX = posLeftPaddingX + buttonsSpace + labelWidth;

    _GameShell->d3d_listvw.x = devListX;
    _GameShell->d3d_listvw.y = 7 * vertMenuSpace + 7 * _fontH + scaledFontHeight;

    int allAdded = 0;
    NC_STACK_button::WidgetDesc btn_64arg;

    allAdded = 0;

    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.tileset_disabled = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.ypos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1168;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->video_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.width = menuWidth;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_RNDTXT2);
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1169;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->video_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.width = menuWidth;
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_RNDTXT3);
            btn_64arg.button_id = 1170;
            btn_64arg.caption2.clear();

            if ( _GameShell->video_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.width = menuWidth;
                btn_64arg.ypos = 3 * (_fontH + vertMenuSpace);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_RNDTXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1171;

                if ( _GameShell->video_button->Add(&btn_64arg) )
                {
                            btn_64arg.tileset_down = 16;
                            btn_64arg.tileset_up = 16;
                            btn_64arg.tileset_disabled = 16;
                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                            btn_64arg.xpos = 0;
                            btn_64arg.ypos = 2 * (3 * (vertMenuSpace + _fontH));
                            btn_64arg.width = rowWidth * 0.4;
                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_RENDERER);
                            btn_64arg.caption2.clear();
                            btn_64arg.downCode = 0;
                            btn_64arg.upCode = 0;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_TEXT;
                            btn_64arg.button_id = 2;
                            btn_64arg.txt_r = _iniColors[60].r;
                            btn_64arg.txt_g = _iniColors[60].g;
                            btn_64arg.txt_b = _iniColors[60].b;

                            if ( _GameShell->video_button->Add(&btn_64arg) )
                            {
                                btn_64arg.width = rowWidth * 0.6;
                                btn_64arg.tileset_down = 19;
                                btn_64arg.tileset_disabled = 30;
                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                btn_64arg.downCode = 1134;
                                btn_64arg.upCode = 1135;
                                btn_64arg.tileset_up = 18;
                                btn_64arg.caption2.clear();
                                btn_64arg.pressedCode = 0;
                                btn_64arg.xpos = buttonsSpace + rowWidth * 0.4;
                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                btn_64arg.caption = _GameShell->win3d_name;
                                btn_64arg.button_id = 1172;
                                btn_64arg.txt_r = _iniColors[68].r;
                                btn_64arg.txt_g = _iniColors[68].g;
                                btn_64arg.txt_b = _iniColors[68].b;

                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                {
                                    int captionsWidth = videoMenuRowWidth - 6 * buttonsSpace - 2 * checkBoxWidth;
                                    int captionWidth = captionsWidth / 2;

                                    // The options' captions
                                    btn_64arg.txt_r = _iniColors[60].r;
                                    btn_64arg.txt_g = _iniColors[60].g;
                                    btn_64arg.txt_b = _iniColors[60].b;

                                    btn_64arg.tileset_down = 19;
                                    btn_64arg.tileset_up = 18;
                                    btn_64arg.width = checkBoxWidth;
                                    btn_64arg.caption = "g";
                                    btn_64arg.caption2 = "g";
                                    btn_64arg.tileset_disabled = 30;
                                    btn_64arg.upCode = 1107;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                    btn_64arg.downCode = 1106;
                                    btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + captionWidth;
                                    btn_64arg.ypos = 7 * (vertMenuSpace + _fontH);
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.flags = 0;
                                    btn_64arg.button_id = 1160;

                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.tileset_down = 16;
                                        btn_64arg.tileset_up = 16;
                                        btn_64arg.tileset_disabled = 16;
                                        btn_64arg.width = captionWidth;
                                        btn_64arg.xpos = 4 * buttonsSpace + captionWidth + 2 * checkBoxWidth;
                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SKY);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 0;
                                        btn_64arg.upCode = 0;
                                        btn_64arg.pressedCode = 0;
                                        btn_64arg.button_id = 2;
                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.width = checkBoxWidth;
                                            btn_64arg.tileset_down = 19;
                                            btn_64arg.tileset_up = 18;
                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                            btn_64arg.tileset_disabled = 30;
                                            btn_64arg.downCode = 1130;
                                            btn_64arg.pressedCode = 0;
                                            btn_64arg.flags = 0;
                                            btn_64arg.caption = "g";
                                            btn_64arg.caption2 = "g";
                                            btn_64arg.upCode = 1131;
                                            btn_64arg.button_id = 1166;
                                            btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + captionWidth;
                                            btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);

                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.tileset_down = 16;
                                                btn_64arg.tileset_up = 16;
                                                btn_64arg.tileset_disabled = 16;
                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                btn_64arg.xpos = 4 * buttonsSpace + captionWidth + 2 * checkBoxWidth;
                                                btn_64arg.width = captionWidth;
                                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_WINDOWEDMODE);
                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                btn_64arg.caption2.clear();
                                                btn_64arg.downCode = 0;
                                                btn_64arg.upCode = 0;
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.button_id = 2;

                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.tileset_down = 16;
                                                    btn_64arg.tileset_up = 16;
                                                    btn_64arg.tileset_disabled = 16;
                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                    btn_64arg.xpos = captionWidth + 2 * checkBoxWidth + 4 * buttonsSpace;
                                                    btn_64arg.ypos = 9 * (vertMenuSpace + _fontH);
                                                    btn_64arg.width = captionWidth;
                                                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_ENCDAUD);
                                                    btn_64arg.caption2.clear();
                                                    btn_64arg.downCode = 0;
                                                    btn_64arg.upCode = 0;
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.button_id = 0;
                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.width = checkBoxWidth;
                                                        btn_64arg.tileset_down = 19;
                                                        btn_64arg.tileset_up = 18;
                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                        btn_64arg.tileset_disabled = 30;
                                                        btn_64arg.downCode = 1128;
                                                        btn_64arg.pressedCode = 0;
                                                        btn_64arg.flags = 0;
                                                        btn_64arg.caption = "g";
                                                        btn_64arg.caption2 = "g";
                                                        btn_64arg.upCode = 1129;
                                                        btn_64arg.button_id = 1164;
                                                        btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + captionWidth;

                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.tileset_down = 19;
                                                            btn_64arg.tileset_disabled = 30;
                                                            btn_64arg.tileset_up = 18;
                                                            btn_64arg.xpos = 0;
                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                            btn_64arg.width = checkBoxWidth;
                                                            btn_64arg.caption = "g";
                                                            btn_64arg.caption2 = "g";
                                                            btn_64arg.downCode = 1126;
                                                            btn_64arg.pressedCode = 0;
                                                            btn_64arg.button_id = 1163;
                                                            btn_64arg.upCode = 1127;
                                                            btn_64arg.flags = 0;

                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                            {
                                                                btn_64arg.tileset_down = 16;
                                                                btn_64arg.tileset_up = 16;
                                                                btn_64arg.tileset_disabled = 16;
                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                                btn_64arg.width = captionWidth - checkBoxWidth;
                                                                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_ENMYINDIC);
                                                                btn_64arg.caption2.clear();
                                                                btn_64arg.downCode = 0;
                                                                btn_64arg.upCode = 0;
                                                                btn_64arg.pressedCode = 0;
                                                                btn_64arg.button_id = 0;
                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                {
                                                                    btn_64arg.tileset_down = 16;
                                                                    btn_64arg.tileset_up = 16;
                                                                    btn_64arg.tileset_disabled = 16;
                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                    btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                                    btn_64arg.ypos = 10 * (vertMenuSpace + _fontH);
                                                                    btn_64arg.width = captionWidth;
                                                                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_INVERT);
                                                                    btn_64arg.caption2.clear();
                                                                    btn_64arg.downCode = 0;
                                                                    btn_64arg.upCode = 0;
                                                                    btn_64arg.pressedCode = 0;
                                                                    btn_64arg.button_id = 0;
                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                    {
                                                                        btn_64arg.width = checkBoxWidth;
                                                                        btn_64arg.tileset_down = 19;
                                                                        btn_64arg.tileset_up = 18;
                                                                        btn_64arg.pressedCode = 0;
                                                                        btn_64arg.flags = 0;
                                                                        btn_64arg.caption = "g";
                                                                        btn_64arg.caption2 = "g";
                                                                        btn_64arg.tileset_disabled = 30;
                                                                        btn_64arg.button_id = 1151;
                                                                        btn_64arg.xpos = 0;
                                                                        btn_64arg.downCode = 1112;
                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                        btn_64arg.upCode = 1111;

                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                        {
                                                                            btn_64arg.tileset_down = 16;
                                                                            btn_64arg.tileset_up = 16;
                                                                            btn_64arg.tileset_disabled = 16;
                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                            btn_64arg.xpos = 0;
                                                                            btn_64arg.ypos = 11 * (_fontH + vertMenuSpace);
                                                                            btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.3;
                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_DESTRFX);
                                                                            btn_64arg.caption2.clear();
                                                                            btn_64arg.downCode = 0;
                                                                            btn_64arg.upCode = 0;
                                                                            btn_64arg.pressedCode = 0;
                                                                            btn_64arg.button_id = 2;
                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                            {
                                                                                NC_STACK_button::Slider slider;

                                                                                slider.value = 8;
                                                                                slider.max = 16;
                                                                                slider.min = 0;

                                                                                btn_64arg.caption2.clear();
                                                                                btn_64arg.tileset_down = 18;
                                                                                btn_64arg.tileset_up = 18;
                                                                                btn_64arg.tileset_disabled = 30;
                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                btn_64arg.pressedCode = 1110;
                                                                                btn_64arg.button_id = 1159;
                                                                                btn_64arg.xpos = buttonsSpace + (videoMenuRowWidth - 5 * buttonsSpace) * 0.3;
                                                                                btn_64arg.caption = " ";
                                                                                btn_64arg.downCode = 1108;
                                                                                btn_64arg.flags = 0;
                                                                                btn_64arg.slider = &slider;
                                                                                btn_64arg.upCode = 1109;
                                                                                btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.55;

                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                {
                                                                                    btn_64arg.tileset_down = 16;
                                                                                    btn_64arg.tileset_up = 16;
                                                                                    btn_64arg.tileset_disabled = 16;
                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                    btn_64arg.caption2.clear();
                                                                                    btn_64arg.xpos = buttonsSpace + (videoMenuRowWidth - 5 * buttonsSpace) * 0.85;
                                                                                    btn_64arg.downCode = 0;
                                                                                    btn_64arg.upCode = 0;
                                                                                    btn_64arg.pressedCode = 0;
                                                                                    btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.15;
                                                                                    btn_64arg.button_id = 1158;
                                                                                    btn_64arg.caption = " 4";
                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;

                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                    {
                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                        btn_64arg.xpos = 0;
                                                                                        btn_64arg.ypos = 13 * (vertMenuSpace + _fontH);
                                                                                        btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.3;
                                                                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_FXVOL);
                                                                                        btn_64arg.caption2.clear();
                                                                                        btn_64arg.downCode = 0;
                                                                                        btn_64arg.upCode = 0;
                                                                                        btn_64arg.pressedCode = 0;
                                                                                        btn_64arg.button_id = 2;
                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                        {
                                                                                            btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.55;

                                                                                            slider.min = 1;
                                                                                            slider.max = 127;
                                                                                            slider.value = 100;

                                                                                            btn_64arg.tileset_disabled = 30;
                                                                                            btn_64arg.tileset_down = 18;
                                                                                            btn_64arg.tileset_up = 18;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.button_id = 1152;
                                                                                            btn_64arg.xpos = buttonsSpace + (videoMenuRowWidth - 5 * buttonsSpace) * 0.3;
                                                                                            btn_64arg.caption = " ";
                                                                                            btn_64arg.downCode = 1115;
                                                                                            btn_64arg.upCode = 1117;
                                                                                            btn_64arg.slider = &slider;
                                                                                            btn_64arg.pressedCode = 1116;
                                                                                            btn_64arg.flags = 0;

                                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                btn_64arg.tileset_down = 16;
                                                                                                btn_64arg.tileset_up = 16;
                                                                                                btn_64arg.tileset_disabled = 16;
                                                                                                btn_64arg.caption = "4";
                                                                                                btn_64arg.button_id = 1153;
                                                                                                btn_64arg.caption2.clear();
                                                                                                btn_64arg.xpos = (2 * buttonsSpace) + (videoMenuRowWidth - 5 * buttonsSpace) * 0.85;
                                                                                                btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.15;
                                                                                                btn_64arg.downCode = 0;
                                                                                                btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                                                                btn_64arg.upCode = 0;
                                                                                                btn_64arg.pressedCode = 0;

                                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                {
                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                    btn_64arg.xpos = 0;
                                                                                                    btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.3;
                                                                                                    btn_64arg.ypos = 14 * (vertMenuSpace + _fontH);
                                                                                                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_CDVOL);
                                                                                                    btn_64arg.caption2.clear();
                                                                                                    btn_64arg.downCode = 0;
                                                                                                    btn_64arg.upCode = 0;
                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                    btn_64arg.button_id = 2;

                                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                    {
                                                                                                        btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.55;
                                                                                                        slider.min = 1;
                                                                                                        slider.max = 127;
                                                                                                        slider.value = 100;

                                                                                                        btn_64arg.tileset_down = 18;
                                                                                                        btn_64arg.tileset_up = 18;
                                                                                                        btn_64arg.upCode = 1120;
                                                                                                        btn_64arg.tileset_disabled = 30;
                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                                        btn_64arg.caption2.clear();
                                                                                                        btn_64arg.downCode = 1118;
                                                                                                        btn_64arg.xpos = buttonsSpace + (videoMenuRowWidth - 5 * buttonsSpace) * 0.3;
                                                                                                        btn_64arg.caption = " ";
                                                                                                        btn_64arg.pressedCode = 1119;
                                                                                                        btn_64arg.slider = &slider;
                                                                                                        btn_64arg.flags = 0;
                                                                                                        btn_64arg.button_id = 1154;

                                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                        {
                                                                                                            btn_64arg.tileset_down = 16;
                                                                                                            btn_64arg.tileset_up = 16;
                                                                                                            btn_64arg.tileset_disabled = 16;
                                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                            btn_64arg.caption = "4";
                                                                                                            btn_64arg.caption2.clear();
                                                                                                            btn_64arg.downCode = 0;
                                                                                                            btn_64arg.upCode = 0;
                                                                                                            btn_64arg.pressedCode = 0;
                                                                                                            btn_64arg.xpos = (2 * buttonsSpace) + (videoMenuRowWidth - 5 * buttonsSpace) * 0.85;
                                                                                                            btn_64arg.width = (videoMenuRowWidth - 5 * buttonsSpace) * 0.15;
                                                                                                            btn_64arg.button_id = 1155;
                                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;

                                                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                            {
                                                                                                                btn_64arg.tileset_up = 18;
                                                                                                                btn_64arg.tileset_disabled = 30;
                                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                                                                                btn_64arg.xpos = 0;
                                                                                                                btn_64arg.ypos = bottomButtonsY;
                                                                                                                btn_64arg.width = button1LineWidth;
                                                                                                                btn_64arg.tileset_down = 19;
                                                                                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                                                                                                btn_64arg.upCode = 1124;
                                                                                                                btn_64arg.caption2.clear();
                                                                                                                btn_64arg.downCode = 0;
                                                                                                                btn_64arg.pressedCode = 0;
                                                                                                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                                                                                btn_64arg.button_id = 1161;
                                                                                                                btn_64arg.txt_r = _iniColors[68].r;
                                                                                                                btn_64arg.txt_g = _iniColors[68].g;
                                                                                                                btn_64arg.txt_b = _iniColors[68].b;

                                                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                {
                                                                                                                    btn_64arg.xpos = bottomThirdBtnPosX;
                                                                                                                    btn_64arg.ypos = bottomButtonsY;
                                                                                                                    btn_64arg.width = button1LineWidth;
                                                                                                                    btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                                                                                                    btn_64arg.upCode = 1250;
                                                                                                                    btn_64arg.caption2.clear();
                                                                                                                    btn_64arg.downCode = 0;
                                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                                    btn_64arg.button_id = 1167;

                                                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                    {
                                                                                                                        btn_64arg.xpos = bottomSecondBtnPosX;
                                                                                                                        btn_64arg.ypos = bottomButtonsY;
                                                                                                                        btn_64arg.width = button1LineWidth;
                                                                                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                                                                                        btn_64arg.upCode = 1125;
                                                                                                                        btn_64arg.caption2.clear();
                                                                                                                        btn_64arg.downCode = 0;
                                                                                                                        btn_64arg.pressedCode = 0;
                                                                                                                        btn_64arg.button_id = 1162;

                                                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                        {
                                                                                                                            allAdded = 1;
                                                                                                                        }
                                                                                                                    }
                                                                                                                }
                                                                                                            }
                                                                                                        }
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                }
            }
        }
    }
    // Vertical sync, which the port adds where the original had a software
    // cursor option, left of windowed mode
    if ( allAdded )
    {
        allAdded = 0;

        btn_64arg.width = checkBoxWidth;
        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
        btn_64arg.tileset_disabled = 30;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 8 * (vertMenuSpace + _fontH);
        btn_64arg.caption = "g";
        btn_64arg.caption2 = "g";
        btn_64arg.downCode = 1136;
        btn_64arg.upCode = 1137;
        btn_64arg.pressedCode = 0;
        btn_64arg.flags = 0;
        btn_64arg.button_id = 1173;

        if ( _GameShell->video_button->Add(&btn_64arg) )
        {
            btn_64arg.tileset_down = 16;
            btn_64arg.tileset_up = 16;
            btn_64arg.tileset_disabled = 16;
            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.xpos = checkBoxWidth + buttonsSpace;
            btn_64arg.width = (videoMenuRowWidth - 6 * buttonsSpace - 2 * checkBoxWidth) / 2;
            btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_VSYNC);
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.upCode = 0;
            btn_64arg.pressedCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
            btn_64arg.button_id = 2;
            // The other options' caption color, not the OK button's set last
            btn_64arg.txt_r = _iniColors[60].r;
            btn_64arg.txt_g = _iniColors[60].g;
            btn_64arg.txt_b = _iniColors[60].b;

            if ( _GameShell->video_button->Add(&btn_64arg) )
                allAdded = 1;
        }
    }

    // Options the port adds from nucleus.ini, which has their defaults:
    // check boxes in free places among the original ones, and sliders
    // between the original ones.
    {
        int captionWidth = (videoMenuRowWidth - 6 * buttonsSpace - 2 * checkBoxWidth) / 2;
        int sliderRowWidth = videoMenuRowWidth - 5 * buttonsSpace;

        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        // A check box and its caption, in the left or the right column
        auto addCheckBox = [&](int row, bool right, int id, int onCode, int offCode, const std::string &caption)
        {
            int x = right ? 3 * buttonsSpace + checkBoxWidth + captionWidth : 0;

            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
            btn_64arg.tileset_down = 19;
            btn_64arg.tileset_up = 18;
            btn_64arg.tileset_disabled = 30;
            btn_64arg.xpos = x;
            btn_64arg.ypos = row * (vertMenuSpace + _fontH);
            btn_64arg.width = checkBoxWidth;
            btn_64arg.caption = "g";
            btn_64arg.caption2 = "g";
            btn_64arg.downCode = onCode;
            btn_64arg.upCode = offCode;
            btn_64arg.pressedCode = 0;
            btn_64arg.flags = 0;
            btn_64arg.button_id = id;

            if ( !_GameShell->video_button->Add(&btn_64arg) )
                return false;

            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.tileset_down = 16;
            btn_64arg.tileset_up = 16;
            btn_64arg.tileset_disabled = 16;
            btn_64arg.xpos = x + checkBoxWidth + buttonsSpace;
            btn_64arg.width = captionWidth;
            btn_64arg.caption = caption;
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.upCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
            btn_64arg.button_id = 2;

            return _GameShell->video_button->Add(&btn_64arg) != 0;
        };

        // A caption, a slider and a caption with the slider's value
        auto addSlider = [&](int row, int id, int valueId, int code, int min, int max, const std::string &caption)
        {
            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.tileset_down = 16;
            btn_64arg.tileset_up = 16;
            btn_64arg.tileset_disabled = 16;
            btn_64arg.xpos = 0;
            btn_64arg.ypos = row * (vertMenuSpace + _fontH);
            btn_64arg.width = sliderRowWidth * 0.3;
            btn_64arg.caption = caption;
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.upCode = 0;
            btn_64arg.pressedCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
            btn_64arg.button_id = 2;

            if ( !_GameShell->video_button->Add(&btn_64arg) )
                return false;

            NC_STACK_button::Slider slider;
            slider.min = min;
            slider.max = max;
            slider.value = min;

            btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
            btn_64arg.tileset_down = 18;
            btn_64arg.tileset_up = 18;
            btn_64arg.tileset_disabled = 30;
            btn_64arg.xpos = buttonsSpace + sliderRowWidth * 0.3;
            btn_64arg.width = sliderRowWidth * 0.55;
            btn_64arg.caption = " ";
            btn_64arg.downCode = code;
            btn_64arg.slider = &slider;
            btn_64arg.flags = 0;
            btn_64arg.button_id = id;

            if ( !_GameShell->video_button->Add(&btn_64arg) )
                return false;

            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.tileset_down = 16;
            btn_64arg.tileset_up = 16;
            btn_64arg.tileset_disabled = 16;
            btn_64arg.xpos = 2 * buttonsSpace + sliderRowWidth * 0.85;
            btn_64arg.width = sliderRowWidth * 0.15;
            btn_64arg.caption = " ";
            btn_64arg.downCode = 0;
            btn_64arg.slider = NULL;
            btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
            btn_64arg.button_id = valueId;

            return _GameShell->video_button->Add(&btn_64arg) != 0;
        };

        allAdded = allAdded
            && addCheckBox(10, true, 1174, 1138, 1139, Locale::Text::Dialogs(Locale::DLG_S_FILTERING))
            && addCheckBox(7, false, 1175, 1140, 1141, Locale::Text::Dialogs(Locale::DLG_S_MOVIES))
            && addSlider(12, 1177, 1178, 1144, 0, UserData::FpsLimits.size() - 1, Locale::Text::Dialogs(Locale::DLG_S_MAXFPS))
            && addSlider(15, 1179, 1180, 1145, 1, AUDIO_CHANNELS, Locale::Text::Dialogs(Locale::DLG_S_CHANNELS))
            && addSlider(16, 1181, 1182, 1146, 1, 30, Locale::Text::Dialogs(Locale::DLG_S_AITIMELINE)); // Minutes
    }

    if ( !allAdded )
    {
        ypa_log_out("Unable to add video-button\n");
        return false;
    }

    NC_STACK_button::WidgetState btnState;
    btnState.butID = 1151;
    btnState.state = ((_GameShell->soundFlags & World::SF_INVERTLR) == 0) + 1;

    _GameShell->video_button->SetState(&btnState);


    _GameShell->video_button->HideScreen();
    return true;
}
bool NC_STACK_ypaworld::CreateDiskControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    
    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = _GameShell->profiles.size();
    args.shownEntries = 10;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 10;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = menuWidth;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    if ( !_GameShell->disk_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create disk-listview\n");
        return false;
    }

    _GameShell->disk_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}} );

    if ( !_GameShell->disk_button )
    {
        ypa_log_out("Unable to create disk-buttonobject\n");
        return false;
    }

    _GameShell->disk_listvw.x = posLeftPaddingX;
    _GameShell->disk_listvw.y = 4 * (vertMenuSpace + _fontH) + scaledFontHeight;

    _GameShell->userNameDir = _GameShell->UserName;

    _GameShell->userNameDirCursor = _GameShell->userNameDir.size();

    std::string nameCaption = _GameShell->userNameDir;

    if ( _GameShell->diskScreenMode )
        nameCaption += "h";

    int allAdded = 0;
    NC_STACK_button::WidgetDesc btn_64arg;

    allAdded = 0;

    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.tileset_disabled = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;

    btn_64arg.xpos = 0;
    btn_64arg.ypos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.button_id = 1108;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->disk_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = buttonsSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT2);
        btn_64arg.button_id = 1109;
        btn_64arg.caption2.clear();
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->disk_button->Add(&btn_64arg))
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + buttonsSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.button_id = 1110;

            if ( _GameShell->disk_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (buttonsSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1111;

                if ( _GameShell->disk_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 17;
                    btn_64arg.tileset_up = 17;
                    btn_64arg.tileset_disabled = 17;
                    btn_64arg.xpos = 0;
                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                    btn_64arg.width = menuWidth;
                    btn_64arg.caption2.clear();
                    btn_64arg.downCode = 0;
                    btn_64arg.upCode = 0;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.caption = nameCaption.c_str();
                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                    btn_64arg.button_id = 1100;
                    btn_64arg.ypos = 6 * buttonsSpace + 14 * _fontH;

                    if ( _GameShell->disk_button->Add(&btn_64arg) )
                    {
                        btn_64arg.tileset_down = 19;
                        btn_64arg.tileset_up = 18;
                        btn_64arg.tileset_disabled = 30;
                        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                        btn_64arg.xpos = buttonsSpace + (menuWidth - 3 * buttonsSpace) * 0.25;
                        btn_64arg.ypos = 7 * buttonsSpace + 15 * _fontH;
                        btn_64arg.width = (menuWidth - 3 * buttonsSpace) * 0.25;
                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_LOAD);
                        btn_64arg.downCode = 1251;
                        btn_64arg.upCode = 1160;
                        btn_64arg.caption2.clear();
                        btn_64arg.pressedCode = 0;
                        btn_64arg.button_id = 1101;
                        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                        btn_64arg.txt_r = _iniColors[68].r;
                        btn_64arg.txt_g = _iniColors[68].g;
                        btn_64arg.txt_b = _iniColors[68].b;

                        if ( _GameShell->disk_button->Add(&btn_64arg) )
                        {
                            btn_64arg.xpos = (3 * buttonsSpace) + (menuWidth - 3 * buttonsSpace) * 0.75;
                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_DELETE);;
                            btn_64arg.caption2.clear();
                            btn_64arg.upCode = 1161;
                            btn_64arg.button_id = 1102;

                            if ( _GameShell->disk_button->Add(&btn_64arg) )
                            {
                                btn_64arg.xpos = 0;
                                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_NEW);
                                btn_64arg.button_id = 1103;
                                btn_64arg.caption2.clear();
                                btn_64arg.upCode = 1162;

                                if ( _GameShell->disk_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.xpos = (2 * buttonsSpace) + (menuWidth - 3 * buttonsSpace) * 0.5;
                                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_SAVE);
                                    btn_64arg.button_id = 1104;
                                    btn_64arg.caption2.clear();
                                    btn_64arg.upCode = 1163;

                                    if ( _GameShell->disk_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.xpos = 0;
                                        btn_64arg.ypos = bottomButtonsY;
                                        btn_64arg.width = button1LineWidth;
                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 1251;
                                        btn_64arg.button_id = 1105;
                                        btn_64arg.pressedCode = 0;
                                        btn_64arg.upCode = 1164;

                                        if ( _GameShell->disk_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.ypos = bottomButtonsY;
                                            btn_64arg.width = button1LineWidth;
                                            btn_64arg.xpos = bottomThirdBtnPosX;
                                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                            btn_64arg.button_id = 1107;
                                            btn_64arg.caption2.clear();
                                            btn_64arg.upCode = 1250;

                                            if ( _GameShell->disk_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.ypos = bottomButtonsY;
                                                btn_64arg.width = button1LineWidth;
                                                btn_64arg.xpos = bottomSecondBtnPosX;
                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                btn_64arg.button_id = 1106;
                                                btn_64arg.caption2.clear();
                                                btn_64arg.upCode = 1165;

                                                if ( _GameShell->disk_button->Add(&btn_64arg) )
                                                {
                                                    allAdded = 1;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if ( !allAdded )
    {
        ypa_log_out("Unable to add button to disk-buttonobject\n");
        return false;
    }


    _GameShell->disk_button->HideScreen();
    NC_STACK_button::WidgetState btnState;
    btnState.state = 0;
    btnState.butID = 1105;
    _GameShell->disk_button->Disable(&btnState);
    return true;
}
bool NC_STACK_ypaworld::CreateLocaleControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    
    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = 10;
    args.shownEntries = 10;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 10;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = menuWidth - _fontVBScrollW;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;


    if ( !_GameShell->local_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create local-listview\n");
        return false;
    }

    _GameShell->locale_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}} );

    if ( !_GameShell->locale_button )
    {
        ypa_log_out("Unable to create locale-buttonobject\n");
        return false;
    }

    _GameShell->local_listvw.x = posLeftPaddingX;
    _GameShell->local_listvw.y = 4 * (vertMenuSpace + _fontH) + scaledFontHeight;

    int allAdded = 0;
    NC_STACK_button::WidgetDesc btn_64arg;

    allAdded = 0;
    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.tileset_disabled = 30;
    btn_64arg.ypos = 0;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1253;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->locale_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT2);
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1254;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->locale_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.button_id = 1255;

            if ( _GameShell->locale_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (vertMenuSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1256;

                if ( _GameShell->locale_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 19;
                    btn_64arg.tileset_up = 18;
                    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                    btn_64arg.tileset_disabled = 30;
                    btn_64arg.xpos = 0;
                    btn_64arg.ypos = bottomButtonsY;
                    btn_64arg.width = button1LineWidth;
                    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                    btn_64arg.caption2.clear();
                    btn_64arg.downCode = 0;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.upCode = 1300;
                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                    btn_64arg.button_id = 1250;
                    btn_64arg.txt_r = _iniColors[68].r;
                    btn_64arg.txt_g = _iniColors[68].g;
                    btn_64arg.txt_b = _iniColors[68].b;

                    if ( _GameShell->locale_button->Add(&btn_64arg) )
                    {
                        btn_64arg.xpos = bottomThirdBtnPosX;
                        btn_64arg.ypos = bottomButtonsY;
                        btn_64arg.width = button1LineWidth;
                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                        btn_64arg.button_id = 1252;
                        btn_64arg.caption2.clear();
                        btn_64arg.downCode = 0;
                        btn_64arg.upCode = 1250;
                        btn_64arg.pressedCode = 0;

                        if ( _GameShell->locale_button->Add(&btn_64arg) )
                        {
                            btn_64arg.xpos = bottomSecondBtnPosX;
                            btn_64arg.ypos = bottomButtonsY;
                            btn_64arg.width = button1LineWidth;
                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                            btn_64arg.caption2.clear();
                            btn_64arg.downCode = 0;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.upCode = 1301;
                            btn_64arg.button_id = 1251;

                            if ( _GameShell->locale_button->Add(&btn_64arg) )
                                allAdded = 1;
                        }
                    }
                }
            }
        }
    }
    if ( !allAdded )
    {
        ypa_log_out("Unable to add locale-button\n");
        return false;
    }

    _GameShell->locale_button->HideScreen();
    return true;
}
bool NC_STACK_ypaworld::CreateAboutControls(){
    _GameShell->about_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - 0)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}} );

    if ( !_GameShell->about_button )
    {
        ypa_log_out("Unable to create sound-buttonobject\n");
        return false;
    }
    int allAdded = 0;
    NC_STACK_button::WidgetDesc btn_64arg;
    allAdded = 0;
    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.tileset_disabled = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.width = (_screenSize.x - 4 * buttonsSpace);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.button_id = 2;
    btn_64arg.ypos = vertMenuSpace + _fontH;
    btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.caption = "Fuer den Kauf dieses erzgebirgischen Qualitaetsspielzeuges bedanken sich";
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->about_button->Add(&btn_64arg) )
    {
        btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
        btn_64arg.caption = "Bernd Beyreuther,";

        if ( _GameShell->about_button->Add(&btn_64arg) )
        {
            btn_64arg.ypos = 3 * (vertMenuSpace + _fontH);
            btn_64arg.caption = "Andre 'Floh' Weissflog, Andreas Flemming,";

            if ( _GameShell->about_button->Add(&btn_64arg) )
            {
                btn_64arg.ypos = 4 * (_fontH + vertMenuSpace);
                btn_64arg.caption = "Stefan 'Metzel Hetzel' Karau, Sylvius Lack,";

                if ( _GameShell->about_button->Add(&btn_64arg) )
                {
                    btn_64arg.ypos = 5 * (vertMenuSpace + _fontH);
                    btn_64arg.caption = "Dietmar 'Didi' Koebelin, Nico Nitsch, Steffen Priebus, ";

                    if ( _GameShell->about_button->Add(&btn_64arg) )
                    {
                        btn_64arg.ypos = 6 * (_fontH + vertMenuSpace);
                        btn_64arg.caption = "Stefan Warias, Henrik Volkening und";

                        if ( _GameShell->about_button->Add(&btn_64arg) )
                        {
                            btn_64arg.ypos = 7 * (vertMenuSpace + _fontH);
                            btn_64arg.caption = "Uta Kapp";

                            if ( _GameShell->about_button->Add(&btn_64arg) )
                            {
                                btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
                                btn_64arg.caption = " ";

                                if ( _GameShell->about_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.ypos = 9 * (_fontH + vertMenuSpace);
                                    btn_64arg.caption = "Unser Dank gilt:";

                                    if ( _GameShell->about_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.ypos = 10 * (_fontH + vertMenuSpace);
                                        btn_64arg.caption = "dem gesamten Microsoft Team, besonders";

                                        if ( _GameShell->about_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.ypos = 11 * (vertMenuSpace + _fontH);
                                            btn_64arg.caption = "Michael Lyons, Jonathan Sposato und Earnest Yuen";

                                            if ( _GameShell->about_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.ypos = 12 * (_fontH + vertMenuSpace);
                                                btn_64arg.caption = "weiterhin";

                                                if ( _GameShell->about_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.ypos = 13 * (_fontH + vertMenuSpace);
                                                    btn_64arg.caption = "Robert Birker, Andre 'Goetz' Blechschmidt, Jan Blechschmidt, Stephan Bludau,";

                                                    if ( _GameShell->about_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.ypos = 14 * (_fontH + vertMenuSpace);
                                                        btn_64arg.caption = "Andre Kunth, Markus Lorenz, Dirk Mansbart";

                                                        if ( _GameShell->about_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.ypos = 15 * (vertMenuSpace + _fontH);
                                                            btn_64arg.caption = "und natuerlich";

                                                            if ( _GameShell->about_button->Add(&btn_64arg) )
                                                            {
                                                                btn_64arg.ypos = 16 * (_fontH + vertMenuSpace);
                                                                btn_64arg.caption = "        GoldEd - dPaint - SAS/C";

                                                                if ( _GameShell->about_button->Add(&btn_64arg) )
                                                                {
                                                                    allAdded = 1;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if ( !allAdded )
    {
        ypa_log_out("Unable to add about-button\n");
        return false;
    }

    _GameShell->about_button->HideScreen();
    return true;
}
bool NC_STACK_ypaworld::CreateNetworkControls()
{
    int posLeftPaddingX = (_screenSize.x * 0.3) / 2;
    
    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = 12;
    args.shownEntries = 12;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 12;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = menuListWidth;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    if ( !_GameShell->network_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create network-listview\n");
        return false;
    }

    int nypos = scaledFontHeight - _fontH;

    _GameShell->network_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)nypos},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - nypos)}});
    if ( !_GameShell->network_button )
    {
        ypa_log_out("Unable to create network-buttonobject\n");
        return false;
    }

    _GameShell->netListY = 3 * (vertMenuSpace + _fontH) + nypos;

    _GameShell->network_listvw.x = posLeftPaddingX;
    _GameShell->network_listvw.y = _GameShell->netListY;

    int allAdded = 0;
    NC_STACK_button::WidgetDesc btn_64arg;

    btn_64arg.tileset_down = 17;
    btn_64arg.tileset_up = 17;
    btn_64arg.tileset_disabled = 17;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.caption = "???";
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.ypos = 14 * (vertMenuSpace + _fontH);
    btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.width = netMenuWidth * 0.8;
    btn_64arg.txt_r = _iniColors[60].r;
    btn_64arg.txt_g = _iniColors[60].g;
    btn_64arg.txt_b = _iniColors[60].b;


    allAdded = 0;

    if ( _GameShell->network_button->Add(&btn_64arg) )
    {
        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.xpos = buttonsSpace + netMenuWidth * 0.8;
        btn_64arg.tileset_disabled = 30;
        btn_64arg.width = netMenuWidth * 0.2 - buttonsSpace;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SEND);
        btn_64arg.caption2.clear();
        btn_64arg.upCode = 1210;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1225;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( _GameShell->network_button->Add(&btn_64arg) )
        {
            int raceSlotWidth = ((netMenuWidth - 3 * buttonsSpace) * 0.25 - 3 * buttonsSpace) * 0.25;

            TileMap *raceTiles = GFX::Engine.GetTileset(8);

            btn_64arg.tileset_down = 16;
            btn_64arg.tileset_up = 16;
            btn_64arg.tileset_disabled = 16;
            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.ypos = (15 * (vertMenuSpace + _fontH));
            btn_64arg.xpos = 0;
            btn_64arg.width = netMenuWidth * 0.4 - 2 * buttonsSpace;
            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SELRACE);
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_TEXT | NC_STACK_button::FLAG_RALIGN;
            btn_64arg.button_id = 1220;
            btn_64arg.txt_r = _iniColors[60].r;
            btn_64arg.txt_g = _iniColors[60].g;
            btn_64arg.txt_b = _iniColors[60].b;

            if ( _GameShell->network_button->Add(&btn_64arg) )
            {
                btn_64arg.tileset_down = 8;
                btn_64arg.tileset_up = 8;
                btn_64arg.tileset_disabled = 30;
                btn_64arg.button_type = 4;
                btn_64arg.xpos += btn_64arg.width + 2 * buttonsSpace;
                btn_64arg.width = raceTiles->map[65].w;
                btn_64arg.caption = "A";
                btn_64arg.caption2 = "B";
                btn_64arg.button_id = 1206;
                btn_64arg.downCode = 1204;
                btn_64arg.flags = 0;

                if ( _GameShell->network_button->Add(&btn_64arg) )
                {
                    btn_64arg.caption = "C";
                    btn_64arg.caption2 = "D";
                    btn_64arg.downCode = 1205;
                    btn_64arg.button_id = 1207;
                    btn_64arg.xpos += raceSlotWidth + buttonsSpace;

                    if ( _GameShell->network_button->Add(&btn_64arg) )
                    {
                        btn_64arg.caption = "E";
                        btn_64arg.caption2 = "F";
                        btn_64arg.downCode = 1206;
                        btn_64arg.button_id = 1208;
                        btn_64arg.xpos += raceSlotWidth + buttonsSpace;

                        if ( _GameShell->network_button->Add(&btn_64arg) )
                        {
                            btn_64arg.caption = "G";
                            btn_64arg.caption2 = "H";
                            btn_64arg.downCode = 1207;
                            btn_64arg.button_id = 1209;
                            btn_64arg.xpos += raceSlotWidth + buttonsSpace;

                            if ( _GameShell->network_button->Add(&btn_64arg) )
                            {
                                btn_64arg.tileset_down = 19;
                                btn_64arg.tileset_up = 18;
                                btn_64arg.tileset_disabled = 30;
                                btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                btn_64arg.xpos += raceSlotWidth + 2 * buttonsSpace;
                                btn_64arg.width = menuListWidth - btn_64arg.xpos;
                                btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_BACK);
                                btn_64arg.caption2.clear();
                                btn_64arg.pressedCode = 0;
                                btn_64arg.button_id = 1205;
                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                btn_64arg.upCode = 1203;
                                btn_64arg.downCode = 1251;
                                btn_64arg.txt_r = _iniColors[68].r;
                                btn_64arg.txt_g = _iniColors[68].g;
                                btn_64arg.txt_b = _iniColors[68].b;

                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.tileset_down = 16;
                                    btn_64arg.xpos = 0;
                                    btn_64arg.ypos = 0;
                                    btn_64arg.tileset_up = 16;
                                    btn_64arg.tileset_disabled = 16;
                                    btn_64arg.width = menuListWidth;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SELPROV);
                                    btn_64arg.caption2.clear();
                                    btn_64arg.downCode = 0;
                                    btn_64arg.upCode = 0;
                                    btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE;
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.xpos = 0;
                                        btn_64arg.ypos = buttonsSpace + _fontH;
                                        btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_TXT2);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1;
                                        btn_64arg.txt_r = _iniColors[60].r;
                                        btn_64arg.txt_g = _iniColors[60].g;
                                        btn_64arg.txt_b = _iniColors[60].b;

                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.xpos = 0;
                                            btn_64arg.ypos = 2 * (_fontH + buttonsSpace);
                                            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_TXT3);
                                            btn_64arg.caption2.clear();
                                            btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2;

                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.tileset_down = 19;
                                                btn_64arg.tileset_up = 18;
                                                btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                btn_64arg.tileset_disabled = 30;
                                                btn_64arg.xpos = netMenuWidth * 0.3;
                                                btn_64arg.ypos = (buttonsSpace + _fontH) * 15.2;
                                                btn_64arg.width = netMenuWidth * 0.4;
                                                btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_NEW);
                                                btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;
                                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                btn_64arg.downCode = 1251;
                                                btn_64arg.upCode = 1201;
                                                btn_64arg.caption2.clear();
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.txt_r = _iniColors[68].r;
                                                btn_64arg.txt_g = _iniColors[68].g;
                                                btn_64arg.txt_b = _iniColors[68].b;

                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.xpos = 0;
                                                    btn_64arg.ypos = bottomButtonsY + _fontH;
                                                    btn_64arg.width = button1LineWidth;
                                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_NEXT);
                                                    btn_64arg.caption2.clear();
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.button_id = 1201;
                                                    btn_64arg.upCode = 1200;

                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.xpos = bottomThirdBtnPosX;
                                                        btn_64arg.ypos = bottomButtonsY + _fontH;
                                                        btn_64arg.width = button1LineWidth;
                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                                        btn_64arg.caption2.clear();
                                                        btn_64arg.upCode = 1250;
                                                        btn_64arg.pressedCode = 0;
                                                        btn_64arg.button_id = 1218;

                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.xpos = bottomSecondBtnPosX;
                                                            btn_64arg.ypos = bottomButtonsY + _fontH;
                                                            btn_64arg.width = button1LineWidth;
                                                            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_CANCEL);
                                                            btn_64arg.caption2.clear();
                                                            btn_64arg.upCode = 1202;
                                                            btn_64arg.pressedCode = 0;
                                                            btn_64arg.button_id = 1203;

                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                            {
                                                                int raceColWidth;

                                                                if ( _screenSize.x < 512 )
                                                                    raceColWidth = 6 * checkBoxWidth;
                                                                else
                                                                    raceColWidth = 4 * checkBoxWidth;

                                                                btn_64arg.tileset_down = 16;
                                                                btn_64arg.tileset_up = 16;
                                                                btn_64arg.tileset_disabled = 16;
                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                btn_64arg.xpos = raceColWidth + checkBoxWidth;
                                                                btn_64arg.ypos = 4 * (_fontH + buttonsSpace);
                                                                btn_64arg.caption = " ";
                                                                btn_64arg.width = menuListWidth - raceColWidth - checkBoxWidth;
                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                btn_64arg.caption2.clear();
                                                                btn_64arg.downCode = 0;
                                                                btn_64arg.upCode = 0;
                                                                btn_64arg.pressedCode = 0;
                                                                btn_64arg.button_id = 1210;
                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                {
                                                                    btn_64arg.ypos = 5 * (buttonsSpace + _fontH);
                                                                    btn_64arg.button_id = 1211;

                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                    {
                                                                        btn_64arg.ypos = 6 * (buttonsSpace + _fontH);
                                                                        btn_64arg.button_id = 1212;

                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                        {
                                                                            btn_64arg.ypos = 7 * (buttonsSpace + _fontH);
                                                                            btn_64arg.button_id = 1213;

                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                            {
                                                                                btn_64arg.tileset_down = 8;
                                                                                btn_64arg.tileset_up = 8;
                                                                                btn_64arg.tileset_disabled = 8;
                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                btn_64arg.xpos = 0;
                                                                                btn_64arg.width = raceColWidth;
                                                                                btn_64arg.caption2.clear();
                                                                                btn_64arg.downCode = 0;
                                                                                btn_64arg.upCode = 0;
                                                                                btn_64arg.ypos = 4 * (_fontH + buttonsSpace);
                                                                                btn_64arg.pressedCode = 0;
                                                                                btn_64arg.caption = " ";
                                                                                btn_64arg.flags = 0;
                                                                                btn_64arg.button_id = 1214;
                                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                {
                                                                                    btn_64arg.ypos = 5 * (_fontH + buttonsSpace);
                                                                                    btn_64arg.button_id = 1215;

                                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                    {
                                                                                        btn_64arg.ypos = 6 * (buttonsSpace + _fontH);
                                                                                        btn_64arg.button_id = 1216;

                                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                        {
                                                                                            btn_64arg.ypos = 7 * (buttonsSpace + _fontH);
                                                                                            btn_64arg.button_id = 1217;

                                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                btn_64arg.tileset_down = 19;
                                                                                                btn_64arg.tileset_up = 18;
                                                                                                btn_64arg.xpos = 0;
                                                                                                btn_64arg.tileset_disabled = 30;
                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                                btn_64arg.width = checkBoxWidth;
                                                                                                btn_64arg.caption = "g";
                                                                                                btn_64arg.caption2 = "g";
                                                                                                btn_64arg.pressedCode = 0;
                                                                                                btn_64arg.button_id = 1219;
                                                                                                btn_64arg.ypos = bottomButtonsY + _fontH;
                                                                                                btn_64arg.downCode = 1208;
                                                                                                btn_64arg.flags = 0;
                                                                                                btn_64arg.upCode = 1209;

                                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                {
                                                                                                    btn_64arg.tileset_down = 16;
                                                                                                    btn_64arg.tileset_up = 16;
                                                                                                    btn_64arg.tileset_disabled = 16;
                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                    btn_64arg.xpos += buttonsSpace + checkBoxWidth;
                                                                                                    btn_64arg.width = button1LineWidth - checkBoxWidth - buttonsSpace;
                                                                                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_READY);
                                                                                                    btn_64arg.caption2.clear();
                                                                                                    btn_64arg.downCode = 0;
                                                                                                    btn_64arg.upCode = 0;
                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                    btn_64arg.button_id = 1221;

                                                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                    {
                                                                                                        btn_64arg.xpos = 0;
                                                                                                        btn_64arg.tileset_down = 16;
                                                                                                        btn_64arg.tileset_up = 16;
                                                                                                        btn_64arg.tileset_disabled = 16;
                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                        btn_64arg.ypos = 3 * (_fontH + buttonsSpace);
                                                                                                        btn_64arg.width = netMenuWidth * 0.3;
                                                                                                        btn_64arg.caption = Locale::Text::Get(Locale::LBL_YOUPLAY, Locale::DefaultStrings::YouPlay);
                                                                                                        btn_64arg.caption2.clear();
                                                                                                        btn_64arg.downCode = 0;
                                                                                                        btn_64arg.upCode = 0;
                                                                                                        btn_64arg.pressedCode = 0;
                                                                                                        btn_64arg.button_id = 1227;
                                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                        btn_64arg.txt_r = _iniColors[68].r;
                                                                                                        btn_64arg.txt_g = _iniColors[68].g;
                                                                                                        btn_64arg.txt_b = _iniColors[68].b;

                                                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                        {
                                                                                                            btn_64arg.xpos = netMenuWidth * 0.3;
                                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                            btn_64arg.width = netMenuWidth * 0.7;
                                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                            btn_64arg.button_id = 1226;
                                                                                                            btn_64arg.caption = "...";

                                                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                            {
                                                                                                                btn_64arg.tileset_down = 16;
                                                                                                                btn_64arg.tileset_up = 16;
                                                                                                                btn_64arg.tileset_disabled = 16;
                                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                btn_64arg.xpos = 0;
                                                                                                                btn_64arg.ypos = (14 * (vertMenuSpace + _fontH));
                                                                                                                btn_64arg.width = menuListWidth;
                                                                                                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_REFRESHSESS);
                                                                                                                btn_64arg.caption2.clear();
                                                                                                                btn_64arg.downCode = 0;
                                                                                                                btn_64arg.upCode = 0;
                                                                                                                btn_64arg.pressedCode = 0;
                                                                                                                btn_64arg.button_id = 1228;
                                                                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                                    allAdded = 1;
                                                                                                            }
                                                                                                        }
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if ( !allAdded )
    {
        ypa_log_out("Unable to add network-button\n");
        return false;
    }
    NC_STACK_button::WidgetState btnState;
    btnState.butID = 1210;
    btnState.state = 0;
    _GameShell->network_button->Disable(&btnState);

    btnState.butID = 1211;
    _GameShell->network_button->Disable(&btnState);

    btnState.butID = 1212;
    _GameShell->network_button->Disable(&btnState);

    btnState.butID = 1213;
    _GameShell->network_button->Disable(&btnState);

    btnState.butID = 1214;
    _GameShell->network_button->Disable(&btnState);

    btnState.butID = 1215;
    _GameShell->network_button->Disable(&btnState);

    btnState.butID = 1216;
    _GameShell->network_button->Disable(&btnState);

    btnState.butID = 1217;
    _GameShell->network_button->Disable(&btnState);

    _GameShell->network_button->HideScreen();
    return true;
}
bool NC_STACK_ypaworld::OpenGameShell()
{
    printf("OpenGameShell\n");
    UpdateScreenSize();
    LoadScreenFonts();

    if ( !yw_LoadSet(46) )
    {
        ypa_log_out("Unable to load set for shell\n");
        return false;
    }

    _GameShell->sgmSaveExist = 0;
    _GameShell->confFirstKey = true;
    _GameShell->lastInputEvent = 0;
    _GameShell->p_YW->_upScreenBorder = 0;
    _GameShell->keyCatchMode = false;
    _GameShell->p_YW->_helpURL.clear();
    _GameShell->blocked = false;

    if ( _GameShell->default_lang_dll )
    {
        if ( ! LoadLanguage(*_GameShell->default_lang_dll) )
            ypa_log_out("Warning: Catalogue not found\n");
    }
    else
    {
        ypa_log_out("Warning: No Language selected, use default set\n");
    }

    CollectNetworkLevels(_GameShell);

    if ( !GameShellInitBkg() )
    {
        ypa_log_out("Could not init level select stuff!\n");
        return false;
    }

    GFX::Engine.SetCursor(1, 0);

    LoadKeyNames();

    
    _GameShell->InputConfigTitle[World::INPUT_BIND_PAUSE]       = Locale::Text::Inputs(Locale::INPUTS_PAUSE);
    _GameShell->InputConfigTitle[World::INPUT_BIND_QUIT]        = Locale::Text::Inputs(Locale::INPUTS_QUIT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_DRIVE_DIR]   = Locale::Text::Inputs(Locale::INPUTS_DRIVEDIR);
    _GameShell->InputConfigTitle[World::INPUT_BIND_DRIVE_SPEED] = Locale::Text::Inputs(Locale::INPUTS_DRIVESPD);
    _GameShell->InputConfigTitle[World::INPUT_BIND_GUN_HEIGHT]  = Locale::Text::Inputs(Locale::INPUTS_GUNHGHT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_FLY_HEIGHT]  = Locale::Text::Inputs(Locale::INPUTS_FLYHGHT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_FLY_SPEED]   = Locale::Text::Inputs(Locale::INPUTS_FLYSPD);
    _GameShell->InputConfigTitle[World::INPUT_BIND_FLY_DIR]     = Locale::Text::Inputs(Locale::INPUTS_FLYDIR);
    _GameShell->InputConfigTitle[World::INPUT_BIND_BRAKE]       = Locale::Text::Inputs(Locale::INPUTS_STOP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_FIRE]        = Locale::Text::Inputs(Locale::INPUTS_FIRE);
    _GameShell->InputConfigTitle[World::INPUT_BIND_CAMFIRE]     = Locale::Text::Inputs(Locale::INPUTS_FIREVW);
    _GameShell->InputConfigTitle[World::INPUT_BIND_GUN]         = Locale::Text::Inputs(Locale::INPUTS_FIREGUN);
    _GameShell->InputConfigTitle[World::INPUT_BIND_SET_COMM]    = Locale::Text::Inputs(Locale::INPUTS_MAKECOMM);
    _GameShell->InputConfigTitle[World::INPUT_BIND_HUD]         = Locale::Text::Inputs(Locale::INPUTS_HEADUPDISP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AUTOPILOT]   = Locale::Text::Inputs(Locale::INPUTS_AUTOPILOT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ORDER]       = Locale::Text::Inputs(Locale::INPUTS_ORDER);
    _GameShell->InputConfigTitle[World::INPUT_BIND_NEW]         = Locale::Text::Inputs(Locale::INPUTS_NEW);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ADD]         = Locale::Text::Inputs(Locale::INPUTS_ADD);
    _GameShell->InputConfigTitle[World::INPUT_BIND_SQ_MANAGE]   = Locale::Text::Inputs(Locale::INPUTS_FINDER);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_1]      = Locale::Text::Inputs(Locale::INPUTS_AGGR1);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_2]      = Locale::Text::Inputs(Locale::INPUTS_AGGR2);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_3]      = Locale::Text::Inputs(Locale::INPUTS_AGGR3);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_4]      = Locale::Text::Inputs(Locale::INPUTS_AGGR4);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_5]      = Locale::Text::Inputs(Locale::INPUTS_AGGR5);
    _GameShell->InputConfigTitle[World::INPUT_BIND_MAP]         = Locale::Text::Inputs(Locale::INPUTS_MAP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_WAPOINT]     = Locale::Text::Inputs(Locale::INPUTS_SELWAYPT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LANDLAYER]   = Locale::Text::Inputs(Locale::INPUTS_LANDSCAPE);
    _GameShell->InputConfigTitle[World::INPUT_BIND_OWNER]       = Locale::Text::Inputs(Locale::INPUTS_OWNER);
    _GameShell->InputConfigTitle[World::INPUT_BIND_HEIGHT]      = Locale::Text::Inputs(Locale::INPUTS_HEIGHT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_MINIMAP]     = Locale::Text::Inputs(Locale::INPUTS_MAPMINI);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LOCKVIEW]    = Locale::Text::Inputs(Locale::INPUTS_LOCKVW);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ZOOMIN]      = Locale::Text::Inputs(Locale::INPUTS_ZOOMIN);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ZOOMOUT]     = Locale::Text::Inputs(Locale::INPUTS_ZOOMOUT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LOG_WND]     = Locale::Text::Inputs(Locale::INPUTS_LOGWIN);
    _GameShell->InputConfigTitle[World::INPUT_BIND_CONTROL]     = Locale::Text::Inputs(Locale::INPUTS_CONTROL);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LAST_SEAT]   = Locale::Text::Inputs(Locale::INPUTS_TOLASTOCCUP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ATTACK]      = Locale::Text::Inputs(Locale::INPUTS_FIGHT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_TO_HOST]     = Locale::Text::Inputs(Locale::INPUTS_TOROBO);
    _GameShell->InputConfigTitle[World::INPUT_BIND_TO_COMM]     = Locale::Text::Inputs(Locale::INPUTS_TOCOMM);
    _GameShell->InputConfigTitle[World::INPUT_BIND_NEXT_UNIT]   = Locale::Text::Inputs(Locale::INPUTS_NEXTUNIT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_NEXT_COMM]   = Locale::Text::Inputs(Locale::INPUTS_NEXTCOM);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LAST_MSG]    = Locale::Text::Inputs(Locale::INPUTS_JUMPLASTMSNG);
    _GameShell->InputConfigTitle[World::INPUT_BIND_TO_ALL]      = Locale::Text::Inputs(Locale::INPUTS_MSGTOALL);
    _GameShell->InputConfigTitle[World::INPUT_BIND_HELP]        = Locale::Text::Inputs(Locale::INPUTS_HELP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ANALYZER]    = Locale::Text::Inputs(Locale::INPUTS_ANALYZER);

    if ( !CreateGameShellControls() )
        return false;

    ShowGameShellPage();

    UpdateGameShell();

    SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 6);

    _GameShell->HasInited = true;

    if ( _GameShell->remoteMode )
    {
        _GameShell->GameShellUiOpenNetwork();
        _GameShell->p_YW->_isNetGame = true;
        _GameShell->FreeFraction = (World::OWNER_GHOR_BIT | World::OWNER_MYKO_BIT | World::OWNER_TAER_BIT);
        _GameShell->SelectedFraction = World::OWNER_RESIST_BIT;
    }
    else
    {
        _GameShell->yw_netcleanup();
        _GameShell->netSelMode = UserData::NETSCREEN_MODE_SELECT;
    }

    _GameShell->netSel = -1;

    if ( _GameShell->p_YW->_preferences & World::PREF_CDMUSICDISABLE )
    {
        SFXEngine::SFXe.StopMusicTrack();
        SFXEngine::SFXe.SetMusicTrack(_GameShell->shelltrack, _GameShell->shelltrack__adv.min_delay, _GameShell->shelltrack__adv.max_delay);
        SFXEngine::SFXe.PlayMusicTrack();
    }

    return true;
}


void NC_STACK_ypaworld::CloseGameShell()
{
    if ( _GameShell->HasInited )
    {       
        DeleteGameShellControls();

        SFXEngine::SFXe.StopPlayingSounds();

        _globalMapRegions.UnloadImages();

        if ( _GameShell->EnvMode == ENVMODE_TUTORIAL || _GameShell->EnvMode == ENVMODE_SINGLEPLAY )
        {
            if ( _levelInfo.State == TLevelInfo::STATE_BRIEFING )
            {
                FreeBriefing();
            }
            else if ( _levelInfo.State == TLevelInfo::STATE_DEBRIEFING )
            {
                FreeDebrief();
            }
        }

        FreeSetData(this);

        _GameShell->HasInited = false;
    }
}

bool NC_STACK_ypaworld::CreateGameShellControls()
{
    if ( _screenSize.x < 512 )
    {
        buttonsSpace = 2;
        vertMenuSpace = 2;
        checkBoxWidth = 8;
        videoMenuRowWidth = 210;
    }
    else
    {
        buttonsSpace = 3;
        vertMenuSpace = 3;
        checkBoxWidth = 16;
        videoMenuRowWidth = 380;
    }
    
    int menuWidth = _screenSize.x * 0.7;
    int menuHeight = _screenSize.y * 0.8;

    scaledFontHeight = _fontH;
    if ( _screenSize.x >= 512 )
        scaledFontHeight += (_screenSize.y - 384) / 2;

    

    if ( _screenSize.x < 512 )
        bottomButtonsY = menuHeight - _fontH;
    else
        bottomButtonsY = menuHeight - _fontH - (_screenSize.y - 384) / 2;

    button1LineWidth = ( menuWidth - 2 * buttonsSpace )/ 3;



    bottomSecondBtnPosX = buttonsSpace + button1LineWidth;
    bottomThirdBtnPosX = 2 * buttonsSpace + 2 * button1LineWidth;

    printf("Creating CreateTitleControls\n");
    if (!this->CreateTitleControls()) return false;
    printf("Creating CreateSubBarControls\n");
    if (!this->CreateSubBarControls()) return false;
    printf("Creating CreateConfirmControls\n");
    if (!this->CreateConfirmControls()) return false;    


    menuListWidth = menuWidth - _fontVBScrollW;

    
    printf("Creating CreateInputControls\n");    
    if (!this->CreateInputControls()) return false;
    printf("Creating CreateVideoControls\n");
    if (!this->CreateVideoControls()) return false;
    printf("Creating CreateDiskControls\n");
    if (!this->CreateDiskControls()) return false;
    printf("Creating CreateLocaleControls\n");
    if (!this->CreateLocaleControls()) return false;
    printf("Creating CreateAboutControls\n");
    if (!this->CreateAboutControls()) return false;
    

    netMenuWidth = menuWidth - _fontVBScrollW;

    if (!this->CreateNetworkControls()) return false;

    return true;
}

void NC_STACK_ypaworld::DeleteGameShellControls()
{
    if ( _GameShell->confirm_button )
    {
        _GameShell->confirm_button->HideScreen();
        _GameShell->confirm_button->Delete();
    }
    _GameShell->confirm_button = NULL;

    if ( _GameShell->sub_bar_button )
    {
        _GameShell->sub_bar_button->HideScreen();
        _GameShell->sub_bar_button->Delete();
    }
    _GameShell->sub_bar_button = NULL;

    if ( _GameShell->titel_button )
    {
        _GameShell->titel_button->HideScreen();
        _GameShell->titel_button->Delete();
    }
    _GameShell->titel_button = NULL;

    if ( _GameShell->button_input_button )
    {
        if ( _GameShell->input_listview.IsOpen() )
            _GameShell->p_YW->GuiWinClose( &_GameShell->input_listview );
        _GameShell->input_listview.Free();

        _GameShell->button_input_button->HideScreen();
        _GameShell->button_input_button->Delete();
        _GameShell->button_input_button = NULL;
    }

    if ( _GameShell->video_button )
    {
        if ( _GameShell->d3d_listvw.IsOpen() )
            _GameShell->p_YW->GuiWinClose( &_GameShell->d3d_listvw );
        _GameShell->d3d_listvw.Free();

        _GameShell->video_button->HideScreen();
        _GameShell->video_button->Delete();
        _GameShell->video_button = NULL;
    }

    if ( _GameShell->disk_button )
    {
        if ( _GameShell->disk_listvw.IsOpen() )
            _GameShell->p_YW->GuiWinClose( &_GameShell->disk_listvw );
        _GameShell->disk_listvw.Free();

        _GameShell->disk_button->HideScreen();
        _GameShell->disk_button->Delete();
        _GameShell->disk_button = NULL;
    }

    if ( _GameShell->locale_button )
    {
        if ( _GameShell->local_listvw.IsOpen() )
            _GameShell->p_YW->GuiWinClose( &_GameShell->local_listvw );
        _GameShell->local_listvw.Free();

        _GameShell->locale_button->HideScreen();
        _GameShell->locale_button->Delete();
        _GameShell->locale_button = NULL;
    }

    if ( _GameShell->about_button )
    {
        _GameShell->about_button->HideScreen();
        _GameShell->about_button->Delete();
        _GameShell->about_button = NULL;
    }

    if ( _GameShell->network_button )
    {
        if ( _GameShell->network_listvw.IsOpen() )
            _GameShell->p_YW->GuiWinClose( &_GameShell->network_listvw );
        _GameShell->network_listvw.Free();

        _GameShell->network_button->HideScreen();
        _GameShell->network_button->Delete();
        _GameShell->network_button = NULL;
    }
}

// Shows the controls of the current shell page, e.g. after creating them anew.
void NC_STACK_ypaworld::ShowGameShellPage()
{
    switch (_GameShell->EnvMode)
    {
    default:
        _GameShell->titel_button->ShowScreen();
        break;
    case ENVMODE_TUTORIAL:
    case ENVMODE_SINGLEPLAY:
        _GameShell->sub_bar_button->ShowScreen();

        if ( _GameShell->GameIsOver )
        {
            NC_STACK_button::WidgetState btnState;
            btnState.state = 0;
            btnState.butID = 1014;

            _GameShell->sub_bar_button->Disable(&btnState);

            btnState.butID = 1013;
            _GameShell->sub_bar_button->Disable(&btnState);
        }
        break;
    case ENVMODE_NETPLAY:
        _GameShell->network_button->ShowScreen();
        break;
    case ENVMODE_INPUT:
        _GameShell->ShowInputSettings();
        break;
    case ENVMODE_SETTINGS:
        _GameShell->ShowOptionsMenu();
        break;
    case ENVMODE_SELPLAYER:
        _GameShell->ShowSaveLoadMenu();
        break;
    case ENVMODE_SELLOCALE:
        _GameShell->ShowLanguageMenu();
        break;
    case ENVMODE_ABOUT:
        _GameShell->ShowAbout();
        break;
    }
}

// Lays the shell out for a new screen size, staying on the same page.
void NC_STACK_ypaworld::RelayoutGameShell()
{
    DeleteGameShellControls();

    if ( !CreateGameShellControls() )
    {
        ypa_log_out("Error: Unable to lay out GameShell for %d x %d\n", _screenSize.x, _screenSize.y);
        return;
    }

    ShowGameShellPage();

    if ( (_GameShell->EnvMode == ENVMODE_TUTORIAL || _GameShell->EnvMode == ENVMODE_SINGLEPLAY)
         && _levelInfo.State == TLevelInfo::STATE_BRIEFING )
        RelayoutBriefingTextList();

    if ( _GameShell->confirmMode )
        _GameShell->ShowConfirmDialog(_GameShell->confirmMode, _GameShell->confirmText1,
                                      _GameShell->confirmText2, _GameShell->confirmSingleButton);

    UpdateGameShell();
}

//Draw bkg or briefing
void NC_STACK_ypaworld::GameShellBkgProcess()
{
    if ( _GameShell->EnvModeChanged )
        GameShellInitBkgMode(_GameShell->EnvMode);

    switch(_GameShell->EnvMode)
    {
    case ENVMODE_TUTORIAL:
    case ENVMODE_SINGLEPLAY:
        UpdateCampaignScreen();
        break;

    case ENVMODE_TITLE:
        GameShellBlitBkg(_globalMapRegions.MenuImage);
        break;

    default:
        GameShellBlitBkg(_globalMapRegions.RolloverImage);
        break;
    }
}

void draw_tooltip(NC_STACK_ypaworld *yw)
{
    if ( yw->_mouseGrabbed || (yw->_toolTipId && !yw->_mouseCursorHidden) )
    {
        int textY = -(yw->_fontH + yw->_downScreenBorder + yw->_fontH / 4);
        std::string hotkeyText;
        
        if ( yw->_toolTipHotKeyId != -1 )
        {
            int16_t keycode = Input::Engine.GetHotKey(yw->_toolTipHotKeyId);

            if ( keycode != Input::KC_NONE )
            {
                if ( yw->_GameShell && !Input::Engine.KeyTitle.at(keycode).empty())
                {
                    hotkeyText = fmt::sprintf("[%s]", Input::Engine.KeyTitle.at(keycode));
                    textY = -(yw->_downScreenBorder + 2 * yw->_fontH + yw->_fontH / 4);
                }
            }
        }

        CmdStream buf;
        buf.reserve(1024);

        FontUA::select_tileset(&buf, 15);
        FontUA::set_xpos(&buf, 0);
        FontUA::set_ypos(&buf, textY);

        if ( !hotkeyText.empty() )
        {
            FontUA::set_txtColor(&buf, yw->_iniColors[61].r, yw->_iniColors[61].g, yw->_iniColors[61].b);

            FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf, hotkeyText.c_str(), yw->_screenSize.x);

            FontUA::next_line(&buf);
        }

        FontUA::set_txtColor(&buf, yw->_iniColors[63].r, yw->_iniColors[63].g, yw->_iniColors[63].b);

        FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf,  yw->GetTooltipString() , yw->_screenSize.x);

        FontUA::set_end(&buf);

        GFX::Engine.ProcessDrawSeq(buf);
    }
}

//Make screenshot
void TakeScreenshot(NC_STACK_ypaworld *yw)
{
    GFX::Engine.SaveScreenshot(fmt::sprintf("env:snaps/f_%04d", yw->_screenShotCount));
    
    yw->_screenShotCount++;
}

//FIXME
int PrevMouseX = 0;
int PrevMouseY = 0;

bool NC_STACK_ypaworld::IsAnyInput(TInputState *struc)
{
    bool click = (struc->ClickInf.flag & ~TClickBoxInf::FLAG_OK);
    bool mousemove = struc->ClickInf.move.ScreenPos.x != PrevMouseX || PrevMouseY != struc->ClickInf.move.ScreenPos.y;

    PrevMouseX = struc->ClickInf.move.ScreenPos.x;
    PrevMouseY = struc->ClickInf.move.ScreenPos.y;

    return struc->KbdLastDown != Input::KC_NONE || struc->KbdLastHit != Input::KC_NONE || struc->HotKeyID >= 0 || click || mousemove;
}

void NC_STACK_ypaworld::ProcessGameShell()
{
    _GameShell->envAction.action = EnvAction::ACTION_NONE;

    SFXEngine::SFXe.BeginSoundFrame(_GameShell->DTime, vec3d(0.0), vec3d(0.0), mat3x3::Ident());

    GFX::Engine.BeginFrame();

    int oldMode = _GameShell->EnvMode;
    _GameShell->EnvModeChanged = false;

    ClearTooltip();
    _GameShell->GameShellUiHandleInput();

    if ( oldMode != _GameShell->EnvMode )
        _GameShell->EnvModeChanged = true;

    GameShellBkgProcess();

    draw_tooltip(this);

    DrawGameShellControls(this, _GameShell);

    if ( _isNetGame )
    {
        _netFlushTimer -= _GameShell->DTime;
        if ( _netFlushTimer <= 0 )
        {
            _netDriver->FlushBroadcastBuffer();

            _netFlushTimer = 100;
        }
    }

    SFXEngine::SFXe.UpdateSoundCarrier(&_GameShell->samples1_info);

    SFXEngine::SFXe.EndSoundFrame();
    

    GFX::Engine.EndFrame();

    
    if ( IsCheatKeyHit(_GameShell->Input, Input::KC_NUMMUL) )
        TakeScreenshot(this);

    if ( _GameShell->netSelMode == UserData::NETSCREEN_INSESSION )
    {
        yw_CheckCRCs(this);
    }

    if ( IsAnyInput(_GameShell->Input) )
        _GameShell->lastInputEvent = _GameShell->GlobalTime;

    if ( (_GameShell->GlobalTime - _GameShell->lastInputEvent) > World::CVDemoWait && _GameShell->EnvMode == ENVMODE_TITLE )
        _GameShell->envAction.action = EnvAction::ACTION_REPLAY;

    _GameShell->GameIsOver = false;

    if ( !_helpURL.empty() )
    {
        const std::string helpLink = _helpURL;
        OpenHelpURL(&helpLink);

        _helpURL.clear();
    }
}


void NC_STACK_ypaworld::ShowMessage(TInfoMessage *arg)
{
    if ( arg->MsgID )
        VoiceMessagePlayMsg(arg->unit, arg->Priority, arg->MsgID);

    if ( arg->unit )
        info_log.last_sender_gid = arg->unit->_gid;
    else
        info_log.last_sender_gid = 0;

    info_log.last_msg_time = _timeStamp;
    info_log.last_msg_id = arg->MsgID;

    if ( !arg->txt.empty() )
    {
        TInfoLogMsg *entry;

        if ( info_log.last_msg_prio >= 5 )
        {
            info_log.msg_count++;

            if ( info_log.msg_count >= 64 )
                info_log.msg_count = 0;

            if ( info_log.oldest_msg_idx == info_log.msg_count )
            {
                info_log.oldest_msg_idx++;

                if ( info_log.oldest_msg_idx >= 64 )
                    info_log.oldest_msg_idx = 0;
            }

            info_log.numEntries++;;

            if ( info_log.numEntries > 64 )
                info_log.numEntries = 64;

            entry = &info_log.msgs[info_log.msg_count];

            info_log.last_msg_idx = info_log.msg_count;
        }
        else
        {
            // TODO(bug): Overwriting a low-priority message doesn't take its lines off
            // numEntries, so the new message's extra lines count again and the log can
            // show stale rows.
            info_log.msg_count = info_log.last_msg_idx;

            entry = &info_log.msgs[info_log.last_msg_idx];
        }

        info_log.last_msg_duration = 5000;
        info_log.last_msg_lines = 1;
        info_log.last_msg_prio = arg->Priority;

        if ( arg->unit )
            entry->id = arg->unit->_gid;
        else
            entry->id = 0;

        entry->displayTime = 7000;
        entry->timestamp = _timeStamp;

        const char *src = arg->txt.c_str();

        int len = 0;

        while ( *src )
        {
            if ( *src == '\n' )
            {
                entry->txt[len] = 0;

                len = 0;

                info_log.msg_count++;

                if ( info_log.msg_count >= 64 )
                    info_log.msg_count = 0;

                if ( info_log.oldest_msg_idx == info_log.msg_count )
                {
                    info_log.oldest_msg_idx++;

                    if ( info_log.oldest_msg_idx >= 64 )
                        info_log.oldest_msg_idx = 0;
                }

                info_log.numEntries++;

                if ( info_log.numEntries > 64 )
                    info_log.numEntries = 64;

                info_log.last_msg_duration += 5000;
                info_log.last_msg_lines++;

                entry = &info_log.msgs[ info_log.msg_count ];

                if ( arg->unit )
                    entry->id = arg->unit->_gid;
                else
                    entry->id = 0;

                entry->displayTime = 7000;
                entry->timestamp = 0;
            }
            else if ( len < 125 )
            {
                entry->txt[len] = *src;
                len++;
            }

            src++;
        }

        entry->txt[len] = 0;

        info_log.firstShownEntries = info_log.numEntries - info_log.shownEntries;

        if ( info_log.firstShownEntries < 0 )
            info_log.firstShownEntries = 0;
    }
}


// Load Level
size_t NC_STACK_ypaworld::CreateLevel(TLevelLoadRequest *arg)
{
    int ok = 0;
    TLevelDescription mapp;
    
    _particles.Clear();

    if ( LevelCommonLoader(&mapp, arg->lvlID, arg->mode) )
    {
        if ( LoadTypeMap(mapp.TypStr) )
        {
            if ( LoadOwnerMap( mapp.OwnStr) )
            {
                if ( LoadHightMap(mapp.HgtStr) )
                {
                    if ( yw_createRobos(mapp.Robos) )
                    {
                        if ( LoadBlgMap(mapp.BlgStr) )
                        {
                            if ( _levelInfo.Mode != 1 )
                            {
                                yw_InitSquads(mapp.Squads);
                                InitBuddies();

                                for (int yy = 0; yy < _mapSize.y; yy++)
                                {
                                    for (int xx = 0; xx < _mapSize.x; xx++)
                                    {
                                        CellCheckHealth(&_cells(xx, yy), World::OWNER_RECALC, NULL);
                                    }
                                }

                                yw_InitTechUpgradeBuildings();
                                InitGates();
                                InitSuperItems();
                                UpdatePowerEnergy();
                            }
                            
                            PrepareAllFillers();

                            if ( GUI_Open(this) )
                                ok = 1;
                        }
                    }
                }
            }
        }
    }

    if ( !ok )
    {
        printf("Load level not OK\n");
        DeleteLevel();
    }

    return ok;
}


size_t NC_STACK_ypaworld::StartReplay(const std::string &fname)
{
    _replayPlayer = new TGameRecorder();

    TGameRecorder *repl = _replayPlayer;

    repl->filename = fname;
    _timeStamp = 0;

    if ( !recorder_open_replay(repl) )
    {
        delete _replayPlayer;
        _replayPlayer = NULL;
        return 0;
    }

    while ( repl->mfile.parse() != IFFile::IFF_ERR_EOC )
    {
        const IFFile::Context &chunk = repl->mfile.GetCurrentChunk();

        if ( chunk.Is(TAG_SINF) )
        {
            repl->seqn = repl->mfile.readU16L();
            repl->level_id = repl->mfile.readU16L();
            repl->mfile.parse();
        }
        else if ( chunk.Is(TAG_FORM, TAG_FRAM) )
        {
            repl->frame_count++;
            repl->mfile.skipChunk();
        }
        else
        {
            repl->mfile.skipChunk();
        }        
    }

    
    repl->mfile.close();

    TLevelLoadRequest loadReq;
    loadReq.mode = 1;
    loadReq.lvlID = repl->level_id;

    if ( !CreateLevel(&loadReq) )
    {
        delete _replayPlayer;
        _replayPlayer = NULL;
        return 0;
    }

    if ( !recorder_create_camera() )
    {
        ypa_log_out("PLAYER ERROR: could not create virtual camera!\n");
        StopReplay();

        return 0;
    }

    repl->camera_pos = vec3d(0.0, 0.0, 0.0);
    repl->rotation_matrix = mat3x3::Ident();

    if ( !recorder_go_to_frame(repl, 0) )
    {
        ypa_log_out("PLAYER ERROR: could not position on 1st frame!\n");
        StopReplay();
        return 0;
    }

    TReplayControl ctrl;

    ctrl.command = 2;
    ctrl.frame = 0;
    ControlReplay(&ctrl);

    ctrl.command = 20;
    ctrl.frame = 0;
    ControlReplay(&ctrl);

    return 1;
}


void NC_STACK_ypaworld::PlayReplayFrame(TUpdateArgs *arg)
{
    TGameRecorder *repl = _replayPlayer;
    uint32_t frameStart = profiler_begin();

    _framesElapsed++;
    _updateMessage.user_action = World::DOACTION_0;
    _updateMessage.gTime = arg->TimeStamp;
    _updateMessage.frameTime = arg->DTime;
    _updateMessage.units_count = 0;
    _updateMessage.inpt = arg->Input;
    _FPS = 1024 / arg->DTime;

    _profileVals[PFID_FPS] = _FPS;

    GFX::Engine.BeginFrame();

    /*_win3d->setRSTR_BGpen(0);

    _win3d->raster_func192(NULL);*/

    UpdateCommanderList();

    _guiVisor.mgunSight = 0;
    _guiVisor.weaponSight = 0;
    _guiVisor.target = NULL;

    if ( repl->play_state != 1 )
        AdvanceReplay(repl, arg->DTime);

    CameraPrepareRender(repl, _userUnit, arg->Input);

    vec3d velocity = _userUnit->_fly_dir * _userUnit->_fly_dir_length;

    SFXEngine::SFXe.BeginSoundFrame(arg->DTime, _userUnit->_position, velocity, _userUnit->_rotation);

    for ( NC_STACK_ypabact* &bct : _userUnit->_kidList )
    {
        bct->_tForm.Pos = bct->_position;

        bct->_tForm.SclRot = bct->_rotation.Transpose();

        bct->_soundcarrier.Position = bct->_position;

        bct->_soundcarrier.Vector = bct->_fly_dir * bct->_fly_dir_length;

        SFXEngine::SFXe.UpdateSoundCarrier(&bct->_soundcarrier);
    }

    const mat3x3 &shake = SFXEngine::SFXe.EndSoundFrame();
    TF::TForm3D *viewPoint = TF::Engine.GetViewPoint();

    viewPoint->SclRot = shake * viewPoint->SclRot;

    uint32_t renderStart = profiler_begin();

    RenderGame(arg, 0);

    debug_info_draw(arg->Input);
    DrawPerfOverlay(arg->Input);

    GFX::Engine.EndFrame();

    _profileVals[PFID_RENDERTIME] = profiler_end(renderStart);

    HandleSnapshotKeys(this, arg->Input);

    _profileVals[PFID_FRAMETIME] = profiler_end(frameStart);
    _profileVals[PFID_POLYGONS] = _polysDraw;

    ProfileCalcValues();
}



void NC_STACK_ypaworld::StopReplay()
{
    if ( _replayPlayer )
    {
        _replayPlayer->mfile.close();

        DeleteLevel();

        delete _replayPlayer;
        _replayPlayer = NULL;
    }
}


void NC_STACK_ypaworld::ControlReplay(TReplayControl *arg)
{
    TGameRecorder *repl = _replayPlayer;

    if ( (repl->camera_mode == 18 || repl->camera_mode == 19 || repl->camera_mode == 20) && (arg->command == 16 || arg->command == 17) )
    {
        repl->camera_pos = _userUnit->_position;
        repl->rotation_matrix = _userUnit->_rotation;
    }

    if ( arg->command == 1 || arg->command == 2 )
    {
        repl->play_state = arg->command;
    }
    else if ( arg->command == 3 )
    {
        recorder_go_to_frame(repl, arg->frame);
    }
    else if ( arg->command == 4 )
    {
        recorder_go_to_frame(repl, repl->frame_id + 1);
    }
    else if ( arg->command == 5 )
    {
        recorder_go_to_frame(repl, repl->frame_id - 1);
    }
    else if ( arg->command == 7 )
    {
        recorder_go_to_frame(repl, repl->frame_id + arg->frame);
    }
    else if ( arg->command == 16 || arg->command == 17 )
    {
        repl->camera_unit_id = 0;
        repl->camera_mode = arg->command;
    }
    else if ( arg->command == 18 || arg->command == 19 || arg->command == 20 )
    {
        repl->camera_mode = arg->command;
        repl->camera_unit_id = arg->frame;

        repl->camera_pos = vec3d(0.0, 0.0, 0.0);
        repl->rotation_matrix = mat3x3::Ident();
    }
    else
    {
        repl->play_state = 1;
    }
}

size_t NC_STACK_ypaworld::LoadLanguage(const std::string &langname)
{
    Locale::Text::SetLangDefault();
    
    Locale::Text::SetLocaleName(langname);

    if ( !Locale::Text::LngFileLoad( fmt::sprintf("locale:%s.lng", langname) ) )
    {
        Locale::Text::SetLangDefault();
        return 0;
    }
    
    LoadScreenFonts();

    return 1;
}


// Update menu values
void NC_STACK_ypaworld::UpdateGameShell()
{
    if ( _GameShell->diskListActiveElement )
    {
        _GameShell->disk_listvw.PosOnSelected(_GameShell->diskListActiveElement - 1);

        NC_STACK_button::WidgetState diskBtnState;
        diskBtnState.state = 1;
        diskBtnState.butID = 1101;
        _GameShell->disk_button->Enable(&diskBtnState);

        diskBtnState.butID = 1102;
        _GameShell->disk_button->Enable(&diskBtnState);

        diskBtnState.butID = 1103;
        _GameShell->disk_button->Enable(&diskBtnState);
    }
    else
    {
        NC_STACK_button::WidgetState diskBtnState;
        diskBtnState.state = 1;
        diskBtnState.butID = 1101;
        _GameShell->disk_button->Disable(&diskBtnState);

        diskBtnState.butID = 1102;
        _GameShell->disk_button->Disable(&diskBtnState);

        diskBtnState.butID = 1103;
        _GameShell->disk_button->Disable(&diskBtnState);
    }

    NC_STACK_button::WidgetState videoBtnState;
    videoBtnState.butID = 1151;
    videoBtnState.state = ((_GameShell->soundFlags & World::SF_INVERTLR) == 0) + 1;

    _GameShell->video_button->SetState(&videoBtnState);

    videoBtnState.state = ((_GameShell->soundFlags & World::SF_CDSOUND) == 0) + 1;
    videoBtnState.butID = 1164;
    _GameShell->video_button->SetState(&videoBtnState);

    NC_STACK_button::Slider *tmp = _GameShell->video_button->GetSliderData(1152);
    tmp->value = _GameShell->soundVolume;

    _GameShell->video_button->Refresh(1152);

    tmp = _GameShell->video_button->GetSliderData(1154);
    tmp->value = _GameShell->musicVolume;

    _GameShell->video_button->Refresh(1154);

    videoBtnState.butID = 1163;
    videoBtnState.state = (_GameShell->enemyIndicator == 0) + 1;
    _GameShell->video_button->SetState(&videoBtnState);

    videoBtnState.state = ((_GameShell->GFXFlags & World::GFX_FLAG_SKYRENDER) == 0) + 1;
    videoBtnState.butID = 1160;
    _GameShell->video_button->SetState(&videoBtnState);

    videoBtnState.butID = 1166;
    videoBtnState.state = (!_GameShell->IsWindowedFlag()) + 1;
    _GameShell->video_button->SetState(&videoBtnState);

    videoBtnState.butID = 1173;
    videoBtnState.state = (!_GameShell->IsVsyncFlag()) + 1;
    _GameShell->video_button->SetState(&videoBtnState);

    _GameShell->SetEngineOptionWidgets();

    tmp = _GameShell->video_button->GetSliderData(1159);
    tmp->value = _GameShell->fxnumber;

    _GameShell->video_button->Refresh(1159);

    NC_STACK_button::WidgetState inputBtnState;
    inputBtnState.butID = 1050;
    inputBtnState.state = ((_GameShell->p_YW->_preferences & World::PREF_JOYDISABLE) != 0) + 1;

    _GameShell->button_input_button->SetState(&inputBtnState);

    inputBtnState.butID = 1061;
    inputBtnState.state = (_GameShell->altJoystickEnabled == false) + 1;
    _GameShell->button_input_button->SetState(&inputBtnState);

    inputBtnState.butID = 1055;
    inputBtnState.state = ((_GameShell->p_YW->_preferences & World::PREF_FFDISABLE) != 0) + 1;
    _GameShell->button_input_button->SetState(&inputBtnState);

    if ( _GameShell->inpListActiveElement )
    {
        int selIndex = _GameShell->inpListActiveElement - 1;
        int shownEnd = _GameShell->input_listview.maxShownEntries + _GameShell->input_listview.firstShownEntries;

        if ( selIndex >= _GameShell->input_listview.firstShownEntries && selIndex < shownEnd )
        {
            if ( shownEnd > _GameShell->input_listview.numEntries )
                _GameShell->input_listview.firstShownEntries = _GameShell->input_listview.numEntries - _GameShell->input_listview.maxShownEntries;

            if ( _GameShell->input_listview.firstShownEntries < 0 )
                _GameShell->input_listview.firstShownEntries = 0;
        }
        else if ( _GameShell->input_listview.numEntries - selIndex <= _GameShell->input_listview.maxShownEntries )
        {
            _GameShell->input_listview.firstShownEntries = _GameShell->input_listview.numEntries - _GameShell->input_listview.maxShownEntries;
        }
        else
        {
            _GameShell->input_listview.firstShownEntries = selIndex;
        }
    }
}


size_t NC_STACK_ypaworld::BeamUnitThroughGate(NC_STACK_ypabact *bact)
{
    if ( bact->_bact_type == BACT_TYPES_GUN || bact->_bact_type == BACT_TYPES_MISSLE )
        return 1;

    if ( bact->_owner == _userRobo->_owner )
    {
        if ( bact->_pSector->PurposeType == cellArea::PT_GATEOPENED )
        {
            if ( _userRobo == bact )
            {
                _levelInfo.State = TLevelInfo::STATE_COMPLETED;
                _levelInfo.GateCompleteID = bact->_pSector->PurposeIndex;
            }
            else
            {
                _beamEnergyCurrent += (bact->_energy_max + 99) / 100;

                if ( _beamEnergyCurrent <= _beamEnergyCapacity )
                    _levelInfo.Buddies.push_back( TMapBuddy( bact->_commandID, bact->_vehicleID, bact->_energy ) );
                else
                    return 0;
            }
        }
    }
    return 1;
}

int NC_STACK_ypaworld::LoadingParseSaveFile(const std::string &filename)
{
    int lvlnum;
    ScriptParser::HandlersList parsers
    {
        new World::Parsers::UserParser(this),
        new World::Parsers::SaveRoboParser(this),
        new World::Parsers::SaveSquadParser(this), // commander and units
        new World::Parsers::SaveGemParser(this),
        new World::Parsers::VhclProtoParser(this),
        new World::Parsers::WeaponProtoParser(this),
        new World::Parsers::BuildProtoParser(this),
        new World::Parsers::SaveExtraViewParser(this),
        new World::Parsers::SaveKwFactorParser(this),
        new World::Parsers::SaveGlobalsParser(this),
        new World::Parsers::SaveOwnerMapParser(this),
        new World::Parsers::SaveBuildingMapParser(this),
        new World::Parsers::SaveEnergyMapParser(this),
        new World::Parsers::SaveLevelNumParser(this, &lvlnum),
        new World::Parsers::LevelStatusParser(this, true),
        new World::Parsers::SaveHistoryParser(this),
        new World::Parsers::SaveMasksParser(this),
        new World::Parsers::SaveSuperBombParser(this),
    };
    
    if (World::IsModsAllow(false))
        parsers.push_back( new World::Parsers::SaveLuaScriptParser(this) );

    return ScriptParser::ParseFile(filename, parsers, ScriptParser::FLAG_SKIP_UNKNOWN);
}

void NC_STACK_ypaworld::LoadingUnitsRefresh()
{
    for ( NC_STACK_ypabact* &station : _unitsList )
    {
        RefreshUnitPRT(station, station, true);

        for ( NC_STACK_ypabact* &commander : station->_kidList )
        {
            RefreshUnitPRT(commander, station, false);

            for ( NC_STACK_ypabact* &slave : commander->_kidList )
                RefreshUnitPRT(slave, station, false);
        }
    }

    if ( _extraViewEnable )
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);

        if ( robo->_roboGuns.at(_extraViewNumber).gun_obj )
        {
            robo->_roboGuns.at(_extraViewNumber).gun_obj->setBACT_viewer(true);
            robo->_roboGuns.at(_extraViewNumber).gun_obj->setBACT_inputting(true);
        }
    }
}

size_t NC_STACK_ypaworld::LoadGame(const std::string &saveFile)
{
    bool loadOK = false;

    if ( !uaFileExist(saveFile) )
        return 1;

    if ( saveFile.find(".sgm") != std::string::npos || saveFile.find(".SGM") != std::string::npos )
    {
        _maxReloadConst = 0;
        _maxRoboEnergy = 0;
    }

    int lvlnum;
    
    ScriptParser::HandlersList parsers
    {
        new World::Parsers::SaveLevelNumParser(this, &lvlnum),
    };

    ScriptParser::ParseFile(saveFile, parsers, ScriptParser::FLAG_SKIP_UNKNOWN);

    _extraViewNumber = -1;
    _extraViewEnable = false;

    TLevelDescription mapp;
    
    _particles.Clear();

    if ( LevelCommonLoader(&mapp, lvlnum, 0) )
    {
        if ( LoadTypeMap(mapp.TypStr) )
        {
            if ( LoadOwnerMap(mapp.OwnStr) )
            {
                if ( LoadHightMap(mapp.HgtStr) )
                {
                    if ( LoadBlgMap(mapp.BlgStr) )
                        loadOK = true;
                }
            }
        }
    }

    if ( !loadOK )
    {
        DeleteLevel();
        return 0;
    }

    _levelInfo.OwnerMask = 0;
    _levelInfo.UserMask = 0;

    bact_id = 0x10000;
    maxLoadedCommandID = 0;

    InitSuperItems();

    _lvlPrimevalTypeMap = _lvlTypeMap;
    _lvlPrimevalOwnMap = _lvlOwnMap;

    if ( !LoadingParseSaveFile(saveFile) )
        return 0;

    maxLoadedCommandID++;
    bact_id++;

    // TODO(bug): Without a user robo, nextCommandID keeps its value from before
    // the load.
    if ( _userRobo )
        dynamic_cast<NC_STACK_yparobo *>(_userRobo) ->setROBO_commCount(maxLoadedCommandID);

    LoadingUnitsRefresh();

    if ( saveFile.find(".fin") != std::string::npos || saveFile.find(".FIN") != std::string::npos )
        InitBuddies();

    for(int y = 0; y < _mapSize.y; y++)
    {
        for(int x = 0; x < _mapSize.x; x++)
        {
            CellCheckHealth(&_cells(x, y), World::OWNER_NOCHANGE, NULL);
        }
    }

    InitGates();
    UpdatePowerEnergy();
    
    PrepareAllFillers();

    if ( !GUI_Open(this) )
        return 0;

    return 1;
}


size_t NC_STACK_ypaworld::SaveGame(const std::string &saveFile)
{
    bool write_ok = true;

    if ( saveFile.find(".sgm") != std::string::npos || saveFile.find(".SGM") != std::string::npos )
        uaDeleteFile( fmt::sprintf("save:%s/sgisold.txt", _GameShell->UserName) );

    int write_modifers;
    int write_user;
    int write_level_statuses;

    bool isfin_save = saveFile.find(".fin") != std::string::npos || saveFile.find(".FIN") != std::string::npos;

    if ( isfin_save )
    {
        _maxRoboEnergy = _userRobo->_energy_max;
        write_modifers = 0;
        write_user = 0;
        write_level_statuses = 0;
        _maxReloadConst = _userRobo->_reload_const;
    }
    else
    {
        write_level_statuses = 1;
        write_modifers = 1;
        write_user = 1;
    }
    
    // Force to write last frame timestamp into history
    if (_historyLastIsTimeStamp)
        _history.Write( _historyLastFrame.MakeByteArray() );

    FSMgr::FileHandle *fil = uaOpenFileAlloc( saveFile, "w");

    if ( !fil )
    {
        ypa_log_out("Unable to open savegame file\n");
        return 0;
    }

    if ( write_modifers )
    {
        if ( !yw_write_item_modifers(this, fil) )
            write_ok = false;
    }

    if ( write_user )
    {
        if ( !yw_write_user(fil, _GameShell) )
            write_ok = false;
    }

    if ( write_ok )
    {
        if ( yw_write_levelnum(this, fil) )
        {
            yw_write_ownermap(this, fil);
            yw_write_buildmap(this, fil);
            yw_write_energymap(this, fil);

            if ( yw_write_units(fil) )
            {
                if ( yw_write_wunderinfo(this, fil) )
                {
                    if ( yw_write_kwfactor(this, fil) )
                    {
                        if ( yw_write_globals(this, fil) )
                        {
                            if ( yw_write_superbomb(this, fil) )
                                write_ok = true;
                        }
                    }
                }
            }
        }
    }

    if ( write_ok && write_level_statuses )
    {
        for (int i = 0; i < 256; i++)
        {
            if ( _globalMapRegions.MapRegions[i].Status != TMapRegionInfo::STATUS_NONE )
            {
                if ( !yw_write_level_status(fil, this, i) )
                {
                    write_ok = false;
                    break;
                }
            }
        }
    }

    if ( write_ok )
    {
        if ( !isfin_save )
        {
            yw_write_history(this, fil);
            yw_write_masks(this, fil);
        }
    }
    
    if (_script)
    {
        std::string buf = _script->GetSaveString();
        
        if (!buf.empty())
        {
            fil->printf("\nbegin_luascript\n");
            fil->printf("%s\n", buf.c_str());
            fil->printf("EOF\n");
            fil->printf("end\n");
        }
    }

    delete fil;
    return write_ok;
}


// Saving game
bool NC_STACK_ypaworld::SaveSettings(UserData *usr, const std::string &fileName, uint32_t sdfMask)
{
    if ( fileName.empty() )
    {
        ypa_log_out("No names for save action\n");
        return false;
    }

    FSMgr::FileHandle *sfil = uaOpenFileAlloc(fmt::sprintf("save:%s", fileName), "w");

    if ( !sfil )
        return false;

    if ( (sdfMask & World::SDF_USER) && !yw_write_user(sfil, usr) )
    {
        ypa_log_out("Unable to write user data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_INPUT) && !yw_write_input(sfil, usr) )
    {
        ypa_log_out("Unable to write input data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_SOUND) && !yw_write_sound(sfil, usr) )
    {
        ypa_log_out("Unable to write sound data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_VIDEO) && !yw_write_video(sfil, usr) )
    {
        ypa_log_out("Unable to write video data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_SCORE) && !yw_write_levels_statuses(sfil, usr->p_YW) )
    {
        ypa_log_out("Unable to write score data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_BUDDY) && !yw_write_buddies(sfil, usr->p_YW) )
    {
        ypa_log_out("Unable to write buddies to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_GAME) && !yw_write_game(sfil, usr) )
    {
        ypa_log_out("Unable to write game settings to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_SHELL) && !yw_write_shell(sfil, usr) )
    {
        ypa_log_out("Unable to write shell data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_PROTO) && !yw_write_item_modifers(usr->p_YW, sfil) )
    {
        ypa_log_out("Unable to write build info to file\n");
        delete sfil;
        return false;
    }

    delete sfil;
    return true;
}



int NC_STACK_ypaworld::ParseSettingsFile(const std::string &fname, uint32_t sdfMask)
{
    ScriptParser::HandlersList parsers;
    if ( sdfMask & World::SDF_USER )
        parsers += new World::Parsers::UserParser(this);

    if ( sdfMask & World::SDF_INPUT )
        parsers += new World::Parsers::InputParser(this);

    if ( sdfMask & World::SDF_VIDEO )
        parsers += new World::Parsers::VideoParser(this);

    if ( sdfMask & World::SDF_SOUND )
        parsers += new World::Parsers::SoundParser(this);

    if ( sdfMask & World::SDF_SCORE )
        parsers += new World::Parsers::LevelStatusParser(this, true);

    if ( sdfMask & World::SDF_BUDDY )
        parsers += new World::Parsers::BuddyParser(this);

    if ( sdfMask & World::SDF_SHELL )
        parsers += new World::Parsers::ShellParser(this);

    if ( sdfMask & World::SDF_PROTO )
    {
        parsers += new World::Parsers::VhclProtoParser(this);
        parsers += new World::Parsers::WeaponProtoParser(this);
        parsers += new World::Parsers::BuildProtoParser(this);
    }

    if ( sdfMask & World::SDF_GAME )
    {
        // Profiles without the section follow nucleus.ini
        SetAITimeline(System::IniConf::GameTimeLine.Get<int>());
        parsers += new World::Parsers::GameParser(this);
    }

    return ScriptParser::ParseFile(fname, parsers, ScriptParser::FLAG_SKIP_UNKNOWN);
}

// Load user settings (Global save)
size_t NC_STACK_ypaworld::LoadSettings(const std::string &fileName, const std::string &userName, uint32_t sdfMask, bool updateGameShell, bool playIntro)
{
    if ( sdfMask & World::SDF_SCORE )
    {
        if ( _GameShell->IgnoreScoreSaving )
        {
            _GameShell->IgnoreScoreSaving = false;
        }
        else
        {
            SaveSettings(_GameShell, fmt::sprintf("%s/user.txt", _GameShell->UserName), World::SDF_ALL);
        }
    }

    _GameShell->savedDataFlags = 0;

    if ( sdfMask & World::SDF_BUDDY )
        _levelInfo.Buddies.clear();
    if ( !ParseSettingsFile(fmt::sprintf("save:%s", fileName), sdfMask) )
    {
        ypa_log_out("Error while loading information from %s\n", fileName.c_str());
        return 0;
    }
    
    if (playIntro && !_GameShell->remoteMode)
    {
        SetWindowed( _GameShell->IsWindowedFlag() );
        PlayIntroMovie();
    }

    if ( updateGameShell && !_GameShell->HasInited && !OpenGameShell() ) // Init menus
    {
        ypa_log_out("Unable to open GameShell\n");
        return 0;
    }
    

    if ( (sdfMask & World::SDF_SCORE) && (_GameShell->savedDataFlags & World::SDF_SCORE) )
    {
        _GameShell->UserName = userName;
    }

    if ( sdfMask & World::SDF_INPUT )
        _GameShell->InputConfCopyToBackup();

    if ( updateGameShell )
        UpdateGameShell(); // Update menu values

    return 1;
}


bool NC_STACK_ypaworld::ReloadInput(size_t id)
{
    std::string keyConfStr;

    if ( id < 1 || id >= _GameShell->InputConfig.size() )
        return false;

    UserData::TInputConf &kconf = _GameShell->InputConfig.at(id);

    if ( Input::Engine.KeyNamesTable.at(kconf.PKeyCode).Name.empty() )
        return false;

    if ( kconf.Type == World::INPUT_BIND_TYPE_SLIDER && Input::Engine.KeyNamesTable.at(kconf.NKeyCode).Name.empty() )
        return false;

    if ( kconf.Type == World::INPUT_BIND_TYPE_SLIDER )
    {
        keyConfStr += "~#";
        keyConfStr += "winp:";
        keyConfStr += Input::Engine.KeyNamesTable.at(kconf.NKeyCode).Name;
        keyConfStr += " #";
        keyConfStr += "winp:";
    }
    else if ( kconf.Type == World::INPUT_BIND_TYPE_BUTTON )
    {
        keyConfStr += "winp:";
    }

    if ( Input::Engine.KeyNamesTable.at(kconf.PKeyCode).Name.empty() )
        return false;

    keyConfStr += Input::Engine.KeyNamesTable.at(kconf.PKeyCode).Name;

    if ( kconf.Type == World::INPUT_BIND_TYPE_HOTKEY )
    {
        if ( !Input::Engine.SetHotKey(kconf.KeyID, keyConfStr) )
            ypa_log_out("input.engine: WARNING: Hotkey[%d] (%s) not accepted.\n", kconf.KeyID, keyConfStr.c_str());
    }
    else
    {
        if ( kconf.Type == World::INPUT_BIND_TYPE_BUTTON )
        {
            if ( !Input::Engine.SetInputExpression(false, kconf.KeyID, keyConfStr) )
                ypa_log_out("input.engine: WARNING: Button[%d] (%s) not accepted.\n", kconf.KeyID, keyConfStr.c_str());
        }
        else
        {
            if ( !Input::Engine.SetInputExpression(true, kconf.KeyID, keyConfStr) )
                ypa_log_out("input.engine: WARNING: Slider[%d] (%s) not accepted.\n", kconf.KeyID, keyConfStr.c_str());
        }
    }
    kconf.NKeyCodeBkp = kconf.NKeyCode;
    kconf.PKeyCodeBkp = kconf.PKeyCode;
    return true;
}


// Applies the player's windowed setting. Only done when the game starts and when
// the player changes it in the options; otherwise the window is left as it is.
void NC_STACK_ypaworld::SetWindowed(bool windowed)
{
    GFX::Engine.SetFullscreen(!windowed);
    UpdateScreenSize();
}

void NC_STACK_ypaworld::UpdateScreenSize()
{
    // The player can also switch fullscreen outside the game, e.g. with the
    // window's fullscreen button; keep their setting in line with the window.
    if ( _GameShell )
    {
        if ( System::IsFullscreen() )
            _GameShell->GFXFlags &= ~World::GFX_FLAG_WINDOWED;
        else
            _GameShell->GFXFlags |= World::GFX_FLAG_WINDOWED;
    }

    GFX::Engine.UpdateResolution();

    if ( _screenSize == GFX::Engine.GetScreenSize() )
        return;

    _screenSize = GFX::Engine.GetScreenSize();

    LoadScreenFonts();

    if ( _GameShell && _GameShell->HasInited )
        RelayoutGameShell();

    if ( _guiLoaded )
        RelayoutGameGui();
}

void NC_STACK_ypaworld::LoadScreenFonts()
{
    if ( _screenSize.x >= 512 )
    {
        GFX::Engine.LoadFontByDescr( Locale::Text::Font() );
    }
    else
    {
        GFX::Engine.LoadFontByDescr( Locale::Text::SmallFont() );
    }
}


size_t NC_STACK_ypaworld::ReloadLanguage()
{
    if ( !_GameShell->default_lang_dll )
    {
        ypa_log_out("Set Language, but no language selected\n");
        return 0;
    }

    int wasOpen;

    if ( _GameShell->HasInited )
    {
        CloseGameShell();
        wasOpen = 1;
    }
    else
    {
        wasOpen = 0;
    }

    if ( !LoadLanguage(*_GameShell->default_lang_dll) )
        ypa_log_out("Warning: SETLANGUAGE failed\n");

    if ( wasOpen && !OpenGameShell() )
    {
        ypa_log_out("Unable to open GameShell\n");
        return 0;
    }

    return 1;
}


void NC_STACK_ypaworld::GetReloadRatios(TReloadRatios *arg)
{
    arg->ratioClamped = _reloadRatioClamped[arg->owner];
    arg->ratioPositive = _reloadRatioPositive[arg->owner];
}


void NC_STACK_ypaworld::HandleRoboDestroyed(TRoboDestroyed *arg)
{
    //Reown sectors for new owner

    if ( !arg->killerOwner ) //New owner
        return;

    if ( arg->bact == _userRobo )
        _playerHSDestroyed = true;

    if ( _userRobo )
    {
        if ( _userRobo->_owner == arg->killerOwner )
            _beamEnergyCapacity += _beamEnergyAdd;
    }

    for ( NC_STACK_ypabact* &unit : _unitsList )
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && unit != arg->bact && arg->bact->_owner == unit->_owner )
            return;
    }

    for (int i = 0; i < _mapSize.y; i++)
    {
        for (int j = 0; j < _mapSize.x; j++)
        {
            cellArea &cell = _cells(j, i);

            if ( cell.owner == arg->bact->_owner )
                CellSetOwner(&cell, arg->killerOwner);
        }
    }

    if ( !_userRobo )
        return;

    if ( _userRobo->_owner != arg->killerOwner )
        return;

    for (int i = 0; i < _mapSize.y; i++)
    {
        for (int j = 0; j < _mapSize.x; j++)
        {
            cellArea &cell = _cells(j, i);

            if ( cell.PurposeType == cellArea::PT_TECHUPGRADE && cell.owner == _userRobo->_owner )
            {

                if ( _isNetGame )
                    ActivateNetWunderstein(this, &cell, cell.PurposeIndex);
                else
                    yw_ActivateWunderstein(&cell, cell.PurposeIndex);
                
                HistoryEventAdd( World::History::Upgrade(j, i, cell.owner, _techUpgrades[ _upgradeId ].Type, _upgradeVehicleId, _upgradeWeaponId, _upgradeBuildId) );
            }

        }
    }
}

//179 method in yw_net


void NC_STACK_ypaworld::FFeedback_Play(TFFeedbackEffect *arg)
{
    if ( _shellConfIsParsed )
    {
        if ( _preferences & (World::PREF_JOYDISABLE | World::PREF_FFDISABLE) )
            return;
    }
    
    switch ( arg->effects_type )
    {
    case 0:
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_MISSILEFIRE);
        break;

    case 1:
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_GRENADEFIRE);
        break;

    case 2:
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_BOMBFIRE);
        break;

    case 3:
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_MINIGUN);
        break;

    case 4:
        Input::Engine.ForceFeedback(Input::FF_STATE_STOP, Input::FF_TYPE_MINIGUN);
        break;

    case 5:
    {
        NC_STACK_ypabact *bct = _userUnit;
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_COLLISION,
            arg->strength, 0.0,
            (arg->pos_z - bct->_position.z) * bct->_rotation.m02 + (arg->pos_x - bct->_position.x) * bct->_rotation.m00,
            -((arg->pos_x - bct->_position.x) * bct->_rotation.m20 + (arg->pos_z - bct->_position.z) * bct->_rotation.m22));

    }
    break;

    default:
        break;
    }
}


bool NC_STACK_ypaworld::NetSendMessage(uamessage_base *data, size_t dataSize, const std::string &recvID, bool garantee)
{
    if (_GameShell->noSent)
        return false;

    return _netDriver->Send(data, dataSize, recvID, garantee);
}

bool NC_STACK_ypaworld::NetBroadcastMessage(uamessage_base *data, size_t dataSize, bool garantee)
{
    if (_GameShell->noSent)
        return false;
    
    if ( _GameShell->netPlayerOwner != 0 && data->msgID != UAMSG_VHCLENERGY )
        _GameShell->msgcount++; 

    return _netDriver->Broadcast(data, dataSize, garantee);
}

int HasLevelFinSave(int lvlID, const char *userName)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc( fmt::sprintf("save:%s/%d.fin", userName, lvlID) , "r");

    if ( !fil )
        return 0;

    delete fil;
    return 1;
}

// Advanced Create Level
size_t NC_STACK_ypaworld::AdvancedCreateLevel(TLevelLoadRequest *arg)
{
    int loaded;

    if ( _globalMapRegions.MapRegions[ arg->lvlID ].Status == TMapRegionInfo::STATUS_COMPLETED && HasLevelFinSave(arg->lvlID, _GameShell->UserName.c_str()) )
    {
        std::string savename = fmt::sprintf("save:%s/%d.fin", _GameShell->UserName, arg->lvlID);
        loaded = LoadGame(savename);

        if ( !loaded )
            ypa_log_out("Warning: in YWM_ADVANCEDCREATELEVEL: YWM_LOADGAME of %s failed!\n", savename.c_str());

        _userRobo->_energy = _userRobo->_energy_max;
    }
    else
    {
        loaded = CreateLevel(arg);

        if ( !loaded )
            ypa_log_out("Warning: in YWM_ADVANCEDCREATELEVEL: YWM_CREATELEVEL %d failed!\n", arg->lvlID);
    }

    if ( loaded )
    {
        if ( !SaveGame(fmt::sprintf("save:%s/%d.rst", _GameShell->UserName, _levelInfo.LevelID)) )
            ypa_log_out("Warning: could not create restart file for level %d, user %s.\n", _levelInfo.LevelID, _GameShell->UserName.c_str());
    }

    _lvlPrimevalTypeMap = _lvlTypeMap;
    _lvlPrimevalOwnMap = _lvlOwnMap;

    return loaded;
}


void NC_STACK_ypaworld::HistoryEventAdd(const World::History::Record &arg)
{
    switch ( arg.type )
    {
    case World::History::TYPE_FRAME: // Do not write timestamp every frame, wait for any another data
        _historyLastIsTimeStamp = true;
        _historyLastFrame = static_cast<const World::History::Frame&>(arg);
        break;
                
    case World::History::TYPE_CONQ:
    case World::History::TYPE_VHCLKILL:
    case World::History::TYPE_VHCLCREATE:
    case World::History::TYPE_POWERST:
    case World::History::TYPE_UPGRADE:
        
        if (_historyLastIsTimeStamp) // If
            _history.Write(_historyLastFrame.MakeByteArray());
            
        _history.Write(arg.MakeByteArray());
        
        _historyLastIsTimeStamp = false;
        
        if (_GameShell && _GameShell->isHost )
            arg.AddScore(&_gameplayStats);
        break;

    default:
        break;
    }

    
}


void NC_STACK_ypaworld::OpenHelpURL(const void *arg)
{
    dprintf("MAKE ME %s\n","OpenHelpURL");
}


void NC_STACK_ypaworld::setYW_skyVisLimit(int limit)
{
    _skyVizLimit = limit;
}

void NC_STACK_ypaworld::setYW_skyFadeLength(int len)
{
    _skyFadeLength = len;
}

void NC_STACK_ypaworld::setYW_skyHeight(int hght)
{
    _skyHeight = hght;
}

void NC_STACK_ypaworld::setYW_skyRender(int dorender)
{
    _skyRender = dorender;
}

void NC_STACK_ypaworld::setYW_doEnergyRecalc(int doRecalc)
{
    _doEnergyRecalc = doRecalc;
}

void NC_STACK_ypaworld::setYW_userHostStation(NC_STACK_ypabact *host)
{
    _userRobo = host;
    _playerOwner = host->_owner;
}

void NC_STACK_ypaworld::setYW_userVehicle(NC_STACK_ypabact *bact)
{
    if ( bact != _userUnit )
    {
        NC_STACK_ypabact *oldpBact = _userUnit;

        if ( oldpBact )
            _prevUnitId = oldpBact->_gid;

        _userUnit = bact;

        _vehicleTakenControlTimestamp = _timeStamp;
        _vehicleTakenCommandId = _userUnit->_commandID;
        _guiDragDefaultMouse = false;

        if ( _userUnit->_bact_type == BACT_TYPES_ROBO )
        {
            _joyIgnoreY = 1;
            _joyIgnoreX = 1;
        }

        FFeedback_VehicleChanged();

        if ( oldpBact )
            RestoreWinStatus(this, oldpBact, _userUnit);
    }
}

void NC_STACK_ypaworld::setYW_screenW(int w)
{
    _screenSize.x = w;
}

void NC_STACK_ypaworld::setYW_screenH(int h)
{
    _screenSize.y = h;
}

void NC_STACK_ypaworld::setYW_dontRender(bool drndr)
{
    _doNotRender = drndr;
}


int NC_STACK_ypaworld::getYW_mapSizeX()
{
    return _mapSize.x;
}

int NC_STACK_ypaworld::getYW_mapSizeY()
{
    return _mapSize.y;
}

Common::Point NC_STACK_ypaworld::GetMapSize()
{
    return _mapSize;
}

int NC_STACK_ypaworld::getYW_skyHeight()
{
    return _skyHeight;
}

int NC_STACK_ypaworld::getYW_skyRender()
{
    return _skyRender;
}

int NC_STACK_ypaworld::getYW_doEnergyRecalc()
{
    return _doEnergyRecalc;
}

NC_STACK_ypabact *NC_STACK_ypaworld::getYW_userHostStation()
{
    return _userRobo;
}

NC_STACK_ypabact *NC_STACK_ypaworld::getYW_userVehicle()
{
    return _userUnit;
}


int NC_STACK_ypaworld::getYW_lvlFinished()
{
    if ( _levelInfo.State != TLevelInfo::STATE_COMPLETED && _levelInfo.State != TLevelInfo::STATE_ABORTED )
        return 0;

    return 1;
}

int NC_STACK_ypaworld::getYW_screenW()
{
    return _screenSize.x;
}

int NC_STACK_ypaworld::getYW_screenH()
{
    return _screenSize.y;
}

TLevelInfo &NC_STACK_ypaworld::GetLevelInfo()
{
    return _levelInfo;
}

int NC_STACK_ypaworld::getYW_destroyFX()
{
    return _fxLimit;
}

NC_STACK_windp *NC_STACK_ypaworld::getYW_pNET()
{
    return _netDriver;
}

int NC_STACK_ypaworld::getYW_invulnerable()
{
    return _invulnerable;
}



int NC_STACK_ypaworld::TestVehicle(int protoID, int job)
{
    const World::TVhclProto &proto = _vhclProtos[ protoID ];

    World::TWeapProto *wpn;

    if ( proto.weapon == -1 )
        wpn = NULL;
    else
        wpn = &_weaponProtos.at( proto.weapon );

    switch ( job )
    {
    case 1:
        if ( (proto.mgun == -1 && !wpn) || proto.model_id == BACT_TYPES_UFO )
            return -1;

        return proto.job_fightrobo;
        break;

    case 2:
        if ( (proto.mgun == -1 && !wpn) || proto.model_id == BACT_TYPES_UFO )
            return -1;

        return proto.job_fighttank;
        break;

    case 4:
        if ( (proto.mgun == -1 && !wpn) || proto.model_id == BACT_TYPES_UFO )
            return -1;

        return proto.job_fighthelicopter;
        break;

    case 3:
        if ( (proto.mgun == -1 && !wpn) || proto.model_id == BACT_TYPES_UFO )
            return -1;

        return proto.job_fightflyer;
        break;

    case 5:
        return proto.job_reconnoitre;
        break;

    case 6:
        if ( !wpn || proto.model_id == BACT_TYPES_UFO )
            return -1;

        return proto.job_conquer;
        break;

    default:
        break;
    }
    return -1;
}


SDL_Color NC_STACK_ypaworld::GetColor(int colorID)
{
    return _iniColors.at(colorID);
}



cellArea *NC_STACK_ypaworld::GetSector(int32_t x, int32_t y)
{
    if (_cells.empty())
        return NULL;
    
    if (x >= 0 && x < _mapSize.x
    &&  y >= 0 && y < _mapSize.y)
        return &_cells(x, y); 
    return NULL;
}

cellArea *NC_STACK_ypaworld::GetSector(const Common::Point &sec)
{
    if (_cells.empty())
        return NULL;
    
    if (sec.x >= 0 && sec.x < _mapSize.x
    &&  sec.y >= 0 && sec.y < _mapSize.y)
        return &_cells(sec.x, sec.y); 
    return NULL;
}

cellArea *NC_STACK_ypaworld::GetSector(size_t id)
{
    if (_cells.empty())
        return NULL;
    
    if (id >= 0 && id < _cells.size())
        return &_cells.At(id);
    return NULL;
}

cellArea &NC_STACK_ypaworld::SectorAt(int32_t x, int32_t y)
{
    return _cells(x, y);
}

cellArea &NC_STACK_ypaworld::SectorAt(const Common::Point &sec)
{
    return _cells.At(sec);
}

cellArea &NC_STACK_ypaworld::SectorAt(size_t id)
{
    return _cells.At(id);
}

void NC_STACK_ypaworld::SetMapSize(const Common::Point &sz)
{
    _mapSize = sz;

    _mapLength = vec2d( _mapSize.x * World::CVSectorLength, _mapSize.y * World::CVSectorLength );
    
    _cells.Clear();
    _cells.Resize(sz);
    
    int32_t id = 0;
    for (int y = 0; y < sz.y; y++)
    {
        for (int x = 0; x < sz.x; x++)
        {
            cellArea &cell = _cells(x, y);
            cell.Id = id;
            cell.CellId = Common::Point(x, y);
            
            if (x == 0 || y == 0 || x == sz.x - 1 || y == sz.y - 1)
                cell.Flags |= cellArea::CF_BORDER;
            
            id++;
        }
    }
    
    _cellsVFCache.Clear();
    _cellsVFCache.Resize(sz.x, sz.y);
    
    _cellsHFCache.Clear();
    _cellsHFCache.Resize(sz.x, sz.y);
    
    _energyAccumMap.Clear();
    _energyAccumMap.Resize(sz);
}

bool NC_STACK_ypaworld::IsGamePlaySector(const Common::Point &sz) const
{
    return sz.x > 0 && sz.x < (_mapSize.x - 1) && sz.y > 0 && sz.y < (_mapSize.y - 1);
}

bool NC_STACK_ypaworld::IsSectorBorder(const Common::Point &sz, int border) const
{
    return sz.x < border || sz.x >= (_mapSize.x - border) || sz.y < border || sz.y >= (_mapSize.y - border);
}

bool NC_STACK_ypaworld::IsSector(const Common::Point &sz) const
{
    return sz.x >= 0 && sz.x < _mapSize.x  && sz.y >= 0 && sz.y < _mapSize.y;
}

Common::PlaneVector<cellArea> &NC_STACK_ypaworld::Sectors()
{
    return _cells;
}

int32_t NC_STACK_ypaworld::GetLegoBld(const cellArea *cell, int bldX, int bldY)
{
    TSubSectorDesc *ssec = _secTypeArray[ cell->type_id ].SubSectors.At(bldX, bldY);
    int32_t hlth = cell->buildings_health.Get(bldX, bldY);
    return ssec->HPModels[ _buildHealthModelId[ hlth ] ];
}

int32_t NC_STACK_ypaworld::GetLegoBld(const Common::Point &cell, int bldX, int bldY)
{
    return GetLegoBld(&SectorAt(cell), bldX, bldY);
}

TCellFillerCh::~TCellFillerCh()
{
    FreeVBO();
}

void TCellFillerCh::FreeVBO()
{
    for (GFX::TMesh &msh : Meshes)
        GFX::Engine.MeshFreeVBO( &msh );
}

void TCellFillerCh::MakeVBO()
{
    for (GFX::TMesh &msh : Meshes)
        GFX::Engine.MeshMakeVBO( &msh );
}
