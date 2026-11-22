#ifndef UA_SYSTEM_H_INCLUDED
#define UA_SYSTEM_H_INCLUDED

#include <string>

#if defined(__APPLE__) && defined(__MACH__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_ttf.h>

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
    
    void Init(bool oldGL = false);
    void Deinit();
    
    // Events update cycle
    bool ProcessEvents();
    
    // Draw content of backbuffer
    void Flip();
    
    // Fullscreen is borderless over the desktop: the display mode is never changed.
    void SetFullscreen(bool fullscreen);
    bool IsFullscreen();

    // Refresh rate of the display the window is on, 0 if unknown.
    int GetRefreshRate();
    
    
    void SetReleativeMouse(bool mode);
    
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
