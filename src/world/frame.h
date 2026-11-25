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
        bool Constructing = false;
        uint8_t ConstructBuilding = 0;
        int32_t ConstructTime = 0;
        int32_t ConstructEnd = 0;

        bool operator==(const Sector &b) const;
        bool operator!=(const Sector &b) const { return !(*this == b); }
    };

    // A bomb's wave
    struct SuperItem
    {
        int32_t State = 0;       // TMapSuperItem::STATE_*
        int32_t Radius = 0;
    };

    uint32_t Step = 0;           // Steps since the level started
    int32_t Time = 0;            // Game time, in 1/1024 s
    uint32_t Viewer = 0;         // The unit the player sees from
    vec3f CameraPosition;
    mat3x3f CameraRotation;

    std::vector<Unit> Units;     // In the order of their ids
    std::vector<Extra> Extras;

    // By index: shared with the frames before while no sector changes
    std::shared_ptr<const std::vector<Sector>> Sectors;

    std::vector<SuperItem> SuperItems;

    const Unit *FindUnit(uint32_t id) const;

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
    explicit FrameMaker(const State::Schema &viewSchema);

    // A frame of the level as it is now. The frame before, when given, keeps
    // its sectors if they're the same.
    void Make(NC_STACK_ypaworld *world, uint32_t step, const Frame *before, Frame &out) const;

    // Or one object of the view at a time, as recordings have them; then
    // End
    void Add(Frame &frame, std::vector<Frame::Sector> &sectors, uint8_t kind, uint32_t id, const std::string &data) const;
    void End(Frame &frame, std::vector<Frame::Sector> &sectors, const Frame *before) const;

protected:
    // Where a field of a kind's view is, and how big it is
    struct Field
    {
        size_t Offset = 0;
        size_t Size = 0;
        bool Found = false;
    };

    struct UnitFields
    {
        Field Vehicle, StatusFlags, Position, Rotation, Scale, Look;
        Field ExtraScale[3], ExtraPos[3], ExtraRotation[3], ExtraFlags[3], ExtraModel[3];
    };

    static Field Find(const State::Schema &schema, uint8_t kind, const char *name);

    State::Schema _schema;
    Field _time, _viewer, _cameraPosition, _cameraRotation;
    Field _sectorType, _sectorHealth, _constructing, _constructBuilding, _constructTime, _constructEnd;
    Field _itemState, _itemRadius;
    std::unordered_map<uint8_t, UnitFields> _units; // By kind
};

// The frames of the last steps, one after the other: a cache of the
// recording as it was drawn, to draw going back
class Frames
{
public:
    // How many frames are kept, the last ones: at least the two drawn
    // between
    explicit Frames(size_t keep = 2) : _keep(std::max(keep, (size_t)2)) {}
    void Keep(size_t keep);

    void Clear() { _frames.clear(); }

    // The frame after the last; one that doesn't follow it starts the
    // frames over
    void Add(Frame &&frame);

    // Drops those after a step
    void DropAfter(uint32_t step);

    bool Empty() const { return _frames.empty(); }
    const Frame *At(uint32_t step) const;
    const Frame &First() const { return _frames.front(); }
    const Frame &Last() const { return _frames.back(); }

protected:
    std::deque<Frame> _frames;
    size_t _keep;
};

}

#endif
