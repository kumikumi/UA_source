#ifndef WORLD_HISTORY_H_INCLUDED
#define WORLD_HISTORY_H_INCLUDED

#include <array>
#include <memory>
#include <string>
#include <vector>
#include "../types.h"
#include "consts.h"


class NC_STACK_ypaworld;

namespace World
{
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

// A level's history, played from its recording's view up to a time: the
// events found so far, in the order they happened. The debriefing plays it
// as it goes, and the score counts the whole of it.
class Replay
{
public:
    Replay();
    ~Replay();

    // Reads the level's recording, as the level ends, with what of the level
    // it needs, and rewinds. False, after logging why, when it can't.
    bool Open(NC_STACK_ypaworld *world, const std::string &path);
    void Close();
    bool IsOpen() const { return _player != nullptr; }

    // Back to the level's start, no events found
    void Rewind();

    // Plays on the steps before that game time, finding their events
    void PlayUntil(uint32_t time);

    // Whether it has played the whole recording
    bool AtEnd() const;

    // The game time of the last step played
    uint32_t Time() const { return _time; }

    const std::vector<Event> &Events() const { return _events; }

protected:
    struct Player;
    std::unique_ptr<Player> _player;
    std::vector<Event> _events;
    uint32_t _time = 0;
};

}
}

#endif
