#ifndef WORLD_STATE_H_INCLUDED
#define WORLD_STATE_H_INCLUDED

#include <cstdint>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "../vectors.h"
#include "../matrix.h"
#include "../common/common.h"
#include "../types.h"

class NC_STACK_ypaworld;
class NC_STACK_ypabact;
class NC_STACK_ypamissile;
struct cellArea;
struct TBactAttacker;
union BactTarget;

namespace World
{
namespace State
{

// The simulation's state: every object the simulation has, each with the
// values of its fields, exact. The fields are listed once, in the VisitState
// of each class (and VisitSector for sectors); a Visitor goes through them to
// write them, to read them back, or to learn their names and types, which
// files hold so that they can be read without this build's layouts. What the
// simulation can work out again (the sector a unit is in, counts per owner,
// the models and sounds of a unit's prototype) is left out and worked out
// after reading.
//
// Values are little-endian, floats by their bits. A list is its u16 length,
// then its entries, each Count elements.

// The type of a field's elements
enum Type : uint8_t
{
    T_BOOL = 1,
    T_I8,
    T_U8,
    T_I16,
    T_U16,
    T_I32,
    T_U32,
    T_F32,
    T_F64,
    T_UNIT,   // u32: a unit's id, 0 for none
    T_SECTOR, // i32: a sector's index, y * map width + x, -1 for none
};

struct FieldInfo
{
    std::string Name;
    Type ElemType = T_U8;
    uint16_t Count = 1; // Elements, of the field or of each entry of a list
    bool List = false;
};

// What an object is. Units by their class: KIND_UNIT + BACT_TYPES_*.
enum Kind : uint8_t
{
    KIND_WORLD = 1,
    KIND_SECTOR = 2,
    KIND_SUPERITEM = 3, // Bombs and waves, by their index in the level
    KIND_VEHICLE = 4,   // Prototypes, by their index: what tech upgrades
    KIND_WEAPON = 5,    // change, and what earlier levels of the campaign
    KIND_BUILDING = 6,  // did
    KIND_UNIT = 16,
};

struct Object
{
    uint8_t Kind = 0;
    uint32_t Id = 0;   // Units: their id; sectors: their index; else 0
    std::string Data;  // The fields' values

    bool operator==(const Object &b) const { return Kind == b.Kind && Id == b.Id && Data == b.Data; }
};

// The state at one moment: the world first, then the sectors in index order,
// the bombs in theirs, the prototypes in theirs, then every unit, a unit's
// kids and missiles after it
struct Image
{
    int32_t Level = 0;
    std::vector<Object> Objects;
};

// The fields of each kind of object, in the order they are written
typedef std::map<uint8_t, std::vector<FieldInfo>> Schema;

class Visitor
{
public:
    enum Mode
    {
        WRITE,  // Fields to bytes
        READ,   // Bytes to fields
        SCHEMA, // Names and types
    };

    Visitor(Mode mode, NC_STACK_ypaworld *world) : _mode(mode), _world(world) {}

    Mode GetMode() const { return _mode; }
    bool Reading() const { return _mode == READ; }

    // Writing: the bytes so far. Reading: the bytes to read from.
    void SetData(const std::string *data) { _in = data; _inPos = 0; }
    std::string &Out() { return _out; }
    std::vector<FieldInfo> &Fields() { return _fields; }

    // Reading: the units by id, to turn ids into units
    void SetUnits(const std::map<uint32_t, NC_STACK_ypabact *> *units) { _units = units; }

    void Field(const char *name, bool &v);
    void Field(const char *name, int8_t &v);
    void Field(const char *name, uint8_t &v);
    void Field(const char *name, int16_t &v);
    void Field(const char *name, uint16_t &v);
    void Field(const char *name, int32_t &v);
    void Field(const char *name, uint32_t &v);
    void Field(const char *name, float &v);
    void Field(const char *name, double &v);
    void Field(const char *name, vec2d &v);
    void Field(const char *name, vec3d &v);
    void Field(const char *name, mat3x3 &v);
    void Field(const char *name, Common::Point &v);
    void Field(const char *name, vec3d *v, size_t count);
    void Field(const char *name, int32_t *v, size_t count);
    void Field(const char *name, int16_t *v, size_t count);
    void Field(const char *name, uint32_t *v, size_t count);

    // Lists
    void Field(const char *name, std::vector<uint8_t> &v);
    void Field(const char *name, std::vector<int32_t> &v);
    void Field(const char *name, std::vector<vec3d> &v);
    void Units(const char *name, std::vector<NC_STACK_ypabact *> &v);

    // A unit it refers to
    void Unit(const char *name, NC_STACK_ypabact *&unit);

    template <class U>
    void Unit(const char *name, U *&unit)
    {
        NC_STACK_ypabact *u = unit;
        Unit(name, u);

        if ( Reading() )
            unit = dynamic_cast<U *>(u);
    }

    // A sector it refers to
    void Sector(const char *name, cellArea *&cell);

    // A target, by its type: a unit, a sector or nothing
    void Target(const char *name, uint8_t type, BactTarget &target);

    // Units in a list, in its order. Reading puts them in the list, as
    // newMaster_msg would (the unit's _kidRef) or in a sector's (_cellRef).
    void Kids(const char *name, World::RefBactList &list);
    void SectorUnits(const char *name, cellArea &cell);
    void Missiles(const char *name, World::MissileList &list);
    void Attackers(const char *typesName, const char *name, std::list<TBactAttacker> &list);

    // Reading: whether all the bytes were read, and none too many
    bool ReadAll() const { return !_failed && _in && _inPos == _in->size(); }
    bool Failed() const { return _failed; }

protected:
    // Writing and reading a field's raw bytes, little-endian on the
    // platforms that build this
    void Raw(const char *name, Type type, uint16_t count, void *data, size_t size);
    void Info(const char *name, Type type, uint16_t count);
    bool Take(void *data, size_t size);
    void Put(const void *data, size_t size);

    uint16_t ListLength(const char *name, Type type, uint16_t count, size_t length);
    uint32_t UnitId(const NC_STACK_ypabact *unit) const;
    NC_STACK_ypabact *UnitById(uint32_t id) const;

    Mode _mode;
    NC_STACK_ypaworld *_world;

    std::string _out;
    const std::string *_in = NULL;
    size_t _inPos = 0;
    bool _failed = false;

    std::vector<FieldInfo> _fields;
    const std::map<uint32_t, NC_STACK_ypabact *> *_units = NULL;
};

// The world's state now
Image Capture(NC_STACK_ypaworld *world);

// The fields of every kind of object in this build
Schema MakeSchema(NC_STACK_ypaworld *world);

// Sets up the level the image is of, in the state it holds. False, after
// logging why, when it can't; the level is then gone.
bool Restore(NC_STACK_ypaworld *world, const Image &image);

// A hash of the whole state
uint64_t Hash(const Image &image);

// An image in one string, zlib-packed, and back
std::string Pack(const Image &image);
bool Unpack(const std::string &packed, Image &image);

// Where two images differ, field by field, as lines of text; empty when they
// are the same. At most maxLines lines.
std::string Diff(const Schema &schema, const Image &a, const Image &b, size_t maxLines = 40);

}
}

#endif
