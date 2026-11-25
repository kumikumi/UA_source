#ifndef WORLD_HISTORY_H_INCLUDED
#define WORLD_HISTORY_H_INCLUDED

#include <array>
#include <vector>
#include "../types.h"
#include "consts.h"


class NC_STACK_ypaworld;

namespace World
{
class Recording;

namespace History
{

// Something that happened in a level, as the debriefing shows it and the
// score counts it
struct Event
{
    enum Type : uint8_t
    {
        CONQUEST,      // Owner took the sector
        POWER_STATION, // Owner took the sector's power station
        CREATION,      // Owner made a unit
        KILL,          // A unit of Killer's killed one of Owner's
        TECH_UPGRADE,  // Owner took the sector's tech upgrade
    };

    Type What = CONQUEST;
    uint32_t Time = 0;           // Game time, in 1/1024 s
    uint8_t Owner = 0;
    int32_t SectorX = 0, SectorY = 0; // CONQUEST, POWER_STATION, TECH_UPGRADE
    uint8_t MapX = 0, MapY = 0;  // CREATION, KILL: where, in 256ths of the map

    // KILL
    uint8_t Killer = 0;          // The killer's owner
    bool ByViewer = false;       // The killer was the unit the player was in
    bool OfViewer = false;       // So was the unit killed
    bool HostStation = false;    // The unit killed was a host station

    // TECH_UPGRADE: of which kind, and what it upgraded
    uint8_t UpgradeType = 0;
    int32_t UpgradeVehicle = 0, UpgradeWeapon = 0, UpgradeBuilding = 0;

    static Event Conquest(int32_t x, int32_t y, uint8_t owner);
    static Event PowerStation(int32_t x, int32_t y, uint8_t owner);
    static Event Creation(uint8_t owner, uint8_t mapX, uint8_t mapY);
    static Event Kill(uint8_t owner, uint8_t killer, bool byViewer, bool ofViewer, bool hostStation,
                      uint8_t mapX, uint8_t mapY);
    static Event TechUpgrade(int32_t x, int32_t y, uint8_t owner, uint8_t type,
                             int32_t vehicle, int32_t weapon, int32_t building);
};

// What it scores: a kill for the killer's owner, anything else for its owner
void AddScore(const Event &event, std::array<TPlayerStatus, CVFractionsCount> &stats);

// The events of a level, worked out from its recording's view, in the
// order they happened. False when the recording's view lacks what they're
// worked out from.
bool Derive(NC_STACK_ypaworld *world, const Recording &recording, std::vector<Event> &out);

}
}

#endif
