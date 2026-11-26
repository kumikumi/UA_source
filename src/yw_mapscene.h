#ifndef YW_MAPSCENE_H_INCLUDED
#define YW_MAPSCENE_H_INCLUDED

#include <cstdint>
#include <functional>

#include "common/common.h"
#include "vectors.h"
#include "matrix.h"

class NC_STACK_ypabact;

// What the game's map, its radar and the marks over units show of the
// level: as the game has it now (the world's own scene, in yw_game_ui.cpp),
// or as a replay's frame does. They read the level only through it, so that
// a replay is shown with the game's own map.
class MapScene
{
public:
    struct Unit
    {
        uint32_t Id = 0;
        uint8_t Type = 0;          // BACT_TYPES_*
        uint8_t Owner = 0;
        uint8_t Status = 0;        // BACT_STATUS_*
        int32_t StatusFlags = 0;   // BACT_STFLAG_*
        int Vehicle = 0;
        vec3d Position;
        Common::Point Sector;      // It's in
        mat3x3 Rotation = mat3x3::Ident();     // Which way it heads
        mat3x3 LookRotation = mat3x3::Ident(); // Which way a host station looks
        int Energy = 0;
        int EnergyMax = 1;
        bool Hidden = false;       // From the player
        bool RoboGun = false;      // One of a host station's guns
        bool Commander = false;    // Of a squad of its host station's

        // Who it follows: its commander, or a commander's host station
        bool HasParent = false;
        uint32_t Parent = 0;
        vec3d ParentPos;

        // Where its squad goes, and what it fights on the way:
        // BACT_TGT_TYPE_*, and where that is, in a sector's case in which
        uint8_t PrimaryType = 0;
        vec3d PrimaryPos;
        Common::Point PrimarySector;
        uint8_t SecondaryType = 0;
        vec3d SecondaryPos;

        // A commander's waypoints, the game's own
        const vec3d *Waypoints = NULL;
        int WaypointCount = 0;
        int CurrentWaypoint = 0;

        NC_STACK_ypabact *Bact = NULL; // The game's own; none in a replay
    };

    typedef std::function<void(const Unit &)> UnitFunc;

    virtual ~MapScene() = default;

    // A sector's, by its column and row: the owners that see it
    // (cellArea::ViewMask), its owner, type, simple type
    // (TSectorDesc::SectorType), its buildings' health (x + 3 * y) and
    // purpose (cellArea::PT_*)
    virtual int32_t ViewMask(const Common::Point &sector) const = 0;
    virtual uint8_t Owner(const Common::Point &sector) const = 0;
    virtual int32_t Type(const Common::Point &sector) const = 0;
    virtual uint8_t SimpleType(const Common::Point &sector) const = 0;
    virtual const int16_t *Health(const Common::Point &sector) const = 0;
    virtual uint8_t Purpose(const Common::Point &sector) const = 0;

    bool CanSee(const Common::Point &sector, uint8_t owner) const;

    // The units in a sector; the units that follow a unit; and the one
    // leading the squad a unit is in, false for a host station
    virtual void ForEachUnitIn(const Common::Point &sector, const UnitFunc &func) const = 0;
    virtual void ForEachKid(const Unit &unit, const UnitFunc &func) const = 0;
    virtual bool SquadLeader(const Unit &unit, Unit &leader) const = 0;

    // The player: whose sight the map shows, its host station and the unit
    // it steers (their ids 0 for none), and the unit it looks from, false
    // for none, and how the view is turned
    virtual uint8_t PlayerOwner() const = 0;
    virtual uint32_t UserRoboId() const = 0;
    virtual uint32_t UserUnitId() const = 0;
    virtual bool UserRobo(Unit &out) const = 0;
    virtual bool UserUnit(Unit &out) const = 0;
    virtual bool ViewerPosition(vec3d &out) const = 0;
    virtual mat3x3 ViewerRotation() const = 0;

    // The unit under the pointer, false for none: shown as the game shows
    // the unit an order would go to
    virtual bool Hovered(Unit &out) const = 0;

    // The player's squads' commanders in the squad manager's order, and the
    // one selected
    virtual void ForEachCommander(const UnitFunc &func) const = 0;
    virtual bool SelectedCommander(Unit &out) const = 0;

    // Power stations: their sectors and effective power
    virtual void ForEachPowerStation(const std::function<void(const Common::Point &, int)> &func) const = 0;

    // A bomb's TMapSuperItem::STATE_*, by its index in the level
    virtual int SuperItemState(size_t index) const = 0;

    // Whether the player plays (the game), or only watches (a replay): only
    // then the map shows what the player is about to order and who last
    // reported, and what the gates can beam
    virtual bool Interactive() const = 0;
};

#endif
