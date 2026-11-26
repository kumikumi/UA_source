#include <algorithm>

#include "../log.h"
#include "playback.h"

namespace World
{

bool Playback::Open(const std::string &path)
{
    _reader.reset();
    _frames.clear();

    if ( !_recording.Open(path) )
        return false;

    if ( _recording.Keyframes().empty() )
    {
        ypa_log_out("%s has no keyframe to play back from\n", path.c_str());
        return false;
    }

    // The recording's own view, whichever build made it, with what the
    // viewer's interface shows too
    _maker.reset(new FrameMaker(_recording.ViewSchema(), true));
    _reader.reset(new ViewReader(_recording));
    _duration = (LastStep() - FirstStep()) * Recording::StepTicks;
    return true;
}

int32_t Playback::Duration() const
{
    return _duration;
}

bool Playback::FrameAt(double step, Frame &out)
{
    _jumped = false;

    if ( !_reader )
        return false;

    step = std::max(step, (double)FirstStep());
    step = std::min(step, (double)LastStep());

    uint32_t whole = (uint32_t)step;
    uint32_t next = std::min(whole + 1, LastStep());

    if ( !MakeUpTo(whole, next) )
        return false;

    const Frame &a = _frames[whole - _frames.front().Step];
    const Frame &b = _frames[next - _frames.front().Step];

    Frame::Between(a, b, step - whole, out);
    return true;
}

// Makes the frames from one step up to another, keeping no older ones
bool Playback::MakeUpTo(uint32_t from, uint32_t to)
{
    bool kept = !_frames.empty() && _frames.front().Step <= from;

    if ( kept && _frames.back().Step >= to )
        return true;

    // Reading on from the last frame kept, the reader's step, is no slower
    // than starting at a keyframe
    if ( !kept || to - _reader->Step() > Recorder::KeyframeSteps )
    {
        const KeyframeAt *at = _recording.KeyframeAtOrBefore(from);

        _frames.clear();
        _jumped = true;

        if ( !at || !_reader->Seek(at->Step) )
        {
            ypa_log_out("Could not play %s back from step %d\n", _recording.Path().c_str(), from);
            return false;
        }

        if ( at->Step >= from )
        {
            _frames.emplace_back();
            _maker->FromView(_reader->View(), _reader->Step(), NULL, _frames.back());
        }
    }

    while ( _reader->Step() < to )
    {
        if ( !_reader->Next() )
        {
            ypa_log_out("Could not play %s back on after step %d\n", _recording.Path().c_str(), _reader->Step());
            _frames.clear();
            return false;
        }

        // Only the frames that are drawn are made
        if ( _reader->Step() >= from )
        {
            Frame frame;
            _maker->FromView(_reader->View(), _reader->Step(), _frames.empty() ? NULL : &_frames.back(), frame);
            _frames.push_back(std::move(frame));
        }
    }

    while ( !_frames.empty() && _frames.front().Step < from )
        _frames.pop_front();

    return !_frames.empty() && _frames.front().Step == from && _frames.back().Step >= to;
}

}
