#ifndef WORLD_FRAME_H_INCLUDED
#define WORLD_FRAME_H_INCLUDED

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../vectors.h"
#include "../matrix.h"
#include "state.h"
#include "recorder.h"

class NC_STACK_ypaworld;

namespace World
{

// What a level looks like after a step: what of it is drawn, as the view of
// its state has it (World::State). The game is drawn from frames, never from
// the simulation's objects, so that it is drawn the same whether it goes on,
// goes back or comes from a recording. Between two steps, it's drawn as a
// frame between theirs (Between).
struct Frame
{
    struct Unit
    {
        uint32_t Id = 0;
        uint8_t Type = 0;        // BACT_TYPES_*
        uint8_t Vehicle = 0;     // Its prototype; a missile's is a weapon
        uint8_t Look = 0;        // The model it shows, as NC_STACK_ypabact::ModelByIndex
        int32_t StatusFlags = 0; // BACT_STFLAG_*
        vec3f Position;
        mat3x3f Rotation;
        vec3f Scale;
    };

    // A model a unit shows besides its own, as while it's being made: only
    // the active ones, in the order of units and slots
    struct Extra
    {
        uint32_t Unit = 0;
        uint8_t Slot = 0;
        uint8_t Model = 0;       // As a unit's look
        int32_t Flags = 0;       // EVPROTO_FLAG_*
        float Scale = 0.0;
        vec3f Position;
        mat3x3f Rotation;
    };

    struct Sector
    {
        int32_t Type = 0;
        int16_t Health[9] = {};  // Each building's, x + 3 * y
        uint8_t Owner = 0;
        uint8_t Purpose = 0;     // cellArea::PT_*
        int32_t Power = 0;       // What host stations on it take in, 0-255
        int32_t StationPower = -1; // Of a power station on it; -1 for none

        bool operator==(const Sector &b) const;
        bool operator!=(const Sector &b) const { return !(*this == b); }
    };

    // A building being built, drawn growing: apart from the sectors, which
    // then don't change every step
    struct Construction
    {
        uint32_t Sector = 0;     // Its index
        uint8_t Building = 0;
        int32_t Time = 0;        // Of End, in 1/1024 s
        int32_t End = 0;
    };

    // A bomb's wave
    struct SuperItem
    {
        int32_t State = 0;       // TMapSuperItem::STATE_*
        int32_t Radius = 0;
    };

    // What else there is to show of a unit than how it's drawn, as the
    // interface shows it: only in a frame with details (FrameMaker)
    struct UnitInfo
    {
        uint8_t Owner = 0;
        uint8_t Status = 0;      // BACT_STATUS_*
        uint8_t Aggression = 0;
        int16_t Shield = -1;     // Armour, in percent; -1 when not recorded
        int32_t Energy = 0;
        int32_t EnergyMax = 0;
        uint32_t Parent = 0;     // Its commander, or a commander's host station
        uint8_t PrimaryType = 0; // BACT_TGT_TYPE_*, of its target
        uint32_t Primary = 0;    // The unit's id, or the sector's index
        vec3f PrimaryPos;        // Where it heads for
        uint8_t SecondaryType = 0; // Of what it fights on the way
        uint32_t Secondary = 0;
        int16_t GunFlags = -1;   // A gun's NC_STACK_ypagun::GUN_FLAGS_*; -1 when not recorded
        int32_t ClockStart = 0;  // The game time its clock started at
        int32_t WeaponTime = 0;  // Its clock at its last shot
        int32_t SalveCounter = 0;
    };

    // A host station's batteries and what it takes in, as the energy panel
    // shows them: only with details
    struct HostStation
    {
        uint32_t Unit = 0;
        int32_t Create = 0;      // The battery new units take from
        int32_t Beam = 0;        // The battery beaming takes from
        int32_t BuildSpare = 0;  // What another faction's builds with
        int32_t VehicleSpare = 0;
        int32_t ReloadPS = 0;    // What it would take in a second, at full power
        uint8_t FillMode = 0;    // Which of its batteries it fills (1, 4, 8)
        uint8_t LoadFlags = 0;   // Which of them filled or drained last
        uint8_t LossFlags = 0;
    };

    uint32_t Step = 0;           // Steps since the level started
    int32_t Time = 0;            // Game time, in 1/1024 s
    uint32_t Viewer = 0;         // The unit the player sees from
    uint32_t UserRobo = 0;       // The player's host station
    uint32_t UserUnit = 0;       // The unit the player steers
    vec3f CameraPosition;
    mat3x3f CameraRotation;

    std::vector<Unit> Units;     // In the order of their ids
    std::vector<Extra> Extras;

    // With details: one for each unit, in the same order, and the host
    // stations in the order of their ids
    std::vector<UnitInfo> Infos;
    std::vector<HostStation> HostStations;

    // By index: shared with the frames before while no sector changes
    std::shared_ptr<const std::vector<Sector>> Sectors;

    std::vector<Construction> Constructions; // In the order of their sectors
    std::vector<SuperItem> SuperItems;

    const Unit *FindUnit(uint32_t id) const;
    const Construction *FindConstruction(uint32_t sector) const;

    // A sector by its index, if the frame has it
    const Sector *SectorAt(size_t index) const { return Sectors && index < Sectors->size() ? &(*Sectors)[index] : NULL; }

    // With details: what there is to show of a unit, by its id or its
    // index in Units, and of a host station
    const UnitInfo *FindInfo(uint32_t id) const;
    const UnitInfo *InfoAt(size_t index) const { return index < Infos.size() ? &Infos[index] : NULL; }
    const HostStation *FindHostStation(uint32_t id) const;

    // The frame t of the way from a to b, b following a or the other way
    // around: units and the camera moved and turned in between, as far as
    // they didn't jump; time in between; and the rest as in the nearer one.
    static void Between(const Frame &a, const Frame &b, float t, Frame &out);
};

// Makes frames from the view of a state: knows where each kind of object
// has what is drawn
class FrameMaker
{
public:
    // With details, the frames have what the interface shows too
    explicit FrameMaker(const State::Schema &viewSchema, bool details = false);

    // A frame of the level as it is now. The frame before, when given, keeps
    // its sectors if they're the same.
    void Make(NC_STACK_ypaworld *world, uint32_t step, const Frame *before, Frame &out) const;

    // Or one object of the view at a time, as recordings have them; then
    // End
    void Add(Frame &frame, std::vector<Frame::Sector> &sectors, uint8_t kind, uint32_t id, const std::string &data) const;
    void End(Frame &frame, std::vector<Frame::Sector> &sectors, const Frame *before) const;

    // A frame of a recording's view after a step, as a ViewReader has it
    void FromView(const ObjectsVersions &view, uint32_t step, const Frame *before, Frame &out) const;

protected:
    // Where a field of a kind's view is, how big it is and of what type
    struct Field
    {
        size_t Offset = 0;
        size_t Size = 0;
        State::Type Type = State::T_U8;
        bool Found = false;
    };

    struct UnitFields
    {
        Field Vehicle, StatusFlags, Position, Rotation, Scale, Look;
        Field ExtraScale[3], ExtraPos[3], ExtraRotation[3], ExtraFlags[3], ExtraModel[3];

        // Details
        Field Owner, Status, Aggression, Shield, Energy, EnergyMax, Parent;
        Field PrimaryType, Primary, PrimaryPos, SecondaryType, Secondary, GunFlags, ClockStart, WeaponTime, SalveCounter;
        Field Create, Beam, BuildSpare, VehicleSpare, ReloadPS, FillMode, LoadFlags, LossFlags;
    };

    static Field Find(const State::Schema &schema, uint8_t kind, const char *name);

    State::Schema _schema;
    bool _details;
    Field _time, _viewer, _userRobo, _userUnit, _cameraPosition, _cameraRotation;
    Field _sectorType, _sectorHealth, _sectorOwner, _sectorPurpose, _sectorPower, _powerStation, _stationPower;
    Field _constructing, _constructBuilding, _constructTime, _constructEnd;
    Field _itemState, _itemRadius;
    std::unordered_map<uint8_t, UnitFields> _units; // By kind
};

// The frames of the last steps, one after the other: a cache of the
// recording as it was drawn, to draw going back
class Frames
{
public:
    void Clear() { _frames.clear(); }

    // The frame after the last; one that doesn't follow it starts the
    // frames over
    void Add(Frame &&frame);

    // The frames of the steps before the first, up to it. False, keeping
    // none of them, when they don't end there.
    bool Prepend(std::vector<Frame> &&frames);

    // Drops those after a step, or before one, but the last
    void DropAfter(uint32_t step);
    void DropBefore(uint32_t step);

    bool Empty() const { return _frames.empty(); }
    const Frame *At(uint32_t step) const;
    const Frame &First() const { return _frames.front(); }
    const Frame &Last() const { return _frames.back(); }

protected:
    std::deque<Frame> _frames;
};

// The frames of a recording's steps from a keyframe up to a step, made a
// little at a time: to go back through steps whose frames weren't kept
class FramesFromRecording
{
public:
    // From the keyframe at from up to to, not to's
    FramesFromRecording(const std::string &path, uint32_t from, uint32_t to) : _path(path), _from(from), _to(to) {}

    // Goes on for about that many milliseconds. True once done, then failed
    // or not.
    bool Work(const FrameMaker &maker, double ms);
    bool Failed() const { return _failed; }

    uint32_t From() const { return _from; }
    uint32_t To() const { return _to; }
    std::vector<Frame> &Made() { return _made; }

protected:
    std::string _path;
    uint32_t _from, _to;
    std::unique_ptr<Recording> _recording;
    std::unique_ptr<ViewReader> _reader; // Of _recording
    std::vector<Frame> _made;
    bool _done = false, _failed = false;
};

}

#endif
