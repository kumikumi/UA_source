#ifndef WORLD_RECORDER_H_INCLUDED
#define WORLD_RECORDER_H_INCLUDED

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "state.h"

class NC_STACK_ypaworld;
class IFFile;
struct TInfoMessage;

namespace World
{

// An object's bytes as its last change left them, and as they were before:
// what a change of a float is guessed from
struct ObjectVersions
{
    std::string Data, Before;
};

typedef std::map<uint64_t, ObjectVersions> ObjectsVersions; // By kind << 32 | id

// Where a recording stands after a step: its view, and its internal part if
// the level can go on from it; and what a recording that goes on from there
// needs: the steps up to it, each as written, and each object's versions
struct RecordedPoint
{
    uint32_t Step = 0;
    State::Image View, Internal;
    std::vector<std::string> Steps;
    ObjectsVersions ViewVersions, InternalVersions;
};

// Records a level as it is played (--record FILE): the state it started
// in, then what each simulation step changed of it, in the two parts of the
// state (see World::State). Every step has what changed of the view, as the
// game was drawn. Every BlockSteps steps, and the last step, also have what
// changed of the internal part since the last such step: the level can go
// on from them, exactly as it was. With --record-full, every step has it.
// tool/recording.py reads recordings.
//
// One IFF FORM RPLY. Chunk headers are big-endian as in any IFF,
// everything inside them little-endian:
//
//   INFO  u16 format version (2), u16 level, then zero-ended strings: how
//         the level started ("level N", the saved game's file, or the
//         recording and step it went on from), the platform and the
//         compiler, and when the program was built
//   SCHV  the view's fields of each kind of object, as
//         State::WriteSchema writes them
//   SCHI  the internal part's
//   STRV  the view before the first step, as State::Pack packs it: u32
//         size unpacked, then zlib: i32 level, u32 objects, each u8 kind,
//         u32 id, u32 size and its fields' bytes
//   STRI  the internal part before it
//   STEP  once for each block of up to BlockSteps steps: u32 size
//         unpacked, then zlib (level 6): the steps, each
//           u32  its size in bytes, after this
//           u8   1 when the level can go on from it, else 0
//           ...  what changed of the view since the step before
//           u16  messages shown, each i32 message id, i16 priority, u32
//                unit id (0 for none), u16 length and the text
//           ...  when the level can go on from it: what changed of the
//                internal part since the last step it could
//
// What changed: u32 objects, each u8 kind, u32 id and u8 what:
//   0  gone
//   1  new: u32 size, then its fields' bytes
//   2  changed: u16 fields, each its u16 index, then its new bytes: a list
//      as it is, any other field XORed with a guess of it. A float's guess
//      is its last value moved on by as much as it moved at the object's
//      change before (when both are finite), so that most of the XOR is
//      zeros; any other field's guess is its last value.
//
// A recording cut short, as by a crash, has every block before the one
// being recorded, and the FORM RPLY's size unwritten: it goes on from its
// last step the level can go on from.
class Recorder
{
public:
    enum { BlockSteps = 600 }; // 10 s of game time at 60 steps a second

    Recorder();
    ~Recorder();

    // Every step one the level can go on from (--record-full)
    void SetFull(bool full) { _full = full; }

    // Starts the recording, before the level's first step. False, after
    // logging why, when the file can't be written.
    bool Start(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom);

    // The same for a level that goes on from a recording: the new one starts
    // as that one did (start), with what it recorded up to where the level
    // goes on from (point)
    bool StartFrom(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom,
                   const RecordedPoint &start, const RecordedPoint &point);

    // After every simulation step
    void Step(NC_STACK_ypaworld *world);

    // A message the game shows
    void Message(const TInfoMessage &msg);

    // Makes the last step one the level can go on from, writes it and
    // closes the file
    void Stop(NC_STACK_ypaworld *world);

    bool IsRecording() const { return _file != nullptr; }

protected:
    // Each object as it was last written, to write only what changed
    struct LastObject
    {
        ObjectVersions Versions;
        uint32_t SeenStep;
    };

    typedef std::unordered_map<uint64_t, LastObject> LastObjects;

    bool Open(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom,
              const State::Image &startView, const State::Image &startInternal);
    std::string Changes(NC_STACK_ypaworld *world, State::Part part, const State::Schema &schema, LastObjects &last);
    void Remember(NC_STACK_ypaworld *world, State::Part part, LastObjects &last);
    void MakeResumable(NC_STACK_ypaworld *world);
    void FlushPending();
    void AddStep(const std::string &step);
    void WriteBlock();

    std::unique_ptr<IFFile> _file;
    bool _full = false;
    State::Schema _viewSchema, _internalSchema;
    LastObjects _lastView, _lastInternal;
    uint32_t _step = 0;

    std::string _steps; // The block's steps so far
    int _blockSteps = 0;

    std::string _messages; // Shown in the step under way
    uint16_t _messageCount = 0;

    // The last step, written when the next comes or the recording ends, so
    // that the end can make it one to go on from
    bool _hasPending = false;
    bool _pendingResumable = false;
    std::string _pendingView, _pendingMessages, _pendingInternal;
};

// A recording read back
class Recording
{
public:
    // Reads it. False, after logging why, when it isn't one.
    bool Open(const std::string &path);

    int Level() const { return _level; }
    const std::string &StartedFrom() const { return _startedFrom; }
    uint32_t Steps() const { return _resumable.size(); }

    // Whether the level can go on from after that step, 0 the start
    bool IsResumable(uint32_t step) const { return step == 0 || (step <= _resumable.size() && _resumable[step - 1]); }

    // The last step at or before that one the level can go on from
    uint32_t LastResumable(uint32_t step) const;

    // Reads it up to after that many steps, 0 for its start. With resume, the
    // internal part too, which only steps the level can go on from have.
    bool Read(uint32_t step, RecordedPoint &point, bool resume) const;

protected:
    int _level = 0;
    std::string _startedFrom;
    State::Schema _viewSchema, _internalSchema;
    std::string _startView, _startInternal; // Packed
    std::vector<std::string> _blocks;        // Packed
    std::vector<bool> _resumable;            // Of each step
};

}

#endif
