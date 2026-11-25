#ifndef WORLD_STATE_H_INCLUDED
#define WORLD_STATE_H_INCLUDED

#include <cstdint>
#include <functional>
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
// The state is in two parts. The view is what shows the game as it is drawn:
// where units are and how they're turned, at the float precision the
// renderer draws with, what they look like, who owns what. The internal part
// is the rest, with those positions and turns exact: the view and it hold
// the whole state. A field's role says which part it belongs to.
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
    KIND_CAMPAIGN = 7,  // Where the campaign stands: what a saved game takes
                        // back, but not a level started again
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

// Which part of the state a field is in
enum Role
{
    INTERNAL,     // The internal part
    VIEW,         // The view: exact, but doubles as the 32-bit floats they're
                  // drawn as
    VIEW_ROUNDED, // The view as 32-bit floats, and the internal part exact
};

// Which part of the state a visitor goes through
enum Part
{
    PART_ALL,
    PART_VIEW,
    PART_INTERNAL,
};

class Visitor
{
public:
    enum Mode
    {
        WRITE,  // Fields to bytes
        READ,   // Bytes to fields
        SCHEMA, // Names and types
    };

    Visitor(Mode mode, NC_STACK_ypaworld *world, Part part = PART_ALL) : _mode(mode), _part(part), _world(world) {}

    Mode GetMode() const { return _mode; }
    bool Reading() const { return _mode == READ; }

    // Whether this visit goes through fields of that role, and whether it
    // reads them
    bool Visits(Role role) const { return Includes(role); }
    bool Reads(Role role) const { return _mode == READ && Includes(role); }

    // Writing: the bytes so far. Reading: the bytes to read from.
    void SetData(const std::string *data) { _in = data; _inPos = 0; }
    std::string &Out() { return _out; }
    std::vector<FieldInfo> &Fields() { return _fields; }

    // Reading: the units by id, to turn ids into units
    void SetUnits(const std::map<uint32_t, NC_STACK_ypabact *> *units) { _units = units; }

    void Field(const char *name, bool &v, Role role = INTERNAL);
    void Field(const char *name, int8_t &v, Role role = INTERNAL);
    void Field(const char *name, uint8_t &v, Role role = INTERNAL);
    void Field(const char *name, int16_t &v, Role role = INTERNAL);
    void Field(const char *name, uint16_t &v, Role role = INTERNAL);
    void Field(const char *name, int32_t &v, Role role = INTERNAL);
    void Field(const char *name, uint32_t &v, Role role = INTERNAL);
    void Field(const char *name, float &v, Role role = INTERNAL);
    void Field(const char *name, double &v, Role role = INTERNAL);
    void Field(const char *name, vec2d &v, Role role = INTERNAL);
    void Field(const char *name, vec3d &v, Role role = INTERNAL);
    void Field(const char *name, mat3x3 &v, Role role = INTERNAL);
    void Field(const char *name, Common::Point &v, Role role = INTERNAL);
    void Field(const char *name, vec3d *v, size_t count, Role role = INTERNAL);
    void Field(const char *name, int32_t *v, size_t count, Role role = INTERNAL);
    void Field(const char *name, int16_t *v, size_t count, Role role = INTERNAL);
    void Field(const char *name, uint32_t *v, size_t count, Role role = INTERNAL);

    // Lists
    void Field(const char *name, std::vector<uint8_t> &v, Role role = INTERNAL);
    void Field(const char *name, std::vector<int32_t> &v, Role role = INTERNAL);
    void Field(const char *name, std::vector<vec3d> &v, Role role = INTERNAL);
    void Units(const char *name, std::vector<NC_STACK_ypabact *> &v, Role role = INTERNAL);

    // A unit it refers to
    void Unit(const char *name, NC_STACK_ypabact *&unit, Role role = INTERNAL);

    template <class U>
    void Unit(const char *name, U *&unit, Role role = INTERNAL)
    {
        NC_STACK_ypabact *u = unit;
        Unit(name, u, role);

        if ( Reads(role) )
            unit = dynamic_cast<U *>(u);
    }

    // A sector it refers to
    void Sector(const char *name, cellArea *&cell, Role role = INTERNAL);

    // A target, by its type: a unit, a sector or nothing
    void Target(const char *name, uint8_t type, BactTarget &target, Role role = INTERNAL);

    // Units in a list, in its order. Reading puts them in the list, as
    // newMaster_msg would (the unit's _kidRef) or in a sector's (_cellRef).
    void Kids(const char *name, World::RefBactList &list, Role role = INTERNAL);
    void SectorUnits(const char *name, cellArea &cell, Role role = INTERNAL);
    void Missiles(const char *name, World::MissileList &list, Role role = INTERNAL);
    void Attackers(const char *typesName, const char *name, std::list<TBactAttacker> &list, Role role = INTERNAL);

    // Reading: whether all the bytes were read, and none too many
    bool ReadAll() const { return !_failed && _in && _inPos == _in->size(); }
    bool Failed() const { return _failed; }

protected:
    bool Includes(Role role) const
    {
        return _part == PART_ALL || (_part == PART_VIEW ? role != INTERNAL : role != VIEW);
    }

    bool Rounds(Role role) const { return _part == PART_VIEW && role != INTERNAL; }

    // Writing and reading a field's raw bytes, little-endian on the
    // platforms that build this
    void Raw(const char *name, Type type, uint16_t count, void *data, size_t size);
    void Doubles(const char *name, double *v, size_t count, Role role);
    void Info(const char *name, Type type, uint16_t count);
    bool Take(void *data, size_t size);
    void Put(const void *data, size_t size);

    uint16_t ListLength(const char *name, Type type, uint16_t count, size_t length);
    uint32_t UnitId(const NC_STACK_ypabact *unit) const;
    NC_STACK_ypabact *UnitById(uint32_t id) const;

    Mode _mode;
    Part _part;
    NC_STACK_ypaworld *_world;

    std::string _out;
    const std::string *_in = NULL;
    size_t _inPos = 0;
    bool _failed = false;

    std::vector<FieldInfo> _fields;
    const std::map<uint32_t, NC_STACK_ypabact *> *_units = NULL;
};

// Every object of a part of the state now, in the order of an Image, to a
// function that gets its kind, id and fields' bytes (valid until it
// returns). Objects with no fields in the part are left out; with the part's
// schema, kinds it doesn't have aren't gone through at all.
typedef std::function<void(uint8_t kind, uint32_t id, const std::string &data)> ObjectFunc;
void ForEachObject(NC_STACK_ypaworld *world, Part part, const ObjectFunc &func, const Schema *schema = NULL);

// A part of the world's state now
Image Capture(NC_STACK_ypaworld *world, Part part = PART_ALL);

// The fields of every kind of object in this build, in a part
Schema MakeSchema(NC_STACK_ypaworld *world, Part part = PART_ALL);

// Sets up the level the images are of, in the state they hold: the whole
// state, or its view and internal part. The campaign's state too, with
// campaign. False, after logging why, when it can't; the level is then
// gone.
bool Restore(NC_STACK_ypaworld *world, const Image &image);
bool Restore(NC_STACK_ypaworld *world, const Image &view, const Image &internal, bool campaign);

// The same for the level being played, in place, a slice at a time while
// the game shows something else, as going back does: what the level is made
// of (its set, sky, maps, fillers and interface's art) stays, and only what
// the state holds is read back, not the campaign's. Until it's done the
// level is neither whole nor to be stepped, drawn from or saved.
class InPlaceRestore
{
public:
    InPlaceRestore(NC_STACK_ypaworld *world, Image &&view, Image &&internal);

    // Goes on for about that many milliseconds. True once done, then failed
    // or not (after logging why; the level is then not whole).
    bool Work(double ms);
    bool Failed() const { return _failed; }

protected:
    enum Phase
    {
        CLEAR,      // What isn't kept goes, but the units
        FREE_UNITS, // A few at a time
        MAKE_UNITS, // Every unit of the images
        READ,       // Objects a few hundred at a time
        FINISH,     // What's worked out from the state
        DONE,
    };

    std::vector<std::pair<Part, const Image *>> Parts() const;

    NC_STACK_ypaworld *_world;
    Image _view, _internal;
    Phase _phase = CLEAR;
    std::map<uint32_t, NC_STACK_ypabact *> _units;
    size_t _part = 0, _object = 0;
    bool _failed = false;
};

// A hash of the whole state
uint64_t Hash(const Image &image);

// An image in one string, zlib-packed, and back
std::string Pack(const Image &image);
bool Unpack(const std::string &packed, Image &image);

// The size of an element of a type
size_t ElemSize(Type type);

// The size of a field at data, which has size bytes left; 0 when it doesn't
// fit
size_t FieldSize(const FieldInfo &field, const char *data, size_t size);

// Where each field of an object's bytes starts, and then their end: one more
// offset than fields. False when the bytes don't fit the fields.
bool FieldOffsets(const std::vector<FieldInfo> &fields, const std::string &data, std::vector<size_t> &offsets);

// A schema as bytes, as files hold it, and back: u16 kinds, each a u8 kind
// and u16 fields, each a u8 type, u16 count, u8 1 for a list, u8 name length
// and the name
std::string WriteSchema(const Schema &schema);
bool ReadSchema(const std::string &bytes, Schema &schema);

// Where two images differ, field by field, as lines of text; empty when they
// are the same. At most maxLines lines.
std::string Diff(const Schema &schema, const Image &a, const Image &b, size_t maxLines = 40);

}
}

#endif
