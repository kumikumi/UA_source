#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <unordered_set>
#include <zlib.h>

#include "state.h"
#include "../yw.h"
#include "../yparobo.h"
#include "../ypamissile.h"
#include "../log.h"
#include "fmtlib/printf.h"

namespace World
{
namespace State
{

// Visitor

void Visitor::Info(const char *name, Type type, uint16_t count)
{
    FieldInfo info;
    info.Name = name;
    info.ElemType = type;
    info.Count = count;
    _fields.push_back(info);
}

void Visitor::Put(const void *data, size_t size)
{
    _out.append((const char *)data, size);
}

bool Visitor::Take(void *data, size_t size)
{
    if ( _failed || !_in || _inPos + size > _in->size() )
    {
        _failed = true;
        memset(data, 0, size);
        return false;
    }

    memcpy(data, _in->data() + _inPos, size);
    _inPos += size;
    return true;
}

void Visitor::Raw(const char *name, Type type, uint16_t count, void *data, size_t size)
{
    switch ( _mode )
    {
    case WRITE:
        Put(data, size);
        break;

    case READ:
        Take(data, size);
        break;

    case SCHEMA:
        Info(name, type, count);
        break;
    }
}

void Visitor::Field(const char *name, bool &v, Role role)
{
    if ( !Includes(role) )
        return;

    uint8_t b = v ? 1 : 0;
    Raw(name, T_BOOL, 1, &b, 1);

    if ( Reading() )
        v = b != 0;
}

void Visitor::Field(const char *name, int8_t &v, Role role)   { if ( Includes(role) ) Raw(name, T_I8, 1, &v, 1); }
void Visitor::Field(const char *name, uint8_t &v, Role role)  { if ( Includes(role) ) Raw(name, T_U8, 1, &v, 1); }
void Visitor::Field(const char *name, int16_t &v, Role role)  { if ( Includes(role) ) Raw(name, T_I16, 1, &v, 2); }
void Visitor::Field(const char *name, uint16_t &v, Role role) { if ( Includes(role) ) Raw(name, T_U16, 1, &v, 2); }
void Visitor::Field(const char *name, int32_t &v, Role role)  { if ( Includes(role) ) Raw(name, T_I32, 1, &v, 4); }
void Visitor::Field(const char *name, uint32_t &v, Role role) { if ( Includes(role) ) Raw(name, T_U32, 1, &v, 4); }
void Visitor::Field(const char *name, float &v, Role role)    { if ( Includes(role) ) Raw(name, T_F32, 1, &v, 4); }

// Doubles the view rounds to floats, as they're drawn. Reading the view
// skips those the internal part has exact.
void Visitor::Doubles(const char *name, double *v, size_t count, Role role)
{
    if ( !Includes(role) )
        return;

    if ( !Rounds(role) )
    {
        Raw(name, T_F64, count, v, count * 8);
        return;
    }

    float f[16];
    for (size_t i = 0; i < count; i++)
        f[i] = v[i];

    Raw(name, T_F32, count, f, count * 4);

    if ( Reading() && role == VIEW )
    {
        for (size_t i = 0; i < count; i++)
            v[i] = f[i];
    }
}

void Visitor::Field(const char *name, double &v, Role role) { Doubles(name, &v, 1, role); }
void Visitor::Field(const char *name, vec2d &v, Role role)  { Doubles(name, &v.x, 2, role); }

void Visitor::Field(const char *name, vec3d &v, Role role)
{
    static_assert(sizeof(vec3d) == 24, "vec3d is three doubles");
    Doubles(name, &v.x, 3, role);
}

void Visitor::Field(const char *name, mat3x3 &v, Role role)
{
    static_assert(sizeof(mat3x3) == 72, "mat3x3 is nine doubles");
    Doubles(name, &v.m00, 9, role);
}

void Visitor::Field(const char *name, Common::Point &v, Role role)
{
    if ( !Includes(role) )
        return;

    int32_t xy[2] = {v.x, v.y};
    Raw(name, T_I32, 2, xy, 8);

    if ( Reading() )
    {
        v.x = xy[0];
        v.y = xy[1];
    }
}

void Visitor::Field(const char *name, vec3d *v, size_t count, Role role)
{
    if ( Includes(role) )
        Raw(name, T_F64, count * 3, v, count * 24);
}

void Visitor::Field(const char *name, int32_t *v, size_t count, Role role)
{
    if ( Includes(role) )
        Raw(name, T_I32, count, v, count * 4);
}

void Visitor::Field(const char *name, int16_t *v, size_t count, Role role)
{
    if ( Includes(role) )
        Raw(name, T_I16, count, v, count * 2);
}

void Visitor::Field(const char *name, uint32_t *v, size_t count, Role role)
{
    if ( Includes(role) )
        Raw(name, T_U32, count, v, count * 4);
}

void Visitor::Field(const char *name, std::vector<uint8_t> &v, Role role)
{
    if ( !Includes(role) )
        return;

    uint16_t n = ListLength(name, T_U8, 1, v.size());

    if ( _mode == WRITE )
        Put(v.data(), n);
    else if ( _mode == READ )
    {
        v.resize(n);
        Take(v.data(), n);
    }
}

void Visitor::Field(const char *name, std::vector<int32_t> &v, Role role)
{
    if ( !Includes(role) )
        return;

    uint16_t n = ListLength(name, T_I32, 1, v.size());

    if ( _mode == WRITE )
        Put(v.data(), n * 4);
    else if ( _mode == READ )
    {
        v.resize(n);
        Take(v.data(), n * 4);
    }
}

void Visitor::Field(const char *name, std::vector<vec3d> &v, Role role)
{
    if ( !Includes(role) )
        return;

    uint16_t n = ListLength(name, T_F64, 3, v.size());

    if ( _mode == WRITE )
        Put(v.data(), n * sizeof(vec3d));
    else if ( _mode == READ )
    {
        v.resize(n);
        Take(v.data(), n * sizeof(vec3d));
    }
}

void Visitor::Units(const char *name, std::vector<NC_STACK_ypabact *> &v, Role role)
{
    if ( !Includes(role) )
        return;

    uint16_t n = ListLength(name, T_UNIT, 1, v.size());

    if ( _mode == WRITE )
    {
        for ( NC_STACK_ypabact *unit : v )
        {
            uint32_t id = UnitId(unit);
            Put(&id, 4);
        }
    }
    else if ( _mode == READ )
    {
        v.resize(n);

        for (uint16_t i = 0; i < n; i++)
        {
            uint32_t id = 0;
            Take(&id, 4);
            v[i] = UnitById(id);
        }
    }
}

uint32_t Visitor::UnitId(const NC_STACK_ypabact *unit) const
{
    return unit ? unit->_gid : 0;
}

NC_STACK_ypabact *Visitor::UnitById(uint32_t id) const
{
    if ( !id || !_units )
        return NULL;

    auto it = _units->find(id);
    if ( it == _units->end() )
        return NULL;

    return it->second;
}

void Visitor::Unit(const char *name, NC_STACK_ypabact *&unit, Role role)
{
    if ( !Includes(role) )
        return;

    uint32_t id = UnitId(unit);
    Raw(name, T_UNIT, 1, &id, 4);

    if ( Reading() )
        unit = UnitById(id);
}

void Visitor::Sector(const char *name, cellArea *&cell, Role role)
{
    if ( !Includes(role) )
        return;

    int32_t index = cell ? cell->CellId.y * _world->_mapSize.x + cell->CellId.x : -1;
    Raw(name, T_SECTOR, 1, &index, 4);

    if ( Reading() )
    {
        if ( index >= 0 && index < (int32_t)_world->_cells.size() )
            cell = &_world->_cells.At(index);
        else
            cell = NULL;
    }
}

void Visitor::Target(const char *name, uint8_t type, BactTarget &target, Role role)
{
    if ( !Includes(role) )
        return;

    // A unit's id or a sector's index, by the type
    if ( _mode == SCHEMA )
    {
        Info(name, T_U32, 1);
        return;
    }

    switch ( type )
    {
    case BACT_TGT_TYPE_UNIT:
        Unit(name, target.pbact, role);
        break;

    case BACT_TGT_TYPE_CELL:
    {
        // Written as a unit's id would be, to keep one field for both
        int32_t index = -1;
        if ( !Reading() && target.pcell )
            index = target.pcell->CellId.y * _world->_mapSize.x + target.pcell->CellId.x;

        Raw(name, T_U32, 1, &index, 4);

        if ( Reading() )
            target.pcell = (index >= 0 && index < (int32_t)_world->_cells.size()) ? &_world->_cells.At(index) : NULL;
        break;
    }

    default:
    {
        uint32_t none = 0;
        Raw(name, T_U32, 1, &none, 4);

        if ( Reading() )
            target.pbact = NULL;
        break;
    }
    }
}

uint16_t Visitor::ListLength(const char *name, Type type, uint16_t count, size_t length)
{
    uint16_t n = length;

    switch ( _mode )
    {
    case WRITE:
        Put(&n, 2);
        break;

    case READ:
        Take(&n, 2);
        break;

    case SCHEMA:
        Info(name, type, count);
        _fields.back().List = true;
        break;
    }

    return n;
}

void Visitor::Kids(const char *name, World::RefBactList &list, Role role)
{
    if ( !Includes(role) )
        return;

    uint16_t n = ListLength(name, T_UNIT, 1, list.size());

    if ( _mode == WRITE )
    {
        for ( NC_STACK_ypabact *kid : list )
        {
            uint32_t id = kid->_gid;
            Put(&id, 4);
        }
    }
    else if ( _mode == READ )
    {
        for (uint16_t i = 0; i < n; i++)
        {
            uint32_t id = 0;
            Take(&id, 4);

            NC_STACK_ypabact *kid = UnitById(id);
            if ( kid )
                kid->_kidRef = list.push_back(kid);
        }
    }
}

void Visitor::SectorUnits(const char *name, cellArea &cell, Role role)
{
    if ( !Includes(role) )
        return;

    // Units waiting in the dead cache for another life are parked in a
    // sector too, out of the way; they have no state
    std::vector<uint32_t> ids;

    if ( _mode == WRITE )
    {
        for ( NC_STACK_ypabact *unit : cell.unitsList )
        {
            if ( !unit->_kidRef.IsListType(World::BLIST_CACHE) )
                ids.push_back(unit->_gid);
        }
    }

    uint16_t n = ListLength(name, T_UNIT, 1, ids.size());

    if ( _mode == WRITE )
    {
        Put(ids.data(), ids.size() * 4);
    }
    else if ( _mode == READ )
    {
        for (uint16_t i = 0; i < n; i++)
        {
            uint32_t id = 0;
            Take(&id, 4);

            NC_STACK_ypabact *unit = UnitById(id);
            if ( unit )
            {
                unit->_cellRef = cell.unitsList.push_back(unit);
                unit->_pSector = &cell;
                unit->_cellId = cell.CellId;
            }
        }
    }
}

void Visitor::Missiles(const char *name, World::MissileList &list, Role role)
{
    if ( !Includes(role) )
        return;

    uint16_t n = ListLength(name, T_UNIT, 1, list.size());

    if ( _mode == WRITE )
    {
        for ( NC_STACK_ypamissile *m : list )
        {
            uint32_t id = m->_gid;
            Put(&id, 4);
        }
    }
    else if ( _mode == READ )
    {
        list.clear();

        for (uint16_t i = 0; i < n; i++)
        {
            uint32_t id = 0;
            Take(&id, 4);

            NC_STACK_ypamissile *m = dynamic_cast<NC_STACK_ypamissile *>(UnitById(id));
            if ( m )
                list.push_back(m);
        }
    }
}

void Visitor::Attackers(const char *typesName, const char *name, std::list<TBactAttacker> &list, Role role)
{
    if ( !Includes(role) )
        return;

    std::vector<uint8_t> types;
    std::vector<NC_STACK_ypabact *> units;

    for ( const TBactAttacker &a : list )
    {
        types.push_back(a.type);
        units.push_back(a.attacker);
    }

    Field(typesName, types, role);
    Units(name, units, role);

    if ( Reading() )
    {
        list.clear();

        for (size_t i = 0; i < types.size() && i < units.size(); i++)
            list.emplace_back(types[i], units[i]);
    }
}


// Sectors

static void VisitSector(Visitor &v, NC_STACK_ypaworld *world, cellArea &cell, EnergyAccum &accum)
{
    v.Field("owner", cell.owner, VIEW);
    v.Field("type", cell.type_id, VIEW);
    v.Field("simple", cell.SectorType, VIEW);
    v.Field("health", cell.buildings_health.data(), 9, VIEW);
    v.Field("purpose", cell.PurposeType, VIEW);

    // A building being built on it, drawn growing
    NC_STACK_ypaworld::TConstructInfo construct;
    auto cIt = world->_inBuildProcess.find(cell.Id);
    bool constructing = cIt != world->_inBuildProcess.end();
    if ( constructing )
        construct = cIt->second;

    v.Field("constructing", constructing, VIEW);
    v.Field("constructBuilding", construct.BuildID, VIEW);
    v.Field("constructTime", construct.Time, VIEW);
    v.Field("constructEnd", construct.EndTime, VIEW);
    v.Field("constructOwner", construct.Owner);

    // Reading only the internal part leaves the view's as they are
    if ( v.Reads(VIEW) )
    {
        if ( constructing )
        {
            construct.CellID = cell.CellId;
            world->_inBuildProcess[cell.Id] = construct;
        }
        else
        {
            world->_inBuildProcess.erase(cell.Id);
        }
    }
    else if ( v.Reads(INTERNAL) )
    {
        cIt = world->_inBuildProcess.find(cell.Id);
        if ( cIt != world->_inBuildProcess.end() )
            cIt->second.Owner = construct.Owner;
    }

    if ( !v.Visits(INTERNAL) )
        return;

    v.Field("energyPower", cell.energy_power);
    v.Field("purposeIndex", cell.PurposeIndex);
    v.Field("accumOwner", accum.Owner);
    v.Field("accumEnergy", accum.Energy);

    // A power station on it, of the world's
    TPowerStationInfo ps;
    auto psIt = world->_powerStations.find(cell.Id);
    bool powerStation = psIt != world->_powerStations.end();
    if ( powerStation )
        ps = psIt->second;

    v.Field("powerStation", powerStation);
    v.Field("power", ps.Power);
    v.Field("effectivePower", ps.EffectivePower);

    if ( v.Reads(INTERNAL) )
    {
        if ( powerStation )
        {
            ps.CellId = cell.CellId;
            ps.pCell = &cell;
            world->_powerStations[cell.Id] = ps;
        }
        else
        {
            world->_powerStations.erase(cell.Id);
        }
    }

    v.SectorUnits("units", cell);
}


// Bombs and waves

static void VisitSuperItem(Visitor &v, TMapSuperItem &item)
{
    v.Field("state", item.State, VIEW);
    v.Field("activeTime", item.ActiveTime);
    v.Field("triggerTime", item.TriggerTime);
    v.Field("activateOwner", item.ActivateOwner);
    v.Field("countDown", item.CountDown, VIEW);
    v.Field("lastTenSeconds", item.LastTenSec);
    v.Field("lastSecond", item.LastSec);
    v.Field("radius", item.CurrentRadius, VIEW);
    v.Field("lastRadius", item.LastRadius);
}


// The campaign: the levels' statuses, the players' statistics, the energy
// the player's host station takes into the next level, and the contacts made

static void VisitCampaign(Visitor &v, NC_STACK_ypaworld *world)
{
    std::array<int32_t, 256> statuses;
    for (size_t i = 0; i < statuses.size(); i++)
        statuses[i] = world->_globalMapRegions.MapRegions[i].Status;

    v.Field("levelStatuses", statuses.data(), statuses.size());

    if ( v.Reads(INTERNAL) )
    {
        for (size_t i = 0; i < statuses.size(); i++)
            world->_globalMapRegions.MapRegions[i].Status = statuses[i];
    }

    static_assert(sizeof(TPlayerStatus) == 28, "seven ints");
    v.Field("playerStats", &world->_playersStats[0].DestroyedUnits, world->_playersStats.size() * 7);
    v.Field("maxRoboEnergy", world->_maxRoboEnergy);
    v.Field("maxReloadConst", world->_maxReloadConst);
    v.Field("contacts", world->_levelInfo.JodieFoster.data(), world->_levelInfo.JodieFoster.size());
}


// Prototypes: their values, not their names, sounds or effects, which come
// with the level's scripts

static void VisitVehicle(Visitor &v, World::TVhclProto &p)
{
    v.Field("model", p.model_id);
    v.Field("enable", p.disable_enable_bitmask);
    v.Field("weapon", p.weapon);
    v.Field("mgun", p.mgun);
    v.Field("icon", p.type_icon);
    v.Field("vpNormal", p.vp_normal);
    v.Field("vpFire", p.vp_fire);
    v.Field("vpDead", p.vp_dead);
    v.Field("vpWait", p.vp_wait);
    v.Field("vpMegadeth", p.vp_megadeth);
    v.Field("vpGenesis", p.vp_genesis);
    v.Field("voice", p.vo_type);
    v.Field("maxPitch", p.max_pitch);
    v.Field("maxSpeedShown", p.max_speed_disp);
    v.Field("maxSpeed", p.max_speed);
    v.Field("shield", p.shield);
    v.Field("energy", p.energy);
    v.Field("adistSector", p.adist_sector);
    v.Field("adistUnit", p.adist_bact);
    v.Field("sdistSector", p.sdist_sector);
    v.Field("sdistUnit", p.sdist_bact);
    v.Field("radar", p.radar);
    v.Field("mass", p.mass);
    v.Field("force", p.force);
    v.Field("airconst", p.airconst);
    v.Field("maxrot", p.maxrot);
    v.Field("height", p.height);
    v.Field("radius", p.radius);
    v.Field("overeof", p.overeof);
    v.Field("viewerRadius", p.vwr_radius);
    v.Field("viewerOvereof", p.vwr_overeof);
    v.Field("gunAngle", p.gun_angle);
    v.Field("fireX", p.fire_x);
    v.Field("fireY", p.fire_y);
    v.Field("fireZ", p.fire_z);
    v.Field("numWeapons", p.num_weapons);
    v.Field("gunPower", p.gun_power);
    v.Field("gunRadius", p.gun_radius);
    v.Field("killAfterShot", p.kill_after_shot);
    v.Field("scaleFx0", p.scale_fx_p0);
    v.Field("scaleFx1", p.scale_fx_p1);
    v.Field("scaleFx2", p.scale_fx_p2);
    v.Field("scaleFx3", p.scale_fx_p3);
    v.Field("scaleFxModels", p.scale_fx_pXX.data(), p.scale_fx_pXX.size());
    v.Field("jobFightTank", p.job_fighttank);
    v.Field("jobFightHeli", p.job_fighthelicopter);
    v.Field("jobFightFlyer", p.job_fightflyer);
    v.Field("jobFightRobo", p.job_fightrobo);
    v.Field("jobConquer", p.job_conquer);
    v.Field("jobRecon", p.job_reconnoitre);
    v.Field("hidden", p.hidden);
    v.Field("unhideRadar", p.unhideRadar);
}

static void VisitWeapon(Visitor &v, World::TWeapProto &p)
{
    v.Field("unitType", p.unitID);
    v.Field("enable", p.enable_mask);
    v.Field("flags", p._weaponFlags);
    v.Field("icon", p.type_icon);
    v.Field("vpNormal", p.vp_normal);
    v.Field("vpFire", p.vp_fire);
    v.Field("vpDead", p.vp_dead);
    v.Field("vpWait", p.vp_wait);
    v.Field("vpMegadeth", p.vp_megadeth);
    v.Field("vpGenesis", p.vp_genesis);
    v.Field("energy", p.energy);
    v.Field("lifeTime", p.life_time);
    v.Field("lifeTimeNoTarget", p.life_time_nt);
    v.Field("driveTime", p.drive_time);
    v.Field("delayTime", p.delay_time);
    v.Field("adistSector", p.adistSector);
    v.Field("adistUnit", p.adistBact);
    v.Field("shotTime", p.shot_time);
    v.Field("shotTimeUser", p.shot_time_user);
    v.Field("salveShots", p.salve_shots);
    v.Field("salveDelay", p.salve_delay);
    v.Field("energyHeli", p.energy_heli);
    v.Field("energyTank", p.energy_tank);
    v.Field("energyFlyer", p.energy_flyer);
    v.Field("energyRobo", p.energy_robo);
    v.Field("radiusHeli", p.radius_heli);
    v.Field("radiusTank", p.radius_tank);
    v.Field("radiusFlyer", p.radius_flyer);
    v.Field("radiusRobo", p.radius_robo);
    v.Field("mass", p.mass);
    v.Field("force", p.force);
    v.Field("airconst", p.airconst);
    v.Field("maxrot", p.maxrot);
    v.Field("height", p.heightStd);
    v.Field("radius", p.radius);
    v.Field("overeof", p.overeof);
    v.Field("viewerRadius", p.vwr_radius);
    v.Field("viewerOvereof", p.vwr_overeof);
    v.Field("startSpeed", p.start_speed);
}

static void VisitBuilding(Visitor &v, World::TBuildingProto &p)
{
    v.Field("sectorType", p.SecType);
    v.Field("enable", p.EnableMask);
    v.Field("model", p.ModelID);
    v.Field("power", p.Power);
    v.Field("icon", p.TypeIcon);
    v.Field("energy", p.Energy);
}

// Capture

static void ForEachUnit(Visitor &v, std::unordered_set<uint32_t> &added, NC_STACK_ypabact *unit, const ObjectFunc &func)
{
    if ( !added.insert(unit->_gid).second )
        return;

    v.Out().clear();
    unit->VisitState(v);
    func(KIND_UNIT + unit->_bact_type, unit->_gid, v.Out());

    for ( NC_STACK_ypabact *kid : unit->_kidList )
        ForEachUnit(v, added, kid, func);

    for ( NC_STACK_ypamissile *m : unit->_missiles_list )
        ForEachUnit(v, added, m, func);
}

void ForEachObject(NC_STACK_ypaworld *world, Part part, const ObjectFunc &allFunc, const Schema *schema)
{
    auto has = [schema](uint8_t kind) { return !schema || schema->count(kind); };

    // Objects with nothing in a part are left out of it
    ObjectFunc func = [part, &allFunc](uint8_t kind, uint32_t id, const std::string &data)
    {
        if ( part == PART_ALL || !data.empty() )
            allFunc(kind, id, data);
    };

    // One visitor for all, its buffer kept
    Visitor v(Visitor::WRITE, world, part);

    world->VisitState(v);
    func(KIND_WORLD, 0, v.Out());

    if ( has(KIND_CAMPAIGN) )
    {
        v.Out().clear();
        VisitCampaign(v, world);
        func(KIND_CAMPAIGN, 0, v.Out());
    }

    for (size_t i = 0; has(KIND_SECTOR) && i < world->_cells.size(); i++)
    {
        v.Out().clear();
        VisitSector(v, world, world->_cells.At(i), world->_energyAccumMap.At(i));
        func(KIND_SECTOR, i, v.Out());
    }

    for (size_t i = 0; has(KIND_SUPERITEM) && i < world->_levelInfo.SuperItems.size(); i++)
    {
        v.Out().clear();
        VisitSuperItem(v, world->_levelInfo.SuperItems[i]);
        func(KIND_SUPERITEM, i, v.Out());
    }

    for (size_t i = 0; has(KIND_VEHICLE) && i < world->_vhclProtos.size(); i++)
    {
        v.Out().clear();
        VisitVehicle(v, world->_vhclProtos[i]);
        func(KIND_VEHICLE, i, v.Out());
    }

    for (size_t i = 0; has(KIND_WEAPON) && i < world->_weaponProtos.size(); i++)
    {
        v.Out().clear();
        VisitWeapon(v, world->_weaponProtos[i]);
        func(KIND_WEAPON, i, v.Out());
    }

    for (size_t i = 0; has(KIND_BUILDING) && i < world->_buildProtos.size(); i++)
    {
        v.Out().clear();
        VisitBuilding(v, world->_buildProtos[i]);
        func(KIND_BUILDING, i, v.Out());
    }

    std::unordered_set<uint32_t> added;

    for ( NC_STACK_ypabact *unit : world->_unitsList )
        ForEachUnit(v, added, unit, func);

    // Units in no list but a sector's, which the internal part would miss.
    // There shouldn't be any.
    for ( cellArea &cell : world->_cells )
    {
        if ( part == PART_VIEW )
            break;

        for ( NC_STACK_ypabact *unit : cell.unitsList )
        {
            if ( !unit->_kidRef.IsListType(World::BLIST_CACHE) && !added.count(unit->_gid) )
            {
                ypa_log_out("State: unit %d (type %d) is only in a sector's list\n", unit->_gid, unit->_bact_type);
                ForEachUnit(v, added, unit, func);
            }
        }
    }
}

Image Capture(NC_STACK_ypaworld *world, Part part)
{
    Image image;
    image.Level = world->_levelInfo.LevelID;

    ForEachObject(world, part, [&image](uint8_t kind, uint32_t id, const std::string &data)
    {
        Object obj;
        obj.Kind = kind;
        obj.Id = id;
        obj.Data = data;
        image.Objects.push_back(std::move(obj));
    });

    return image;
}


// Schema

Schema MakeSchema(NC_STACK_ypaworld *world, Part part)
{
    Schema schema;

    {
        Visitor v(Visitor::SCHEMA, world, part);
        world->VisitState(v);
        schema[KIND_WORLD] = v.Fields();
    }

    if ( world->_cells.size() )
    {
        Visitor v(Visitor::SCHEMA, world, part);
        VisitSector(v, world, world->_cells.At(0), world->_energyAccumMap.At(0));
        schema[KIND_SECTOR] = v.Fields();
    }

    {
        TMapSuperItem item;
        Visitor v(Visitor::SCHEMA, world, part);
        VisitSuperItem(v, item);
        schema[KIND_SUPERITEM] = v.Fields();
    }

    {
        Visitor v(Visitor::SCHEMA, world, part);
        VisitCampaign(v, world);
        schema[KIND_CAMPAIGN] = v.Fields();
    }

    {
        World::TVhclProto vhcl;
        Visitor v(Visitor::SCHEMA, world, part);
        VisitVehicle(v, vhcl);
        schema[KIND_VEHICLE] = v.Fields();
    }

    {
        World::TWeapProto weapon;
        Visitor v(Visitor::SCHEMA, world, part);
        VisitWeapon(v, weapon);
        schema[KIND_WEAPON] = v.Fields();
    }

    {
        World::TBuildingProto building;
        Visitor v(Visitor::SCHEMA, world, part);
        VisitBuilding(v, building);
        schema[KIND_BUILDING] = v.Fields();
    }

    // A unit of each class
    for (int type : {BACT_TYPES_BACT, BACT_TYPES_TANK, BACT_TYPES_ROBO, BACT_TYPES_MISSLE, BACT_TYPES_FLYER,
                     BACT_TYPES_UFO, BACT_TYPES_CAR, BACT_TYPES_GUN})
    {
        NC_STACK_ypabact *unit = world->MakeUnitObject(type);
        if ( !unit )
            continue;

        Visitor v(Visitor::SCHEMA, world, part);
        unit->VisitState(v);
        schema[KIND_UNIT + type] = v.Fields();

        // Never alive, so with nothing to do as it goes
        unit->_status_flg = BACT_STFLAG_DEATH1;
        unit->Delete();
    }

    // Kinds with nothing in the part
    for (auto it = schema.begin(); it != schema.end(); )
    {
        if ( it->second.empty() )
            it = schema.erase(it);
        else
            it++;
    }

    return schema;
}


// Hash and diff

uint64_t Hash(const Image &image)
{
    uint64_t h = 1469598103934665603ULL;

    auto mix = [&h](const void *data, size_t size)
    {
        const uint8_t *bytes = (const uint8_t *)data;
        for (size_t i = 0; i < size; i++)
        {
            h ^= bytes[i];
            h *= 1099511628211ULL;
        }
    };

    for ( const Object &obj : image.Objects )
    {
        mix(&obj.Kind, 1);
        mix(&obj.Id, 4);
        mix(obj.Data.data(), obj.Data.size());
    }

    return h;
}

std::string Pack(const Image &image)
{
    std::string raw;

    auto put = [&raw](const void *data, size_t size) { raw.append((const char *)data, size); };

    int32_t level = image.Level;
    uint32_t count = image.Objects.size();
    put(&level, 4);
    put(&count, 4);

    for ( const Object &obj : image.Objects )
    {
        uint32_t size = obj.Data.size();
        put(&obj.Kind, 1);
        put(&obj.Id, 4);
        put(&size, 4);
        raw += obj.Data;
    }

    uLongf packedSize = compressBound(raw.size());
    std::string packed(packedSize + 4, '\0');
    uint32_t rawSize = raw.size();
    memcpy(&packed[0], &rawSize, 4);

    if ( compress2((Bytef *)&packed[4], &packedSize, (const Bytef *)raw.data(), raw.size(), Z_BEST_SPEED) != Z_OK )
        return std::string();

    packed.resize(packedSize + 4);
    return packed;
}

bool Unpack(const std::string &packed, Image &image)
{
    if ( packed.size() < 4 )
        return false;

    uint32_t rawSize;
    memcpy(&rawSize, packed.data(), 4);

    std::string raw(rawSize, '\0');
    uLongf size = rawSize;

    if ( uncompress((Bytef *)&raw[0], &size, (const Bytef *)packed.data() + 4, packed.size() - 4) != Z_OK || size != rawSize )
        return false;

    size_t pos = 0;
    auto take = [&raw, &pos](void *data, size_t n)
    {
        if ( pos + n > raw.size() )
            return false;
        memcpy(data, raw.data() + pos, n);
        pos += n;
        return true;
    };

    uint32_t count;
    image = Image();

    if ( !take(&image.Level, 4) || !take(&count, 4) )
        return false;

    for (uint32_t i = 0; i < count; i++)
    {
        Object obj;
        uint32_t dataSize;

        if ( !take(&obj.Kind, 1) || !take(&obj.Id, 4) || !take(&dataSize, 4) || pos + dataSize > raw.size() )
            return false;

        obj.Data = raw.substr(pos, dataSize);
        pos += dataSize;
        image.Objects.push_back(std::move(obj));
    }

    return pos == raw.size();
}

size_t ElemSize(Type type)
{
    switch ( type )
    {
    case T_BOOL:
    case T_I8:
    case T_U8:
        return 1;

    case T_I16:
    case T_U16:
        return 2;

    case T_I32:
    case T_U32:
    case T_F32:
    case T_UNIT:
    case T_SECTOR:
        return 4;

    case T_F64:
        return 8;
    }

    return 1;
}

size_t FieldSize(const FieldInfo &field, const char *data, size_t size)
{
    size_t need;

    if ( !field.List )
    {
        need = ElemSize(field.ElemType) * field.Count;
    }
    else
    {
        if ( size < 2 )
            return 0;

        uint16_t n;
        memcpy(&n, data, 2);
        need = 2 + n * field.Count * ElemSize(field.ElemType);
    }

    return need <= size ? need : 0;
}

bool FieldOffsets(const std::vector<FieldInfo> &fields, const std::string &data, std::vector<size_t> &offsets)
{
    offsets.resize(fields.size() + 1);
    size_t pos = 0;

    for (size_t i = 0; i < fields.size(); i++)
    {
        offsets[i] = pos;

        size_t size = FieldSize(fields[i], data.data() + pos, data.size() - pos);
        if ( !size )
            return false;

        pos += size;
    }

    offsets[fields.size()] = pos;
    return pos == data.size();
}

std::string WriteSchema(const Schema &schema)
{
    std::string out;

    auto put = [&out](const void *data, size_t size) { out.append((const char *)data, size); };

    uint16_t kinds = schema.size();
    put(&kinds, 2);

    for ( const auto &it : schema )
    {
        uint16_t n = it.second.size();
        put(&it.first, 1);
        put(&n, 2);

        for ( const FieldInfo &f : it.second )
        {
            uint8_t type = f.ElemType;
            uint8_t list = f.List ? 1 : 0;
            uint8_t len = f.Name.size();
            put(&type, 1);
            put(&f.Count, 2);
            put(&list, 1);
            put(&len, 1);
            out.append(f.Name, 0, len);
        }
    }

    return out;
}

bool ReadSchema(const std::string &bytes, Schema &schema)
{
    size_t pos = 0;

    auto take = [&bytes, &pos](void *data, size_t size)
    {
        if ( pos + size > bytes.size() )
            return false;
        memcpy(data, bytes.data() + pos, size);
        pos += size;
        return true;
    };

    schema.clear();

    uint16_t kinds;
    if ( !take(&kinds, 2) )
        return false;

    for (uint16_t k = 0; k < kinds; k++)
    {
        uint8_t kind;
        uint16_t n;
        if ( !take(&kind, 1) || !take(&n, 2) )
            return false;

        std::vector<FieldInfo> &fields = schema[kind];

        for (uint16_t i = 0; i < n; i++)
        {
            FieldInfo f;
            uint8_t type, list, len;

            if ( !take(&type, 1) || !take(&f.Count, 2) || !take(&list, 1) || !take(&len, 1) || pos + len > bytes.size() )
                return false;

            f.ElemType = (Type)type;
            f.List = list != 0;
            f.Name = bytes.substr(pos, len);
            pos += len;
            fields.push_back(f);
        }
    }

    return pos == bytes.size();
}

// The bytes of each field of an object
static std::vector<std::string> SplitFields(const std::vector<FieldInfo> &fields, const std::string &data)
{
    std::vector<std::string> out;
    std::vector<size_t> offsets;

    FieldOffsets(fields, data, offsets);

    for (size_t i = 0; i < fields.size(); i++)
    {
        size_t start = std::min(offsets[i], data.size());
        size_t end = std::max(start, std::min(offsets[i + 1], data.size()));
        out.push_back(data.substr(start, end - start));
    }

    return out;
}

static std::string ShowValue(const FieldInfo &f, const std::string &bytes)
{
    std::string s;
    size_t elem = ElemSize(f.ElemType);
    size_t start = f.List ? 2 : 0;
    size_t shown = 0;

    for (size_t pos = start; pos + elem <= bytes.size() && shown < 12; pos += elem, shown++)
    {
        const char *p = bytes.data() + pos;

        if ( !s.empty() )
            s += " ";

        switch ( f.ElemType )
        {
        case T_BOOL:
        case T_U8:  s += fmt::sprintf("%u", (uint8_t)*p); break;
        case T_I8:  s += fmt::sprintf("%d", (int8_t)*p); break;
        case T_I16: { int16_t x; memcpy(&x, p, 2); s += fmt::sprintf("%d", x); break; }
        case T_U16: { uint16_t x; memcpy(&x, p, 2); s += fmt::sprintf("%u", x); break; }
        case T_I32:
        case T_SECTOR: { int32_t x; memcpy(&x, p, 4); s += fmt::sprintf("%d", x); break; }
        case T_U32: { uint32_t x; memcpy(&x, p, 4); s += fmt::sprintf("%u", x); break; }
        case T_UNIT: { uint32_t x; memcpy(&x, p, 4); s += fmt::sprintf("#%u", x); break; }
        case T_F32: { float x; memcpy(&x, p, 4); s += fmt::sprintf("%.9g", x); break; }
        case T_F64: { double x; memcpy(&x, p, 8); s += fmt::sprintf("%.17g", x); break; }
        }
    }

    if ( shown == 12 )
        s += " ...";

    return s;
}

static std::string ObjectName(const Object &obj)
{
    if ( obj.Kind == KIND_WORLD )
        return "world";

    if ( obj.Kind == KIND_SECTOR )
        return fmt::sprintf("sector %d", obj.Id);

    if ( obj.Kind == KIND_SUPERITEM )
        return fmt::sprintf("bomb %d", obj.Id);

    if ( obj.Kind == KIND_CAMPAIGN )
        return "campaign";

    if ( obj.Kind == KIND_VEHICLE )
        return fmt::sprintf("vehicle %d", obj.Id);

    if ( obj.Kind == KIND_WEAPON )
        return fmt::sprintf("weapon %d", obj.Id);

    if ( obj.Kind == KIND_BUILDING )
        return fmt::sprintf("building %d", obj.Id);

    return fmt::sprintf("unit #%u (type %d)", obj.Id, obj.Kind - KIND_UNIT);
}

std::string Diff(const Schema &schema, const Image &a, const Image &b, size_t maxLines)
{
    std::vector<std::string> lines;

    std::map<std::pair<uint8_t, uint32_t>, const Object *> inB;
    for ( const Object &obj : b.Objects )
        inB[{obj.Kind, obj.Id}] = &obj;

    std::set<std::pair<uint8_t, uint32_t>> inA;

    for ( const Object &oa : a.Objects )
    {
        inA.insert({oa.Kind, oa.Id});

        auto it = inB.find({oa.Kind, oa.Id});
        if ( it == inB.end() )
        {
            lines.push_back(ObjectName(oa) + ": only in the first");
            continue;
        }

        const Object &ob = *it->second;
        if ( oa.Data == ob.Data )
            continue;

        auto sit = schema.find(oa.Kind);
        if ( sit == schema.end() )
        {
            lines.push_back(ObjectName(oa) + ": differs");
            continue;
        }

        std::vector<std::string> fa = SplitFields(sit->second, oa.Data);
        std::vector<std::string> fb = SplitFields(sit->second, ob.Data);

        for (size_t i = 0; i < sit->second.size(); i++)
        {
            if ( fa[i] != fb[i] )
                lines.push_back(fmt::sprintf("%s %s: %s / %s", ObjectName(oa), sit->second[i].Name,
                                             ShowValue(sit->second[i], fa[i]), ShowValue(sit->second[i], fb[i])));
        }
    }

    for ( const Object &ob : b.Objects )
    {
        if ( !inA.count({ob.Kind, ob.Id}) )
            lines.push_back(ObjectName(ob) + ": only in the second");
    }

    // Objects in another order
    if ( lines.empty() && !(a.Objects == b.Objects) )
        lines.push_back("the same objects, in another order");

    std::string out;
    for (size_t i = 0; i < lines.size() && i < maxLines; i++)
        out += lines[i] + "\n";

    if ( lines.size() > maxLines )
        out += fmt::sprintf("... and %d more\n", (int)(lines.size() - maxLines));

    return out;
}


// Restore

// Reads the objects of the images, each of a part, in that order
static bool RestoreParts(NC_STACK_ypaworld *world, const std::vector<std::pair<Part, const Image *>> &parts, bool campaign)
{
    const Image &first = *parts.front().second;

    if ( first.Objects.empty() || first.Objects[0].Kind != KIND_WORLD )
    {
        ypa_log_out("State: no world in the image\n");
        return false;
    }

    if ( !world->LoadLevelData(first.Level) )
    {
        ypa_log_out("State: can't load level %d\n", first.Level);
        return false;
    }

    // Every unit first, so that the fields can refer to any of them
    std::map<uint32_t, NC_STACK_ypabact *> units;

    for ( const auto &part : parts )
    {
        for ( const Object &obj : part.second->Objects )
        {
            if ( obj.Kind < KIND_UNIT || units.count(obj.Id) )
                continue;

            NC_STACK_ypabact *unit = world->NewUnitObject(obj.Kind - KIND_UNIT);
            if ( !unit )
            {
                ypa_log_out("State: can't make a unit of type %d\n", obj.Kind - KIND_UNIT);
                world->DeleteLevel();
                return false;
            }

            unit->_gid = obj.Id;
            units[obj.Id] = unit;
        }
    }

    for ( const auto &part : parts )
    {
        for ( const Object &obj : part.second->Objects )
        {
            Visitor v(Visitor::READ, world, part.first);
            v.SetUnits(&units);
            v.SetData(&obj.Data);

            if ( obj.Kind == KIND_WORLD )
            {
                world->VisitState(v);
            }
            else if ( obj.Kind == KIND_CAMPAIGN )
            {
                if ( !campaign )
                    continue;

                VisitCampaign(v, world);
            }
            else if ( obj.Kind == KIND_SECTOR )
            {
                if ( obj.Id >= world->_cells.size() )
                    continue;

                VisitSector(v, world, world->_cells.At(obj.Id), world->_energyAccumMap.At(obj.Id));
            }
            else if ( obj.Kind == KIND_SUPERITEM )
            {
                if ( obj.Id >= world->_levelInfo.SuperItems.size() )
                    continue;

                VisitSuperItem(v, world->_levelInfo.SuperItems[obj.Id]);
            }
            else if ( obj.Kind == KIND_VEHICLE )
            {
                if ( obj.Id >= world->_vhclProtos.size() )
                    continue;

                VisitVehicle(v, world->_vhclProtos[obj.Id]);
            }
            else if ( obj.Kind == KIND_WEAPON )
            {
                if ( obj.Id >= world->_weaponProtos.size() )
                    continue;

                VisitWeapon(v, world->_weaponProtos[obj.Id]);
            }
            else if ( obj.Kind == KIND_BUILDING )
            {
                if ( obj.Id >= world->_buildProtos.size() )
                    continue;

                VisitBuilding(v, world->_buildProtos[obj.Id]);
            }
            else
            {
                units[obj.Id]->VisitState(v);
            }

            if ( !v.ReadAll() )
            {
                ypa_log_out("State: %s doesn't read back\n", ObjectName(obj).c_str());
                world->DeleteLevel();
                return false;
            }
        }
    }

    world->FinishRestore();
    return true;
}

bool Restore(NC_STACK_ypaworld *world, const Image &image)
{
    return RestoreParts(world, {{PART_ALL, &image}}, true);
}

// The view first: it has each unit's vehicle, which gives the unit its
// prototype's models and guns that the internal part then fills in
bool Restore(NC_STACK_ypaworld *world, const Image &view, const Image &internal, bool campaign)
{
    return RestoreParts(world, {{PART_VIEW, &view}, {PART_INTERNAL, &internal}}, campaign);
}

}
}
