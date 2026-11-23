#ifndef ENGINE_RENDER_BACKEND_H_INCLUDED
#define ENGINE_RENDER_BACKEND_H_INCLUDED

#include <string>
#include <SDL3/SDL.h>

#include "../matrix.h"

struct ResBitmap;

namespace GFX
{

class TMesh;
struct TRenderQueue;

// What GFXEngine needs from a graphics API: drawing the queued 3D scene,
// putting the software-drawn 2D screen surface over it and presenting the
// frame. GFXEngine keeps everything that does not depend on the API.
class RenderBackend
{
public:
    virtual ~RenderBackend() {};

    // Called before the window is created, to add what the API needs to
    // the window's creation properties.
    virtual void PrepareWindow(SDL_PropertiesID props) = 0;

    // Starts drawing into the window. Deinit stops it, before the window
    // is destroyed.
    virtual void Init(SDL_Window *window) = 0;
    virtual void Deinit() = 0;

    // The API and its version, such as "OpenGL 2.1 legacy renderer".
    virtual std::string GetInfo() = 0;

    virtual void SetVsync(bool enabled) = 0;
    virtual std::string GetVsyncMode() = 0; // "on", "off" or "adaptive"

    // Puts the render states to their defaults, after the settings or the
    // resolution changed.
    virtual void ResetStates() = 0;

    // The screen surface was created anew, with the given size.
    virtual void ScreenSurfaceChanged(SDL_Surface *screen) = 0;

    // The window's size changed.
    virtual void WindowResized() = 0;

    virtual void BeginFrame() = 0;

    // Puts the screen surface over the frame and presents it.
    virtual void EndFrame(SDL_Surface *screen) = 0;

    // Puts the screen surface over what is drawn so far, so that the 3D
    // drawn next goes over it.
    virtual void FlushScreenSurface(SDL_Surface *screen) = 0;

    virtual void BeginScene(const mat4x4f &projection) = 0;

    // Draws the queued nodes of the given RASTER passes and empties their
    // lists.
    virtual void Rasterize(TRenderQueue &queue, uint32_t passes) = 0;

    virtual void EndScene() = 0;

    virtual bool AllocTexture(ResBitmap *bitm) = 0;
    virtual void FreeTexture(ResBitmap *bitm) = 0;

    virtual void MakeMeshBuffers(TMesh *mesh) = 0;
    virtual void FreeMeshBuffers(TMesh *mesh) = 0;

    // How the color effects pass blends the scene over the screen.
    virtual void SetColorEffectsBlending(int mode) = 0;

    // The presented frame as RGB, bottom line first, in memory from
    // malloc. NULL if it cannot be read.
    virtual uint8_t *ReadScreen(int *ow, int *oh) = 0;

    // Shows one frame of a movie, letterboxed, and presents it. The frame
    // is RGB without row padding.
    virtual void DrawVideoFrame(const uint8_t *rgb, int width, int height) = 0;

    // The movie is over: frees what DrawVideoFrame kept.
    virtual void EndVideo() = 0;
};

}

#endif // ENGINE_RENDER_BACKEND_H_INCLUDED
