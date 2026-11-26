#ifndef WORLD_PLAYBACK_H_INCLUDED
#define WORLD_PLAYBACK_H_INCLUDED

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "frame.h"
#include "recorder.h"

namespace World
{

// A recording played back as it was drawn, from any step: the frames of the
// steps around where it's at are made from its file as they're needed, so
// that however long it is, it takes as little memory as a short one. Going
// on reads the file on; going back or far ahead starts from the keyframe
// before the step.
class Playback
{
public:
    // False, after logging why, when it isn't a recording or its view
    // can't be drawn
    bool Open(const std::string &path);

    const Recording &GetRecording() const { return _recording; }

    // Its steps, as the world counts them
    uint32_t FirstStep() const { return _recording.FirstStep(); }
    uint32_t LastStep() const { return _recording.LastStep(); }

    // Game time from the first step to the last, in 1/1024 s
    int32_t Duration() const;

    // The frame at a step, or between two, step counting steps as frames
    // do, within the recording. False, after logging why, when the file
    // can't be read there.
    bool FrameAt(double step, Frame &out);

    // Whether the last FrameAt had to start from a keyframe: what was drawn
    // before then isn't what comes next
    bool Jumped() const { return _jumped; }

protected:
    // The frames of the steps from one to another, read on from the frames
    // kept or from the keyframe before
    bool MakeUpTo(uint32_t from, uint32_t to);

    Recording _recording;
    std::unique_ptr<ViewReader> _reader; // Of _recording
    std::unique_ptr<FrameMaker> _maker;
    std::deque<Frame> _frames;           // One after the other, the last the reader's step
    int32_t _duration = 0;
    bool _jumped = false;
};

}

#endif
