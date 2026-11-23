// Shared between the shaders and gpu_backend.cpp, which includes this file
// too: keep it to plain constants.

// Instance flags of the world shader
static const uint INST_COMPUTED_COLOR = 1; // Use the vertex's computed color
static const uint INST_REPLACE        = 2; // The texture replaces the color
static const uint INST_ALPHA_FROM_TEX = 4; // Color modulated, alpha from the texture
static const uint INST_FALLOFF        = 8; // Fade out with distance (gfx.newsky)
static const uint INST_WRAP           = 16; // The texture repeats: the force field
