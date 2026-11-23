// The force field of the map's border and of open beam gates, drawn by the
// SDL_GPU backend into a 256 x 256 layer of the texture array every frame, in
// place of the original's animation: a small patch of FX2.ILBM shifted around
// at 25 frames per second, which showed as streaks on the tall walls.
//
// Here a dim, mottled veil with faint facets has streaks of light rising
// through it. The texture tiles seamlessly, and the animation repeats every
// LOOP seconds without a jump: everything that moves does so by whole tiles
// or whole turns per loop. The features are soft, as the texture has no
// mipmaps: fine lines would flicker in the distance. See world.slang for the
// binding rules.

struct ForceFieldUniforms
{
    float4 params; // x: time in seconds, modulo LOOP; y: 1 to premultiply the
                   // colors by the alpha, for additive blending
};

[[vk::binding(0, 3)]] ConstantBuffer<ForceFieldUniforms> forceField : register(b0, space3);

static const float LOOP = 64.0;
static const float TAU = 6.2831853;

uint HashU(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// Two numbers from 0 to 1 for a lattice point, the lattice wrapping around
// every n points so that the pattern tiles
float2 Hash2(int2 p, int2 n)
{
    int2 w = ((p % n) + n) % n;
    uint h = HashU(uint(w.x) * 1597334677u ^ uint(w.y) * 3812015801u ^ uint(n.x * 31 + n.y) * 2654435761u);
    return float2(float(h & 0xFFFFu), float(h >> 16)) / 65535.0;
}

// Smooth noise from 0 to 1, with n.x by n.y lattice cells per tile
float Noise(float2 uv, int2 n)
{
    float2 p = uv * float2(n);
    int2 i = int2(floor(p));
    float2 f = p - floor(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = Hash2(i, n).x;
    float b = Hash2(i + int2(1, 0), n).x;
    float c = Hash2(i + int2(0, 1), n).x;
    float d = Hash2(i + int2(1, 1), n).x;
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// The random brightness of the cell nearest to uv, among n x n cells per
// tile whose points circle within them: the original's faint shards
float Facets(float2 uv, int n, float t, int turns)
{
    float2 p = uv * n;
    int2 cell = int2(floor(p));
    float2 f = p - floor(p);

    float nearest = 8.0;
    float id = 0.0;

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float2 h = Hash2(cell + int2(x, y), int2(n, n));
            float angle = TAU * (h.x + (t / LOOP) * turns * (h.y < 0.5 ? 1.0 : -1.0));
            float2 point = float2(x, y) + 0.5 + 0.3 * float2(cos(angle), sin(angle));
            float d = length(point - f);

            if (d < nearest)
            {
                nearest = d;
                id = h.y;
            }
        }
    }

    return id;
}

[shader("fragment")]
float4 fsForceField(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float s = forceField.params.x / LOOP; // From 0 to 1 over the loop

    // A slow warp, so that nothing moves quite in a straight line
    float2 warp = float2(Noise(uv + float2(s * 2.0, 0.0), int2(3, 3)),
                         Noise(uv + float2(0.0, s * 3.0), int2(3, 3))) - 0.5;
    float2 p = uv + 0.12 * warp;

    // Mottled patches drifting upwards, longer than they are wide
    float mottle = 0.50 * Noise(p + float2(0.0, s * 2.0), int2(5, 2))
                 + 0.30 * Noise(p + float2(s, s * 4.0), int2(10, 4))
                 + 0.20 * Noise(p + float2(0.0, s * 6.0), int2(20, 6));

    // Streaks rising faster through them: more noise cells across than up
    float streaks = 0.6 * Noise(p + float2(0.0, s * 8.0), int2(14, 3))
                  + 0.4 * Noise(p + float2(0.0, s * 13.0), int2(24, 4));
    streaks = smoothstep(0.35, 0.9, streaks);

    // Faint shards, each pulsing at its own phase
    float shard = Facets(p, 5, forceField.params.x, 2);
    shard *= 0.5 + 0.5 * sin(TAU * (shard * 5.0 + s * 20.0));

    float light = saturate(0.65 * mottle + 0.35 * streaks + 0.15 * shard);

    // From deep blue through the original's lavender to a pale cyan
    float3 deep = float3(0.14, 0.22, 0.72);
    float3 lavender = float3(0.48, 0.52, 0.98);
    float3 cyan = float3(0.60, 0.90, 1.00);
    float3 rgb = lerp(deep, lavender, saturate(light * 1.3));
    rgb = lerp(rgb, cyan, streaks * 0.5);

    // About as see-through as the original
    float alpha = saturate(0.16 + 0.28 * light + 0.08 * streaks);

    if (forceField.params.y > 0.5)
        return float4(rgb * alpha, 1.0);

    return float4(rgb, alpha);
}
