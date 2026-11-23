#include "../fmtlib/printf.h"
#include <SDL3/SDL_opengl.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <OpenGL/glext.h>
#else
#include <GL/glext.h>
#endif

#include "gl_backend.h"
#include "../log.h"
#include "inivals.h"
#include "perf.h"
#include "glfuncs.h"

namespace GFX
{

void GLBackend::PrepareWindow(SDL_PropertiesID props)
{
    // The legacy renderer needs GL 2.1, the shader renderer GL 3.1
    if ( !System::IniConf::GfxVBO.Get<bool>() )
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    }
    else
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
}

void GLBackend::Init(SDL_Window *window)
{
    _window = window;

    _context = SDL_GL_CreateContext(window);

    if (!_context && System::IniConf::GfxVBO.Get<bool>())
    {
        // macOS has no 3.x compatibility profile, so the request fails there.
        // GL 2.1 is enough for the legacy renderer, which is then selected.
        printf("Couldn't create OpenGL 3.1 context (%s), trying 2.1\n", SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        _context = SDL_GL_CreateContext(window);
    }

    if (!_context)
        printf("Couldn't create OpenGL context: %s\n", SDL_GetError());

    if (GFXEngine::GetPixelFormatU32() == SDL_PIXELFORMAT_ARGB8888)
        _glPixfmt = GL_BGRA;
    else
        _glPixfmt = GL_RGBA;

    _glPixtype = GL_UNSIGNED_BYTE;

    _glext = Glext::init();

    printf("OpenGL: %s, %s, %s\n", (const char *)glGetString(GL_VERSION),
           (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VENDOR));

    _vbo = System::IniConf::GfxVBO.Get<bool>();
    _colorEffects = System::IniConf::GfxColorEffects.Get<int32_t>();
    
    if (!_glext)
    {
        _colorEffects = 0;
        _vbo = false;
    }

    // The VBO renderer's GLSL 1.40 shaders and uniform buffers need GL 3.1.
    if (_vbo && !Glext::HasVersion(3, 1))
    {
        printf("OpenGL %d.%d context, gfx.vbo needs 3.1: using the legacy renderer\n",
               Glext::VersionMajor, Glext::VersionMinor);
        _vbo = false;
    }

    if (_vbo)
    {
        Glext::GLGenVertexArrays(1, &_globalVao);
        Glext::GLBindVertexArray(_globalVao);
        
        Glext::GLGenBuffers(1, &_vboParams);
        Glext::GLBindBuffer(GL_UNIFORM_BUFFER, _vboParams);
        Glext::GLBufferData(GL_UNIFORM_BUFFER, _vboParamsSize, NULL, GL_STREAM_DRAW); 
        
        Glext::GLBindBufferBase(GL_UNIFORM_BUFFER, _vboParamsBlockBinding, _vboParams);
    }
    else
    {
        // The legacy renderer draws from client-side arrays, and nothing
        // disables these, so enable them once rather than every frame:
        // movies are drawn before the first BeginFrame.
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
    }

    if (_vbo)
    {
        _stdPsShader = CompileShader(GL_FRAGMENT_SHADER, _stdPShaderText);
        _stdVsShader = CompileShader(GL_VERTEX_SHADER,   _stdVShaderText);
        uint32_t progID = Glext::GLCreateProgram();
        
        Glext::GLAttachShader(progID, _stdPsShader);
        Glext::GLAttachShader(progID, _stdVsShader);
        Glext::GLLinkProgram(progID);
        
        _stdShaderProg = TShaderProg( progID );
        
        BindVBOParameters(_stdShaderProg);
    }

    if (_colorEffects > 0)
    {
        Glext::GLGenFramebuffers(1, &_fbo);
        Glext::GLBindFramebuffer(GL_FRAMEBUFFER, _fbo);

        glGenTextures(1, &_fboTex);
        glBindTexture(GL_TEXTURE_2D, _fboTex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, FBOTEXTYPE, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);


        Glext::GLGenRenderbuffers(1, &_fbod);
        Glext::GLBindRenderbuffer(GL_RENDERBUFFER, _fbod);
        Glext::GLRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 640, 480);

        
        Glext::GLFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _fbod);

        Glext::GLFrameBufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _fboTex, 0);

        Glext::GLBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        Glext::GLBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (_vbo)
        {
            _psShader = LoadShader(GL_FRAGMENT_SHADER, "res/clreff_vbo.ps");
            _vsShader = LoadShader(GL_VERTEX_SHADER, "res/clreff_vbo.vs");
        }
        else
        {
            _psShader = LoadShader(GL_FRAGMENT_SHADER, "res/clreff.ps");
            _vsShader = LoadShader(GL_VERTEX_SHADER, "res/clreff.vs");
        }

        uint32_t progID = Glext::GLCreateProgram();
        
        Glext::GLAttachShader(progID, _psShader);
        Glext::GLAttachShader(progID, _vsShader);
        Glext::GLLinkProgram(progID);
        
        _colorEffectsShaderProg = TColorEffectsProg(progID);
        
        if (_vbo)
            BindVBOParameters(_colorEffectsShaderProg);
    }
}

void GLBackend::Deinit()
{
    if (!_context)
        return;

    if (_stdQuadDataBuf)
        Glext::GLDeleteBuffers(1, &_stdQuadDataBuf);
    
    _stdQuadDataBuf = 0;
    
    if (_stdQuadIndexBuf)
        Glext::GLDeleteBuffers(1, &_stdQuadIndexBuf);
    
    _stdQuadIndexBuf = 0;
    
    if (_stdShaderProg.ID)
        Glext::GLDeleteProgram(_stdShaderProg.ID);
    
    if (_stdVsShader)
        Glext::GLDeleteShader(_stdVsShader);
    
    if (_stdPsShader)
        Glext::GLDeleteShader(_stdPsShader);
    
    _stdShaderProg.ID = 0;
    _stdVsShader = 0;
    _stdPsShader = 0;   
    
    if (_colorEffectsShaderProg.ID)
        Glext::GLDeleteProgram(_colorEffectsShaderProg.ID); 
    
    if (_psShader)
        Glext::GLDeleteShader(_psShader);
    
    if (_vsShader)
        Glext::GLDeleteShader(_vsShader);
    
    _colorEffectsShaderProg.ID = 0;
    _vsShader = 0;
    _psShader = 0;

    SDL_GL_DestroyContext(_context);
    _context = NULL;
}

std::string GLBackend::GetInfo()
{
    return fmt::sprintf("OpenGL %d.%d %s renderer", Glext::VersionMajor, Glext::VersionMinor,
                        _vbo ? "shader" : "legacy");
}

void GLBackend::SetVsync(bool enabled)
{
    if ( !enabled )
        SDL_GL_SetSwapInterval(0);
    else if ( System::IniConf::GfxVsync.Get<int>() != 2 || !SDL_GL_SetSwapInterval(-1) )
        SDL_GL_SetSwapInterval(1); // Adaptive (-1) is not supported everywhere
}

std::string GLBackend::GetVsyncMode()
{
    std::string vsync;
    int interval = 0;
    SDL_GL_GetSwapInterval(&interval);

    switch ( interval )
    {
        case 0:
            vsync = "off";
            break;

        case -1:
            vsync = "adaptive";
            break;

        default:
            vsync = "on";
            break;
    }

    return vsync;
}

void GLBackend::ResetStates()
{
    _states = GfxStates();
    _states.LinearFilter = (Engine._filter != 0);

    SetRenderStates(1);

    glEnable(GL_CULL_FACE);

    if (Engine._dither)
        glEnable(GL_DITHER);
    else
        glDisable(GL_DITHER);
}

void GLBackend::ScreenSurfaceChanged(SDL_Surface *screen)
{
    if ( !screenTex )
        glGenTextures(1, &screenTex);

    _states.Tex = screenTex;
    SetRenderStates(0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen->w, screen->h, 0, _glPixfmt, _glPixtype, NULL);
}

void GLBackend::WindowResized()
{
    UpdateFBOSizes();
}

void GLBackend::BeginFrame()
{
    Common::Point scrSz = System::GetResolution();
    glViewport(0, 0, scrSz.x, scrSz.y);
    
    bool saved = _states.Zwrite;
    
    _states.Zwrite = true;
    SetRenderStates(0);
    
    if (_colorEffects)
    {
        Glext::GLBindFramebuffer(GL_FRAMEBUFFER, 0);
    
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
        Glext::GLBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    }

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    _states.Zwrite = saved;
    SetRenderStates(0);
    
    _states.Prog = _stdShaderProg;
}

uint64_t GLBackend::EndFrame(SDL_Surface *screen)
{
    if (_colorEffects > 1)
    {
        Glext::GLBindFramebuffer(GL_FRAMEBUFFER, 0);   
        DrawFBO();
    }    

    DrawScreenSurface(screen);
    
    if (_colorEffects == 1)
    {
        Glext::GLBindFramebuffer(GL_FRAMEBUFFER, 0);   
        DrawFBO();
    }
    
    // The wait for vsync is somewhere in the swap
    SDL_GL_SwapWindow(_window);
    return 0;
}

void GLBackend::FlushScreenSurface(SDL_Surface *screen)
{
    if (_colorEffects)
        Glext::GLBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    DrawScreenSurface(screen);

    if (_colorEffects)
        Glext::GLBindFramebuffer(GL_FRAMEBUFFER, _fbo);
}

void GLBackend::BeginScene(const mat4x4f &projection)
{
    SetRenderStates(2);
    
    SetProjectionMatrix( projection );
    SetModelViewMatrix( mat4x4f() );
    
    _sceneBeginned = 1;
}

void GLBackend::EndScene()
{
    _sceneBeginned = 0;
}

void GLBackend::Rasterize(TRenderQueue &queue, uint32_t passes)
{
    if (passes & RASTER_SKY)
    {
        queue.SkyBox.sort(TRenderNode::CompareSolid);
        
        while(!queue.SkyBox.empty())
        {
            RenderNode( queue.SkyBox.front() );
            queue.SkyBox.pop_front();
        }
    }
    
    if (passes & RASTER_SOLID)
    {
        queue.Solid.sort(TRenderNode::CompareSolid);
        
        while(!queue.Solid.empty())
        {
            RenderNode( queue.Solid.front() );
            queue.Solid.pop_front();
        }
    }
    
    if (passes & RASTER_ZEROTR)
    {
        queue.ZeroTracy.sort(TRenderNode::CompareSolid);
        
        while(!queue.ZeroTracy.empty())
        {
            RenderNode( queue.ZeroTracy.front() );
            queue.ZeroTracy.pop_front();
        }
    }
    
    if (passes & RASTER_LUMATR)
    {
        queue.LumaTracy.sort(TRenderNode::CompareTransparent);
        
        while(!queue.LumaTracy.empty())
        {
            RenderNode( queue.LumaTracy.front() );
            queue.LumaTracy.pop_front();
        }
    }
}

bool GLBackend::AllocTexture(ResBitmap *bitm)
{
    if (bitm->swTex && !bitm->hwTex)
    {
        glGenTextures(1, &bitm->hwTex);

        if (!bitm->hwTex)
        {
            return false;
        }

        _states.Tex = bitm->hwTex;
        SetRenderStates(0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (bitm->swTex->format == GFXEngine::GetPixelFormatU32())
        {
            SDL_LockSurface(bitm->swTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitm->width, bitm->height, 0, _glPixfmt, _glPixtype, bitm->swTex->pixels);
            SDL_UnlockSurface(bitm->swTex);
        }
        else
        {
            SDL_Surface *conv = GFXEngine::ConvertSDLSurface(bitm->swTex, (SDL_PixelFormat)GFXEngine::GetPixelFormatU32());
            
            SDL_LockSurface(conv);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitm->width, bitm->height, 0, _glPixfmt, _glPixtype, conv->pixels);
            SDL_UnlockSurface(conv);
            
            SDL_DestroySurface(conv);
        }
    }

    return true;
}

void GLBackend::FreeTexture(ResBitmap *bitm)
{
    if ( bitm->hwTex )
        glDeleteTextures(1, &bitm->hwTex);

    bitm->hwTex = 0;
}

void GLBackend::MakeMeshBuffers(TMesh *mesh)
{
    if (_vbo)
    {        
        if (!mesh->HwVertexBuf)
            Glext::GLGenBuffers(1, &mesh->HwVertexBuf);
        
        if (!mesh->HwIndexBuf)
            Glext::GLGenBuffers(1, &mesh->HwIndexBuf);
                
        Glext::GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->HwIndexBuf);
        Glext::GLBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->Indixes.size() * sizeof(IndexType), mesh->Indixes.data(), GL_STATIC_DRAW);
        
        Glext::GLBindBuffer(GL_ARRAY_BUFFER, mesh->HwVertexBuf);
        int32_t vtxDataSz = mesh->Vertexes.size() * sizeof(TVertex);
        int32_t coordDataSz = mesh->Vertexes.size() * sizeof(tUtV);
        Glext::GLBufferData(GL_ARRAY_BUFFER, vtxDataSz + mesh->CoordsCache.size() * coordDataSz, NULL, GL_STATIC_DRAW);
        
        int32_t off = 0;
        Glext::GLBufferSubData(GL_ARRAY_BUFFER, off, vtxDataSz, mesh->Vertexes.data());
        
        off += vtxDataSz;
        for (TCoordsCache &cch : mesh->CoordsCache)
        {
            Glext::GLBufferSubData(GL_ARRAY_BUFFER, off, coordDataSz, cch.Coords.data());
            cch.BufferPos = off;
            
            off += coordDataSz;
        }
        
        Glext::GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _lastStates.IndexBuf);
        Glext::GLBindBuffer(GL_ARRAY_BUFFER, _lastStates.DataBuf);  
    }
}

void GLBackend::FreeMeshBuffers(TMesh *mesh)
{
    if (_vbo)
    {
        if (mesh->HwVertexBuf)
        {
            Glext::GLDeleteBuffers(1, &mesh->HwVertexBuf);
            mesh->HwVertexBuf = 0;
        }
        
        if (mesh->HwIndexBuf)
        {
            Glext::GLDeleteBuffers(1, &mesh->HwIndexBuf);
            mesh->HwIndexBuf = 0;
        }
        
//        if (mesh->glVao)
//        {
//            Glext::GLDeleteVertexArrays(1, &mesh->glVao);
//            mesh->glVao = 0;
//        }
    }
}

void GLBackend::SetColorEffectsBlending(int mode)
{
    _fboBlend = mode;
}

uint8_t *GLBackend::ReadScreen(int *ow, int *oh)
{
    Common::Point res = System::GetResolution();

    // power of 2
    res.x &= ~1;
    res.y &= ~1;

    uint8_t *buf = (uint8_t *)malloc(res.x * res.y * 3);

    glPixelStorei(GL_PACK_ALIGNMENT, 1); // Rows aren't padded to 4 bytes
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, res.x, res.y, GL_RGB, GL_UNSIGNED_BYTE, buf);

    *ow = res.x;
    *oh = res.y;
    return buf;
}

void GLBackend::RenderingMeshOld(TRenderNode *nod)
{
    if ( !_sceneBeginned )
        return;
    
    if (!nod)
        return;
    
    TMesh *mesh = nod->Mesh;
    
    if (!mesh)
        return;
    
    uint32_t flags = nod->Flags;

    _states.Shaded = false;
    _states.Stipple = false;
    _states.SrcBlend = GL_ONE;
    _states.DstBlend = GL_ZERO;
    _states.TexBlend = 0; //REPLACE
    _states.AlphaBlend = false;
    _states.DepthTest = true;
    _states.Zwrite = true;
    _states.Tex = 0;
    _states.LinearFilter = (Engine._filter != 0);
    _states.Fog = false;
    _states.AlphaTest = false;
    _states.AFog = false;
    _states.Prog = TShaderProg();
    
    bool useComputedColor = false;

    if ( flags & RFLAGS_TEXTURED )
    {
        if (nod->Tex)
            _states.Tex = nod->Tex->hwTex;
    }

    if ( flags & RFLAGS_SHADED )
    {
        _states.TexBlend = 2; //MODULATE
        _states.Shaded = true;
    }
    
    if ( flags & RFLAGS_FOG )
    {
        _states.Fog = true;
        _states.FogStart = nod->FogStart;
        _states.FogLength = nod->FogLength;
    }
    
    if ( flags & RFLAGS_LUMTRACY )
    {
        if ( !Engine._zbuf_when_tracy )
            _states.Zwrite = false;

        if ( GFXEngine::can_destblend )
        {
            _states.AlphaBlend = true;
            _states.TexBlend = 1; //MODULATEALPHA;
            _states.SrcBlend = GL_ONE;
            _states.DstBlend = GL_ONE;
            _states.Shaded = false;
        }
        else if ( GFXEngine::can_srcblend )
        {
            _states.AlphaBlend = true;
            _states.TexBlend = 1; //MODULATEALPHA;
            _states.SrcBlend = GL_SRC_ALPHA;
            _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;
            _states.Shaded = false;
        }
        else if ( GFXEngine::can_stippling )
        {
            _states.AlphaBlend = true;
            _states.TexBlend = 1; //MODULATEALPHA;
            _states.SrcBlend = GL_SRC_ALPHA;//D3DBLEND_SRCALPHA;, 
            _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;//D3DBLEND_INVSRCALPHA;
            _states.Stipple = true;
            _states.Shaded = false;
        }
    }
    else if ( flags & RFLAGS_ZEROTRACY )
    {
        _states.AlphaTest = true;        

        if ( SDL_BYTESPERPIXEL(GFXEngine::GetPixelFormatU32()) != 1 )
        {
            _states.SrcBlend = GL_SRC_ALPHA;//D3DBLEND_SRCALPHA;, 
            _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;//D3DBLEND_INVSRCALPHA;
        }

        _states.AlphaBlend = true;
        _states.LinearFilter = false;
        _states.TexBlend = 2; //MODULATE
    }

    if (flags & RFLAGS_SKY)
    {
        // The sky is a backdrop drawn before everything else, so it must
        // never hide geometry, however far away that geometry is.
        _states.DepthTest = false;
        _states.Zwrite = false;
        _states.Fog = false;
        _states.Shaded = true;
    }
    else if (flags & RFLAGS_FALLOFF)
    {
        float transDist = System::IniConf::GfxSkyDistance.Get<int>();
        float transLen = System::IniConf::GfxSkyLength.Get<int>();

        for(TVertex &v : mesh->Vertexes)
        {
            v.ComputedColor = v.Color;

            float distance = nod->TForm.Transform( v.Pos ).XZ().length();

            if (distance > transDist)
            {
                float prc = (distance - transDist) / transLen;
                if (prc > 1.0)
                    prc = 1.0;
                if (prc < 0.0)
                    prc = 0.0;

                prc = (1.0 - prc);

                v.ComputedColor.a *= prc;

                useComputedColor = true;
            }
        }

        if (useComputedColor)
        {
            _states.Zwrite = true;
            _states.AlphaBlend = true;
            _states.TexBlend = 2; // MODULATE
            _states.SrcBlend = GL_SRC_ALPHA;

            if (flags & RFLAGS_LUMTRACY)
                _states.DstBlend = GL_ONE;
            else
                _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;

            _states.Shaded = true;
            _states.Stipple = false;
        }
    }
    
    if (flags & RFLAGS_COMPUTED_COLOR)
        useComputedColor = true;
    
    SetRenderStates(0);

    SetModelViewMatrix( nod->TForm );
  
    glVertexPointer(3, GL_FLOAT, sizeof(TVertex), &mesh->Vertexes[0].Pos);
    
    if (useComputedColor)
        glColorPointer(4, GL_FLOAT, sizeof(TVertex), &mesh->Vertexes[0].ComputedColor);
    else
        glColorPointer(4, GL_FLOAT, sizeof(TVertex), &mesh->Vertexes[0].Color);
    
    if (flags & RFLAGS_TEXTURED)
    {
        if ( (flags & RFLAGS_DYNAMIC_TEXTURE) && nod->coordsID >= 0 )
            glTexCoordPointer(2, GL_FLOAT, sizeof(tUtV), nod->Mesh->CoordsCache.at( nod->coordsID ).Coords.data());
        else
            glTexCoordPointer(2, GL_FLOAT, sizeof(TVertex), &mesh->Vertexes[0].TexCoord);
    }
    
    glDrawElements(GL_TRIANGLES, mesh->Indixes.size(), GLINDEXTYPE, mesh->Indixes.data());

    System::Perf.Count(System::PerfStats::CNT_DRAWCALLS);
    System::Perf.Count(System::PerfStats::CNT_TRIANGLES, mesh->Indixes.size() / 3);
}

void GLBackend::RenderingMesh(TRenderNode *nod)
{
    if ( !_sceneBeginned )
        return;
    
    if (!nod)
        return;
    
    TMesh *mesh = nod->Mesh;
    
    if (!mesh)
        return;
    
    uint32_t flags = nod->Flags;

    _states.Shaded = false;
    _states.Stipple = false;
    _states.SrcBlend = GL_ONE;
    _states.DstBlend = GL_ZERO;
    _states.TexBlend = 0; //REPLACE
    _states.AlphaBlend = false;
    _states.DepthTest = true;
    _states.Zwrite = true;
    _states.Tex = 0;
    _states.LinearFilter = (Engine._filter != 0);
    _states.Fog = false;
    _states.AlphaTest = false;
    _states.AFog = false;
    _states.Prog = _stdShaderProg;
    
    if ( flags & RFLAGS_TEXTURED )
    {
        if (nod->Tex)
            _states.Tex = nod->Tex->hwTex;
    }

    if ( flags & RFLAGS_SHADED )
    {
        _states.TexBlend = 2; //MODULATE
        _states.Shaded = true;
    }
    
    if ( flags & RFLAGS_FOG )
    {
        _states.Fog = true;
        _states.FogStart = nod->FogStart;
        _states.FogLength = nod->FogLength;
    }
    
    if ( flags & RFLAGS_LUMTRACY )
    {
        if ( !Engine._zbuf_when_tracy )
            _states.Zwrite = false;

        if ( GFXEngine::can_destblend )
        {
            _states.AlphaBlend = true;
            _states.TexBlend = 1; //MODULATEALPHA;
            _states.SrcBlend = GL_ONE;
            _states.DstBlend = GL_ONE;
            _states.Shaded = false;
        }
        else if ( GFXEngine::can_srcblend )
        {
            _states.AlphaBlend = true;
            _states.TexBlend = 1; //MODULATEALPHA;
            _states.SrcBlend = GL_SRC_ALPHA;
            _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;
            _states.Shaded = false;
        }
        else if ( GFXEngine::can_stippling )
        {
            _states.AlphaBlend = true;
            _states.TexBlend = 1; //MODULATEALPHA;
            _states.SrcBlend = GL_SRC_ALPHA;//D3DBLEND_SRCALPHA;, 
            _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;//D3DBLEND_INVSRCALPHA;
            _states.Stipple = true;
            _states.Shaded = false;
        }
    }
    else if ( flags & RFLAGS_ZEROTRACY )
    {
        _states.AlphaTest = true;        

        if ( SDL_BYTESPERPIXEL(GFXEngine::GetPixelFormatU32()) != 1 )
        {
            _states.SrcBlend = GL_SRC_ALPHA;//D3DBLEND_SRCALPHA;, 
            _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;//D3DBLEND_INVSRCALPHA;
        }

        _states.AlphaBlend = true;
        _states.LinearFilter = false;
        _states.TexBlend = 2; //MODULATE
    }

    if (flags & RFLAGS_SKY)
    {
        // The sky is a backdrop drawn before everything else, so it must
        // never hide geometry, however far away that geometry is.
        _states.DepthTest = false;
        _states.Zwrite = false;
        _states.Fog = false;
        _states.Shaded = true;
    }
    else if (flags & RFLAGS_FALLOFF)
    {
        _states.Zwrite = true;
        _states.AlphaBlend = true;
        _states.TexBlend = 2; // MODULATE
        _states.SrcBlend = GL_SRC_ALPHA;

        if (flags & RFLAGS_LUMTRACY)
            _states.DstBlend = GL_ONE;
        else
            _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;

        _states.Shaded = true;
        _states.Stipple = false;
        
        _states.AFog = true; 
        _states.AFogStart = System::IniConf::GfxSkyDistance.Get<int>();
        _states.AFogLength = System::IniConf::GfxSkyLength.Get<int>();
    }
    
    _states.DataBuf = nod->Mesh->HwVertexBuf;
    _states.IndexBuf = nod->Mesh->HwIndexBuf;
    
    SetRenderStates(0);
    
    SetModelViewMatrix(nod->TForm);

    Glext::GLVertexAttribPointer(_lastStates.Prog.PosLoc, 3, GL_FLOAT, GL_FALSE,  sizeof(TVertex), (void *)offsetof(TVertex, Pos));
    
    if (flags & RFLAGS_COMPUTED_COLOR)
        Glext::GLVertexAttribPointer(_lastStates.Prog.ColorLoc, 4, GL_FLOAT, GL_FALSE,  sizeof(TVertex), (void *)offsetof(TVertex, ComputedColor));
    else
        Glext::GLVertexAttribPointer(_lastStates.Prog.ColorLoc, 4, GL_FLOAT, GL_FALSE,  sizeof(TVertex), (void *)offsetof(TVertex, Color));
    
    if (flags & RFLAGS_TEXTURED)
    {
        if ( (flags & RFLAGS_DYNAMIC_TEXTURE) && nod->coordsID >= 0 )
            Glext::GLVertexAttribPointer(_lastStates.Prog.UVLoc, 2, GL_FLOAT, GL_FALSE,  sizeof(tUtV), (void *)(size_t)nod->Mesh->CoordsCache.at( nod->coordsID ).BufferPos);
        else
            Glext::GLVertexAttribPointer(_lastStates.Prog.UVLoc, 2, GL_FLOAT, GL_FALSE,  sizeof(TVertex), (void *)offsetof(TVertex, TexCoord));
    }
    
    CommitUBOParameters();    
    glDrawElements(GL_TRIANGLES, mesh->Indixes.size(), GLINDEXTYPE, NULL);

    System::Perf.Count(System::PerfStats::CNT_DRAWCALLS);
    System::Perf.Count(System::PerfStats::CNT_TRIANGLES, mesh->Indixes.size() / 3);
}

void GLBackend::RenderNode(TRenderNode *node)
{
    if (!node)
        return;
    
    switch(node->Type)
    {
        case TRenderNode::TYPE_MESH:
            if (_vbo)
                RenderingMesh(node);
            else
                RenderingMeshOld(node);
            break;
            
        case TRenderNode::TYPE_PARTICLE:
        {
            if (_vbo)
                RenderingMesh(node);
            else
                RenderingMeshOld(node);
        }
            break;
            
        default:
            break;
    }
}

void GLBackend::SetRenderStates(int setAll)
{
//    static const std::array<int, 4> blends = {GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA};
    GfxStates *newStates;
    
    if (setAll < 2)
        newStates = &_states;
    else
        newStates = &_lastStates;
    
    bool forceSetShader = false;
    if (setAll)
        forceSetShader = true;
    
    if (_glext)
    {
        if (setAll || (newStates->Prog.ID != _lastStates.Prog.ID))
        {
            if (_vbo && _lastStates.Prog.ID)
            {
                if (_lastStates.Prog.PosLoc != -1)
                    Glext::GLDisableVertexAttribArray(_lastStates.Prog.PosLoc);
                
                if (_lastStates.Prog.ColorLoc != -1)
                    Glext::GLDisableVertexAttribArray(_lastStates.Prog.ColorLoc);
                
                if (_lastStates.Prog.UVLoc != -1)
                    Glext::GLDisableVertexAttribArray(_lastStates.Prog.UVLoc);
            }
            
            Glext::GLUseProgram(newStates->Prog.ID);
            
            forceSetShader = true;
            
            if (_vbo && newStates->Prog.ID)
            {
                if (newStates->Prog.PosLoc != -1)
                    Glext::GLEnableVertexAttribArray(newStates->Prog.PosLoc);
                
                if (newStates->Prog.ColorLoc != -1)
                    Glext::GLEnableVertexAttribArray(newStates->Prog.ColorLoc);
            }
        }
    }
    
    if (_vbo)
    {        
        if (setAll || (newStates->DataBuf != _lastStates.DataBuf))
        {
            Glext::GLBindBuffer(GL_ARRAY_BUFFER, newStates->DataBuf);
        }
        
        if (setAll || (newStates->IndexBuf != _lastStates.IndexBuf))
        {
            Glext::GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newStates->IndexBuf);
        }
        
        if (forceSetShader || (newStates->Tex != _lastStates.Tex))
        {
            System::Perf.Count(System::PerfStats::CNT_TEXCHANGES);

            if (newStates->Tex)
            {
                if (newStates->Prog.UVLoc != -1)
                    Glext::GLEnableVertexAttribArray(newStates->Prog.UVLoc);
                
                glBindTexture(GL_TEXTURE_2D, newStates->Tex);

                _vboStatesBlock.Textured = 1;
            }
            else
            {
                if (newStates->Prog.UVLoc != -1)
                    Glext::GLDisableVertexAttribArray(newStates->Prog.UVLoc);
                
                glBindTexture(GL_TEXTURE_2D, 0);

                _vboStatesBlock.Textured = 0;
            }
            _vboStatesChanged = true;
        }
        
        if ((forceSetShader || (newStates->Fog != _lastStates.Fog) ||
            (newStates->FogLength != _lastStates.FogLength) ||
            (newStates->FogStart != _lastStates.FogStart)) )
        {
            if (newStates->Fog)
            {
                _vboStatesBlock.Fog = 1.0;
                _vboStatesBlock.FogStart = newStates->FogStart;
                _vboStatesBlock.FogLength = newStates->FogLength;
            }
            else
            {
                _vboStatesBlock.Fog = 0.0;
            }
            _vboStatesChanged = true;
        }
        
        if ((forceSetShader || (newStates->AFog != _lastStates.AFog) ||
            (newStates->AFogLength != _lastStates.AFogLength) ||
            (newStates->AFogStart != _lastStates.AFogStart)) )
        {
            if (newStates->AFog)
            {
                _vboStatesBlock.AFog = 1.0;
                _vboStatesBlock.AFogStart = newStates->AFogStart;
                _vboStatesBlock.AFogLength = newStates->AFogLength;
            }
            else
            {
                _vboStatesBlock.AFog = 0.0;
            }
            _vboStatesChanged = true;
        }
        
        if (forceSetShader || (newStates->Shaded != _lastStates.Shaded))
        {
            if (newStates->Shaded)
            {
                _vboStatesBlock.Flat = 0;
            }
            else
            {
                _vboStatesBlock.Flat = 1;
            }
            _vboStatesChanged = true;
        }
        
        if (setAll || (newStates->AlphaTest != _lastStates.AlphaTest))
        {
            if (newStates->AlphaTest == false)
            {
                _vboStatesBlock.ATest = 0;
            }
            else
            {
                _vboStatesBlock.ATest = 1;
            }
            _vboStatesChanged = true;
        }
    }
    else
    {
        if (setAll || (newStates->Stipple != _lastStates.Stipple))
        {
        }

        if (setAll || (newStates->Fog != _lastStates.Fog) ||
            (newStates->FogLength != _lastStates.FogLength) ||
            (newStates->FogStart != _lastStates.FogStart) )
        {
            if (newStates->Fog)
            {
                glEnable(GL_FOG);
                glFogf(GL_FOG_DENSITY, 1.0);
                glFogi(GL_FOG_MODE, GL_LINEAR);
                glHint(GL_FOG_HINT, GL_DONT_CARE);

                float fcolors[4] = {0.0, 0.0, 0.0, 1.0};

                glFogfv(GL_FOG_COLOR, fcolors);
                
                glFogf(GL_FOG_START, newStates->FogStart);
                glFogf(GL_FOG_END, newStates->FogStart + newStates->FogLength);
            }
            else
            {
                glDisable(GL_FOG);
            }
        }
        
        if (setAll || (newStates->Shaded != _lastStates.Shaded))
        {
            if (newStates->Shaded)
                glShadeModel(GL_SMOOTH);
            else
                glShadeModel(GL_FLAT);
        }
        
        if (setAll || (newStates->Tex != _lastStates.Tex))
        {
            System::Perf.Count(System::PerfStats::CNT_TEXCHANGES);

            if (newStates->Tex)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, newStates->Tex);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            }
            else
            {
                glDisable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            }
        }
        
        if (setAll || (newStates->AlphaTest != _lastStates.AlphaTest))
        {
            if (newStates->AlphaTest == false)
            {
                glDisable(GL_ALPHA_TEST);
            }
            else
            {
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0.0);
            }
        }
    }
    
    if (setAll || (newStates->DepthTest != _lastStates.DepthTest))
    {
        if (newStates->DepthTest)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
    }

    if (setAll || (newStates->Tex != _lastStates.Tex)
               || (newStates->LinearFilter != _lastStates.LinearFilter) )
    {
        if (newStates->Tex)
        {
            if (newStates->LinearFilter)
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }
            else
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            }
        }
    }
    
    if (setAll || (newStates->SrcBlend != _lastStates.SrcBlend)
                   || (newStates->DstBlend != _lastStates.DstBlend))
    {
        glBlendFunc(newStates->SrcBlend, newStates->DstBlend);
    }

    if (setAll || (newStates->TexBlend != _lastStates.TexBlend))
    {
        if (newStates->TexBlend == 0)
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        else if (newStates->TexBlend == 1)
        {
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        }
        else if (newStates->TexBlend == 2)
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    if (setAll || (newStates->AlphaBlend != _lastStates.AlphaBlend))
    {
        if (newStates->AlphaBlend)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
    }

    if (setAll || (newStates->Zwrite != _lastStates.Zwrite))
    {
        if (newStates->Zwrite)
            glDepthMask(GL_TRUE);
        else
            glDepthMask(GL_FALSE);
    }

    if (setAll < 2)
        _lastStates = _states;
}

void GLBackend::DrawVtxQuad(const std::array<GFX::TVertex, 4> &vtx)
{
    static const uint16_t indexes[6] = {0, 1, 2, 0, 2, 3};
    
    if (_vbo)
    {
        if (!_stdQuadDataBuf)
        {
            Glext::GLGenBuffers(1, &_stdQuadDataBuf);
            Glext::GLBindBuffer(GL_ARRAY_BUFFER, _stdQuadDataBuf);
            Glext::GLBufferData(GL_ARRAY_BUFFER, sizeof(TVertex) * vtx.size(), NULL, GL_STREAM_DRAW);
        }
        
        if (!_stdQuadIndexBuf)
        {
            Glext::GLGenBuffers(1, &_stdQuadIndexBuf);
            Glext::GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _stdQuadIndexBuf);
            Glext::GLBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes), &indexes, GL_STATIC_DRAW);
        }
        
        _states.DataBuf = _stdQuadDataBuf;
        _states.IndexBuf = _stdQuadIndexBuf;
        
        SetRenderStates(0);
        
        Glext::GLBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(TVertex) * vtx.size(), vtx.data()); 
        
        if (_lastStates.Prog.PosLoc != -1)
            Glext::GLVertexAttribPointer(_lastStates.Prog.PosLoc, 3, GL_FLOAT, GL_FALSE,  sizeof(TVertex), (void *)offsetof(TVertex, Pos));  
        if (_lastStates.Prog.ColorLoc != -1)
            Glext::GLVertexAttribPointer(_lastStates.Prog.ColorLoc, 4, GL_FLOAT, GL_FALSE,  sizeof(TVertex), (void *)offsetof(TVertex, Color));
        if (_lastStates.Prog.UVLoc != -1)
            Glext::GLVertexAttribPointer(_lastStates.Prog.UVLoc, 2, GL_FLOAT, GL_FALSE,  sizeof(TVertex), (void *)offsetof(TVertex, TexCoord));
        
        CommitUBOParameters();    
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
    }
    else
    {
        glVertexPointer(3, GL_FLOAT, sizeof(TVertex), &vtx[0].Pos);
        glColorPointer(4, GL_FLOAT, sizeof(TVertex), &vtx[0].Color);
        glTexCoordPointer(2, GL_FLOAT, sizeof(TVertex), &vtx[0].TexCoord);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, &indexes);
    }

    System::Perf.Count(System::PerfStats::CNT_DRAWCALLS);
    System::Perf.Count(System::PerfStats::CNT_TRIANGLES, 2);
}

void GLBackend::DrawScreenSurface(SDL_Surface *screen)
{
    GfxStates save = _states;
    
    Common::Point scrSz = System::GetResolution();
    glViewport(0, 0, scrSz.x, scrSz.y);
    
    SetProjectionMatrix( mat4x4f() );
    SetModelViewMatrix( mat4x4f() );
    
    _states.DepthTest = false;
    _states.Zwrite = false;
    _states.AlphaBlend = true;
    _states.SrcBlend = GL_SRC_ALPHA;
    _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;
    _states.Tex = screenTex;
    _states.TexBlend = 2;
    _states.Prog = _stdShaderProg;
    _states.AlphaTest = false;
    _states.Shaded = true;
    _states.LinearFilter = true;

    SetRenderStates(0);
    
    // Will be binded with SetRenderStates
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, screen->w, screen->h, _glPixfmt, _glPixtype, screen->pixels);

    static std::array<TVertex, 4> vtx = {
        GFX::TVertex( vec3f(-1.0,  1.0, 0.0), tUtV(0.0, 0.0) ),
        GFX::TVertex( vec3f(-1.0, -1.0, 0.0), tUtV(0.0, 1.0) ),
        GFX::TVertex( vec3f( 1.0, -1.0, 0.0), tUtV(1.0, 1.0) ),
        GFX::TVertex( vec3f( 1.0,  1.0, 0.0), tUtV(1.0, 0.0) )
    };
    
    DrawVtxQuad(vtx);
    
    _states = save;
}

uint32_t GLBackend::CompileShader(int32_t type, const std::string &string)
{
    GLuint sh = Glext::GLCreateShader(type);
    if (!sh)
        return 0;
    
    const GLchar *source = (const GLchar *)string.c_str();
    
    Glext::GLShaderSource(sh, 1, &source, 0);
    Glext::GLCompileShader(sh);
    
    GLint tmpvar;
    Glext::GLGetShaderiv(sh, GL_COMPILE_STATUS, &tmpvar);
    
    if (tmpvar == GL_FALSE)
    {
        Glext::GLGetShaderiv(sh, GL_INFO_LOG_LENGTH, &tmpvar);
        if (tmpvar > 0)
        {
            char *logbuff = new char[tmpvar + 2];
            Glext::GLGetShaderInfoLog(sh, tmpvar, NULL, logbuff);
            printf("Shader error: %s\n", logbuff);
            log_d3dlog("Shader error: %s\n", logbuff);
            delete[] logbuff;
        }
        Glext::GLDeleteShader(sh);
        return 0;
    }
    
    return sh;
}


uint32_t GLBackend::LoadShader(int32_t type, const std::string &fl)
{
    FSMgr::FileHandle *f = FSMgr::iDir::openFileAlloc(fl, "rb");
    if (!f)
        return 0;

    f->seek(0, SEEK_END);
    size_t sz = f->tell();
    f->seek(0, SEEK_SET);

    char *tmp = new char[sz];
    f->read(tmp, sz);

    std::string b;
    b.assign(tmp, sz);

    delete[] tmp;

    delete f;
    
    return CompileShader(type, b);
}

void GLBackend::DrawFBO()
{
    GfxStates save = _states;
    
    Common::Point scrSz = System::GetResolution();
    glViewport(0, 0, scrSz.x, scrSz.y);
        
    SetProjectionMatrix( mat4x4f() );
    SetModelViewMatrix( mat4x4f() );
    
    _states.DepthTest = false;
    _states.Zwrite = false;
    _states.AlphaBlend = true;
    
    if (_fboBlend == 0)
    {
        _states.SrcBlend = GL_ONE;
        _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;
    }
    else if (_fboBlend == 1)
    {
        _states.SrcBlend = GL_SRC_ALPHA;
        _states.DstBlend = GL_ONE_MINUS_SRC_ALPHA;
    }
    
    _states.Tex = _fboTex;
    _states.TexBlend = 2;
    _states.Prog = _colorEffectsShaderProg;
    
    _states.AlphaTest = false;
    _states.Shaded = true;
    _states.LinearFilter = true;

    // Apply texture and program
    SetRenderStates(0);
    
    if (_colorEffectsShaderProg.NormLoc >= 0)
        Glext::GLUniform3f(_colorEffectsShaderProg.NormLoc, Engine.GetColorEffectNorm().x, Engine.GetColorEffectNorm().y, Engine.GetColorEffectNorm().z);
    
    if (_colorEffectsShaderProg.InvLoc >= 0)
        Glext::GLUniform3f(_colorEffectsShaderProg.InvLoc, Engine.GetColorEffectInv().x, Engine.GetColorEffectInv().y, Engine.GetColorEffectInv().z);
    
    if (_colorEffectsShaderProg.RandLoc >= 0)
        Glext::GLUniform1i(_colorEffectsShaderProg.RandLoc, rand());
    
    if (_colorEffectsShaderProg.ScrSizeLoc >= 0)
        Glext::GLUniform2i(_colorEffectsShaderProg.ScrSizeLoc, scrSz.x, scrSz.y);
    
    if (_colorEffectsShaderProg.MillisecsLoc >= 0)
        Glext::GLUniform1i(_colorEffectsShaderProg.MillisecsLoc, SDL_GetTicks());
    
    static std::array<TVertex, 4> vtx = {
        GFX::TVertex( vec3f(-1.0,  1.0, 0.0), tUtV(0.0, 1.0) ),
        GFX::TVertex( vec3f(-1.0, -1.0, 0.0), tUtV(0.0, 0.0) ),
        GFX::TVertex( vec3f( 1.0, -1.0, 0.0), tUtV(1.0, 0.0) ),
        GFX::TVertex( vec3f( 1.0,  1.0, 0.0), tUtV(1.0, 1.0) )
    };
    
    DrawVtxQuad(vtx);
    
    _states = save;
}

void GLBackend::UpdateFBOSizes()
{
    if (_colorEffects)
    {
        Common::Point scrSz = System::GetResolution();

        _states.Tex = _fboTex;
        SetRenderStates(0);
        
        glTexImage2D(GL_TEXTURE_2D, 0, FBOTEXTYPE, scrSz.x, scrSz.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        Glext::GLBindRenderbuffer(GL_RENDERBUFFER, _fbod);
        Glext::GLRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, scrSz.x, scrSz.y);
        Glext::GLBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
}

void GLBackend::BindVBOParameters(TShaderProg &shader)
{
    if (_vbo)
    {
        uint32_t blockIndex = Glext::GLGetUniformBlockIndex(shader.ID, "Parameters");   
        if (blockIndex != GL_INVALID_INDEX)
            Glext::GLUniformBlockBinding(shader.ID, blockIndex, _vboParamsBlockBinding);
    }
}

void GLBackend::CommitUBOParameters()
{
    if (_vbo && _vboStatesChanged)
    {
        _vboStatesChanged = false;
        
        Glext::GLBindBuffer(GL_UNIFORM_BUFFER, _vboParams);
        
        // Orphan ubo
        Glext::GLBufferData(GL_UNIFORM_BUFFER, _vboParamsSize, NULL, GL_STREAM_DRAW);
        
        Glext::GLBufferData(GL_UNIFORM_BUFFER, _vboParamsSize, &_vboStatesBlock, GL_STREAM_DRAW);
    }
}

void GLBackend::SetProjectionMatrix(const mat4x4f &mat)
{
    mat4x4f tmp = mat.Transpose();
    if (_vbo)
    {
        _vboStatesBlock.Proj = tmp;
        _vboStatesChanged = true;
    }
    else
    {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(&tmp.m00);
    }
}

void GLBackend::SetModelViewMatrix(const mat4x4f &mat)
{
    mat4x4f tmp = mat.Transpose();
    if (_vbo)
    {
        _vboStatesBlock.View = tmp;
        _vboStatesChanged = true;
    }
    else
    {
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(&tmp.m00);
    }
}

TShaderProg::TShaderProg(uint32_t id)
: ID(id)
{
    PosLoc = Glext::GLGetAttribLocation(ID, "vPos");
    UVLoc  = Glext::GLGetAttribLocation(ID, "vUV");
    ColorLoc = Glext::GLGetAttribLocation(ID, "vColor");
}

TColorEffectsProg::TColorEffectsProg(uint32_t id)
: TShaderProg(id)
{
    NormLoc = Glext::GLGetUniformLocation(ID, "normclr");
    InvLoc = Glext::GLGetUniformLocation(ID, "invclr");
    RandLoc = Glext::GLGetUniformLocation(ID, "randval");
    ScrSizeLoc = Glext::GLGetUniformLocation(ID, "screenSize");
    MillisecsLoc = Glext::GLGetUniformLocation(ID, "millisecs");
}

void GLBackend::DrawVideoFrame(const uint8_t *rgb, int width, int height)
{
    Common::Point scrSz = System::GetResolution();
    glViewport(0, 0, scrSz.x, scrSz.y);
    
    SetProjectionMatrix( mat4x4f() );
    SetModelViewMatrix( mat4x4f() );

    GfxStates saved = _states;

    _states.Zwrite = false;
    _states.DepthTest = false;
    _states.AlphaBlend = false;
    _states.Prog = _stdShaderProg;

    SetRenderStates(0);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (_videoTex == 0)
    {
        glGenTextures(1, &_videoTex);
        
        _states.Tex = _videoTex;
        SetRenderStates(0);
        
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    }
    
    _states.Tex = _videoTex;
    SetRenderStates(0);
    
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    
    double scrAspect = (double)scrSz.x / (double)scrSz.y;
    double frmAspect = (double)width / (double)height;
    double qW = 1.0;
    double qH = 1.0;
    if (scrAspect > frmAspect)
        qW = frmAspect / scrAspect;
    else if (scrAspect < frmAspect)
        qH = scrAspect / frmAspect;

    std::array<GFX::TVertex, 4> vtx = {
        GFX::TVertex( vec3f(-1.0 * qW,  1.0 * qH, 0.0), tUtV(0.0, 0.0) ),
        GFX::TVertex( vec3f(-1.0 * qW, -1.0 * qH, 0.0), tUtV(0.0, 1.0) ),
        GFX::TVertex( vec3f( 1.0 * qW, -1.0 * qH, 0.0), tUtV(1.0, 1.0) ),
        GFX::TVertex( vec3f( 1.0 * qW,  1.0 * qH, 0.0), tUtV(1.0, 0.0) ),
    };
    
    DrawVtxQuad(vtx);

    SDL_GL_SwapWindow(_window);
    
    _states = saved;
}

void GLBackend::EndVideo()
{
    if (_videoTex != 0)
        glDeleteTextures(1, &_videoTex);

    _videoTex = 0;
}

}
