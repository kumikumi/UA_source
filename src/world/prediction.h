#ifndef WORLD_PREDICTION_H_INCLUDED
#define WORLD_PREDICTION_H_INCLUDED

#include <cstdint>
#include <deque>

#include "../vectors.h"
#include "../matrix.h"
#include "frame.h"
#include "playercontrols.h"
#include "state.h"
#include "viewstream.h"

class NC_STACK_ypaworld;
class NC_STACK_ypabact;

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
// Only what the controls move is stepped: the game fires the unit's weapons
// and deals out damage, and other units and players are as the game shows
// them.
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
    // unit goes by them
    void Steer(const PlayerControls &controls);

    // The unit driven, 0 for none: none when the game hasn't sent it for a
    // while, as when the player rides along or looks from a missile
    uint32_t UnitId() const;

    // A frame drawn as the player drives the unit: the unit where it is
    // fraction of the way from the step before the last to the last, and
    // the player's camera on it
    void Apply(Frame &frame, uint32_t player, float fraction) const;

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

    void StepUnit(const PlayerControls &controls);
    Pose PoseOf(const NC_STACK_ypabact *unit) const;
    void Drop();

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

    // How far the game had the unit from where it was here after the same
    // controls: how well it was predicted
    uint32_t _corrections = 0;
    double _errorSum = 0.0, _errorMax = 0.0;
};

}

#endif
