#include <inttypes.h>
#include "includes.h"
#include "nucleas.h"
#include "button.h"
#include "utils.h"
#include "system/gfx.h"
#include "system/inpt.h"

#include "font.h"


size_t NC_STACK_button::Init(IDVList &stak)
{
    if ( !NC_STACK_nucleus::Init(stak) )
        return 0;

    track_left_chr = 97;
    track_fill_chr = 115;
    track_right_chr = 99;

    left_chr = 97;
    right_chr = 99;
    fill_chr = 32;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case BTN_ATT_X:
                setBTN_x(val.Get<int32_t>());
                break;
            case BTN_ATT_Y:
                setBTN_y(val.Get<int32_t>());
                break;
            case BTN_ATT_W:
                setBTN_w(val.Get<int32_t>());
                break;
            case BTN_ATT_H:
                setBTN_h(val.Get<int32_t>());
                break;
            case BTN_ATT_CHARS:
                setBTN_chars( val.Get<std::string>() );
                break;

            default:
                break;
            }
        }
    }

    if ( w > 0 && h > 0 )
    {
        screen_width = GFX::Engine.GetScreenW();
        screen_height = GFX::Engine.GetScreenH();
    }
    else
    {
        Deinit();
        return 0;
    }

    return 1;
}

size_t NC_STACK_button::Deinit()
{
    for (WidgetArr::iterator it = widgets.begin(); it != widgets.end(); it++)
    {
        if (it->button_type == NC_STACK_button::TYPE_SLIDER)
            delete it->slider;
    }

    widgets.clear();
    buttons.clear();

    return NC_STACK_nucleus::Deinit();
}


// Update slider
void NC_STACK_button::UpdateSlider(WidgetEntry *sbt)
{
    Slider *slider = sbt->slider;

    if ( slider->value > slider->max )
        slider->value = slider->max;

    if ( slider->value < slider->min)
        slider->value = slider->min;

    int thumbWidth = sbt->width / (slider->max - slider->min + 1) + 1;

    if ( thumbWidth < 6 )
        thumbWidth = 6;

    slider->thumbStart = (float)((slider->value - slider->min) * (sbt->width - thumbWidth) / (slider->max - slider->min)) + 0.5;

    if ( slider->thumbStart < 0 )
        slider->thumbStart = 0;

    slider->thumbEnd = thumbWidth + slider->thumbStart;

    if ( slider->thumbEnd > sbt->width )
        slider->thumbEnd = sbt->width;


    CmdStream captionbuf;
    captionbuf.reserve(512);
    
    if ( slider->thumbStart > 0 )
    {
        if ( sbt->flags & FLAG_BORDER )
        {
            FontUA::store_u8(&captionbuf, track_left_chr);

            if ( slider->thumbStart > 1 )
            {
                FontUA::op17(&captionbuf, slider->thumbStart);
                FontUA::store_u8(&captionbuf, track_fill_chr);
            }
        }
        else
        {
            FontUA::op17(&captionbuf, slider->thumbStart);
            FontUA::store_u8(&captionbuf, track_fill_chr);
        }
    }

    FontUA::store_u8(&captionbuf, left_chr);

    FontUA::op17(&captionbuf, slider->thumbEnd);

    FontUA::store_u8(&captionbuf, fill_chr);
    FontUA::store_u8(&captionbuf, right_chr);

    if ( sbt->flags & FLAG_BORDER )
    {
        if ( slider->thumbEnd < sbt->width - 1 )
        {
            FontUA::op17(&captionbuf, sbt->width - 1);

            FontUA::store_u8(&captionbuf, track_fill_chr);
            FontUA::store_u8(&captionbuf, track_right_chr);
        }
    }
    else if ( slider->thumbEnd < sbt->width )
    {
        FontUA::op17(&captionbuf, sbt->width - 1);

        FontUA::store_u8(&captionbuf, track_fill_chr);
    }
    FontUA::set_end(&captionbuf);
    
    sbt->caption.assign(captionbuf.begin(), captionbuf.end());
}

size_t NC_STACK_button::Add(WidgetDesc *arg)
{
    /*if ( idd >= 48 )
        return 0;*/

    // TODO(bug): Entries are added before the checks below, so an early return
    // leaves buttons and widgets longer than widget_count. A slider without
    // slider data returns after widget_count++ and leaves a NULL slider that
    // UpdateSlider, Refresh and ProcessWidgetsEvents dereference.
    buttons.emplace_back();
    widgets.emplace_back(); //field_d8[idd] = (WidgetEntry *)AllocVec(sizeof(WidgetEntry), 65537);

    WidgetEntry &sbt = widgets[widget_count];
    ButtonBox &bt = buttons[widget_count];

    if (   arg->button_type != TYPE_BUTTON
            && arg->button_type != TYPE_CHECKBX
            && arg->button_type != TYPE_CAPTION
            && arg->button_type != TYPE_RADIOBTN
            && arg->button_type != TYPE_SLIDER )
        return 0;

    sbt.button_type = arg->button_type;

    if ( arg->button_type != NC_STACK_button::TYPE_SLIDER )
    {
        if ( arg->caption.empty() )
            return 0;

        sbt.caption = arg->caption;

        if ( !arg->caption2.empty() )
            sbt.caption2 = arg->caption2;
        else
            sbt.caption2 = sbt.caption;
    }

    sbt.xpos = arg->xpos;
    sbt.ypos = arg->ypos;
    sbt.width = arg->width;

    if ( sbt.xpos < 0 || sbt.ypos < 0 || sbt.width < 0 || sbt.width + sbt.xpos > w )
        return 0;

    sbt.down_id = arg->downCode;
    sbt.up_id = arg->upCode;
    sbt.pressed_id = arg->pressedCode;
    sbt.txt_r = arg->txt_r;
    sbt.txt_g = arg->txt_g;
    sbt.txt_b = arg->txt_b;

    bt.x = sbt.xpos;
    bt.y = sbt.ypos;
    bt.w = sbt.width;

    if ( sbt.button_type == TYPE_CAPTION )
        bt.h = 0;
    else
        bt.h = GFX::Engine.GetTileset( arg->tileset_up )->h;

    widget_count++;

    sbt.button_id = arg->button_id;
    sbt.tileset_down = arg->tileset_down;
    sbt.tileset_up = arg->tileset_up;
    sbt.tileset_disabled = arg->tileset_disabled;
    sbt.flags = arg->flags;

    sbt.flags |= FLAG_DRAW;

    if ( sbt.button_type == TYPE_SLIDER )
    {
        if ( !arg->slider )
            return 0;

        Slider *sbtt = new Slider;
        if ( sbtt )
        {
            *sbtt = *arg->slider;

            sbt.slider = sbtt;

            UpdateSlider(&sbt);
            return 1;
        }
    }
    else
        return 1;

    return 0;
}

size_t NC_STACK_button::Remove(int butID)
{
    int id = GetIndexByID(butID);

    if ( id >= 0 && id < widget_count )
    {
        if (widgets[id].slider)
            delete widgets[id].slider;

        widgets.erase( widgets.begin() + id );
        buttons.erase( buttons.begin() + id );

        widget_count--;

        return 1;
    }

    return 0;
}

size_t NC_STACK_button::Enable(WidgetState *arg)
{
    int id = GetIndexByID(arg->butID);

    if ( id >= 0 && id < widget_count )
    {
        if ( widgets[id].button_type != TYPE_CAPTION )
        {
            buttons[id].w = widgets[id].width;
            // TODO(bug): Add sizes the hit box from tileset_up; this uses tileset_down.
            buttons[id].h = GFX::Engine.GetTileset( widgets[id].tileset_down )->h;
        }

        widgets[id].flags &= ~FLAG_DISABLED;
        widgets[id].flags |= FLAG_DRAW;
        return 1;
    }

    return 0;
}

size_t NC_STACK_button::Disable(WidgetState *arg)
{
    int id = GetIndexByID(arg->butID);

    if ( id >= 0 && id < widget_count )
    {
        buttons[id].w = 0;
        buttons[id].h = 0;
        widgets[id].flags |= FLAG_DISABLED;

        if ( !arg->state )
            widgets[id].flags &= ~FLAG_DRAW;

        return 1;
    }

    return 0;
}

void NC_STACK_button::ShowScreen()
{
    if ( !visible )
    {
        visible = true;
        Input::Engine.AddClickBoxFront(this);
    }
}

void NC_STACK_button::HideScreen()
{
    if ( visible )
    {
        visible = false;
        Input::Engine.RemClickBox(this);
    }
}

NC_STACK_button::ResCode NC_STACK_button::ProcessWidgetsEvents(TInputState *arg)
{
    ResCode result = ResCode(0);

    if ( !(arg->ClickInf.flag & TClickBoxInf::FLAG_OK) )
        return ResCode(0);

    if ( arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN )
    {
        for (WidgetArr::iterator it = widgets.begin(); it != widgets.end(); it++)
        {
            it->flags &= ~FLAG_DOWN;
        }
    }

    if (arg->ClickInf.flag & TClickBoxInf::FLAG_LM_UP)
    {
        for (WidgetArr::iterator it = widgets.begin(); it != widgets.end(); it++)
        {
            if ( it->button_type == TYPE_SLIDER )
            {
                if ( it->flags & FLAG_DOWN )
                {
                    it->flags &= ~(FLAG_PRESSED | FLAG_DOWN);
                    it->slider->pressPart = 0;

                    return ResCode(it->up_id, it->button_id);
                }
            }
        }
    }

    if ( arg->ClickInf.flag & TClickBoxInf::FLAG_LM_HOLD )
    {
        if ( this == arg->ClickInf.selected_btn && arg->ClickInf.selected_btnID != -1 && (widgets[arg->ClickInf.selected_btnID].flags & FLAG_DOWN) )
        {
            WidgetEntry &sbt = widgets[arg->ClickInf.selected_btnID];

            if ( sbt.button_type == TYPE_BUTTON )
                sbt.flags |= FLAG_PRESSED;
        }
        else
        {
            for (WidgetArr::iterator it = widgets.begin(); it != widgets.end(); it++)
            {
                if (it->button_type == TYPE_BUTTON)
                    it->flags &= ~FLAG_PRESSED;
            }
        }

        for (WidgetArr::iterator it = widgets.begin(); it != widgets.end(); it++)
        {
            if (it->button_type == TYPE_SLIDER && it->flags & FLAG_DOWN)
            {
                Slider *slider = it->slider;

                if (slider->pressPart == 2)
                {
                    if (it->flags & FLAG_PRESSED)
                        result = ResCode(it->pressed_id, it->button_id);

                    int scaledDx = (arg->ClickInf.move.ScreenPos.x - slider->scrDownX) * (slider->max - slider->min + 1);

                    slider->value = slider->oldValue + scaledDx / it->width;

                    if ( slider->value < slider->min )
                        slider->value = slider->min;

                    if ( slider->value > slider->max )
                        slider->value = slider->max;

                    UpdateSlider(&(*it));
                }
            }
        }
    }

    if ( this == arg->ClickInf.selected_btn )
    {
        if ( arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN )
            result = ResCode(3);

        if ( arg->ClickInf.selected_btnID != -1 && arg->ClickInf.selected_btnID < (int)widgets.size())
        {
            WidgetEntry &sbt = widgets[arg->ClickInf.selected_btnID];
            result = ResCode(0, sbt.button_id);

            switch ( sbt.button_type )
            {
            case TYPE_BUTTON:
                if ( arg->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    sbt.flags |= FLAG_PRESSED | FLAG_DOWN;
                    result = ResCode(sbt.down_id, sbt.button_id);
                }

                if ( arg->ClickInf.flag & TClickBoxInf::FLAG_BTN_HOLD )
                {
                    if ( sbt.flags & FLAG_PRESSED )
                        result = ResCode(sbt.pressed_id, sbt.button_id);
                }

                if ( (arg->ClickInf.flag & TClickBoxInf::FLAG_BTN_UP) )
                {
                    if ( sbt.flags & FLAG_DOWN )
                    {
                        sbt.flags &= ~(FLAG_PRESSED | FLAG_DOWN);
                        result = ResCode(sbt.up_id, sbt.button_id);
                    }
                }

                break;

            case TYPE_CHECKBX:
                if ( !(arg->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN) )
                    return ResCode(0, sbt.button_id);

                if ( sbt.flags & FLAG_PRESSED )
                {
                    sbt.flags &= ~FLAG_PRESSED;
                    return ResCode(sbt.up_id, sbt.button_id);
                }
                else
                {
                    sbt.flags |= FLAG_PRESSED;
                    return ResCode(sbt.down_id, sbt.button_id);
                }
                break;

            case TYPE_RADIOBTN:
                if ( arg->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    UnsetRadioButtons();
                    sbt.flags |= FLAG_PRESSED | FLAG_DOWN;
                    result = ResCode(sbt.down_id, sbt.button_id);
                }
                break;

            case TYPE_SLIDER:
                if ( !(arg->ClickInf.flag & TClickBoxInf::FLAG_BTN_UP) )
                {
                    if ( arg->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN )
                    {
                        Slider *slider = sbt.slider;
                        if ( arg->ClickInf.ldw_pos.BtnPos.x < slider->thumbStart )
                        {
                            slider->pressPart = 1;
                            if ( slider->value > slider->min )
                                slider->value -= 1;
                        }
                        else if ( arg->ClickInf.ldw_pos.BtnPos.x <= slider->thumbEnd )
                        {
                            slider->pressPart = 2;
                            slider->oldValue = slider->value;
                            slider->scrDownX = arg->ClickInf.ldw_pos.ScreenPos.x;
                        }
                        else
                        {
                            slider->pressPart = 3;
                            if ( slider->value < slider->max )
                                slider->value += 1;
                        }

                        sbt.flags |= FLAG_PRESSED | FLAG_DOWN;
                        result = ResCode(sbt.down_id, sbt.button_id);
                    }
                    else if ( arg->ClickInf.flag & TClickBoxInf::FLAG_BTN_HOLD )
                    {
                        result = ResCode(sbt.pressed_id, sbt.button_id);
                    }
                }
                UpdateSlider(&sbt);
                break;

            default:
                break;
            }
        }
    }
    return result;
}


void NC_STACK_button::DrawTextButton(WidgetEntry *sbt, CmdStream *pbuf)
{
    int tilesetId;

    if ( sbt->flags & FLAG_DISABLED )
    {
        tilesetId = sbt->tileset_disabled;
    }
    else if ( !(sbt->flags & FLAG_PRESSED) || (sbt->flags & FLAG_NOPRESS) )
    {
        tilesetId = sbt->tileset_up;
    }
    else
    {
        tilesetId = sbt->tileset_down;
    }

    TileMap *tileset = GFX::Engine.GetTileset(tilesetId);

    FontUA::select_tileset(pbuf, tilesetId);

    FontUA::set_center_xpos(pbuf, sbt->xpos + x - (screen_width / 2));

    FontUA::set_center_ypos(pbuf, sbt->ypos + y - (screen_height / 2));

    int tmp = sbt->width;

    if ( sbt->button_type != TYPE_CAPTION )
    {
        if ( sbt->flags & FLAG_BORDER )
        {
            FontUA::store_u8(pbuf, left_chr); //Padding gfx
            tmp += -tileset->map[left_chr].w - tileset->map[right_chr].w;
        }
    }

    FontUA::copy_position(pbuf);

    FontUA::op17(pbuf, tmp);

    FontUA::store_u8(pbuf, fill_chr); //Padding gfx

    if ( sbt->button_type != TYPE_CAPTION )
    {
        if ( sbt->flags & FLAG_BORDER )
        {
            FontUA::store_u8(pbuf, right_chr); //Padding gfx
        }
    }

    int textFlags;
    std::string text;

    if ( sbt->flags & FLAG_PRESSED )
    {
        if ( sbt->flags & FLAG_NOPRESS )
            textFlags = 0;
        else
            textFlags = 16;

        text = sbt->caption2;
    }
    else
    {
        textFlags = 0;
        text = sbt->caption;
    }

    if ( sbt->flags & FLAG_CENTER )
    {
        textFlags |= 4;
    }
    else if ( sbt->flags & FLAG_RALIGN )
    {
        textFlags |= 2;
    }
    else
    {
        textFlags |= 1;
    }

    FontUA::set_txtColor(pbuf, sbt->txt_r, sbt->txt_g, sbt->txt_b);

    FontUA::add_txt(pbuf, tmp, textFlags, text);
}

void NC_STACK_button::DrawTileButton(WidgetEntry *sbt, CmdStream *pbuf)
{
    int drawnW = 0;
    int tilesetId;

    if ( sbt->flags & FLAG_DISABLED )
    {
        tilesetId = sbt->tileset_disabled;
    }
    else if ( sbt->flags & FLAG_PRESSED )
    {
        tilesetId = sbt->tileset_down;
    }
    else
    {
        tilesetId = sbt->tileset_up;
    }

    TileMap *tileset = GFX::Engine.GetTileset(tilesetId);

    int strwdth = 0;

    if ( sbt->flags & FLAG_PRESSED )
    {
        int caplen = sbt->caption2.size();

        for (int i = 0; i < caplen; i++)
            strwdth += tileset->map[(uint8_t)sbt->caption2[i]].w;
    }
    else
    {
        int caplen = sbt->caption.size();

        for (int i = 0; i < caplen; i++)
            strwdth += tileset->map[(uint8_t)sbt->caption[i]].w;
    }

    int centerPad = (sbt->width - strwdth - tileset->map[left_chr].w) / 2;

    FontUA::select_tileset(pbuf, tilesetId);

    FontUA::set_center_xpos(pbuf, x + sbt->xpos - (screen_width / 2));
    FontUA::set_center_ypos(pbuf, y + sbt->ypos - (screen_height / 2));

    if ( sbt->button_type == TYPE_SLIDER )
    {
        int charIdx = 0;
        while ( sbt->caption[charIdx] != 0 || sbt->caption[charIdx + 1] != 0 )
        {
            pbuf->push_back( sbt->caption[charIdx] );
            charIdx++;
        }
    }
    else
    {
        int16_t ttmp = sbt->width;

        if ( sbt->button_type != TYPE_CAPTION )
        {
            if ( sbt->flags & FLAG_BORDER )
            {
                FontUA::store_u8(pbuf, left_chr);
                ttmp += -tileset->map[left_chr].w - tileset->map[right_chr].w;
            }
        }

        if ( centerPad > 0 )
        {
            if ( sbt->flags & FLAG_CENTER )
            {
                FontUA::op10(pbuf, centerPad);

                FontUA::store_u8(pbuf, fill_chr);

                drawnW = centerPad;
            }
        }

        std::string capt;

        if ( sbt->flags & FLAG_PRESSED )
            capt = sbt->caption2;
        else
            capt = sbt->caption;

        size_t charIdx = 0;
        while ( capt[charIdx] && charIdx < capt.size() )
        {
            Common::PointRect &pr = tileset->map[(uint8_t)capt[charIdx]];
            if ( pr.w + drawnW > ttmp )
            {
                if ( (ttmp - drawnW) > 2 )
                {
                    FontUA::set_xwidth(pbuf, ttmp - drawnW);

                    FontUA::store_s8(pbuf, capt[charIdx]);

                    drawnW = ttmp;
                }
                break;
            }

            drawnW += pr.w;

            FontUA::store_s8(pbuf, capt[charIdx]);

            charIdx++;
        }

        if ( drawnW < ttmp )
        {
            if ( sbt->button_type != TYPE_CAPTION && sbt->flags & FLAG_BORDER )
            {
                FontUA::op17(pbuf, sbt->width - tileset->map[right_chr].w);
            }
            else
            {
                FontUA::op17(pbuf, sbt->width);
            }
            FontUA::store_u8(pbuf, fill_chr);
        }

        if ( sbt->button_type != TYPE_CAPTION )
        {
            if ( sbt->flags & FLAG_BORDER )
            {
                FontUA::store_u8(pbuf, right_chr);
            }
        }
    }
}

size_t NC_STACK_button::Draw()
{
    static CmdStream button_tmpbuf;
    button_tmpbuf.reserve(5008);
    button_tmpbuf.clear();
    
    if ( visible )
    {
        for (WidgetArr::iterator it = widgets.begin(); it != widgets.end(); it++)
        {
            if ( it->flags & FLAG_DRAW )
            {
                if ( it->flags & FLAG_TEXT )
                    DrawTextButton(&*it, &button_tmpbuf);
                else
                    DrawTileButton(&*it, &button_tmpbuf);
            }
        }
        FontUA::set_end(&button_tmpbuf);

        GFX::Engine.ProcessDrawSeq(button_tmpbuf);
    }

    return 1;
}

bool NC_STACK_button::SetText(int butID, const std::string &caption, const std::string &caption2)
{
    int idx = GetIndexByID(butID);

    if ( idx >= 0 && idx < widget_count &&  !caption.empty() )
    {
        WidgetEntry &widget = widgets[idx];

        widget.caption = caption;

        if ( !caption2.empty() )
            widget.caption2 = caption2;
        else
            widget.caption2 = widget.caption;

        return true;
    }

    return false;
}

bool NC_STACK_button::SetText(int butID, const std::string &caption)
{
    int idx = GetIndexByID(butID);

    if ( idx >= 0 && idx < widget_count &&  !caption.empty() )
    {
        WidgetEntry &widget = widgets[idx];

        widget.caption = caption;
        widget.caption2 = caption;

        return true;
    }

    return false;
}

int NC_STACK_button::GetIndexByID(int butid)
{
    for (unsigned int i = 0; i < widgets.size(); i++)
    {
        if (widgets[i].button_id == butid)
            return i;
    }
    return -1;
}

void NC_STACK_button::SetState(WidgetState *arg)
{
    int id = GetIndexByID(arg->butID);

    if ( id < 0 || id >= widget_count )
        return;

    WidgetEntry &b = widgets[id];

    if ( b.button_type == TYPE_BUTTON || b.button_type == TYPE_CHECKBX || b.button_type == TYPE_RADIOBTN )
    {
        if ( b.button_type == TYPE_RADIOBTN )
            UnsetRadioButtons();

        if ( arg->state == 1 )
            b.flags |= FLAG_PRESSED | FLAG_DOWN;
        else if ( arg->state == 2 )
            b.flags &= ~(FLAG_PRESSED | FLAG_DOWN);
    }
}

NC_STACK_button::Slider * NC_STACK_button::GetSliderData(int butid)
{
    int id = GetIndexByID(butid);

    if ( id != -1 && widgets[id].button_type == TYPE_SLIDER)
        return widgets[id].slider;

    return NULL;
}

size_t NC_STACK_button::Refresh(int butid)
{
    int id = GetIndexByID(butid);

    if ( id != -1 && widgets[id].button_type == TYPE_SLIDER)
        UpdateSlider(&widgets[id]);

    return 1;
}

size_t NC_STACK_button::setXYWidth(WidgetPlacement *arg)
{
    int id = GetIndexByID(arg->butID);

    if ( id >= 0 && id < widget_count )
    {
        if (arg->xpos != -1)
        {
            widgets[id].xpos = arg->xpos;
            buttons[id].x = arg->xpos;
        }

        if (arg->ypos != -1)
        {
            widgets[id].ypos = arg->ypos;
            buttons[id].y = arg->ypos;
        }

        if (arg->width != -1)
        {
            widgets[id].width = arg->width;
            buttons[id].w = arg->width;
        }
        return 1;
    }

    return 0;
}


void NC_STACK_button::UnsetRadioButtons()
{
    for (WidgetArr::iterator it = widgets.begin(); it != widgets.end(); it++)
    {
        if ( it->button_type == TYPE_RADIOBTN )
            it->flags &= ~(FLAG_DOWN | FLAG_PRESSED);
    }
}



void NC_STACK_button::setBTN_x(int _x)
{
    x = _x;
}

void NC_STACK_button::setBTN_y(int _y)
{
    y = _y;
}

void NC_STACK_button::setBTN_w(int _w)
{
    w = _w;
}

void NC_STACK_button::setBTN_h(int _h)
{
    h = _h;
}

void NC_STACK_button::setBTN_chars(const std::string &chrs)
{
    left_chr = (uint8_t)chrs[0];
    right_chr = (uint8_t)chrs[1];
    fill_chr = (uint8_t)chrs[2];
}


int NC_STACK_button::getBTN_x()
{
    return x;
}

int NC_STACK_button::getBTN_y()
{
    return y;
}

int NC_STACK_button::getBTN_w()
{
    return w;
}

int NC_STACK_button::getBTN_h()
{
    return h;
}
