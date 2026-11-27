#ifndef WORLD_PLAYER_H_INCLUDED
#define WORLD_PLAYER_H_INCLUDED

class NC_STACK_ypabact;

namespace World
{

// Someone playing a faction. The world has the player at this machine (its
// local player), whose view is what the screen shows.
struct Player
{
    NC_STACK_ypabact *HostStation = NULL; // Where their orders go
    NC_STACK_ypabact *Unit = NULL;        // Looked from: the camera and the HUD
};

}

#endif
