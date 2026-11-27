#ifndef WORLD_PREDICTION_H_INCLUDED
#define WORLD_PREDICTION_H_INCLUDED

#include <cstdint>
#include <deque>
#include <map>
#include <vector>

#include "../vectors.h"
#include "../matrix.h"
#include "frame.h"
#include "playercontrols.h"
#include "state.h"
#include "viewstream.h"

class NC_STACK_ypaworld;
class NC_STACK_ypabact;
class NC_STACK_ypamissile;

namespace World
{

// Driving a unit at this machine in a game that runs elsewhere (client-side
// prediction). The game goes by the player's controls a while after they
// make them, and what it makes of them comes back later still. So the unit
// the player controls is stepped here as well, as the game steps it, by
// their controls as they make them: it moves as soon as they steer. As the
// game sends the unit as it had it after a step (ControlledUnit), with the
// number of the controls it went by, the unit here starts over from there
// and goes by the controls the game hadn't gone by yet again.
//
// Only what the controls move is stepped, and the missiles they fire: those
// fly here as they're fired, as the game's code flies them, and hit what's
// drawn, but deal no damage. The game deals it, as its own missiles hit,
// which aren't drawn as long as the ones fired here show them (as many of
// them as were fired here). Machine guns' hits are the game's, and other
// units and players are as the game shows them.
class Prediction
{
public:
    // In the world of a game watched live, whose units' state has the
    // fields of unitSchema in the game's build
    Prediction(NC_STACK_ypaworld *world, const State::Schema &unitSchema);
    ~Prediction();

    // Whether this build reads the game's units: the same fields
    bool Possible() const { return _possible; }

    // The unit as the game had it after a step: driven on from there by the
    // controls since
    void FromGame(const ControlledUnit &unit);

    // The player's controls of a step, as they're sent to the game: the
    // unit goes by them, and its shots fly on, hitting units as the frame
    // drawn last has them
    void Steer(const PlayerControls &controls, const Frame &drawn);

    // The unit driven, 0 for none: none when the game hasn't sent it for a
    // while, as when the player rides along or looks from a missile
    uint32_t UnitId() const;

    // A frame drawn as the player drives the unit: the unit where it is
    // fraction of the way from the step before the last to the last, the
    // player's camera on it, and its shots as they fly here instead of the
    // game's
    void Apply(Frame &frame, uint32_t player, float fraction);

protected:
    // Where a unit is, and its camera and gun
    struct Pose
    {
        vec3d Position;
        mat3x3 Rotation;
        vec3d CameraPosition;
        mat3x3 CameraRotation;
        float GunAngle = 0.0;
        float GunLeftRight = 0.0;
    };

    // Controls made, and where the unit was after going by them here, if
    // it was driven then
    struct Steered
    {
        PlayerControls Controls;
        bool Driven = false;
        vec3d Position;
    };

    // A shot fired here
    struct Shot
    {
        NC_STACK_ypamissile *Missile = NULL;
        uint32_t DrawnId = 0; // In frames, past the ids of the game's units
        vec3d Before;         // Where it was a step before
    };

    // A step of the unit, and of its shots: kept only when the controls are
    // gone by the first time, as they're made
    void StepUnit(const PlayerControls &controls, bool shoot);
    void StepShots(const Frame &drawn);
    void ApplyShots(Frame &frame, float fraction);
    Pose PoseOf(const NC_STACK_ypabact *unit) const;
    void Drop();
    void DeleteUnit(NC_STACK_ypabact *unit);

    NC_STACK_ypaworld *_world;
    bool _possible = false;

    NC_STACK_ypabact *_unit = NULL;
    uint8_t _kind = 0;
    uint32_t _id = 0;
    int32_t _time = 0;             // The game's time of the unit's state
    uint32_t _sinceGame = 0;       // Steps since the game sent it
    std::deque<Steered> _steered;  // The last controls made, the oldest first
    Pose _before, _now;            // After the step before the last, and the last
    vec3d _correction;             // From where it's driven to where it's drawn
    std::vector<Shot> _shots;
    uint32_t _shotsFired = 0;

    // The game's missiles of the unit as they came, whether each is drawn
    // (by id), and how many came since none flew; while fewer came than
    // were fired here, the ones fired here show them
    std::map<uint32_t, bool> _gameShots;
    uint32_t _gameShotsCame = 0, _shotsShowing = 0;

    // How far the game had the unit from where it was here after the same
    // controls: how well it was predicted
    uint32_t _corrections = 0;
    double _errorSum = 0.0, _errorMax = 0.0;
};

}

#endif
