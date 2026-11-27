#ifndef WORLD_VIEWSTREAM_H_INCLUDED
#define WORLD_VIEWSTREAM_H_INCLUDED

#include <cstdint>
#include <string>

#include "recorder.h"

namespace World
{

// A game's view for those watching it as it's played: the host's recorder's
// steps, the same bytes as in its recording, after an image of the view to
// read them from. Its messages, each beginning with its type:
//
//   'H'  hello, first: u16 the stream's format, i32 the level, then the
//        level's description and the view's schema (State::WriteSchema),
//        each u32 its size and its bytes
//   'I'  the view after a step, which the steps after it are read from: u32
//        the step, u32 objects, each u8 kind, u32 id, its last bytes and
//        those before (both of its kind's size, in the order of the
//        objects' keys); sent to a watcher who joins, and to all again when
//        the host's view is taken anew
//   'S'  a step: u32 the step, u8 1 when the guesses start over before it
//        (at a keyframe of the recording), then the recorder's step: what
//        changed of the view, then the messages shown (Recorder)
//   'E'  the end: the level was left
//
// The steps come one after the other from the image on.
namespace ViewStream
{
    enum Type : uint8_t
    {
        HELLO = 'H',
        IMAGE = 'I',
        STEP = 'S',
        END = 'E',
    };

    constexpr uint16_t Format = 1;

    std::string Hello(int level, const std::string &description, const State::Schema &schema);
    std::string Image(const Recorder &recorder);
    std::string Step(const Recorder &recorder);
    std::string End();
}

// Reads a view stream into the view after each step, as a ViewReader reads
// a recording's
class ViewStreamReader
{
public:
    // A message. Returns its type, or 0 after logging why when it doesn't
    // read; nothing is read after that.
    uint8_t Read(const uint8_t *data, size_t size);

    bool HasLevel() const { return _hasLevel; }
    int Level() const { return _level; }
    const std::string &LevelDescription() const { return _description; }
    const State::Schema &ViewSchema() const { return _schema; }

    // The view after Step, once an image came
    bool HasView() const { return _hasView; }
    uint32_t Step() const { return _step; }
    const ObjectsVersions &View() const { return _view; }

    // What the last step changed; after an image, every object
    const ChangedObjects &Changed() const { return _changed; }

    bool Ended() const { return _ended; }
    bool Failed() const { return _failed; }

protected:
    uint8_t Fail(const char *why);

    bool _hasLevel = false;
    int _level = 0;
    std::string _description;
    State::Schema _schema;
    KindLayouts _layouts;

    bool _hasView = false;
    uint32_t _step = 0;
    ObjectsVersions _view;
    ChangedObjects _changed;

    bool _ended = false;
    bool _failed = false;
};

}

#endif
