#ifndef WORLD_PLAYER_H_INCLUDED
#define WORLD_PLAYER_H_INCLUDED

#include <cstdint>
#include "../ypabact.h"

namespace World
{

// Someone playing a faction. The world has the player at this machine (its
// local player), whose view is what the screen shows.
//
// Any number of players can play a faction together; one no one plays is
// played by its AI.
//
// A player looks from one unit, which any number of players can look from,
// and controls one, which only one player can control at a time. Usually
// they're the same unit; the camera can ride a missile while the unit that
// fired it is still controlled.
struct Player
{
    uint8_t Owner = 0;                    // The faction played, 0 for none
    NC_STACK_ypabact *HostStation = NULL; // Where their orders go
    NC_STACK_ypabact *Unit = NULL;        // Looked from: the camera and the HUD
    NC_STACK_ypabact *Controls = NULL;    // Moved by their controls
    TInputState *Input = NULL;            // Their controls this step
    TUserOrder Order;                     // To their host station, this step
};

}

#endif
