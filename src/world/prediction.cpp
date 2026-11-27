#include <algorithm>
#include <map>

#include "prediction.h"
#include "consts.h"
#include "../yw.h"
#include "../ypamissile.h"
#include "../log.h"

namespace World
{

// When the game hasn't sent the unit for this many steps, it's no longer the
// player's to drive
static const uint32_t MaxStepsWithoutGame = 30;

// The controls kept: the game is never further behind than that
static const size_t MaxSteered = 2 * CVStepsPerSecond;

// The ids shots fired here have in frames: past those of the game's units
static const uint32_t FirstShotId = 0xF0000000;

// A unit that moves further than this in a step, as when it's set somewhere
// else, is drawn where it ended up instead of in between
static const double MaxInterpolatedMove = 300.0;

// Where the game had the unit other than where it was driven here, it's
// drawn as it was, nearer each step by this much of the way, unless that's
// further than this
static const double CorrectionKept = 0.8;
static const double MaxCorrection = 200.0;

static State::Schema UnitsOnly(State::Schema schema)
{
    schema.erase(schema.begin(), schema.lower_bound(State::KIND_UNIT));
    return schema;
}

Prediction::Prediction(NC_STACK_ypaworld *world, const State::Schema &unitSchema)
: _world(world)
{
    _possible = State::WriteSchema(UnitsOnly(State::MakeSchema(world, State::PART_ALL))) == State::WriteSchema(unitSchema);

    if ( !_possible )
        ypa_log_out("Prediction: the game's units have other fields than here: the unit played is shown as the game sends it\n");
}

Prediction::~Prediction()
{
    if ( _corrections )
        ypa_log_out("Prediction: %u steps the game sent, %.1f off on average from where they were driven here, %.1f at most\n",
                    _corrections, _errorSum / _corrections, _errorMax);

    Drop();
}

// A unit gone from here: not one that died, as the game would have it die
void Prediction::DeleteUnit(NC_STACK_ypabact *unit)
{
    unit->_status_flg |= BACT_STFLAG_DEATH1;
    unit->Delete();
}

void Prediction::Drop()
{
    // Its shots, which it fired
    for ( Shot &shot : _shots )
        DeleteUnit(shot.Missile);

    _shots.clear();

    if ( _unit )
        DeleteUnit(_unit);

    _unit = NULL;
    _kind = 0;
    _id = 0;
}

uint32_t Prediction::UnitId() const
{
    if ( !_unit || _sinceGame > MaxStepsWithoutGame )
        return 0;

    // Driven, as the controls drive a unit that goes on
    if ( _unit->_status != BACT_STATUS_NORMAL && _unit->_status != BACT_STATUS_IDLE )
        return 0;

    return _id;
}

void Prediction::FromGame(const ControlledUnit &unit)
{
    if ( !_possible )
        return;

    // Where it's drawn now, to go on from there
    bool driven = UnitId() && unit.Id == _id;
    vec3d drawnAt = _now.Position + _correction;

    if ( !_unit || unit.Kind != _kind || unit.Id != _id )
    {
        Drop();

        _unit = _world->NewUnitObject(unit.Kind - State::KIND_UNIT);

        if ( !_unit )
            return;

        _unit->_gid = unit.Id;
        _kind = unit.Kind;
        _id = unit.Id;
    }

    // All of it as the game had it, its clock by the game's time then
    int32_t worldTime = _world->_timeStamp;
    _world->_timeStamp = unit.Time;

    std::map<uint32_t, NC_STACK_ypabact *> units = {{unit.Id, _unit}};

    State::Visitor v(State::Visitor::READ, _world, State::PART_ALL);
    v.SetUnits(&units);
    v.SetData(&unit.Fields);
    _unit->VisitState(v);

    _world->_timeStamp = worldTime;

    if ( !v.ReadAll() )
    {
        ypa_log_out("Prediction: the unit played doesn't read: it's shown as the game sends it\n");
        Drop();
        _possible = false;
        return;
    }

    // The units it refers to aren't here
    if ( _unit->_primTtype == BACT_TGT_TYPE_UNIT && !_unit->_primT.pbact )
        _unit->_primTtype = BACT_TGT_TYPE_NONE;

    if ( _unit->_secndTtype == BACT_TGT_TYPE_UNIT && !_unit->_secndT.pbact )
        _unit->_secndTtype = BACT_TGT_TYPE_NONE;

    _unit->_attackersList.remove_if([](const TBactAttacker &a) { return !a.attacker; });

    _time = unit.Time;
    _sinceGame = 0;

    // How far the game had it from where it was here after the same
    // controls; those the game went by are done with
    while ( !_steered.empty() && _steered.front().Controls.Number <= unit.Controls )
    {
        if ( _steered.front().Controls.Number == unit.Controls && _steered.front().Driven )
        {
            double error = (_steered.front().Position - _unit->_position).length();
            _corrections++;
            _errorSum += error;
            _errorMax = std::max(_errorMax, error);
        }

        _steered.pop_front();
    }

    _now = _before = PoseOf(_unit);

    if ( !UnitId() )
        return;

    // On from there by those it hadn't gone by yet
    for ( Steered &s : _steered )
    {
        StepUnit(s.Controls, false);
        s.Driven = true;
        s.Position = _unit->_position;
    }

    _correction = driven ? drawnAt - _now.Position : vec3d();

    if ( _correction.length() > MaxCorrection )
        _correction = vec3d();
}

void Prediction::Steer(const PlayerControls &controls, const Frame &drawn)
{
    _steered.emplace_back();
    _steered.back().Controls = controls;

    while ( _steered.size() > MaxSteered )
        _steered.pop_front();

    _sinceGame++;
    _correction *= CorrectionKept;

    if ( UnitId() )
    {
        StepUnit(controls, true);
        _steered.back().Driven = true;
        _steered.back().Position = _unit->_position;
    }

    StepShots(drawn);
}

void Prediction::StepUnit(const PlayerControls &controls, bool shoot)
{
    TInputState input;
    input.Period = CVStepTicks;
    controls.To(input);

    _time += CVStepTicks;

    update_msg msg;
    msg.gTime = _time;
    msg.frameTime = CVStepTicks;
    msg.inpt = &input;

    // In the game's time, and dealt no damage: that's the game's to deal
    int32_t worldTime = _world->_timeStamp;
    bool invulnerable = _world->_invulnerable;
    _world->_timeStamp = _time;
    _world->_invulnerable = true;

    _unit->PredictStep(&msg);

    _world->_timeStamp = worldTime;
    _world->_invulnerable = invulnerable;

    // What it fired: missiles the first time, and gone when it's fired
    // again as the unit goes by the same controls again, as are machine
    // guns' hits, which come dead
    for ( NC_STACK_ypamissile *missile : _unit->_missiles_list )
    {
        if ( !shoot || missile->_status == BACT_STATUS_DEAD )
        {
            DeleteUnit(missile);
            continue;
        }

        missile->SetPhantom();

        Shot shot;
        shot.Missile = missile;
        shot.DrawnId = FirstShotId + _shotsFired++;
        shot.Before = missile->_position;
        _shots.push_back(shot);
        _shotsShowing++;
    }

    _unit->_missiles_list.clear();

    _before = _now;
    _now = PoseOf(_unit);
}

// Whether a unit as big as radius at center is in the way from a to b, and
// where it's hit
static bool HitOnTheWay(const vec3d &a, const vec3d &b, const vec3d &center, double radius, vec3d &at)
{
    vec3d way = b - a;
    double length = way.length();
    double along = length > 0.0 ? std::max(0.0, std::min((center - a).dot(way) / length, length)) : 0.0;

    at = length > 0.0 ? a + way * (along / length) : a;
    return (center - at).length() < radius;
}

void Prediction::StepShots(const Frame &drawn)
{
    TInputState none;
    none.Period = CVStepTicks;

    update_msg msg;
    msg.gTime = _time;
    msg.frameTime = CVStepTicks;
    msg.inpt = &none;

    int32_t worldTime = _world->_timeStamp;
    bool invulnerable = _world->_invulnerable;
    _world->_timeStamp = _time;
    _world->_invulnerable = true;

    for ( Shot &shot : _shots )
    {
        NC_STACK_ypamissile *missile = shot.Missile;
        bool flying = missile->_status == BACT_STATUS_NORMAL;

        shot.Before = missile->_position;
        missile->Update(&msg);

        if ( !flying || missile->_status != BACT_STATUS_NORMAL )
            continue;

        // Into a unit drawn: shown hitting it, as the game would hit it
        for ( size_t i = 0; i < drawn.Units.size(); i++ )
        {
            const Frame::Unit &unit = drawn.Units[i];
            const Frame::UnitInfo *info = drawn.InfoAt(i);
            vec3d at;

            if ( unit.Id == _id || unit.Type == BACT_TYPES_MISSLE || unit.Id >= FirstShotId || unit.Vehicle >= _world->_vhclProtos.size() ||
                 (info && info->Status == BACT_STATUS_DEAD) )
                continue;

            double radius = _world->_vhclProtos[unit.Vehicle].radius + missile->_radius;

            if ( HitOnTheWay(shot.Before, missile->_position, vec3d(unit.Position), radius, at) )
            {
                missile->_position = at;

                setState_msg state;
                state.newStatus = BACT_STATUS_DEAD;
                state.setFlags = 0;
                state.unsetFlags = 0;
                missile->SetState(&state);
                break;
            }
        }
    }

    _world->_timeStamp = worldTime;
    _world->_invulnerable = invulnerable;

    // Those that showed their end
    for ( auto it = _shots.begin(); it != _shots.end(); )
    {
        if ( it->Missile->getBACT_yourLastSeconds() <= 0 )
        {
            DeleteUnit(it->Missile);
            it = _shots.erase(it);
        }
        else
            it++;
    }
}

Prediction::Pose Prediction::PoseOf(const NC_STACK_ypabact *unit) const
{
    Pose pose;
    pose.Position = unit->_position;
    pose.Rotation = unit->_rotation;
    pose.GunAngle = unit->_gun_angle_user;
    pose.GunLeftRight = unit->_gun_leftright;

    // Where the player sees from, as the game places their camera on it
    if ( unit->getBACT_extraViewer() )
    {
        pose.CameraPosition = unit->_position + unit->_rotation.Transpose().Transform(unit->_viewer_position);
        pose.CameraRotation = unit->_viewer_rotation;
    }
    else
    {
        pose.CameraPosition = unit->_position;
        pose.CameraRotation = unit->_rotation;
    }

    return pose;
}

void Prediction::Apply(Frame &frame, uint32_t player, float fraction)
{
    uint32_t id = UnitId();

    if ( !id )
    {
        ApplyShots(frame, fraction);
        return;
    }

    // While the frame has the player in it, as they drive it
    auto p = std::find_if(frame.Players.begin(), frame.Players.end(), [player](const Frame::Player &p) { return p.Id == player; });

    if ( p == frame.Players.end() || p->Controls != id || p->Unit != id )
        return;

    auto u = std::lower_bound(frame.Units.begin(), frame.Units.end(), id, [](const Frame::Unit &u, uint32_t id) { return u.Id < id; });

    if ( u == frame.Units.end() || u->Id != id )
        return;

    Pose pose = _now;

    if ( (_now.Position - _before.Position).length() <= MaxInterpolatedMove )
    {
        float t = std::max(0.0f, std::min(fraction, 1.0f));

        pose.Position = _before.Position + (_now.Position - _before.Position) * t;
        pose.Rotation = _before.Rotation * (1.0 - t) + _now.Rotation * t;
        pose.CameraPosition = _before.CameraPosition + (_now.CameraPosition - _before.CameraPosition) * t;
        pose.CameraRotation = _before.CameraRotation * (1.0 - t) + _now.CameraRotation * t;
        pose.GunAngle = _before.GunAngle + (_now.GunAngle - _before.GunAngle) * t;
        pose.GunLeftRight = _before.GunLeftRight + (_now.GunLeftRight - _before.GunLeftRight) * t;
    }

    pose.Position += _correction;
    pose.CameraPosition += _correction;

    u->Position = vec3f(pose.Position);
    u->Rotation = mat3x3f(pose.Rotation);

    size_t index = u - frame.Units.begin();

    // Its gun, and its weapon's reload by its clock here
    if ( index < frame.Infos.size() )
    {
        Frame::UnitInfo &info = frame.Infos[index];
        info.GunAngle = pose.GunAngle;
        info.GunLeftRight = pose.GunLeftRight;
        info.ClockStart = frame.Time - _unit->_clock;
        info.WeaponTime = _unit->_weapon_time;
        info.SalveCounter = _unit->_salve_counter;
    }

    // As it looks firing
    u->Look = _unit->ModelIndex(_unit->GetVP());

    p->CameraPosition = vec3f(pose.CameraPosition);
    p->CameraRotation = mat3x3f(pose.CameraRotation);

    ApplyShots(frame, fraction);
}

void Prediction::ApplyShots(Frame &frame, float fraction)
{
    if ( !_id )
        return;

    bool infos = frame.Infos.size() == frame.Units.size();

    // The game's missiles of the unit, as they come: not drawn while the
    // ones fired here show them
    if ( infos )
    {
        std::map<uint32_t, bool> flying;
        size_t kept = 0;

        for ( size_t i = 0; i < frame.Units.size(); i++ )
        {
            const Frame::UnitInfo &info = frame.Infos[i];

            if ( frame.Units[i].Type == BACT_TYPES_MISSLE && info.Emitter == _id )
            {
                auto it = _gameShots.find(frame.Units[i].Id);
                bool hidden;

                if ( it != _gameShots.end() )
                    hidden = it->second;
                else if ( info.Status == BACT_STATUS_DEAD )
                    hidden = false;
                else
                    hidden = _gameShotsCame++ < _shotsShowing;

                flying[frame.Units[i].Id] = hidden;

                if ( hidden )
                    continue;
            }

            frame.Units[kept] = frame.Units[i];
            frame.Infos[kept] = frame.Infos[i];
            kept++;
        }

        frame.Units.resize(kept);
        frame.Infos.resize(kept);
        _gameShots.swap(flying);

        // None of either left: they're counted anew
        if ( _gameShots.empty() && _shots.empty() )
            _gameShotsCame = _shotsShowing = 0;
    }

    float t = std::max(0.0f, std::min(fraction, 1.0f));

    for ( const Shot &shot : _shots )
    {
        NC_STACK_ypamissile *missile = shot.Missile;

        Frame::Unit unit;
        unit.Id = shot.DrawnId;
        unit.Type = BACT_TYPES_MISSLE;
        unit.Vehicle = missile->_vehicleID;
        unit.Look = missile->ModelIndex(missile->GetVP());
        unit.StatusFlags = missile->_status_flg;
        unit.Position = vec3f(shot.Before + (missile->_position - shot.Before) * t);
        unit.Rotation = mat3x3f(missile->_rotation);
        unit.Scale = vec3f(missile->_scale);
        frame.Units.push_back(unit);

        if ( infos )
        {
            Frame::UnitInfo info;
            info.Owner = missile->_owner;
            info.Status = missile->_status;
            info.Emitter = _id;
            frame.Infos.push_back(info);
        }
    }
}

}
