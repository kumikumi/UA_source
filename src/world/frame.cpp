#include <algorithm>
#include <cstring>

#include "../yw.h"
#include "../log.h"
#include "../system/perf.h"
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
    return Type == b.Type && !memcmp(Health, b.Health, sizeof(Health)) && Owner == b.Owner && Purpose == b.Purpose &&
           Power == b.Power && StationPower == b.StationPower;
}

const Frame::Player *Frame::FindPlayer(uint32_t id) const
{
    for ( const Player &p : Players )
    {
        if ( p.Id == id )
            return &p;
    }

    return NULL;
}

void Frame::LookAs(uint32_t player)
{
    const Player *p = FindPlayer(player);

    if ( !p )
        return;

    Viewer = p->Unit;
    UserUnit = p->Unit;
    UserRobo = p->HostStation;
    CameraPosition = p->CameraPosition;
    CameraRotation = p->CameraRotation;
}

const Frame::Unit *Frame::FindUnit(uint32_t id) const
{
    auto it = std::lower_bound(Units.begin(), Units.end(), id,
                               [](const Unit &u, uint32_t id) { return u.Id < id; });

    if ( it == Units.end() || it->Id != id )
        return NULL;

    return &*it;
}

const Frame::UnitInfo *Frame::FindInfo(uint32_t id) const
{
    const Unit *unit = FindUnit(id);
    return unit ? InfoAt(unit - Units.data()) : NULL;
}

const Frame::HostStation *Frame::FindHostStation(uint32_t id) const
{
    auto it = std::lower_bound(HostStations.begin(), HostStations.end(), id,
                               [](const HostStation &h, uint32_t id) { return h.Unit < id; });

    if ( it == HostStations.end() || it->Unit != id )
        return NULL;

    return &*it;
}

const Frame::Construction *Frame::FindConstruction(uint32_t sector) const
{
    for (const Construction &c : Constructions)
    {
        if ( c.Sector == sector )
            return &c;
    }

    return NULL;
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
    out.UserRobo = nearer.UserRobo;
    out.UserUnit = nearer.UserUnit;
    out.Sectors = nearer.Sectors;
    out.SuperItems = nearer.SuperItems;
    out.Infos = nearer.Infos;
    out.HostStations = nearer.HostStations;

    out.CameraPosition = nearer.CameraPosition;
    out.CameraRotation = nearer.CameraRotation;

    if ( !IsStepJump(a.CameraPosition, a.CameraRotation, b.CameraPosition, b.CameraRotation) )
    {
        out.CameraPosition = a.CameraPosition + (b.CameraPosition - a.CameraPosition) * t;
        out.CameraRotation = Blend(a.CameraRotation, b.CameraRotation, t);
    }

    // Each player's camera as the one drawn from, so that any of them can be
    out.LocalPlayer = nearer.LocalPlayer;
    out.Players = nearer.Players;

    for ( Player &p : out.Players )
    {
        const Player *pa = a.FindPlayer(p.Id);
        const Player *pb = b.FindPlayer(p.Id);

        if ( pa && pb && !IsStepJump(pa->CameraPosition, pa->CameraRotation, pb->CameraPosition, pb->CameraRotation) )
        {
            p.CameraPosition = pa->CameraPosition + (pb->CameraPosition - pa->CameraPosition) * t;
            p.CameraRotation = Blend(pa->CameraRotation, pb->CameraRotation, t);
        }
    }

    // A building grows in between
    out.Constructions = nearer.Constructions;

    for (Construction &c : out.Constructions)
    {
        const Construction *ca = a.FindConstruction(c.Sector);
        const Construction *cb = b.FindConstruction(c.Sector);

        if ( ca && cb && ca->Building == cb->Building )
            c.Time = ca->Time + (cb->Time - ca->Time) * t;
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
            out.Type = f.ElemType;
            out.Found = true;
            return out;
        }

        offset += size;
    }

    return out;
}

FrameMaker::FrameMaker(const State::Schema &viewSchema, bool details)
: _schema(viewSchema), _details(details)
{
    _time = Find(_schema, State::KIND_WORLD, "time");
    _localPlayer = Find(_schema, State::KIND_WORLD, "localPlayer");

    _playerOwner = Find(_schema, State::KIND_PLAYER, "owner");
    _playerHostStation = Find(_schema, State::KIND_PLAYER, "hostStation");
    _playerUnit = Find(_schema, State::KIND_PLAYER, "unit");
    _playerControls = Find(_schema, State::KIND_PLAYER, "controls");
    _playerCameraPosition = Find(_schema, State::KIND_PLAYER, "cameraPosition");
    _playerCameraRotation = Find(_schema, State::KIND_PLAYER, "cameraRotation");
    _playerName = Find(_schema, State::KIND_PLAYER, "name");

    _sectorType = Find(_schema, State::KIND_SECTOR, "type");
    _sectorHealth = Find(_schema, State::KIND_SECTOR, "health");
    _sectorOwner = Find(_schema, State::KIND_SECTOR, "owner");
    _sectorPurpose = Find(_schema, State::KIND_SECTOR, "purpose");
    _sectorPower = Find(_schema, State::KIND_SECTOR, "energyPower");
    _powerStation = Find(_schema, State::KIND_SECTOR, "powerStation");
    _stationPower = Find(_schema, State::KIND_SECTOR, "effectivePower");
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

        f.Owner = Find(_schema, kind, "owner");
        f.Status = Find(_schema, kind, "status");
        f.Aggression = Find(_schema, kind, "aggression");
        f.Shield = Find(_schema, kind, "shield");
        f.Energy = Find(_schema, kind, "energy");
        f.EnergyMax = Find(_schema, kind, "energyMax");
        f.Parent = Find(_schema, kind, "parent");
        f.PrimaryType = Find(_schema, kind, "primaryType");
        f.Primary = Find(_schema, kind, "primary");
        f.PrimaryPos = Find(_schema, kind, "primaryPos");
        f.SecondaryType = Find(_schema, kind, "secondaryType");
        f.Secondary = Find(_schema, kind, "secondary");
        f.GunFlags = Find(_schema, kind, "gunFlags");
        f.ClockStart = Find(_schema, kind, "clockStart");
        f.WeaponTime = Find(_schema, kind, "weaponTime");
        f.SalveCounter = Find(_schema, kind, "salveCounter");

        f.Create = Find(_schema, kind, "energyLife");
        f.Beam = Find(_schema, kind, "energyMove");
        f.BuildSpare = Find(_schema, kind, "buildSpare");
        f.VehicleSpare = Find(_schema, kind, "vehicleSpare");
        f.ReloadPS = Find(_schema, kind, "energyReloadPS");
        f.FillMode = Find(_schema, kind, "fillMode");
        f.LoadFlags = Find(_schema, kind, "energyLoadFlags");
        f.LossFlags = Find(_schema, kind, "energyLossFlags");
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

    // A unit's position and rotation, which the view has in fixed point and
    // as a quaternion
    auto getPosition = [&data](const Field &f, vec3f &v)
    {
        int32_t fixed[3];

        if ( f.Found && f.Type == State::T_FIXED && f.Size == sizeof(fixed) && f.Offset + f.Size <= data.size() )
        {
            memcpy(fixed, data.data() + f.Offset, sizeof(fixed));
            v = State::PositionFromView(fixed);
        }
    };

    auto getRotation = [&data](const Field &f, mat3x3f &m)
    {
        int16_t quat[4];

        if ( f.Found && f.Type == State::T_QUAT && f.Size == sizeof(quat) && f.Offset + f.Size <= data.size() )
        {
            memcpy(quat, data.data() + f.Offset, sizeof(quat));
            m = State::RotationFromView(quat);
        }
    };

    if ( kind == State::KIND_WORLD )
    {
        get(_time, &frame.Time, sizeof(frame.Time));
        get(_localPlayer, &frame.LocalPlayer, sizeof(frame.LocalPlayer));
    }
    else if ( kind == State::KIND_PLAYER )
    {
        Frame::Player p;
        p.Id = id;
        get(_playerOwner, &p.Owner, sizeof(p.Owner));
        get(_playerHostStation, &p.HostStation, sizeof(p.HostStation));
        get(_playerUnit, &p.Unit, sizeof(p.Unit));
        get(_playerControls, &p.Controls, sizeof(p.Controls));
        get(_playerCameraPosition, &p.CameraPosition, sizeof(p.CameraPosition));
        get(_playerCameraRotation, &p.CameraRotation, sizeof(p.CameraRotation));

        if ( _playerName.Found && _playerName.Offset + _playerName.Size <= data.size() )
        {
            const char *name = data.data() + _playerName.Offset;
            p.Name.assign(name, strnlen(name, _playerName.Size));
        }

        frame.Players.push_back(p);
    }
    else if ( kind == State::KIND_SECTOR )
    {
        if ( id >= sectors.size() )
            sectors.resize(id + 1);

        Frame::Sector &s = sectors[id];
        get(_sectorType, &s.Type, sizeof(s.Type));
        get(_sectorHealth, s.Health, sizeof(s.Health));
        get(_sectorOwner, &s.Owner, sizeof(s.Owner));
        get(_sectorPurpose, &s.Purpose, sizeof(s.Purpose));
        get(_sectorPower, &s.Power, sizeof(s.Power));

        bool powerStation = false;
        get(_powerStation, &powerStation, sizeof(powerStation));

        if ( powerStation )
            get(_stationPower, &s.StationPower, sizeof(s.StationPower));

        bool constructing = false;
        get(_constructing, &constructing, sizeof(constructing));

        if ( constructing )
        {
            Frame::Construction c;
            c.Sector = id;
            get(_constructBuilding, &c.Building, sizeof(c.Building));
            get(_constructTime, &c.Time, sizeof(c.Time));
            get(_constructEnd, &c.End, sizeof(c.End));
            frame.Constructions.push_back(c);
        }
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
        getPosition(f.Position, u.Position);
        getRotation(f.Rotation, u.Rotation);
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
            getPosition(f.ExtraPos[i], e.Position);
            getRotation(f.ExtraRotation[i], e.Rotation);
            frame.Extras.push_back(e);
        }

        if ( !_details )
            return;

        frame.Infos.emplace_back();
        Frame::UnitInfo &info = frame.Infos.back();
        get(f.Owner, &info.Owner, sizeof(info.Owner));
        get(f.Status, &info.Status, sizeof(info.Status));
        get(f.Aggression, &info.Aggression, sizeof(info.Aggression));

        uint8_t shield = 0;
        if ( f.Shield.Found )
        {
            get(f.Shield, &shield, sizeof(shield));
            info.Shield = shield;
        }

        get(f.Energy, &info.Energy, sizeof(info.Energy));
        get(f.EnergyMax, &info.EnergyMax, sizeof(info.EnergyMax));
        get(f.Parent, &info.Parent, sizeof(info.Parent));
        get(f.PrimaryType, &info.PrimaryType, sizeof(info.PrimaryType));
        get(f.Primary, &info.Primary, sizeof(info.Primary));
        get(f.PrimaryPos, &info.PrimaryPos, sizeof(info.PrimaryPos));
        get(f.SecondaryType, &info.SecondaryType, sizeof(info.SecondaryType));
        get(f.Secondary, &info.Secondary, sizeof(info.Secondary));

        if ( f.GunFlags.Found )
        {
            uint8_t gunFlags = 0;
            get(f.GunFlags, &gunFlags, sizeof(gunFlags));
            info.GunFlags = gunFlags;
        }
        get(f.ClockStart, &info.ClockStart, sizeof(info.ClockStart));
        get(f.WeaponTime, &info.WeaponTime, sizeof(info.WeaponTime));
        get(f.SalveCounter, &info.SalveCounter, sizeof(info.SalveCounter));

        if ( f.Create.Found )
        {
            Frame::HostStation h;
            h.Unit = id;
            get(f.Create, &h.Create, sizeof(h.Create));
            get(f.Beam, &h.Beam, sizeof(h.Beam));
            get(f.BuildSpare, &h.BuildSpare, sizeof(h.BuildSpare));
            get(f.VehicleSpare, &h.VehicleSpare, sizeof(h.VehicleSpare));
            get(f.ReloadPS, &h.ReloadPS, sizeof(h.ReloadPS));
            get(f.FillMode, &h.FillMode, sizeof(h.FillMode));
            get(f.LoadFlags, &h.LoadFlags, sizeof(h.LoadFlags));
            get(f.LossFlags, &h.LossFlags, sizeof(h.LossFlags));
            frame.HostStations.push_back(h);
        }
    }
}

void FrameMaker::End(Frame &frame, std::vector<Frame::Sector> &sectors, const Frame *before) const
{
    if ( frame.Infos.size() == frame.Units.size() && !frame.Infos.empty() )
    {
        // Units and what there is to show of them, in the same order
        std::vector<size_t> order(frame.Units.size());
        for (size_t i = 0; i < order.size(); i++)
            order[i] = i;

        std::sort(order.begin(), order.end(),
                  [&frame](size_t a, size_t b) { return frame.Units[a].Id < frame.Units[b].Id; });

        std::vector<Frame::Unit> units;
        std::vector<Frame::UnitInfo> infos;
        units.reserve(order.size());
        infos.reserve(order.size());

        for (size_t i : order)
        {
            units.push_back(frame.Units[i]);
            infos.push_back(frame.Infos[i]);
        }

        frame.Units = std::move(units);
        frame.Infos = std::move(infos);
    }
    else
    {
        frame.Infos.clear();
        std::sort(frame.Units.begin(), frame.Units.end(),
                  [](const Frame::Unit &a, const Frame::Unit &b) { return a.Id < b.Id; });
    }

    std::sort(frame.HostStations.begin(), frame.HostStations.end(),
              [](const Frame::HostStation &a, const Frame::HostStation &b) { return a.Unit < b.Unit; });

    std::sort(frame.Players.begin(), frame.Players.end(),
              [](const Frame::Player &a, const Frame::Player &b) { return a.Id < b.Id; });

    // As the player at the machine that played it saw it, if they played
    frame.LookAs(frame.LocalPlayer);

    std::sort(frame.Constructions.begin(), frame.Constructions.end(),
              [](const Frame::Construction &a, const Frame::Construction &b) { return a.Sector < b.Sector; });

    std::sort(frame.Extras.begin(), frame.Extras.end(),
              [](const Frame::Extra &a, const Frame::Extra &b)
              { return a.Unit < b.Unit || (a.Unit == b.Unit && a.Slot < b.Slot); });

    // Frames are kept by the hundred: without room to grow
    frame.Units.shrink_to_fit();
    frame.Extras.shrink_to_fit();

    if ( before && before->Sectors && *before->Sectors == sectors )
        frame.Sectors = before->Sectors;
    else
        frame.Sectors = std::make_shared<const std::vector<Frame::Sector>>(std::move(sectors));
}

void FrameMaker::FromView(const ObjectsVersions &view, uint32_t step, const Frame *before, Frame &out) const
{
    out = Frame();
    out.Step = step;

    if ( before )
        out.Units.reserve(before->Units.size());

    std::vector<Frame::Sector> sectors;

    for ( const auto &it : view )
        Add(out, sectors, it.first >> 32, it.first & 0xFFFFFFFF, it.second.Data);

    End(out, sectors, before);
}

void FrameMaker::Make(NC_STACK_ypaworld *world, uint32_t step, const Frame *before, Frame &out) const
{
    out = Frame();
    out.Step = step;

    if ( before )
        out.Units.reserve(before->Units.size());

    std::vector<Frame::Sector> sectors;
    sectors.reserve(world->_cells.size());

    State::ForEachObject(world, State::PART_VIEW, [&](uint8_t kind, uint32_t id, const std::string &data)
    {
        Add(out, sectors, kind, id, data);
    }, &_schema);

    End(out, sectors, before);
}


// Frames kept

void Frames::Add(Frame &&frame)
{
    if ( !_frames.empty() && frame.Step != _frames.back().Step + 1 )
        _frames.clear();

    _frames.push_back(std::move(frame));
}

bool Frames::Prepend(std::vector<Frame> &&frames)
{
    if ( frames.empty() )
        return true;

    if ( !_frames.empty() && frames.back().Step + 1 != _frames.front().Step )
        return false;

    _frames.insert(_frames.begin(), std::make_move_iterator(frames.begin()), std::make_move_iterator(frames.end()));
    frames.clear();
    return true;
}

void Frames::DropAfter(uint32_t step)
{
    while ( !_frames.empty() && _frames.back().Step > step )
        _frames.pop_back();
}

void Frames::DropBefore(uint32_t step)
{
    while ( _frames.size() > 1 && _frames.front().Step < step )
        _frames.pop_front();
}


// Frames from a recording

bool FramesFromRecording::Work(const FrameMaker &maker, double ms)
{
    uint64_t start = System::PerfStats::Now();

    while ( !_done )
    {
        bool read;

        if ( !_reader )
        {
            _recording.reset(new Recording);
            _reader.reset(new ViewReader(*_recording));
            read = _recording->Open(_path) && _reader->Seek(_from);
        }
        else
        {
            read = _reader->Next();
        }

        if ( !read )
        {
            ypa_log_out("Could not make the frames of steps %d to %d from %s\n", _from, _to, _path.c_str());
            _failed = true;
            _done = true;
            break;
        }

        _made.emplace_back();
        maker.FromView(_reader->View(), _reader->Step(), _made.size() > 1 ? &_made[_made.size() - 2] : NULL, _made.back());

        if ( _reader->Step() + 1 >= _to )
            _done = true;
        else if ( System::PerfStats::TicksToMs(System::PerfStats::Now() - start) >= ms )
            break;
    }

    if ( _done )
    {
        _reader.reset();
        _recording.reset();
    }

    return _done;
}

const Frame *Frames::At(uint32_t step) const
{
    if ( _frames.empty() || step < _frames.front().Step || step > _frames.back().Step )
        return NULL;

    return &_frames[step - _frames.front().Step];
}

}
