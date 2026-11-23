#ifndef UA_SYSTEM_H_INCLUDED
#define UA_SYSTEM_H_INCLUDED

#include <string>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "common/common.h"

namespace System 
{
    struct ResRatio
    {
        int32_t W = 0;
        int32_t H = 0;
        float   R = 1.0;

        ResRatio();
        ResRatio(const Common::Point &p);
        ResRatio(int32_t w, int32_t h);
        bool operator==(const ResRatio &b) const;
        bool operator!=(const ResRatio &b) const;
        void Set(int32_t w, int32_t h);
        
        operator Common::Point() const;
    };
    
    void Init();
    void Deinit();

    // Creates the game's window, with what the graphics API needs already
    // in props.
    void OpenWindow(SDL_PropertiesID props);
    SDL_Window *GetWindow();
    
    // Events update cycle
    bool ProcessEvents();
    
    // Fullscreen is borderless over the desktop: the display mode is never changed.
    void SetFullscreen(bool fullscreen);
    bool IsFullscreen();

    // Refresh rate of the display the window is on, 0 if unknown.
    int GetRefreshRate();
    
    
    void SetReleativeMouse(bool mode);

    // Typed characters only arrive while text input is on. Off, letters are
    // just keys: macOS, for one, opens its accent menu when one is held down.
    void SetTextInput(bool on);
    
    TTF_Font *LoadFont(const std::string &fontname, int height);
    
    void PostQuitMessage();
    
    void EventsAddHandler(SDL_EventFilter func, bool first = true);
    void EventsDeleteHandler(SDL_EventFilter func);
    
    // Return drawable area sizes
    ResRatio GetRResolution();
    Common::Point GetResolution();
    
    std::vector<std::string>& GetCmdLineArray();
    void AddCmdLine(const std::string &);
    int32_t FindCmdLineArg(const std::string &);

}
#endif
