#ifndef YW_REPLAYUI_H_INCLUDED
#define YW_REPLAYUI_H_INCLUDED

#include <cstdint>
#include <string>
#include <vector>

#include "lstvw.h"
#include "yw_mapscene.h"
#include "vectors.h"
#include "matrix.h"

#include "world/frame.h"

class NC_STACK_ypaworld;
struct TInputState;

// Game time, in 1/1024 s, as minutes and seconds, or hours too
std::string ReplayTime(int32_t ticks);

// A box filled with the pen's colour, in pixels from the top left
void FillScreenBox(const NC_STACK_ypaworld *yw, int x, int y, int w, int h);

// How the replay viewer looks at the level (yw_replay.cpp)
struct ReplayCamera
{
    enum Mode
    {
        PLAYER,   // From where a player looked, and once they're gone from
                  // where they last did
        MANUAL,   // From a unit, and once it's gone from where it was last
        FREE,     // Flown about with the keys and the mouse
    };

    Mode CameraMode = PLAYER;
    uint32_t Player = 0;     // The player followed; 0 until one is
    uint32_t PlayerUnit = 0; // The unit they saw from in the frame drawn last
    uint32_t Unit = 0;       // Manual: the unit looked from, and its owner
    uint8_t UnitOwner = 0;
    vec3d Position;        // Where it was drawn from last
    mat3x3 Rotation = mat3x3::Ident();
    vec3d Velocity;        // Free: in world units a second
    double Yaw = 0.0;      // Free: turned about the up axis from looking
    double Pitch = 0.0;    // north, and up from level
    bool Looking = false;  // Free: the right mouse button held, the mouse
                           // turns it
};

// A faction's units in the replay viewer, as the squadron manager lists the
// player's: each of its host stations with its batteries and what it takes
// in, as the energy panel shows them, then its squads. Drawn from the frame
// shown; double-clicking a unit picks it to look from.
class ReplayFactionWindow : public GuiList
{
public:
    void Create(NC_STACK_ypaworld *yw, uint8_t owner);
    bool Created() const { return _owner != 0; }
    uint8_t Owner() const { return _owner; }

    bool Moved = false; // By the user: no longer stacked with the others

    // Its rows as of a frame, the unit looked from blinking among them
    void Update(NC_STACK_ypaworld *yw, const World::Frame &frame, uint32_t viewed);

    // Scrolled and resized as the game's lists, and the unit under the
    // pointer in it, 0 for none
    void HandleInput(NC_STACK_ypaworld *yw, TInputState *inpt);
    uint32_t Hovered() const { return _hovered; }

    // What its text doesn't draw: the batteries' bars
    void DrawExtras(NC_STACK_ypaworld *yw);

protected:
    struct Row
    {
        enum Kind
        {
            HOST,    // A host station: Units has it, as its batteries' rows
            BATTERY, // One of its batteries
            RELOAD,  // What it takes in
            SQUAD,   // A commander, then the units of its squad
        };

        Kind RowKind = HOST;
        std::vector<uint32_t> Units;
        std::string Label, Value;
        float Fill = 0.0;      // BATTERY: how full
    };

    void LayoutHost(NC_STACK_ypaworld *yw, const World::Frame &frame, const Row &row, uint32_t viewed);
    void LayoutText(NC_STACK_ypaworld *yw, const Row &row, int rowTop);
    void LayoutSquad(NC_STACK_ypaworld *yw, const World::Frame &frame, const Row &row, uint32_t viewed);

    // The unit of a row at a point across it, 0 for none
    uint32_t UnitAt(const Row &row, int x) const;

    struct Bar
    {
        int X = 0, Y = 0, W = 0, H = 0;
        float Fill = 0.0;
    };

    uint8_t _owner = 0;
    bool _sized = false;        // By the user: no longer as tall as its rows
    std::vector<Row> _rows;
    std::vector<Bar> _bars;     // Laid out with the rows shown
    uint32_t _hovered = 0;      // The unit under the mouse

    int _iconWidth = 0;         // Of the squad manager's icons
    int _cmdrIconX = 0;
    int _kidIconsX = 0;
    int _labelWidth = 0;        // Of the batteries' rows
    int _valueWidth = 0;
};

// A replay's frame as the game's map, its radar and the marks over units
// show a level: all of it seen, the player being the faction of the unit
// looked from, or else the one who played
class ReplayMapScene : public MapScene
{
public:
    // As of a frame, which outlives it, looked at from a unit (0 for none,
    // flown freely), from the camera there, the unit under the pointer
    // hovered
    void Set(NC_STACK_ypaworld *yw, const World::Frame *frame, uint32_t viewed, const vec3d &camera, const mat3x3 &rotation,
             uint32_t hovered);

    int32_t ViewMask(const Common::Point &sector) const override { return -1; }
    uint8_t Owner(const Common::Point &sector) const override;
    int32_t Type(const Common::Point &sector) const override;
    uint8_t SimpleType(const Common::Point &sector) const override;
    const int16_t *Health(const Common::Point &sector) const override;
    uint8_t Purpose(const Common::Point &sector) const override;

    void ForEachUnitIn(const Common::Point &sector, const UnitFunc &func) const override;
    void ForEachKid(const Unit &unit, const UnitFunc &func) const override;
    bool SquadLeader(const Unit &unit, Unit &leader) const override;

    uint8_t PlayerOwner() const override { return _player; }
    uint32_t UserRoboId() const override { return _userRobo; }
    uint32_t UserUnitId() const override;
    bool UserRobo(Unit &out) const override { return Find(_userRobo, out); }
    bool UserUnit(Unit &out) const override;
    bool ViewerPosition(vec3d &out) const override;
    mat3x3 ViewerRotation() const override { return _rotation; }
    bool Hovered(Unit &out) const override { return Find(_hovered, out); }

    void ForEachSquad(uint8_t owner, const UnitFunc &func) const override;
    void ForEachHostStation(uint8_t owner, const UnitFunc &func) const override;

    void ForEachCommander(const UnitFunc &func) const override;
    bool SelectedCommander(Unit &out) const override;
    void ForEachPowerStation(const std::function<void(const Common::Point &, int)> &func) const override;
    int SuperItemState(size_t index) const override;
    bool Interactive() const override { return false; }

    // A unit of the frame by its id, false for none
    bool Find(uint32_t id, Unit &out) const override;

protected:
    Unit UnitAt(size_t index) const;
    const World::Frame::Sector *SectorOf(const Common::Point &sector) const;

    NC_STACK_ypaworld *_yw = NULL;
    const World::Frame *_frame = NULL;
    uint32_t _viewed = 0;
    uint32_t _hovered = 0;
    uint32_t _userRobo = 0;
    uint8_t _player = 1;
    vec3d _camera;
    mat3x3 _rotation = mat3x3::Ident();

    std::vector<int32_t> _firstInSector, _nextInSector; // Units by sector, as indices of the frame's
    std::vector<int32_t> _firstKid, _nextKid;           // And by parent
};

#endif
