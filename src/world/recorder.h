#ifndef WORLD_RECORDER_H_INCLUDED
#define WORLD_RECORDER_H_INCLUDED

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class NC_STACK_ypaworld;
class NC_STACK_ypabact;
class IFFile;
struct TInfoMessage;

namespace World
{

// Records a level as it is played (--record FILE): every simulation step,
// and every BlockSteps steps the state a save game holds, to view, inspect
// or resume from. tool/recording.py reads them. One IFF FORM RPLY; chunk
// headers are big-endian as in any IFF, everything inside them
// little-endian:
//
//   INFO  u16 format version (1), u16 level, then zero-ended strings: how
//         the level started ("level N" or the saved game's file), the
//         platform and the compiler, and when the program was built.
//
//   FORM BLCK, once for each block of up to BlockSteps steps:
//     KEYF  u32 size unpacked, then zlib: the save game, as text, of the
//           state before the block's first step
//     STEP  u32 size unpacked, then zlib: the block's steps, each
//           u32  step, from 1 at the level's start
//           i32  game time, in timer ticks (1/1024 s)
//           u64  hash of every unit's exact state, u64 hash of every
//                sector's as written: where two recordings part
//           u32  the unit the player controls
//           u16  objects, each u32 id, u16 fields that follow (OBJ_*),
//                then those fields in OBJ_* order. Units and missiles;
//                the block's first step has every object with every field,
//                later steps only the objects and fields that changed.
//           u16  objects gone, each u32 id
//           u16  sectors, each u16 index (y * map width + x), u8 owner,
//                u8 sector type, u8 simple (1) or 3x3, u8 purpose, then
//                9 x u8 building health, rows from the top. Every sector in
//                the block's first step, later only the changed ones.
//           u16  messages shown, each i32 message id, i16 priority,
//                u32 unit id (0 for none), u16 length and the text
//
//   The last block has no steps: its KEYF is the state after the last
//   step. A recording cut short, as by a crash, has every block before the
//   one being recorded, and the FORM RPLY's size unwritten.
//
// Positions are floats and rotations 16-bit fractions: enough to show.
// The exact state is in the KEYF saves.
class Recorder
{
public:
    enum { BlockSteps = 600 }; // 10 s of game time at 60 steps a second

    // The fields of an object, in the order they are written
    enum
    {
        OBJ_KIND      = 1 << 0, // u8 unit type, u8 owner, u8 vehicle or weapon,
                                // u32 parent: commander, host station or, for
                                // a missile, the unit that fired it
        OBJ_STATUS    = 1 << 1, // u8 status, u32 status flags
        OBJ_ENERGY    = 1 << 2, // i32
        OBJ_POSITION  = 1 << 3, // 3 x f32
        OBJ_ROTATION  = 1 << 4, // 9 x i16, the rotation matrix's rows / 32767
        OBJ_LOOK      = 1 << 5, // u8 model: 0 other, 1 normal, 2 firing,
                                // 3 waiting, 4 wreck, 5 exploding, 6 beaming in
        OBJ_TARGETS   = 1 << 6, // u8 primary target type, u32 primary: unit id
                                // or sector index; the same for the secondary
        OBJ_AGGRESSION = 1 << 7, // u8
    };

    Recorder();
    ~Recorder();

    // Starts the recording, before the level's first step. False, after
    // logging why, when the file can't be written.
    bool Start(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom);

    // After every simulation step
    void Step(NC_STACK_ypaworld *world);

    // A message the game shows
    void Message(const TInfoMessage &msg);

    // Writes the last block and closes the file
    void Stop(NC_STACK_ypaworld *world);

    bool IsRecording() const { return _file != nullptr; }

protected:
    // An object's fields as written, little-endian as the platforms that
    // build this are
#pragma pack(push, 1)
    struct ObjectRecord
    {
        uint8_t type, owner, vehicle;  // OBJ_KIND
        uint32_t parent;
        uint8_t status;                // OBJ_STATUS
        uint32_t flags;
        int32_t energy;                // OBJ_ENERGY
        float position[3];             // OBJ_POSITION
        int16_t rotation[9];           // OBJ_ROTATION
        uint8_t look;                  // OBJ_LOOK
        uint8_t primaryType;           // OBJ_TARGETS
        uint32_t primary;
        uint8_t secondaryType;
        uint32_t secondary;
        uint8_t aggression;            // OBJ_AGGRESSION
    };

    // A sector as written, after its u16 index
    struct SectorRecord
    {
        uint8_t owner, type, simple, purpose;
        uint8_t health[9];
    };
#pragma pack(pop)

    void BeginBlock(NC_STACK_ypaworld *world);
    void WriteBlock();
    void WriteObject(NC_STACK_ypaworld *world, NC_STACK_ypabact *unit, uint64_t *hash);
    void WriteSectors(NC_STACK_ypaworld *world, uint64_t *hash);

    std::unique_ptr<IFFile> _file;
    uint32_t _step = 0;
    int _blockSteps = 0;

    std::string _keyframe; // The save at the block's start
    std::string _steps;    // The block's steps so far
    std::string _objects;  // The current step's objects
    uint16_t _objectCount = 0;
    std::string _messages;
    uint16_t _messageCount = 0;

    // What was written last of each object and sector, to write only the
    // changes. Emptied at each block's start, so it starts in full.
    struct LastObject
    {
        ObjectRecord rec;
        uint32_t seenStep = 0; // 0: not written yet
    };

    std::map<uint32_t, LastObject> _lastObjects;
    std::vector<SectorRecord> _lastSectors;
    std::vector<SectorRecord> _sectorsNow;
    std::string _sectorsOut; // The current step's sectors
};

}

#endif
