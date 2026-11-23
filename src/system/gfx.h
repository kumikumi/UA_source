#ifndef ENGINE_GFX_H_INCLUDED
#define ENGINE_GFX_H_INCLUDED

#include <array>
#include <deque>
#include <math.h>
#include <inttypes.h>

#include "system.h"
#include "../ini.h"
#include "../vectors.h"
#include "../matrix.h"
#include "../IFFile.h"
#include "../nucleas.h"
#include "common/common.h"
#include "common/cachestor.h"
#include "common/line.h"


#define GFX_MAX_VERTEX 12

class NC_STACK_bitmap;

typedef std::array<SDL_Color, 256> UA_PALETTE;

struct ResBitmap
{    
    int16_t width = 0;
    int16_t height = 0;
    UA_PALETTE *palette = NULL;
    SDL_Surface *swTex = NULL;
    GLuint hwTex = 0;
};

typedef std::vector<uint8_t> CmdStream;
typedef std::vector< CmdStream* > CmdIncludes;

struct TileMap
{
    NC_STACK_bitmap *img = NULL;
    std::array<Common::PointRect, 256> map;
    int h = 0;
    
    TileMap();  
    ~TileMap();
    
    void Draw(SDL_Surface *surface, const Common::Point &pos, uint8_t c);
    void Draw(SDL_Surface *surface, const Common::PointRect &posRect, uint8_t c);
    void Draw(SDL_Surface *surface, const Common::Rect &posRect, uint8_t c);
    
    void Fill(SDL_Surface *surface, const Common::Rect &rect, uint8_t c);
    void Fill(SDL_Surface *surface, const Common::PointRect &rect, uint8_t c);
    
    //Special fill use 1pix column
    void FillColumn(SDL_Surface *surface, const Common::Rect &rect, uint8_t c);
    void FillColumn(SDL_Surface *surface, const Common::PointRect &rect, uint8_t c);
    
    int GetWidth(uint8_t c) const;
    int GetWidth(const std::string &str) const;
    Common::Point GetSize(uint8_t c) const;
};

struct w3d_a209
{
    char *cmdbuf = NULL;
    char **includ = NULL;
};

struct tUtV
{
    float tu = -1.0;
    float tv = -1.0;
    
    tUtV() = default;
    tUtV(float u, float v): tu(u), tv(v) {};
};




namespace GFX
{

#ifndef GFXSHORTINDEX    
    
typedef uint32_t IndexType;
#define GLINDEXTYPE GL_UNSIGNED_INT

#else

typedef uint16_t IndexType;
#define GLINDEXTYPE GL_UNSIGNED_SHORT

#endif

#ifndef __EMSCRIPTEN__
#define FBOTEXTYPE GL_RGBA32F
#else
#define FBOTEXTYPE GL_RGBA
#endif

enum RFLAGS
{
    RFLAGS_TEXTURED  =      (1 << 0),
    RFLAGS_DYNAMIC_TEXTURE = (1 << 1),
    RFLAGS_SHADED    =      (1 << 2),
    RFLAGS_FOG       =      (1 << 3),
    RFLAGS_ZEROTRACY =      (1 << 4),
    RFLAGS_LUMTRACY  =      (1 << 5),
    RFLAGS_SKY       =      (1 << 6),
    RFLAGS_FALLOFF   =      (1 << 7),
    RFLAGS_IGNORE_FALLOFF = (1 << 8),
    RFLAGS_COMPUTED_COLOR = (1 << 9),
};    

struct __attribute__((packed)) TGLColor
{
    float r = 1.0;
    float g = 1.0;
    float b = 1.0;
    float a = 1.0;
    
    TGLColor() = default;
    TGLColor(float _r, float _g, float _b, float _a = 1.0)
        : r(_r), g(_g), b(_b), a(_a) {};
};

struct TBoundBox
{
    vec3d Min;
    vec3d Max;
    
    bool Inited = false;
    
    void Add(const vec3d &pt) 
    {
        if (Inited)
        {
            if (pt.x < Min.x) Min.x = pt.x;
            if (pt.y < Min.y) Min.y = pt.y;
            if (pt.z < Min.z) Min.z = pt.z;
            if (pt.x > Max.x) Max.x = pt.x;
            if (pt.y > Max.y) Max.y = pt.y;
            if (pt.z > Max.z) Max.z = pt.z;
        }
        else
        {
            Inited = true;
            Min = pt;
            Max = pt;
        }
    }
    
    void Add(const TBoundBox &box) 
    {
        Add(box.Min);
        Add(box.Max);
    }
};

struct TRenderParams
{
    NC_STACK_bitmap *TexSource = NULL;
    ResBitmap *Tex = NULL;
    
    uint32_t Flags = 0;
    TGLColor Color;
    
    TRenderParams() = default;
    TRenderParams(const TRenderParams &) = default;
    TRenderParams(TRenderParams &&) = default;
    
    TRenderParams(uint32_t flags)
        : Flags(flags){};
    
    TRenderParams(NC_STACK_bitmap *tex, uint32_t flags)
        : TexSource(tex), Flags(flags){};
        
    TRenderParams(ResBitmap *tex, uint32_t flags)
        : Tex(tex), Flags(flags){};
    
    TRenderParams& operator=(const TRenderParams &) = default;
    TRenderParams& operator=(TRenderParams &&) = default;
    
    bool operator==(const TRenderParams &b);
};

struct TCoordsCache
{
    ResBitmap *Tex = NULL;
    std::vector<tUtV> Coords;
    
    // OpenGL
    int32_t BufferPos = 0;
};

struct TVertex
{
    vec3f Pos;
    //vec3f Normal;
    tUtV  TexCoord;
    TGLColor Color;
    TGLColor ComputedColor;
    uint32_t TexCoordId = 0;
    
    TVertex() = default;
    TVertex(const vec3f &p): Pos(p) {};
    TVertex(const vec3f &p, const tUtV &uv): Pos(p), TexCoord(uv) {};
    TVertex(const vec3f &p, const tUtV &uv, const TGLColor &clr): Pos(p), TexCoord(uv), Color(clr) {};
};

class TMesh
{
public:
    TRenderParams Mat;
    std::vector<TVertex> Vertexes;
    std::vector<IndexType> Indixes;
    
    std::vector<TCoordsCache> CoordsCache;
    
    TBoundBox BoundBox;
    
    TMesh() = default;
    ~TMesh();
    TMesh(const TMesh&);
    TMesh(TMesh&&) = default;
    
    TMesh &operator=(const TMesh&);
    TMesh &operator=(TMesh&&) = default;
    
    void RecalcBoundBox()
    {
        BoundBox = TBoundBox();
        for (const TVertex &v : Vertexes)
            BoundBox.Add(v.Pos);
    }
    
    // OpenGL
    // Do not copy it!
    GLuint glDataBuf = 0;
    GLuint glIndexBuf = 0;
    //GLuint glVao = 0;
};

struct TRenderNode
{
    enum
    {
        TYPE_MESH,
        TYPE_PARTICLE
    };
    
    uint8_t Type = TYPE_MESH;
    
    TMesh *Mesh = NULL;
    
    int32_t coordsID = -1;
    
    ResBitmap *Tex = NULL;
    
    uint32_t Flags = 0;
    TGLColor Color;
    
    mat4x4 TForm;
    int32_t TimeStamp = 0; 
    int32_t FrameTime = 0;
    
    float FogStart = 0.0;
    float FogLength = 0.0;
    
    float Distance = 0.0;
    
    float ParticleSize = 0.0;
    
    TRenderNode() = default;
    TRenderNode(TRenderNode &&) = default;
    TRenderNode(const TRenderNode &) = default;
    
    TRenderNode& operator=(const TRenderNode &) = default;
    TRenderNode& operator=(TRenderNode &&) = default;
    
    TRenderNode(uint8_t tp): Type(tp) {};
    
    static bool CompareSolid(TRenderNode *a, TRenderNode *b);
    static bool CompareTransparent(TRenderNode *a, TRenderNode *b);
    static bool CompareDistance(TRenderNode *a, TRenderNode *b);
};
    
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

struct TBitmapBlitParams
{
    ResBitmap *pbitm = NULL;
    Common::FRect srcRect;
    Common::FRect dstRect;
};

struct TMaskedBlitParams
{
    ResBitmap *bitmap;
    ResBitmap *mask;
    int maskIndex;
    Common::FRect srcRect;
    Common::FRect dstRect;
};

struct ColorFx
{
    int Id = 0;
    float Pwr = 1.0;
    
    ColorFx() {};
    ColorFx(int id, float pwr) : Id(id), Pwr(pwr) {};
};

// TODO(cleanup): bitm is written but never read; SetCursor only takes the id.
struct TCursorParams
{
    ResBitmap *bitm = NULL;
    int pointer_id = 0;
};

struct windd_dlgBox
{
    const char *title;
    const char *ok;
    const char *cancel;
    const char *startText;
    int time;
    void (*timer_func)(int, int, int);
    void *timer_context;
    int replace;
    int maxLen;
    char *result;
};


struct TGFXDeviceInfo
{
    std::string name;
    std::string guid;
    bool isCurrent = false;
    
    TGFXDeviceInfo(const std::string &__name, const std::string &__guid)
    : name(__name), guid(__guid) {}
};

struct ScreenText
{
    std::string string;
    int p1;
    int p2;
    int p3;
    int p4;
    int flag;
};

struct ScreenFont
{
    TTF_Font *ttfFont = NULL;
    int height = 0;
    uint8_t r = 0, g = 0, b = 0;

    std::list<ScreenText *> entries;
};
    
enum WDD_ATT
{
    WDD_ATT_CURSOR = 0x80002000,
    WDD_ATT_TEXFILT = 0x80002001,
    WDD_ATT_DIS_LOWRES = 0x80002002,
    WDD_ATT_16BIT_TEX = 0x80002003,
    WDD_ATT_DRAW_PRIM = 0x80002004
};

enum DISP_ATT
{
    ATT_FGPEN       = 0x80001000,
    ATT_BGPEN       = 0x80001001,
    ATT_SHADE_RMP   = 0x80001002,
    ATT_TRACY_RMP   = 0x80001003,
    ATT_FGAPEN      = 0x80001004,
    ATT_WIDTH       = 0x80001005,
    ATT_HEIGHT      = 0x80001006,
    ATT_PALETTE     = 0x80001007,
    ATT_DISPLAY_ID  = 0x80001008,
    ATT_DISPLAY_INF = 0x80001009,
    ATT_DISPLAY_WIN = 0x8000100A,
};

enum RASTER
{
    RASTER_SOLID   = (1 << 0),
    RASTER_SKY     = (1 << 1),
    RASTER_ZEROTR  = (1 << 2),
    RASTER_LUMATR  = (1 << 3),
    RASTER_PARTICL = (1 << 4),
    
    RASTER_ALL = (RASTER_SOLID | RASTER_SKY | RASTER_ZEROTR | RASTER_LUMATR | RASTER_PARTICL)
};
    
class GFXEngine
{
public:
    static const std::array<vec3d, 8> _clrEff;
    
    static int can_srcblend;
    static int can_destblend;
    static int can_stippling;
    static uint32_t FpsMaxTicks;

protected:
    static SDL_PixelFormat *_pixfmt;
    static GLint _glPixfmt, _glPixtype;
    static bool _staticInited;
    
    static void StaticInit();
    
public:
    void Init();
    void Deinit();
    
    void RecreateScreenSurface();
    void DrawScreenSurface();
    
    uint32_t LoadShader(int32_t type, const std::string &fl);
    uint32_t CompileShader(int32_t type, const std::string &string);
    
    void SetFBOBlending(int mode);
    void DrawFBO();
    void UpdateFBOSizes();
    
    
    SDL_Surface *Screen();
    

    uint8_t *MakeScreenCopy(int *ow, int *oh);
    uint8_t *MakeDepthScreenCopy(int *ow, int *oh);

    
    // Fullscreen covers the desktop without changing the display mode, so the
    // display keeps its resolution and refresh rate.
    void SetFullscreen(bool fullscreen);

    // Vertical sync on or off; gfx.vsync 2 makes "on" adaptive.
    void SetVsync(bool enabled);

    // Renders at the window's current size. Returns true if it changed; call
    // outside BeginFrame/EndFrame.
    bool UpdateResolution();

    void SetTracyRmp(ResBitmap *rmp);
    void SetShadeRmp(ResBitmap *rmp);

    void SetCursor(int curID, int force);

    void SetTileset(TileMap *tileset, int id);
    TileMap * GetTileset(int id);

    bool LoadPalette(const std::string &palette_ilbm);
    SDL_Cursor *LoadCursor(const std::string &name);
    
    
    static uint32_t GetPixelFormatU32() { return _pixfmt->format; };
    static void GetGlPixTypeFmt(GLint *format, GLint *type);
    
    
    static void DrawLine(SDL_Surface *surface, const Common::Line &line, uint8_t cr, uint8_t cg, uint8_t cb);
    static void BlitScaleMasked(SDL_Surface *src, Common::Rect sRect, SDL_Surface *mask, uint8_t index, SDL_Surface *dst, Common::Rect dRect);
    static void DrawFill(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect);
    
    static void Draw(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, Common::Point dPoint);
    static void Draw(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, Common::PointRect dRect);
    
    static SDL_Color Color(uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255);
    static bool ColorCmp(const SDL_Color &a, const SDL_Color &b);
    
protected:
    template <typename T> 
    static void DrawFillIntCKey(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect);
    template <typename T> 
    static void DrawFillIntACpy(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect);
    static void DrawFillIntCpy(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect);
    
private:
    GFXEngine();
    ~GFXEngine();
    
    uint32_t CursPix(uint8_t *data, int ofs, int bpp);
    
    static int EventsWatcher(void *, SDL_Event *event);

public:
    virtual size_t ReadIniSettings();
    virtual size_t DrawLineNormalized(const Common::FLine &arg);
    virtual size_t DrawLineCentered(const Common::Line &arg);
    virtual size_t DrawClippedLineNormalized(const Common::FLine &arg);
    virtual size_t DrawClippedLineCentered(const Common::Line &r);
    virtual size_t DrawBitmapNormalized(TBitmapBlitParams *arg);
    virtual size_t DrawClippedBitmapNormalized(TBitmapBlitParams *arg);
    virtual void _SetTileset(int id, TileMap *tiles);
    virtual TileMap *_GetTileset(int id);
    virtual int GetTilesetID(TileMap *tiles);
    
    virtual void ProcessDrawSeq(const CmdStream &cmdline, const CmdIncludes *arr = NULL);
    
    virtual void SetClipNormalized(const Common::FRect &arg);
    virtual void SetClipCentered(const Common::Rect &arg);
    virtual void BeginScene();
    virtual void EndScene();
    virtual void DrawMaskedBitmapNormalized(TMaskedBlitParams *arg);
    virtual void SetInverseClipCentered(const Common::Rect &arg);
    virtual void BeginFrame();
    virtual void EndFrame();
    virtual void SetColorEffectsPowers(const std::vector<ColorFx> &arg);

    virtual bool AllocTexture(ResBitmap *arg);
    virtual void FreeTexture(ResBitmap *arg);
    virtual void SaveScreenshot(const std::string & screenName);
    virtual void BeginSystemDialog(IDVPair *);
    virtual void EndSystemDialog(IDVPair *);
    virtual void ShowTextInputDialog(windd_dlgBox *dlgBox);
    
    virtual const std::vector<TGFXDeviceInfo>& GetDevices();
    virtual void SetDeviceByGUID(const std::string &guid, bool writefile = true);

    
    void MeshMakeVBO(TMesh *);
    void MeshFreeVBO(TMesh *);
    

    virtual void setWDD_cursor(int arg);
    virtual void setWDD_disLowRes(int arg);
    virtual void setWDD_16bitTex(int arg);
    virtual void setWDD_drawPrim(int arg);

    virtual int getWDD_16bitTex();
    virtual int getWDD_drawPrim();
    
    
    
    virtual void SetPalette(UA_PALETTE &pal);
    virtual void SetPen(SDL_Color pen);
    virtual void setRSTR_BGpen(uint32_t pen) {};
    virtual void setRSTR_shdRmp(ResBitmap *rmp) {};
    virtual void setRSTR_trcRmp(ResBitmap *rmp) {};
    virtual void setRSTR_FGApen(uint32_t pen) {};

    virtual UA_PALETTE *GetPalette();

    virtual int16_t GetScreenW() const { return _resolution.x; }
    virtual int16_t GetScreenH() const { return _resolution.y; }
    virtual Common::Point GetScreenSize() const { return _resolution; };
    
    
    
    
    // windd methods
    size_t ReadIniDisplaySettings();

    virtual void setW3D_texFilt(int arg);

    void draw2DandFlush();
    
    void DrawVtxQuad(const std::array<GFX::TVertex, 4> &vtx);

    int LoadFontByDescr(const std::string &fontname);
    int GetTextWidth(const std::string &text); // In the font of AddScreenText's text
    void matrixAspectCorrection(mat3x3 &inout, bool invert);
    void getAspectCorrection(float &cW, float &cH, bool invert);

    void setFrustumClip(float near, float far);

    virtual void ConvAlphaPalette(UA_PALETTE *dst, const UA_PALETTE &src, bool transp);

    virtual SDL_Surface *CreateSurfaceScreenFormat(int width, int height);
    virtual SDL_Surface *ConvertToScreenFormat(SDL_Surface *src);
    
    static SDL_Surface *ConvertSDLSurface(SDL_Surface *src, const SDL_PixelFormat * fmt);
        
    void fpsLimitter(int value);
    void UpdateFpsLimit();
    
    // One line on the renderer in use, for the performance overlay.
    std::string GetRendererInfo();

    float GetColorEffectPower(int id);
    
    Common::Point ConvertPosTo2DStuff(const Common::Point &pos);
    
    void RenderingMeshOld(TRenderNode *mesh);
    void RenderingMesh(TRenderNode *mesh);
    void RenderNode(TRenderNode *node);
    void Rasterize(uint32_t RasterEtapes = RASTER_ALL);
    
    void QueueRenderMesh(TRenderNode *mesh);
    TRenderNode &AllocRenderNode() { return _renderNodesCache.GetNext(); };
    
    float GetAlpha() const { return _alpha / 255.0; };
    
    GfxStates& States() { return _states; };
    const GfxStates& GetLastStates() { return _lastStates; };
    
    const TShaderProg &GetStdShaderProg() { return _stdShaderProg; }; 
    
    void SetRenderStates(int arg);
    
    void BindVBOParameters(TShaderProg &shader);
    
    void CommitUBOParameters();
    
    void SetProjectionMatrix(const mat4x4f &mat);
    void SetModelViewMatrix(const mat4x4f &mat);
    
    const mat4x4f &GetProjectionMatrix() const { return _frustum; }; 
    float GetProjectionNear() const { return _frustumNear; }; 
    float GetProjectionFar() const { return _frustumFar; }; 
    
protected:
    void initPolyEngine();

    void DrawScreenText();
    void AddScreenText(const std::string &string, int p1, int p2, int p3, int p4, int flag);
    void DrawTextEntry(const ScreenText *txt);

    
    void WriteScreenshotPPM(FSMgr::FileHandle *fil);

    static SDL_Cursor *wrapLoadCursor(const char *name);

    void _setFrustumClip(float near, float far);
    
    void DrawClippedLine(Common::Line line, uint8_t r, uint8_t g, uint8_t b);
    char * RunTextInputDialog(const char *box_title, const char *box_ok, const char *box_cancel, const char *box_startText, uint32_t timer_time, void (*timer_func)(int, int, int), void *timer_context, int replace, int maxLen);
    
    void ApplyResolution();

    bool SetResVariables(Common::Point res);
    

public:
    //Data
    //static constexpr const char * __ClassName = ;
    
    
public:
    static GFXEngine Instance;
    
private:
    std::array<SDL_Cursor *, 11> cursors;
    int CurrentCursorID = -1;
    
    SDL_Surface *ScreenSurface = NULL;
    GLuint screenTex = 0;
    
    static const std::string _stdPShaderText;
    static const std::string _stdVShaderText;

public:
    int _forcesoftcursor;
    int _txt16bit;
    int _use_simple_d3d;
    int _disable_lowres;
    int _export_window_mode;
    int _flags;

    // From bigdata
    // \From bigdata

    int _dither;
    int _filter;
    int _antialias;
    int _alpha;
    int _zbuf_when_tracy;
    int _colorkey;

    
    
    std::list<TRenderNode *> _renderSolidList;
    std::list<TRenderNode *> _renderSkyBoxList;
    std::list<TRenderNode *> _renderZeroTracyList;
    std::list<TRenderNode *> _renderLumaTracyList;
    
    Common::CacheStorage<TRenderNode> _renderNodesCache;

    int _sceneBeginned;

    ScreenFont _font;

    float _corrW, _corrIW;
    float _corrH, _corrIH;
    bool _solidFont;
    
    bool _glext = false;
    bool _vbo = false;
    int _fpsMaxSetting = 0; // gfx.maxfps
    
    // Display class
    SDL_Color _penColor = {0}; // For the lines, set with SetPen
    Common::Rect _clip;
    Common::Rect _inverseClip;
    std::array<TileMap *, 256> _tiles;
    int _centerX = 0; // Where the centred 2D coordinates have their origin
    int _centerY = 0;
    float _normScaleX = 0; // Pixels per unit of the normalized 2D coordinates, which run from -1 to 1
    float _normScaleY = 0;
    
    UA_PALETTE _palette;
    
protected:
    // Display class
    Common::Point _resolution;

    // Win3d
    mat4x4f _frustum;
    float _frustumNear;
    float _frustumFar;
    
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

    vec3d _normClr;
    vec3d _invClr;
    
    int32_t _colorEffects = 0;
    
    uint32_t _fbo = 0;
    uint32_t _fbod = 0;
    uint32_t _fboTex = 0;
    int32_t _fboBlend = 0;
    
    uint32_t _psShader = 0;
    uint32_t _vsShader = 0;
    TColorEffectsProg _colorEffectsShaderProg;
    
    static std::vector<TGFXDeviceInfo> _devices;
};
    
static constexpr GFXEngine &Engine = GFXEngine::Instance;






    
}

#endif // ENGINE_GFX_H_INCLUDED
