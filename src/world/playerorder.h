#ifndef WORLD_PLAYERORDER_H_INCLUDED
#define WORLD_PLAYERORDER_H_INCLUDED

#include <cstdint>
#include <string>

#include "../vectors.h"

namespace World
{

// What a player tells their faction's units to do, as the game's interface
// makes it, naming units by their ids and sectors by their indices: the same
// whether the player is at this machine or sends it from another. The world
// carries it out for the player (NC_STACK_ypaworld::ApplyPlayerOrder) once
// it has checked that what it names is there and the player's to order.
struct PlayerOrder
{
    enum Kind : uint8_t
    {
        NONE = 0,

        // To the player's host station, or to a squad: Action (DOACTION_*),
        // Proto the vehicle or building made, Unit the squad's commander,
        // Target the unit to attack or Sector the one to go to, build on or
        // beam to, at Point; Energy what the interface reckoned it costs
        COMMAND = 'C',

        // Of the squad Unit leads: Value, 0-100
        AGGRESSION = 'A',

        // Unit changes squads as TReorgOrder has it: Value the mode, Target
        // the other unit, 0 for none
        REORGANIZE = 'R',

        // The player gets in Unit: looks from it and controls it
        GET_IN = 'G',

        // Which of its batteries host station Unit fills: Value
        FILL_MODE = 'F',
    };

    Kind Type = NONE;
    uint8_t Action = 0;
    int32_t Proto = 0;
    uint32_t Unit = 0;
    uint32_t Target = 0;
    int32_t Sector = -1; // -1 for none
    vec3d Point;
    int32_t Value = 0;
    int32_t Energy = 0;

    // As bytes, Type first; false when they don't read as an order
    std::string Write() const;
    bool Read(const uint8_t *data, size_t size);
};

}

#endif
