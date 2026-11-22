#include <string.h>
#include "includes.h"
#include "font.h"



void FontUA::store_u32(CmdStream *pos, uint32_t val)
{
    pos->push_back( (val >> 24) & 0xFF );
    pos->push_back( (val >> 16) & 0xFF );
    pos->push_back( (val >>  8) & 0xFF );
    pos->push_back( (val      ) & 0xFF );
}


void FontUA::store_s32(CmdStream *pos, int32_t val)
{
    pos->push_back( (val >> 24) & 0xFF );
    pos->push_back( (val >> 16) & 0xFF );
    pos->push_back( (val >>  8) & 0xFF );
    pos->push_back( (val      ) & 0xFF );
}

void FontUA::store_u16(CmdStream *pos, uint16_t val)
{
    pos->push_back( (val >>  8) & 0xFF );
    pos->push_back( (val      ) & 0xFF );
}

void FontUA::store_s16(CmdStream *pos, int16_t val)
{
    pos->push_back( (val >>  8) & 0xFF );
    pos->push_back( (val      ) & 0xFF );
}

void FontUA::store_u8(CmdStream *pos, uint8_t val)
{
    pos->push_back( val & 0xFF );
}

void FontUA::store_s8(CmdStream *pos, int8_t val)
{
    pos->push_back( val & 0xFF );
}

void FontUA::store_string(CmdStream *pos, const std::string &string)
{
    pos->insert(pos->end(), string.begin(), string.end());
    pos->push_back(0);
}

uint16_t FontUA::get_u16(const CmdStream &stream, int32_t *pos)
{
    uint16_t tmp = (stream.at((*pos) + 1)  ) & 0xFF;

    tmp |= (stream.at(*pos) <<  8) & 0xFF00;
    
    (*pos) += 2;
    
    return tmp;
}

int16_t FontUA::get_s16(const CmdStream &stream, int32_t *pos)
{
    int16_t tmp = (stream.at((*pos) + 1)  ) & 0xFF;

    tmp |= (stream.at(*pos) <<  8) & 0xFF00;
    
    (*pos) += 2;

    return tmp;
}

uint8_t FontUA::get_u8(const CmdStream &stream, int32_t *pos)
{
    uint8_t tmp = stream.at(*pos) & 0xFF;
    
    (*pos)++;

    return tmp;
}


void FontUA::set_end(CmdStream *pos)
{
    FontUA::store_u16(pos, 0);
}

void FontUA::set_center_xpos(CmdStream *pos, int16_t xpos)
{
    FontUA::store_u16(pos, 1);
    FontUA::store_s16(pos, xpos);
}

void FontUA::set_center_ypos(CmdStream *pos, int16_t ypos)
{
    FontUA::store_u16(pos, 2);
    FontUA::store_s16(pos, ypos);
}

void FontUA::set_xpos(CmdStream *pos, int16_t xpos)
{
    FontUA::store_u16(pos, 3);
    FontUA::store_s16(pos, xpos);
}

void FontUA::set_ypos(CmdStream *pos, int16_t ypos)
{
    FontUA::store_u16(pos, 4);
    FontUA::store_s16(pos, ypos);
}

void FontUA::add_xpos(CmdStream *pos, int16_t xpos)
{
    FontUA::store_u16(pos, 5);
    FontUA::store_s16(pos, xpos);
}

void FontUA::add_ypos(CmdStream *pos, int16_t ypos)
{
    FontUA::store_u16(pos, 6);
    FontUA::store_s16(pos, ypos);
}

void FontUA::next_line(CmdStream *pos)
{
    FontUA::store_u16(pos, 7);
}

void FontUA::select_tileset(CmdStream *pos, uint8_t tileset)
{
    FontUA::store_u16(pos, 8);
    FontUA::store_u8(pos, tileset);
}

void FontUA::include(CmdStream *pos, uint8_t cmdstream_id)
{
    FontUA::store_u16(pos, 9);
    FontUA::store_u8(pos, cmdstream_id);
}

void FontUA::op10(CmdStream *pos, uint8_t arg)
{
    FontUA::store_u16(pos, 10);
    FontUA::store_u8(pos, arg);
}

void FontUA::op11(CmdStream *pos, uint8_t arg)
{
    FontUA::store_u16(pos, 11);
    FontUA::store_u8(pos, arg);
}

void FontUA::set_xoff(CmdStream *pos, uint8_t xoff)
{
    FontUA::store_u16(pos, 12);
    FontUA::store_u8(pos, xoff);
}

void FontUA::set_xwidth(CmdStream *pos, uint8_t xwidth)
{
    FontUA::store_u16(pos, 13);
    FontUA::store_u8(pos, xwidth);
}

void FontUA::set_yoff(CmdStream *pos, uint8_t yoff)
{
    FontUA::store_u16(pos, 14);
    FontUA::store_u8(pos, yoff);
}

void FontUA::set_yheight(CmdStream *pos, uint8_t yheight)
{
    FontUA::store_u16(pos, 15);
    FontUA::store_u8(pos, yheight);
}

void FontUA::reset_tileset(CmdStream *pos, uint8_t tileset)
{
    FontUA::store_u16(pos, 16);
    FontUA::store_u8(pos, tileset);
}

void FontUA::op17(CmdStream *pos, int16_t arg)
{
    FontUA::store_u16(pos, 17);
    FontUA::store_s16(pos, arg);
}

void FontUA::add_txt(CmdStream *pos, int16_t block_width, uint16_t flag, const std::string &string)
{
    FontUA::store_u16(pos, 18);
    FontUA::store_s16(pos, block_width);
    FontUA::store_u16(pos, flag);
    FontUA::store_u16(pos, string.size());
    FontUA::store_string(pos, string);
}

void FontUA::copy_position(CmdStream *pos)
{
    FontUA::store_u16(pos, 19);
}

void FontUA::set_flag(CmdStream *pos, uint16_t flag)
{
    FontUA::store_u16(pos, 20);
    FontUA::store_u16(pos, flag);
}

void FontUA::unset_flag(CmdStream *pos, uint16_t flag)
{
    FontUA::store_u16(pos, 21);
    FontUA::store_u16(pos, flag);
}

void FontUA::set_txtColor(CmdStream *pos, uint8_t r, uint8_t g, uint8_t b)
{
    FontUA::store_u16(pos, 22);
    FontUA::store_u16(pos, r);
    FontUA::store_u16(pos, g);
    FontUA::store_u16(pos, b);
}







// Funcs

void FontUA::FormateClippedText(TileMap *, CmdStream *pos, const std::string &str, int width_space, char space_chr)
{
    if ( width_space > 0 )
    {
        FontUA::copy_position(pos);

        int remaining = width_space;

        while (remaining > 0)
        {
            if ( remaining <= 255 )
            {
                FontUA::op10(pos, remaining);
            }
            else
            {
                FontUA::op10(pos, 255);
            }

            FontUA::store_s8(pos, space_chr);

            remaining -= 255;
        }

        FontUA::add_txt(pos, width_space, 1, str);
    }
}

void FontUA::FormateCenteredSkipableItem(TileMap *, CmdStream *in, const std::string &str, int width)
{
    if ( width > 0 )
    {
        FontUA::copy_position(in);
        FontUA::add_txt(in, width, 4, str);
    }
}

void FontUA::TextRelWidthItem(TileMap *, CmdStream *pos, const std::string &txt, int wdth, int arg)
{
    FontUA::copy_position(pos);

    int flag;

    if ( arg == 4)
        flag = 9;
    else if ( arg == 8 )
        flag = 10;
    else
        flag = 12;

    FontUA::add_txt(pos, wdth, flag, txt);
}



void FontUA::FormateTextAlignedClippedString(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, ColumnItem *arg)
{
    TileMap *tileset = yw->_guiTiles[ arg->fontID ];

    int textW = arg->width;

    if ( arg->flags & 1 )
        textW -= tileset->map[arg->prefixChar].w;

    if ( arg->flags & 2 )
        textW -= tileset->map[arg->postfixChar].w;

    FontUA::select_tileset(cmdbuf, arg->fontID);


    if ( arg->flags & 1 )
        FontUA::store_s8(cmdbuf, arg->prefixChar);

    if ( textW > 0 )
    {
        FontUA::copy_position(cmdbuf);

        int remaining = textW;

        while (remaining > 0)
        {
            if ( remaining <= 255 )
            {
                FontUA::op10(cmdbuf, remaining);
            }
            else
            {
                FontUA::op10(cmdbuf, 255);
            }

            FontUA::store_u8(cmdbuf, arg->spaceChar);
            remaining -= 255;
        }

        int alignFlags = 0;

        if ( arg->flags & 4 )
        {
            alignFlags = 1;
        }
        else if ( arg->flags & 8 )
        {
            alignFlags = 2;
        }
        else
        {
            alignFlags = 4;
        }

        FontUA::add_txt(cmdbuf, textW, alignFlags, arg->txt);
    }

    if ( arg->flags & 2 )
    {
        FontUA::store_s8(cmdbuf, arg->postfixChar);
    }
}

void FontUA::FormateAlignedClippedString(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, ColumnItem *arg)
{
    TileMap *tset = yw->_guiTiles[ arg->fontID ];

    int clipLeft = 0;
    int clipRight = 0;
    int padLeft = 0;
    int padRight = 0;

    int txtWidth = 0;
    
    txtWidth += tset->GetWidth(arg->txt);

    int freeWidth = arg->width - txtWidth;
    if ( arg->flags & 1 )
        freeWidth -= tset->GetWidth(arg->prefixChar);
    if ( arg->flags & 2 )
        freeWidth -= tset->GetWidth(arg->postfixChar);

    FontUA::select_tileset(cmdbuf, arg->fontID);

    if ( freeWidth <= 0 )
    {
        if ( freeWidth < 0 )
        {
            if ( arg->flags & 4 )
            {
                clipRight = -freeWidth;
            }
            else if ( arg->flags & 8 )
            {
                clipLeft = -freeWidth;
            }
            else if ( arg->flags & 0x10 )
            {
                clipLeft = (-freeWidth) / 2;
                clipRight = (-freeWidth) - ((-freeWidth) / 2);
            }
        }
    }
    else
    {
        if ( arg->flags & 4 )
        {
            padRight = freeWidth;
        }
        else if ( arg->flags & 8 )
        {
            padLeft = freeWidth;
        }
        else if ( arg->flags & 0x10 )
        {
            padLeft = freeWidth / 2;
            padRight = freeWidth - (freeWidth / 2);
        }
    }

    int visibleW = txtWidth - clipRight - clipLeft;

    if ( arg->flags & 1 )
        FontUA::store_u8(cmdbuf, arg->prefixChar);

    int chr = 0;

    size_t cI = 0;

    while (clipLeft > 0 && cI < arg->txt.size())
    {
        chr = arg->txt[cI];
        cI++;
        clipLeft -= tset->GetWidth(chr);
    }

    if ( clipLeft < 0 )
    {
        FontUA::set_xoff(cmdbuf, clipLeft + tset->GetWidth(chr));
        FontUA::store_u8(cmdbuf, chr);
    }

    if ( padLeft > 0 )
    {
        while ( padLeft > 255 )
        {
            FontUA::op10(cmdbuf, 255);
            FontUA::store_u8(cmdbuf, arg->spaceChar);
            padLeft -= 255;
        }

        if ( padLeft > 0 )
        {
            FontUA::op10(cmdbuf, padLeft);
            FontUA::store_u8(cmdbuf, arg->spaceChar);
        }
    }

    if (visibleW > 0)
    {
        chr = 0;
        while ( visibleW >= 0 )
        {
            if (cI >= arg->txt.size())
            {
                chr = 0;
                break;
            }
            
            chr = arg->txt[cI];
            cI++;

            visibleW -= tset->GetWidth(chr);
            FontUA::store_u8(cmdbuf, chr);
        }
        if (chr)
        {
            FontUA::set_xwidth(cmdbuf, visibleW + tset->GetWidth(chr));
            FontUA::store_u8(cmdbuf, chr);
        }
    }

    if ( padRight > 0 )
    {
        while ( padRight > 255 )
        {
            FontUA::op10(cmdbuf, 255);
            FontUA::store_u8(cmdbuf, arg->spaceChar);
            padRight -= 255;
        }
        if ( padRight > 0 )
        {
            FontUA::op10(cmdbuf, padRight);
            FontUA::store_u8(cmdbuf, arg->spaceChar);
        }
    }
    if ( arg->flags & 2 )
        FontUA::store_u8(cmdbuf, arg->postfixChar);

}


void FontUA::FormateColumnItem(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int num, ColumnItem *items)
{
    for (int i = 0; i < num; i++)
        if (items[i].flags & 0x20)
            FormateTextAlignedClippedString(yw, cmdbuf, &items[i]);
        else
            FormateAlignedClippedString(yw, cmdbuf, &items[i]);
}


