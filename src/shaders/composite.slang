// Textured quads over the frame, drawn by the SDL_GPU backend: the screen
// surface the 2D interface is drawn into, movie frames and the color effects
// pass. See world.slang for the binding rules.

struct QuadUniforms
{
    float4 scale; // xy: half the quad's size in normalized device coordinates
};

struct EffectUniforms
{
    float4 norm; // Multiplier for the colors
    float4 inv;  // Multiplier for their inverses
};

[[vk::binding(0, 1)]] ConstantBuffer<QuadUniforms> quad : register(b0, space1);
[[vk::binding(0, 2)]] Sampler2D image : register(t0, space2);
[[vk::binding(0, 3)]] ConstantBuffer<EffectUniforms> effect : register(b0, space3);

struct QuadOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// A triangle strip of four vertices, centered on the screen. The vertices
// are the texture coordinates of the corners, top left first. (Not from
// SV_VertexID: for SPIR-V, Slang computes it with an optional Vulkan feature.)
[shader("vertex")]
QuadOut vsQuad(float2 uv : TEXCOORD0)
{
    QuadOut o;
    o.pos = float4((uv.x * 2.0 - 1.0) * quad.scale.x, (1.0 - uv.y * 2.0) * quad.scale.y, 0.0, 1.0);
    o.uv = uv;
    return o;
}

[shader("fragment")]
float4 fsQuad(QuadOut i) : SV_Target
{
    return image.Sample(i.uv);
}

[shader("fragment")]
float4 fsColorEffects(QuadOut i) : SV_Target
{
    float4 c = image.Sample(i.uv);
    return float4(c.rgb * effect.norm.rgb + (1.0 - c.rgb) * effect.inv.rgb, c.a);
}
