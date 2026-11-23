#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>
#include "nucleas.h"
#include "rsrc.h"
#include "image.h"
#include "utils.h"
#include "includes.h"


NC_STACK_image::NC_STACK_image()
{
}

NC_STACK_image::~NC_STACK_image()
{
}

size_t NC_STACK_image::Init(IDVList &stak)
{
    if ( !NC_STACK_bitmap::Init(stak) )
        return 0;

    return 1;
}



// An SDL stream over a file of the game's file system, which closes the file
// when the stream is closed.
static Sint64 mysizefunc(void *userdata)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;

    size_t curr = fil->tell();
    fil->seek(0, SEEK_END);
    size_t sz = fil->tell();
    fil->seek(curr, SEEK_SET);
    return sz;
}

static Sint64 myseekfunc(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;
    switch (whence)
    {
    case SDL_IO_SEEK_SET:
        if ( fil->seek(offset, SEEK_SET) != 0 )
            return -1;
        break;
    case SDL_IO_SEEK_CUR:
        if ( fil->seek(offset, SEEK_CUR) != 0 )
            return -1;
        break;
    case SDL_IO_SEEK_END:
        if ( fil->seek(offset, SEEK_END) != 0 )
            return -1;
        break;
    default:
        SDL_SetError("Can't seek of this kind of whence");
        return -1;
    }
    return fil->tell();
}

static size_t myreadfunc(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;
    size_t done = fil->read(ptr, size);
    if (done < size)
        *status = SDL_IO_STATUS_EOF;
    return done;
}

static size_t mywritefunc(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;
    size_t done = fil->write(ptr, size);
    if (done < size)
        *status = SDL_IO_STATUS_ERROR;
    return done;
}

static bool myclosefunc(void *userdata)
{
    FSMgr::FileHandle *fil = (FSMgr::FileHandle *)userdata;
    delete fil;
    return true;
}

static SDL_IOStream * MyCustomRWop(FSMgr::FileHandle *fil)
{
    SDL_IOStreamInterface iface;
    SDL_INIT_INTERFACE(&iface);

    iface.size = mysizefunc;
    iface.seek = myseekfunc;
    iface.read = myreadfunc;
    iface.write = mywritefunc;
    iface.close = myclosefunc;
    return SDL_OpenIO(&iface, fil);
}




rsrc * NC_STACK_image::CreateResource(IDVList &stak)
{
    const std::string resName = stak.Get<std::string>(RSRC_ATT_NAME, "");
    int convertColor = stak.Get<int32_t>(BMD_ATT_CONVCOLOR, 0);

    if ( resName.empty() )
        return NULL;

    std::string tmpBuf = "rsrc:";
    tmpBuf += resName;

    FSMgr::FileHandle *fil = uaOpenFileAlloc(tmpBuf, "rb");
    if (!fil)
        return NULL;

    SDL_IOStream *rwops = MyCustomRWop(fil);
    if (!rwops)
    {
        delete fil;
        return NULL;
    }

    SDL_Surface *loaded = IMG_Load_IO(rwops, true);

    if (!loaded)
    {
        printf("%s\n", SDL_GetError());
        return NULL;
    }

    rsrc *res = NC_STACK_rsrc::CreateResource(stak); // bitmap_func64

    if ( !res )
    {
        SDL_DestroySurface(loaded);
        return NULL;
    }

    ResBitmap *bitm = new ResBitmap;   
    if ( !bitm )
    {
        SDL_DestroySurface(loaded);
        FreeResource(res);
        return NULL;
    }
    
    res->data = bitm;
    
    bitm->width = loaded->w;
    bitm->height = loaded->h;
    bitm->swTex = loaded;
    
    if (convertColor)
    {
        SDL_Surface *screenFmt = GFX::Engine.ConvertToScreenFormat(loaded);
        if (screenFmt)
        {
            SDL_DestroySurface(bitm->swTex);
            bitm->swTex = screenFmt;
        }
    } 
    else if (SDL_Palette *pal = SDL_GetSurfacePalette(loaded))
    {
        if ( !bitm->palette )
            bitm->palette = new UA_PALETTE;

        if ( bitm->palette )
        {
            for (int i = 0; i < pal->ncolors; i++)
                (*bitm->palette)[i] = pal->colors[i];
        }
    }

    return res;
}

bool NC_STACK_image::SavePng(ResBitmap *bitm, const std::string &fname)
{
    if (!bitm)
        return false;
    
    if (!bitm->swTex)
        return false;
    
    FSMgr::FileHandle *fil = uaOpenFileAlloc(fname, "wb");
    if (!fil || !fil->OK())
        return false;

    SDL_IOStream *rwops = MyCustomRWop(fil);
    if (!rwops)
    {
        delete fil;
        return false;
    }

    return IMG_SavePNG_IO(bitm->swTex, rwops, true);
}