#ifndef WORLD_RECORDER_H_INCLUDED
#define WORLD_RECORDER_H_INCLUDED

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "state.h"

struct z_stream_s;

class NC_STACK_ypaworld;
namespace FSMgr { class FileHandle; }
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

// The objects a step changed or added, with their versions, or took away
// (NULL)
typedef std::vector<std::pair<uint64_t, const ObjectVersions *>> ChangedObjects;

// A kind's fields, and where they start when they're all of a fixed size,
// as all of the view's are
struct KindLayout
{
    const std::vector<State::FieldInfo> *Fields = NULL;
    bool Fixed = false;
    std::vector<size_t> Offsets;
};

typedef std::unordered_map<uint8_t, KindLayout> KindLayouts; // Point into the schema
KindLayouts MakeLayouts(const State::Schema &schema);

// A keyframe in a recording's file: its step, and where its chunk starts
// and ends
struct KeyframeAt
{
    uint32_t Step = 0;
    uint64_t Start = 0, End = 0;
};

class Recording;

// How a level was left, as the end of its recording says
enum RecordingEnd : uint8_t
{
    END_UNFINISHED = 0, // Gone back to the level map, or the game quit: it
                        // can go on (Continue)
    END_WON = 1,
    END_LOST = 2,       // The player's host station was destroyed
};

// Records a level as it is played (--record FILE): keyframes, each the whole
// state of the level after a step (World::State: the view and the internal
// part), and between them what each step changed of the view, as the game
// was drawn. The level can go on from any keyframe, exactly as it was, and
// the recording can be read from any keyframe on without what came before
// it. Keyframes come where the recording starts, after every
// KeyframeSteps-th step, where a game is saved and at the end; with
// --record-full after every step. tool/recording.py reads recordings.
//
// The file: "UAREC", a zero byte and the u16 format version (3), then
// chunks, each its 4-letter tag, u64 size and that many bytes. All of it is
// little-endian.
//
//   INFO  u16 level, then zero-ended strings: how the level started
//         ("level N", or the saved game's or recording's file), the
//         platform and the compiler, and when the program was built
//   SCHV  the view's fields of each kind of object, as State::WriteSchema
//         writes them
//   SCHI  the internal part's
//   KEYF  a keyframe: u32 step, u32 size unpacked, then zlib (level 6): the
//         view and then the internal part, each as State::Serialize writes
//         them: i32 level, u32 objects, each u8 kind, u32 id, u32 size and
//         its fields' bytes
//   STEP  the steps after a keyframe, up to the next one: u32 the first
//         step, u32 steps, u32 size unpacked, then zlib (level 6): each step
//           u32  its size in bytes, after this
//           ...  what changed of the view since the step before
//           u16  messages shown, each i32 message id, i16 priority, u32
//                unit id (0 for none), u16 length and the text
//   END   (with a space) u8 how the level was left (RecordingEnd)
//
// INFO, SCHV and SCHI come first, then keyframes with the steps between
// them: KEYF, STEP, KEYF, STEP ... KEYF, and END when the level was left.
// A saved game, a recording still being made and one cut short have no
// END: they're unfinished.
//
// What changed: u32 objects, each u8 kind, u32 id and u8 what:
//   0  gone
//   1  new: u32 size, then its fields' bytes
//   2  changed: u16 fields, each its u16 index, then its new bytes: a list
//      or a field of over 256 bytes as it is, any other field against a
//      guess of it. The guess of a float, a fixed-point position or a
//      quaternion (State::T_FIXED, T_QUAT) is its last value moved on by as
//      much as it moved at the object's change before (floats when both are
//      finite); any other field's is its last value. Positions and
//      quaternions are written as their difference from the guess, wrapping
//      around, everything else XORed with it: either is mostly zeros when
//      the guess is close. The guesses start over at each keyframe: an
//      object's bytes before its first change after a keyframe are taken to
//      be the same as at the keyframe.
//
// A recording cut short, as by a crash, ends with its last whole chunk: it
// goes on from its last keyframe.
//
// A saved game is a recording up to the moment it was saved, and a won
// level's end a recording of one keyframe. In a campaign, every level is
// recorded (NC_STACK_ypaworld::SessionPath); a saved game's level goes on
// recording from where the saved game ends.
//
// A recording can go back to any of its keyframes (Rewind): the file is cut
// after it, and recording goes on from it.
class Recorder
{
public:
    enum { KeyframeSteps = 600 }; // 10 s of game time at 60 steps a second

    Recorder();
    ~Recorder();

    // A keyframe after every step (--record-full)
    void SetFull(bool full) { _full = full; }

    // Starts recording the level as it is now, which is the first keyframe.
    // False, after logging why, when the file can't be written.
    bool Start(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom);

    // Goes on recording a level that was set up from a keyframe of a
    // recording (source): the recording at path is that one up to the
    // keyframe, then what comes. path may be the source's own file, which is
    // then cut after the keyframe. A recording made by a build whose state
    // has other fields can't be gone on with: a new one starts at the
    // keyframe.
    bool ContinueFrom(NC_STACK_ypaworld *world, const std::string &path, const Recording &source, uint32_t step);

    // After every simulation step
    void Step(NC_STACK_ypaworld *world);

    // A message the game shows
    void Message(const TInfoMessage &msg);

    // Ends the recording with a keyframe of the level as it is now and how
    // it was left, and closes the file. Its path, or empty when nothing was
    // recorded.
    std::string Stop(NC_STACK_ypaworld *world, RecordingEnd end);

    // A saved game: a keyframe of the level as it is now, and the recording
    // so far copied to path. Recording goes on.
    bool SaveTo(NC_STACK_ypaworld *world, const std::string &path);

    // A recording of the level as it is now, one keyframe, as of a won
    // level's end
    static bool WriteState(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom);

    // Goes back to the latest keyframe at least minSteps before the last
    // step: the recording ends there and goes on from it, and view and
    // internal are the state then. False when there's none. The file is cut
    // there as recording goes on, so that until then it can still be read
    // as it was (Recording).
    bool Rewind(uint32_t minSteps, State::Image &view, State::Image &internal);

    // The step Rewind would go back to. False when there's none.
    bool RewindTarget(uint32_t minSteps, uint32_t &step) const;

    // The latest keyframe in the file before a step. False when there's none.
    bool KeyframeBefore(uint32_t step, uint32_t &keyframe) const;

    // Puts what was written so far in the file, for a Recording to read
    void Flush();

    bool IsRecording() const { return _file != nullptr; }
    uint32_t Step() const { return _step; } // The last step recorded, as the world counts them
    const std::string &Path() const { return _path; }

protected:
    // Each object of the view as it was last written, to write only what
    // changed
    struct LastObject
    {
        ObjectVersions Versions;
        uint32_t SeenStep;
    };

    typedef std::unordered_map<uint64_t, LastObject> LastObjects;

    // Packs what comes into one zlib stream, a piece at a time
    struct Packer
    {
        Packer();
        ~Packer();

        void Add(const void *data, size_t size);
        std::string Finish(); // What was packed, and starts over
        void Drop();          // Starts over

        bool Started() const { return _stream != nullptr; }
        uint64_t Unpacked() const { return _unpacked; }

    protected:
        void Deflate(const void *data, size_t size, int flush);

        std::unique_ptr<z_stream_s> _stream;
        std::string _packed;
        uint64_t _unpacked = 0;
    };

    bool Open(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom);
    void Reset();
    bool WriteChunk(const char *tag, const std::string &head, const std::string &data);
    std::string Changes(NC_STACK_ypaworld *world, State::Part part, const State::Schema &schema, const KindLayouts &layouts,
                        LastObjects &last);
    void Remember(NC_STACK_ypaworld *world);
    void RememberImage(const State::Image &view);
    void EndBlock();
    void MakeKeyframe(NC_STACK_ypaworld *world);
    void PackKeyframe(size_t bytes);
    void Cut();

    std::unique_ptr<FSMgr::FileHandle> _file;
    std::string _path;
    int _level = 0;
    bool _full = false;
    State::Schema _viewSchema, _internalSchema;
    KindLayouts _viewLayouts;
    LastObjects _lastView;
    uint32_t _step = 0;

    std::vector<KeyframeAt> _keyframes; // In the file, in order

    // Where Rewind went back to: the file is cut there before it's written
    // again
    bool _cutPending = false;
    uint64_t _cutAt = 0;

    // The steps since the last keyframe, packed as they come
    Packer _steps;
    uint32_t _blockStart = 0; // The first of them
    uint32_t _blockSteps = 0;

    // The last keyframe, packed a piece every step until it's written
    Packer _keyPacker;
    std::string _keyRaw;
    size_t _keyAt = 0;
    uint32_t _keyStep = 0;
    bool _keyPending = false;

    std::string _messages; // Shown in the step under way
    uint16_t _messageCount = 0;
};

// A recording read back. Only where its chunks are is read at first; the
// rest when it's asked for.
class Recording
{
public:
    // Finds its chunks. False, after logging why, when it isn't one.
    bool Open(const std::string &path);

    const std::string &Path() const { return _path; }
    int Level() const { return _level; }
    const std::string &StartedFrom() const { return _startedFrom; }
    const State::Schema &ViewSchema() const { return _viewSchema; }

    // The steps it has, as the world counts them: its first keyframe's, and
    // the last
    uint32_t FirstStep() const { return _keyframes.empty() ? 0 : _keyframes.front().Step; }
    uint32_t LastStep() const { return _lastStep; }

    const std::vector<KeyframeAt> &Keyframes() const { return _keyframes; }

    // How the level was left: unfinished when the recording doesn't say
    RecordingEnd End() const { return _end; }

    // The keyframe at that step, or the latest before it; NULL when there's
    // none
    const KeyframeAt *KeyframeAtOrBefore(uint32_t step) const;

    // The state at a keyframe's step. False, after logging why, when it
    // can't be read.
    bool ReadKeyframe(uint32_t step, State::Image &view, State::Image &internal) const;

    // Whether a build with these schemas can go on recording it
    bool SameSchemas(const State::Schema &view, const State::Schema &internal) const;

protected:
    friend class ViewReader;

    // The steps after a keyframe, up to the next one
    struct Block
    {
        uint32_t FirstStep = 0, Steps = 0;
        uint64_t Start = 0, End = 0; // Of its chunk
    };

    // A block's steps, unpacked. False when they can't be read.
    bool ReadBlock(size_t index, std::string &steps) const;

    std::string _path;
    int _level = 0;
    std::string _startedFrom;
    std::string _viewSchemaBytes, _internalSchemaBytes;
    State::Schema _viewSchema, _internalSchema;
    KindLayouts _viewLayouts;
    std::vector<KeyframeAt> _keyframes;
    std::vector<Block> _blocks;
    uint32_t _lastStep = 0;
    RecordingEnd _end = END_UNFINISHED;
};

// Goes through a recording's views a step at a time, from a keyframe on
class ViewReader
{
public:
    // The recording stays as long as this does
    explicit ViewReader(const Recording &recording) : _recording(recording) {}

    // To the first keyframe. False when the recording can't be read.
    bool Rewind();

    // To a keyframe. False when there's none at that step, or it can't be
    // read.
    bool Seek(uint32_t step);

    // On to the next step. False at the end, or where the recording can't
    // be read on (and then not AtEnd).
    bool Next();
    bool AtEnd() const;

    uint32_t Step() const { return _step; }
    const ObjectsVersions &View() const { return _view; }

    // What the step changed; at a keyframe sought, every object
    const ChangedObjects &Changed() const { return _changed; }

protected:
    const Recording &_recording;
    ObjectsVersions _view;
    ChangedObjects _changed;
    uint32_t _step = 0;
    size_t _nextBlock = 0;
    std::string _block; // Unpacked
    size_t _at = 0;     // Where its next step starts
};

}

#endif
