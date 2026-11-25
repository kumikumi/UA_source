#include <algorithm>
#include <cstring>

#include "../yw.h"
#include "frame.h"

namespace World
{

static_assert(sizeof(vec3f) == 12, "three floats");
static_assert(sizeof(mat3x3f) == 36, "nine floats");

// Units that move further than this in one step, as when beamed, are drawn
// where they ended up instead of in between
static const float MaxInterpolatedMove = 300.0;

// Whether a unit or the camera changed too much in one step to draw it in
// between: it moved too far, or turned more than about 30 degrees, as the
// camera does when the player switches to another unit
static bool IsStepJump(const vec3f &pos0, const mat3x3f &rot0, const vec3f &pos1, const mat3x3f &rot1)
{
    if ( (pos1 - pos0).length() > MaxInterpolatedMove )
        return true;

    return (rot1.AxisY() - rot0.AxisY()).length() > 0.5 ||
           (rot1.AxisZ() - rot0.AxisZ()).length() > 0.5;
}

// Turns within one step are small enough that blending the matrices keeps
// them turns
static mat3x3f Blend(const mat3x3f &a, const mat3x3f &b, float t)
{
    return a * (1.0f - t) + b * t;
}

bool Frame::Sector::operator==(const Sector &b) const
{
    return Type == b.Type && !memcmp(Health, b.Health, sizeof(Health)) && Constructing == b.Constructing &&
           ConstructBuilding == b.ConstructBuilding && ConstructTime == b.ConstructTime &&
           ConstructEnd == b.ConstructEnd;
}

const Frame::Unit *Frame::FindUnit(uint32_t id) const
{
    auto it = std::lower_bound(Units.begin(), Units.end(), id,
                               [](const Unit &u, uint32_t id) { return u.Id < id; });

    if ( it == Units.end() || it->Id != id )
        return NULL;

    return &*it;
}

static const Frame::Extra *FindExtra(const std::vector<Frame::Extra> &extras, uint32_t unit, uint8_t slot)
{
    for (const Frame::Extra &e : extras)
    {
        if ( e.Unit == unit && e.Slot == slot )
            return &e;
    }

    return NULL;
}

void Frame::Between(const Frame &a, const Frame &b, float t, Frame &out)
{
    const Frame &nearer = t < 0.5 ? a : b;
    const Frame &other = t < 0.5 ? b : a;

    out.Step = nearer.Step;
    out.Time = a.Time + (int32_t)((b.Time - a.Time) * t);
    out.Viewer = nearer.Viewer;
    out.Sectors = nearer.Sectors;
    out.SuperItems = nearer.SuperItems;

    out.CameraPosition = nearer.CameraPosition;
    out.CameraRotation = nearer.CameraRotation;

    if ( !IsStepJump(a.CameraPosition, a.CameraRotation, b.CameraPosition, b.CameraRotation) )
    {
        out.CameraPosition = a.CameraPosition + (b.CameraPosition - a.CameraPosition) * t;
        out.CameraRotation = Blend(a.CameraRotation, b.CameraRotation, t);
    }

    // A wave of the same bomb grows in between
    for (size_t i = 0; i < out.SuperItems.size() && i < other.SuperItems.size(); i++)
    {
        if ( a.SuperItems[i].State == b.SuperItems[i].State )
            out.SuperItems[i].Radius = a.SuperItems[i].Radius + (b.SuperItems[i].Radius - a.SuperItems[i].Radius) * t;
    }

    out.Units = nearer.Units;

    for (Unit &u : out.Units)
    {
        const Unit *o = other.FindUnit(u.Id);
        if ( !o )
            continue;

        const Unit &ua = t < 0.5 ? u : *o;
        const Unit &ub = t < 0.5 ? *o : u;

        if ( IsStepJump(ua.Position, ua.Rotation, ub.Position, ub.Rotation) )
            continue;

        vec3f pos = ua.Position + (ub.Position - ua.Position) * t;
        mat3x3f rot = Blend(ua.Rotation, ub.Rotation, t);
        vec3f scale = ua.Scale + (ub.Scale - ua.Scale) * t;

        u.Position = pos;
        u.Rotation = rot;
        u.Scale = scale;
    }

    out.Extras = nearer.Extras;

    for (Extra &e : out.Extras)
    {
        const Extra *o = FindExtra(other.Extras, e.Unit, e.Slot);
        if ( !o )
            continue;

        const Extra &ea = t < 0.5 ? e : *o;
        const Extra &eb = t < 0.5 ? *o : e;

        if ( IsStepJump(ea.Position, ea.Rotation, eb.Position, eb.Rotation) )
            continue;

        vec3f pos = ea.Position + (eb.Position - ea.Position) * t;
        mat3x3f rot = Blend(ea.Rotation, eb.Rotation, t);
        float scale = ea.Scale + (eb.Scale - ea.Scale) * t;

        e.Position = pos;
        e.Rotation = rot;
        e.Scale = scale;
    }
}


// Making frames

FrameMaker::Field FrameMaker::Find(const State::Schema &schema, uint8_t kind, const char *name)
{
    Field out;
    auto it = schema.find(kind);
    if ( it == schema.end() )
        return out;

    size_t offset = 0;
    for ( const State::FieldInfo &f : it->second )
    {
        // The view's fields are all of a fixed size
        if ( f.List )
            return out;

        size_t size = State::ElemSize(f.ElemType) * f.Count;

        if ( f.Name == name )
        {
            out.Offset = offset;
            out.Size = size;
            out.Found = true;
            return out;
        }

        offset += size;
    }

    return out;
}

FrameMaker::FrameMaker(const State::Schema &viewSchema)
: _schema(viewSchema)
{
    _time = Find(_schema, State::KIND_WORLD, "time");
    _viewer = Find(_schema, State::KIND_WORLD, "viewer");
    _cameraPosition = Find(_schema, State::KIND_WORLD, "cameraPosition");
    _cameraRotation = Find(_schema, State::KIND_WORLD, "cameraRotation");

    _sectorType = Find(_schema, State::KIND_SECTOR, "type");
    _sectorHealth = Find(_schema, State::KIND_SECTOR, "health");
    _constructing = Find(_schema, State::KIND_SECTOR, "constructing");
    _constructBuilding = Find(_schema, State::KIND_SECTOR, "constructBuilding");
    _constructTime = Find(_schema, State::KIND_SECTOR, "constructTime");
    _constructEnd = Find(_schema, State::KIND_SECTOR, "constructEnd");

    _itemState = Find(_schema, State::KIND_SUPERITEM, "state");
    _itemRadius = Find(_schema, State::KIND_SUPERITEM, "radius");

    static const char *const extraNames[3][5] = {
        {"extra0Scale", "extra0Pos", "extra0Rotation", "extra0Flags", "extra0Model"},
        {"extra1Scale", "extra1Pos", "extra1Rotation", "extra1Flags", "extra1Model"},
        {"extra2Scale", "extra2Pos", "extra2Rotation", "extra2Flags", "extra2Model"},
    };

    for ( const auto &it : _schema )
    {
        uint8_t kind = it.first;
        if ( kind < State::KIND_UNIT )
            continue;

        UnitFields &f = _units[kind];
        f.Vehicle = Find(_schema, kind, "vehicle");
        f.StatusFlags = Find(_schema, kind, "statusFlags");
        f.Position = Find(_schema, kind, "position");
        f.Rotation = Find(_schema, kind, "rotation");
        f.Scale = Find(_schema, kind, "scale");
        f.Look = Find(_schema, kind, "look");

        for (int i = 0; i < 3; i++)
        {
            f.ExtraScale[i] = Find(_schema, kind, extraNames[i][0]);
            f.ExtraPos[i] = Find(_schema, kind, extraNames[i][1]);
            f.ExtraRotation[i] = Find(_schema, kind, extraNames[i][2]);
            f.ExtraFlags[i] = Find(_schema, kind, extraNames[i][3]);
            f.ExtraModel[i] = Find(_schema, kind, extraNames[i][4]);
        }
    }
}

void FrameMaker::Add(Frame &frame, std::vector<Frame::Sector> &sectors, uint8_t kind, uint32_t id, const std::string &data) const
{
    // A field's value, when the view has it of that size; else it stays as
    // it was
    auto get = [&data](const Field &f, void *v, size_t size)
    {
        if ( f.Found && f.Size == size && f.Offset + size <= data.size() )
            memcpy(v, data.data() + f.Offset, size);
    };

    if ( kind == State::KIND_WORLD )
    {
        get(_time, &frame.Time, sizeof(frame.Time));
        get(_viewer, &frame.Viewer, sizeof(frame.Viewer));
        get(_cameraPosition, &frame.CameraPosition, sizeof(frame.CameraPosition));
        get(_cameraRotation, &frame.CameraRotation, sizeof(frame.CameraRotation));
    }
    else if ( kind == State::KIND_SECTOR )
    {
        if ( id >= sectors.size() )
            sectors.resize(id + 1);

        Frame::Sector &s = sectors[id];
        get(_sectorType, &s.Type, sizeof(s.Type));
        get(_sectorHealth, s.Health, sizeof(s.Health));
        get(_constructing, &s.Constructing, sizeof(s.Constructing));
        get(_constructBuilding, &s.ConstructBuilding, sizeof(s.ConstructBuilding));
        get(_constructTime, &s.ConstructTime, sizeof(s.ConstructTime));
        get(_constructEnd, &s.ConstructEnd, sizeof(s.ConstructEnd));
    }
    else if ( kind == State::KIND_SUPERITEM )
    {
        if ( id >= frame.SuperItems.size() )
            frame.SuperItems.resize(id + 1);

        Frame::SuperItem &item = frame.SuperItems[id];
        get(_itemState, &item.State, sizeof(item.State));
        get(_itemRadius, &item.Radius, sizeof(item.Radius));
    }
    else if ( kind >= State::KIND_UNIT )
    {
        auto it = _units.find(kind);
        if ( it == _units.end() )
            return;

        const UnitFields &f = it->second;

        frame.Units.emplace_back();
        Frame::Unit &u = frame.Units.back();
        u.Id = id;
        u.Type = kind - State::KIND_UNIT;
        get(f.Vehicle, &u.Vehicle, sizeof(u.Vehicle));
        get(f.StatusFlags, &u.StatusFlags, sizeof(u.StatusFlags));
        get(f.Position, &u.Position, sizeof(u.Position));
        get(f.Rotation, &u.Rotation, sizeof(u.Rotation));
        get(f.Scale, &u.Scale, sizeof(u.Scale));
        get(f.Look, &u.Look, sizeof(u.Look));

        for (uint8_t i = 0; i < 3; i++)
        {
            Frame::Extra e;
            e.Unit = id;
            e.Slot = i;
            get(f.ExtraModel[i], &e.Model, sizeof(e.Model));
            get(f.ExtraFlags[i], &e.Flags, sizeof(e.Flags));

            if ( !e.Model || !(e.Flags & EVPROTO_FLAG_ACTIVE) )
                continue;

            get(f.ExtraScale[i], &e.Scale, sizeof(e.Scale));
            get(f.ExtraPos[i], &e.Position, sizeof(e.Position));
            get(f.ExtraRotation[i], &e.Rotation, sizeof(e.Rotation));
            frame.Extras.push_back(e);
        }
    }
}

void FrameMaker::End(Frame &frame, std::vector<Frame::Sector> &sectors, const Frame *before) const
{
    std::sort(frame.Units.begin(), frame.Units.end(),
              [](const Frame::Unit &a, const Frame::Unit &b) { return a.Id < b.Id; });

    std::sort(frame.Extras.begin(), frame.Extras.end(),
              [](const Frame::Extra &a, const Frame::Extra &b)
              { return a.Unit < b.Unit || (a.Unit == b.Unit && a.Slot < b.Slot); });

    if ( before && before->Sectors && *before->Sectors == sectors )
        frame.Sectors = before->Sectors;
    else
        frame.Sectors = std::make_shared<const std::vector<Frame::Sector>>(std::move(sectors));
}

void FrameMaker::Make(NC_STACK_ypaworld *world, uint32_t step, const Frame *before, Frame &out) const
{
    out = Frame();
    out.Step = step;

    std::vector<Frame::Sector> sectors;
    sectors.reserve(world->_cells.size());

    State::ForEachObject(world, State::PART_VIEW, [&](uint8_t kind, uint32_t id, const std::string &data)
    {
        Add(out, sectors, kind, id, data);
    }, &_schema);

    End(out, sectors, before);
}


// Frames kept

void Frames::Keep(size_t keep)
{
    _keep = std::max(keep, (size_t)2);

    while ( _frames.size() > _keep )
        _frames.pop_front();
}

void Frames::Add(Frame &&frame)
{
    if ( !_frames.empty() && frame.Step != _frames.back().Step + 1 )
        _frames.clear();

    _frames.push_back(std::move(frame));

    while ( _frames.size() > _keep )
        _frames.pop_front();
}

void Frames::DropAfter(uint32_t step)
{
    while ( !_frames.empty() && _frames.back().Step > step )
        _frames.pop_back();
}

const Frame *Frames::At(uint32_t step) const
{
    if ( _frames.empty() || step < _frames.front().Step || step > _frames.back().Step )
        return NULL;

    return &_frames[step - _frames.front().Step];
}

}
