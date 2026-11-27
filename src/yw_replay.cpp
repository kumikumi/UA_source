#include <algorithm>
#include <cmath>

#include "includes.h"
#include "yw.h"
#include "yw_internal.h"
#include "font.h"
#include "log.h"
#include "system/system.h"
#include "world/playback.h"
#include "ypagun.h"

extern yw_infolog info_log; // In yw_game_ui.cpp

// The replay viewer: a recording played back in its level as it was drawn,
// with a timeline at the bottom to go to any moment of it. It's looked at
// from where the player looked, from any unit, or from a camera flown
// freely; each faction's units and its host stations' energy are listed in
// a window of its own, as the squadron manager lists the player's, and the
// map shows the level.

// How fast it can play, times real time
static const std::array<double, 7> ReplaySpeeds = {0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0};
static const int ReplayNormalSpeed = 2;

// The arrow keys skip that far, in steps: 10 s
static const double ReplaySkipSteps = 600.0;

// The free camera: how fast it flies, in world units a second, and with
// shift held; how soon it gets to that speed and stops again, as the share
// of the way it goes in a second; how far the mouse turns it, in radians a
// point; how far above the ground it stays
static const double FreeCameraSpeed = 1500.0;
static const double FreeCameraFastSpeed = 6000.0;
static const double FreeCameraEase = 6.0;
static const double FreeCameraTurn = 0.004;
static const double FreeCameraClearance = 60.0;

// Where on the screen the timeline is, in pixels from the top left: a row
// of text with the ways to look at the right, the track, and the keys
struct ReplayBar
{
    int Top = 0;           // Of the panel, which goes to the bottom
    int TextY = 0;
    int TrackX0 = 0, TrackX1 = 0, TrackY = 0;
    int KeysY = 0;
    int Pad = 0;

    // The ways to look: from each player in the frame shown, by name, then
    // manual and free
    struct Button
    {
        int X = 0, W = 0;
        ReplayCamera::Mode Mode = ReplayCamera::FREE;
        uint32_t Player = 0;
        std::string Title;
    };

    std::vector<Button> Buttons;
};

static ReplayBar LayOutReplayBar(NC_STACK_ypaworld *yw)
{
    ReplayBar bar;
    bar.Pad = std::max(4, yw->_fontH / 2);
    bar.Top = yw->_screenSize.y - (3 * yw->_fontH + 4 * bar.Pad);
    bar.TextY = bar.Top + bar.Pad;
    bar.TrackY = bar.TextY + yw->_fontH + bar.Pad + yw->_fontH / 2;
    bar.KeysY = bar.TrackY + yw->_fontH / 2 + bar.Pad;
    bar.TrackX0 = 2 * bar.Pad;
    bar.TrackX1 = yw->_screenSize.x - 2 * bar.Pad;

    for ( const World::Frame::Player &p : yw->_replayFrame.Players )
    {
        ReplayBar::Button b;
        b.Mode = ReplayCamera::PLAYER;
        b.Player = p.Id;
        // In capitals, as the panel's font has them and the other buttons are
        b.Title = p.Name.empty() ? fmt::sprintf("#%u", p.Id) : p.Name;
        std::transform(b.Title.begin(), b.Title.end(), b.Title.begin(), [](unsigned char c) { return std::toupper(c); });
        bar.Buttons.push_back(b);
    }

    ReplayBar::Button manual, free;
    manual.Mode = ReplayCamera::MANUAL;
    manual.Title = Locale::Text::Title(Locale::TITLE_REPLAYMANUAL);
    free.Mode = ReplayCamera::FREE;
    free.Title = Locale::Text::Title(Locale::TITLE_REPLAYFREE);
    bar.Buttons.push_back(manual);
    bar.Buttons.push_back(free);

    TileMap *font = yw->_guiTiles[15];
    int right = bar.TrackX1;

    for (auto b = bar.Buttons.rbegin(); b != bar.Buttons.rend(); b++)
    {
        b->W = font->GetWidth(b->Title) + 2 * bar.Pad;
        b->X = right - b->W;
        right = b->X - bar.Pad;
    }

    return bar;
}

// Sets the level of a recording up to watch it, from a step on. Only what
// the level is made of: everything drawn of it comes from the recording's
// view, read with the recording's own fields, so that a recording made by
// another build of the game can be watched too. False, after logging why,
// when it can't; there's no level then.
bool NC_STACK_ypaworld::StartReplay(const std::string &path, uint32_t startStep)
{
    std::unique_ptr<World::Playback> replay(new World::Playback);

    if ( !replay->Open(path) )
        return false;

    int level = replay->GetRecording().Level();
    std::string description = RecordedLevel(replay->GetRecording());

    return StartViewer(std::move(replay), level, description, startStep);
}

bool NC_STACK_ypaworld::StartWatching(std::unique_ptr<World::FrameSource> source, int level, const std::string &levelDescription)
{
    uint32_t newest = source->LastStep();
    return StartViewer(std::move(source), level, levelDescription, newest);
}

bool NC_STACK_ypaworld::StartViewer(std::unique_ptr<World::FrameSource> source, int level, const std::string &levelDescription,
                                    uint32_t startStep)
{
    // Set first: failing to set the level up deletes it only as a replay's
    _replay = std::move(source);

    if ( !LoadLevelData(level, levelDescription) )
    {
        ypa_log_out("Could not set level %d up to watch\n", level);
        _replay.reset();
        _levelInfo.State = TLevelInfo::STATE_MENU;
        return false;
    }

    _replayPosition = std::min(std::max(startStep, _replay->FirstStep()), _replay->LastStep());
    _replayLiveEnded = false;
    _replayPaused = false;
    _replaySpeed = ReplayNormalSpeed;
    _replayDragging = false;
    _replayLeave = false;
    _replayCamera = ReplayCamera();
    _replayFrame = World::Frame();
    _replayWindowsShown = true;
    _replayHudShown = true;
    _replayClock = 0;
    _cameraShake = mat3x3::Ident();
    _cameraOverridden = false;
    _particles.Clear();

    _replayPlaying = false;
    _replayBorders = Common::Point(_upScreenBorder, _downScreenBorder);

    OpenReplayWindows();

    GFX::Engine.SetCursor(1, 0);
    return true;
}

// The viewer's own windows and HUD
void NC_STACK_ypaworld::OpenReplayWindows()
{
    // Windows stay clear of the timeline
    _upScreenBorder = 0;
    _downScreenBorder = _screenSize.y - LayOutReplayBar(this).Top;

    LoadHud(this);

    // The game's own map, of the frames shown: at the top right, a third
    // of the screen wide, following where the camera is, without the help
    // the game has
    _replayHovered = 0;
    _replayPressed = 0;
    _replayScene.Set(this, &_replayFrame, 0, vec3d(), mat3x3::Ident(), 0);

    CreateRoboMap(this, &_replayScene);
    robo_map.flags &= ~GuiBase::FLAG_WITH_HELP;
    robo_map.lockViewer = 1;
    robo_map.zoomLevel = 3;
    RoboMapZoom(this, 0);
    robo_map.w = std::max((int)robo_map.minWidth, _screenSize.x / 3);
    robo_map.h = std::max((int)robo_map.minHeight, _screenSize.y * 2 / 5);
    robo_map.x = _screenSize.x - robo_map.w;
    robo_map.y = _upScreenBorder;

    GuiWinAttach(&robo_map);
    GuiWinOpen(&robo_map);

    // How far the world is drawn, with F10 as in a level
    _viewDistanceWindow.Create(this);
    GuiWinAttach(&_viewDistanceWindow);
}

// Lays the viewer out for a new screen size, its windows where they were
// as far as they fit, as RelayoutGameGui does the game's
void NC_STACK_ypaworld::RelayoutReplay()
{
    // Playing, the game's interface is laid out as the game's
    if ( _replayPlaying )
        return;

    _downScreenBorder = _screenSize.y - LayOutReplayBar(this).Top;

    SetHudLayout(this);
    RoboMapKeepOnScreen(this);
    _viewDistanceWindow.SetRect(this, -2, -2);

    for (ReplayFactionWindow &window : _replayFactions)
    {
        if ( window.Created() )
            window.SetRect(this, -2, -2);
    }
}

void NC_STACK_ypaworld::CloseReplayWindows()
{
    GuiWinDetach(&robo_map);

    _viewDistanceWindow.Close(this);
    GuiWinDetach(&_viewDistanceWindow);
    _viewDistanceWindow.Free();

    for (ReplayFactionWindow &window : _replayFactions)
    {
        if ( window.Created() )
        {
            GuiWinDetach(&window);
            window.Free();
        }

        window = ReplayFactionWindow();
    }

    _guiDragging = false;
    _guiDragElement = NULL;
}

// The level goes, as a replay's: nothing of it was played
void NC_STACK_ypaworld::EndReplay()
{
    SDL_SetWindowRelativeMouseMode(System::GetWindow(), false);

    if ( _replayPlaying )
        StopPlaying();

    CloseReplayWindows();
    FreeHud(this);

    _upScreenBorder = _replayBorders.x;
    _downScreenBorder = _replayBorders.y;

    _cameraOverridden = false;
    _replayFrame = World::Frame();

    FreeLevel();
    _replay.reset();
    _levelInfo.State = TLevelInfo::STATE_MENU;
}

// The step at a point of the track
// Live, steps are shown this many behind the latest that came; when as
// many more behind, it plays twice as fast; this far behind, it jumps
constexpr double LiveLagSteps = 3.0;
constexpr double LiveCatchUpSteps = 30.0;
constexpr double LiveJumpSteps = 5 * World::CVStepsPerSecond;

static double ReplayStepAt(const ReplayBar &bar, const World::FrameSource &replay, int x)
{
    double t = (double)(x - bar.TrackX0) / std::max(1, bar.TrackX1 - bar.TrackX0);
    t = std::max(0.0, std::min(1.0, t));
    return replay.FirstStep() + t * (replay.LastStep() - replay.FirstStep());
}

// At the end it waits there; played again, it starts over. A game watched
// live goes on.
void NC_STACK_ypaworld::ToggleReplayPause()
{
    if ( _replay->Live() )
        return;

    if ( _replayPaused && _replayPosition >= _replay->LastStep() )
        _replayPosition = _replay->FirstStep();

    _replayPaused = !_replayPaused;
}

// The unit the HUD is of and the windows blink: the one the player followed
// saw from, the one looked from, or none flying freely
uint32_t NC_STACK_ypaworld::ReplayViewedUnit(const World::Frame &frame) const
{
    switch ( _replayCamera.CameraMode )
    {
    case ReplayCamera::PLAYER:
        return frame.Viewer;

    case ReplayCamera::MANUAL:
        return _replayCamera.Unit;

    default:
        return 0;
    }
}

// What a player saw, as long as they play; then from where they last looked
void NC_STACK_ypaworld::FollowReplayPlayer(uint32_t player)
{
    SDL_SetWindowRelativeMouseMode(System::GetWindow(), false);

    _replayCamera.CameraMode = ReplayCamera::PLAYER;
    _replayCamera.Player = player;
    _replayCamera.Looking = false;
    _hud.switch_time = _replayClock;
}

// From a unit on, wherever it goes; once it's gone, from where it was
void NC_STACK_ypaworld::LookFromUnit(uint32_t unit)
{
    if ( !unit )
        return;

    SDL_SetWindowRelativeMouseMode(System::GetWindow(), false);

    _replayCamera.CameraMode = ReplayCamera::MANUAL;
    _replayCamera.Unit = unit;
    _replayCamera.Looking = false;
    _hud.switch_time = _replayClock;
}

// Once the unit looked from is dead or gone, the next of its faction by id
// that isn't, a host station's guns and missiles left out. False when it
// isn't gone or there's none.
bool NC_STACK_ypaworld::LookFromNextUnit(const World::Frame &frame)
{
    ReplayCamera &cam = _replayCamera;
    const World::Frame::UnitInfo *info = frame.FindInfo(cam.Unit);

    if ( (info && info->Status != BACT_STATUS_DEAD) || !cam.UnitOwner )
        return false;

    uint32_t first = 0;

    for (size_t i = 0; i < frame.Units.size(); i++)
    {
        const World::Frame::Unit &u = frame.Units[i];
        const World::Frame::UnitInfo *other = frame.InfoAt(i);

        if ( !other || other->Owner != cam.UnitOwner || other->Status == BACT_STATUS_DEAD ||
             other->Status == BACT_STATUS_CREATE || other->Status == BACT_STATUS_BEAM || u.Type == BACT_TYPES_MISSLE ||
             (u.Type == BACT_TYPES_GUN && other->GunFlags >= 0 && (other->GunFlags & NC_STACK_ypagun::GUN_FLAGS_ROBO)) )
            continue;

        if ( u.Id > cam.Unit )
        {
            LookFromUnit(u.Id);
            return true;
        }

        if ( !first )
            first = u.Id;
    }

    LookFromUnit(first);
    return first != 0;
}

void NC_STACK_ypaworld::SetReplayCameraMode(ReplayCamera::Mode mode, const World::Frame *frame)
{
    ReplayCamera &cam = _replayCamera;

    if ( mode == ReplayCamera::PLAYER )
    {
        // Following a player already: the next one along. Else the one
        // followed before, or the one at the machine that recorded it.
        if ( !frame || frame->Players.empty() )
            return;

        uint32_t next = 0;

        if ( cam.CameraMode == ReplayCamera::PLAYER )
        {
            for ( const World::Frame::Player &p : frame->Players )
            {
                if ( p.Id > cam.Player )
                {
                    next = p.Id;
                    break;
                }
            }
        }
        else if ( frame->FindPlayer(cam.Player) )
        {
            next = cam.Player;
        }
        else if ( frame->FindPlayer(frame->LocalPlayer) )
        {
            next = frame->LocalPlayer;
        }

        FollowReplayPlayer(next ? next : frame->Players.front().Id);
        return;
    }

    if ( mode == ReplayCamera::MANUAL )
    {
        // The unit last looked from, or the one the player followed saw from
        if ( !frame || !frame->FindUnit(cam.Unit) )
            cam.Unit = cam.PlayerUnit;

        LookFromUnit(cam.Unit);
        return;
    }

    if ( mode == ReplayCamera::FREE && cam.CameraMode != ReplayCamera::FREE )
    {
        // Off from where it looks now, level
        vec3d forward = cam.Rotation.AxisZ();
        cam.Yaw = std::atan2(forward.x, forward.z);
        cam.Pitch = std::asin(std::max(-1.0, std::min(1.0, -forward.y)));
        cam.Pitch = std::max(-1.5, std::min(1.5, cam.Pitch));
        cam.Velocity = vec3d();
    }

    if ( mode != ReplayCamera::FREE )
        SDL_SetWindowRelativeMouseMode(System::GetWindow(), false);

    cam.CameraMode = mode;
    cam.Looking = false;
    _hud.switch_time = _replayClock;
}

// The free camera flies with W, A, S and D, down and up with Q and E,
// faster with shift, easing into its speed and out of it, and turns with
// the mouse while the right button is held
void NC_STACK_ypaworld::FlyReplayCamera(TInputState *inpt)
{
    ReplayCamera &cam = _replayCamera;
    double dt = _frameTime / 1024.0;
    TClickBoxInf &click = inpt->ClickInf;

    bool looking = (click.flag & TClickBoxInf::FLAG_RM_HOLD) && (cam.Looking || !click.selected_btn);

    if ( looking != cam.Looking )
    {
        cam.Looking = looking;
        SDL_SetWindowRelativeMouseMode(System::GetWindow(), looking);
    }
    else if ( looking )
    {
        cam.Yaw += click.moved.x * FreeCameraTurn;
        cam.Pitch -= click.moved.y * FreeCameraTurn;
        cam.Pitch = std::max(-1.5, std::min(1.5, cam.Pitch));
    }

    // Up is towards -y; the rotation's rows go right, down and forward
    vec3d forward(std::sin(cam.Yaw) * std::cos(cam.Pitch), -std::sin(cam.Pitch), std::cos(cam.Yaw) * std::cos(cam.Pitch));
    vec3d right = vec3d::OY(1.0) * forward;
    right.normalise();
    vec3d down = forward * right;

    cam.Rotation = mat3x3::Basis(right, down, forward);

    auto held = [](int16_t key) { return Input::Engine.KeyMatrix.at(key).down; };

    vec3d want;

    if ( held(Input::KC_W) )
        want += forward;
    if ( held(Input::KC_S) )
        want -= forward;
    if ( held(Input::KC_D) )
        want += right;
    if ( held(Input::KC_A) )
        want -= right;
    if ( held(Input::KC_E) )
        want -= vec3d::OY(1.0);
    if ( held(Input::KC_Q) )
        want += vec3d::OY(1.0);

    if ( want.normalise() > 0.0 )
        want *= held(Input::KC_SHIFT) ? FreeCameraFastSpeed : FreeCameraSpeed;

    cam.Velocity += (want - cam.Velocity) * (1.0 - std::exp(-dt * FreeCameraEase));
    cam.Position += cam.Velocity * dt;

    // Over the map, and above its ground
    cam.Position.x = std::max(0.0, std::min(cam.Position.x, (double)_mapSize.x * World::CVSectorLength));
    cam.Position.z = std::min(0.0, std::max(cam.Position.z, -(double)_mapSize.y * World::CVSectorLength));

    Common::Point sector = World::PositionToSectorID(cam.Position);

    if ( IsSector(sector) && cam.Position.y > _cells(sector).height - FreeCameraClearance )
    {
        cam.Position.y = _cells(sector).height - FreeCameraClearance;
        cam.Velocity.y = std::min(cam.Velocity.y, 0.0);
    }
}

// Where the frame is drawn from, as the way to look has it, and the unit
// not drawn as it's looked from
void NC_STACK_ypaworld::PlaceReplayCamera(World::Frame &frame)
{
    ReplayCamera &cam = _replayCamera;

    // At the start: the player at the machine that recorded it, or else the
    // first; with no players at all, the first host station, or with none,
    // the first unit that isn't a missile or a gun
    if ( cam.CameraMode == ReplayCamera::PLAYER && !cam.Player )
    {
        if ( !frame.Players.empty() )
            SetReplayCameraMode(ReplayCamera::PLAYER, &frame);
        else
        {
            const World::Frame::Unit *first = NULL;

            for ( const World::Frame::Unit &u : frame.Units )
            {
                if ( u.Type == BACT_TYPES_ROBO )
                {
                    first = &u;
                    break;
                }

                if ( !first && u.Type != BACT_TYPES_MISSLE && u.Type != BACT_TYPES_GUN )
                    first = &u;
            }

            if ( first )
                LookFromUnit(first->Id);
        }
    }

    switch ( cam.CameraMode )
    {
    case ReplayCamera::PLAYER:
        if ( frame.FindPlayer(cam.Player) )
        {
            frame.LookAs(cam.Player);
            cam.PlayerUnit = frame.Viewer;
            cam.Position = vec3d(frame.CameraPosition);
            cam.Rotation = mat3x3(frame.CameraRotation);
            _cameraOverridden = false;
            return;
        }

        // Gone, or not there yet: from where they last looked
        frame.Viewer = 0;
        break;

    case ReplayCamera::MANUAL:
        if ( const World::Frame::Unit *unit = frame.FindUnit(cam.Unit) )
        {
            if ( const World::Frame::UnitInfo *info = frame.FindInfo(cam.Unit) )
                cam.UnitOwner = info->Owner;

            cam.Position = vec3d(unit->Position);
            cam.Rotation = mat3x3(unit->Rotation);

            // A host station is looked from its bridge
            if ( unit->Type == BACT_TYPES_ROBO && unit->Vehicle < _vhclProtos.size() && _vhclProtos[unit->Vehicle].RoboProto )
                cam.Position += cam.Rotation.Transpose().Transform(_vhclProtos[unit->Vehicle].RoboProto->viewer);
        }

        frame.Viewer = cam.Unit;
        break;

    default:
        frame.Viewer = 0;
        break;
    }

    OverrideCamera(cam.Position, cam.Rotation);
}

// The unit under the pointer in the world as it was last drawn: the one
// drawn nearest it, within its size; 0 for none
uint32_t NC_STACK_ypaworld::ReplayUnitInWorld(const Common::Point &mouse, const World::Frame &frame) const
{
    mat3x3 corrected = _replayCamera.Rotation;
    GFX::Engine.matrixAspectCorrection(corrected, false);

    float mouseX = 2.0 * mouse.x / _screenSize.x - 1.0;
    float mouseY = 2.0 * mouse.y / _screenSize.y - 1.0;

    uint32_t viewed = ReplayViewedUnit(frame);
    uint32_t best = 0;
    float bestDist = 0.0;

    for (size_t i = 0; i < frame.Units.size(); i++)
    {
        const World::Frame::Unit &u = frame.Units[i];
        const World::Frame::UnitInfo *info = frame.InfoAt(i);

        if ( u.Id == viewed || u.Type == BACT_TYPES_MISSLE || (u.StatusFlags & BACT_STFLAG_NORENDER) || !info ||
             info->Status == BACT_STATUS_DEAD || info->Status == BACT_STATUS_CREATE || info->Status == BACT_STATUS_BEAM )
            continue;

        vec3d view = corrected.Transform(vec3d(u.Position) - _replayCamera.Position);

        if ( view.z < 30.0 )
            continue;

        float radius = u.Vehicle < _vhclProtos.size() ? std::max(_vhclProtos[u.Vehicle].radius, 30.0f) : 50.0f;
        float reach = std::max(radius / (float)view.z, 0.03f);
        float dist = std::hypot(view.x / view.z - mouseX, view.y / view.z - mouseY);

        if ( dist < reach && (!best || dist < bestDist) )
        {
            best = u.Id;
            bestDist = dist;
        }
    }

    return best;
}

// The windows: dragged, closed, scrolled and resized as the game's, and the
// map clicked while flying freely moves the camera there. Then what's under
// the pointer: a unit in a faction's window, on the map or in the world,
// which a double click looks from. The double click comes as the button
// goes up the second time, the pointer where it pressed it.
void NC_STACK_ypaworld::HandleReplayWindows(TInputState *inpt, const World::Frame &frame)
{
    TClickBoxInf &click = inpt->ClickInf;

    if ( !(click.flag & TClickBoxInf::FLAG_OK) )
        return;

    HandleGuiWindows(inpt);
    _viewDistanceWindow.HandleInput(this, inpt);

    uint32_t hovered = 0;

    for (ReplayFactionWindow &window : _replayFactions)
    {
        if ( !window.Created() )
            continue;

        // Dragged, it stays where it's put
        if ( _guiDragging && _guiDragElement == &window )
            window.Moved = true;

        window.HandleInput(this, inpt);

        if ( window.Hovered() )
            hovered = window.Hovered();
    }

    RoboMap_InputHandle(this, inpt);

    bool onMap = robo_map.IsOpen() && click.selected_btn == &robo_map && click.selected_btnID == 17;
    MapScene::Unit mapUnit;

    if ( onMap && RoboMapUnitAt(this, &click, mapUnit) )
        hovered = mapUnit.Id;

    if ( onMap && _replayCamera.CameraMode == ReplayCamera::FREE && (click.flag & TClickBoxInf::FLAG_LM_DOWN) )
    {
        vec2d at = RoboMapWorldAt(&click);
        _replayCamera.Position.x = at.x;
        _replayCamera.Position.z = at.y;
        _replayCamera.Velocity = vec3d();
    }

    if ( !click.selected_btn && click.move.ScreenPos.y < LayOutReplayBar(this).Top && !_replayDragging && !_replayCamera.Looking )
        hovered = ReplayUnitInWorld(click.move.ScreenPos, frame);

    _replayHovered = hovered;

    if ( click.flag & TClickBoxInf::FLAG_LM_DOWN )
        _replayPressed = hovered;

    if ( click.flag & TClickBoxInf::FLAG_DBL_CLICK )
    {
        LookFromUnit(hovered ? hovered : _replayPressed);
        _replayPressed = 0;
    }

    // As the game shows what a click would select
    if ( !_replayCamera.Looking )
        GFX::Engine.SetCursor(hovered ? 2 : 1, 0);
}

void NC_STACK_ypaworld::HandleReplayInput(TInputState *inpt)
{
    // The windows tell anew what they're about
    ClearTooltip();

    const World::FrameSource &replay = *_replay;
    ReplayBar bar = LayOutReplayBar(this);
    const Common::Point &mouse = inpt->ClickInf.move.ScreenPos;
    bool overWindow = inpt->ClickInf.selected_btn != NULL;

    HandleReplayWindows(inpt, _replayFrame);

    // The track: clicked or held, it goes there; a game watched live only
    // goes on
    bool down = (inpt->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN) && !overWindow;

    if ( down && mouse.y >= bar.TextY + _fontH && mouse.y < bar.KeysY && !replay.Live() )
        _replayDragging = true;

    if ( _replayDragging )
    {
        _replayPosition = ReplayStepAt(bar, replay, mouse.x);

        if ( !(inpt->ClickInf.flag & TClickBoxInf::FLAG_LM_HOLD) || (inpt->ClickInf.flag & TClickBoxInf::FLAG_LM_UP) )
            _replayDragging = false;
    }
    else if ( down && mouse.y >= bar.Top )
    {
        // A way to look's button, or anywhere else above the track to
        // pause and play
        bool onButton = false;

        for ( const ReplayBar::Button &b : bar.Buttons )
        {
            if ( mouse.y < bar.TextY + _fontH && mouse.x >= b.X && mouse.x < b.X + b.W )
            {
                if ( b.Mode == ReplayCamera::PLAYER )
                    FollowReplayPlayer(b.Player);
                else
                    SetReplayCameraMode(b.Mode, &_replayFrame);

                onButton = true;
            }
        }

        if ( !onButton && mouse.y < bar.TextY + _fontH )
            ToggleReplayPause();
    }

    int key = inpt->KbdLastHit;

    // Live, it can't be gone back in, sped up or paused
    if ( replay.Live() && (key == Input::KC_PAUSE || key == Input::KC_LEFT || key == Input::KC_RIGHT || key == Input::KC_UP ||
                           key == Input::KC_DOWN || key == Input::KC_HOME || key == Input::KC_END) )
        key = Input::KC_NONE;

    switch ( key )
    {
    case Input::KC_ESCAPE:
        _replayLeave = true;
        break;

    case Input::KC_SPACE:
        // As in the game, once the unit looked from is gone: to the next
        // of its faction
        if ( _replayCamera.CameraMode == ReplayCamera::MANUAL && LookFromNextUnit(_replayFrame) )
            break;

        ToggleReplayPause();
        break;

    case Input::KC_PAUSE:
        ToggleReplayPause();
        break;

    case Input::KC_LEFT:
        _replayPosition -= ReplaySkipSteps;
        break;

    case Input::KC_RIGHT:
        _replayPosition += ReplaySkipSteps;
        break;

    case Input::KC_UP:
        _replaySpeed = std::min(_replaySpeed + 1, (int)ReplaySpeeds.size() - 1);
        break;

    case Input::KC_DOWN:
        _replaySpeed = std::max(_replaySpeed - 1, 0);
        break;

    case Input::KC_HOME:
        _replayPosition = replay.FirstStep();
        break;

    case Input::KC_END:
        _replayPosition = replay.LastStep();
        break;

    case Input::KC_1:
        SetReplayCameraMode(ReplayCamera::PLAYER, &_replayFrame);
        break;

    case Input::KC_2:
        SetReplayCameraMode(ReplayCamera::MANUAL, &_replayFrame);
        break;

    case Input::KC_3:
        SetReplayCameraMode(ReplayCamera::FREE, &_replayFrame);
        break;

    case Input::KC_M:
        if ( robo_map.IsOpen() )
            GuiWinClose(&robo_map);
        else
            GuiWinOpen(&robo_map);
        break;

    case Input::KC_U:
        _replayWindowsShown = !_replayWindowsShown;

        for (ReplayFactionWindow &window : _replayFactions)
        {
            if ( !window.Created() )
                continue;

            if ( _replayWindowsShown )
                GuiWinOpen(&window);
            else
                GuiWinClose(&window);
        }
        break;

    case Input::KC_H:
        _replayHudShown = !_replayHudShown;
        break;

    default:
        break;
    }

    _replayPosition = std::max(_replayPosition, (double)replay.FirstStep());
    _replayPosition = std::min(_replayPosition, (double)replay.LastStep());

    if ( _replayCamera.CameraMode == ReplayCamera::FREE )
        FlyReplayCamera(inpt);
}

// Text from a point on, in the timeline's font
static void ReplayText(NC_STACK_ypaworld *yw, CmdStream *cmds, int x, int y, int width, int flags, const std::string &text)
{
    FontUA::set_xpos(cmds, x);
    FontUA::set_ypos(cmds, y);
    FontUA::copy_position(cmds);
    FontUA::add_txt(cmds, width, flags, text);
}

void NC_STACK_ypaworld::DrawReplayControls()
{
    const World::FrameSource &replay = *_replay;
    ReplayBar bar = LayOutReplayBar(this);

    // The panel
    GFX::Engine.SetPen( SDL_Color{16, 16, 24, 255} );
    FillScreenBox(this, 0, bar.Top, _screenSize.x, _screenSize.y - bar.Top);

    // The track, the part played, and where it's at
    double span = std::max(1u, replay.LastStep() - replay.FirstStep());
    int at = bar.TrackX0 + (int)((bar.TrackX1 - bar.TrackX0) * (_replayPosition - replay.FirstStep()) / span);
    int thickness = std::max(3, _fontH / 4);

    GFX::Engine.SetPen( SDL_Color{80, 80, 96, 255} );
    FillScreenBox(this, bar.TrackX0, bar.TrackY - thickness / 2, bar.TrackX1 - bar.TrackX0, thickness);

    GFX::Engine.SetPen( SDL_Color{230, 160, 40, 255} );
    FillScreenBox(this, bar.TrackX0, bar.TrackY - thickness / 2, at - bar.TrackX0, thickness);

    int knobW = std::max(4, thickness * 2);
    int knobH = std::max(10, _fontH * 2 / 3);
    GFX::Engine.SetPen( SDL_Color{240, 240, 240, 255} );
    FillScreenBox(this, at - knobW / 2, bar.TrackY - knobH / 2, knobW, knobH);

    // The ways to look, the one looked with lit
    auto lit = [this](const ReplayBar::Button &b)
    {
        return b.Mode == _replayCamera.CameraMode && (b.Mode != ReplayCamera::PLAYER || b.Player == _replayCamera.Player);
    };

    for ( const ReplayBar::Button &b : bar.Buttons )
    {
        if ( lit(b) )
            GFX::Engine.SetPen( SDL_Color{230, 160, 40, 255} );
        else
            GFX::Engine.SetPen( SDL_Color{48, 48, 64, 255} );

        FillScreenBox(this, b.X, bar.TextY - bar.Pad / 2, b.W, _fontH + bar.Pad);
    }

    // Where it's at of how long it is and how fast it plays, the ways to
    // look's names, and the keys
    int32_t now = (int32_t)((_replayPosition - replay.FirstStep()) * World::CVStepTicks);
    std::string state;

    if ( _replayPaused )
        state = Locale::Text::Title(Locale::TITLE_REPLAYPAUSED);
    else if ( _replaySpeed != ReplayNormalSpeed )
        state = fmt::sprintf("x%g", ReplaySpeeds[_replaySpeed]);

    std::string left = fmt::sprintf("%s / %s   %s", ReplayTime(now), ReplayTime(replay.Duration()), state);

    // Live: how long it's been watched, and whether it still goes on
    if ( replay.Live() )
        left = fmt::sprintf("%s   %s", ReplayTime(replay.Duration()), _replayLiveEnded ? "ENDED" : "LIVE");

    const SDL_Color &txt = _iniColors[World::COLOR_TEXT_HUD];

    CmdStream cmds;
    cmds.reserve(1024);

    FontUA::select_tileset(&cmds, 15);
    FontUA::set_txtColor(&cmds, txt.r, txt.g, txt.b);

    ReplayText(this, &cmds, bar.TrackX0, bar.TextY, bar.Buttons.front().X - bar.TrackX0, 1, left);

    for ( const ReplayBar::Button &b : bar.Buttons )
    {
        if ( lit(b) )
            FontUA::set_txtColor(&cmds, 16, 16, 24);
        else
            FontUA::set_txtColor(&cmds, txt.r, txt.g, txt.b);

        ReplayText(this, &cmds, b.X, bar.TextY, b.W, 1 | 2, b.Title);
    }

    FontUA::set_txtColor(&cmds, txt.r, txt.g, txt.b);

    // The replay's keys at the left, the way to look's at the right
    std::string keys = Locale::Text::Advanced(_replay->Live() ? Locale::ADV_LIVEKEYS : Locale::ADV_REPLAYKEYS);
    std::string modeKeys = Locale::Text::HUD(_replayCamera.CameraMode == ReplayCamera::FREE ? Locale::HUDSTR_REPLAYFREEKEYS
                                                                                              : Locale::HUDSTR_REPLAYVIEWKEYS);
    int half = (bar.TrackX1 - bar.TrackX0) / 2;

    ReplayText(this, &cmds, bar.TrackX0, bar.KeysY, half, 1, keys);
    ReplayText(this, &cmds, bar.TrackX0 + half, bar.KeysY, bar.TrackX1 - bar.TrackX0 - half, 1 | 2, modeKeys);

    FontUA::set_end(&cmds);
    GFX::Engine.ProcessDrawSeq(cmds);
}

// The windows as of a frame: a faction's opens the first time it has a
// host station; until moved, they're stacked down the left side in the
// order of their owners, as tall as their rows
void NC_STACK_ypaworld::UpdateReplayWindows(const World::Frame &frame)
{
    uint32_t viewed = ReplayViewedUnit(frame);

    for (size_t i = 0; i < frame.Units.size(); i++)
    {
        const World::Frame::UnitInfo *info = frame.InfoAt(i);

        if ( frame.Units[i].Type != BACT_TYPES_ROBO || !info || info->Owner == 0 || info->Owner >= _replayFactions.size() )
            continue;

        ReplayFactionWindow &window = _replayFactions[info->Owner];

        if ( window.Created() )
            continue;

        window.Create(this, info->Owner);
        GuiWinAttach(&window);

        if ( _replayWindowsShown )
            GuiWinOpen(&window);
    }

    int top = _upScreenBorder;
    int left = 0;
    int columnWidth = 0;
    int bottom = _screenSize.y - _downScreenBorder;

    for (ReplayFactionWindow &window : _replayFactions)
    {
        if ( !window.Created() || window.IsClosed() )
            continue;

        if ( !window.Moved )
        {
            int least = _fontH + window.upperVborder + window.lowerVborder + window.minShownEntries * window.entryHeight;

            if ( top + least > bottom && top > _upScreenBorder )
            {
                top = _upScreenBorder;
                left += columnWidth;
                columnWidth = 0;
            }

            window.SetRect(this, left, top);
        }

        window.Update(this, frame, viewed);

        if ( !window.Moved )
        {
            top = window.y + window.h;
            columnWidth = std::max(columnWidth, (int)window.w);
        }
    }

    // The map, as the game keeps it up
    RoboMapFollowViewer(this);
    RoboMapUpdateLayout(this);
}

// Each window, and what its text doesn't draw over it, the front one last
void NC_STACK_ypaworld::DrawReplayWindows()
{
    for (GuiBase *window : _guiActive)
    {
        if ( window->IsClosed() )
            continue;

        GFX::Engine.ProcessDrawSeq(window->cmdCommands, &window->cmdInclude);

        if ( window->postDraw )
            window->postDraw(this);

        for (ReplayFactionWindow &faction : _replayFactions)
        {
            if ( window == &faction )
                faction.DrawExtras(this);
        }
    }
}

// The HUD of the unit looked from, as the game shows the player's: none in
// a host station or a missile, and a blinking line once it's gone
void NC_STACK_ypaworld::DrawReplayHud(const World::Frame &frame)
{
    uint32_t viewed = ReplayViewedUnit(frame);
    const World::Frame::Unit *unit = frame.FindUnit(viewed);
    const World::Frame::UnitInfo *info = frame.FindInfo(viewed);

    CmdStream buf;
    buf.reserve(1024);

    // Flown freely, hidden, a missile as the game shows nothing of, or gone
    // back to before it was made: nothing to show of it
    bool shown = viewed && _replayHudShown && !(unit && unit->Type == BACT_TYPES_MISSLE);
    uint32_t lockedOn = 0; // By its sights

    if ( !shown || (!unit && (frame.Units.empty() || viewed > frame.Units.back().Id)) )
        ;
    else if ( !unit || !info || info->Status == BACT_STATUS_DEAD )
    {
        const World::Frame::UnitInfo *robo = frame.FindInfo(frame.UserRobo);
        bool hostStation = (unit && unit->Type == BACT_TYPES_ROBO) || (robo && robo->Status == BACT_STATUS_DEAD &&
                                                                       _replayCamera.CameraMode == ReplayCamera::PLAYER);
        DrawHudDestroyed(this, &buf, hostStation ? Locale::CMN_HSDESTR : Locale::CMN_UNITDESTR);
    }
    else if ( unit->Type != BACT_TYPES_ROBO && unit->Vehicle < _vhclProtos.size() )
    {
        const World::TVhclProto &vhcl = _vhclProtos[unit->Vehicle];

        THudSubject subject;
        subject.Vehicle = unit->Vehicle;
        subject.Position = vec3d(unit->Position);
        subject.Rotation = _replayCamera.Rotation;
        subject.Energy = info->Energy;
        subject.EnergyMax = std::max(info->EnergyMax, 1);
        subject.Shield = info->Shield >= 0 ? info->Shield : vhcl.shield;

        if ( vhcl.weapon != -1 && vhcl.weapon < (int)_weaponProtos.size() )
        {
            int clock = frame.Time - info->ClockStart;
            subject.ReloadPercent = WeaponReloadPercent(_weaponProtos.at(vhcl.weapon), clock - info->WeaponTime, info->SalveCounter);
        }

        const World::Frame::Unit *parent = frame.FindUnit(info->Parent);

        if ( parent )
        {
            subject.HasParent = true;
            subject.ParentPos = vec3d(parent->Position);

            // A commander shows where its squad goes
            if ( parent->Type == BACT_TYPES_ROBO )
            {
                if ( info->PrimaryType == BACT_TGT_TYPE_CELL )
                {
                    subject.HasTarget = true;
                    subject.TargetPos = vec3d(info->PrimaryPos);
                }
                else if ( info->PrimaryType == BACT_TGT_TYPE_UNIT )
                {
                    if ( const World::Frame::Unit *target = frame.FindUnit(info->Primary) )
                    {
                        subject.HasTarget = true;
                        subject.TargetPos = vec3d(target->Position);
                    }
                }
            }
        }

        Common::Point sector = World::PositionToSectorID(unit->Position.x, unit->Position.z);

        const World::Frame::Sector *under = IsSector(sector) ? frame.SectorAt(sector.y * _mapSize.x + sector.x) : NULL;

        if ( under )
            subject.SectorOwner = under->Owner;

        int weaponFlags = vhcl.weapon != -1 && vhcl.weapon < (int)_weaponProtos.size() ? _weaponProtos.at(vhcl.weapon)._weaponFlags : 0;
        subject.Visor = NC_STACK_ypabact::Sights(vhcl.mgun, vhcl.weapon, weaponFlags, -info->GunLeftRight, -info->GunAngle,
                                                 NC_STACK_ypabact::NoWeaponSight(weaponFlags));

        // What they lock on to, for the player who controls it
        if ( const World::Frame::Player *controller = frame.ControllerOf(viewed) )
        {
            if ( const World::Frame::Unit *target = frame.FindUnit(controller->Aim) )
            {
                subject.VisorTarget = lockedOn = target->Id;
                subject.VisorTargetPos = vec3d(target->Position);
                NC_STACK_ypabact::LockSights(subject.Visor, subject.Position, mat3x3(unit->Rotation), subject.VisorTargetPos);
            }
        }

        yw_RenderHUDRadare(this);
        DrawHudOf(this, &buf, subject);
    }

    // The marks over units, as the game's: what's under the pointer, and
    // with the enemy indicator what's around
    MapScene::Unit hovered;

    if ( _replayScene.Hovered(hovered) )
    {
        yw_RenderCursorOverUnit(this, hovered);
        yw_RenderUnitLifeBar(this, &buf, hovered);
    }

    MapScene::Unit locked;

    if ( lockedOn && _replayScene.Find(lockedOn, locked) )
        yw_RenderUnitLifeBar(this, &buf, locked);

    if ( shown && unit )
        yw_RenderOverlayCursors(this, &buf);

    FontUA::set_end(&buf);
    GFX::Engine.ProcessDrawSeq(buf);

    GFX::Engine.SetClipNormalized( Common::FRect(-1.0, -1.0, 1.0, 1.0) );
}

// A frame of the replay: the controls, then the replay drawn at where it's
// at, played on by the frame's time unless paused or held, from where the
// way to look has it, with the windows and the HUD over it
void NC_STACK_ypaworld::ReplayFrame(TUpdateArgs *arg)
{
    if ( arg->DTime > 0 )
        _FPS = 1024 / arg->DTime;

    _frameTime = arg->DTime;
    _replayClock += arg->DTime;

    // Nothing of it is heard
    SFXEngine::SFXe.BeginSoundFrame(1, vec3d(0.0, 50000.0, 0.0), vec3d(0.0), mat3x3::Ident());
    SFXEngine::SFXe.EndSoundFrame();

    // Playing it, the game's interface takes the controls
    if ( (_replay->Player() != 0) != _replayPlaying )
    {
        if ( _replayPlaying )
            StopPlaying();
        else if ( _replayFrame.FindPlayer(_replay->Player()) )
            StartPlaying();
    }

    if ( !_replayPlaying )
        HandleReplayInput(arg->Input);

    bool playing = !_replayPaused && !_replayDragging;

    if ( _replay->Live() )
    {
        // What came, shown a few steps behind the latest so that steps
        // coming unevenly still play evenly. Fallen behind, as after a
        // stall, it plays faster up to there; far behind, it jumps.
        if ( !_replayLiveEnded && !_replay->Update() )
            _replayLiveEnded = true;

        double newest = (double)_replay->LastStep() - LiveLagSteps;
        double behind = newest - _replayPosition;

        if ( behind > LiveJumpSteps )
            _replayPosition = newest;
        else
            _replayPosition += (double)arg->DTime / World::CVStepTicks * (1.0 + std::max(0.0, behind / LiveCatchUpSteps));

        _replayPosition = std::min(_replayPosition, newest);
        _replayPosition = std::max(_replayPosition, (double)_replay->FirstStep());
    }
    else if ( playing )
    {
        _replayPosition += (double)arg->DTime / World::CVStepTicks * ReplaySpeeds[_replaySpeed];

        if ( _replayPosition >= _replay->LastStep() )
        {
            _replayPosition = _replay->LastStep();
            _replayPaused = true;
        }
    }

    GFX::Engine.BeginFrame();

    World::Frame frame;
    bool drawn = _replay->FrameAt(_replayPosition, frame);

    // Gone elsewhere: what was drawn moving goes
    if ( _replay->Jumped() )
    {
        _particles.Clear();
        FreeDrawings();
    }

    if ( drawn )
    {
        for (auto it = _unitDrawings.begin(); it != _unitDrawings.end(); )
        {
            if ( !frame.FindUnit(it->first) )
                it = _unitDrawings.erase(it);
            else
                it++;
        }

        PlaceReplayCamera(frame);
        _replayFrame = std::move(frame);

        // What the game's map and the marks over units show, and where
        // they're seen from
        _replayScene.Set(this, &_replayFrame, ReplayViewedUnit(_replayFrame), _replayCamera.Position, _replayCamera.Rotation,
                         _replayHovered);
        _viewerPosition = _replayCamera.Position;
        _viewerRotation = _replayCamera.Rotation;

        // Smoke and sparks come as it plays on, at speeds they keep up with
        GFX::Engine.SetAnimationTime(_replayFrame.Time);
        RenderGame(arg, _replayFrame, false, playing && ReplaySpeeds[_replaySpeed] <= 2.0);

        // The interface animates in the viewer's own time, paused or not
        int32_t gameTime = _timeStamp;
        _timeStamp = _replayClock;

        if ( _replayPlaying )
        {
            // The game's interface, the HUD under its windows as in the game
            PlayFromFrame(_replayFrame);
            PlayInterfaceStep(arg->Input);
            DrawReplayHud(_replayFrame);
            DrawGameWindows(this);
        }
        else
        {
            // The HUD over the windows: they'd hide its bars
            UpdateReplayWindows(_replayFrame);
            DrawReplayWindows();
            DrawReplayHud(_replayFrame);
            draw_tooltip(this);
        }

        _timeStamp = gameTime;
    }
    else
    {
        ypa_log_out("The replay can't be played on: leaving it\n");
        _replayLeave = true;
    }

    if ( !_replayPlaying )
        DrawReplayControls();

    DrawPerfOverlay(arg->Input);

    GFX::Engine.EndFrame();

    HandleSnapshotKeys(this, arg->Input);
}


// Playing a game watched live: the game's own interface instead of the
// viewer's, following the player played as, its orders sent to the game
void NC_STACK_ypaworld::StartPlaying()
{
    CloseReplayWindows();
    FreeHud(this);

    // The game's layout, between the energy panel and the bottom bar
    _upScreenBorder = _replayBorders.x;
    _downScreenBorder = _replayBorders.y;

    _cmdrsRemap.clear();
    _activeCmdrID = 0;
    _activeCmdrRemapIndex = -1;
    _cmdrIdToSelect = -1;
    _bactOnMouse = MapScene::Unit();
    _cellOnMouse = Common::Point(-1, -1);
    _playUnit = 0;
    _playSelectNewSquad = false;
    _playerInHSGun = false;
    _levelInfo.State = TLevelInfo::STATE_PLAYING;

    _playScene.SetPlayer(this, &_replayFrame, _replay->Player(), _replayCamera.Position, _replayCamera.Rotation);
    _uiScene = &_playScene;
    _orderSink = [this](const World::PlayerOrder &order) { _replay->SendOrder(order); };

    // Its message log begins at the game's time, shown by the interface's
    // own clock
    int32_t gameTime = _timeStamp;
    _timeStamp = _replayFrame.Time;
    GUI_Open(this, &_playScene);
    _timeStamp = gameTime;

    info_log.msgs[0].timestamp = _replayClock + 1;
    info_log.last_msg_time = _replayClock;

    _replayPlaying = true;
    FollowReplayPlayer(_replay->Player());
    GFX::Engine.SetCursor(1, 0);
}

void NC_STACK_ypaworld::StopPlaying()
{
    GUI_Close();

    _uiScene = NULL;
    _orderSink = nullptr;
    _cmdrsRemap.clear();
    _activeCmdrRemapIndex = -1;
    _replayPlaying = false;

    OpenReplayWindows();
    GFX::Engine.SetCursor(1, 0);
}

// What of the game the interface goes by, as a frame has it: the
// prototypes it offers, the sectors the mouse picks on, and the unit the
// player is in
void NC_STACK_ypaworld::PlayFromFrame(const World::Frame &frame)
{
    for ( size_t i = 0; i < frame.Vehicles.size() && i < _vhclProtos.size(); i++ )
    {
        _vhclProtos[i].disable_enable_bitmask = frame.Vehicles[i].Enable;
        _vhclProtos[i].energy = frame.Vehicles[i].Energy;
    }

    for ( size_t i = 0; i < frame.Buildings.size() && i < _buildProtos.size(); i++ )
    {
        _buildProtos[i].EnableMask = frame.Buildings[i].Enable;
        _buildProtos[i].Energy = frame.Buildings[i].Energy;
    }

    if ( frame.Sectors )
    {
        for ( size_t i = 0; i < frame.Sectors->size() && i < _cells.size(); i++ )
        {
            int32_t type = (*frame.Sectors)[i].Type;

            if ( type >= 0 && type < (int32_t)_secTypeArray.size() )
                _cells[i].type_id = type;
        }
    }

    // The last tech upgrade, timed by the interface's own clock
    _upgradeId = frame.Upgrade.Id;
    _upgradeTimeStamp = _replayClock - (frame.Time - (int32_t)frame.Upgrade.Time);
    _upgradeVehicleId = frame.Upgrade.Vehicle;
    _upgradeWeaponId = frame.Upgrade.Weapon;
    _upgradeBuildId = frame.Upgrade.Building;

    _playScene.SetPlayer(this, &frame, _replay->Player(), _replayCamera.Position, _replayCamera.Rotation);

    // Got in another unit: what the game does for the player then
    MapScene::Unit unit;

    if ( _playScene.UserUnit(unit) && unit.Id && unit.Id != _playUnit )
    {
        if ( _playUnit )
            _prevUnitId = _playUnit;

        _playUnit = unit.Id;
        _vehicleTakenControlTimestamp = _timeStamp;
        _vehicleTakenCommandId = unit.CommandId;
        _guiDragDefaultMouse = false;
    }
}

// A step of the game's interface, as SimulationStep runs it, on the frame
// shown: what it lists, then the windows' and the mouse's input
void NC_STACK_ypaworld::PlayInterfaceStep(TInputState *inpt)
{
    // The windows tell anew what they're about
    ClearTooltip();

    // What the game takes of the input before its interface does
    TakeDialogKeys(inpt);
    TrackFireButton(inpt);
    UpdatePlayerInRoboGun(this);

    // A squad just ordered is selected once it's there, as the game selects
    // the one its host station makes
    if ( _playSelectNewSquad )
    {
        uint8_t owner = _playScene.PlayerOwner();

        _playScene.ForEachSquad(owner, [this](const MapScene::Unit &commander)
        {
            if ( _playSelectNewSquad && commander.Type != BACT_TYPES_GUN &&
                 std::find(_playSquadsBefore.begin(), _playSquadsBefore.end(), commander.Id) == _playSquadsBefore.end() )
            {
                _cmdrIdToSelect = commander.CommandId;
                _playSelectNewSquad = false;
            }
        });
    }

    UpdateGunList(this);
    UpdateVehicleList(this);
    UpdateBuildingList(this);
    UpdateCommanderList();

    RoboMapFollowViewer(this);
    HandleGuiInput(inpt);
    RoboMapUpdateLayout(this);
    HandleWorldMouse(inpt);

    // Left from the game's menu
    if ( _levelInfo.State == TLevelInfo::STATE_ABORTED )
        _replayLeave = true;
}
