#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "../yw.h"
#include "history.h"
#include "recorder.h"

namespace World
{
namespace History
{

// Where a field of a kind's view starts, and how big it is; the view's
// fields all have a fixed size
struct ViewField
{
    size_t Offset = 0, Size = 0;
    bool Found = false;
};

static ViewField FindField(const State::Schema &schema, uint8_t kind, const char *name)
{
    ViewField out;
    auto it = schema.find(kind);
    if ( it == schema.end() )
        return out;

    size_t offset = 0;
    for ( const State::FieldInfo &f : it->second )
    {
        size_t size = State::ElemSize(f.ElemType) * f.Count;

        if ( f.Name == name && !f.List )
        {
            out.Offset = offset;
            out.Size = size;
            out.Found = true;
            return out;
        }

        offset += size;
    }

    return out;
}

template <class T>
static T Get(const std::string &data, const ViewField &f)
{
    T v = T();
    if ( f.Found && f.Offset + sizeof(T) <= data.size() )
        memcpy(&v, data.data() + f.Offset, sizeof(T));
    return v;
}

bool Derive(NC_STACK_ypaworld *world, const Recording &recording, Common::BlocksStream &out)
{
    const State::Schema &schema = recording.ViewSchema();
    const uint8_t unitKinds[] = {State::KIND_UNIT + BACT_TYPES_BACT, State::KIND_UNIT + BACT_TYPES_TANK,
                                 State::KIND_UNIT + BACT_TYPES_ROBO, State::KIND_UNIT + BACT_TYPES_FLYER,
                                 State::KIND_UNIT + BACT_TYPES_UFO, State::KIND_UNIT + BACT_TYPES_CAR,
                                 State::KIND_UNIT + BACT_TYPES_GUN, State::KIND_UNIT + BACT_TYPES_MISSLE};

    // The world's
    ViewField time = FindField(schema, State::KIND_WORLD, "time");
    ViewField viewer = FindField(schema, State::KIND_WORLD, "viewer");
    ViewField upgrade = FindField(schema, State::KIND_WORLD, "upgrade");
    ViewField upgradeTime = FindField(schema, State::KIND_WORLD, "upgradeTime");
    ViewField upgradeVehicle = FindField(schema, State::KIND_WORLD, "upgradeVehicle");
    ViewField upgradeWeapon = FindField(schema, State::KIND_WORLD, "upgradeWeapon");
    ViewField upgradeBuilding = FindField(schema, State::KIND_WORLD, "upgradeBuilding");

    // A sector's
    ViewField sectorOwner = FindField(schema, State::KIND_SECTOR, "owner");
    ViewField sectorPurpose = FindField(schema, State::KIND_SECTOR, "purpose");

    // A unit's, the same in every class: the view lists the base class's
    // fields first
    const uint8_t anyUnit = State::KIND_UNIT + BACT_TYPES_BACT;
    ViewField unitVehicle = FindField(schema, anyUnit, "vehicle");
    ViewField unitOwner = FindField(schema, anyUnit, "owner");
    ViewField unitStatus = FindField(schema, anyUnit, "status");
    ViewField unitFlags = FindField(schema, anyUnit, "statusFlags");
    ViewField unitParent = FindField(schema, anyUnit, "parent");
    ViewField unitKiller = FindField(schema, anyUnit, "killer");
    ViewField unitPosition = FindField(schema, anyUnit, "position");

    if ( !time.Found || !sectorOwner.Found || !unitStatus.Found || !unitPosition.Found )
        return false;

    const int mapWidth = world->_mapSize.x;
    const vec2d worldSize = World::SectorIDToPos2(world->_mapSize);

    std::vector<uint8_t> owners;      // Each sector's, the step before
    std::vector<uint8_t> purposes;
    std::unordered_map<uint32_t, uint8_t> statuses; // Each unit's, the step before
    std::unordered_map<uint32_t, uint8_t> lastOwners; // Each unit's, as last seen
    uint32_t lastUpgradeTime = 0;
    bool started = false;

    out.Clear();

    bool ok = recording.ForEachView([&](uint32_t step, const ObjectsVersions &view, const Recording::ChangedObjects &changed)
    {
        auto wit = view.find((uint64_t)State::KIND_WORLD << 32);
        if ( wit == view.end() )
            return;

        const std::string &worldData = wit->second.Data;
        uint32_t viewerId = Get<uint32_t>(worldData, viewer);
        std::vector<Common::ByteArray> events;

        if ( !started )
        {
            // How the level started. Its sectors start with the owners of
            // the level's map, which the debriefing starts from too; those
            // that building the level gave to others were taken at once.
            // The border is nobody's from the start.
            owners.assign(world->_cells.size(), 0);
            purposes.assign(world->_cells.size(), 0);

            for (size_t i = 0; i < owners.size() && i < world->_lvlPrimevalOwnMap.size(); i++)
            {
                if ( world->_cells.At(i).IsGamePlaySector() )
                    owners[i] = world->_lvlPrimevalOwnMap(i);
            }

            for ( const auto &it : view )
            {
                uint8_t kind = it.first >> 32;
                uint32_t id = it.first & 0xFFFFFFFF;

                if ( kind >= State::KIND_UNIT )
                {
                    statuses[id] = Get<uint8_t>(it.second.Data, unitStatus);
                    lastOwners[id] = Get<uint8_t>(it.second.Data, unitOwner);
                }
            }

            lastUpgradeTime = Get<uint32_t>(worldData, upgradeTime);
            started = true;
        }

        for ( const auto &change : changed )
        {
            uint8_t kind = change.first >> 32;
            uint32_t id = change.first & 0xFFFFFFFF;

            if ( !change.second )
            {
                statuses.erase(id);
                continue;
            }

            const std::string &data = change.second->Data;

            if ( kind == State::KIND_SECTOR )
            {
                // A sector taken: by its new owner, a power station's too
                if ( id >= owners.size() )
                    continue;

                uint8_t owner = Get<uint8_t>(data, sectorOwner);
                uint8_t purpose = Get<uint8_t>(data, sectorPurpose);

                if ( owner != owners[id] )
                {
                    int x = id % mapWidth, y = id / mapWidth;
                    events.push_back(Conq(x, y, owner).MakeByteArray());

                    // A power station then, or lost with its owner in the step
                    if ( purpose == cellArea::PT_POWERSTATION || purposes[id] == cellArea::PT_POWERSTATION )
                        events.push_back(PowerST(x, y, owner).MakeByteArray());

                    owners[id] = owner;
                }

                purposes[id] = purpose;
            }
            else if ( kind >= State::KIND_UNIT )
            {
                uint8_t owner = Get<uint8_t>(data, unitOwner);
                uint8_t status = Get<uint8_t>(data, unitStatus);
                int32_t flags = Get<int32_t>(data, unitFlags);
                float pos[3];
                memcpy(pos, data.data() + unitPosition.Offset, sizeof(pos));
                uint8_t mapX = pos[0] * 256.0 / worldSize.x;
                uint8_t mapY = pos[2] * 256.0 / worldSize.y;

                auto sit = statuses.find(id);

                if ( sit == statuses.end() )
                {
                    // A unit built by a host station: in a squad, not a
                    // missile or a gun
                    if ( kind != State::KIND_UNIT + BACT_TYPES_MISSLE && kind != State::KIND_UNIT + BACT_TYPES_GUN &&
                         Get<uint32_t>(data, unitParent) && status != BACT_STATUS_DEAD )
                        events.push_back(VhclCreate(owner, Get<uint8_t>(data, unitVehicle), mapX, mapY).MakeByteArray());
                }
                else if ( sit->second != BACT_STATUS_DEAD && status == BACT_STATUS_DEAD )
                {
                    // A unit killed, not taken away, by someone
                    uint32_t killer = Get<uint32_t>(data, unitKiller);

                    if ( owner && killer && !(flags & BACT_STFLAG_CLEAN) )
                    {
                        uint8_t killerOwner = 0;
                        uint32_t killerFlags = 0;
                        auto lit = lastOwners.find(killer);
                        if ( lit != lastOwners.end() )
                            killerOwner = lit->second;

                        // Units are under a key by their class; the killer's
                        // flags by whichever it has
                        for ( uint8_t k : unitKinds )
                        {
                            auto kv = view.find(((uint64_t)k << 32) | killer);
                            if ( kv != view.end() )
                            {
                                killerOwner = Get<uint8_t>(kv->second.Data, unitOwner);
                                killerFlags = Get<int32_t>(kv->second.Data, unitFlags);
                                break;
                            }
                        }

                        uint8_t ownerBits = (killerOwner << 3) | owner;

                        if ( killer == viewerId || (killerFlags & BACT_STFLAG_ISVIEW) )
                            ownerBits |= 0x80;

                        if ( id == viewerId || (flags & BACT_STFLAG_ISVIEW) )
                            ownerBits |= 0x40;

                        uint16_t vp = Get<uint8_t>(data, unitVehicle);
                        if ( kind == State::KIND_UNIT + BACT_TYPES_ROBO )
                            vp |= 0x8000;

                        events.push_back(VhclKill(ownerBits, vp, mapX, mapY).MakeByteArray());
                    }
                }

                statuses[id] = status;
                lastOwners[id] = owner;
            }
        }

        // A tech upgrade, of the sector's new owner
        uint32_t upgraded = Get<uint32_t>(worldData, upgradeTime);
        if ( upgraded != lastUpgradeTime )
        {
            int32_t gem = Get<int32_t>(worldData, upgrade);

            if ( gem >= 0 && gem < (int32_t)world->_techUpgrades.size() )
            {
                const TMapGem &g = world->_techUpgrades[gem];
                uint8_t owner = 0;
                auto sit = view.find(((uint64_t)State::KIND_SECTOR << 32) | (g.CellId.y * mapWidth + g.CellId.x));
                if ( sit != view.end() )
                    owner = Get<uint8_t>(sit->second.Data, sectorOwner);

                events.push_back(Upgrade(g.CellId.x, g.CellId.y, owner, g.Type, Get<int32_t>(worldData, upgradeVehicle),
                                         Get<int32_t>(worldData, upgradeWeapon), Get<int32_t>(worldData, upgradeBuilding)).MakeByteArray());
            }

            lastUpgradeTime = upgraded;
        }

        if ( !events.empty() )
        {
            out.Write(Frame(Get<int32_t>(worldData, time)).MakeByteArray());

            for ( const Common::ByteArray &e : events )
                out.Write(e);
        }
    });

    return ok;
}

}
}
