#ifndef ENGINE_GL_BACKEND_H_INCLUDED
#define ENGINE_GL_BACKEND_H_INCLUDED

#if defined(__APPLE__) && defined(__MACH__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <array>
#include <SDL3/SDL_opengl.h>

#include "gfx.h"
#include "render_backend.h"

namespace GFX
{

#ifndef GFXSHORTINDEX
#define GLINDEXTYPE GL_UNSIGNED_INT
#else
#define GLINDEXTYPE GL_UNSIGNED_SHORT
#endif

#ifndef __EMSCRIPTEN__
#define FBOTEXTYPE GL_RGBA32F
#else
#define FBOTEXTYPE GL_RGBA
#endif

struct TShaderProg
{
    uint32_t ID = 0;
    int32_t PosLoc = -1;
    int32_t ColorLoc = -1;
    int32_t UVLoc = -1;

    TShaderProg() = default;
    TShaderProg(uint32_t id);
};

struct TColorEffectsProg : TShaderProg
{
    int32_t NormLoc = -1;
    int32_t InvLoc = -1;
    int32_t RandLoc = -1;
    int32_t ScrSizeLoc = -1;
    int32_t MillisecsLoc = -1;

    TColorEffectsProg() = default;
    TColorEffectsProg(uint32_t id);
};


struct GfxStates
{
    uint32_t Tex = 0;
    bool Shaded = true;
    bool Stipple = false;
    uint32_t SrcBlend = GL_ONE;
    uint32_t DstBlend = GL_ZERO;
    int8_t TexBlend = 2;
    bool AlphaBlend = false;
    bool Zwrite = true;
    bool LinearFilter = true;
    bool AlphaTest = false;
    bool Fog = false;
    float FogStart = 3496.0;
    float FogLength = 600.0;
    bool AFog = false;
    float AFogStart = 3496.0;
    float AFogLength = 600.0;

    bool DepthTest = true;

    uint32_t DataBuf = 0;
    uint32_t IndexBuf = 0;
    TShaderProg Prog;
};

// OpenGL 2.1 with the fixed function pipeline ("legacy"), or OpenGL 3.1 with
// shaders and buffer objects when gfx.vbo is on and the context allows it.
class GLBackend : public RenderBackend
{
public:
    void PrepareWindow(SDL_PropertiesID props) override;
    void Init(SDL_Window *window) override;
    void Deinit() override;

    std::string GetInfo() override;
    void SetVsync(bool enabled) override;
    std::string GetVsyncMode() override;

    void ResetStates() override;
    void ScreenSurfaceChanged(SDL_Surface *screen) override;
    void WindowResized() override;

    void BeginFrame() override;
    uint64_t EndFrame(SDL_Surface *screen) override;
    void FlushScreenSurface(SDL_Surface *screen) override;

    void BeginScene(const mat4x4f &projection) override;
    void Rasterize(TRenderQueue &queue, uint32_t passes) override;
    void EndScene() override;

    bool AllocTexture(ResBitmap *bitm) override;
    void FreeTexture(ResBitmap *bitm) override;

    void MakeMeshBuffers(TMesh *mesh) override;
    void FreeMeshBuffers(TMesh *mesh) override;

    void SetColorEffectsBlending(int mode) override;

    uint8_t *ReadScreen(int *ow, int *oh) override;

    void DrawVideoFrame(const uint8_t *rgb, int width, int height) override;
    void EndVideo() override;

protected:
    uint32_t LoadShader(int32_t type, const std::string &fl);
    uint32_t CompileShader(int32_t type, const std::string &string);

    void DrawFBO();
    void UpdateFBOSizes();
    void DrawScreenSurface(SDL_Surface *screen);
    void DrawVtxQuad(const std::array<GFX::TVertex, 4> &vtx);

    void RenderingMeshOld(TRenderNode *mesh);
    void RenderingMesh(TRenderNode *mesh);
    void RenderNode(TRenderNode *node);

    void SetRenderStates(int arg);
    void BindVBOParameters(TShaderProg &shader);
    void CommitUBOParameters();

    void SetProjectionMatrix(const mat4x4f &mat);
    void SetModelViewMatrix(const mat4x4f &mat);

protected:
    static const std::string _stdPShaderText;
    static const std::string _stdVShaderText;

    SDL_Window *_window = NULL;
    SDL_GLContext _context = NULL;

    // The texture format of the screen surface's pixel format
    GLint _glPixfmt = GL_RGBA;
    GLint _glPixtype = GL_UNSIGNED_BYTE;

    bool _glext = false;
    bool _vbo = false;

    int _sceneBeginned = 0;

    GLuint screenTex = 0;
    GLuint _videoTex = 0;

    uint32_t _stdPsShader = 0;
    uint32_t _stdVsShader = 0;
    TShaderProg _stdShaderProg;

    uint32_t _vboParams = 0;

    static constexpr int32_t _vboMProjOff = 0;   // 4 * 4 * 4 = 64
    static constexpr int32_t _vboMViewOff = 64;  // 4 * 4 * 4 = 64
    static constexpr int32_t _vboFogOff   = 128; // 3 * 4 = 12  ... + 4(because next vec3)
    static constexpr int32_t _vboAFogOff  = 144; // 3 * 4 = 12
    static constexpr int32_t _vboTextured = 156; // 4
    static constexpr int32_t _vboFlat     = 160; // 4
    static constexpr int32_t _vboATest    = 164; // 4
    static constexpr int32_t _vboParamsSize = 168;
    static constexpr int32_t _vboParamsBlockBinding = 0;

    struct
    {
        mat4x4f Proj;
        mat4x4f View;
        float   Fog = 0.0;
        float   FogStart = 0.0;
        float   FogLength = 0.0;
        float   _pad1 = 0.0;
        float   AFog = 0.0;
        float   AFogStart = 0.0;
        float   AFogLength = 0.0;
        int32_t Textured = 0;
        int32_t Flat = 0;
        int32_t ATest = 0;
    } _vboStatesBlock;

    bool _vboStatesChanged = true;

    uint32_t _globalVao = 0;

    GfxStates _states;
    GfxStates _lastStates;

    uint32_t _stdQuadDataBuf = 0;
    uint32_t _stdQuadIndexBuf = 0;

    int32_t _colorEffects = 0;

    uint32_t _fbo = 0;
    uint32_t _fbod = 0;
    uint32_t _fboTex = 0;
    int32_t _fboBlend = 0;

    uint32_t _psShader = 0;
    uint32_t _vsShader = 0;
    TColorEffectsProg _colorEffectsShaderProg;
};

}

#endif // ENGINE_GL_BACKEND_H_INCLUDED
