#ifndef WORLD_PLAYBACK_H_INCLUDED
#define WORLD_PLAYBACK_H_INCLUDED

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "frame.h"
#include "playerorder.h"
#include "playercontrols.h"
#include "recorder.h"

namespace World
{

// What the replay viewer shows: a recording (Playback), or a game as it's
// played elsewhere (Net::ViewClient)
class FrameSource
{
public:
    virtual ~FrameSource() {}

    // The steps it has, as the world counts them
    virtual uint32_t FirstStep() const = 0;
    virtual uint32_t LastStep() const = 0;

    // Game time from the first step to the last, in 1/1024 s
    virtual int32_t Duration() const = 0;

    // The frame at a step, or between two, step counting steps as frames
    // do, within FirstStep and LastStep. False, after logging why, when
    // there's none.
    virtual bool FrameAt(double step, Frame &out) = 0;

    // Whether the last FrameAt didn't follow on from the one before: what
    // was drawn before then isn't what comes next
    virtual bool Jumped() const = 0;

    // A game as it's played, whose steps come as it goes on (Update) and
    // which keeps only the latest
    virtual bool Live() const { return false; }

    // Takes in what came. False when nothing more will.
    virtual bool Update() { return true; }

    // A game played elsewhere that this machine plays too: the player it
    // plays as (0 for none, only watching), their orders to it, and their
    // controls every step
    virtual uint32_t Player() const { return 0; }
    virtual void SendOrder(const PlayerOrder &order) {}
    virtual void SendControls(const PlayerControls &controls) {}
};

// A recording played back as it was drawn, from any step: the frames of the
// steps around where it's at are made from its file as they're needed, so
// that however long it is, it takes as little memory as a short one. Going
// on reads the file on; going back or far ahead starts from the keyframe
// before the step.
class Playback : public FrameSource
{
public:
    // False, after logging why, when it isn't a recording or its view
    // can't be drawn
    bool Open(const std::string &path);

    const Recording &GetRecording() const { return _recording; }

    uint32_t FirstStep() const override { return _recording.FirstStep(); }
    uint32_t LastStep() const override { return _recording.LastStep(); }
    int32_t Duration() const override;

    // Within the recording; going back or far ahead reads it from a keyframe
    bool FrameAt(double step, Frame &out) override;

    // Started from a keyframe
    bool Jumped() const override { return _jumped; }

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
