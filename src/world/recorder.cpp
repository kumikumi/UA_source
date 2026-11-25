#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <zlib.h>

#include "recorder.h"
#include "../yw.h"
#include "../ypamissile.h"
#include "../IFFile.h"
#include "../utils.h"
#include "../log.h"

namespace World
{

// Little-endian values into a byte string

static void Put8(std::string &out, uint8_t v)
{
    out.push_back((char)v);
}

static void Put16(std::string &out, uint16_t v)
{
    Put8(out, v & 0xFF);
    Put8(out, v >> 8);
}

static void Put32(std::string &out, uint32_t v)
{
    Put16(out, v & 0xFFFF);
    Put16(out, v >> 16);
}

static void Put64(std::string &out, uint64_t v)
{
    Put32(out, v & 0xFFFFFFFF);
    Put32(out, v >> 32);
}

static void PutText(std::string &out, const std::string &text)
{
    size_t length = std::min<size_t>(text.size(), 0xFFFF);
    Put16(out, length);
    out.append(text, 0, length);
}


// Mixes the state's bytes, 8 at a time, doubles and all: what tells two
// recordings apart at the first step they differ
static uint64_t Hash(uint64_t h, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;

    for (; size >= 8; bytes += 8, size -= 8)
    {
        uint64_t word;
        memcpy(&word, bytes, 8);
        h = (h ^ word) * 0x9E3779B97F4A7C15ULL;
        h ^= h >> 29;
    }

    for (; size > 0; bytes++, size--)
    {
        h = (h ^ *bytes) * 0x9E3779B97F4A7C15ULL;
        h ^= h >> 29;
    }

    return h;
}

static const uint64_t HashStart = 1469598103934665603ULL;

static uint32_t SectorIndex(NC_STACK_ypaworld *world, const cellArea *cell)
{
    if ( !cell )
        return 0;

    return cell->CellId.y * world->_mapSize.x + cell->CellId.x;
}

static uint32_t TargetId(NC_STACK_ypaworld *world, uint8_t type, const BactTarget &target, const vec3d &pos)
{
    switch ( type )
    {
    case BACT_TGT_TYPE_UNIT:
        return target.pbact ? target.pbact->_gid : 0;

    case BACT_TGT_TYPE_CELL:
        return SectorIndex(world, target.pcell);

    case BACT_TGT_TYPE_FRMT:
    case BACT_TGT_TYPE_DRCT:
    {
        // A position: the sector it is in
        Common::Point sector = World::PositionToSectorID(pos);
        return sector.y * world->_mapSize.x + sector.x;
    }

    default:
        return 0;
    }
}

static uint8_t Look(NC_STACK_ypabact *unit)
{
    NC_STACK_base *vp = unit->GetVP();

    if ( vp == unit->_vp_normal )
        return 1;
    else if ( vp == unit->_vp_fire )
        return 2;
    else if ( vp == unit->_vp_wait )
        return 3;
    else if ( vp == unit->_vp_dead )
        return 4;
    else if ( vp == unit->_vp_megadeth )
        return 5;
    else if ( vp == unit->_vp_genesis )
        return 6;

    return 0;
}

static std::string Platform()
{
    std::string platform;

#if defined(__APPLE__)
    platform = "macOS";
#elif defined(_WIN32)
    platform = "Windows";
#elif defined(__linux__)
    platform = "Linux";
#else
    platform = "unknown";
#endif

#if defined(__aarch64__) || defined(__arm64__)
    platform += " arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    platform += " x86-64";
#endif

    return platform;
}


Recorder::Recorder() {}
Recorder::~Recorder() {}

bool Recorder::Start(NC_STACK_ypaworld *world, const std::string &path, const std::string &startedFrom)
{
    FSMgr::FileHandle *file = new FSMgr::FileHandle(path, "wb");

    if ( !file->OK() )
    {
        delete file;
        ypa_log_out("Unable to write the recording %s\n", path.c_str());
        return false;
    }

    _file.reset(new IFFile(file, true));
    _file->pushChunk(TAG_RPLY, TAG_FORM, -1);

    std::string info;
    Put16(info, 1);
    Put16(info, world->_levelInfo.LevelID);

    for (const std::string &text : {startedFrom, Platform(),
#if defined(__VERSION__)
                                    std::string(__VERSION__),
#else
                                    std::string(),
#endif
                                    std::string(__DATE__ " " __TIME__)})
    {
        info += text;
        info.push_back('\0');
    }

    _file->pushChunk(0, TAG_INFO, -1);
    _file->write(info.data(), info.size());
    _file->popChunk();

    _step = 0;
    BeginBlock(world);
    return true;
}

void Recorder::Step(NC_STACK_ypaworld *world)
{
    if ( !_file )
        return;

    _step++;

    Put32(_steps, _step);
    Put32(_steps, world->_timeStamp);

    _objects.clear();
    _objectCount = 0;

    uint64_t unitsHash = HashStart;
    for (NC_STACK_ypabact *unit : world->_unitsList)
        WriteObject(world, unit, &unitsHash);

    uint64_t sectorsHash = HashStart;
    WriteSectors(world, &sectorsHash);

    Put64(_steps, unitsHash);
    Put64(_steps, sectorsHash);
    Put32(_steps, world->_userUnit ? world->_userUnit->_gid : 0);

    Put16(_steps, _objectCount);
    _steps += _objects;

    std::string gone;
    uint16_t goneCount = 0;

    for (auto it = _lastObjects.begin(); it != _lastObjects.end(); )
    {
        if ( it->second.seenStep == _step )
        {
            it++;
        }
        else
        {
            Put32(gone, it->first);
            goneCount++;
            it = _lastObjects.erase(it);
        }
    }

    Put16(_steps, goneCount);
    _steps += gone;

    _steps += _sectorsOut;

    Put16(_steps, _messageCount);
    _steps += _messages;
    _messages.clear();
    _messageCount = 0;

    _blockSteps++;

    if ( _blockSteps >= BlockSteps )
    {
        WriteBlock();
        BeginBlock(world);
    }
}

void Recorder::Message(const TInfoMessage &msg)
{
    if ( !_file )
        return;

    Put32(_messages, msg.MsgID);
    Put16(_messages, msg.Priority);
    Put32(_messages, msg.unit ? msg.unit->_gid : 0);
    PutText(_messages, msg.txt);
    _messageCount++;
}

void Recorder::Stop(NC_STACK_ypaworld *world)
{
    if ( !_file )
        return;

    // The block being recorded, then one with only the end's state
    if ( _blockSteps > 0 )
    {
        WriteBlock();
        BeginBlock(world);
    }

    WriteBlock();

    _file->popChunk();
    _file.reset();
}

void Recorder::BeginBlock(NC_STACK_ypaworld *world)
{
    FSMgr::MemFileHandle save;
    world->WriteSave(&save, false);

    _keyframe = std::move(save.Data);
    _steps.clear();
    _blockSteps = 0;

    _lastObjects.clear();
    _lastSectors.clear();
}

void Recorder::WriteBlock()
{
    _file->pushChunk(TAG_BLCK, TAG_FORM, -1);

    for (auto chunk : {std::make_pair(TAG_KEYF, &_keyframe), std::make_pair(TAG_STEP, &_steps)})
    {
        const std::string &data = *chunk.second;

        uLongf packedSize = compressBound(data.size());
        std::vector<uint8_t> packed(packedSize);

        if ( compress2(packed.data(), &packedSize, (const Bytef *)data.data(), data.size(), Z_BEST_SPEED) != Z_OK )
        {
            ypa_log_out("Recording: could not compress a block\n");
            packedSize = 0;
        }

        _file->pushChunk(0, chunk.first, -1);
        _file->writeU32L(data.size());
        _file->write(packed.data(), packedSize);
        _file->popChunk();
    }

    _file->popChunk();
}

void Recorder::WriteObject(NC_STACK_ypaworld *world, NC_STACK_ypabact *unit, uint64_t *hash)
{
    // The exact state, for the hash, in one piece
    struct
    {
        vec3d position, flyDir;
        mat3x3 rotation;
        uint32_t gid;
        int32_t energy, flags;
        float flyDirLength;
        uint8_t owner, type, status;
    } exact;

    memset(&exact, 0, sizeof(exact));
    exact.position = unit->_position;
    exact.flyDir = unit->_fly_dir;
    exact.rotation = unit->_rotation;
    exact.gid = unit->_gid;
    exact.energy = unit->_energy;
    exact.flags = unit->_status_flg;
    exact.flyDirLength = unit->_fly_dir_length;
    exact.owner = unit->_owner;
    exact.type = unit->_bact_type;
    exact.status = unit->_status;
    *hash = Hash(*hash, &exact, sizeof(exact));

    ObjectRecord rec;

    NC_STACK_ypabact *parent = unit->_parent;
    if ( unit->_bact_type == BACT_TYPES_MISSLE )
        parent = static_cast<NC_STACK_ypamissile *>(unit)->GetLauncherBact();

    rec.type = unit->_bact_type;
    rec.owner = unit->_owner;
    rec.vehicle = unit->_vehicleID;
    rec.parent = parent ? parent->_gid : 0;
    rec.status = unit->_status;
    rec.flags = unit->_status_flg;
    rec.energy = unit->_energy;
    rec.position[0] = unit->_position.x;
    rec.position[1] = unit->_position.y;
    rec.position[2] = unit->_position.z;

    const mat3x3 &m = unit->_rotation;
    const double rows[9] = {m.m00, m.m01, m.m02, m.m10, m.m11, m.m12, m.m20, m.m21, m.m22};
    for (int i = 0; i < 9; i++)
    {
        double v = rows[i] > 1.0 ? 1.0 : (rows[i] < -1.0 ? -1.0 : rows[i]);
        rec.rotation[i] = (int16_t)(v * 32767.0 + (v < 0.0 ? -0.5 : 0.5));
    }

    rec.look = Look(unit);
    rec.primaryType = unit->_primTtype;
    rec.primary = TargetId(world, unit->_primTtype, unit->_primT, unit->_primTpos);
    rec.secondaryType = unit->_secndTtype;
    rec.secondary = TargetId(world, unit->_secndTtype, unit->_secndT, unit->_sencdTpos);
    rec.aggression = unit->_aggr;

    // Where each OBJ_* field starts in the record, and the record's end
    static const size_t starts[] = {
        offsetof(ObjectRecord, type), offsetof(ObjectRecord, status), offsetof(ObjectRecord, energy),
        offsetof(ObjectRecord, position), offsetof(ObjectRecord, rotation), offsetof(ObjectRecord, look),
        offsetof(ObjectRecord, primaryType), offsetof(ObjectRecord, aggression), sizeof(ObjectRecord)};

    const uint8_t *now = (const uint8_t *)&rec;
    LastObject &last = _lastObjects[unit->_gid];
    uint16_t changed = 0;

    if ( last.seenStep == 0 )
        changed = 0xFF; // New
    else
    {
        const uint8_t *before = (const uint8_t *)&last.rec;

        for (int i = 0; i < 8; i++)
        {
            if ( memcmp(now + starts[i], before + starts[i], starts[i + 1] - starts[i]) )
                changed |= 1 << i;
        }
    }

    if ( changed )
    {
        Put32(_objects, unit->_gid);
        Put16(_objects, changed);

        for (int i = 0; i < 8; i++)
        {
            if ( changed & (1 << i) )
                _objects.append((const char *)now + starts[i], starts[i + 1] - starts[i]);
        }

        _objectCount++;
        last.rec = rec;
    }

    last.seenStep = _step;

    for (NC_STACK_ypamissile *m : unit->_missiles_list)
        WriteObject(world, m, hash);

    for (NC_STACK_ypabact *kid : unit->_kidList)
        WriteObject(world, kid, hash);
}

void Recorder::WriteSectors(NC_STACK_ypaworld *world, uint64_t *hash)
{
    const size_t count = world->_mapSize.x * world->_mapSize.y;
    const bool full = _lastSectors.size() != count; // All written anew

    if ( full )
        _lastSectors.resize(count);

    std::vector<SectorRecord> &now = _sectorsNow;
    now.resize(count);

    for (int y = 0; y < world->_mapSize.y; y++)
    {
        for (int x = 0; x < world->_mapSize.x; x++)
        {
            cellArea &cell = world->_cells(x, y);
            SectorRecord &rec = now[y * world->_mapSize.x + x];

            rec.owner = cell.owner;
            rec.type = cell.type_id;
            rec.simple = cell.SectorType;
            rec.purpose = cell.PurposeType;

            const int16_t *health = cell.buildings_health.data();
            for (int i = 0; i < 9; i++)
                rec.health[i] = health[i] < 0 ? 0 : (health[i] > 255 ? 255 : health[i]);
        }
    }

    *hash = Hash(*hash, now.data(), count * sizeof(SectorRecord));

    uint16_t written = 0;
    std::string out;

    for (size_t i = 0; i < count; i++)
    {
        if ( full || memcmp(&now[i], &_lastSectors[i], sizeof(SectorRecord)) )
        {
            Put16(out, i);
            out.append((const char *)&now[i], sizeof(SectorRecord));
            written++;
            _lastSectors[i] = now[i];
        }
    }

    _sectorsOut.clear();
    Put16(_sectorsOut, written);
    _sectorsOut += out;
}

}
