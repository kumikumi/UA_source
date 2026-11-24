#include "../fmtlib/printf.h"

#include "gfx.h"
#include "gl_backend.h"
#include "null_backend.h"
#ifdef UA_GPU_RENDERER
#include "gpu_backend.h"
#endif
#include "../utils.h"
#include "common/common.h"
#include "../ini.h"
#include "../bitmap.h"
#include "../ilbm.h"
#include "../log.h"
#include "../font.h"
#include "inivals.h"
#include "perf.h"
#include "cmdline.h"

namespace GFX
{   
GFXEngine GFXEngine::Instance;

int GFXEngine::can_srcblend;
int GFXEngine::can_destblend;
int GFXEngine::can_stippling;
uint32_t GFXEngine::FpsMaxTicks = 1000/60;

const SDL_PixelFormatDetails *GFXEngine::_pixfmt = NULL;
bool GFXEngine::_staticInited = false;

const std::array<vec3d, 8> GFXEngine::_clrEff 
{   vec3d(1.0,  1.0,  1.0)
,   vec3d(1.21, 0.0,  0.29)
,   vec3d(0.13, 0.43, 2.17)
,   vec3d(0.0,  1.60, 1.60)
,   vec3d(1.0,  1.0,  1.0)
,   vec3d(0.57, 0.59, 0.59)
,   vec3d(1.4,  1.08,  1.12)
,   vec3d(0.3,  0.60, 0.7)};

// The renderers' GUIDs in env/guid3d.def, where the options save the choice
static const std::string DEVICE_GPU = "<sdl_gpu>";
static const std::string DEVICE_GL = "<opengl>";


bool TRenderNode::CompareSolid(TRenderNode *a, TRenderNode *b)
{
    if ( !a->Mesh )
        return true;
    
    if ( !b->Mesh )
        return false;
    
    return a->Tex < b->Tex;
}

bool TRenderNode::CompareTransparent(TRenderNode* a, TRenderNode* b)
{
    return a->Distance > b->Distance;
}

bool TRenderNode::CompareDistance(TRenderNode* a, TRenderNode* b)
{
    return a->Distance < b->Distance;
}

bool TRenderParams::operator==(const TRenderParams &b)
{
    if (Flags != b.Flags)
        return false;

    if (Flags & RFLAGS_DYNAMIC_TEXTURE)
        return TexSource->IsSameRes( b.TexSource );

    return Tex == b.Tex;
}

void GFXEngine::StaticInit()
{
    if (_staticInited)
        return;

    _staticInited = true;
    
    const SDL_DisplayMode *curr = SDL_GetCurrentDisplayMode( SDL_GetPrimaryDisplay() );
    
    switch(curr ? curr->format : SDL_PIXELFORMAT_UNKNOWN)
    {
        case SDL_PIXELFORMAT_XRGB8888:
        case SDL_PIXELFORMAT_ARGB8888:
            _pixfmt = SDL_GetPixelFormatDetails( SDL_PIXELFORMAT_ARGB8888 );
            break;
        
        case SDL_PIXELFORMAT_XBGR8888:
        case SDL_PIXELFORMAT_ABGR8888:
        default:
            _pixfmt = SDL_GetPixelFormatDetails( SDL_PIXELFORMAT_ABGR8888 );
            break;
    }
}

GFXEngine::GFXEngine()
{
    for(TileMap *&t : _tiles)
        t = NULL;
    
    for(SDL_Color &c : _palette)
        c = {0, 0, 0, 0};
    
    _disable_lowres = 0;
    _export_window_mode = 0;
    _flags = 0;

    _dither = 0;
    _filter = 0;
    _antialias = 0;
    _alpha = 255;
    _zbuf_when_tracy = 0;
    _colorkey = 0;

    _corrIW = _corrW = 1.0;
    _corrIH = _corrH = 1.0;

    _solidFont = true;
    
    _setFrustumClip(1.0, 8192.0);
    
    _normClr = vec3d(1.0, 1.0, 1.0);
    _invClr = vec3d(0.0, 0.0, 0.0);
}

std::string read_guid(const std::string &filename)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc(filename, "r");
    if ( !fil )
        return "";
    
    std::string guid;
    fil->ReadLine(&guid);
    delete fil;
    return guid;
}

bool out_guid_to_file(const std::string &filename, const std::string &name)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc(filename, "w");
    if ( !fil )
        return false;
    
    fil->puts(name);
    delete fil;
    return true;
}

void GFXEngine::DrawTextEntry(const ScreenText *txt)
{
    if ( _font.ttfFont )
    {
        if ( txt->flag & 0x20 )
        {
            _font.r = txt->p1;
            _font.g = txt->p2;
            _font.b = txt->p3;
        }
        else
        {
            if (!txt->string.empty())
            {

                int cx = 0, cy = 0;

                if ( txt->flag & 0xE )
                {
                    TTF_GetStringSize(_font.ttfFont, txt->string.c_str(), 0, &cx, &cy);
                }

                int p1 = txt->p1;
                int p2 = txt->p2;
                int p3 = txt->p3;
                int p4 = txt->p4;

                if ( txt->flag & 8 )
                    p3 = cx * p3 / 100;

                SDL_Rect clipRect;

                clipRect.x = p1;
                clipRect.w = p3 + 4;
                clipRect.h = p4 + 1;
                clipRect.y = p2;

                if ( txt->flag & 2 )
                {
                    if ( txt->flag & 8 )
                    {
                        p1 -= cx;
                        clipRect.x = p1;
                        clipRect.w = p3 + 4;
                    }
                    else
                    {
                        p1 += (p3 - cx);
                    }
                }
                else if ( txt->flag & 4 )
                {
                    if ( txt->flag & 8 )
                    {
                        p1 -= cx / 2;
                        clipRect.x = p1;
                        clipRect.w = p3 + 4;
                    }
                    else
                    {
                        p1 += (p3 - cx) / 2;
                    }
                }

                SDL_SetSurfaceClipRect(Screen(), &clipRect);


                int textY = ((p4 - _font.height) / 2) - 2 + p2;
                if ( txt->flag & 0x10 )
                {
                    textY++;
                    p1++;
                }

                SDL_Color clr;
                clr.a = 255;
                clr.r = 0;
                clr.g = 0;
                clr.b = 0;

                SDL_Surface *tmp;

                if (_solidFont)
                {
                    tmp = TTF_RenderText_Solid(_font.ttfFont, txt->string.c_str(), 0, clr);
                    SDL_SetSurfaceBlendMode(tmp, SDL_BLENDMODE_NONE);
                }
                else
                {
                    tmp = TTF_RenderText_Blended(_font.ttfFont, txt->string.c_str(), 0, clr);
                    SDL_SetSurfaceBlendMode(tmp, SDL_BLENDMODE_BLEND);
                }
                
                if (tmp)
                {
                    SDL_Rect want;
                    want.w = tmp->w;
                    want.h = tmp->h;
                    want.x = p1 + 2;
                    want.y = textY + 1;

                    SDL_BlitSurface(tmp, NULL, Screen(), &want);

                    clr.a = 255;
                    clr.r = _font.r;
                    clr.g = _font.g;
                    clr.b = _font.b;

                    if (_solidFont)
                    {
                        SDL_SetPaletteColors(SDL_GetSurfacePalette(tmp), &clr, 1, 1);
                    }
                    else
                    {
                        SDL_DestroySurface(tmp);
                        tmp = TTF_RenderText_Blended(_font.ttfFont, txt->string.c_str(), 0, clr);
                        if (tmp)
                            SDL_SetSurfaceBlendMode(tmp, SDL_BLENDMODE_BLEND);
                    }

                    if (tmp)
                    {
                        want.w = tmp->w;
                        want.h = tmp->h;
                        want.x = p1 + 1;
                        want.y = textY;

                        SDL_BlitSurface(tmp, NULL, Screen(), &want);
                        SDL_DestroySurface(tmp);
                    }


                    SDL_SetSurfaceClipRect(Screen(), NULL);
                }
            }
        }
    }
}

void GFXEngine::AddScreenText(const std::string &string, int p1, int p2, int p3, int p4, int flag)
{
    ScreenText *entry = new ScreenText;
    entry->string = string;
    entry->p1 = p1;
    entry->p2 = p2;
    entry->p3 = p3;
    entry->p4 = p4;
    entry->flag = flag;

    _font.entries.push_back(entry);
}

void GFXEngine::DrawScreenText()
{
    _font.r = 255;
    _font.g = 255;
    _font.b = 0;

    for ( std::list<ScreenText *>::iterator it = _font.entries.begin(); it != _font.entries.end(); it++ )
    {
        DrawTextEntry(*it);
        delete (*it);
    }

    _font.entries.clear();
}

int GFXEngine::GetTextWidth(const std::string &text)
{
    int w = 0, h = 0;

    if ( _font.ttfFont )
        TTF_GetStringSize(_font.ttfFont, text.c_str(), 0, &w, &h);

    return w;
}

int GFXEngine::LoadFontByDescr(const std::string &fontname)
{
    std::vector<std::string> splt = Stok::Split(fontname, ",");

    std::string facename;
    std::string s_height;
    
    if (splt.size() > 0)
        facename = splt[0];
    
    if (splt.size() > 1)
        s_height = splt[1];
    //const char *s_weight = strtok(0, ",");
    //const char *s_charset = strtok(0, ",");

    int height;//, weight, charset;
    if ( !facename.empty() && !s_height.empty() )//&& s_weight && s_charset )
    {
        height = std::stoi(s_height);
        //weight = atoi(s_weight);
        //charset = atoi(s_charset);
    }
    else
    {
        height = 12;
        //charset = 0;
        facename = "MS Sans Serif";
        //weight = 400;
    }

    if ( _font.ttfFont )
    {
        TTF_CloseFont(_font.ttfFont);
        _font.ttfFont = NULL;
    }

    _font.height = height;
    _font.ttfFont = System::LoadFont(facename, height);

    if ( _font.ttfFont )
    {
        if (!_solidFont)
            TTF_SetFontHinting(_font.ttfFont, TTF_HINTING_LIGHT);
        else
            TTF_SetFontHinting(_font.ttfFont, TTF_HINTING_MONO);

        return 1;
    }

    printf("Can't load font\n");

    return 0;
}


size_t GFXEngine::ReadIniDisplaySettings()
{
    _export_window_mode = System::IniConf::GfxExportWindowMode.Get<bool>();     // gfx.export_window_mode

    switch(System::IniConf::GfxBlending.Get<int>())
    {
        case 0:
        {
            can_srcblend = 1;
            can_destblend = 0;
            can_stippling = 0;
        }
        break;
        
        default:
        case 1:
        {
            can_srcblend = 1;
            can_destblend = 1;
            can_stippling = 0;
        }
        break;
    
        case 2:
        {
            can_srcblend = 0;
            can_destblend = 0;
            can_stippling = 1;
        }
        break;
    }

    SetResVariables( System::GetResolution() );

    _disable_lowres = System::IniConf::GfxDisableLowres.Get<bool>();

    _solidFont = System::IniConf::GfxSolidFont.Get<bool>();

    // The player's setting replaces this once their profile is loaded
    SetVsync( System::IniConf::GfxVsync.Get<int>() != 0 );

    fpsLimitter(System::IniConf::GfxMaxFps.Get<int>());

    LoadFontByDescr("MS Sans Serif,12,400,0");

    //win3d->field_54______rsrc_field4 = (bitmap_intern *)getRsrc_pData();
    return 1;
}


bool GFXEngine::SetResVariables(Common::Point res)
{
    _resolution = res;   //stak.Get<int32_t>(ATT_WIDTH, 0);
    
    _clip = _resolution - Common::Point(1, 1);

    _centerX = _resolution.x / 2;
    _centerY = _resolution.y / 2;

    _normScaleX = _resolution.x / 2;
    _normScaleY = _resolution.y / 2;
    
    return true;
}

size_t GFXEngine::ReadIniSettings()
{
    System::IniConf::ReadFromNucleusIni();

    if ( !ReadIniDisplaySettings() )
        return 0;

    _dither = System::IniConf::GfxDither.Get<bool>();
    _filter = System::IniConf::GfxFilter.Get<bool>();
    _antialias = System::IniConf::GfxAntialias.Get<bool>();
    _zbuf_when_tracy = System::IniConf::GfxZbufWhenTracy.Get<bool>();
    _colorkey = System::IniConf::GfxColorkey.Get<bool>();

    if ( can_srcblend )
        _alpha = 192;
    else
        _alpha = 128;

    ApplyResolution();

    return 1;
}

void GFXEngine::ApplyResolution()
{        
    if ( (float)_resolution.x / (float)_resolution.y >= 1.4 )
    {
        int half = (_resolution.x + _resolution.y) / 2;
        _corrW = (float)half * 1.1429 / (float)_resolution.x;
        _corrH = (float)half * 0.85715 / (float)_resolution.y;
        _corrIW = 1.0 / _corrW;
        _corrIH = 1.0 / _corrH;
    }
    else //No correction
    {
        _corrIW = _corrW = 1.0;
        _corrIH = _corrH = 1.0;
    }
    
    _backend->ResetStates();
}


size_t GFXEngine::DrawLineNormalized(const Common::FLine &arg)
{
    float tX = _normScaleX - 1.0;
    float tY = _normScaleY - 1.0;

    int y1 = (arg.y1 + 1.0) * tY;
    int y2 = (arg.y2 + 1.0) * tY;
    int x1 = (arg.x1 + 1.0) * tX;
    int x2 = (arg.x2 + 1.0) * tX;

    DrawLine(Screen(),
                      Common::Line(x1, y1, x2, y2),
                      _penColor.r,
                      _penColor.g,
                      _penColor.b);
    return 1;
}


size_t GFXEngine::DrawLineCentered(const Common::Line &arg)
{
    DrawLine(Screen(),
                      Common::Line(_centerX + arg.x1, _centerY + arg.y1,
                                   _centerX + arg.x2, _centerY + arg.y2),
                      _penColor.r,
                      _penColor.g,
                      _penColor.b );

    return 1;
}

void GFXEngine::DrawClippedLine(Common::Line line, uint8_t r, uint8_t g, uint8_t b)
{
    if ( line.ClipBy(_clip) )
    {
        Common::Line tmp2 = line;

        if ( _inverseClip.IsEmpty() || !tmp2.ClipBy(_inverseClip) )
        {
            DrawLine(Screen(), line, r, g, b);
        }
        else
        {
            if ( tmp2.P2() != line.P2() )
            {
                if ( tmp2.P1() != line.P1() )
                {
                    DrawLine(Screen(), Common::Line(line.P1(), tmp2.P1()), r, g, b);
                    DrawLine(Screen(), Common::Line(tmp2.P2(), line.P2()), r, g, b);
                }
                else
                {
                    DrawLine(Screen(), Common::Line(tmp2.P2(), line.P2()), r, g, b);
                }
            }
            else
            {
                DrawLine(Screen(), Common::Line(line.P1(), tmp2.P1()), r, g, b);
            }
        }
    }
}

size_t GFXEngine::DrawClippedLineNormalized(const Common::FLine &arg)
{
    float tX = _normScaleX - 1.0;
    float tY = _normScaleY - 1.0;

    DrawClippedLine (Common::Line( (arg.x1 + 1.0) * tX
                            , (arg.y1 + 1.0) * tY
                            , (arg.x2 + 1.0) * tX
                            , (arg.y2 + 1.0) * tY),
                        _penColor.r,
                        _penColor.g,
                        _penColor.b);

    return 1;
}

size_t GFXEngine::DrawClippedLineCentered(const Common::Line &l)
{
    DrawClippedLine( Common::Line( _centerX + l.x1
                            , _centerY + l.y1
                            , _centerX + l.x2
                            , _centerY + l.y2),
                        _penColor.r,
                        _penColor.g,
                        _penColor.b );

    return 1;
}

size_t GFXEngine::DrawBitmapNormalized(TBitmapBlitParams *arg)
{
    Common::Rect r1;
    r1.left   = (arg->srcRect.left   + 1.0) * (arg->pbitm->width / 2);
    r1.top    = (arg->srcRect.top    + 1.0) * (arg->pbitm->height / 2);
    r1.right  = (arg->srcRect.right  + 1.0) * (arg->pbitm->width / 2);
    r1.bottom = (arg->srcRect.bottom + 1.0) * (arg->pbitm->height / 2);

    Common::Rect r2;
    r2.left   = _normScaleX * (arg->dstRect.left   + 1.0);
    r2.top    = _normScaleY * (arg->dstRect.top    + 1.0);
    r2.right  = _normScaleX * (arg->dstRect.right  + 1.0);
    r2.bottom = _normScaleY * (arg->dstRect.bottom + 1.0);
    
    SDL_Rect src = r1;
    SDL_Rect dst = r2;
    
    SDL_BlitSurfaceScaled(arg->pbitm->swTex, &src, Screen(), &dst, SDL_SCALEMODE_NEAREST);
    
    return 1;
}

size_t GFXEngine::DrawClippedBitmapNormalized(TBitmapBlitParams *arg)
{   
    Common::Rect r1;
    r1.left   = (arg->srcRect.left   + 1.0) * (arg->pbitm->width / 2);
    r1.top    = (arg->srcRect.top    + 1.0) * (arg->pbitm->height / 2);
    r1.right  = (arg->srcRect.right  + 1.0) * (arg->pbitm->width / 2);
    r1.bottom = (arg->srcRect.bottom + 1.0) * (arg->pbitm->height / 2);

    Common::Rect r2;
    r2.left   = _normScaleX * (arg->dstRect.left   + 1.0);
    r2.top    = _normScaleY * (arg->dstRect.top    + 1.0);
    r2.right  = _normScaleX * (arg->dstRect.right  + 1.0);
    r2.bottom = _normScaleY * (arg->dstRect.bottom + 1.0);
    
    if ( _clip.IsIntersects(r2) )
    {
        if ( r2.left < _clip.left )
        {
            r1.left += (_clip.left - r2.left) * r1.Width() / r2.Width();
            r2.left = _clip.left;
        }

        if ( r2.right > _clip.right )
        {
            r1.right += (_clip.right - r2.right) * r1.Width() / r2.Width();
            r2.right = _clip.right;
        }

        if ( r2.top < _clip.top )
        {
            r1.top += (_clip.top - r2.top) * r1.Height() / r2.Height();
            r2.top = _clip.top;
        }

        if ( r2.bottom > _clip.bottom )
        {
            r1.bottom += (_clip.bottom - r2.bottom) * r1.Height() / r2.Height();
            r2.bottom = _clip.bottom;
        }
    
        SDL_Rect src = r1;
        SDL_Rect dst = r2;
        SDL_BlitSurfaceScaled(arg->pbitm->swTex, &src, Screen(), &dst, SDL_SCALEMODE_NEAREST);
    }

    return 1;
}

void GFXEngine::QueueRenderMesh(TRenderNode *nod)
{
    if (!nod)
        return;
    
    TMesh *mesh = nod->Mesh;    
    if (!mesh)
        return;
    
    uint32_t flags = nod->Flags;
    
    if (flags & RFLAGS_SKY)
        _renderQueue.SkyBox.push_back(nod);
    else if (flags & RFLAGS_ZEROTRACY)
        _renderQueue.ZeroTracy.push_back(nod);
    else if (flags & RFLAGS_LUMTRACY)
        _renderQueue.LumaTracy.push_back(nod);
    else
        _renderQueue.Solid.push_back(nod);
}

void GFXEngine::Rasterize(uint32_t RasterEtapes)
{
    System::PerfStats::Timer perf(System::PerfStats::SEC_DRAW);

    _backend->Rasterize(_renderQueue, RasterEtapes);
}


void GFXEngine::_SetTileset(int id, TileMap *t)
{
    _tiles[id] = t;
}

TileMap *GFXEngine::_GetTileset(int id)
{
    return _tiles[id];
}

int GFXEngine::GetTilesetID(TileMap *t)
{
    if ( t )
    {
        for (int i = 0; i < 256; i++)
        {
            if (_tiles[i] == t)
                return i;
        }
    }
    return -1;
}

void GFXEngine::ProcessDrawSeq(const CmdStream &drawSeq, const CmdIncludes *includes)
{
    struct CmdStkEntr
    {
        const CmdStream &seq;
        const int32_t pos;
    };
    
    int normalCopy;

    int bytesPerColor = SDL_BYTESPERPIXEL(Screen()->format);

    int32_t curPos = 0;
    const CmdStream *curStream = &drawSeq;
    
    int w_pixels = Screen()->pitch / bytesPerColor;
    TileMap *tile = NULL;

    int x_out = 0;
    int y_out = 0;

    int x_out_txt = 0;
    int y_out_txt = 0;
    int txt_flag = 0;

    int y_pos_line = 0;
    int x_pos_line = 0;

    int halfWidth = _resolution.x / 2;
    int halfHeight = _resolution.y / 2;

    int line_width = 0;
    int line_height = 0;

    //if normalCopy = 0 - clone first column of tile  (count = line_width)
    //if normalCopy = 1 - normal copy of tile
    normalCopy = 1;


    int x_off = 0;
    int y_off = 0;

    std::stack<CmdStkEntr> Stack;

    while ( 1 )
    {
        int glyph = FontUA::get_u8(*curStream, &curPos);

        if ( glyph )
        {
            Common::PointRect &chrr = tile->map[glyph];

            int cpy_width;

            if ( line_width )
                cpy_width = line_width - x_off;
            else
                cpy_width = chrr.w - x_off;

            int cpy_height = line_height - y_off;

            //SDL_Rect srcR, dstR;
            Common::Rect dstR(x_out, y_out, x_out + cpy_width, y_out + cpy_height);            
            /*dstR.x = x_out;
            dstR.y = y_out;
            dstR.w = cpy_width;
            dstR.h = cpy_height;*/
            
            Common::Rect srcR;
            srcR.left = chrr.x + x_off;
            srcR.top = chrr.y + y_off;
            srcR.bottom = chrr.y + y_off + cpy_height;
            
            if (normalCopy)
                srcR.right = chrr.x + x_off + cpy_width;
            else
                srcR.right = chrr.x + x_off + 1;
            
            DrawFill(tile->img->GetSwTex(), srcR, Screen(), dstR);
                    
            /*srcR.h = cpy_height;
            
            if (normalCopy)
                srcR.w = cpy_width;
            else
                srcR.w = 1;
            
            for(int i = 0; i < cpy_width; i += srcR.w)
            {
                SDL_BlitSurface(tile->img->GetSwTex(), &srcR, Screen(), &dstR);
                dstR.x += srcR.w;
            }*/     

            line_width = 0;
            x_off = 0;
            x_out += cpy_width;
            normalCopy = 1;
        }
        else // 0
        {
            int opcode = FontUA::get_u8(*curStream, &curPos);

            switch ( opcode )
            {
            case 0: // End
                
                if (Stack.empty())
                {
                    DrawScreenText();
                    return;
                }
                
                curPos = Stack.top().pos;
                curStream = &Stack.top().seq;
                
                Stack.pop();
                break;

            case 1: // x pos from center
                x_out = halfWidth + FontUA::get_s16(*curStream, &curPos);
                x_pos_line = x_out;

                y_pos_line = y_out;
                y_off = 0;

                line_height = tile->h;
                break;

            case 2: // y pos from center
                y_out = halfHeight + FontUA::get_s16(*curStream, &curPos);
                x_pos_line = x_out;

                y_pos_line = y_out;
                y_off = 0;

                line_height = tile->h;
                break;

            case 3: //xpos
                x_out = FontUA::get_s16(*curStream, &curPos);
                if ( x_out < 0 )
                    x_out += w_pixels;

                x_pos_line = x_out;
                y_pos_line = y_out;

                line_height = tile->h;
                y_off = 0;
                break;

            case 4: //ypos
                y_out = FontUA::get_s16(*curStream, &curPos);
                if ( y_out < 0 )
                    y_out += _resolution.y;

                x_pos_line = x_out;
                y_pos_line = y_out;

                line_height = tile->h;
                y_off = 0;
                break;

            case 5: //add to x pos
                x_out += FontUA::get_s16(*curStream, &curPos);
                break;

            case 6: //add to y pos
                y_out += FontUA::get_s16(*curStream, &curPos);
                break;

            case 7: //next line
                y_out = (line_height - y_off) + y_pos_line;
                y_pos_line = y_out;
                x_out = x_pos_line;

                y_off = 0;
                line_height = tile->h;
                break;

            case 8: // Select tileset
                tile = _tiles[FontUA::get_u8(*curStream, &curPos)];
                break;

            case 9: // Include another cmdlist source
            {
                int azaza = FontUA::get_u8(*curStream, &curPos);
                Stack.push( {*curStream, curPos} );
                curPos = 0;
                curStream = includes->at(azaza);
            }
            break;

            case 10:
                line_width = FontUA::get_u8(*curStream, &curPos);

                normalCopy = 0;
                x_off = 0;

                break;

            case 11:

                line_width = FontUA::get_u8(*curStream, &curPos);

                normalCopy = 0;
                x_off = 0;

                line_width -= (x_out - x_pos_line);
                break;

            case 12: // Set x offset
                x_off = FontUA::get_u8(*curStream, &curPos);
                break;

            case 13: // Set x width
                line_width = FontUA::get_u8(*curStream, &curPos);
                break;

            case 14: // Set y offset
                y_off = FontUA::get_u8(*curStream, &curPos);
                break;

            case 15: // Set y height
                line_height = FontUA::get_u8(*curStream, &curPos);
                break;

            case 16: // Full reset tileset
                tile = _tiles[FontUA::get_u8(*curStream, &curPos)];
                line_height = tile->h;
                y_off = 0;
                break;

            case 17:
                line_width = FontUA::get_s16(*curStream, &curPos);
                normalCopy = 0;
                x_off = 0;
                line_width -= (x_out - x_pos_line);
                break;

            case 18: // Add text
            {
                int block_width = FontUA::get_s16(*curStream, &curPos);
                int flag = txt_flag | FontUA::get_u16(*curStream, &curPos);
                
                int32_t sz = FontUA::get_u16(*curStream, &curPos);

                std::string txt;
                txt.assign((const char *)(curStream->data()) + curPos, sz);
                

                curPos += sz + 1;
                AddScreenText(txt, x_out_txt, y_out_txt, block_width, tile->h, flag);
            }
            break;

            case 19: // Copy current x/y pos for text output
                x_out_txt = x_out;
                y_out_txt = y_out;
                break;

            case 20: // Add txtout flag
                txt_flag |= FontUA::get_u16(*curStream, &curPos);
                break;

            case 21: // Delete txtout flag
                txt_flag &= ~(FontUA::get_u16(*curStream, &curPos));
                break;

            case 22: // set color for font
            {
                int r = FontUA::get_u16(*curStream, &curPos);

                int g = FontUA::get_u16(*curStream, &curPos);

                int b = FontUA::get_u16(*curStream, &curPos);

                AddScreenText("", r, g, b, 0, 0x20);
            }
            break;
            }
        }
    }
}


void GFXEngine::SetClipNormalized(const Common::FRect &arg)
{
    _clip = Common::Rect(   (arg.left + 1.0) * (_normScaleX + -1.0),
                            (arg.top + 1.0) * (_normScaleY + -1.0),
                            (arg.right + 1.0) * (_normScaleX + -1.0),
                            (arg.bottom + 1.0) * (_normScaleY + -1.0) );
    }

void GFXEngine::SetClipCentered(const Common::Rect &arg)
{
    _clip = Common::Rect(   _centerX + arg.left,
                            _centerY + arg.top,
                            _centerX + arg.right,
                            _centerY + arg.bottom );
}


void GFXEngine::BeginScene()
{
    _backend->BeginScene( _frustum );
}

// Draw transparent
void GFXEngine::EndScene()
{
    _backend->EndScene();
    
    _renderQueue.SkyBox.clear();
    _renderQueue.Solid.clear();
    _renderQueue.ZeroTracy.clear();
    _renderQueue.LumaTracy.clear();
    
    _renderNodesCache.Rewind();
}

void GFXEngine::DrawMaskedBitmapNormalized(TMaskedBlitParams *arg)
{
    Common::Rect sRect( (arg->srcRect.left + 1.0) * (arg->bitmap->width / 2),
                        (arg->srcRect.top + 1.0) * (arg->bitmap->height / 2),
                        (arg->srcRect.right + 1.0) * (arg->bitmap->width / 2),
                        (arg->srcRect.bottom + 1.0) * (arg->bitmap->height / 2) );

    Common::Rect dRect( (arg->dstRect.left + 1.0) * _normScaleX,
                        (arg->dstRect.top + 1.0) * _normScaleY,
                        (arg->dstRect.right + 1.0) * _normScaleX,
                        (arg->dstRect.bottom + 1.0) * _normScaleY );

    BlitScaleMasked(arg->bitmap->swTex, sRect, arg->mask->swTex, arg->maskIndex, Screen(), dRect);
}

void GFXEngine::SetInverseClipCentered(const Common::Rect &arg)
{
    _inverseClip.left = _centerX + arg.left;
    _inverseClip.top = _centerY + arg.top;
    _inverseClip.right = _centerX + arg.right;
    _inverseClip.bottom = _centerY + arg.bottom;
}

void GFXEngine::BeginFrame()
{
    System::Perf.FrameStart();

    SDL_FillSurfaceRect(Screen(), NULL, SDL_MapSurfaceRGBA(Screen(), 0, 0, 0, 0) );
    
    _backend->BeginFrame();
}

void GFXEngine::EndFrame()
{       
    uint64_t start = System::PerfStats::Now();
    uint64_t waited = _backend->EndFrame(ScreenSurface);
    uint64_t took = System::PerfStats::Now() - start;

    System::Perf.AddTime(System::PerfStats::SEC_PRESENT, took - waited);
    System::Perf.AddTime(System::PerfStats::SEC_WAIT, waited);
}

void GFXEngine::SetColorEffectsPowers(const std::vector<ColorFx> &arg)
{    
    if (arg.empty())
    {
        _normClr = vec3d(1.0, 1.0, 1.0);
        _invClr = vec3d(0.0, 0.0, 0.0);
    }
    else
    {
        _normClr = vec3d(0.0, 0.0, 0.0);
        _invClr = vec3d(0.0, 0.0, 0.0);

        for (ColorFx fx : arg)
        {
            switch(fx.Id)
            {
                case 4:
                case 5:
                case 7:
                    _invClr += _clrEff.at( fx.Id ) * fx.Pwr;
                    break;
                default:
                    _normClr += _clrEff.at( fx.Id ) * fx.Pwr;
                    break;
            }
        }
    }
}

bool GFXEngine::AllocTexture(ResBitmap *bitm)
{
    return _backend->AllocTexture(bitm);
}

void GFXEngine::FreeTexture(ResBitmap *bitm)
{
    _backend->FreeTexture(bitm);
}

void GFXEngine::WriteScreenshotPPM(FSMgr::FileHandle *fil)
{
    int bf_w = 0, bf_h = 0;

    uint8_t *buf = MakeScreenCopy(&bf_w, &bf_h);

    if (buf && bf_w && bf_h)
    {
        fil->printf("P6\n#YPA screenshot\n%d %d\n255\n", bf_w, bf_h);

        int lwidth = 3 * bf_w;

        for (int j = 0; j < bf_h; j++)
        {
            uint8_t *ln = buf + lwidth * (bf_h - 1 - j);

            for (int i = 0; i < bf_w; i++)
            {
                uint8_t *px = ln + i * 3;

                uint8_t r = px[0];
                uint8_t g = px[1];
                uint8_t b = px[2];

                fil->writeU8(r);
                fil->writeU8(g);
                fil->writeU8(b);
            }
        }

        free(buf);
    }
}

void GFXEngine::SaveScreenshot(const std::string & screenName)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc(fmt::sprintf("%s.ppm", screenName), "wb");
    if ( fil )
    {
        WriteScreenshotPPM(fil);
        delete fil;
    }
}


void GFXEngine::BeginSystemDialog(IDVPair *)
{
}

void GFXEngine::EndSystemDialog(IDVPair *)
{
}





char * GFXEngine::RunTextInputDialog(const char *box_title, const char *box_ok, const char *box_cancel, const char *box_startText, uint32_t timer_time, void (*timer_func)(int, int, int), void *timer_context, int replace, int maxLen)
{
    dprintf("MAKE ME %s\n","RunTextInputDialog");
    return NULL;
}

//Show DLGBox with edit field and get entered value
void GFXEngine::ShowTextInputDialog(windd_dlgBox *dlgBox)
{
    BeginSystemDialog(NULL);

    dlgBox->result = RunTextInputDialog(
                         dlgBox->title,
                         dlgBox->ok,
                         dlgBox->cancel,
                         dlgBox->startText,
                         dlgBox->time,
                         dlgBox->timer_func,
                         dlgBox->timer_context,
                         dlgBox->replace,
                         dlgBox->maxLen);

    EndSystemDialog(NULL);
}



const std::vector<TGFXDeviceInfo>& GFXEngine::GetDevices()
{
    return _devices;
}

void GFXEngine::SetDeviceByGUID(const std::string &guid, bool writefile)
{
    // An unknown GUID, such as "<primary>" from before there were two
    // renderers, chooses the one in use
    TGFXDeviceInfo *chosen = NULL;

    for (TGFXDeviceInfo &dev : _devices)
    {
        if (dev.guid == guid)
            chosen = &dev;
    }

    for (TGFXDeviceInfo &dev : _devices)
    {
        if (!chosen && dev.isCurrent)
            chosen = &dev;
    }

    for (TGFXDeviceInfo &dev : _devices)
        dev.isChosen = (&dev == chosen);

    if (writefile && chosen)
        out_guid_to_file("env/guid3d.def", chosen->guid);
}



void GFXEngine::setWDD_disLowRes(int arg)
{
}


void GFXEngine::setW3D_texFilt(int arg)
{
    _filter = arg;
}

void GFXEngine::SetPalette(UA_PALETTE &newPal)
{
    _palette = newPal;
    
    _normClr = vec3d(1.0, 1.0, 1.0);
    _invClr = vec3d(0.0, 0.0, 0.0);
}

void GFXEngine::SetPen(SDL_Color pen)
{
    _penColor = pen;
}

UA_PALETTE *GFXEngine::GetPalette()
{
    return &_palette;
}




void GFXEngine::draw2DandFlush()
{
    _backend->FlushScreenSurface(ScreenSurface);

    SDL_FillSurfaceRect(Screen(), NULL, SDL_MapSurfaceRGBA(Screen(), 0, 0, 0, 0) );
}

void GFXEngine::matrixAspectCorrection(mat3x3 &inout, bool invert)
{
    if (invert)
    {
        inout.m00 *= _corrIW;
        inout.m01 *= _corrIW;
        inout.m02 *= _corrIW;
        inout.m10 *= _corrIH;
        inout.m11 *= _corrIH;
        inout.m12 *= _corrIH;
    }
    else
    {
        inout.m00 *= _corrW;
        inout.m01 *= _corrW;
        inout.m02 *= _corrW;
        inout.m10 *= _corrH;
        inout.m11 *= _corrH;
        inout.m12 *= _corrH;
    }
}

void GFXEngine::getAspectCorrection(float &cW, float &cH, bool invert)
{
    if (invert)
    {
        cW = _corrIW;
        cH = _corrIH;
    }
    else
    {
        cW = _corrW;
        cH = _corrH;
    }
}

void GFXEngine::setFrustumClip(float _near, float _far)
{
    if (_near != _frustumNear || _far != _frustumFar)
        _setFrustumClip(_near, _far);
}

void GFXEngine::_setFrustumClip(float _near, float _far)
{
    //-z * frustum
    _frustumNear = _near;
    _frustumFar = _far;
    
    _frustum = mat4x4f::UAFrustum(_near, _far);
}

void GFXEngine::ConvAlphaPalette(UA_PALETTE *dst, const UA_PALETTE &src, bool transp)
{
    for (uint16_t i = 0; i < dst->size(); i++)
    {
        SDL_Color &c = (*dst)[i];
        c = src[i];
        c.a = 255;

        if (c.r == 255 && c.g == 255 && c.b == 0)
        {
            c.a = 0;
            c.r = 0;
            c.g = 0;
            c.b = 0;
        }
        else
        {
            if (!can_destblend && can_srcblend && transp)
            {
                int mx = (c.r >= c.g) ? (c.r > c.b ? c.r: c.b) : (c.g > c.b ? c.g : c.b);

                if (mx <= 8)
                {
                    c.r = 0;
                    c.g = 0;
                    c.b = 0;
                    c.a = 0;
                }
                else
                {
                    float prm = mx;
                    c.r = 255.0 * (c.r / prm);
                    c.g = 255.0 * (c.g / prm);
                    c.b = 255.0 * (c.b / prm);
                    c.a = mx;
                }
            }
            else
            {
                c.a = 255;
            }
        }
    }
}

SDL_Surface *GFXEngine::CreateSurfaceScreenFormat(int width, int height)
{
    return SDL_CreateSurface(width, height, _pixfmt->format);
}

SDL_Surface *GFXEngine::CreateIndexedSurface(int width, int height)
{
    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_INDEX8);

    // Unlike SDL2's, SDL3's indexed surfaces come without a palette.
    if (surface && !SDL_CreateSurfacePalette(surface))
    {
        SDL_DestroySurface(surface);
        return NULL;
    }

    return surface;
}

SDL_Surface *GFXEngine::ConvertToScreenFormat(SDL_Surface *src)
{
    return ConvertSDLSurface(src, _pixfmt->format);
}

SDL_Surface * GFXEngine::ConvertSDLSurface(SDL_Surface *src, SDL_PixelFormat fmt)
{
    return SDL_ConvertSurface(src, fmt);
}

void GFXEngine::fpsLimitter(int value)
{
    _fpsMaxSetting = value;
    UpdateFpsLimit();
}

// gfx.maxfps 0 caps the frame rate a quarter above the refresh rate of the
// window's display: vsync paces the frames, and the cap only stops the game
// from spinning when vsync does not wait, as for a hidden window.
void GFXEngine::UpdateFpsLimit()
{
    int fps = _fpsMaxSetting;

    if (fps == 0)
    {
        int refresh = System::GetRefreshRate();
        if (refresh <= 0)
            refresh = 200; // Unknown, assume a fast display

        fps = refresh + refresh / 4;
    }

    if (fps <= 0 || fps > 1000)
        FpsMaxTicks = 0;
    else
        FpsMaxTicks = 1000 / fps;
}

void GFXEngine::SetVsync(bool enabled)
{
    _backend->SetVsync(enabled);
}

std::string GFXEngine::GetRendererInfo()
{
    std::string cap = "none";
    if ( FpsMaxTicks )
        cap = fmt::sprintf("%d fps", 1000 / FpsMaxTicks);

    if ( _fpsMaxSetting == 0 )
        cap += " (auto)";

    return fmt::sprintf("%s, %dx%d, vsync %s, fps cap %s",
                        _backend->GetInfo(),
                        _resolution.x, _resolution.y, _backend->GetVsyncMode(), cap);
}


}


TileMap::TileMap()
{
    img = NULL;
    h = 0;
}
    
TileMap::~TileMap()
{
    if (img)
        img->Delete();
}

void TileMap::Draw(SDL_Surface *surface, const Common::Point &pos, uint8_t c)
{
    SDL_Rect src = map[c];
    SDL_Rect dst = pos;
    SDL_BlitSurface(img->GetSwTex(), &src, surface, &dst);
}

void TileMap::Draw(SDL_Surface *surface, const Common::PointRect &pos, uint8_t c)
{
    SDL_Rect src = map[c];
    if (src.w > pos.w)
        src.w = pos.w;
    if (src.h > pos.h)
        src.h = pos.h;    
    SDL_Rect dst = pos;
    SDL_BlitSurface(img->GetSwTex(), &src, surface, &dst);
}

void TileMap::Draw(SDL_Surface *surface, const Common::Rect &pos, uint8_t c)
{
    SDL_Rect src = map[c];
    if (src.w > pos.Width())
        src.w = pos.Width();
    if (src.h > pos.Height())
        src.h = pos.Height();    
    SDL_Rect dst = pos;
    SDL_BlitSurface(img->GetSwTex(), &src, surface, &dst);
}

void TileMap::Fill(SDL_Surface *surface, const Common::Rect &rect, uint8_t c)
{
    GFX::Engine.DrawFill(img->GetSwTex(), map[c], surface, rect);
}

void TileMap::Fill(SDL_Surface *surface, const Common::PointRect &rect, uint8_t c)
{
    GFX::Engine.DrawFill(img->GetSwTex(), map[c], surface, rect);
}

void TileMap::FillColumn(SDL_Surface *surface, const Common::Rect &rect, uint8_t c)
{
    Common::PointRect sPRect = map[c];
    sPRect.w = 1;
    GFX::Engine.DrawFill(img->GetSwTex(), sPRect, surface, rect);
}

void TileMap::FillColumn(SDL_Surface *surface, const Common::PointRect &rect, uint8_t c)
{
    Common::PointRect sPRect = map[c];
    sPRect.w = 1;
    GFX::Engine.DrawFill(img->GetSwTex(), sPRect, surface, rect);
}

int TileMap::GetWidth(uint8_t c) const
{
    return map[c].w;
}

int TileMap::GetWidth(const std::string &str) const
{
    int wdth = 0;
    for (uint8_t c : str)
        wdth += map[c].w;
    return wdth;
}

Common::Point TileMap::GetSize(uint8_t c) const
{
    return Common::Point(map[c].w, h);
}












namespace GFX
{


uint32_t GFXEngine::CursPix(uint8_t *data, int ofs, int bpp)
{
    switch (bpp)
    {
    case 1:
        return (data[ofs / 8] >> (7 - ofs % 8)) & 1;
    case 2:
        return (data[ofs / 4] >> ((3 - ofs % 4) << 1)) & 3;
    case 4:
        return (data[ofs / 2] >> ((1 - ofs % 2) << 2)) & 15;
    case 8:
        return data[ofs];
    case 16:
        return data[2 * ofs] | data[2 * ofs + 1] << 8;
    case 24:
        return data[3 * ofs] | data[3 * ofs + 1] << 8 | data[ 3 * ofs + 2] << 16;
    case 32:
        return data[4 * ofs] | data[4 * ofs + 1] << 8 | data[4 * ofs + 2] << 16 | data[4 * ofs + 3] << 24;
    }

    return 0;
}

SDL_Cursor *GFXEngine::LoadCursor(const std::string &name)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc( fmt::sprintf("res/%s.cur", name) , "rb");

    UA_PALETTE pal;

    if (!fil)
        return NULL;

    fil->readU16L();
    if (fil->readU16L() != 2)
    {
        delete fil;
        return NULL;
    }

    fil->readU16L(); //count

    //Only first entry
    fil->readU8(); //w
    fil->readU8(); //h
    fil->readU8(); //color count
    fil->readU8(); //reserved
    int hotX = fil->readU16L();
    int hotY = fil->readU16L();
    fil->readU32L(); //size
    int off = fil->readU32L();

    //seek to cursor
    fil->seek(off, SEEK_SET);

    //read InfoHeader
    fil->readU32L(); //header size
    int bmpw = fil->readS32L();
    int bmph = fil->readS32L();
    fil->readU16L(); //planes
    int bitcount = fil->readU16L();
    fil->readU32L(); //compression
    fil->readU32L(); //imagesize
    fil->readU32L(); //XpixelsPerM
    fil->readU32L(); //YpixelsPerM
    int clrused = fil->readU32L(); //ColorsUsed
    fil->readU32L(); //ColorsImportant

    // read pallete
    int palcnt = 0;
    if (clrused == 0 || bitcount < 16)
    {
        palcnt = clrused != 0 ? clrused : 1 << bitcount;

        for (int i = 0; i < palcnt; i++)
        {
            pal[i].r = fil->readU8();
            pal[i].g = fil->readU8();
            pal[i].b = fil->readU8();
            fil->readU8(); //reserved
        }
    }

    int width = bmpw;
    int height = abs(bmph)/2;

    int imgsz = height * width * bitcount / 8;
    int mask_size = height * width / 8;

    uint8_t *data = (uint8_t *)malloc(imgsz);
    uint8_t *mask = (uint8_t *)malloc(mask_size);

    fil->read(data, imgsz);
    fil->read(mask, mask_size);

    delete fil;


    SDL_Surface *cursr = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ABGR8888); // Bytes R, G, B, A

    for (int y = 0; y < height; y++)
    {
        int invY = height - 1 - y;
        uint8_t *row = (uint8_t *)cursr->pixels + y * cursr->pitch;

        for (int x = 0; x < width; x++)
        {
            if (palcnt > 0)
            {
                int idx = CursPix(data, invY * width + x, bitcount);
                int alph = CursPix(mask, invY * width + x, 1);
                row[x * 4 + 0] = pal[ idx ].r;
                row[x * 4 + 1] = pal[ idx ].g;
                row[x * 4 + 2] = pal[ idx ].b;
                row[x * 4 + 3] = (1 - alph) * 255;
            }
            else
            {
                uint32_t clr = CursPix(data, invY * width + x, bitcount);
                int alph = CursPix(mask, invY * width + x, 1);
                row[x * 4 + 0] = clr & 0xFF;
                row[x * 4 + 1] = (clr >> 8) & 0xFF;
                row[x * 4 + 2] = (clr >> 16) & 0xFF;
                row[x * 4 + 3] = (1 - alph) * 255;
            }
        }
    }

    free(data);
    free(mask);

    SDL_Cursor* cursor = SDL_CreateColorCursor(cursr, hotX, hotY);

    SDL_DestroySurface(cursr);

    return cursor;
}

void GFXEngine::Init(bool headless)
{
    StaticInit();

    if (headless)
    {
        _backend = new NullBackend();
        _backend->Init(NULL);

        // The options' renderer list shows the one in use
        _devices.emplace_back("None", "<none>");
        _devices.back().isCurrent = true;
        _devices.back().isChosen = true;

        ReadIniSettings();
        RecreateScreenSurface();
        LoadPalette(System::IniConf::GfxPalette.Get<std::string>());
        return;
    }

    // The renderer chosen in the options, else gfx.renderer, unless
    // --renderer on the command line says otherwise: "gpu" or "gl"
    std::string chosen = read_guid("env/guid3d.def");
    std::string renderer = System::IniConf::GfxRenderer.Get<std::string>();

    if (chosen == DEVICE_GPU)
        renderer = "gpu";
    else if (chosen == DEVICE_GL)
        renderer = "gl";

    System::CmdLine::Get("renderer", &renderer);

    // It lives until the program ends: objects destroyed at exit still free
    // their textures and buffers through it.
    _backend = NULL;
    bool gpuInUse = false;

#ifdef UA_GPU_RENDERER
    std::string gpuDriver;

    if (renderer != "gl")
    {
        GPUBackend *gpu = GPUBackend::Create();
        if (gpu)
        {
            gpuDriver = gpu->GetDriver();
            gpuInUse = true;
            _backend = gpu;
        }
    }

    // SDL_GPU is in the options whenever the system has a device for it
    if (!gpuInUse)
        gpuDriver = GPUBackend::AvailableDriver();

    if (!gpuDriver.empty())
    {
        _devices.emplace_back(GPUBackend::DeviceName(gpuDriver), DEVICE_GPU);
        _devices.back().isCurrent = gpuInUse;
    }
#endif

    if (!_backend)
        _backend = new GLBackend();

    _devices.emplace_back("OpenGL", DEVICE_GL);
    _devices.back().isCurrent = !gpuInUse;

    SDL_PropertiesID windowProps = SDL_CreateProperties();
    _backend->PrepareWindow(windowProps);
    System::OpenWindow(windowProps);
    SDL_DestroyProperties(windowProps);

    // Not saved: should SDL_GPU fail, using OpenGL instead must not stick
    SetDeviceByGUID(chosen, false);
    
    System::EventsAddHandler(EventsWatcher);
    
    System::IniConf::ReadFromNucleusIni();
    
    _backend->Init( System::GetWindow() );
    
    cursors[0] = LoadCursor("Pointer");
    cursors[1] = LoadCursor("Cancel");
    cursors[2] = LoadCursor("Select");
    cursors[3] = LoadCursor("Attack");
    cursors[4] = LoadCursor("Goto");
    cursors[5] = LoadCursor("Disk");
    cursors[6] = LoadCursor("New");
    cursors[7] = LoadCursor("Add");
    cursors[8] = LoadCursor("Control");
    cursors[9] = LoadCursor("Beam");
    cursors[10] = LoadCursor("Build");

    ReadIniSettings();

    RecreateScreenSurface();
    
    LoadPalette(System::IniConf::GfxPalette.Get<std::string>());
}

void GFXEngine::Deinit()
{
    if ( _font.ttfFont )
    {
        TTF_CloseFont(_font.ttfFont);
        _font.ttfFont = NULL;
    }
    
    if (ScreenSurface)
        SDL_DestroySurface(ScreenSurface);
    
    ScreenSurface = NULL;
    
    if (_backend)
        _backend->Deinit();
}

GFXEngine::~GFXEngine()
{
    Deinit();
}

bool GFXEngine::EventsWatcher(void *, SDL_Event *event)
{
    switch(event->type)
    {
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        Instance._backend->WindowResized();
        break;

    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        Instance.UpdateFpsLimit(); // The new display may refresh at another rate
        break;

    default:
        break;
    }
    
    return true; // This event can be passed to another event watcher
}

void GFXEngine::SetCursor(int curID, int force)
{
    int sett = 0;

    if ( force )
        sett = 1;
    else if ( curID != CurrentCursorID )
        sett = 1;

    if ( sett )
    {
        if ( curID == 0 )
            SDL_HideCursor();
        else if ( curID <= 11 )
        {
            if ( cursors[curID - 1] )
                SDL_SetCursor( cursors[curID - 1] );

            if (!CurrentCursorID)
                SDL_ShowCursor();
        }

    }

    CurrentCursorID = curID;
}

void GFXEngine::SetFullscreen(bool fullscreen)
{
    System::SetFullscreen(fullscreen);
    UpdateResolution();
}

bool GFXEngine::UpdateResolution()
{
    Common::Point res = System::GetResolution();

    if (res == _resolution || res.x <= 0 || res.y <= 0)
        return false;

    SetResVariables(res);
    ApplyResolution();
    RecreateScreenSurface();
    _backend->WindowResized();

    return true;
}

void GFXEngine::SetTracyRmp(ResBitmap *rmp)
{
    setRSTR_trcRmp(rmp);
}

void GFXEngine::SetShadeRmp(ResBitmap *rmp)
{
    setRSTR_shdRmp(rmp);
}

TileMap * GFXEngine::GetTileset(int id)
{
    return _GetTileset(id);
}

void GFXEngine::SetTileset(TileMap *tileset, int id)
{
    _SetTileset(id, tileset);
}


bool GFXEngine::LoadPalette(const std::string &palette_ilbm)
{
    NC_STACK_bitmap *ilbm = Nucleus::CInit<NC_STACK_ilbm>( {
        {NC_STACK_rsrc::RSRC_ATT_NAME, std::string(palette_ilbm)},
        {NC_STACK_bitmap::BMD_ATT_HAS_COLORMAP, (int32_t)1}} );

    if (!ilbm)
        return false;

    SetPalette( *ilbm->getBMD_palette() );

    ilbm->Delete();

    return true;
}
    
uint8_t *GFXEngine::MakeScreenCopy(int *ow, int *oh)
{
    return _backend->ReadScreen(ow, oh);
}



SDL_Surface *GFXEngine::Screen()
{
    return ScreenSurface;
}



// Draw line Bresenham's algorithm
void GFXEngine::DrawLine(SDL_Surface *surface, const Common::Line &line, uint8_t cr, uint8_t cg, uint8_t cb )
{
    if ((line.Width() == 0 && line.Height() == 0) || 
         !Common::Rect(surface->w, surface->h).IsIn(line.P1()) ||
         !Common::Rect(surface->w, surface->h).IsIn(line.P2()) )
        return;
    
    int rilWidth = surface->pitch / SDL_BYTESPERPIXEL(surface->format);

    int xCount = line.Width();
    int yCount = line.Height();

    uint32_t color = SDL_MapSurfaceRGBA(surface, cr, cg, cb, 255);

    int stepAdd, stepOdd;
    int steps, subSteps;

    if ( xCount <= yCount )
    {
        if ( line.y2 <= line.y1 )
            stepAdd = -rilWidth;
        else
            stepAdd = rilWidth;

        if ( line.x2 <= line.x1 )
            stepOdd = -1;
        else
            stepOdd = 1;

        steps = yCount;
        subSteps = xCount;
    }
    else
    {
        if ( line.x2 <= line.x1 )
            stepAdd = -1;
        else
            stepAdd = 1;

        if ( line.y2 <= line.y1 )
            stepOdd = -rilWidth;
        else
            stepOdd = rilWidth;
        
        steps = xCount;
        subSteps = yCount;
    }

    int incr1 = 2 * subSteps;
    int t = 2 * subSteps - steps;
    int incr2 = 2 * (subSteps - steps);
    
    SDL_LockSurface(surface);
    
    void *surfPos = (void *) ((uint8_t *) surface->pixels 
                    + line.y1 * surface->pitch 
                    + line.x1 * SDL_BYTESPERPIXEL(surface->format) );

    switch(SDL_BYTESPERPIXEL(surface->format))
    {
        case 1:
        {
            uint8_t *surf = (uint8_t *)surfPos;

            for (int i = 0; i <= steps; i++) // Verify i bound
            {
                *surf = color;
                if ( t > 0 )
                {
                    t += incr2;
                    surf += stepOdd;
                }
                else
                    t += incr1;

                surf += stepAdd;
            }
        }
        break;
        
        case 2:
        {
            uint16_t *surf = (uint16_t *)surfPos;

            for (int i = 0; i <= steps; i++) // Verify i bound
            {
                *surf = color;
                if ( t > 0 )
                {
                    t += incr2;
                    surf += stepOdd;
                }
                else
                    t += incr1;

                surf += stepAdd;
            }
        }
        break;
        
        case 4:
        {
            uint32_t *surf = (uint32_t *)surfPos;

            for (int i = 0; i <= steps; i++) // Verify i bound
            {
                *surf = color;
                if ( t > 0 )
                {
                    t += incr2;
                    surf += stepOdd;
                }
                else
                    t += incr1;

                surf += stepAdd;
            }
        }
        break;
        
        default:
        break;
    }
    
    SDL_UnlockSurface(surface);
}

void GFXEngine::BlitScaleMasked(SDL_Surface *src, Common::Rect sRect, SDL_Surface *mask, uint8_t index, SDL_Surface *dst, Common::Rect dRect)
{
    if (SDL_BITSPERPIXEL(mask->format) != 8)
        return;
        
    if (src->w != mask->w || src->h != mask->h)
        return;
    
    if (sRect.IsEmpty() || !sRect.IsValid())
        sRect = Common::Rect(src->w, src->h);
    else if (!Common::Rect(src->w, src->h).IsIn(sRect))
        return;
    
    if (dRect.IsEmpty() || !dRect.IsValid())
        dRect = Common::Rect(dst->w, dst->h);
    else if (!Common::Rect(dst->w, dst->h).IsIn(dRect))
        return;
    
    // Try fast
    if (src->format == dst->format)
    {
        switch(SDL_BYTESPERPIXEL(src->format))
        {
            case 2:
            {
                SDL_LockSurface(src);
                SDL_LockSurface(mask);
                SDL_LockSurface(dst);
                
                int32_t dY = (sRect.Height() << 16) / dRect.Height();
                int32_t dX = (sRect.Width()  << 16) / dRect.Width();
                
                int32_t srcY  = sRect.top << 16;
                for (int y = dRect.top; y < dRect.bottom; y++)
                {
                    uint16_t *dBuf = (uint16_t *)((uint8_t *)dst->pixels + y * dst->pitch) + dRect.left;
                    uint16_t *sBuf = (uint16_t *)((uint8_t *)src->pixels + (srcY >> 16) * src->pitch) + sRect.left;
                    uint8_t  *mBuf = (uint8_t *)mask->pixels + (srcY >> 16) * mask->pitch + sRect.left;
                    
                    int32_t xx = 0;
                    for (int x = dRect.left; x < dRect.right; x++)
                    {
                        if (mBuf[xx >> 16] == index)
                            *dBuf = sBuf[xx >> 16];
                        dBuf++;
                        xx += dX;
                    }
                    srcY += dY;
                }
                
                SDL_UnlockSurface(dst);
                SDL_UnlockSurface(mask);
                SDL_UnlockSurface(src);
            }
            break;
            
            case 4:
            {
                SDL_LockSurface(src);
                SDL_LockSurface(mask);
                SDL_LockSurface(dst);
                
                int32_t dY = (sRect.Height() << 16) / dRect.Height();
                int32_t dX = (sRect.Width()  << 16) / dRect.Width();
                
                int32_t srcY  = sRect.top << 16;
                for (int y = dRect.top; y < dRect.bottom; y++)
                {
                    uint32_t *dBuf = (uint32_t *)((uint8_t *)dst->pixels + y * dst->pitch) + dRect.left;
                    uint32_t *sBuf = (uint32_t *)((uint8_t *)src->pixels + (srcY >> 16) * src->pitch) + sRect.left;
                    uint8_t  *mBuf = (uint8_t *)mask->pixels + (srcY >> 16) * mask->pitch + sRect.left;
                    
                    int32_t xx = 0;
                    for (int x = dRect.left; x < dRect.right; x++)
                    {
                        if (mBuf[xx >> 16] == index)
                            *dBuf = sBuf[xx >> 16];
                        dBuf++;
                        xx += dX;
                    }
                    srcY += dY;
                }
                
                SDL_UnlockSurface(dst);
                SDL_UnlockSurface(mask);
                SDL_UnlockSurface(src);
            }
            break;
            
            default:
            break;
        }
    }
    else // Slow
    {
        SDL_LockSurface(src);
        SDL_LockSurface(mask);
        SDL_LockSurface(dst);

        uint8_t sbpp = SDL_BYTESPERPIXEL(src->format);
        uint8_t dbpp = SDL_BYTESPERPIXEL(dst->format);

        const SDL_PixelFormatDetails *sfmt = SDL_GetPixelFormatDetails(src->format);
        const SDL_PixelFormatDetails *dfmt = SDL_GetPixelFormatDetails(dst->format);
        SDL_Palette *spal = SDL_GetSurfacePalette(src);
        SDL_Palette *dpal = SDL_GetSurfacePalette(dst);
        
        int32_t dY = (sRect.Height() << 16) / dRect.Height();
        int32_t dX = (sRect.Width()  << 16) / dRect.Width();

        int32_t srcY  = sRect.top << 16;
        for (int y = dRect.top; y < dRect.bottom; y++)
        {
            uint8_t *dBuf = (uint8_t *)dst->pixels + y * dst->pitch + dRect.left * dbpp;
            uint8_t *sBuf = (uint8_t *)src->pixels + (srcY >> 16) * src->pitch + sRect.left * sbpp;
            uint8_t  *mBuf = (uint8_t *)mask->pixels + (srcY >> 16) * mask->pitch + sRect.left;

            int32_t xx = 0;
            for (int x = dRect.left; x < dRect.right; x++)
            {
                if (mBuf[xx >> 16] == index)
                {
                    uint8_t r,g,b;
                    uint32_t clr = 0;
                    
                    uint8_t *spix = sBuf + (xx >> 16) * sbpp;
                    for(int i = 0; i < sbpp; i++)
                        clr |= spix[i] << (i * 8);
                    
                    SDL_GetRGB(clr, sfmt, spal, &r, &g, &b);
                    clr = SDL_MapRGB(dfmt, dpal, r, g, b);
                    
                    for(int i = 0; i < dbpp; i++)
                        dBuf[i] = (clr >> (i * 8)) & 0xFF;
                }
                dBuf += dbpp;
                xx += dX;
            }
            srcY += dY;
        }

        SDL_UnlockSurface(dst);
        SDL_UnlockSurface(mask);
        SDL_UnlockSurface(src);
    }
}

void GFXEngine::DrawFillIntCpy(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect)
{
    const int32_t sh = sRect.Height();
    const int32_t sw = sRect.Width();
    const int32_t dh = dRect.Height();
    const int32_t dw = dRect.Width();
        
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
                    
    if (dw > sw)
    {
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);
        
        const int32_t nit = dw / sw;
        const int32_t nit2 = (dw % sw) * SDL_BYTESPERPIXEL(dst->format);
        const int32_t cpsz = sw * SDL_BYTESPERPIXEL(dst->format);
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format) + nit2;
                
        int sptch = src->pitch;
        
        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {
            for(int n = nit; n > 0; --n)
            {
                memcpy(pdst, psrcl, cpsz);
                pdst += cpsz;
            }
            
            memcpy(pdst, psrcl, nit2);
            pdst += skipdst;
            
            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;
        }        
    }
    else
    {
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);
        
        const int32_t cpsz = dw * SDL_BYTESPERPIXEL(dst->format);
        const int32_t skipdst = dst->pitch;
                
        int sptch = src->pitch;
        
        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {            
            memcpy(pdst, psrcl, cpsz);
            pdst += skipdst;
            
            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;
        }        
    }
                
    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}
    
template <typename T>
void GFXEngine::DrawFillIntCKey(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect)
{
    const int32_t sh = sRect.Height();
    const int32_t sw = sRect.Width();
    const int32_t dh = dRect.Height();
    const int32_t dw = dRect.Width();
        
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    
    uint32_t clrKey = 0;
    SDL_GetSurfaceColorKey(src, &clrKey);
                
    if (dw > sw)
    {
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format);
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);
        
        const int32_t nit = dw / sw;
        const int32_t nit2 = dw % sw;
        
        const int sz = SDL_BYTESPERPIXEL(dst->format);
        
        int sptch = src->pitch;
        
        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {
            
            for(int n = nit; n > 0; --n)
            {
                const uint8_t *lsrc = psrcl;
                for (int iw = sw; iw > 0; --iw)
                {
                    if (*(T *)lsrc != clrKey)
                        *(T *)pdst = *(T *)lsrc;
                    
                    lsrc += sz;
                    pdst += sz;
                }
            }
            
            uint8_t *rsrc = psrcl;            
            for(int iw = nit2; iw > 0; --iw)
            {
                if (*(T *)rsrc != clrKey)
                    *(T *)pdst = *(T *)rsrc;

                rsrc += sz;
                pdst += sz;
            }
            
            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;
            
            pdst += skipdst;
        }        
    }
    else
    {
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format);
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);
        
        const int sz = SDL_BYTESPERPIXEL(dst->format);
        
        int sptch = src->pitch;
        
        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {            
            uint8_t *rsrc = psrcl;            
            for(int iw = dw; iw > 0; --iw)
            {
                if (*(T *)rsrc != clrKey)
                    *(T *)pdst = *(T *)rsrc;

                rsrc += sz;
                pdst += sz;
            }
            
            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;
            
            pdst += skipdst;
        }        
    }
                
    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}

template <typename T>
void GFXEngine::DrawFillIntACpy(SDL_Surface* src, const Common::Rect& sRect, SDL_Surface* dst, const Common::Rect& dRect)
{
    const int32_t sh = sRect.Height();
    const int32_t sw = sRect.Width();
    const int32_t dh = dRect.Height();
    const int32_t dw = dRect.Width();
        
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    
    if (dw > sw)
    {
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format);
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);
        
        const int32_t nit = dw / sw;
        const int32_t nit2 = dw % sw;
        uint32_t amask = SDL_GetPixelFormatDetails(src->format)->Amask;
        
        const int sz = SDL_BYTESPERPIXEL(dst->format);
        
        int sptch = src->pitch;
        
        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {
            
            for(int n = nit; n > 0; --n)
            {
                const uint8_t *lsrc = psrcl;
                for (int iw = sw; iw > 0; --iw)
                {
                    if (*(T *)lsrc & amask)
                        *(T *)pdst = *(T *)lsrc;
                    
                    lsrc += sz;
                    pdst += sz;
                }
            }
            
            uint8_t *rsrc = psrcl;            
            for(int iw = nit2; iw > 0; --iw)
            {
                if (*(T *)rsrc & amask)
                    *(T *)pdst = *(T *)rsrc;

                rsrc += sz;
                pdst += sz;
            }
            
            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;
            
            pdst += skipdst;
        }        
    }
    else
    {
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format);
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);
        
        const int sz = SDL_BYTESPERPIXEL(dst->format);
        
        int sptch = src->pitch;
        uint32_t amask = SDL_GetPixelFormatDetails(src->format)->Amask;
        
        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {            
            uint8_t *rsrc = psrcl;            
            for(int iw = dw; iw > 0; --iw)
            {
                if (*(T *)rsrc & amask)
                    *(T *)pdst = *(T *)rsrc;

                rsrc += sz;
                pdst += sz;
            }
            
            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;
            
            pdst += skipdst;
        }        
    }
                
    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}

void GFXEngine::DrawFill(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect)
{
    if (sRect.IsEmpty() || dRect.IsEmpty())
        return;
    
    if (src->format == dst->format)
    {     
        if (SDL_SurfaceHasColorKey(src))
        {
            switch(SDL_BYTESPERPIXEL(src->format))
            {

                case 2:
                    DrawFillIntCKey<uint16_t>(src, sRect, dst, dRect);
                    break;

                case 4:
                    DrawFillIntCKey<uint32_t>(src, sRect, dst, dRect);
                    break;

                default:
                break;
            }
        }
        else
        {
            if (SDL_ISPIXELFORMAT_ALPHA(src->format))
            {
                switch(SDL_BYTESPERPIXEL(src->format))
                {

                    case 2:
                        DrawFillIntACpy<uint16_t>(src, sRect, dst, dRect);
                        break;

                    case 4:
                        DrawFillIntACpy<uint32_t>(src, sRect, dst, dRect);
                        break;

                    default:
                    break;
                }
            }
            else
                DrawFillIntCpy(src, sRect, dst, dRect);
        }  
    }
    else
    {
        SDL_Rect lsrc = sRect;
        SDL_Rect ldst;
        for(ldst.y = dRect.top; ldst.y < dRect.bottom; ldst.y += lsrc.h)
        {
            if (dRect.bottom - ldst.y < lsrc.h)
                lsrc.h = dRect.bottom - ldst.y;

            lsrc.w = sRect.Width();

            for(ldst.x = dRect.left; ldst.x < dRect.right; ldst.x += lsrc.w)
            {
                if (dRect.right - ldst.x < lsrc.w)
                    lsrc.w = dRect.right - ldst.x;

                SDL_BlitSurface(src, &lsrc, dst, &ldst);
            }
        }
    }
}

void GFXEngine::Draw(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, Common::Point dPoint)
{
    SDL_Rect Ssrc = sRect;
    SDL_Rect Sdst = dPoint;
    SDL_BlitSurface(src, &Ssrc, dst, &Sdst);
}

void GFXEngine::Draw(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, Common::PointRect dRect)
{
    SDL_Rect Ssrc = sRect;
    if (Ssrc.w > dRect.w)
        Ssrc.w = dRect.w;
    if (Ssrc.h > dRect.h)
        Ssrc.h = dRect.h;
    SDL_Rect Sdst = dRect;
    SDL_BlitSurface(src, &Ssrc, dst, &Sdst);
}

void GFXEngine::RecreateScreenSurface()
{
    if ( ScreenSurface )
        SDL_DestroySurface(ScreenSurface);

    ScreenSurface = SDL_CreateSurface(_resolution.x, _resolution.y, _pixfmt->format);

    _backend->ScreenSurfaceChanged(ScreenSurface);
}

void GFXEngine::SetFBOBlending(int mode)
{
    _backend->SetColorEffectsBlending(mode);
}

void GFXEngine::DrawVideoFrame(const uint8_t *rgb, int width, int height)
{
    _backend->DrawVideoFrame(rgb, width, height);
}

void GFXEngine::EndVideo()
{
    _backend->EndVideo();
}

SDL_Color GFXEngine::Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SDL_Color tmp;
    tmp.a = a;
    tmp.r = r;
    tmp.g = g;
    tmp.b = b;
    return tmp;
}

bool GFXEngine::ColorCmp(const SDL_Color &a, const SDL_Color &b)
{
    if (a.a != b.a || a.r != b.r || a.g != b.g || a.b != b.b)
        return false;
    return true;
}

float GFXEngine::GetColorEffectPower(int id)
{
    int32_t pwr = 0;
    switch(id)
    {
        default:
        case 0:
            return 1.0;
        
        case 1:
            pwr = System::IniConf::GfxColorEffPower1.Get<int32_t>();
            break;
        
        case 2:
            pwr = System::IniConf::GfxColorEffPower2.Get<int32_t>();
            break;
        
        case 3:
            pwr = System::IniConf::GfxColorEffPower3.Get<int32_t>();
            break;
        
        case 4:
            pwr = System::IniConf::GfxColorEffPower4.Get<int32_t>();
            break;
            
        case 5:
            pwr = System::IniConf::GfxColorEffPower5.Get<int32_t>();
            break;
            
        case 6:
            pwr = System::IniConf::GfxColorEffPower6.Get<int32_t>();
            break;
            
        case 7:
            pwr = System::IniConf::GfxColorEffPower7.Get<int32_t>();
            break;
    }
    
    if (pwr < 0)
        pwr = 0;
    
    if (pwr > 100)
        pwr = 100;
    
    return (float)pwr / 100.0;
}

Common::Point GFXEngine::ConvertPosTo2DStuff(const Common::Point &pos)
{
    Common::Point real = System::GetResolution();

    if (real == _resolution)
        return pos;
    
    Common::Point t( pos.x * _resolution.x / real.x,
                     pos.y * _resolution.y / real.y );

    if (t.x < 0)
        t.x = 0;
    if (t.x >= _resolution.x)
        t.x = _resolution.x - 1;

    if (t.y < 0)
        t.y = 0;
    if (t.y >= _resolution.y)
        t.y = _resolution.y - 1;

    return t;
}

TMesh::TMesh(const TMesh &b)
{
    Mat = b.Mat;
    Vertexes = b.Vertexes;
    Indixes = b.Indixes;
    CoordsCache = b.CoordsCache;
    BoundBox = b.BoundBox;
    
    HwVertexBuf = 0;
    HwIndexBuf = 0;
}

TMesh &TMesh::operator=(const TMesh& b)
{
    Mat = b.Mat;
    Vertexes = b.Vertexes;
    Indixes = b.Indixes;
    CoordsCache = b.CoordsCache;
    BoundBox = b.BoundBox;
    
    HwVertexBuf = 0;
    HwIndexBuf = 0;
    
    return *this;
}

TMesh::~TMesh()
{
    GFX::Engine.MeshFreeVBO(this);
    
    // Just notify about still existing buffers
    if (HwVertexBuf || HwIndexBuf)
        printf("TMesh still has buffers!\n");
}

void GFXEngine::MeshMakeVBO(TMesh *mesh)
{
    _backend->MakeMeshBuffers(mesh);
}

void GFXEngine::MeshFreeVBO(TMesh *mesh)
{
    _backend->FreeMeshBuffers(mesh);
}

}
