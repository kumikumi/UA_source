// The 3D world, drawn by the SDL_GPU backend. Each render node is one
// instance: the vertex shader gets the mesh's vertices and the node's
// transform, texture and fog as instance data.
//
// Resources follow SDL_GPU's binding rules: [[vk::binding]] for SPIR-V
// (vertex shaders use sets 0 and 1, fragment shaders 2 and 3) and register()
// for Metal, whose [[buffer]] slots take the uniform buffers first, then the
// storage buffers.

#include "common.slang"

struct FrameUniforms
{
    // The projection's rows
    float4 proj0;
    float4 proj1;
    float4 proj2;
    float4 proj3;
};

[[vk::binding(0, 1)]] ConstantBuffer<FrameUniforms> frame : register(b0, space1);

// Texture coordinates of animated textures, one per vertex for each frame
[[vk::binding(0, 0)]] StructuredBuffer<float2> frameCoords : register(t1, space0);

[[vk::binding(0, 2)]] Sampler2DArray textures : register(t0, space2);

struct VertexIn
{
    // Per vertex
    float3 pos           : POSITION;
    float2 uv            : TEXCOORD0;
    float4 color         : COLOR0;
    float4 computedColor : COLOR1;
    uint   index         : BLENDINDICES0; // In its mesh

    // Per instance: the rows of the transform to view space, ...
    float4 row0 : TEXCOORD1;
    float4 row1 : TEXCOORD2;
    float4 row2 : TEXCOORD3;

    // ... the fog start and 1 / length (0: none), the fall-off start and
    // 1 / length, ...
    float4 fog : TEXCOORD4;

    // ... and the texture layer, where frameCoords has the coordinates of the
    // animation frame (NO_FRAME_COORDS: the vertex's own) and the INST flags.
    uint4 misc : TEXCOORD5;
};

static const uint NO_FRAME_COORDS = 0xFFFFFFFF;

struct VertexOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float3 viewPos : TEXCOORD1;
    nointerpolation float4 fog : TEXCOORD2;
    nointerpolation uint layer : TEXCOORD3;
    nointerpolation uint flags : TEXCOORD4;
};

[shader("vertex")]
VertexOut vsWorld(VertexIn i)
{
    float4 p = float4(i.pos, 1.0);
    float4 view = float4(dot(i.row0, p), dot(i.row1, p), dot(i.row2, p), 1.0);

    VertexOut o;
    o.pos = float4(dot(frame.proj0, view), dot(frame.proj1, view),
                   dot(frame.proj2, view), dot(frame.proj3, view));
    o.uv = i.misc.y != NO_FRAME_COORDS ? frameCoords[i.misc.y + i.index] : i.uv;
    o.color = (i.misc.z & INST_COMPUTED_COLOR) != 0 ? i.computedColor : i.color;
    o.viewPos = view.xyz;
    o.fog = i.fog;
    o.layer = i.misc.x;
    o.flags = i.misc.z;
    return o;
}

// Combines the texture and the vertex color like the legacy OpenGL texture
// environment, then fogs towards black and fades out with distance.
float4 Shade(VertexOut i)
{
    float4 tex = textures.Sample(float3(i.uv, float(i.layer)));
    float4 c;

    if ((i.flags & INST_REPLACE) != 0)
        c = tex;
    else if ((i.flags & INST_ALPHA_FROM_TEX) != 0)
        c = float4(tex.rgb * i.color.rgb, tex.a);
    else
        c = tex * i.color;

    // Linear fog by the distance along the view
    c.rgb *= 1.0 - saturate((i.viewPos.z - i.fog.x) * i.fog.y);

    // gfx.newsky: far objects fade out by their distance over the ground
    if ((i.flags & INST_FALLOFF) != 0)
        c.a *= 1.0 - saturate((length(i.viewPos.xz) - i.fog.z) * i.fog.w);

    return c;
}

[shader("fragment")]
float4 fsWorld(VertexOut i) : SV_Target
{
    return Shade(i);
}

// For textures with see-through texels: those are left out entirely.
[shader("fragment")]
float4 fsWorldAlphaTest(VertexOut i) : SV_Target
{
    float4 c = Shade(i);
    if (c.a <= 0.0)
        discard;
    return c;
}
