#include <inttypes.h>
#include <string.h>
#include "includes.h"
#include "utils.h"

#include "lstvw.h"
#include "font.h"



void GuiBase::FormateTitle(NC_STACK_ypaworld *yw, int xpos, int ypos, int w, const std::string &title, CmdStream *in, uint8_t postf_char, int flag)
{
    int closeW = 0;
    if ( flag & FLAG_WITH_CLOSE )
        closeW = yw->_guiTiles[24]->map[65].w; // Help button

    int helpW = 0;
    if ( flag & FLAG_WITH_HELP )
        helpW = yw->_guiTiles[24]->map[66].w; // Close button

    int postfixW = 0;
    if ( postf_char )
        postfixW = yw->_guiTiles[0]->map[postf_char].w;

    std::string buf = " ";
    buf += title;

    FontUA::select_tileset(in, 6);
    FontUA::set_center_xpos(in, xpos);
    FontUA::set_center_ypos(in, ypos);

    FontUA::store_s8(in, 98);

    FontUA::set_txtColor(in, yw->_iniColors[60].r, yw->_iniColors[60].g, yw->_iniColors[60].b);

    FontUA::FormateClippedText(yw->_guiTiles[6], in, buf, w - closeW - helpW - postfixW - yw->_fontBorderW, 99);

    if ( postf_char )
    {
        FontUA::select_tileset(in, 0);
        FontUA::store_u8(in, postf_char);
    }

    if ( flag & FLAG_WITH_HELP )
    {
        int ts = ((flag & FLAG_HELP_DOWN) != 0) + 24;

        FontUA::select_tileset(in, ts);
        FontUA::store_s8(in, 65);
    }

    if ( flag & FLAG_WITH_CLOSE )
    {
        int ts = ((flag & FLAG_CLOSE_DOWN) != 0) + 24;

        FontUA::select_tileset(in, ts);
        FontUA::store_s8(in, 85);
    }
}


int GuiList::initDialogStrings(NC_STACK_ypaworld *yw)
{
    int xpos = x - (yw->_screenSize.x / 2);
    int ypos = y - (yw->_screenSize.y / 2);

    cmdCommands.reserve(512);

    if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
    {
        GuiBase::FormateTitle(yw, xpos, ypos, w, title, &cmdCommands, 0, flags);

        FontUA::next_line(&cmdCommands);
    }
    else
    {
        FontUA::select_tileset(&cmdCommands, 0);
        FontUA::set_center_xpos(&cmdCommands, xpos);
        FontUA::set_center_ypos(&cmdCommands, ypos);
    }
    FontUA::include(&cmdCommands, 1); // Include data part
    FontUA::include(&cmdCommands, 0); // Include slider
    FontUA::set_end(&cmdCommands);

    return 1;
}

int GuiList::InitBuffers(NC_STACK_ypaworld *yw)
{
    iconString.reserve(32);
    FontUA::set_end(&iconString);

    if ( !initDialogStrings(yw) )
        return 0;

    itemBlock.reserve(0x2000);
    FontUA::set_end(&itemBlock);

    scrollbar.reserve(256);
    FontUA::set_end(&scrollbar);

    cmdInclude.assign( {&scrollbar, &itemBlock} );

    return 1;
}

void GuiList::SetRect(NC_STACK_ypaworld *yw, int xpos, int ypos)
{
    if ( entryWidth < minEntryWidth)
        entryWidth = minEntryWidth;

    if ( entryWidth > maxEntryWidth)
        entryWidth = maxEntryWidth;

    if ( shownEntries < minShownEntries)
        shownEntries = minShownEntries;

    if ( shownEntries > maxShownEntries)
        shownEntries = maxShownEntries;

    int ww = entryWidth;

    if ( (listFlags & GLIST_FLAG_RESIZEABLE) || numEntries > maxShownEntries )
        ww += yw->_fontVBScrollW;

    int hh = upperVborder + lowerVborder + entryHeight * shownEntries;

    if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
        hh += yw->_fontH;

    if ( xpos == -1 )
    {
        xpos = (yw->_screenSize.x / 2) - (ww / 2);
    }
    else if ( xpos == -2 )
    {
        xpos = x;
    }

    if ( ypos == -1 )
    {
        ypos = (yw->_screenSize.y / 2) - (hh / 2);
    }
    else if ( ypos == -2 )
    {
        ypos = y;
    }

    if ( xpos < 0 )
        xpos = 0;

    if ( ypos < yw->_upScreenBorder )
        ypos = yw->_upScreenBorder;

    if ( ww + xpos >= yw->_screenSize.x )
        xpos = yw->_screenSize.x - ww;

    if ( hh + ypos >= yw->_screenSize.y - yw->_downScreenBorder )
        ypos = yw->_screenSize.y - yw->_downScreenBorder - hh;

    x = xpos;
    y = ypos;
    w = ww;
    h = hh;
}

int GuiList::initButtons()
{
    buttons.resize(shownEntries + 8);

    if ( listFlags & GLIST_FLAG_WITH_ICON )
    {
        iconBox.buttons.resize(1);
        iconBox.buttons[0] = ButtonBox(0, 0, 16, 16);
    }
    else
    {
        iconBox.buttons.resize(0);
    }

    return 1;
}

void GuiList::FormateTitle(NC_STACK_ypaworld *yw)
{
    if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
    {
        cmdCommands.clear();

        int xpos = x - (yw->_screenSize.x / 2);
        int ypos = y - (yw->_screenSize.y / 2);

        int closeW, helpW;
        if ( flags & FLAG_WITH_CLOSE )
            closeW = yw->_fontDefCloseW;
        else
            closeW = 0;

        if ( flags & FLAG_WITH_HELP )
            helpW = yw->_fontDefCloseW;
        else
            helpW = 0;

        if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
        {
            GuiBase::FormateTitle(yw, xpos, ypos, w, title, &cmdCommands, 0, flags);
            FontUA::next_line(&cmdCommands);
        }
        else
        {
            FontUA::select_tileset(&cmdCommands, 0);
            FontUA::set_center_xpos(&cmdCommands, xpos);
            FontUA::set_center_ypos(&cmdCommands, ypos);
        }

        FontUA::include(&cmdCommands, 1); // Data
        FontUA::include(&cmdCommands, 0); // Slider
        FontUA::set_end(&cmdCommands);

        int dragBarW = w - closeW - helpW;

        buttons[1] = ButtonBox(0, 0, dragBarW, yw->_fontH);

        if ( flags & FLAG_WITH_HELP )
            // TODO(cleanup): The help button uses closeW; should be helpW. Harmless today: both are
            // _fontDefCloseW and help is only ever set together with close.
            buttons[7] = ButtonBox(dragBarW, 0, closeW, yw->_fontH);
        else
            buttons[7] = ButtonBox();

        if ( flags & FLAG_WITH_CLOSE )
            buttons[0] = ButtonBox(w - closeW, 0, closeW, yw->_fontH);
        else
            buttons[0] = ButtonBox();
    }
}

void GuiList::ScrollParamsFromEntries(NC_STACK_ypaworld *yw)
{
    int trackSize = h;

    if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
        trackSize -= yw->_fontH;

    if ( listFlags & GLIST_FLAG_RESIZEABLE )
        trackSize -= yw->_fontHScrollH;

    int thumbSize, thumbStart;

    if ( numEntries )
    {
        thumbSize = trackSize * shownEntries / numEntries;
        thumbStart = trackSize * firstShownEntries / numEntries;
    }
    else
    {
        thumbSize = trackSize;
        thumbStart = 0;
    }

    if ( thumbSize >= 2 )
    {
        if ( thumbSize > trackSize )
            thumbSize = trackSize;
    }
    else
    {
        thumbStart -= (2 - thumbSize) / 2;
        thumbSize = 2;
    }

    if ( thumbStart < 0 )
        thumbStart = 0;
    if ( thumbStart + thumbSize > trackSize )
        thumbStart = trackSize - thumbSize;

    btnSize = thumbSize;
    btnStart = thumbStart;
    scrlSize = trackSize;
}

void GuiList::FormateScrollbar(NC_STACK_ypaworld *yw)
{
    scrollbar.clear();

    if ( listFlags & GLIST_FLAG_RESIZEABLE || numEntries > maxShownEntries )
    {

        if ( listFlags & GLIST_FLAG_IN_SCROLLING )
        {
            firstShownEntries = numEntries * btnStart  / scrlSize;

            if ( shownEntries + firstShownEntries > numEntries)
            {
                firstShownEntries = numEntries - shownEntries;

                if ( firstShownEntries < 0 )
                    firstShownEntries = 0;
            }
        }
        else
        {
            ScrollParamsFromEntries(yw);
        }

        int barX = w - yw->_fontVBScrollW;
        int barY = 0;

        if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
            barY = yw->_fontH;

        buttons[2] = ButtonBox(barX, barY, yw->_fontVBScrollW, btnStart); // Scroll up part
        buttons[3] = ButtonBox(barX, btnStart + barY, yw->_fontVBScrollW, btnSize); // Scroll position
        buttons[4] = ButtonBox(barX, btnSize + btnStart + barY,  yw->_fontVBScrollW, scrlSize - (btnSize + btnStart));
        buttons[6] = ButtonBox(0, h - lowerVborder, w - yw->_fontVBScrollW, lowerVborder);

        if ( listFlags & GLIST_FLAG_RESIZEABLE )
            buttons[5] = ButtonBox(barX, scrlSize + barY, yw->_fontVBScrollW, yw->_fontHScrollH); // Resize button
        else
            buttons[5] = ButtonBox();

        int screenX = x + barX - (yw->_screenSize.x / 2);
        int screenY = y + barY - (yw->_screenSize.y / 2);

        FontUA::set_center_xpos(&scrollbar, screenX);
        FontUA::set_center_ypos(&scrollbar, screenY);

        if ( btnStart > 0 )
        {
            FontUA::reset_tileset(&scrollbar, 13);
            FontUA::store_u8(&scrollbar, 67);
            FontUA::next_line(&scrollbar);
            FontUA::reset_tileset(&scrollbar, 12);

            int remaining = btnStart - 1;

            while (remaining >= yw->_fontVScrollH)
            {
                FontUA::store_u8(&scrollbar, 66);
                FontUA::next_line(&scrollbar);
                remaining -= yw->_fontVScrollH;
            }

            if ( remaining > 0 )
            {
                FontUA::set_yheight(&scrollbar, remaining);

                FontUA::store_u8(&scrollbar, 66);
                FontUA::next_line(&scrollbar);
            }
        }


        if ( btnSize > 0 )
        {
            FontUA::reset_tileset(&scrollbar, 13);
            FontUA::store_u8(&scrollbar, 69);
            FontUA::next_line(&scrollbar);
            FontUA::reset_tileset(&scrollbar, 12);

            int remaining = btnSize - 1;

            while (remaining > yw->_fontVScrollH)
            {
                FontUA::store_u8(&scrollbar, 67);
                FontUA::next_line(&scrollbar);
                remaining -= yw->_fontVScrollH;
            }

            if ( remaining > 1 )
            {
                FontUA::set_yheight(&scrollbar, remaining - 1);

                FontUA::store_u8(&scrollbar, 67);
                FontUA::next_line(&scrollbar);
            }
        }

        FontUA::reset_tileset(&scrollbar, 13);
        FontUA::store_u8(&scrollbar, 70);
        FontUA::next_line(&scrollbar);

        int lowerPart = scrlSize - (btnStart  + btnSize);

        if ( lowerPart > 0 )
        {
            FontUA::reset_tileset(&scrollbar, 12);

            while (lowerPart > yw->_fontVScrollH)
            {
                FontUA::store_u8(&scrollbar, 66);
                FontUA::next_line(&scrollbar);
                lowerPart -= yw->_fontVScrollH;
            }

            if ( lowerPart > 1 )
            {
                FontUA::set_yheight(&scrollbar, lowerPart - 1);

                FontUA::store_u8(&scrollbar, 66);
                FontUA::next_line(&scrollbar);
            }

            FontUA::reset_tileset(&scrollbar, 13);
            FontUA::store_u8(&scrollbar, 68);
            FontUA::next_line(&scrollbar);
        }

        if ( listFlags & GLIST_FLAG_RESIZEABLE )
        {
            FontUA::reset_tileset(&scrollbar, 11);
            FontUA::store_u8(&scrollbar, 71);
        }
    }
    else
    {
        buttons[2] = ButtonBox();
        buttons[3] = ButtonBox();
        buttons[4] = ButtonBox();
        buttons[5] = ButtonBox();
        buttons[6] = ButtonBox();
    }
    FontUA::set_end(&scrollbar);
}

void GuiList::FormateItemBlock(NC_STACK_ypaworld *yw)
{
    buttons.resize(shownEntries + 8);

    int itemsTop = upperVborder;

    if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
        itemsTop += yw->_fontH;

    for (int i = 0; i < shownEntries; i++)
        buttons[8 + i] = ButtonBox( 0, itemsTop + i * entryHeight, entryWidth, entryHeight );
}


int GuiList::Init(NC_STACK_ypaworld *yw, tInit &in)
{
    closeChar = in.closeChar;
    numEntries = in.numEntries;
    shownEntries = in.shownEntries;
    firstShownEntries = in.firstShownEntry;
    selectedEntry = in.selectedEntry;
    maxShownEntries = in.maxShownEntries;
    minShownEntries = in.minShownEntries;
    entryHeight = in.entryHeight;
    entryWidth = in.entryWidth;
    minEntryWidth = in.minEntryWidth;
    maxEntryWidth = in.maxEntryWidth;
    upperVborder = in.upperVborder;
    lowerVborder = in.lowerVborder;



    if ( !upperVborder && !lowerVborder )
    {
        lowerVborder = upperVborder = in.vborder;
    }

    listFlags = 0;
    mouseItem = -1;

    if ( in.enabled )
        listFlags |= GLIST_FLAG_ENABLED;

    if ( !in.title.empty() )
    {
        listFlags |= GLIST_FLAG_WITH_TITLEBAR;
        strncpy(title, in.title.c_str(), 64);
    }


    if ( in.resizeable )
        listFlags |= GLIST_FLAG_RESIZEABLE;

    if ( in.instantInput )
        listFlags |= GLIST_FLAG_INSTANT_INPUT;

    if ( in.staticItems )
        listFlags |= GLIST_FLAG_STATIC;

    if ( in.keyboardInput )
        listFlags |= GLIST_FLAG_KEYB_INPUT;

    if ( in.withIcon )
    {
        if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
            listFlags |= GLIST_FLAG_WITH_ICON;
    }

    flags = 0;

    if ( listFlags & GLIST_FLAG_WITH_ICON )
        flags |= (FLAG_ICONIFED | FLAG_WITH_ICON);
    else
        flags |= FLAG_CLOSED;

    if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
    {
        flags |= (FLAG_WITH_CLOSE | FLAG_WITH_DRAGBAR);

        if ( in.withHelp )
            flags |= FLAG_WITH_HELP;
    }


    if ( listFlags & GLIST_FLAG_WITH_ICON )
    {
        iconBox.x = 16 * in.iconPos;
        iconBox.y = yw->_screenSize.y - 32;
        iconBox.w = 16;
        iconBox.h = 16;
        iconBox.buttons.resize(1);
    }
    else
    {
        iconBox.x = 0;
        iconBox.y = 0;
        iconBox.w = 0;
        iconBox.h = 0;
        iconBox.buttons.resize(0);
    }


    if ( !InitBuffers(yw) )
    {
        return 0;
    }



    SetRect(yw, -1, -1);

    if ( !initButtons() )
    {
        return 0;
    }


    FormateTitle(yw);
    FormateScrollbar(yw);
    FormateItemBlock(yw);

    return 1;
}


void GuiList::ItemsPreLayout(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int tileset, const char *borderChars)
{
    int xpos = x - (yw->_screenSize.x / 2);
    int ypos = y - (yw->_screenSize.y / 2);

    if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
        ypos += yw->_fontH;

    FontUA::reset_tileset(cmdbuf, tileset);
    FontUA::set_center_xpos(cmdbuf, xpos);
    FontUA::set_center_ypos(cmdbuf, ypos);

    FontUA::set_yheight(cmdbuf, upperVborder);

    FontUA::store_s8(cmdbuf, borderChars[0]);

    FontUA::op17(cmdbuf, entryWidth - yw->_fontBorderW);

    FontUA::store_s8(cmdbuf, borderChars[1]);
    FontUA::store_s8(cmdbuf, borderChars[2]);
    FontUA::next_line(cmdbuf);
}

void GuiList::ItemsPostLayout(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int tileset, const char *borderChars)
{
    FontUA::reset_tileset(cmdbuf, tileset);
    FontUA::set_yoff(cmdbuf, yw->_guiTiles[tileset]->h - lowerVborder);
    FontUA::store_s8(cmdbuf, borderChars[0]);
    FontUA::op17(cmdbuf, entryWidth - yw->_fontBorderW);
    FontUA::store_s8(cmdbuf, borderChars[1]);
    FontUA::store_s8(cmdbuf, borderChars[2]);
}


void GuiList::InputHandle(NC_STACK_ypaworld *yw, TInputState *struc)
{
    if ( flags & (FLAG_CLOSED | FLAG_ICONIFED) )
    {
        listFlags &= ~(GLIST_FLAG_IN_RESIZING | GLIST_FLAG_IN_SCROLLING | GLIST_FLAG_IN_SELECT);
        return;
    }

    TClickBoxInf *clickInf = &struc->ClickInf;

    int curW = w;
    int curH = h;

    listFlags &= ~GLIST_FLAG_SEL_DONE;

    int xpos = x;

    int ypos = y;

    mouseItem = -1;



    if ( listFlags & GLIST_FLAG_KEYB_INPUT )
    {
        if ( struc->KbdLastHit == Input::KC_UP )
        {
            selectedEntry--;
            if ( selectedEntry < 0 )
            {
                selectedEntry = numEntries - 1;
                firstShownEntries = numEntries - shownEntries;
            }
            if ( selectedEntry < firstShownEntries )
            {
                firstShownEntries = selectedEntry;
            }
        }
        else if ( struc->KbdLastHit == Input::KC_DOWN )
        {
            selectedEntry++;
            if ( selectedEntry >= numEntries )
            {
                firstShownEntries = 0;
                selectedEntry = 0;
            }
            if ( firstShownEntries + shownEntries - 1 <= selectedEntry )
            {
                firstShownEntries = selectedEntry - shownEntries + 1;
            }
        }
    }

    yw->_guiDragDefaultMouse = (listFlags & (GLIST_FLAG_IN_RESIZING | GLIST_FLAG_IN_SCROLLING | GLIST_FLAG_IN_SELECT)) && (clickInf->flag & TClickBoxInf::FLAG_LM_HOLD);

    //printf("%x %x %d, \n", (int)&dialogBox , (int)struc->winp131arg.selected_btn , struc->winp131arg.selected_btnID);

    if ( this == struc->ClickInf.selected_btn && struc->ClickInf.selected_btnID >= 8 )
        listFlags &= ~GLIST_FLAG_NO_SCROLL;

    if ( listFlags & GLIST_FLAG_IN_RESIZING )
    {
        if ( clickInf->flag & TClickBoxInf::FLAG_LM_HOLD )
        {
            int xps = clickInf->move.ScreenPos.x + rszX;
            int yps = clickInf->move.ScreenPos.y + rszY;

            int maxH = lowerVborder + upperVborder + entryHeight * maxShownEntries;
            int minH = lowerVborder + upperVborder + entryHeight * minShownEntries;

            if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
            {
                minH += yw->_fontH;
                maxH += yw->_fontH;
            }

            int limitW = yw->_fontVBScrollW + maxEntryWidth;

            if ( xps > limitW )
                xps = limitW;
            else
            {
                limitW = yw->_fontVBScrollW + minEntryWidth;

                if ( xps < limitW )
                    xps = limitW;
            }

            if ( yps > maxH )
                yps = maxH;
            else if ( yps < minH)
                yps = minH;

            if ( xps + xpos >= yw->_screenSize.x )
                xps = yw->_screenSize.x - xpos;

            if ( ypos + yps >= yw->_screenSize.y - yw->_downScreenBorder )
                yps = yw->_screenSize.y - yw->_downScreenBorder - ypos;

            if ( listFlags & GLIST_FLAG_WITH_TITLEBAR )
                shownEntries = (yps - lowerVborder - upperVborder - yw->_fontH) / entryHeight;
            else
                shownEntries = (yps - lowerVborder - upperVborder ) / entryHeight;

            if ( firstShownEntries + shownEntries > numEntries )
            {
                firstShownEntries = numEntries - shownEntries;
                if ( firstShownEntries < 0 )
                    firstShownEntries = 0;
            }

            entryWidth = xps - yw->_fontVBScrollW;

            SetRect(yw, -2, -2);
        }
        else
        {
            listFlags &= ~GLIST_FLAG_IN_RESIZING;
        }
    }
    else if ( listFlags & GLIST_FLAG_IN_SCROLLING )
    {
        if ( clickInf->flag & TClickBoxInf::FLAG_LM_HOLD )
        {
            btnStart += struc->ClickInf.move.ScreenPos.y - scrlY;
            scrlY = struc->ClickInf.move.ScreenPos.y;
            if ( btnStart >= 0 )
            {
                if ( btnStart + btnSize > scrlSize )
                    btnStart = scrlSize - btnSize;
            }
            else
            {
                btnStart = 0;
            }
        }
        else
        {
            listFlags &= ~GLIST_FLAG_IN_SCROLLING;
        }
    }
    else if ( listFlags & GLIST_FLAG_IN_SELECT )
    {
        if ( (this == struc->ClickInf.selected_btn) && (struc->ClickInf.selected_btnID >= 8) )
        {
            if ( clickInf->flag & (TClickBoxInf::FLAG_LM_HOLD | TClickBoxInf::FLAG_LM_UP) )
            {
                selectedEntry = firstShownEntries + struc->ClickInf.selected_btnID - 8;
                mouseItem = selectedEntry;

                if ( clickInf->flag & TClickBoxInf::FLAG_LM_UP )
                {
                    listFlags &= ~(GLIST_FLAG_SEL_DONE | GLIST_FLAG_IN_SELECT);
                    listFlags |= GLIST_FLAG_SEL_DONE;

                    if ( listFlags & GLIST_FLAG_INSTANT_INPUT )
                    {
                        flags |= FLAG_CLOSED;
                        Input::Engine.RemClickBox(struc->ClickInf.selected_btn);
                    }
                }
            }
            else
            {
                listFlags &= ~GLIST_FLAG_IN_SELECT;

            }
        }
        else if ( !(clickInf->flag & TClickBoxInf::FLAG_LM_HOLD) )
        {
            listFlags &= ~GLIST_FLAG_IN_SELECT;
        }
        else if ( !(listFlags & GLIST_FLAG_NO_SCROLL) )
        {
            ButtonBox &firstItem = buttons[8];
            ButtonBox &lastItem = buttons[shownEntries + 7];

            if ( scrollTimer > 0 )
            {
                scrollTimer -= struc->Period;
            }
            else
            {
                scrollTimer = 70;

                if ( struc->ClickInf.move.ScreenPos.y >= ypos + firstItem.y )
                {
                    if ( struc->ClickInf.move.ScreenPos.y > ypos + lastItem.y + lastItem.h )
                    {
                        firstShownEntries++;

                        if ( shownEntries + firstShownEntries > numEntries )
                        {
                            firstShownEntries = numEntries - shownEntries;
                            if ( firstShownEntries < 0 )
                                firstShownEntries = 0;
                        }
                        selectedEntry = shownEntries + firstShownEntries - 1;
                    }
                }
                else
                {
                    firstShownEntries--;

                    if ( firstShownEntries < 0 )
                        firstShownEntries = 0;

                    selectedEntry = firstShownEntries;
                }
            }

            if ( struc->ClickInf.move.ScreenPos.y >= ypos + firstItem.y )
            {
                if ( struc->ClickInf.move.ScreenPos.y > ypos + lastItem.y + lastItem.h )
                    selectedEntry = shownEntries + firstShownEntries - 1;
            }
            else
            {
                selectedEntry = firstShownEntries;
            }
        }
    }
    else if ( this == struc->ClickInf.selected_btn )
    {
        if ( struc->ClickInf.selected_btnID >= 8 )
            mouseItem = struc->ClickInf.selected_btnID + firstShownEntries - 8;

        if ( struc->ClickInf.selected_btnID == 7 )
        {
            if ( clickInf->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                flags |= FLAG_HELP_DOWN;
            if ( clickInf->flag & TClickBoxInf::FLAG_BTN_UP )
                flags &= ~FLAG_HELP_DOWN;
        }

        if ( clickInf->flag & TClickBoxInf::FLAG_BTN_HOLD )
        {
            if ( struc->ClickInf.selected_btnID == 2 )
            {
                if ( scrollTimer > 0 )
                {
                    scrollTimer -= struc->Period;
                }
                else
                {
                    scrollTimer = 70;

                    firstShownEntries--;
                    if ( firstShownEntries < 0 )
                        firstShownEntries = 0;
                }
            }
            else if ( struc->ClickInf.selected_btnID == 4 )
            {
                if ( scrollTimer > 0 )
                {
                    scrollTimer -= struc->Period;
                }
                else
                {
                    firstShownEntries++;
                    scrollTimer = 70;
                    if ( shownEntries + firstShownEntries > numEntries )
                    {
                        firstShownEntries = numEntries - shownEntries;
                        if ( firstShownEntries < 0 )
                            firstShownEntries = 0;
                    }
                }
            }
        }

        if ( clickInf->flag & TClickBoxInf::FLAG_BTN_DOWN )
        {
            if ( clickInf->selected_btnID == 3 )
            {
                listFlags |= GLIST_FLAG_IN_SCROLLING;
                scrlY = clickInf->ldw_pos.ScreenPos.y;
            }
            else if ( clickInf->selected_btnID == 5 )
            {
                listFlags |= GLIST_FLAG_IN_RESIZING;
                rszX = curW - clickInf->ldw_pos.ScreenPos.x;
                rszY = curH - clickInf->ldw_pos.ScreenPos.y;
            }

            if ( clickInf->selected_btnID >= 8 )
            {
                listFlags |= GLIST_FLAG_IN_SELECT;
                selectedEntry = mouseItem;
            }
        }
    }
    else if ( listFlags & GLIST_FLAG_INSTANT_INPUT )
    {
        if ( clickInf->flag & TClickBoxInf::FLAG_LM_DOWN )
        {
            flags |= FLAG_CLOSED;
            Input::Engine.RemClickBox(this);
        }
    }
}

void GuiBase::Detach()
{
    if (AttachedTo)
    {
        AttachedTo->remove(this);
        AttachedTo = NULL;
    }
}

void GuiBase::Attach(GuiBaseList &_lst)
{
    Detach();

    _lst.push_back(this);
    AttachedTo = &_lst;
}

void GuiList::Formate(NC_STACK_ypaworld *yw)
{
    FormateTitle(yw);
    FormateScrollbar(yw);
    if ( !(listFlags & GLIST_FLAG_STATIC) )
        FormateItemBlock(yw);
}

void GuiList::PosOnSelected(int entryIdx)
{
    int shownEnd = maxShownEntries + firstShownEntries;

    if ( entryIdx >= firstShownEntries && entryIdx <= (shownEnd - 1) )
    {
        if ( shownEnd > numEntries )
            firstShownEntries = numEntries - maxShownEntries;
        if ( firstShownEntries < 0 )
            firstShownEntries = 0;
    }
    else if ( numEntries - entryIdx <= maxShownEntries )
    {
        firstShownEntries = numEntries - maxShownEntries;
    }
    else
    {
        firstShownEntries = entryIdx;
    }
}


void GuiList::Free()
{
    cmdCommands.clear();
    itemBlock.clear();
    scrollbar.clear();
    cmdInclude.clear();
    iconString.clear();
    
    buttons.clear();
}
