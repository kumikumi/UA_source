#ifndef ENGINE_GPU_SHADERS_H_INCLUDED
#define ENGINE_GPU_SHADERS_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

namespace GFX
{

// A shader entry point of src/shaders, compiled when the game is built
struct TGpuShaderCode
{
    const char *Name;      // The entry point, such as "vsWorld"
    const uint8_t *Msl;    // Metal Shading Language source
    size_t MslSize;
    const uint8_t *Spirv;  // SPIR-V for Vulkan
    size_t SpirvSize;
};

extern const TGpuShaderCode GpuShaderCode[];
extern const size_t GpuShaderCodeCount;

}

#endif // ENGINE_GPU_SHADERS_H_INCLUDED
