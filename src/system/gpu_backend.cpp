#include <algorithm>
#include <cstring>

#include "../fmtlib/printf.h"
#include "gpu_backend.h"
#include "gpu_shaders.h"
#include "inivals.h"
#include "perf.h"

#ifdef __APPLE__
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace GFX
{

namespace
{
// The constants the shaders share with this file
typedef uint32_t uint;
#include "../shaders/common.slang"

const uint32_t NO_FRAME_COORDS = 0xFFFFFFFF;

// All textures the game draws in 3D are this size
const uint32_t TEXTURE_SIZE = 256;

const SDL_GPUTextureFormat DEPTH_FORMAT = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

// The corners of the quads: texture coordinates, top left first
const float QUAD_CORNERS[4][2] = { {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0} };
}


uint32_t RangeAllocator::Alloc(uint32_t count)
{
    for (std::map<uint32_t, uint32_t>::iterator it = _free.begin(); it != _free.end(); ++it)
    {
        if (it->second >= count)
        {
            uint32_t first = it->first;
            uint32_t rest = it->second - count;
            _free.erase(it);

            if (rest)
                _free[first + count] = rest;

            _used[first] = count;
            return first;
        }
    }

    uint32_t first = _end;
    _end += count;
    _used[first] = count;
    return first;
}

void RangeAllocator::Free(uint32_t first)
{
    std::unordered_map<uint32_t, uint32_t>::iterator used = _used.find(first);
    if (used == _used.end())
        return;

    uint32_t count = used->second;
    _used.erase(used);

    // Join the free ranges on either side
    std::map<uint32_t, uint32_t>::iterator next = _free.lower_bound(first);
    if (next != _free.end() && first + count == next->first)
    {
        count += next->second;
        next = _free.erase(next);
    }

    if (next != _free.begin())
    {
        std::map<uint32_t, uint32_t>::iterator prev = std::prev(next);
        if (prev->first + prev->second == first)
        {
            first = prev->first;
            count += prev->second;
            _free.erase(prev);
        }
    }

    if (first + count == _end)
        _end = first;
    else
        _free[first] = count;
}



#ifdef __APPLE__
// Unless the window has SDL_WINDOW_HIGH_PIXEL_DENSITY, macOS scales the frame
// up on Retina displays. It smooths a Metal layer's pixels, where OpenGL's
// stay sharp: keep them sharp, as they always were.
static void KeepPixelsSharp(SDL_Window *window)
{
    typedef id (*MsgId)(id, SEL);
    typedef id (*MsgIdIndex)(id, SEL, unsigned long);
    typedef id (*MsgIdString)(id, SEL, const char *);
    typedef unsigned long (*MsgCount)(id, SEL);
    typedef BOOL (*MsgIsKind)(id, SEL, Class);
    typedef void (*MsgSetId)(id, SEL, id);

    id nsWindow = (id)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    if (!nsWindow)
        return;

    Class metalLayer = objc_getClass("CAMetalLayer");
    id nearest = ((MsgIdString)objc_msgSend)((id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "nearest"); // kCAFilterNearest

    // SDL's Metal view is a subview of the window's content view
    id content = ((MsgId)objc_msgSend)(nsWindow, sel_registerName("contentView"));
    id subviews = ((MsgId)objc_msgSend)(content, sel_registerName("subviews"));
    unsigned long count = ((MsgCount)objc_msgSend)(subviews, sel_registerName("count"));

    for (unsigned long i = 0; i < count; i++)
    {
        id view = ((MsgIdIndex)objc_msgSend)(subviews, sel_registerName("objectAtIndex:"), i);
        id layer = ((MsgId)objc_msgSend)(view, sel_registerName("layer"));

        if (layer && ((MsgIsKind)objc_msgSend)(layer, sel_registerName("isKindOfClass:"), metalLayer))
            ((MsgSetId)objc_msgSend)(layer, sel_registerName("setMagnificationFilter:"), nearest);
    }
}
#endif

GPUBackend *GPUBackend::Create()
{
    // Validation finds misuse of the API, but slows the drawing down
    bool debug = System::FindCmdLineArg("-gpudebug") >= 0;

    SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV, debug, NULL);
    if (!device)
    {
        printf("SDL_GPU: no device (%s), using OpenGL\n", SDL_GetError());
        return NULL;
    }

    return new GPUBackend(device);
}

std::string GPUBackend::AvailableDriver()
{
    // SDL_CreateGPUDevice takes the first of these that works
    for (int i = 0; i < SDL_GetNumGPUDrivers(); i++)
    {
        const char *driver = SDL_GetGPUDriver(i);

        if (SDL_GPUSupportsShaderFormats(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV, driver))
            return driver;
    }

    return "";
}

std::string GPUBackend::DeviceName(const std::string &driver)
{
    if (driver == "metal")
        return "Metal";
    else if (driver == "vulkan")
        return "Vulkan";
    else if (driver == "direct3d12")
        return "Direct3D 12";

    return driver;
}

GPUBackend::GPUBackend(SDL_GPUDevice *device)
: _device(device)
{
    if (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_MSL)
        _shaderFormat = SDL_GPU_SHADERFORMAT_MSL;
    else
        _shaderFormat = SDL_GPU_SHADERFORMAT_SPIRV;
}

void GPUBackend::PrepareWindow(SDL_PropertiesID props)
{
    const char *driver = SDL_GetGPUDeviceDriver(_device);

    if (driver && !strcmp(driver, "vulkan"))
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
    else if (driver && !strcmp(driver, "metal"))
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_METAL_BOOLEAN, true);
}

void GPUBackend::Init(SDL_Window *window)
{
    _window = window;

    if (!SDL_ClaimWindowForGPUDevice(_device, window))
        printf("SDL_GPU: can't draw into the window: %s\n", SDL_GetError());

    printf("SDL_GPU: %s\n", SDL_GetGPUDeviceDriver(_device));

#ifdef __APPLE__
    KeepPixelsSharp(window);
#endif

    _swapchainFormat = SDL_GetGPUSwapchainTextureFormat(_device, window);
    _textureFormat = SurfaceTextureFormat();
    _colorEffects = System::IniConf::GfxColorEffects.Get<int32_t>();

    CreatePipelinesAndSamplers();

    SDL_GPUBufferCreateInfo bci = {};
    bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bci.size = sizeof(QUAD_CORNERS);
    _quadBuffer = SDL_CreateGPUBuffer(_device, &bci);
    _pending.push_back({TPendingUpload::QUAD, 0, Stage(QUAD_CORNERS, sizeof(QUAD_CORNERS)), sizeof(QUAD_CORNERS)});

    _vertexCapacity = 65536 * sizeof(TGpuVertex);
    _indexCapacity = 262144 * sizeof(IndexType);
    _coordsCapacity = 16384 * sizeof(tUtV);

    bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bci.size = _vertexCapacity;
    _vertexBuffer = SDL_CreateGPUBuffer(_device, &bci);

    bci.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    bci.size = _indexCapacity;
    _indexBuffer = SDL_CreateGPUBuffer(_device, &bci);

    bci.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    bci.size = _coordsCapacity;
    _coordsBuffer = SDL_CreateGPUBuffer(_device, &bci);

    _textureLayers = 64;

    SDL_GPUTextureCreateInfo tci = {};
    tci.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    tci.format = _textureFormat;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.width = TEXTURE_SIZE;
    tci.height = TEXTURE_SIZE;
    tci.layer_count_or_depth = _textureLayers;
    tci.num_levels = 1;
    _textures = SDL_CreateGPUTexture(_device, &tci);

    std::vector<uint8_t> white(TEXTURE_SIZE * TEXTURE_SIZE * 4, 0xFF);
    _pending.push_back({TPendingUpload::TEXTURE_LAYER, 0, Stage(white.data(), white.size(), 256), (uint32_t)white.size()});

    // Every pipeline the world may need, now rather than in the middle of
    // the game, the first time something is drawn with it
    uint64_t start = System::PerfStats::Now();

    for (int floatTarget = 0; floatTarget <= (_colorEffects ? 1 : 0); floatTarget++)
    {
        for (PipelineKind kind : {PIPE_WORLD, PIPE_WORLD_ALPHATEST})
        {
            for (BlendMode blend : {BLEND_NONE, BLEND_ALPHA, BLEND_ADD, BLEND_ALPHA_ADD})
            {
                for (int depth = 0; depth < 4; depth++)
                {
                    TPipelineDesc desc;
                    desc.Kind = kind;
                    desc.Blend = blend;
                    desc.DepthTest = depth & 1;
                    desc.DepthWrite = (depth >> 1) & 1;
                    desc.FloatTarget = floatTarget;

                    if (desc.DepthWrite && !desc.DepthTest)
                        continue;

                    GetPipeline(EncodePipeline(desc));
                }
            }
        }
    }

    printf("SDL_GPU: %d pipelines made in %.0f ms\n", (int)_pipelines.size(),
           System::PerfStats::TicksToMs(System::PerfStats::Now() - start));
}

void GPUBackend::Deinit()
{
    if (!_device)
        return;

    if (_cmd)
        SDL_CancelGPUCommandBuffer(_cmd);

    _cmd = NULL;

    SDL_WaitForGPUIdle(_device);

    for (std::pair<const uint32_t, SDL_GPUGraphicsPipeline *> &p : _pipelines)
        SDL_ReleaseGPUGraphicsPipeline(_device, p.second);

    _pipelines.clear();

    for (SDL_GPUShader *s : {_vsWorld, _fsWorld, _fsWorldAlphaTest, _vsQuad, _fsQuad, _fsColorEffects})
    {
        if (s)
            SDL_ReleaseGPUShader(_device, s);
    }

    for (SDL_GPUBuffer *b : {_vertexBuffer, _indexBuffer, _coordsBuffer, _quadBuffer, _instanceBuffer, _commandBuffer})
    {
        if (b)
            SDL_ReleaseGPUBuffer(_device, b);
    }

    for (SDL_GPUTexture *t : _surfaceTextures)
        SDL_ReleaseGPUTexture(_device, t);

    for (SDL_GPUTexture *t : {_textures, _depth, _videoTexture, _effectsTarget})
    {
        if (t)
            SDL_ReleaseGPUTexture(_device, t);
    }

    for (SDL_GPUTransferBuffer *t : {_pendingTransfer, _frameTransfer, _surfaceTransfer, _videoTransfer})
    {
        if (t)
            SDL_ReleaseGPUTransferBuffer(_device, t);
    }

    SDL_ReleaseGPUSampler(_device, _linearSampler);
    SDL_ReleaseGPUSampler(_device, _nearestSampler);

    SDL_ReleaseWindowFromGPUDevice(_device, _window);
    SDL_DestroyGPUDevice(_device);
    _device = NULL;
}

std::string GPUBackend::GetInfo()
{
    return fmt::sprintf("SDL_GPU %s renderer", SDL_GetGPUDeviceDriver(_device));
}

void GPUBackend::SetVsync(bool enabled)
{
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;

    if (!enabled)
    {
        if (SDL_WindowSupportsGPUPresentMode(_device, _window, SDL_GPU_PRESENTMODE_IMMEDIATE))
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        else if (SDL_WindowSupportsGPUPresentMode(_device, _window, SDL_GPU_PRESENTMODE_MAILBOX))
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }

    if (SDL_SetGPUSwapchainParameters(_device, _window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
        _presentMode = mode;
}

std::string GPUBackend::GetVsyncMode()
{
    return _presentMode == SDL_GPU_PRESENTMODE_VSYNC ? "on" : "off";
}

SDL_GPUTextureFormat GPUBackend::SurfaceTextureFormat() const
{
    // The same bytes as the screen surface's pixel format
    if (GFXEngine::GetPixelFormatU32() == SDL_PIXELFORMAT_ARGB8888)
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;

    return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
}

SDL_GPUShader *GPUBackend::LoadShader(const char *name, SDL_GPUShaderStage stage,
                                      uint32_t samplers, uint32_t storageBuffers, uint32_t uniformBuffers)
{
    for (size_t i = 0; i < GpuShaderCodeCount; i++)
    {
        const TGpuShaderCode &code = GpuShaderCode[i];
        if (strcmp(code.Name, name))
            continue;

        SDL_GPUShaderCreateInfo ci = {};
        ci.format = _shaderFormat;

        if (_shaderFormat == SDL_GPU_SHADERFORMAT_MSL)
        {
            ci.code = code.Msl;
            ci.code_size = code.MslSize;
        }
        else
        {
            ci.code = code.Spirv;
            ci.code_size = code.SpirvSize;
        }

        ci.entrypoint = name;
        ci.stage = stage;
        ci.num_samplers = samplers;
        ci.num_storage_buffers = storageBuffers;
        ci.num_uniform_buffers = uniformBuffers;

        SDL_GPUShader *shader = SDL_CreateGPUShader(_device, &ci);
        if (!shader)
            printf("SDL_GPU: shader %s failed: %s\n", name, SDL_GetError());

        return shader;
    }

    printf("SDL_GPU: no shader %s\n", name);
    return NULL;
}

void GPUBackend::CreatePipelinesAndSamplers()
{
    _vsWorld = LoadShader("vsWorld", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1, 1);
    _fsWorld = LoadShader("fsWorld", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0);
    _fsWorldAlphaTest = LoadShader("fsWorldAlphaTest", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0);
    _vsQuad = LoadShader("vsQuad", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 1);
    _fsQuad = LoadShader("fsQuad", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0);
    _fsColorEffects = LoadShader("fsColorEffects", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 1);

    SDL_GPUSamplerCreateInfo sci = {};
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;

    sci.min_filter = sci.mag_filter = SDL_GPU_FILTER_LINEAR;
    _linearSampler = SDL_CreateGPUSampler(_device, &sci);

    sci.min_filter = sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    _nearestSampler = SDL_CreateGPUSampler(_device, &sci);
}

uint32_t GPUBackend::EncodePipeline(const TPipelineDesc &desc)
{
    // The blend mode is the highest, so that the opaque pipelines sort first
    return (desc.Blend << 5) | (desc.Kind << 3) | (desc.DepthTest << 2) | (desc.DepthWrite << 1) | (desc.FloatTarget << 8);
}

GPUBackend::TPipelineDesc GPUBackend::DecodePipeline(uint32_t key)
{
    TPipelineDesc desc;
    desc.Blend = (BlendMode)((key >> 5) & 7);
    desc.Kind = (PipelineKind)((key >> 3) & 3);
    desc.DepthTest = (key >> 2) & 1;
    desc.DepthWrite = (key >> 1) & 1;
    desc.FloatTarget = (key >> 8) & 1;
    return desc;
}

SDL_GPUGraphicsPipeline *GPUBackend::GetPipeline(uint32_t key)
{
    std::unordered_map<uint32_t, SDL_GPUGraphicsPipeline *>::iterator it = _pipelines.find(key);
    if (it != _pipelines.end())
        return it->second;

    TPipelineDesc desc = DecodePipeline(key);
    bool world = (desc.Kind == PIPE_WORLD || desc.Kind == PIPE_WORLD_ALPHATEST);

    SDL_GPUColorTargetDescription target = {};
    target.format = desc.FloatTarget ? SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT : _swapchainFormat;

    SDL_GPUColorTargetBlendState &blend = target.blend_state;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.enable_blend = desc.Blend != BLEND_NONE;

    switch (desc.Blend)
    {
        case BLEND_ALPHA:
            blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            break;

        case BLEND_ADD:
            blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            break;

        case BLEND_ALPHA_ADD:
            blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            break;

        case BLEND_PREMULTIPLIED:
            blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            break;

        default:
            blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            break;
    }

    // OpenGL's glBlendFunc treats the alpha like the colors
    blend.src_alpha_blendfactor = blend.src_color_blendfactor;
    blend.dst_alpha_blendfactor = blend.dst_color_blendfactor;

    SDL_GPUVertexBufferDescription buffers[2] = {};
    SDL_GPUVertexAttribute attribs[10] = {};
    SDL_GPUGraphicsPipelineCreateInfo ci = {};

    if (world)
    {
        buffers[0].slot = 0;
        buffers[0].pitch = sizeof(TGpuVertex);
        buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        buffers[1].slot = 1;
        buffers[1].pitch = sizeof(TGpuInstance);
        buffers[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;

        attribs[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(TGpuVertex, Pos)};
        attribs[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(TGpuVertex, UV)};
        attribs[2] = {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TGpuVertex, Color)};
        attribs[3] = {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TGpuVertex, ComputedColor)};
        attribs[4] = {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT, offsetof(TGpuVertex, Index)};
        attribs[5] = {5, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TGpuInstance, Rows[0])};
        attribs[6] = {6, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TGpuInstance, Rows[1])};
        attribs[7] = {7, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TGpuInstance, Rows[2])};
        attribs[8] = {8, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(TGpuInstance, Fog)};
        attribs[9] = {9, 1, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, offsetof(TGpuInstance, Misc)};

        ci.vertex_input_state.num_vertex_buffers = 2;
        ci.vertex_input_state.num_vertex_attributes = 10;
        ci.vertex_shader = _vsWorld;
        ci.fragment_shader = (desc.Kind == PIPE_WORLD_ALPHATEST) ? _fsWorldAlphaTest : _fsWorld;
        ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

        // As OpenGL's defaults, which the game kept
        ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        ci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    }
    else
    {
        buffers[0].slot = 0;
        buffers[0].pitch = sizeof(QUAD_CORNERS[0]);
        buffers[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        attribs[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0};

        ci.vertex_input_state.num_vertex_buffers = 1;
        ci.vertex_input_state.num_vertex_attributes = 1;
        ci.vertex_shader = _vsQuad;
        ci.fragment_shader = (desc.Kind == PIPE_COLOR_EFFECTS) ? _fsColorEffects : _fsQuad;
        ci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
        ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    }

    ci.vertex_input_state.vertex_buffer_descriptions = buffers;
    ci.vertex_input_state.vertex_attributes = attribs;
    ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    // Clip what is beyond the far plane, as OpenGL does, rather than clamp
    // it onto the plane
    ci.rasterizer_state.enable_depth_clip = true;

    // The depth is reversed: 1 near, 0 far, which spreads its precision over
    // the distance far better
    ci.depth_stencil_state.enable_depth_test = desc.DepthTest;
    ci.depth_stencil_state.enable_depth_write = desc.DepthTest && desc.DepthWrite;
    ci.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;

    ci.target_info.color_target_descriptions = &target;
    ci.target_info.num_color_targets = 1;
    ci.target_info.has_depth_stencil_target = true;
    ci.target_info.depth_stencil_format = DEPTH_FORMAT;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(_device, &ci);
    if (!pipeline)
        printf("SDL_GPU: pipeline 0x%x failed: %s\n", key, SDL_GetError());

    _pipelines[key] = pipeline;
    return pipeline;
}

uint32_t GPUBackend::Stage(const void *data, uint32_t size, uint32_t align)
{
    uint32_t offset = (_staging.size() + align - 1) / align * align;
    _staging.resize(offset + size);

    if (data)
        memcpy(&_staging[offset], data, size);

    return offset;
}

void GPUBackend::CancelPending(TPendingUpload::Target to, uint32_t first, uint32_t size)
{
    // Uploads to a range freed before the next copy pass are dropped, so that
    // they cannot overwrite what a new owner of the range uploads
    for (size_t i = 0; i < _pending.size(); )
    {
        TPendingUpload &p = _pending[i];

        if (p.To == to && p.DstOffset < first + size && first < p.DstOffset + p.Size)
            _pending.erase(_pending.begin() + i);
        else
            i++;
    }
}



bool GPUBackend::AllocTexture(ResBitmap *bitm)
{
    if (!bitm->swTex || bitm->hwTex)
        return true;

    SDL_Surface *src = bitm->swTex;
    SDL_Surface *conv = NULL;

    if (src->format != (SDL_PixelFormat)GFXEngine::GetPixelFormatU32())
    {
        conv = GFXEngine::ConvertSDLSurface(src, (SDL_PixelFormat)GFXEngine::GetPixelFormatU32());
        if (!conv)
            return false;

        src = conv;
    }

    if (src->w != (int)TEXTURE_SIZE || src->h != (int)TEXTURE_SIZE)
    {
        // Not seen in the game's data: stretch it over a layer
        printf("SDL_GPU: scaling a %dx%d texture to %dx%d\n", src->w, src->h, TEXTURE_SIZE, TEXTURE_SIZE);

        SDL_Surface *scaled = SDL_ScaleSurface(src, TEXTURE_SIZE, TEXTURE_SIZE, SDL_SCALEMODE_LINEAR);

        if (conv)
            SDL_DestroySurface(conv);

        if (!scaled)
            return false;

        conv = scaled;
        src = scaled;
    }

    uint32_t layer;
    if (!_freeLayers.empty())
    {
        layer = _freeLayers.back();
        _freeLayers.pop_back();
    }
    else
        layer = _nextLayer++;

    const uint32_t rowSize = TEXTURE_SIZE * 4;
    uint32_t offset = Stage(NULL, rowSize * TEXTURE_SIZE, 256);

    SDL_LockSurface(src);
    for (uint32_t y = 0; y < TEXTURE_SIZE; y++)
        memcpy(&_staging[offset + y * rowSize], (uint8_t *)src->pixels + y * src->pitch, rowSize);
    SDL_UnlockSurface(src);

    if (conv)
        SDL_DestroySurface(conv);

    _pending.push_back({TPendingUpload::TEXTURE_LAYER, layer, offset, rowSize * TEXTURE_SIZE});
    bitm->hwTex = layer;
    return true;
}

void GPUBackend::FreeTexture(ResBitmap *bitm)
{
    if (!bitm->hwTex)
        return;

    CancelPending(TPendingUpload::TEXTURE_LAYER, bitm->hwTex, 1);
    _freeLayers.push_back(bitm->hwTex);
    bitm->hwTex = 0;
}

void GPUBackend::MakeMeshBuffers(TMesh *mesh)
{
    if (mesh->HwVertexBuf)
        FreeMeshBuffers(mesh);

    uint32_t vertexCount = mesh->Vertexes.size();
    uint32_t indexCount = mesh->Indixes.size();

    if (!vertexCount || !indexCount)
        return;

    uint32_t id;
    if (!_freeMeshAllocs.empty())
    {
        id = _freeMeshAllocs.back();
        _freeMeshAllocs.pop_back();
    }
    else
    {
        id = _meshAllocs.size();
        _meshAllocs.emplace_back();
    }

    TMeshAlloc &alloc = _meshAllocs[id];
    alloc.FirstVertex = _vertexRanges.Alloc(vertexCount);
    alloc.FirstIndex = _indexRanges.Alloc(indexCount);
    alloc.HasCoords = !mesh->CoordsCache.empty();

    uint32_t size = vertexCount * sizeof(TGpuVertex);
    uint32_t offset = Stage(NULL, size);
    TGpuVertex *dst = (TGpuVertex *)&_staging[offset];

    for (uint32_t i = 0; i < vertexCount; i++)
    {
        const TVertex &v = mesh->Vertexes[i];
        TGpuVertex &g = dst[i];

        g.Pos[0] = v.Pos.x;
        g.Pos[1] = v.Pos.y;
        g.Pos[2] = v.Pos.z;
        g.UV[0] = v.TexCoord.tu;
        g.UV[1] = v.TexCoord.tv;
        g.Color[0] = v.Color.r;
        g.Color[1] = v.Color.g;
        g.Color[2] = v.Color.b;
        g.Color[3] = v.Color.a;
        g.ComputedColor[0] = v.ComputedColor.r;
        g.ComputedColor[1] = v.ComputedColor.g;
        g.ComputedColor[2] = v.ComputedColor.b;
        g.ComputedColor[3] = v.ComputedColor.a;
        g.Index = i;
    }

    _pending.push_back({TPendingUpload::VERTICES, alloc.FirstVertex * (uint32_t)sizeof(TGpuVertex), offset, size});

    size = indexCount * sizeof(IndexType);
    _pending.push_back({TPendingUpload::INDICES, alloc.FirstIndex * (uint32_t)sizeof(IndexType),
                        Stage(mesh->Indixes.data(), size), size});

    if (alloc.HasCoords)
    {
        // The coords of each animation frame, one per vertex
        alloc.FirstCoords = _coordsRanges.Alloc(vertexCount * mesh->CoordsCache.size());

        for (size_t f = 0; f < mesh->CoordsCache.size(); f++)
        {
            TCoordsCache &frame = mesh->CoordsCache[f];
            frame.BufferPos = alloc.FirstCoords + f * vertexCount;

            size = vertexCount * sizeof(tUtV);
            _pending.push_back({TPendingUpload::COORDS, frame.BufferPos * (uint32_t)sizeof(tUtV),
                                Stage(frame.Coords.data(), size), size});
        }
    }

    mesh->HwVertexBuf = id + 1;
    mesh->HwIndexBuf = alloc.FirstIndex + 1;
}

void GPUBackend::FreeMeshBuffers(TMesh *mesh)
{
    if (!mesh->HwVertexBuf)
        return;

    uint32_t id = mesh->HwVertexBuf - 1;
    TMeshAlloc &alloc = _meshAllocs[id];

    CancelPending(TPendingUpload::VERTICES, alloc.FirstVertex * sizeof(TGpuVertex), mesh->Vertexes.size() * sizeof(TGpuVertex));
    CancelPending(TPendingUpload::INDICES, alloc.FirstIndex * sizeof(IndexType), mesh->Indixes.size() * sizeof(IndexType));
    _vertexRanges.Free(alloc.FirstVertex);
    _indexRanges.Free(alloc.FirstIndex);

    if (alloc.HasCoords)
    {
        CancelPending(TPendingUpload::COORDS, alloc.FirstCoords * sizeof(tUtV),
                      mesh->Vertexes.size() * mesh->CoordsCache.size() * sizeof(tUtV));
        _coordsRanges.Free(alloc.FirstCoords);
    }

    alloc = TMeshAlloc();
    _freeMeshAllocs.push_back(id);

    mesh->HwVertexBuf = 0;
    mesh->HwIndexBuf = 0;
}

uint32_t GPUBackend::MeshAllocId(TMesh *mesh)
{
    // Meshes are given their buffers when made, but a copy of one is not
    if (!mesh->HwVertexBuf)
        MakeMeshBuffers(mesh);

    return mesh->HwVertexBuf - 1;
}



void GPUBackend::StartFrame()
{
    if (_frameStarted)
        return;

    _frameStarted = true;

    _layers.clear();
    _sceneBatches.clear();
    _commands.clear();
    _instances.clear();
    _surfaceLayers = 0;

    if (!_cmd)
        _cmd = SDL_AcquireGPUCommandBuffer(_device);
}

void GPUBackend::BeginFrame()
{
    _frameStarted = false;
    StartFrame();
}

void GPUBackend::BeginScene(const mat4x4f &projection)
{
    StartFrame();

    // UAFrustum's x and y, and w = z; the depth reversed, from 1 at the near
    // plane to 0 at the far one
    double n = Engine.GetProjectionNear();
    double f = Engine.GetProjectionFar();

    float rows[4][4] = {
        {projection.m00, projection.m01, projection.m02, projection.m03},
        {projection.m10, projection.m11, projection.m12, projection.m13},
        {0.0, 0.0, (float)(-n / (f - n)), (float)(n * f / (f - n))},
        {projection.m30, projection.m31, projection.m32, projection.m33}
    };

    memcpy(_projection, rows, sizeof(_projection));
}

void GPUBackend::NodeStates(const TRenderNode *nod, uint32_t *pipeline, bool *linear, TGpuInstance *inst) const
{
    // As GLBackend::RenderingMeshOld sets the legacy OpenGL states
    uint32_t flags = nod->Flags;

    int texBlend = 0; // 0 replace, 1 modulate the colors only, 2 modulate
    bool alphaBlend = false;
    SDL_GPUBlendFactor src = SDL_GPU_BLENDFACTOR_ONE;
    SDL_GPUBlendFactor dst = SDL_GPU_BLENDFACTOR_ZERO;
    bool depthTest = true;
    bool zwrite = true;
    bool fog = false;
    bool alphaTest = false;
    bool falloff = false;

    *linear = (Engine._filter != 0);

    bool textured = (flags & RFLAGS_TEXTURED) && nod->Tex && nod->Tex->hwTex;

    if ( flags & RFLAGS_SHADED )
        texBlend = 2;

    if ( flags & RFLAGS_FOG )
        fog = true;

    if ( flags & RFLAGS_LUMTRACY )
    {
        if ( !Engine._zbuf_when_tracy )
            zwrite = false;

        if ( GFXEngine::can_destblend )
        {
            alphaBlend = true;
            texBlend = 1;
            src = SDL_GPU_BLENDFACTOR_ONE;
            dst = SDL_GPU_BLENDFACTOR_ONE;
        }
        else if ( GFXEngine::can_srcblend || GFXEngine::can_stippling )
        {
            alphaBlend = true;
            texBlend = 1;
            src = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            dst = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        }
    }
    else if ( flags & RFLAGS_ZEROTRACY )
    {
        alphaTest = true;
        alphaBlend = true;
        src = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        dst = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        *linear = false;
        texBlend = 2;
    }

    if (flags & RFLAGS_SKY)
    {
        // A backdrop drawn before everything else, never hiding anything
        depthTest = false;
        zwrite = false;
        fog = false;
    }
    else if (flags & RFLAGS_FALLOFF)
    {
        zwrite = true;
        alphaBlend = true;
        texBlend = 2;
        src = SDL_GPU_BLENDFACTOR_SRC_ALPHA;

        if (flags & RFLAGS_LUMTRACY)
            dst = SDL_GPU_BLENDFACTOR_ONE;
        else
            dst = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

        falloff = true;
    }

    TPipelineDesc desc;
    desc.Kind = alphaTest ? PIPE_WORLD_ALPHATEST : PIPE_WORLD;
    desc.DepthTest = depthTest;
    desc.DepthWrite = zwrite;

    if (!alphaBlend || (src == SDL_GPU_BLENDFACTOR_ONE && dst == SDL_GPU_BLENDFACTOR_ZERO))
        desc.Blend = BLEND_NONE;
    else if (src == SDL_GPU_BLENDFACTOR_ONE)
        desc.Blend = BLEND_ADD;
    else if (dst == SDL_GPU_BLENDFACTOR_ONE)
        desc.Blend = BLEND_ALPHA_ADD;
    else
        desc.Blend = BLEND_ALPHA;

    *pipeline = EncodePipeline(desc);

    uint32_t instFlags = 0;

    if (flags & RFLAGS_COMPUTED_COLOR)
        instFlags |= INST_COMPUTED_COLOR;

    if (textured && texBlend == 0)
        instFlags |= INST_REPLACE;
    else if (textured && texBlend == 1)
        instFlags |= INST_ALPHA_FROM_TEX;

    if (falloff)
        instFlags |= INST_FALLOFF;

    inst->Misc[0] = textured ? nod->Tex->hwTex : 0;
    inst->Misc[1] = NO_FRAME_COORDS;
    inst->Misc[2] = instFlags;
    inst->Misc[3] = 0;

    if ( textured && (flags & RFLAGS_DYNAMIC_TEXTURE) && nod->coordsID >= 0
         && nod->coordsID < (int32_t)nod->Mesh->CoordsCache.size() )
        inst->Misc[1] = nod->Mesh->CoordsCache[nod->coordsID].BufferPos;

    // OpenGL's fog with its start at its end fogs all past it
    inst->Fog[0] = nod->FogStart;
    inst->Fog[1] = !fog ? 0.0 : (nod->FogLength > 0.0 ? 1.0 / nod->FogLength : 1e30);
    inst->Fog[2] = _falloffStart;
    inst->Fog[3] = _falloffScale;
}

// Sorts the keys by their bits from lowBit up, keeping the order of equal ones
static void RadixSort(std::vector<GPUBackend::TNodeKey> &keys, std::vector<GPUBackend::TNodeKey> &tmp,
                      int lowBit, int bits)
{
    static uint32_t counts[4096];

    size_t n = keys.size();
    tmp.resize(n);

    for (int shift = lowBit; shift < lowBit + bits; shift += 12)
    {
        GPUBackend::TNodeKey *src = keys.data();
        GPUBackend::TNodeKey *dst = tmp.data();

        memset(counts, 0, sizeof(counts));

        for (size_t i = 0; i < n; i++)
            counts[(src[i].Key >> shift) & 0xFFF]++;

        uint32_t sum = 0;
        for (int i = 0; i < 4096; i++)
        {
            uint32_t c = counts[i];
            counts[i] = sum;
            sum += c;
        }

        for (size_t i = 0; i < n; i++)
            dst[ counts[(src[i].Key >> shift) & 0xFFF]++ ] = src[i];

        keys.swap(tmp);
    }
}

void GPUBackend::QueueNodes(std::list<TRenderNode *> &list, bool keepOrder)
{
    // Each node gets a key of its pipeline and mesh. Sorted, the nodes of a
    // mesh are drawn with one command, the commands of a pipeline with one
    // call. Lists drawn in order only join nodes that come one after another.
    _nodeKeys.clear();
    _slotNodes.clear();
    _slotInstances.clear();

    for (TRenderNode *node : list)
    {
        TMesh *mesh = node->Mesh;
        if (!mesh || mesh->Indixes.empty())
            continue;

        uint32_t meshId = MeshAllocId(mesh);
        uint32_t slot = _slotNodes.size();

        _slotNodes.push_back(node);
        _slotInstances.emplace_back();

        TGpuInstance &inst = _slotInstances.back();
        uint32_t pipeline;
        bool linear;
        NodeStates(node, &pipeline, &linear, &inst);

        const mat4x4 &t = node->TForm;
        inst.Rows[0][0] = t.m00;  inst.Rows[0][1] = t.m01;  inst.Rows[0][2] = t.m02;  inst.Rows[0][3] = t.m03;
        inst.Rows[1][0] = t.m10;  inst.Rows[1][1] = t.m11;  inst.Rows[1][2] = t.m12;  inst.Rows[1][3] = t.m13;
        inst.Rows[2][0] = t.m20;  inst.Rows[2][1] = t.m21;  inst.Rows[2][2] = t.m22;  inst.Rows[2][3] = t.m23;

        uint64_t state = (pipeline << 1) | (linear ? 1 : 0);
        _nodeKeys.push_back({(state << 27) | meshId, slot});

        System::Perf.Count(System::PerfStats::CNT_TRIANGLES, mesh->Indixes.size() / 3);
    }

    list.clear();

    if (!keepOrder)
        RadixSort(_nodeKeys, _nodeKeysTmp, 0, 36);

    size_t n = _nodeKeys.size();
    const TNodeKey *keys = _nodeKeys.data();
    size_t i = 0;

    while (i < n)
    {
        uint64_t state = keys[i].Key >> 27;

        TBatch batch;
        batch.Pipeline = state >> 1;
        batch.Linear = (state & 1) != 0;
        batch.FirstCommand = _commands.size();
        batch.Commands = 0;

        while (i < n && (keys[i].Key >> 27) == state)
        {
            uint64_t key = keys[i].Key;
            size_t end = i + 1;

            while (end < n && keys[end].Key == key)
                end++;

            const TMeshAlloc &alloc = _meshAllocs[key & 0x7FFFFFF];

            SDL_GPUIndexedIndirectDrawCommand cmd;
            cmd.num_indices = _slotNodes[keys[i].Slot]->Mesh->Indixes.size();
            cmd.num_instances = end - i;
            cmd.first_index = alloc.FirstIndex;
            cmd.vertex_offset = alloc.FirstVertex;
            cmd.first_instance = _instances.size();
            _commands.push_back(cmd);
            batch.Commands++;

            for (size_t j = i; j < end; j++)
                _instances.push_back(_slotInstances[keys[j].Slot]);

            i = end;
        }

        _sceneBatches.push_back(batch);
    }
}

void GPUBackend::Rasterize(TRenderQueue &queue, uint32_t passes)
{
    StartFrame();

    _falloffStart = System::IniConf::GfxSkyDistance.Get<int>();
    float length = System::IniConf::GfxSkyLength.Get<int>();
    _falloffScale = length > 0.0 ? 1.0 / length : 1e30;

    TLayer layer;
    layer.Scene = true;
    layer.Index = _sceneBatches.size();
    memcpy(layer.Projection, _projection, sizeof(layer.Projection));

    // The lists in the order and, where it matters, the sorting of the
    // legacy renderer: the sky has no depth test, and the luminous meshes
    // blend over each other from back to front
    if (passes & RASTER_SKY)
    {
        queue.SkyBox.sort(TRenderNode::CompareSolid);
        QueueNodes(queue.SkyBox, true);
    }

    if (passes & RASTER_SOLID)
        QueueNodes(queue.Solid, false);

    if (passes & RASTER_ZEROTR)
        QueueNodes(queue.ZeroTracy, false);

    if (passes & RASTER_LUMATR)
    {
        queue.LumaTracy.sort(TRenderNode::CompareTransparent);
        QueueNodes(queue.LumaTracy, true);
    }

    layer.Batches = _sceneBatches.size() - layer.Index;

    if (layer.Batches)
        _layers.push_back(layer);
}



SDL_GPUTransferBuffer *GPUBackend::UploadBuffer(SDL_GPUTransferBuffer *tb, uint32_t *capacity, uint32_t size)
{
    if (tb && *capacity >= size)
        return tb;

    if (tb)
        SDL_ReleaseGPUTransferBuffer(_device, tb);

    *capacity = std::max(size + size / 2, (uint32_t)65536);

    SDL_GPUTransferBufferCreateInfo ci = {};
    ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ci.size = *capacity;
    return SDL_CreateGPUTransferBuffer(_device, &ci);
}

SDL_GPUBuffer *GPUBackend::GrowBuffer(SDL_GPUCopyPass *pass, SDL_GPUBuffer *buffer, uint32_t *capacity,
                                      uint32_t needed, SDL_GPUBufferUsageFlags usage)
{
    if (needed <= *capacity)
        return buffer;

    SDL_GPUBufferCreateInfo ci = {};
    ci.usage = usage;
    ci.size = std::max(needed, *capacity * 2);

    SDL_GPUBuffer *grown = SDL_CreateGPUBuffer(_device, &ci);

    if (buffer)
    {
        SDL_GPUBufferLocation src = {buffer, 0};
        SDL_GPUBufferLocation dst = {grown, 0};
        SDL_CopyGPUBufferToBuffer(pass, &src, &dst, *capacity, false);
        SDL_ReleaseGPUBuffer(_device, buffer);
    }

    *capacity = ci.size;
    return grown;
}

void GPUBackend::UploadPending(SDL_GPUCommandBuffer *cmd)
{
    if (_pending.empty())
        return;

    // Make room first, in a pass of its own: the copies of the old contents
    // must be done before the new data goes over them
    uint32_t vertexBytes = _vertexRanges.End() * sizeof(TGpuVertex);
    uint32_t indexBytes = _indexRanges.End() * sizeof(IndexType);
    uint32_t coordsBytes = _coordsRanges.End() * sizeof(tUtV);

    if (vertexBytes > _vertexCapacity || indexBytes > _indexCapacity ||
        coordsBytes > _coordsCapacity || _nextLayer > _textureLayers)
    {
        SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);

        _vertexBuffer = GrowBuffer(pass, _vertexBuffer, &_vertexCapacity, vertexBytes, SDL_GPU_BUFFERUSAGE_VERTEX);
        _indexBuffer = GrowBuffer(pass, _indexBuffer, &_indexCapacity, indexBytes, SDL_GPU_BUFFERUSAGE_INDEX);
        _coordsBuffer = GrowBuffer(pass, _coordsBuffer, &_coordsCapacity, coordsBytes, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);

        if (_nextLayer > _textureLayers)
        {
            uint32_t layers = std::max(_nextLayer, _textureLayers * 2);

            SDL_GPUTextureCreateInfo tci = {};
            tci.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
            tci.format = _textureFormat;
            tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            tci.width = TEXTURE_SIZE;
            tci.height = TEXTURE_SIZE;
            tci.layer_count_or_depth = layers;
            tci.num_levels = 1;
            SDL_GPUTexture *grown = SDL_CreateGPUTexture(_device, &tci);

            for (uint32_t l = 0; l < _textureLayers; l++)
            {
                SDL_GPUTextureLocation src = {_textures, 0, l, 0, 0, 0};
                SDL_GPUTextureLocation dst = {grown, 0, l, 0, 0, 0};
                SDL_CopyGPUTextureToTexture(pass, &src, &dst, TEXTURE_SIZE, TEXTURE_SIZE, 1, false);
            }

            SDL_ReleaseGPUTexture(_device, _textures);
            _textures = grown;
            _textureLayers = layers;
        }

        SDL_EndGPUCopyPass(pass);
    }

    _pendingTransfer = UploadBuffer(_pendingTransfer, &_pendingTransferSize, _staging.size());

    void *mapped = SDL_MapGPUTransferBuffer(_device, _pendingTransfer, true);
    memcpy(mapped, _staging.data(), _staging.size());
    SDL_UnmapGPUTransferBuffer(_device, _pendingTransfer);

    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);

    for (const TPendingUpload &p : _pending)
    {
        if (p.To == TPendingUpload::TEXTURE_LAYER)
        {
            SDL_GPUTextureTransferInfo src = {_pendingTransfer, p.SrcOffset, TEXTURE_SIZE, TEXTURE_SIZE};
            SDL_GPUTextureRegion dst = {};
            dst.texture = _textures;
            dst.layer = p.DstOffset;
            dst.w = TEXTURE_SIZE;
            dst.h = TEXTURE_SIZE;
            dst.d = 1;
            SDL_UploadToGPUTexture(pass, &src, &dst, false);
            continue;
        }

        SDL_GPUBuffer *buffer;
        switch (p.To)
        {
            case TPendingUpload::VERTICES: buffer = _vertexBuffer; break;
            case TPendingUpload::INDICES:  buffer = _indexBuffer;  break;
            case TPendingUpload::COORDS:   buffer = _coordsBuffer; break;
            default:                       buffer = _quadBuffer;   break;
        }

        SDL_GPUTransferBufferLocation src = {_pendingTransfer, p.SrcOffset};
        SDL_GPUBufferRegion dst = {buffer, p.DstOffset, p.Size};
        SDL_UploadToGPUBuffer(pass, &src, &dst, false);
    }

    SDL_EndGPUCopyPass(pass);

    _pending.clear();
    _staging.clear();
}

void GPUBackend::UploadFrameData(SDL_GPUCopyPass *pass)
{
    uint32_t instanceBytes = _instances.size() * sizeof(TGpuInstance);
    uint32_t commandBytes = _commands.size() * sizeof(SDL_GPUIndexedIndirectDrawCommand);

    if (!instanceBytes)
        return;

    // The last frame may still be drawing from them: new ones need no copy
    if (instanceBytes > _instanceCapacity)
    {
        if (_instanceBuffer)
            SDL_ReleaseGPUBuffer(_device, _instanceBuffer);

        _instanceCapacity = std::max(instanceBytes + instanceBytes / 2, (uint32_t)65536);

        SDL_GPUBufferCreateInfo ci = {};
        ci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        ci.size = _instanceCapacity;
        _instanceBuffer = SDL_CreateGPUBuffer(_device, &ci);
    }

    if (commandBytes > _commandCapacity)
    {
        if (_commandBuffer)
            SDL_ReleaseGPUBuffer(_device, _commandBuffer);

        _commandCapacity = std::max(commandBytes + commandBytes / 2, (uint32_t)16384);

        SDL_GPUBufferCreateInfo ci = {};
        ci.usage = SDL_GPU_BUFFERUSAGE_INDIRECT;
        ci.size = _commandCapacity;
        _commandBuffer = SDL_CreateGPUBuffer(_device, &ci);
    }

    _frameTransfer = UploadBuffer(_frameTransfer, &_frameTransferSize, instanceBytes + commandBytes);

    uint8_t *mapped = (uint8_t *)SDL_MapGPUTransferBuffer(_device, _frameTransfer, true);
    memcpy(mapped, _instances.data(), instanceBytes);
    memcpy(mapped + instanceBytes, _commands.data(), commandBytes);
    SDL_UnmapGPUTransferBuffer(_device, _frameTransfer);

    SDL_GPUTransferBufferLocation src = {_frameTransfer, 0};
    SDL_GPUBufferRegion dst = {_instanceBuffer, 0, instanceBytes};
    SDL_UploadToGPUBuffer(pass, &src, &dst, true);

    src.offset = instanceBytes;
    dst = {_commandBuffer, 0, commandBytes};
    SDL_UploadToGPUBuffer(pass, &src, &dst, true);
}

void GPUBackend::ScreenSurfaceChanged(SDL_Surface *screen)
{
    for (SDL_GPUTexture *t : _surfaceTextures)
        SDL_ReleaseGPUTexture(_device, t);

    _surfaceTextures.clear();

    // Should the size change in the middle of a frame, what was flushed of
    // the old surface is not drawn
    for (size_t i = 0; i < _layers.size(); )
    {
        if (!_layers[i].Scene)
            _layers.erase(_layers.begin() + i);
        else
            i++;
    }

    _surfaceLayers = 0;

    if (_surfaceTransfer)
        SDL_ReleaseGPUTransferBuffer(_device, _surfaceTransfer);

    _surfaceW = screen->w;
    _surfaceH = screen->h;

    SDL_GPUTransferBufferCreateInfo ci = {};
    ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ci.size = _surfaceW * _surfaceH * 4;
    _surfaceTransfer = SDL_CreateGPUTransferBuffer(_device, &ci);
}

void GPUBackend::UploadScreenSurface(SDL_GPUCopyPass *pass, SDL_Surface *screen, uint32_t texture)
{
    while (_surfaceTextures.size() <= texture)
    {
        SDL_GPUTextureCreateInfo tci = {};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = _textureFormat;
        tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = _surfaceW;
        tci.height = _surfaceH;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        _surfaceTextures.push_back(SDL_CreateGPUTexture(_device, &tci));
    }

    uint32_t rowSize = _surfaceW * 4;
    uint8_t *mapped = (uint8_t *)SDL_MapGPUTransferBuffer(_device, _surfaceTransfer, true);

    if ((uint32_t)screen->pitch == rowSize)
        memcpy(mapped, screen->pixels, rowSize * _surfaceH);
    else
    {
        for (uint32_t y = 0; y < _surfaceH; y++)
            memcpy(mapped + y * rowSize, (uint8_t *)screen->pixels + y * screen->pitch, rowSize);
    }

    SDL_UnmapGPUTransferBuffer(_device, _surfaceTransfer);

    SDL_GPUTextureTransferInfo src = {_surfaceTransfer, 0, _surfaceW, _surfaceH};
    SDL_GPUTextureRegion dst = {};
    dst.texture = _surfaceTextures[texture];
    dst.w = _surfaceW;
    dst.h = _surfaceH;
    dst.d = 1;
    SDL_UploadToGPUTexture(pass, &src, &dst, true);
}

void GPUBackend::AddScreenSurfaceLayer(SDL_GPUCopyPass *pass, SDL_Surface *screen)
{
    TLayer layer;
    layer.Scene = false;
    layer.Index = _surfaceLayers++;
    layer.Batches = 0;

    UploadScreenSurface(pass, screen, layer.Index);
    _layers.push_back(layer);
}

void GPUBackend::FlushScreenSurface(SDL_Surface *screen)
{
    StartFrame();

    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(_cmd);
    AddScreenSurfaceLayer(pass, screen);
    SDL_EndGPUCopyPass(pass);
}

void GPUBackend::EnsureDepth(uint32_t w, uint32_t h)
{
    if (_depth && _depthW == w && _depthH == h)
        return;

    if (_depth)
        SDL_ReleaseGPUTexture(_device, _depth);

    SDL_GPUTextureCreateInfo tci = {};
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = DEPTH_FORMAT;
    tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    tci.width = w;
    tci.height = h;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    _depth = SDL_CreateGPUTexture(_device, &tci);
    _depthW = w;
    _depthH = h;
}

uint64_t GPUBackend::EndFrame(SDL_Surface *screen)
{
    StartFrame();

    UploadPending(_cmd);

    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(_cmd);
    UploadFrameData(pass);
    AddScreenSurfaceLayer(pass, screen);
    SDL_EndGPUCopyPass(pass);

    SDL_GPUTexture *swapchain = NULL;
    Uint32 w = 0, h = 0;

    // Blocks with vsync, and without it too when the window has no free
    // image, as macOS's run out at high frame rates
    uint64_t waitStart = System::PerfStats::Now();

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(_cmd, _window, &swapchain, &w, &h))
        printf("SDL_GPU: no swapchain texture: %s\n", SDL_GetError());

    uint64_t waited = System::PerfStats::Now() - waitStart;

    // None while the window is hidden
    if (swapchain)
    {
        EnsureDepth(w, h);
        DrawLayers(_cmd, swapchain, true);
    }

    SDL_SubmitGPUCommandBuffer(_cmd);
    _cmd = NULL;
    _frameStarted = false;

    return waited;
}

void GPUBackend::EnsureEffectsTarget(uint32_t w, uint32_t h)
{
    if (_effectsTarget && _effectsW == w && _effectsH == h)
        return;

    if (_effectsTarget)
        SDL_ReleaseGPUTexture(_device, _effectsTarget);

    // Floating point like the OpenGL renderer's, so that blending is not
    // clamped before the effects
    SDL_GPUTextureCreateInfo tci = {};
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.width = w;
    tci.height = h;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    _effectsTarget = SDL_CreateGPUTexture(_device, &tci);
    _effectsW = w;
    _effectsH = h;
}

void GPUBackend::DrawColorEffects(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd)
{
    const vec3d &norm = Engine.GetColorEffectNorm();
    const vec3d &inv = Engine.GetColorEffectInv();
    float uniforms[8] = {(float)norm.x, (float)norm.y, (float)norm.z, 0.0,
                         (float)inv.x, (float)inv.y, (float)inv.z, 0.0};
    SDL_PushGPUFragmentUniformData(cmd, 0, uniforms, sizeof(uniforms));

    TPipelineDesc desc;
    desc.Kind = PIPE_COLOR_EFFECTS;
    desc.Blend = _colorEffectsBlending == 1 ? BLEND_ALPHA : BLEND_PREMULTIPLIED;
    desc.DepthTest = false;
    desc.DepthWrite = false;
    DrawQuad(pass, cmd, EncodePipeline(desc), _effectsTarget, 1.0, 1.0);
}

void GPUBackend::DrawLayers(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, bool count)
{
    SDL_GPUDepthStencilTargetInfo depth = {};
    depth.texture = _depth;
    depth.clear_depth = 0.0; // The far plane: the depth is reversed
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    TPipelineDesc surface;
    surface.Kind = PIPE_QUAD;
    surface.Blend = BLEND_ALPHA;
    surface.DepthTest = false;
    surface.DepthWrite = false;
    uint32_t surfacePipeline = EncodePipeline(surface);

    if (_colorEffects)
    {
        // The scenes first, into the texture the effects take. With
        // gfx.color_effects 1 the frame's own surface, the last, goes there
        // too, over the scenes; the surfaces flushed before it never do.
        EnsureEffectsTarget(_depthW, _depthH);

        SDL_GPUColorTargetInfo scene = {};
        scene.texture = _effectsTarget;
        scene.clear_color = {0.0, 0.0, 0.0, 0.0};
        scene.load_op = SDL_GPU_LOADOP_CLEAR;
        scene.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &scene, 1, &depth);

        for (const TLayer &layer : _layers)
        {
            if (layer.Scene)
                DrawScene(pass, cmd, layer, true, count);
        }

        if (_colorEffects == 1 && _surfaceLayers)
        {
            surface.FloatTarget = true;
            DrawQuad(pass, cmd, EncodePipeline(surface), _surfaceTextures[_surfaceLayers - 1], 1.0, 1.0);
        }

        SDL_EndGPURenderPass(pass);

        depth.load_op = SDL_GPU_LOADOP_DONT_CARE;
    }

    SDL_GPUColorTargetInfo color = {};
    color.texture = target;
    color.clear_color = {0.0, 0.0, 0.0, 1.0};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, &depth);

    bool effectsDrawn = false;

    for (const TLayer &layer : _layers)
    {
        if (layer.Scene)
        {
            if (!_colorEffects)
                DrawScene(pass, cmd, layer, false, count);

            continue;
        }

        // The frame's own surface, the last, goes over the processed scene
        // (in the scene with gfx.color_effects 1)
        if (_colorEffects && layer.Index + 1 == _surfaceLayers)
        {
            DrawColorEffects(pass, cmd);
            effectsDrawn = true;

            if (_colorEffects == 1)
                continue;
        }

        DrawQuad(pass, cmd, surfacePipeline, _surfaceTextures[layer.Index], 1.0, 1.0);
    }

    if (_colorEffects && !effectsDrawn)
        DrawColorEffects(pass, cmd);

    SDL_EndGPURenderPass(pass);
}

void GPUBackend::DrawScene(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd, const TLayer &layer,
                           bool floatTarget, bool count)
{
    SDL_PushGPUVertexUniformData(cmd, 0, layer.Projection, sizeof(layer.Projection));

    SDL_GPUBufferBinding vertexBuffers[2] = { {_vertexBuffer, 0}, {_instanceBuffer, 0} };
    SDL_BindGPUVertexBuffers(pass, 0, vertexBuffers, 2);

    SDL_GPUBufferBinding indexBuffer = {_indexBuffer, 0};
    SDL_BindGPUIndexBuffer(pass, &indexBuffer, sizeof(IndexType) == 2 ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_BindGPUVertexStorageBuffers(pass, 0, &_coordsBuffer, 1);

    SDL_GPUGraphicsPipeline *bound = NULL;
    int boundSampler = -1;

    for (uint32_t b = layer.Index; b < layer.Index + layer.Batches; b++)
    {
        const TBatch &batch = _sceneBatches[b];

        SDL_GPUGraphicsPipeline *pipeline = GetPipeline(batch.Pipeline | (floatTarget ? 0x100 : 0));
        if (!pipeline)
            continue;

        if (pipeline != bound)
        {
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            bound = pipeline;
        }

        if ((int)batch.Linear != boundSampler)
        {
            SDL_GPUTextureSamplerBinding binding = {_textures, batch.Linear ? _linearSampler : _nearestSampler};
            SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
            boundSampler = batch.Linear;

            if (count)
                System::Perf.Count(System::PerfStats::CNT_TEXCHANGES);
        }

        SDL_DrawGPUIndexedPrimitivesIndirect(pass, _commandBuffer,
                                             batch.FirstCommand * sizeof(SDL_GPUIndexedIndirectDrawCommand),
                                             batch.Commands);

        if (count)
            System::Perf.Count(System::PerfStats::CNT_DRAWCALLS);
    }
}

void GPUBackend::DrawQuad(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd, uint32_t pipelineKey,
                          SDL_GPUTexture *texture, float scaleX, float scaleY)
{
    SDL_GPUGraphicsPipeline *pipeline = GetPipeline(pipelineKey);
    if (!pipeline)
        return;

    SDL_BindGPUGraphicsPipeline(pass, pipeline);

    float scale[4] = {scaleX, scaleY, 0.0, 0.0};
    SDL_PushGPUVertexUniformData(cmd, 0, scale, sizeof(scale));

    SDL_GPUBufferBinding corners = {_quadBuffer, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &corners, 1);

    SDL_GPUTextureSamplerBinding binding = {texture, _linearSampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
}

void GPUBackend::SetColorEffectsBlending(int mode)
{
    _colorEffectsBlending = mode;
}

uint8_t *GPUBackend::ReadScreen(int *ow, int *oh)
{
    // The window's textures cannot be read, so the last frame is drawn again
    // into one that can
    if (!_depth)
        return NULL;

    uint32_t w = _depthW & ~1;
    uint32_t h = _depthH & ~1;

    SDL_GPUTextureCreateInfo tci = {};
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = _swapchainFormat;
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tci.width = _depthW;
    tci.height = _depthH;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    SDL_GPUTexture *target = SDL_CreateGPUTexture(_device, &tci);

    SDL_GPUTransferBufferCreateInfo bci = {};
    bci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    bci.size = w * h * 4;
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(_device, &bci);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(_device);
    DrawLayers(cmd, target, false);

    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion src = {};
    src.texture = target;
    src.w = w;
    src.h = h;
    src.d = 1;
    SDL_GPUTextureTransferInfo dst = {download, 0, w, h};
    SDL_DownloadFromGPUTexture(pass, &src, &dst);
    SDL_EndGPUCopyPass(pass);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(_device, true, &fence, 1);
    SDL_ReleaseGPUFence(_device, fence);

    // RGB with the bottom line first, as OpenGL reads it
    uint8_t *buf = (uint8_t *)malloc(w * h * 3);
    const uint8_t *pixels = (const uint8_t *)SDL_MapGPUTransferBuffer(_device, download, false);
    bool bgra = (_swapchainFormat == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);

    for (uint32_t y = 0; y < h; y++)
    {
        const uint8_t *in = pixels + (h - 1 - y) * w * 4;
        uint8_t *out = buf + y * w * 3;

        for (uint32_t x = 0; x < w; x++)
        {
            out[x * 3 + 0] = in[x * 4 + (bgra ? 2 : 0)];
            out[x * 3 + 1] = in[x * 4 + 1];
            out[x * 3 + 2] = in[x * 4 + (bgra ? 0 : 2)];
        }
    }

    SDL_UnmapGPUTransferBuffer(_device, download);
    SDL_ReleaseGPUTransferBuffer(_device, download);
    SDL_ReleaseGPUTexture(_device, target);

    *ow = w;
    *oh = h;
    return buf;
}

void GPUBackend::DrawVideoFrame(const uint8_t *rgb, int width, int height)
{
    if (!_videoTexture || width != _videoW || height != _videoH)
    {
        EndVideo();

        SDL_GPUTextureCreateInfo tci = {};
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = width;
        tci.height = height;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        _videoTexture = SDL_CreateGPUTexture(_device, &tci);

        SDL_GPUTransferBufferCreateInfo bci = {};
        bci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        bci.size = width * height * 4;
        _videoTransfer = SDL_CreateGPUTransferBuffer(_device, &bci);

        _videoW = width;
        _videoH = height;
    }

    uint8_t *rgba = (uint8_t *)SDL_MapGPUTransferBuffer(_device, _videoTransfer, true);
    for (int i = 0; i < width * height; i++)
    {
        rgba[i * 4 + 0] = rgb[i * 3 + 0];
        rgba[i * 4 + 1] = rgb[i * 3 + 1];
        rgba[i * 4 + 2] = rgb[i * 3 + 2];
        rgba[i * 4 + 3] = 255;
    }
    SDL_UnmapGPUTransferBuffer(_device, _videoTransfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(_device);

    // The quad's corners may not be there yet: movies come before the first frame
    UploadPending(cmd);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo src = {_videoTransfer, 0, (Uint32)width, (Uint32)height};
    SDL_GPUTextureRegion dst = {};
    dst.texture = _videoTexture;
    dst.w = width;
    dst.h = height;
    dst.d = 1;
    SDL_UploadToGPUTexture(copy, &src, &dst, true);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUTexture *swapchain = NULL;
    Uint32 w = 0, h = 0;

    if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, _window, &swapchain, &w, &h) && swapchain)
    {
        EnsureDepth(w, h);

        SDL_GPUColorTargetInfo color = {};
        color.texture = swapchain;
        color.clear_color = {0.0, 0.0, 0.0, 1.0};
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depth = {};
        depth.texture = _depth;
        depth.load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, &depth);

        // Letterboxed, keeping the movie's aspect
        double scrAspect = (double)w / (double)h;
        double frmAspect = (double)width / (double)height;
        double qW = 1.0;
        double qH = 1.0;
        if (scrAspect > frmAspect)
            qW = frmAspect / scrAspect;
        else if (scrAspect < frmAspect)
            qH = scrAspect / frmAspect;

        TPipelineDesc desc;
        desc.Kind = PIPE_QUAD;
        desc.Blend = BLEND_NONE;
        desc.DepthTest = false;
        desc.DepthWrite = false;
        DrawQuad(pass, cmd, EncodePipeline(desc), _videoTexture, qW, qH);

        SDL_EndGPURenderPass(pass);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
}

void GPUBackend::EndVideo()
{
    if (_videoTexture)
        SDL_ReleaseGPUTexture(_device, _videoTexture);

    if (_videoTransfer)
        SDL_ReleaseGPUTransferBuffer(_device, _videoTransfer);

    _videoTexture = NULL;
    _videoTransfer = NULL;
    _videoW = 0;
    _videoH = 0;
}

}
