#include <algorithm>
#include <cmath>

#include "yw_viewdist.h"
#include "yw.h"
#include "font.h"
#include "fmtlib/printf.h"

static const std::array<std::string, 7> RowLabels = {
    "Distance", "Sectors", "Clip", "Fog start", "Fog end", "Fade start", "Fade end"
};

void ViewDistanceWindow::Create(NC_STACK_ypaworld *yw)
{
    // The text is drawn in the screen font; the tile font's letters are
    // only good for its capitals.
    int margin = GFX::Engine.GetTextWidth("  ");

    _padding = yw->_fontH / 8;

    _labelWidth = 0;
    for (const std::string &label : RowLabels)
        _labelWidth = std::max(_labelWidth, GFX::Engine.GetTextWidth(label));
    _labelWidth += 2 * margin;

    _sliderWidth = 15 * yw->_fontH;
    _valueWidth = GFX::Engine.GetTextWidth("unlimited") + 2 * margin;
    _knobWidth = yw->_guiTiles[11]->h;
    _buttonWidth = std::max(GFX::Engine.GetTextWidth("Unlimited") + 4 * margin, yw->_guiTiles[0]->GetWidth("WWWWWWW"));

    GuiList::tInit args;
    args.title = "View distance";
    args.resizeable = false;
    args.numEntries = ROW_MAX + 1; // The sliders, then a row of buttons
    args.shownEntries = ROW_MAX + 1;
    args.maxShownEntries = ROW_MAX + 1;
    args.minShownEntries = ROW_MAX + 1;
    args.entryHeight = yw->_fontH + 2 * _padding;
    args.entryWidth = 2 * yw->_fontBorderW + _labelWidth + _sliderWidth + _valueWidth;
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.staticItems = true;
    args.closeChar = 85;
    args.withHelp = false;

    if ( !Init(yw, args) )
        return;

    buttons.resize(BTN_MAX);

    int sliderX = yw->_fontBorderW + _labelWidth;
    int rowsTop = yw->_fontH + upperVborder; // Below the title bar

    for (int row = 0; row < ROW_MAX; row++)
        buttons[BTN_SLIDERS + row] = ButtonBox(sliderX, rowsTop + row * entryHeight, _sliderWidth, entryHeight);

    int buttonsY = rowsTop + ROW_MAX * entryHeight + _padding;

    buttons[BTN_DEFAULT] = ButtonBox(sliderX, buttonsY, _buttonWidth, yw->_fontH);
    buttons[BTN_UNLIMITED] = ButtonBox(sliderX + _sliderWidth - _buttonWidth, buttonsY, _buttonWidth, yw->_fontH);
}

void ViewDistanceWindow::Open(NC_STACK_ypaworld *yw, int top)
{
    // Finer steps where small changes show, and only as far as the map
    // reaches.
    _distances.clear();

    float mapDiagonal = std::max(yw->GetMapDiagonal(), 5000.0f);

    for (float dist = 200.0; dist < mapDiagonal; )
    {
        _distances.push_back(dist);

        if ( dist < 2000.0 )
            dist += 100.0;
        else if ( dist < 5000.0 )
            dist += 250.0;
        else if ( dist < 10000.0 )
            dist += 500.0;
        else if ( dist < 20000.0 )
            dist += 1000.0;
        else
            dist += 2500.0;
    }

    _distances.push_back(TViewDistance::UNLIMITED);

    if ( !_placed )
    {
        SetRect(yw, yw->_screenSize.x - w - 8, top);
        _placed = true;
    }
    else
    {
        SetRect(yw, -2, -2);
    }

    UpdateSliders(yw);

    yw->GuiWinOpen(this);
    Attach(yw->_guiActive);

    Formate(yw);
    LayoutItems(yw, -1);
}

void ViewDistanceWindow::Close(NC_STACK_ypaworld *yw)
{
    StopDragging(yw);
    yw->GuiWinClose(this);
}

void ViewDistanceWindow::HandleInput(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    // The close button and the title bar are handled with the other
    // windows, in HandleGuiInput.
    if ( IsClosed() )
    {
        StopDragging(yw);
        return;
    }

    TClickBoxInf *winpt = &inpt->ClickInf;
    int pressedBtn = -1;

    if ( winpt->selected_btn == this )
    {
        int btn = winpt->selected_btnID;

        if ( btn >= BTN_SLIDERS && btn < BTN_SLIDERS + ROW_MAX && (winpt->flag & TClickBoxInf::FLAG_BTN_DOWN) )
        {
            int row = btn - BTN_SLIDERS;
            int knobCenter = KnobLeft(row) + _knobWidth / 2;
            int mouseX = winpt->ldw_pos.BtnPos.x;

            // Grabbing the knob keeps it under the mouse where it was
            // grabbed; anywhere else on the bar the knob jumps there.
            if ( std::abs(mouseX - knobCenter) <= _knobWidth / 2 )
                _grabOffset = mouseX - knobCenter;
            else
                _grabOffset = 0;

            _dragRow = row;
        }

        if ( btn == BTN_DEFAULT || btn == BTN_UNLIMITED )
        {
            if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                pressedBtn = btn;

            if ( (winpt->flag & TClickBoxInf::FLAG_BTN_DOWN) && yw->_GameShell )
                SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);

            if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
            {
                if ( btn == BTN_DEFAULT )
                {
                    yw->ResetViewDistance();
                    UpdateSliders(yw);
                }
                else
                {
                    ApplySlider(yw, ROW_VIEW, _distances.size() - 1);
                }
            }
        }
    }

    if ( _dragRow >= 0 )
    {
        if ( winpt->flag & (TClickBoxInf::FLAG_LM_DOWN | TClickBoxInf::FLAG_LM_HOLD) )
        {
            const ButtonBox &box = buttons[BTN_SLIDERS + _dragRow];
            int pos = SliderPosAt(_dragRow, winpt->move.ScreenPos.x - (x + box.x) - _grabOffset);

            if ( pos != _sliderPos[_dragRow] )
                ApplySlider(yw, _dragRow, pos);

            // The normal mouse pointer, and nothing done in the world,
            // wherever the mouse takes the knob.
            yw->_guiDragDefaultMouse = true;
        }
        else
        {
            StopDragging(yw);
        }
    }

    strncpy(title, yw->IsViewDistanceSet() ? "View distance" : "View distance (default)", sizeof(title) - 1);

    Formate(yw);
    LayoutItems(yw, pressedBtn);
}

void ViewDistanceWindow::StopDragging(NC_STACK_ypaworld *yw)
{
    if ( _dragRow >= 0 )
    {
        _dragRow = -1;
        yw->_guiDragDefaultMouse = false;
    }
}

void ViewDistanceWindow::UpdateSliders(NC_STACK_ypaworld *yw)
{
    TViewDistance view = yw->GetViewDistance();
    int mapRadius = yw->GetMapSectorRadius();

    _sliderCount[ROW_VIEW]     = _distances.size();
    _sliderCount[ROW_SECTORS]  = mapRadius + 1; // The last one draws them all
    _sliderCount[ROW_CLIP]     = _distances.size();
    _sliderCount[ROW_FOGSTART] = _distances.size() - 1; // A fog can't start at infinity
    _sliderCount[ROW_FOGEND]   = _distances.size();
    _sliderCount[ROW_FADESTART] = _distances.size() - 1;
    _sliderCount[ROW_FADEEND]   = _distances.size();

    std::array<int, ROW_MAX> pos;
    pos[ROW_VIEW]     = DistancePos(view.FogEnd);
    pos[ROW_SECTORS]  = std::min(view.SectorRadius, mapRadius);
    pos[ROW_CLIP]     = DistancePos(view.ClipDistance);
    pos[ROW_FOGSTART] = DistancePos(view.FogStart);
    pos[ROW_FOGEND]   = DistancePos(view.FogEnd);
    pos[ROW_FADESTART] = DistancePos(view.FadeOutStart);
    pos[ROW_FADEEND]   = DistancePos(view.FadeOutEnd);

    // The slider being dragged stays where the mouse put it.
    for (int row = 0; row < ROW_MAX; row++)
    {
        if ( row != _dragRow )
            _sliderPos[row] = std::min(pos[row], _sliderCount[row] - 1);
    }
}

void ViewDistanceWindow::ApplySlider(NC_STACK_ypaworld *yw, int row, int pos)
{
    TViewDistance view = yw->GetViewDistance();
    float dist = _distances.at(pos);

    _sliderPos[row] = pos;

    switch ( row )
    {
    case ROW_VIEW:
        if ( dist == TViewDistance::UNLIMITED )
        {
            view.SectorRadius = TViewDistance::ALL_SECTORS;
            view.ClipDistance = TViewDistance::UNLIMITED;
            view.FogEnd = TViewDistance::UNLIMITED;
            view.FadeOutEnd = TViewDistance::UNLIMITED;
        }
        else
        {
            // Fog and fading over the same shares of the distance as by
            // default. The sectors reach the distance at the corners of the
            // screen too, where it lies some way to the side.
            TViewDistance def = yw->GetDefaultViewDistance();

            view.SectorRadius = static_cast<int32_t>(std::ceil(2.0 * dist / World::CVSectorLength)) + 1;
            view.ClipDistance = dist;
            view.FogStart = (def.FogEnd > 0.0) ? dist * def.FogStart / def.FogEnd : 0.0;
            view.FogEnd = dist;
            view.FadeOutStart = dist * def.FadeOutStart / def.FadeOutEnd;
            view.FadeOutEnd = dist;
        }
        break;

    case ROW_SECTORS:
        if ( pos >= yw->GetMapSectorRadius() )
            view.SectorRadius = TViewDistance::ALL_SECTORS;
        else
            view.SectorRadius = pos;
        break;

    case ROW_CLIP:
        view.ClipDistance = dist;
        break;

    // Each fog distance pushes the other one along when they would cross.
    case ROW_FOGSTART:
        view.FogStart = dist;
        if ( view.FogEnd <= dist )
            view.FogEnd = _distances.at(pos + 1);
        break;

    case ROW_FOGEND:
        view.FogEnd = dist;
        if ( view.FogStart >= dist )
            view.FogStart = (pos > 0) ? _distances.at(pos - 1) : 0.0;
        break;

    // And so does each fading distance.
    case ROW_FADESTART:
        view.FadeOutStart = dist;
        if ( view.FadeOutEnd <= dist )
            view.FadeOutEnd = _distances.at(pos + 1);
        break;

    case ROW_FADEEND:
        view.FadeOutEnd = dist;
        if ( view.FadeOutStart >= dist )
            view.FadeOutStart = (pos > 0) ? _distances.at(pos - 1) : 0.0;
        break;

    default:
        return;
    }

    yw->SetViewDistance(view);
    UpdateSliders(yw);
}

void ViewDistanceWindow::LayoutItems(NC_STACK_ypaworld *yw, int pressedBtn)
{
    TViewDistance view = yw->GetViewDistance();
    TileMap *font = yw->_guiTiles[0];
    const SDL_Color &textColor = yw->_iniColors[60];

    itemBlock.clear();
    ItemsPreLayout(yw, &itemBlock, 0, "{ }");

    // Each row: its name, the bar the slider is drawn over afterwards, and
    // its value.
    for (int row = 0; row < ROW_MAX; row++)
    {
        LayoutPadLine(yw);

        FontUA::set_txtColor(&itemBlock, textColor.r, textColor.g, textColor.b);
        FontUA::store_u8(&itemBlock, '{');
        FontUA::FormateClippedText(font, &itemBlock, RowLabels[row], _labelWidth, ' ');
        FontUA::op17(&itemBlock, yw->_fontBorderW + _labelWidth + _sliderWidth);
        FontUA::store_u8(&itemBlock, ' ');
        FontUA::FormateClippedText(font, &itemBlock, ValueText(yw, row, view), _valueWidth, ' ');
        FontUA::store_u8(&itemBlock, '}');
        FontUA::next_line(&itemBlock);

        LayoutPadLine(yw);
    }

    LayoutPadLine(yw);

    FontUA::store_u8(&itemBlock, '{');
    FontUA::op17(&itemBlock, buttons[BTN_DEFAULT].x);
    FontUA::store_u8(&itemBlock, ' ');
    LayoutButton(yw, BTN_DEFAULT, pressedBtn == BTN_DEFAULT, "Default");
    FontUA::op17(&itemBlock, buttons[BTN_UNLIMITED].x);
    FontUA::store_u8(&itemBlock, ' ');
    LayoutButton(yw, BTN_UNLIMITED, pressedBtn == BTN_UNLIMITED, "Unlimited");
    FontUA::op17(&itemBlock, entryWidth - yw->_fontBorderW);
    FontUA::store_u8(&itemBlock, ' ');
    FontUA::store_u8(&itemBlock, '}');
    FontUA::next_line(&itemBlock);

    LayoutPadLine(yw);

    ItemsPostLayout(yw, &itemBlock, 0, "xyz");

    for (int row = 0; row < ROW_MAX; row++)
        LayoutSlider(yw, row);

    FontUA::set_end(&itemBlock);
}

void ViewDistanceWindow::LayoutPadLine(NC_STACK_ypaworld *yw)
{
    if ( _padding <= 0 )
        return;

    FontUA::set_yheight(&itemBlock, _padding);
    FontUA::store_u8(&itemBlock, '{');
    FontUA::op17(&itemBlock, entryWidth - yw->_fontBorderW);
    FontUA::store_u8(&itemBlock, ' ');
    FontUA::store_u8(&itemBlock, '}');
    FontUA::next_line(&itemBlock);
}

// Drawn the way the exit menu draws its buttons.
void ViewDistanceWindow::LayoutButton(NC_STACK_ypaworld *yw, int btn, bool pressed, const std::string &label)
{
    int tileset = pressed ? 8 : 3;
    TileMap *tiles = yw->_guiTiles[tileset];
    const SDL_Color &textColor = yw->_iniColors[68];
    const ButtonBox &box = buttons[btn];

    FontUA::set_txtColor(&itemBlock, textColor.r, textColor.g, textColor.b);
    FontUA::select_tileset(&itemBlock, tileset);
    FontUA::store_u8(&itemBlock, 'b');

    if ( pressed )
        FontUA::set_flag(&itemBlock, 0x10);

    FontUA::FormateCenteredSkipableItem(tiles, &itemBlock, label, box.w - tiles->map['b'].w - tiles->map['d'].w);
    FontUA::op17(&itemBlock, box.x + box.w - tiles->map['d'].w);
    FontUA::store_u8(&itemBlock, ' ');

    FontUA::unset_flag(&itemBlock, 0x10);

    FontUA::store_u8(&itemBlock, 'd');
    FontUA::select_tileset(&itemBlock, 0);
}

// Drawn over its row with the map's horizontal scroller, like the map's
// own scroll bar.
void ViewDistanceWindow::LayoutSlider(NC_STACK_ypaworld *yw, int row)
{
    TileMap *gfx = yw->_guiTiles[11];
    const ButtonBox &box = buttons[BTN_SLIDERS + row];

    FontUA::reset_tileset(&itemBlock, 11);
    FontUA::set_center_xpos(&itemBlock, x + box.x - (yw->_screenSize.x / 2));
    FontUA::set_center_ypos(&itemBlock, y + box.y + (box.h - gfx->h) / 2 - (yw->_screenSize.y / 2));

    int knobLeft = KnobLeft(row);
    int knobRight = knobLeft + _knobWidth;

    if ( knobLeft > 0 )
    {
        FontUA::store_u8(&itemBlock, 'A');

        if ( knobLeft > gfx->GetWidth('A') )
        {
            FontUA::op17(&itemBlock, knobLeft);
            FontUA::store_u8(&itemBlock, 'B');
        }
    }

    FontUA::store_u8(&itemBlock, 'D');
    FontUA::op17(&itemBlock, knobRight - gfx->GetWidth('F'));
    FontUA::store_u8(&itemBlock, 'E');
    FontUA::store_u8(&itemBlock, 'F');

    if ( knobRight < _sliderWidth )
    {
        if ( knobRight < _sliderWidth - gfx->GetWidth('C') )
        {
            FontUA::op17(&itemBlock, _sliderWidth - gfx->GetWidth('C'));
            FontUA::store_u8(&itemBlock, 'B');
        }

        FontUA::store_u8(&itemBlock, 'C');
    }
}

int ViewDistanceWindow::KnobLeft(int row) const
{
    if ( _sliderCount[row] < 2 )
        return 0;

    return (_sliderWidth - _knobWidth) * _sliderPos[row] / (_sliderCount[row] - 1);
}

int ViewDistanceWindow::SliderPosAt(int row, int knobCenter) const
{
    int travel = _sliderWidth - _knobWidth;
    if ( _sliderCount[row] < 2 || travel <= 0 )
        return 0;

    int knobLeft = knobCenter - _knobWidth / 2;
    int pos = (knobLeft * (_sliderCount[row] - 1) + travel / 2) / travel;

    return std::min(std::max(pos, 0), _sliderCount[row] - 1);
}

int ViewDistanceWindow::DistancePos(float distance) const
{
    if ( distance == TViewDistance::UNLIMITED )
        return _distances.size() - 1;

    // The nearest of the finite steps.
    int best = 0;
    for (size_t i = 1; i < _distances.size() - 1; i++)
    {
        if ( std::fabs(_distances[i] - distance) < std::fabs(_distances[best] - distance) )
            best = i;
    }
    return best;
}

std::string ViewDistanceWindow::ValueText(NC_STACK_ypaworld *yw, int row, const TViewDistance &view) const
{
    bool noFog = (view.FogEnd == TViewDistance::UNLIMITED);
    bool noFade = (view.FadeOutEnd == TViewDistance::UNLIMITED);

    switch ( row )
    {
    case ROW_VIEW:
        return noFog ? "unlimited" : fmt::sprintf("%.0f", view.FogEnd);

    case ROW_SECTORS:
        if ( view.SectorRadius >= yw->GetMapSectorRadius() )
            return "all";
        return fmt::sprintf("%d", view.SectorRadius);

    case ROW_CLIP:
        if ( view.ClipDistance == TViewDistance::UNLIMITED )
            return "none";
        return fmt::sprintf("%.0f", view.ClipDistance);

    case ROW_FOGSTART:
        return noFog ? "-" : fmt::sprintf("%.0f", view.FogStart);

    case ROW_FOGEND:
        return noFog ? "no fog" : fmt::sprintf("%.0f", view.FogEnd);

    case ROW_FADESTART:
        return noFade ? "-" : fmt::sprintf("%.0f", view.FadeOutStart);

    case ROW_FADEEND:
        return noFade ? "no fading" : fmt::sprintf("%.0f", view.FadeOutEnd);

    default:
        return std::string();
    }
}
