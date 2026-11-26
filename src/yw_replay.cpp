#include <algorithm>
#include <cmath>

#include "includes.h"
#include "yw.h"
#include "yw_internal.h"
#include "font.h"
#include "log.h"
#include "world/playback.h"

// The replay viewer: a recording played back in its level as it was drawn,
// from where the player looked, with a timeline at the bottom to go to any
// moment of it

// How fast it can play, times real time
static const std::array<double, 7> ReplaySpeeds = {0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0};
static const int ReplayNormalSpeed = 2;

// The arrow keys skip that far, in steps: 10 s
static const double ReplaySkipSteps = 600.0;

// Game time as minutes and seconds, or hours too
static std::string ReplayTime(int32_t ticks)
{
    int seconds = std::max(ticks, 0) / 1024;

    if ( seconds >= 3600 )
        return fmt::sprintf("%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);

    return fmt::sprintf("%d:%02d", seconds / 60, seconds % 60);
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

    // Set first: failing to set the level up deletes it only as a replay's
    _replay = std::move(replay);

    if ( !LoadLevelData(_replay->GetRecording().Level()) )
    {
        ypa_log_out("Could not set level %d up to watch %s\n", _replay->GetRecording().Level(), path.c_str());
        _replay.reset();
        return false;
    }

    _replayPosition = std::min(std::max(startStep, _replay->FirstStep()), _replay->LastStep());
    _replayPaused = false;
    _replaySpeed = ReplayNormalSpeed;
    _replayDragging = false;
    _replayLeave = false;
    _cameraShake = mat3x3::Ident();
    _particles.Clear();

    GFX::Engine.SetCursor(1, 0);
    return true;
}

// The level goes, as a replay's: nothing of it was played
void NC_STACK_ypaworld::EndReplay()
{
    FreeLevel();
    _replay.reset();
    _levelInfo.State = TLevelInfo::STATE_MENU;
}

// Where on the screen the timeline is, in pixels from the top left
struct ReplayBar
{
    int Top = 0;           // Of the panel, which goes to the bottom
    int TextY = 0;
    int TrackX0 = 0, TrackX1 = 0, TrackY = 0;
    int Pad = 0;
};

static ReplayBar LayOutReplayBar(const NC_STACK_ypaworld *yw)
{
    ReplayBar bar;
    bar.Pad = std::max(4, yw->_fontH / 2);
    bar.Top = yw->_screenSize.y - (2 * yw->_fontH + 3 * bar.Pad);
    bar.TextY = bar.Top + bar.Pad;
    bar.TrackY = bar.TextY + yw->_fontH + bar.Pad + yw->_fontH / 2;
    bar.TrackX0 = 2 * bar.Pad;
    bar.TrackX1 = yw->_screenSize.x - 2 * bar.Pad;
    return bar;
}

// The step at a point of the track
static double ReplayStepAt(const ReplayBar &bar, const World::Playback &replay, int x)
{
    double t = (double)(x - bar.TrackX0) / std::max(1, bar.TrackX1 - bar.TrackX0);
    t = std::max(0.0, std::min(1.0, t));
    return replay.FirstStep() + t * (replay.LastStep() - replay.FirstStep());
}

// At the end it waits there; played again, it starts over
void NC_STACK_ypaworld::ToggleReplayPause()
{
    if ( _replayPaused && _replayPosition >= _replay->LastStep() )
        _replayPosition = _replay->FirstStep();

    _replayPaused = !_replayPaused;
}

void NC_STACK_ypaworld::HandleReplayInput(TInputState *inpt)
{
    const World::Playback &replay = *_replay;
    ReplayBar bar = LayOutReplayBar(this);
    const Common::Point &mouse = inpt->ClickInf.move.ScreenPos;

    // The track: clicked or held, it goes there
    if ( (inpt->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN) && mouse.y >= bar.TextY + _fontH )
        _replayDragging = true;

    if ( _replayDragging )
    {
        _replayPosition = ReplayStepAt(bar, replay, mouse.x);

        if ( !(inpt->ClickInf.flag & TClickBoxInf::FLAG_LM_HOLD) || (inpt->ClickInf.flag & TClickBoxInf::FLAG_LM_UP) )
            _replayDragging = false;
    }
    else if ( (inpt->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN) && mouse.y >= bar.Top )
    {
        // Above the track, it pauses and plays
        ToggleReplayPause();
    }

    switch ( inpt->KbdLastHit )
    {
    case Input::KC_ESCAPE:
        _replayLeave = true;
        break;

    case Input::KC_SPACE:
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

    default:
        break;
    }

    _replayPosition = std::max(_replayPosition, (double)replay.FirstStep());
    _replayPosition = std::min(_replayPosition, (double)replay.LastStep());
}

// A box filled with the pen's colour, in pixels from the top left
static void FillScreenBox(const NC_STACK_ypaworld *yw, int x, int y, int w, int h)
{
    int cx = yw->_screenSize.x / 2;
    int cy = yw->_screenSize.y / 2;

    for (int row = y; row < y + h; row++)
        GFX::Engine.DrawLineCentered(Common::Line(x - cx, row - cy, x + w - 1 - cx, row - cy));
}

void NC_STACK_ypaworld::DrawReplayControls()
{
    const World::Playback &replay = *_replay;
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

    // Where it's at of how long it is, how fast it plays, and the keys
    int32_t now = (int32_t)((_replayPosition - replay.FirstStep()) * World::Recording::StepTicks);
    std::string state;

    if ( _replayPaused )
        state = Locale::Text::Title(Locale::TITLE_REPLAYPAUSED);
    else if ( _replaySpeed != ReplayNormalSpeed )
        state = fmt::sprintf("x%g", ReplaySpeeds[_replaySpeed]);

    std::string left = fmt::sprintf("%s / %s   %s", ReplayTime(now), ReplayTime(replay.Duration()), state);

    const SDL_Color &txt = _iniColors[World::COLOR_TEXT_HUD];

    CmdStream cmds;
    cmds.reserve(512);

    FontUA::select_tileset(&cmds, 15);
    FontUA::set_txtColor(&cmds, txt.r, txt.g, txt.b);

    FontUA::set_xpos(&cmds, bar.TrackX0);
    FontUA::set_ypos(&cmds, bar.TextY);
    FontUA::copy_position(&cmds);
    FontUA::add_txt(&cmds, _screenSize.x / 2, 1, left);

    FontUA::set_xpos(&cmds, _screenSize.x / 2);
    FontUA::set_ypos(&cmds, bar.TextY);
    FontUA::copy_position(&cmds);
    FontUA::add_txt(&cmds, bar.TrackX1 - _screenSize.x / 2, 1 | 2, Locale::Text::Advanced(Locale::ADV_REPLAYKEYS));

    FontUA::set_end(&cmds);
    GFX::Engine.ProcessDrawSeq(cmds);
}

// A frame of the replay: the controls, then the replay drawn at where it's
// at, played on by the frame's time unless paused or held
void NC_STACK_ypaworld::ReplayFrame(TUpdateArgs *arg)
{
    if ( arg->DTime > 0 )
        _FPS = 1024 / arg->DTime;

    _frameTime = arg->DTime;

    // Nothing of it is heard
    SFXEngine::SFXe.BeginSoundFrame(1, vec3d(0.0, 50000.0, 0.0), vec3d(0.0), mat3x3::Ident());
    SFXEngine::SFXe.EndSoundFrame();

    HandleReplayInput(arg->Input);

    bool playing = !_replayPaused && !_replayDragging;

    if ( playing )
    {
        _replayPosition += (double)arg->DTime / World::Recording::StepTicks * ReplaySpeeds[_replaySpeed];

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

        // Smoke and sparks come as it plays on, at speeds they keep up with
        GFX::Engine.SetAnimationTime(frame.Time);
        RenderGame(arg, frame, false, playing && ReplaySpeeds[_replaySpeed] <= 2.0);
    }
    else
    {
        ypa_log_out("The replay can't be played on: leaving it\n");
        _replayLeave = true;
    }

    DrawReplayControls();

    GFX::Engine.EndFrame();

    HandleSnapshotKeys(this, arg->Input);
}
