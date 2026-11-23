#ifndef ENGINE_GPU_BACKEND_H_INCLUDED
#define ENGINE_GPU_BACKEND_H_INCLUDED

#include <map>
#include <unordered_map>
#include <vector>
#include <SDL3/SDL_gpu.h>

#include "gfx.h"
#include "render_backend.h"

namespace GFX
{

// Hands out ranges of a buffer's elements, the first free one that fits.
// The buffer grows to End() when the data is uploaded.
class RangeAllocator
{
public:
    uint32_t Alloc(uint32_t count);
    void Free(uint32_t first);

    // One past the last element in use
    uint32_t End() const { return _end; };

protected:
    std::map<uint32_t, uint32_t> _free; // First element -> count
    std::unordered_map<uint32_t, uint32_t> _used;
    uint32_t _end = 0;
};

// SDL_GPU: Metal on macOS, Vulkan elsewhere.
//
// All meshes share one vertex and one index buffer, and all textures, which
// are 256 x 256, are the layers of one texture array. Each render node is an
// instance: the nodes of the same mesh are drawn with one indirect draw
// command, and the commands of a pipeline with one call. The frame's layers,
// its 3D scenes and the screen surface flushed between them, are collected
// until EndFrame, which uploads what they need and draws them in one render
// pass into the window.
class GPUBackend : public RenderBackend
{
public:
    // NULL if the system has no device SDL_GPU can use
    static GPUBackend *Create();

    // The driver SDL_GPU would use, such as "metal", without making a
    // device; empty if it has none
    static std::string AvailableDriver();

    // For the options: the driver's API, such as "Metal"
    static std::string DeviceName(const std::string &driver);

    std::string GetDriver() const { return SDL_GetGPUDeviceDriver(_device); };

    void PrepareWindow(SDL_PropertiesID props) override;
    void Init(SDL_Window *window) override;
    void Deinit() override;

    std::string GetInfo() override;
    void SetVsync(bool enabled) override;
    std::string GetVsyncMode() override;

    void ResetStates() override {};
    void ScreenSurfaceChanged(SDL_Surface *screen) override;
    void WindowResized() override {};

    void BeginFrame() override;
    uint64_t EndFrame(SDL_Surface *screen) override;
    void FlushScreenSurface(SDL_Surface *screen) override;

    void BeginScene(const mat4x4f &projection) override;
    void Rasterize(TRenderQueue &queue, uint32_t passes) override;
    void EndScene() override {};

    bool AllocTexture(ResBitmap *bitm) override;
    void FreeTexture(ResBitmap *bitm) override;

    void MakeMeshBuffers(TMesh *mesh) override;
    void FreeMeshBuffers(TMesh *mesh) override;

    void SetColorEffectsBlending(int mode) override;

    uint8_t *ReadScreen(int *ow, int *oh) override;

    void DrawVideoFrame(const uint8_t *rgb, int width, int height) override;
    void EndVideo() override;

    // Sorting key of a node: its pipeline and mesh, see QueueNodes
    struct TNodeKey
    {
        uint64_t Key;
        uint32_t Slot; // In _slotNodes and _slotInstances
    };

protected:
    // A vertex of the shared vertex buffer
    struct TGpuVertex
    {
        float Pos[3];
        float UV[2];
        float Color[4];
        float ComputedColor[4];
        uint32_t Index; // In its mesh
    };

    // A render node's instance data
    struct TGpuInstance
    {
        float Rows[3][4]; // Transform to view space
        float Fog[4];     // Fog start, 1 / length; fall-off start, 1 / length
        uint32_t Misc[4]; // Texture layer, frame coords, INST flags
    };

    // Where a mesh's data is in the shared buffers
    struct TMeshAlloc
    {
        uint32_t FirstVertex = 0;
        uint32_t FirstIndex = 0;
        uint32_t FirstCoords = 0;
        bool HasCoords = false;
    };

    // Data waiting for the next copy pass
    struct TPendingUpload
    {
        enum Target { VERTICES, INDICES, COORDS, TEXTURE_LAYER, QUAD };

        Target To;
        uint32_t DstOffset;  // Bytes, or the texture layer
        uint32_t SrcOffset;  // In _staging
        uint32_t Size;
    };

    enum PipelineKind
    {
        PIPE_WORLD,
        PIPE_WORLD_ALPHATEST,
        PIPE_QUAD,
        PIPE_COLOR_EFFECTS
    };

    enum BlendMode
    {
        BLEND_NONE,
        BLEND_ALPHA,           // SRC_ALPHA, ONE_MINUS_SRC_ALPHA
        BLEND_ADD,             // ONE, ONE
        BLEND_ALPHA_ADD,       // SRC_ALPHA, ONE
        BLEND_PREMULTIPLIED,   // ONE, ONE_MINUS_SRC_ALPHA
        BLEND_REPLACE          // Blending on, ONE, ZERO
    };

    // The pipeline as bits: what EncodePipeline packs
    struct TPipelineDesc
    {
        PipelineKind Kind = PIPE_WORLD;
        BlendMode Blend = BLEND_NONE;
        bool DepthTest = true;
        bool DepthWrite = true;
        bool FloatTarget = false; // The color effects' scene target
    };

    // Consecutive indirect commands drawn with one call
    struct TBatch
    {
        uint32_t Pipeline; // EncodePipeline's key
        bool Linear;       // Texture filtering
        uint32_t FirstCommand;
        uint32_t Commands;
    };

    // A part of the frame, drawn over the earlier ones
    struct TLayer
    {
        bool Scene;        // A 3D scene, or else a screen surface
        uint32_t Index;    // The scene's batches start at _sceneBatches[Index]; the surface's texture
        uint32_t Batches;
        float Projection[4][4];
    };

protected:
    GPUBackend(SDL_GPUDevice *device);

    SDL_GPUShader *LoadShader(const char *name, SDL_GPUShaderStage stage,
                              uint32_t samplers, uint32_t storageBuffers, uint32_t uniformBuffers);
    void CreatePipelinesAndSamplers();
    SDL_GPUGraphicsPipeline *GetPipeline(uint32_t key);
    static uint32_t EncodePipeline(const TPipelineDesc &desc);
    static TPipelineDesc DecodePipeline(uint32_t key);

    // The render states the legacy OpenGL renderer would set for the node
    void NodeStates(const TRenderNode *node, uint32_t *pipeline, bool *linear,
                    TGpuInstance *inst) const;

    void QueueNodes(std::list<TRenderNode *> &list, bool keepOrder);

    uint32_t MeshAllocId(TMesh *mesh);
    uint32_t Stage(const void *data, uint32_t size, uint32_t align = 4);
    void CancelPending(TPendingUpload::Target to, uint32_t first, uint32_t size);
    SDL_GPUTextureFormat SurfaceTextureFormat() const;

    void StartFrame();
    void UploadPending(SDL_GPUCommandBuffer *cmd);
    void UploadFrameData(SDL_GPUCopyPass *pass);
    void UploadScreenSurface(SDL_GPUCopyPass *pass, SDL_Surface *screen, uint32_t texture);
    void AddScreenSurfaceLayer(SDL_GPUCopyPass *pass, SDL_Surface *screen);

    SDL_GPUBuffer *GrowBuffer(SDL_GPUCopyPass *pass, SDL_GPUBuffer *buffer, uint32_t *capacity,
                              uint32_t needed, SDL_GPUBufferUsageFlags usage);
    SDL_GPUTransferBuffer *UploadBuffer(SDL_GPUTransferBuffer *tb, uint32_t *capacity, uint32_t size);

    void EnsureDepth(uint32_t w, uint32_t h);
    void EnsureEffectsTarget(uint32_t w, uint32_t h);
    void DrawColorEffects(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd);

    // count: add the draw calls to the frame's statistics
    void DrawLayers(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, bool count);
    void DrawScene(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd, const TLayer &layer,
                   bool floatTarget, bool count);
    void DrawQuad(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd, uint32_t pipeline,
                  SDL_GPUTexture *texture, float scaleX, float scaleY);

protected:
    SDL_GPUDevice *_device = NULL;
    SDL_Window *_window = NULL;
    SDL_GPUShaderFormat _shaderFormat = SDL_GPU_SHADERFORMAT_INVALID;
    SDL_GPUTextureFormat _swapchainFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUPresentMode _presentMode = SDL_GPU_PRESENTMODE_VSYNC;

    SDL_GPUShader *_vsWorld = NULL;
    SDL_GPUShader *_fsWorld = NULL;
    SDL_GPUShader *_fsWorldAlphaTest = NULL;
    SDL_GPUShader *_vsQuad = NULL;
    SDL_GPUShader *_fsQuad = NULL;
    SDL_GPUShader *_fsColorEffects = NULL;

    std::unordered_map<uint32_t, SDL_GPUGraphicsPipeline *> _pipelines;

    SDL_GPUSampler *_linearSampler = NULL;
    SDL_GPUSampler *_nearestSampler = NULL;

    // The shared mesh buffers, sized in vertices, indices and coords
    SDL_GPUBuffer *_vertexBuffer = NULL;
    SDL_GPUBuffer *_indexBuffer = NULL;
    SDL_GPUBuffer *_coordsBuffer = NULL;
    uint32_t _vertexCapacity = 0;
    uint32_t _indexCapacity = 0;
    uint32_t _coordsCapacity = 0;
    RangeAllocator _vertexRanges;
    RangeAllocator _indexRanges;
    RangeAllocator _coordsRanges;

    std::vector<TMeshAlloc> _meshAllocs; // By TMesh::HwVertexBuf - 1
    std::vector<uint32_t> _freeMeshAllocs;

    // All textures; layer 0 is white, for meshes without one
    SDL_GPUTexture *_textures = NULL;
    uint32_t _textureLayers = 0;
    uint32_t _nextLayer = 1;
    std::vector<uint32_t> _freeLayers;
    SDL_GPUTextureFormat _textureFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

    std::vector<uint8_t> _staging;
    std::vector<TPendingUpload> _pending;
    SDL_GPUTransferBuffer *_pendingTransfer = NULL;
    uint32_t _pendingTransferSize = 0;

    // The four corners of the quads
    SDL_GPUBuffer *_quadBuffer = NULL;

    // The frame being drawn
    SDL_GPUCommandBuffer *_cmd = NULL;
    bool _frameStarted = false;
    std::vector<TLayer> _layers;
    std::vector<TBatch> _sceneBatches;
    std::vector<SDL_GPUIndexedIndirectDrawCommand> _commands;
    std::vector<TGpuInstance> _instances;
    std::vector<TNodeKey> _nodeKeys;
    std::vector<TNodeKey> _nodeKeysTmp;
    std::vector<TRenderNode *> _slotNodes;
    std::vector<TGpuInstance> _slotInstances;
    float _falloffStart = 0.0; // gfx.skydistance
    float _falloffScale = 0.0; // 1 / gfx.skylength
    uint32_t _surfaceLayers = 0;

    SDL_GPUBuffer *_instanceBuffer = NULL;
    SDL_GPUBuffer *_commandBuffer = NULL;
    uint32_t _instanceCapacity = 0;
    uint32_t _commandCapacity = 0;
    SDL_GPUTransferBuffer *_frameTransfer = NULL;
    uint32_t _frameTransferSize = 0;

    // The screen surface, once per layer of the frame
    std::vector<SDL_GPUTexture *> _surfaceTextures;
    uint32_t _surfaceW = 0;
    uint32_t _surfaceH = 0;
    SDL_GPUTransferBuffer *_surfaceTransfer = NULL;

    SDL_GPUTexture *_depth = NULL;
    uint32_t _depthW = 0;
    uint32_t _depthH = 0;

    SDL_GPUTexture *_videoTexture = NULL;
    SDL_GPUTransferBuffer *_videoTransfer = NULL;
    int _videoW = 0;
    int _videoH = 0;

    float _projection[4][4] = {};

    // gfx.color_effects: the scenes are drawn into _effectsTarget, which is
    // then drawn into the window through the effects' shader, before (2) or
    // after (1) the screen surface
    int _colorEffects = 0;
    int _colorEffectsBlending = 0;
    SDL_GPUTexture *_effectsTarget = NULL;
    uint32_t _effectsW = 0;
    uint32_t _effectsH = 0;
};

}

#endif // ENGINE_GPU_BACKEND_H_INCLUDED
