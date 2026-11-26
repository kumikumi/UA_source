#include <string>
#include <list>
#include "../fmtlib/printf.h"

#include "system.h"
#include "fsmgr.h"
#include "../utils.h"


namespace System
{

    
struct FontNode
{
    std::string Path;
    std::string Name;
    std::string Style;
    
    FontNode() {};
    FontNode(const std::string &p, const std::string &n, const std::string &s)
    : Path(p)
    , Name(n)
    , Style(s)
    {};
    
    FontNode(const FontNode &&fnt)
    {
        Path = std::move(fnt.Path);
        Name = std::move(fnt.Name);
        Style = std::move(fnt.Style);
    };
};

struct TFontLookup
{
    const std::string Name; // Font name or part
    const std::string Alt1;
    const std::string Alt2;
};


ResRatio::ResRatio()
{}

ResRatio::ResRatio(const Common::Point &p)
: W(p.x), H(p.y)
{
    R = (float)W / (float)H;
}

ResRatio::ResRatio(int32_t w, int32_t h)
: W(w), H(h)
{
    R = (float)W / (float)H;
}

bool ResRatio::operator==(const ResRatio &b) const
{
    if (W != b.W || H != b.H)
        return false;
    return true;
}

bool ResRatio::operator!=(const ResRatio &b) const
{
    if (W == b.W || H == b.H)
        return false;
    return true;
}

ResRatio::operator Common::Point() const
{
    return Common::Point(W, H);
}

void ResRatio::Set(int32_t w, int32_t h)
{
    W = w;
    H = h;
    R = (float)W / (float)H;
}


static std::list< SDL_EventFilter > EventHandlers;
static std::list< FontNode > FontsList;

static TFontLookup FontsLookup[]
{
    {"arial", "liberation sans", ""},
    {"dungeon", "xolonium bold", "xolonium regular"},
    {"ms sans serif", "microsoft sans serif", "liberation serif"},
    {"courier", "liberation mono", ""},
    {"small font", "press start 2p", ""},
    {"ＭＳ Ｐゴシック", "MS PGothic", "Textar"},
    {"MS PGothic", "Textar", ""}
};
    
    
static SDL_Window *window = NULL;
static ResRatio winRes(640, 480);
static bool curRelMouse = false;

void ScanFonts()
{
    FontsList.clear();

    FSMgr::DirIter dir = FSMgr::iDir::readDir("fonts/");
    if (dir)
    {
        FSMgr::iNode *nod = NULL;
        while(dir.getNext(&nod))
        {
            if (nod && nod->getType() == FSMgr::iNode::NTYPE_FILE)
            {
                std::string tmp = nod->getName();
                for (size_t i = 0; i < tmp.length(); i++)
                    tmp[i] = std::tolower(tmp[i]);

                if( tmp.find(".ttf") != std::string::npos || tmp.find(".otf") != std::string::npos || tmp.find(".fon") != std::string::npos )
                {
                    TTF_Font *fnt = TTF_OpenFont(nod->getPath().c_str(), 12);
                    if (fnt)
                    {
                        FontsList.push_back( FontNode(nod->getPath(), TTF_GetFontFamilyName(fnt), TTF_GetFontStyleName(fnt)) );

                        TTF_CloseFont(fnt);
                    }
                }
            }
        }
    }
}

std::string FindFont(const std::string &fontName)
{
    for(const FontNode &fnt : FontsList )
    {
        if (StriCmp(fontName, fnt.Name) == 0)
            return fnt.Path;
        else
        {
            std::string ttmp = fnt.Name;
            ttmp += " ";
            ttmp += fnt.Style;

            if (StriCmp(fontName, ttmp) == 0)
                return fnt.Path;
        }
    }

    return std::string();
}


TTF_Font *LoadFont(const std::string &fontname, int height)
{
    std::string tmp2 = fontname;
    for (size_t i = 0; i < tmp2.length(); i++)
        tmp2[i] = std::tolower(tmp2[i]);

    TTF_Font *fnt = NULL;

    std::string filename = FindFont(tmp2);

    if (filename.empty())
    {
        for( const TFontLookup &lookup : FontsLookup )
        {
            if ( !StriCmp(tmp2, lookup.Name) )
            {
                filename = FindFont(lookup.Alt1);

                if (!filename.empty())
                    break;

                filename = FindFont(lookup.Alt2);

                if (!filename.empty())
                    break;
            }
        }
    }

    if (filename.empty())
    {
        for( const TFontLookup &lookup : FontsLookup )
        {
            if ( tmp2.find(lookup.Name) != std::string::npos )
            {
                filename = FindFont(lookup.Alt1);

                if (!filename.empty())
                    break;

                filename = FindFont(lookup.Alt2);

                if (!filename.empty())
                    break;
            }
        }
    }

    if (!filename.empty())
        fnt = TTF_OpenFont(filename.c_str(), height);

    if (!fnt)
        fmt::printf("Can't load font %s %d\n", fontname, height);

    return fnt;
}


void Init(bool headless)
{
    EventHandlers.clear();

    SDL_InitFlags flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC;
    if (headless)
        flags = 0;

    if (!SDL_Init(flags))
    {
        printf("Couldn't initialize SDL: %s", SDL_GetError());
        return;
    }

    TTF_Init();

    ScanFonts();
}

void OpenWindow(SDL_PropertiesID props)
{
    // The game renders at whatever size the window has, so start with most of
    // the desktop rather than a fixed resolution.
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
        winRes = ResRatio(usable.w * 3 / 4, usable.h * 3 / 4);

    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "OpenUA (Urban Assault)");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, winRes.W);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, winRes.H);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    window = SDL_CreateWindowWithProperties(props);

    if (!window)
    {
        printf("Couldn't create window and renderer: %s", SDL_GetError());
        return;
    }

    int drawW, drawH;
    SDL_GetWindowSizeInPixels(window, &drawW, &drawH);
    winRes = ResRatio(drawW, drawH);
}

SDL_Window *GetWindow()
{
    return window;
}

void Deinit()
{
    FontsList.clear();

    if (window)
        SDL_DestroyWindow(window);

    SDL_Quit();
}


bool ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            int w, h;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            winRes = ResRatio(w, h);
        }
        break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            return true;
        
        case SDL_EVENT_QUIT:
            return true;

        default:
            break;
        }
        
        for(SDL_EventFilter &e : EventHandlers)
        {
            if ( !(e)(NULL, &event) )
                break;
        }
    }

    return false;
}

ResRatio GetRResolution()
{
    return winRes;
}

Common::Point GetResolution()
{
    return Common::Point(winRes.W, winRes.H);
}

void SetFullscreen(bool fullscreen)
{
    if (!window || fullscreen == IsFullscreen())
        return;

    // Without a fullscreen display mode set, this is borderless fullscreen.
    SDL_SetWindowFullscreen(window, fullscreen);

    // SDL3 changes the window in the background, as when macOS animates it
    // into fullscreen. Wait for the new size, as SDL2 did, so that the game
    // lays out its screen once. Should the wait time out, the size event
    // updates winRes later.
    SDL_SyncWindow(window);

    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    winRes = ResRatio(w, h);
}

bool IsFullscreen()
{
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

int GetRefreshRate()
{
    if (!window)
        return 0;

    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!display)
        return 0;

    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);
    if (!mode)
        return 0;

    return (int)(mode->refresh_rate + 0.5f); // 59.94 Hz counts as 60, as in SDL2
}

void SetReleativeMouse(bool mode)
{
    if (mode != curRelMouse)
    {
        if (mode)
            SDL_SetWindowRelativeMouseMode(window, true);
        else
        {
            SDL_SetWindowRelativeMouseMode(window, false);

            int WW = 0, WH = 0;
            SDL_GetWindowSize(window, &WW, &WH);
            SDL_WarpMouseInWindow(window, WW / 2, WH / 2);
        }

        curRelMouse = mode;
    }
}

void SetTextInput(bool on)
{
    if (!window || on == SDL_TextInputActive(window))
        return;

    if (on)
        SDL_StartTextInput(window);
    else
        SDL_StopTextInput(window);
}

void EventsAddHandler(SDL_EventFilter func, bool first)
{
    if (first)
        EventHandlers.push_front(func);
    else
        EventHandlers.push_back(func);
}

void EventsDeleteHandler(SDL_EventFilter func)
{
    EventHandlers.remove(func);
}

void PostQuitMessage()
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}



}
