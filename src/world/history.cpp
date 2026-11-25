#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "../yw.h"
#include "../log.h"
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

Event Event::Conquest(int32_t x, int32_t y, uint8_t owner)
{
    Event e;
    e.What = CONQUEST;
    e.SectorX = x;
    e.SectorY = y;
    e.Owner = owner;
    return e;
}

Event Event::PowerStation(int32_t x, int32_t y, uint8_t owner)
{
    Event e = Conquest(x, y, owner);
    e.What = POWER_STATION;
    return e;
}

Event Event::Creation(uint8_t owner, uint8_t mapX, uint8_t mapY)
{
    Event e;
    e.What = CREATION;
    e.Owner = owner;
    e.MapX = mapX;
    e.MapY = mapY;
    return e;
}

Event Event::Kill(uint8_t owner, uint8_t killer, bool byViewer, bool ofViewer, bool hostStation,
                  uint8_t mapX, uint8_t mapY)
{
    Event e = Creation(owner, mapX, mapY);
    e.What = KILL;
    e.Killer = killer;
    e.ByViewer = byViewer;
    e.OfViewer = ofViewer;
    e.HostStation = hostStation;
    return e;
}

Event Event::TechUpgrade(int32_t x, int32_t y, uint8_t owner, uint8_t type,
                         int32_t vehicle, int32_t weapon, int32_t building)
{
    Event e = Conquest(x, y, owner);
    e.What = TECH_UPGRADE;
    e.UpgradeType = type;
    e.UpgradeVehicle = vehicle;
    e.UpgradeWeapon = weapon;
    e.UpgradeBuilding = building;
    return e;
}

void AddScore(const Event &event, std::array<TPlayerStatus, CVFractionsCount> &stats)
{
    uint8_t owner = event.What == Event::KILL ? event.Killer : event.Owner;

    if ( owner >= stats.size() )
        return;

    TPlayerStatus &s = stats[owner];

    switch ( event.What )
    {
    case Event::CONQUEST:
        s.SectorsTaked++;
        s.Score++;
        break;

    case Event::POWER_STATION:
        s.Power++;
        s.Score += 100;
        break;

    case Event::TECH_UPGRADE:
        s.Upgrades++;
        s.Score += 500;
        break;

    case Event::KILL:
        s.DestroyedUnits++;

        // Most for the player's own kills, and those of one unit the
        // player was in of another
        if ( event.ByViewer )
        {
            s.DestroyedByUser++;
            s.Score += event.OfViewer ? 200 : 20;
        }
        else
        {
            s.Score += 10;
        }

        if ( event.HostStation )
            s.Score += 1000;
        break;

    case Event::CREATION:
        break;
    }
}

// What playing a recording needs
struct Replay::Player
{
    Recording Rec;
    std::unique_ptr<ViewReader> Reader; // Of Rec

    // Of the level
    int MapWidth = 0;
    vec2d WorldSize;
    std::vector<uint8_t> StartOwners; // Each sector's: the map's, the border nobody's
    std::vector<std::pair<Common::Point, uint8_t>> TechUpgrades; // Each one's sector and kind

    // Where the view has what the events are found from: the world's
    ViewField Time, Viewer, Upgrade, UpgradeTime, UpgradeVehicle, UpgradeWeapon, UpgradeBuilding;

    // A sector's
    ViewField SectorOwner, SectorPurpose;

    // A unit's, the same in every class: the view lists the base class's
    // fields first
    ViewField UnitOwner, UnitStatus, UnitFlags, UnitParent, UnitKiller, UnitPosition;

    // As the steps played left them
    std::vector<uint8_t> Owners, Purposes;             // Each sector's
    std::unordered_map<uint32_t, uint8_t> Statuses;    // Each unit's
    std::unordered_map<uint32_t, uint8_t> LastOwners;  // Each unit's, as last seen
    uint32_t LastUpgradeTime = 0;

    // The step read, and its events, until the replay comes to its time
    bool Pending = false;
    uint32_t PendingTime = 0;
    std::vector<Event> PendingEvents;
    bool End = false;

    void FindEvents();
};

static const uint8_t UnitKinds[] = {State::KIND_UNIT + BACT_TYPES_BACT, State::KIND_UNIT + BACT_TYPES_TANK,
                                    State::KIND_UNIT + BACT_TYPES_ROBO, State::KIND_UNIT + BACT_TYPES_FLYER,
                                    State::KIND_UNIT + BACT_TYPES_UFO, State::KIND_UNIT + BACT_TYPES_CAR,
                                    State::KIND_UNIT + BACT_TYPES_GUN, State::KIND_UNIT + BACT_TYPES_MISSLE};

Replay::Replay() {}
Replay::~Replay() {}

bool Replay::Open(NC_STACK_ypaworld *world, const std::string &path)
{
    Close();

    std::unique_ptr<Player> p(new Player);
    if ( !p->Rec.Open(path) )
        return false;

    const State::Schema &schema = p->Rec.ViewSchema();
    const uint8_t anyUnit = State::KIND_UNIT + BACT_TYPES_BACT;

    p->Time = FindField(schema, State::KIND_WORLD, "time");
    p->Viewer = FindField(schema, State::KIND_WORLD, "viewer");
    p->Upgrade = FindField(schema, State::KIND_WORLD, "upgrade");
    p->UpgradeTime = FindField(schema, State::KIND_WORLD, "upgradeTime");
    p->UpgradeVehicle = FindField(schema, State::KIND_WORLD, "upgradeVehicle");
    p->UpgradeWeapon = FindField(schema, State::KIND_WORLD, "upgradeWeapon");
    p->UpgradeBuilding = FindField(schema, State::KIND_WORLD, "upgradeBuilding");
    p->SectorOwner = FindField(schema, State::KIND_SECTOR, "owner");
    p->SectorPurpose = FindField(schema, State::KIND_SECTOR, "purpose");
    p->UnitOwner = FindField(schema, anyUnit, "owner");
    p->UnitStatus = FindField(schema, anyUnit, "status");
    p->UnitFlags = FindField(schema, anyUnit, "statusFlags");
    p->UnitParent = FindField(schema, anyUnit, "parent");
    p->UnitKiller = FindField(schema, anyUnit, "killer");
    p->UnitPosition = FindField(schema, anyUnit, "position");

    if ( !p->Time.Found || !p->SectorOwner.Found || !p->UnitStatus.Found || !p->UnitPosition.Found )
    {
        ypa_log_out("%s lacks what the level's history is found from\n", path.c_str());
        return false;
    }

    // The level, while it is still there
    p->MapWidth = world->_mapSize.x;
    p->WorldSize = World::SectorIDToPos2(world->_mapSize);
    p->StartOwners.assign(world->_cells.size(), 0);

    for (size_t i = 0; i < p->StartOwners.size() && i < world->_lvlPrimevalOwnMap.size(); i++)
    {
        if ( world->_cells.At(i).IsGamePlaySector() )
            p->StartOwners[i] = world->_lvlPrimevalOwnMap(i);
    }

    for ( const TMapGem &g : world->_techUpgrades )
        p->TechUpgrades.emplace_back(g.CellId, g.Type);

    p->Reader.reset(new ViewReader(p->Rec));
    _player = std::move(p);

    Rewind();
    return true;
}

void Replay::Close()
{
    _player.reset();
    _events.clear();
    _time = 0;
}

void Replay::Rewind()
{
    _events.clear();
    _time = 0;

    if ( !_player )
        return;

    Player &p = *_player;
    p.Pending = false;
    p.End = false;

    if ( !p.Reader->Rewind() )
    {
        ypa_log_out("Could not read the start of the level's recording\n");
        p.End = true;
        return;
    }

    // How the level started. Its sectors start with the owners of the
    // level's map, which the debriefing starts from too; those that
    // building the level gave to others were taken at once.
    p.Owners = p.StartOwners;
    p.Purposes.assign(p.Owners.size(), 0);
    p.Statuses.clear();
    p.LastOwners.clear();

    for ( const auto &it : p.Reader->View() )
    {
        uint8_t kind = it.first >> 32;
        uint32_t id = it.first & 0xFFFFFFFF;

        if ( kind >= State::KIND_UNIT )
        {
            p.Statuses[id] = Get<uint8_t>(it.second.Data, p.UnitStatus);
            p.LastOwners[id] = Get<uint8_t>(it.second.Data, p.UnitOwner);
        }
        else if ( kind == State::KIND_WORLD )
        {
            p.LastUpgradeTime = Get<uint32_t>(it.second.Data, p.UpgradeTime);
        }
    }

    p.FindEvents();
}

void Replay::PlayUntil(uint32_t time)
{
    if ( !_player )
        return;

    Player &p = *_player;

    while ( true )
    {
        if ( !p.Pending )
        {
            if ( p.End )
                return;

            if ( !p.Reader->Next() )
            {
                if ( !p.Reader->AtEnd() )
                    ypa_log_out("Could not read the level's recording on after step %d\n", p.Reader->Step());

                p.End = true;
                return;
            }

            p.FindEvents();
        }

        if ( p.PendingTime >= time )
            return;

        _events.insert(_events.end(), p.PendingEvents.begin(), p.PendingEvents.end());
        _time = p.PendingTime;
        p.Pending = false;
    }
}

bool Replay::AtEnd() const
{
    return !_player || (_player->End && !_player->Pending);
}

// The events of the reader's step, from what it changed, and its time
void Replay::Player::FindEvents()
{
    const ObjectsVersions &view = Reader->View();
    PendingEvents.clear();
    Pending = true;

    auto wit = view.find((uint64_t)State::KIND_WORLD << 32);
    if ( wit == view.end() )
        return;

    const std::string &worldData = wit->second.Data;
    uint32_t viewerId = Get<uint32_t>(worldData, Viewer);
    PendingTime = Get<uint32_t>(worldData, Time);

    for ( const auto &change : Reader->Changed() )
    {
        uint8_t kind = change.first >> 32;
        uint32_t id = change.first & 0xFFFFFFFF;

        if ( !change.second )
        {
            Statuses.erase(id);
            continue;
        }

        const std::string &data = change.second->Data;

        if ( kind == State::KIND_SECTOR )
        {
            // A sector taken: by its new owner, a power station's too
            if ( id >= Owners.size() )
                continue;

            uint8_t owner = Get<uint8_t>(data, SectorOwner);
            uint8_t purpose = Get<uint8_t>(data, SectorPurpose);

            if ( owner != Owners[id] )
            {
                int x = id % MapWidth, y = id / MapWidth;
                PendingEvents.push_back(Event::Conquest(x, y, owner));

                // A power station then, or lost with its owner in the step
                if ( purpose == cellArea::PT_POWERSTATION || Purposes[id] == cellArea::PT_POWERSTATION )
                    PendingEvents.push_back(Event::PowerStation(x, y, owner));

                Owners[id] = owner;
            }

            Purposes[id] = purpose;
        }
        else if ( kind >= State::KIND_UNIT )
        {
            uint8_t owner = Get<uint8_t>(data, UnitOwner);
            uint8_t status = Get<uint8_t>(data, UnitStatus);
            int32_t flags = Get<int32_t>(data, UnitFlags);
            float pos[3];
            memcpy(pos, data.data() + UnitPosition.Offset, sizeof(pos));
            uint8_t mapX = pos[0] * 256.0 / WorldSize.x;
            uint8_t mapY = pos[2] * 256.0 / WorldSize.y;

            auto sit = Statuses.find(id);

            if ( sit == Statuses.end() )
            {
                // A unit built by a host station: in a squad, not a
                // missile or a gun
                if ( kind != State::KIND_UNIT + BACT_TYPES_MISSLE && kind != State::KIND_UNIT + BACT_TYPES_GUN &&
                     Get<uint32_t>(data, UnitParent) && status != BACT_STATUS_DEAD )
                    PendingEvents.push_back(Event::Creation(owner, mapX, mapY));
            }
            else if ( sit->second != BACT_STATUS_DEAD && status == BACT_STATUS_DEAD )
            {
                // A unit killed, not taken away, by someone
                uint32_t killer = Get<uint32_t>(data, UnitKiller);

                if ( owner && killer && !(flags & BACT_STFLAG_CLEAN) )
                {
                    uint8_t killerOwner = 0;
                    uint32_t killerFlags = 0;
                    auto lit = LastOwners.find(killer);
                    if ( lit != LastOwners.end() )
                        killerOwner = lit->second;

                    // Units are under a key by their class; the killer's
                    // flags by whichever it has
                    for ( uint8_t k : UnitKinds )
                    {
                        auto kv = view.find(((uint64_t)k << 32) | killer);
                        if ( kv != view.end() )
                        {
                            killerOwner = Get<uint8_t>(kv->second.Data, UnitOwner);
                            killerFlags = Get<int32_t>(kv->second.Data, UnitFlags);
                            break;
                        }
                    }

                    PendingEvents.push_back(Event::Kill(owner, killerOwner,
                                                        killer == viewerId || (killerFlags & BACT_STFLAG_ISVIEW),
                                                        id == viewerId || (flags & BACT_STFLAG_ISVIEW),
                                                        kind == State::KIND_UNIT + BACT_TYPES_ROBO, mapX, mapY));
                }
            }

            Statuses[id] = status;
            LastOwners[id] = owner;
        }
    }

    // A tech upgrade, of the sector's new owner
    uint32_t upgraded = Get<uint32_t>(worldData, UpgradeTime);
    if ( upgraded != LastUpgradeTime )
    {
        int32_t gem = Get<int32_t>(worldData, Upgrade);

        if ( gem >= 0 && gem < (int32_t)TechUpgrades.size() )
        {
            const Common::Point &cell = TechUpgrades[gem].first;
            uint8_t owner = 0;
            auto sit = view.find(((uint64_t)State::KIND_SECTOR << 32) | (cell.y * MapWidth + cell.x));
            if ( sit != view.end() )
                owner = Get<uint8_t>(sit->second.Data, SectorOwner);

            PendingEvents.push_back(Event::TechUpgrade(cell.x, cell.y, owner, TechUpgrades[gem].second,
                                                       Get<int32_t>(worldData, UpgradeVehicle),
                                                       Get<int32_t>(worldData, UpgradeWeapon),
                                                       Get<int32_t>(worldData, UpgradeBuilding)));
        }

        LastUpgradeTime = upgraded;
    }

    for ( Event &e : PendingEvents )
        e.Time = PendingTime;
}

}
}
