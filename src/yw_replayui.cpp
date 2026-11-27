#include <algorithm>
#include <cmath>
#include <map>

#include "includes.h"
#include "yw.h"
#include "yw_internal.h"
#include "yw_replayui.h"
#include "font.h"
#include "world/frame.h"
#include "ypagun.h"

// The replay viewer's windows: each faction's units and its host stations'
// energy, and the map. Everything they show comes from the frame drawn.

static std::string FactionName(uint8_t owner)
{
    switch ( owner )
    {
    case 1:  return Locale::Text::Advanced(Locale::ADV_RESIST);
    case 2:  return Locale::Text::Advanced(Locale::ADV_SULG);
    case 3:  return Locale::Text::Advanced(Locale::ADV_MYKO);
    case 4:  return Locale::Text::Advanced(Locale::ADV_TAER);
    case 5:  return Locale::Text::Advanced(Locale::ADV_BLKSCT);
    case 6:  return Locale::Text::Advanced(Locale::ADV_GHOR);
    default: return Locale::Text::Advanced(Locale::ADV_NEUTRAL);
    }
}

std::string ReplayTime(int32_t ticks)
{
    int seconds = std::max(ticks, 0) / 1024;

    if ( seconds >= 3600 )
        return fmt::sprintf("%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);

    return fmt::sprintf("%d:%02d", seconds / 60, seconds % 60);
}

void FillScreenBox(const NC_STACK_ypaworld *yw, int x, int y, int w, int h)
{
    int cx = yw->_screenSize.x / 2;
    int cy = yw->_screenSize.y / 2;

    for (int row = y; row < y + h; row++)
        GFX::Engine.DrawLineCentered(Common::Line(x - cx, row - cy, x + w - 1 - cx, row - cy));
}

// The units of a frame by who they belong to: host stations' commanders and
// commanders' squads, in the order of their ids, as far as the squad manager
// lists them: neither guns nor missiles, none dead or beaming, and of a
// squad's units none still being made
struct FrameSquads
{
    std::map<uint32_t, std::vector<size_t>> Kids; // By parent, indices of Units

    explicit FrameSquads(const World::Frame &frame)
    {
        for (size_t i = 0; i < frame.Units.size(); i++)
        {
            const World::Frame::Unit &u = frame.Units[i];
            const World::Frame::UnitInfo *info = frame.InfoAt(i);

            if ( !info || !info->Parent || u.Type == BACT_TYPES_GUN || u.Type == BACT_TYPES_MISSLE ||
                 u.Type == BACT_TYPES_ROBO )
                continue;

            const World::Frame::Unit *parent = frame.FindUnit(info->Parent);
            bool commander = parent && parent->Type == BACT_TYPES_ROBO;

            if ( info->Status == BACT_STATUS_DEAD || info->Status == BACT_STATUS_BEAM ||
                 (info->Status == BACT_STATUS_CREATE && !commander) )
                continue;

            Kids[info->Parent].push_back(i);
        }
    }

    const std::vector<size_t> &Of(uint32_t parent) const
    {
        static const std::vector<size_t> none;
        auto it = Kids.find(parent);
        return it == Kids.end() ? none : it->second;
    }
};

// What each faction has of the sectors and their power stations, for the
// share of the power under its host stations it gets
static std::array<float, 8> ReloadRatios(const World::Frame &frame)
{
    std::array<int, 8> sectors {}, power {};

    if ( frame.Sectors )
    {
        for (const World::Frame::Sector &s : *frame.Sectors)
        {
            if ( s.Owner >= 8 )
                continue;

            sectors[s.Owner]++;

            if ( s.StationPower > 0 )
                power[s.Owner] += s.StationPower;
        }
    }

    std::array<float, 8> ratios;
    for (size_t i = 0; i < 8; i++)
        ratios[i] = std::min(NC_STACK_ypaworld::ReloadRatio(sectors[i], power[i]), 1.0f);

    return ratios;
}


// A faction's window

void ReplayFactionWindow::Create(NC_STACK_ypaworld *yw, uint8_t owner)
{
    auto &fnt28 = yw->_guiTiles[28]->map;
    TileMap *font = yw->_guiTiles[0];

    _owner = owner;
    _iconWidth = fnt28[65].w;
    _cmdrIconX = fnt28[64].w + fnt28[97].w + yw->_fontBorderW;
    _kidIconsX = fnt28[64].w + _iconWidth + _cmdrIconX;

    _labelWidth = 0;
    for (int id : {Locale::HUDSTR_SYSTEM, Locale::HUDSTR_CREATE, Locale::HUDSTR_BEAM, Locale::HUDSTR_RELOAD,
                   Locale::HUDSTR_VEHICLES, Locale::HUDSTR_BUILDINGS})
        _labelWidth = std::max(_labelWidth, font->GetWidth(Locale::Text::HUD(id) + " "));

    _valueWidth = font->GetWidth(" +0000  (100%)");

    int width = std::max(_kidIconsX + 8 * _iconWidth, _labelWidth + 6 * font->map['0'].w + _valueWidth) + 2 * yw->_fontBorderW;

    GuiList::tInit args;
    args.title = FactionName(owner);
    args.resizeable = true;
    args.numEntries = 0;
    args.shownEntries = 10;
    args.maxShownEntries = 48;
    args.minShownEntries = 3;
    args.entryHeight = yw->_fontH;
    args.entryWidth = width;
    args.minEntryWidth = _labelWidth + _valueWidth + 4 * font->map['0'].w + 2 * yw->_fontBorderW;
    args.maxEntryWidth = 32000;
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.withHelp = false;

    Init(yw, args);
}

void ReplayFactionWindow::Update(NC_STACK_ypaworld *yw, const World::Frame &frame, uint32_t viewed)
{
    FrameSquads squads(frame);
    std::array<float, 8> ratios = ReloadRatios(frame);

    _rows.clear();

    for (size_t i = 0; i < frame.Units.size(); i++)
    {
        const World::Frame::Unit &robo = frame.Units[i];
        const World::Frame::UnitInfo *info = frame.InfoAt(i);

        if ( robo.Type != BACT_TYPES_ROBO || !info || info->Owner != _owner )
            continue;

        Row host;
        host.RowKind = Row::HOST;
        host.Units.push_back(robo.Id);
        _rows.push_back(host);

        // Its batteries, as the energy panel shows them: the player's, or
        // what another faction's builds with
        const World::Frame::HostStation *h = frame.FindHostStation(robo.Id);
        int max = std::max(info->EnergyMax, 1);

        auto battery = [this, max, &robo](int label, int energy)
        {
            Row row;
            row.RowKind = Row::BATTERY;
            row.Units.push_back(robo.Id);
            row.Label = Locale::Text::HUD(label);
            row.Value = fmt::sprintf("%d", energy / 100);
            row.Fill = std::min(std::max((float)energy / max, 0.0f), 1.0f);
            _rows.push_back(row);
        };

        battery(Locale::HUDSTR_SYSTEM, info->Energy);

        // Recordings from before the batteries were recorded have only that
        if ( h && robo.Id == frame.UserRobo )
        {
            battery(Locale::HUDSTR_CREATE, h->Create);
            battery(Locale::HUDSTR_BEAM, h->Beam);
        }
        else if ( h )
        {
            battery(Locale::HUDSTR_VEHICLES, h->VehicleSpare);
            battery(Locale::HUDSTR_BUILDINGS, h->BuildSpare);
        }

        // What it takes in: the reload gauge's figures, of the power of
        // the sector it's on
        Common::Point sector = World::PositionToSectorID(robo.Position.x, robo.Position.z);

        const World::Frame::Sector *under = yw->IsSector(sector) ? frame.SectorAt(sector.y * yw->_mapSize.x + sector.x) : NULL;

        if ( h && under )
        {
            const World::Frame::Sector &s = *under;
            TReloadGauge gauge = ReloadGauge(info->Owner, s.Owner, s.Power, s.Owner < 8 ? ratios[s.Owner] : 0.0, h->ReloadPS);

            Row reload;
            reload.RowKind = Row::RELOAD;
            reload.Units.push_back(robo.Id);
            reload.Label = Locale::Text::HUD(Locale::HUDSTR_RELOAD);
            reload.Value = gauge.Label;
            reload.Fill = s.Power / 255.0;
            _rows.push_back(reload);
        }

        for (size_t c : squads.Of(robo.Id))
        {
            Row squad;
            squad.RowKind = Row::SQUAD;
            squad.Units.push_back(frame.Units[c].Id);

            for (size_t k : squads.Of(frame.Units[c].Id))
                squad.Units.push_back(frame.Units[k].Id);

            _rows.push_back(squad);
        }
    }

    // None left
    if ( _rows.empty() )
    {
        Row gone;
        gone.RowKind = Row::RELOAD;
        gone.Label = Locale::Text::Common(Locale::CMN_HSDESTR);
        _rows.push_back(gone);
    }

    numEntries = _rows.size();

    // As tall as its rows, until resized
    if ( listFlags & GLIST_FLAG_IN_RESIZING )
        _sized = true;

    if ( !_sized )
    {
        int rows = std::max((int)minShownEntries, std::min((int)numEntries, (int)maxShownEntries));
        int room = (yw->_screenSize.y - yw->_downScreenBorder - y - yw->_fontH - upperVborder - lowerVborder) / entryHeight;

        rows = std::max((int)minShownEntries, std::min(rows, room));

        if ( rows != shownEntries )
        {
            shownEntries = rows;
            SetRect(yw, -2, -2);
        }
    }

    if ( firstShownEntries + shownEntries > numEntries )
        firstShownEntries = std::max(0, numEntries - shownEntries);

    Formate(yw);

    // The rows shown, each as the squad manager lays out its own
    itemBlock.clear();
    _bars.clear();

    ItemsPreLayout(yw, &itemBlock, 0, "{ }");

    int rowsTop = y + upperVborder + yw->_fontH;

    for (int i = 0; i < shownEntries; i++)
    {
        int index = firstShownEntries + i;

        if ( index >= (int)_rows.size() )
        {
            FontUA::select_tileset(&itemBlock, 0);
            FontUA::store_u8(&itemBlock, '{');
            FontUA::op17(&itemBlock, entryWidth - yw->_fontBorderW);
            FontUA::store_u8(&itemBlock, ' ');
            FontUA::store_u8(&itemBlock, '}');
            FontUA::next_line(&itemBlock);
            continue;
        }

        const Row &row = _rows[index];

        switch ( row.RowKind )
        {
        case Row::HOST:
            LayoutHost(yw, frame, row, viewed);
            break;

        case Row::SQUAD:
            LayoutSquad(yw, frame, row, viewed);
            break;

        default:
            LayoutText(yw, row, rowsTop + i * entryHeight);
            break;
        }
    }

    ItemsPostLayout(yw, &itemBlock, 0, "xyz");
    FontUA::set_end(&itemBlock);
}

// The host station's icon and name
void ReplayFactionWindow::LayoutHost(NC_STACK_ypaworld *yw, const World::Frame &frame, const Row &row, uint32_t viewed)
{
    const World::Frame::Unit *robo = frame.FindUnit(row.Units[0]);
    const SDL_Color &text = yw->_iniColors[60];
    bool blink = yw->_replayClock / 300 & 1;

    FontUA::select_tileset(&itemBlock, 0);
    FontUA::store_u8(&itemBlock, '{');
    FontUA::op17(&itemBlock, entryWidth - yw->_fontBorderW);
    FontUA::store_u8(&itemBlock, ' ');
    FontUA::store_u8(&itemBlock, '}');
    FontUA::add_xpos(&itemBlock, -(entryWidth - 2 * yw->_fontBorderW + 1));

    FontUA::select_tileset(&itemBlock, 28);
    FontUA::store_u8(&itemBlock, robo->Id == viewed && blink ? 33 : VehicleTypeIcon(yw, robo->Vehicle));

    std::string name;
    if ( robo->Vehicle < yw->_vhclProtos.size() )
        name = yw->GetVehicleName(yw->_vhclProtos[robo->Vehicle]);

    FontUA::select_tileset(&itemBlock, 0);
    FontUA::set_txtColor(&itemBlock, text.r, text.g, text.b);
    FontUA::FormateClippedText(yw->_guiTiles[0], &itemBlock, " " + name, entryWidth - 2 * yw->_fontBorderW - _iconWidth, ' ');
    FontUA::next_line(&itemBlock);
}

// A label, a bar drawn over the row afterwards, and a value
void ReplayFactionWindow::LayoutText(NC_STACK_ypaworld *yw, const Row &row, int rowTop)
{
    TileMap *font = yw->_guiTiles[0];
    const SDL_Color &text = yw->_iniColors[61];
    int inner = entryWidth - 2 * yw->_fontBorderW;
    int barWidth = inner - _labelWidth - _valueWidth;

    FontUA::select_tileset(&itemBlock, 0);
    FontUA::set_txtColor(&itemBlock, text.r, text.g, text.b);
    FontUA::store_u8(&itemBlock, '{');

    if ( row.RowKind == Row::BATTERY || !row.Value.empty() )
    {
        FontUA::FormateClippedText(font, &itemBlock, " " + row.Label, _labelWidth, ' ');
        FontUA::op17(&itemBlock, yw->_fontBorderW + inner - _valueWidth);
        FontUA::store_u8(&itemBlock, ' ');

        std::string value = row.Value;
        int valueW = std::min(font->GetWidth(value + " "), _valueWidth);
        FontUA::op17(&itemBlock, yw->_fontBorderW + inner - valueW);
        FontUA::store_u8(&itemBlock, ' ');
        FontUA::FormateClippedText(font, &itemBlock, value + " ", valueW, ' ');

        if ( barWidth > 4 )
        {
            Bar bar;
            bar.X = x + yw->_fontBorderW + _labelWidth;
            bar.H = std::max(2, yw->_fontH / 3);
            bar.Y = rowTop + (yw->_fontH - bar.H) / 2;
            bar.W = barWidth - font->map['0'].w;
            bar.Fill = row.Fill;
            _bars.push_back(bar);
        }
    }
    else
    {
        FontUA::FormateClippedText(font, &itemBlock, " " + row.Label, inner, ' ');
    }

    FontUA::op17(&itemBlock, entryWidth - yw->_fontBorderW);
    FontUA::store_u8(&itemBlock, ' ');
    FontUA::store_u8(&itemBlock, '}');
    FontUA::next_line(&itemBlock);
}

// As the squad manager: what the squad does, its commander and its units,
// the one looked from blinking, and how much energy the one under the mouse
// has
void ReplayFactionWindow::LayoutSquad(NC_STACK_ypaworld *yw, const World::Frame &frame, const Row &row, uint32_t viewed)
{
    bool blink = yw->_replayClock / 300 & 1;
    const World::Frame::Unit *cmdr = frame.FindUnit(row.Units[0]);
    const World::Frame::UnitInfo *cmdrInfo = frame.FindInfo(row.Units[0]);
    int hoveredX = 0;
    uint8_t hoveredEnergy = 0;
    std::string units;

    for (size_t i = 0; i < row.Units.size(); i++)
    {
        const World::Frame::Unit *unit = frame.FindUnit(row.Units[i]);
        const World::Frame::UnitInfo *info = frame.FindInfo(row.Units[i]);

        if ( !unit || !info )
            continue;

        units += unit->Id == viewed && blink ? '!' : VehicleTypeIcon(yw, unit->Vehicle);

        if ( unit->Id == _hovered )
        {
            hoveredX = i == 0 ? _cmdrIconX : _kidIconsX + (int)(i - 1) * _iconWidth;
            hoveredEnergy = EnergyGlyph(info->Energy, info->EnergyMax);
        }

        if ( units.size() >= SquadRowMaxUnits )
            break;
    }

    char status = cmdr && cmdrInfo ? SquadStatusIcon(cmdrInfo->Status, cmdr->StatusFlags, cmdrInfo->PrimaryType,
                                                     cmdrInfo->SecondaryType) : 97;

    LayoutSquadRow(yw, &itemBlock, entryWidth, status, units, false, hoveredX, hoveredEnergy);
}

uint32_t ReplayFactionWindow::UnitAt(const Row &row, int px) const
{
    if ( row.Units.empty() )
        return 0;

    // A host station's rows are all of it
    if ( row.RowKind != Row::SQUAD )
        return row.Units[0];

    if ( px >= _cmdrIconX && px < _cmdrIconX + _iconWidth )
        return row.Units[0];

    if ( px >= _kidIconsX )
    {
        size_t kid = 1 + (px - _kidIconsX) / _iconWidth;
        if ( kid < row.Units.size() )
            return row.Units[kid];
    }

    return 0;
}

void ReplayFactionWindow::HandleInput(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    InputHandle(yw, inpt);

    _hovered = 0;

    if ( IsClosed() )
        return;

    TClickBoxInf &click = inpt->ClickInf;

    if ( click.selected_btn != this || click.selected_btnID < 8 )
        return;

    int index = firstShownEntries + click.selected_btnID - 8;

    if ( index >= 0 && index < (int)_rows.size() )
        _hovered = UnitAt(_rows[index], click.move.BtnPos.x);
}

void ReplayFactionWindow::DrawExtras(NC_STACK_ypaworld *yw)
{
    if ( IsClosed() )
        return;

    SDL_Color color = yw->GetColor(_owner);
    SDL_Color dark = {(uint8_t)(color.r / 4), (uint8_t)(color.g / 4), (uint8_t)(color.b / 4), 255};

    for (const Bar &bar : _bars)
    {
        int filled = (int)(bar.W * bar.Fill + 0.5);

        GFX::Engine.SetPen(dark);
        FillScreenBox(yw, bar.X, bar.Y, bar.W, bar.H);

        if ( filled > 0 )
        {
            GFX::Engine.SetPen(color);
            FillScreenBox(yw, bar.X, bar.Y, filled, bar.H);
        }
    }
}


// The frame as the game's map shows the level

void ReplayMapScene::Set(NC_STACK_ypaworld *yw, const World::Frame *frame, uint32_t viewed, const vec3d &camera,
                         const mat3x3 &rotation, uint32_t hovered)
{
    _yw = yw;
    _frame = frame;
    _viewed = viewed;
    _hovered = hovered;
    _camera = camera;
    _rotation = rotation;

    size_t count = frame->Units.size();

    // By sector and by parent, each in the order of their ids
    _firstInSector.assign(yw->_cells.size(), -1);
    _nextInSector.assign(count, -1);
    _firstKid.assign(count, -1);
    _nextKid.assign(count, -1);

    for (int32_t i = (int32_t)count - 1; i >= 0; i--)
    {
        const World::Frame::Unit &u = frame->Units[i];
        Common::Point sector = World::PositionToSectorID(u.Position.x, u.Position.z);

        if ( yw->IsSector(sector) )
        {
            size_t index = sector.y * yw->_mapSize.x + sector.x;
            _nextInSector[i] = _firstInSector[index];
            _firstInSector[index] = i;
        }

        const World::Frame::UnitInfo *info = frame->InfoAt(i);
        const World::Frame::Unit *parent = info ? frame->FindUnit(info->Parent) : NULL;

        if ( parent )
        {
            size_t p = parent - frame->Units.data();
            _nextKid[i] = _firstKid[p];
            _firstKid[p] = i;
        }
    }

    // The player: the faction of the unit looked from, or the one who
    // played, and its host station
    const World::Frame::UnitInfo *viewedInfo = frame->FindInfo(viewed);
    const World::Frame::UnitInfo *userInfo = frame->FindInfo(frame->UserRobo);

    _player = viewedInfo ? viewedInfo->Owner : (userInfo ? userInfo->Owner : 1);
    _userRobo = 0;

    for (size_t i = 0; i < count && !_userRobo; i++)
    {
        const World::Frame::UnitInfo *info = frame->InfoAt(i);

        if ( frame->Units[i].Type == BACT_TYPES_ROBO && info && info->Owner == _player )
            _userRobo = frame->Units[i].Id;
    }
}

const World::Frame::Sector *ReplayMapScene::SectorOf(const Common::Point &sector) const
{
    return _frame ? _frame->SectorAt(sector.y * _yw->_mapSize.x + sector.x) : NULL;
}

uint8_t ReplayMapScene::Owner(const Common::Point &sector) const
{
    const World::Frame::Sector *s = SectorOf(sector);
    return s ? s->Owner : _yw->_cells(sector).owner;
}

int32_t ReplayMapScene::Type(const Common::Point &sector) const
{
    const World::Frame::Sector *s = SectorOf(sector);

    if ( s && s->Type >= 0 && s->Type < (int32_t)_yw->_secTypeArray.size() )
        return s->Type;

    return _yw->_cells(sector).type_id;
}

uint8_t ReplayMapScene::SimpleType(const Common::Point &sector) const
{
    return _yw->_secTypeArray[Type(sector)].SectorType;
}

const int16_t *ReplayMapScene::Health(const Common::Point &sector) const
{
    const World::Frame::Sector *s = SectorOf(sector);

    if ( s )
    {
        for (int16_t health : s->Health)
        {
            if ( health < 0 || health >= (int)_yw->_buildHealthModelId.size() )
                return _yw->_cells(sector).buildings_health.data();
        }

        return s->Health;
    }

    return _yw->_cells(sector).buildings_health.data();
}

uint8_t ReplayMapScene::Purpose(const Common::Point &sector) const
{
    const World::Frame::Sector *s = SectorOf(sector);
    return s ? s->Purpose : _yw->_cells(sector).PurposeType;
}

MapScene::Unit ReplayMapScene::UnitAt(size_t index) const
{
    const World::Frame::Unit &f = _frame->Units[index];
    const World::Frame::UnitInfo *info = _frame->InfoAt(index);

    Unit u;
    u.Id = f.Id;
    u.Type = f.Type;
    u.Vehicle = f.Vehicle;
    u.StatusFlags = f.StatusFlags;
    u.Position = vec3d(f.Position);
    u.Sector = World::PositionToSectorID(f.Position.x, f.Position.z);
    u.Rotation = mat3x3(f.Rotation);
    u.LookRotation = u.Rotation;

    // Missiles' are weapons', and the mouse doesn't pick them
    if ( f.Type != BACT_TYPES_MISSLE && f.Vehicle < _yw->_vhclProtos.size() )
    {
        u.Radius = _yw->_vhclProtos[f.Vehicle].radius;
        u.Height = _yw->_vhclProtos[f.Vehicle].height;
    }

    if ( !info )
        return u;

    u.Owner = info->Owner;
    u.Status = info->Status;
    u.Aggression = info->Aggression;
    u.CommandId = info->CommandId;
    u.Energy = info->Energy;
    u.EnergyMax = std::max(info->EnergyMax, 1);
    u.RoboGun = f.Type == BACT_TYPES_GUN && info->GunFlags >= 0 && (info->GunFlags & NC_STACK_ypagun::GUN_FLAGS_ROBO);

    if ( const World::Frame::Unit *parent = _frame->FindUnit(info->Parent) )
    {
        u.HasParent = true;
        u.Parent = parent->Id;
        u.ParentPos = vec3d(parent->Position);
        u.Commander = parent->Type == BACT_TYPES_ROBO;
    }
    else
    {
        // A squad of a faction without host stations leads at the top level
        u.Commander = f.Type != BACT_TYPES_ROBO && f.Type != BACT_TYPES_MISSLE;
    }

    // Targets that are gone aren't shown
    if ( info->PrimaryType == BACT_TGT_TYPE_CELL )
    {
        u.PrimaryType = info->PrimaryType;
        u.PrimaryPos = vec3d(info->PrimaryPos);
        u.PrimarySector = Common::Point(info->Primary % _yw->_mapSize.x, info->Primary / _yw->_mapSize.x);
    }
    else if ( info->PrimaryType == BACT_TGT_TYPE_UNIT )
    {
        if ( const World::Frame::Unit *target = _frame->FindUnit(info->Primary) )
        {
            u.PrimaryType = info->PrimaryType;
            u.PrimaryPos = vec3d(target->Position);
        }
    }

    if ( info->SecondaryType == BACT_TGT_TYPE_CELL )
    {
        u.SecondaryType = info->SecondaryType;
        u.SecondaryPos = World::SectorIDToCenterPos3( {(int)(info->Secondary % _yw->_mapSize.x), (int)(info->Secondary / _yw->_mapSize.x)} );
    }
    else if ( info->SecondaryType == BACT_TGT_TYPE_UNIT )
    {
        if ( const World::Frame::Unit *target = _frame->FindUnit(info->Secondary) )
        {
            u.SecondaryType = info->SecondaryType;
            u.SecondaryPos = vec3d(target->Position);
        }
    }

    return u;
}

bool ReplayMapScene::Find(uint32_t id, Unit &out) const
{
    const World::Frame::Unit *unit = _frame && id ? _frame->FindUnit(id) : NULL;

    if ( !unit )
        return false;

    out = UnitAt(unit - _frame->Units.data());
    return true;
}

void ReplayMapScene::ForEachUnitIn(const Common::Point &sector, const UnitFunc &func) const
{
    if ( !_frame || !_yw->IsSector(sector) )
        return;

    for (int32_t i = _firstInSector[sector.y * _yw->_mapSize.x + sector.x]; i >= 0; i = _nextInSector[i])
        func(UnitAt(i));
}

void ReplayMapScene::ForEachKid(const Unit &unit, const UnitFunc &func) const
{
    const World::Frame::Unit *parent = _frame ? _frame->FindUnit(unit.Id) : NULL;

    if ( !parent )
        return;

    for (int32_t i = _firstKid[parent - _frame->Units.data()]; i >= 0; i = _nextKid[i])
        func(UnitAt(i));
}

bool ReplayMapScene::SquadLeader(const Unit &unit, Unit &leader) const
{
    if ( !unit.HasParent )
        return false;

    if ( unit.Commander )
    {
        leader = unit;
        return true;
    }

    return Find(unit.Parent, leader);
}

uint32_t ReplayMapScene::UserUnitId() const
{
    return _frame && _frame->FindUnit(_viewed) ? _viewed : 0;
}

// Flown freely, or once the unit looked from is gone, the camera is where
// the player is
bool ReplayMapScene::UserUnit(Unit &out) const
{
    if ( Find(_viewed, out) )
        return true;

    out = Unit();
    out.Position = _camera;
    out.Sector = World::PositionToSectorID(_camera);
    out.Rotation = _rotation;
    return true;
}

bool ReplayMapScene::ViewerPosition(vec3d &out) const
{
    Unit viewed;
    out = Find(_viewed, viewed) ? viewed.Position : _camera;
    return true;
}

void ReplayMapScene::ForEachSquad(uint8_t owner, const UnitFunc &func) const
{
    if ( !_frame )
        return;

    for (size_t i = 0; i < _frame->Units.size(); i++)
    {
        const World::Frame::UnitInfo *info = _frame->InfoAt(i);

        if ( !info || info->Owner != owner )
            continue;

        Unit unit = UnitAt(i);

        if ( unit.Commander )
            func(unit);
    }
}

void ReplayMapScene::ForEachHostStation(uint8_t owner, const UnitFunc &func) const
{
    if ( !_frame )
        return;

    for (size_t i = 0; i < _frame->Units.size(); i++)
    {
        const World::Frame::UnitInfo *info = _frame->InfoAt(i);
        const World::Frame::Unit &unit = _frame->Units[i];

        if ( unit.Type == BACT_TYPES_ROBO && info && info->Owner == owner && info->Status != BACT_STATUS_DEAD &&
             !(unit.StatusFlags & BACT_STFLAG_DEATH1) )
            func(UnitAt(i));
    }
}

void ReplayMapScene::ForEachRoboGun(const Unit &host, const UnitFunc &func) const
{
    ForEachKid(host, [&](const Unit &kid)
    {
        if ( kid.RoboGun )
            func(kid);
    });
}

bool ReplayMapScene::Defeated(uint8_t owner) const
{
    if ( !_frame )
        return false;

    for (size_t i = 0; i < _frame->Units.size(); i++)
    {
        const World::Frame::UnitInfo *info = _frame->InfoAt(i);

        if ( info && info->Owner == owner && info->Status != BACT_STATUS_DEAD )
            return false;
    }

    return true;
}

// A building being built in a sector of the faction's
bool ReplayMapScene::Constructing(uint8_t owner) const
{
    if ( !_frame )
        return false;

    for (const World::Frame::Construction &c : _frame->Constructions)
    {
        const World::Frame::Sector *s = _frame->SectorAt(c.Sector);

        if ( s && s->Owner == owner )
            return true;
    }

    return false;
}

bool ReplayMapScene::HostStationBatteries(uint32_t id, Batteries &out) const
{
    const World::Frame::HostStation *host = _frame ? _frame->FindHostStation(id) : NULL;
    const World::Frame::UnitInfo *info = _frame ? _frame->FindInfo(id) : NULL;
    const World::Frame::Unit *unit = _frame ? _frame->FindUnit(id) : NULL;

    if ( !host || !info || !unit )
        return false;

    out.FillMode = host->FillMode;
    out.Main = info->Energy;
    out.Max = std::max(info->EnergyMax, 1);
    out.Create = host->Create;
    out.Beam = host->Beam;
    out.LoadFlags = host->LoadFlags;
    out.LossFlags = host->LossFlags;
    out.AbsReload = host->ReloadPS;

    Common::Point sector = World::PositionToSectorID(unit->Position.x, unit->Position.z);

    if ( const World::Frame::Sector *s = _yw->IsSector(sector) ? SectorOf(sector) : NULL )
    {
        out.SectorOwner = s->Owner;
        out.SectorPower = s->Power;
    }

    out.ReloadRatio = ReloadRatios(*_frame)[out.SectorOwner & 7];
    return true;
}

// The player's squads, as the squad manager has them
void ReplayMapScene::ForEachCommander(const UnitFunc &func) const
{
    if ( !_frame )
        return;

    for (size_t i = 0; i < _frame->Units.size(); i++)
    {
        const World::Frame::Unit &robo = _frame->Units[i];
        const World::Frame::UnitInfo *info = _frame->InfoAt(i);

        if ( robo.Type != BACT_TYPES_ROBO || !info || info->Owner != _player )
            continue;

        for (int32_t k = _firstKid[i]; k >= 0; k = _nextKid[k])
        {
            const World::Frame::UnitInfo *kid = _frame->InfoAt(k);

            if ( _frame->Units[k].Type != BACT_TYPES_GUN && kid && kid->Status != BACT_STATUS_DEAD && kid->Status != BACT_STATUS_BEAM )
                func(UnitAt(k));
        }
    }
}

// The squad of the unit looked from
bool ReplayMapScene::SelectedCommander(Unit &out) const
{
    Unit viewed;
    return Find(_viewed, viewed) && viewed.Type != BACT_TYPES_ROBO && SquadLeader(viewed, out);
}

void ReplayMapScene::ForEachPowerStation(const std::function<void(const Common::Point &, int)> &func) const
{
    if ( !_frame || !_frame->Sectors )
        return;

    for (size_t i = 0; i < _frame->Sectors->size(); i++)
    {
        const World::Frame::Sector &s = (*_frame->Sectors)[i];

        if ( s.StationPower >= 0 )
            func(Common::Point(i % _yw->_mapSize.x, i / _yw->_mapSize.x), s.StationPower);
    }
}

void PlayMapScene::SetPlayer(NC_STACK_ypaworld *yw, const World::Frame *frame, uint32_t player, const vec3d &camera,
                             const mat3x3 &rotation)
{
    const World::Frame::Player *p = frame->FindPlayer(player);

    Set(yw, frame, p ? p->Unit : 0, camera, rotation, 0);

    if ( p )
    {
        _player = p->Owner;
        _userRobo = p->HostStation;
    }

    // What the faction sees: its sectors, and around its units
    _seen.assign(yw->_cells.size(), 0);

    if ( frame->Sectors )
    {
        for (size_t i = 0; i < frame->Sectors->size() && i < _seen.size(); i++)
            _seen[i] = (*frame->Sectors)[i].Owner == _player;
    }

    for (size_t i = 0; i < frame->Units.size(); i++)
    {
        const World::Frame::Unit &unit = frame->Units[i];
        const World::Frame::UnitInfo *info = frame->InfoAt(i);

        if ( !info || info->Owner != _player || unit.Type == BACT_TYPES_MISSLE || info->Status == BACT_STATUS_DEAD ||
             info->Status == BACT_STATUS_CREATE || unit.Vehicle >= yw->_vhclProtos.size() )
            continue;

        MarkSeen(World::PositionToSectorID(unit.Position.x, unit.Position.z), yw->_vhclProtos[unit.Vehicle].radar);
    }
}

// The sectors a unit in a sector sees with its radar, as
// NC_STACK_ypabact::MarkSectorsForView marks them
void PlayMapScene::MarkSeen(const Common::Point &at, int radar)
{
    auto mark = [this](const Common::Point &sector)
    {
        _seen[sector.y * _yw->_mapSize.x + sector.x] = 1;
    };

    for (int i = -radar; i <= radar; i++)
    {
        int yy = at.y + i;

        if ( radar == 1 )
        {
            if ( yy > 0 && yy < _yw->_mapSize.y - 1 )
            {
                if ( at.x > 1 )
                    mark({at.x - 1, yy});

                if ( _yw->IsSector({at.x, yy}) )
                    mark({at.x, yy});

                if ( at.x + 1 < _yw->_mapSize.x - 1 )
                    mark({at.x + 1, yy});
            }
        }
        else
        {
            float squared = POW2((float)radar) - POW2((float)i);
            int width = dround( sqrt(std::max(squared, 0.0f)) );

            for (int j = -width; j <= width; j++)
            {
                Common::Point sector(at.x + j, yy);

                if ( _yw->IsGamePlaySector(sector) )
                    mark(sector);
            }
        }
    }
}

int32_t PlayMapScene::ViewMask(const Common::Point &sector) const
{
    size_t index = sector.y * _yw->_mapSize.x + sector.x;

    if ( index >= _seen.size() )
        return 0;

    return _seen[index] ? cellArea::ViewMask(_player) : 0;
}

// Kept from an earlier frame: as it is in this one, if it's still there
MapScene::Unit PlayMapScene::Fresh(const Unit &unit) const
{
    Unit now;
    return Find(unit.Id, now) ? now : unit;
}

void PlayMapScene::ForEachCommander(const UnitFunc &func) const
{
    for ( const Unit &commander : _yw->_cmdrsRemap )
        func(Fresh(commander));
}

bool PlayMapScene::SelectedCommander(Unit &out) const
{
    if ( _yw->_activeCmdrRemapIndex < 0 || (size_t)_yw->_activeCmdrRemapIndex >= _yw->_cmdrsRemap.size() )
        return false;

    out = Fresh(_yw->_cmdrsRemap[_yw->_activeCmdrRemapIndex]);
    return true;
}

int ReplayMapScene::SuperItemState(size_t index) const
{
    if ( _frame && index < _frame->SuperItems.size() )
        return _frame->SuperItems[index].State;

    return _yw->_levelInfo.SuperItems[index].State;
}
