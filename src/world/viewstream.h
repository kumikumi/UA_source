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
//        level's description, the view's schema and the schema of units'
//        whole state (State::WriteSchema), each u32 its size and its bytes
//   'I'  the view after a step, which the steps after it are read from: u32
//        the step, u32 objects, each u8 kind, u32 id, its last bytes and
//        those before (both of its kind's size, in the order of the
//        objects' keys); sent to a watcher who joins, and to all again when
//        the host's view is taken anew
//   'S'  a step: u32 the step, u8 1 when the guesses start over before it
//        (at a keyframe of the recording), then the recorder's step: what
//        changed of the view, then the messages shown (Recorder)
//   'Y'  you: u32 the player the watcher plays as from the next step on, 0
//        when they only watch (Net::ViewServer)
//   'U'  your unit, after a step, to a watcher who plays and controls a
//        unit: the unit as ControlledUnit has it, u32 the step, i32 the
//        time, u32 the controls, u8 the kind, u32 the id, then its fields,
//        u32 their size and their bytes
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
        YOU = 'Y',
        UNIT = 'U',
        END = 'E',
    };

    constexpr uint16_t Format = 3;
}

// The unit a watcher controls as the game had it after a step, for the
// watcher's machine to drive on from as they steer it (World::Prediction)
struct ControlledUnit
{
    uint32_t Step = 0;
    int32_t Time = 0;      // The game's time after the step
    uint32_t Controls = 0; // The number of the controls it went by in the
                           // step (World::PlayerControls), 0 for none
    uint8_t Kind = 0;      // State::KIND_UNIT and its type
    uint32_t Id = 0;
    std::string Fields;    // All of them, as State::Visitor writes them
};

namespace ViewStream
{
    std::string Hello(int level, const std::string &description, const State::Schema &schema,
                      const State::Schema &unitSchema);
    std::string Image(const Recorder &recorder);
    std::string Step(const Recorder &recorder);
    std::string You(uint32_t player);
    std::string Unit(const ControlledUnit &unit);
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

    // The fields of units' whole state, in the game's build
    const State::Schema &UnitSchema() const { return _unitSchema; }

    // The view after Step, once an image came
    bool HasView() const { return _hasView; }
    uint32_t Step() const { return _step; }
    const ObjectsVersions &View() const { return _view; }

    // What the last step changed; after an image, every object
    const ChangedObjects &Changed() const { return _changed; }

    // The player the watcher plays as, 0 for none
    uint32_t Player() const { return _player; }

    // The unit they control, as the last 'U' had it
    const ControlledUnit &Unit() const { return _unit; }

    bool Ended() const { return _ended; }
    bool Failed() const { return _failed; }

protected:
    uint8_t Fail(const char *why);

    bool _hasLevel = false;
    int _level = 0;
    std::string _description;
    State::Schema _schema;
    State::Schema _unitSchema;
    KindLayouts _layouts;

    bool _hasView = false;
    uint32_t _step = 0;
    ObjectsVersions _view;
    ChangedObjects _changed;

    uint32_t _player = 0;
    ControlledUnit _unit;
    bool _ended = false;
    bool _failed = false;
};

}

#endif
