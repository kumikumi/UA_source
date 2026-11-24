#ifndef ENGINE_NULL_BACKEND_H_INCLUDED
#define ENGINE_NULL_BACKEND_H_INCLUDED

#include "gfx.h"
#include "render_backend.h"

namespace GFX
{

// Draws nothing, for running without a display or a GPU (--headless).
// Textures and meshes stay in memory only, and frames go nowhere.
class NullBackend : public RenderBackend
{
public:
    virtual void PrepareWindow(SDL_PropertiesID) override {}
    virtual void Init(SDL_Window *) override {}
    virtual void Deinit() override {}

    virtual std::string GetInfo() override { return "no renderer (headless)"; }

    virtual void SetVsync(bool) override {}
    virtual std::string GetVsyncMode() override { return "off"; }

    virtual void ResetStates() override {}
    virtual void ScreenSurfaceChanged(SDL_Surface *) override {}
    virtual void WindowResized() override {}

    virtual void BeginFrame() override {}
    virtual uint64_t EndFrame(SDL_Surface *) override { return 0; }
    virtual void FlushScreenSurface(SDL_Surface *) override {}

    virtual void BeginScene(const mat4x4f &) override {}
    virtual void Rasterize(TRenderQueue &queue, uint32_t passes) override;
    virtual void EndScene() override {}

    virtual bool AllocTexture(ResBitmap *) override { return true; }
    virtual void FreeTexture(ResBitmap *) override {}

    virtual void MakeMeshBuffers(TMesh *) override {}
    virtual void FreeMeshBuffers(TMesh *) override {}

    virtual void SetColorEffectsBlending(int) override {}

    virtual uint8_t *ReadScreen(int *, int *) override { return NULL; }

    virtual void DrawVideoFrame(const uint8_t *, int, int) override {}
    virtual void EndVideo() override {}
};

}

#endif // ENGINE_NULL_BACKEND_H_INCLUDED
