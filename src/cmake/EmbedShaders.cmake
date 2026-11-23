# Writes the compiled shaders into a C++ source file as byte arrays.
#
#   cmake -DOUTPUT=<file.cpp> -DSHADER_DIR=<dir> -DENTRIES=<a,b,...> -P EmbedShaders.cmake
#
# For each entry point, SHADER_DIR has <entry>.metal (MSL source) and
# <entry>.spv (SPIR-V).

string(REPLACE "," ";" ENTRIES "${ENTRIES}")

set(arrays "")
set(table "")

foreach(entry ${ENTRIES})
    foreach(ext metal spv)
        file(READ "${SHADER_DIR}/${entry}.${ext}" hex HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
        # Wrap the lines every 32 bytes
        string(REGEX REPLACE "((0x..,){32})" "\\1\n    " bytes "${bytes}")
        string(APPEND arrays "static const uint8_t ${entry}_${ext}[] =\n{\n    ${bytes}\n};\n\n")
    endforeach()
    string(APPEND table "    {\"${entry}\", ${entry}_metal, sizeof(${entry}_metal), ${entry}_spv, sizeof(${entry}_spv)},\n")
endforeach()

file(WRITE "${OUTPUT}.tmp"
"// Generated from src/shaders by cmake/EmbedShaders.cmake: do not edit.

#include \"system/gpu_shaders.h\"

namespace GFX
{

${arrays}const TGpuShaderCode GpuShaderCode[] =
{
${table}};

const size_t GpuShaderCodeCount = sizeof(GpuShaderCode) / sizeof(GpuShaderCode[0]);

}
")

# Only touch the output when it changed, so that nothing rebuilds needlessly
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OUTPUT}.tmp" "${OUTPUT}")
file(REMOVE "${OUTPUT}.tmp")
