// The force field of the map's border and of open beam gates, drawn by the
// SDL_GPU backend into a 256 x 256 layer of the texture array every frame, in
// place of the original's animation: a small patch of FX2.ILBM shifted around
// at 25 frames per second. The walls stretched the patch over each of their
// polygons, 150 units wide and 900 tall, so its texels showed as thin
// streaks, 2 units wide and 11 tall.
//
// Here a dim, mottled veil with faint facets has streaks of light rising
// through it, in the original's colors and about as see-through. The texture
// tiles seamlessly, and the animation repeats every LOOP seconds without a
// jump: everything that moves does so by whole tiles or whole turns per loop.
// The features are smooth, as the texture has no mipmaps, and come down to
// about 20 units across, a little coarser than the original's: finer ones
// would sparkle in the distance. See world.slang for the binding rules.

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

// The random brightness of the cell nearest to uv, among n.x by n.y cells
// per tile whose points circle within them: the original's shards, stretched
// like the lattice
float Facets(float2 uv, int2 n, float t, int turns)
{
    float2 p = uv * float2(n);
    int2 cell = int2(floor(p));
    float2 f = p - floor(p);

    float nearest = 8.0;
    float id = 0.0;

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float2 h = Hash2(cell + int2(x, y), n);
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
    float2 warp = float2(Noise(uv + float2(s * 2.0, 0.0), int2(3, 8)),
                         Noise(uv + float2(0.0, s * 3.0), int2(3, 8))) - 0.5;
    float2 p = uv + 0.03 * warp;

    // Mottled patches drifting upwards, longer than they are wide
    float mottle = 0.15 * Noise(p + float2(0.0, s * 2.0), int2(8, 4))
                 + 0.35 * Noise(p + float2(s, s * 4.0), int2(16, 8))
                 + 0.50 * Noise(p + float2(0.0, s * 6.0), int2(28, 12));

    // Streaks rising faster through them: more noise cells across than up,
    // though enough up that the tile's repeats up a wall don't line them up
    // into stripes
    float streaks = 0.5 * Noise(p + float2(0.0, s * 8.0), int2(24, 8))
                  + 0.5 * Noise(p + float2(0.0, s * 13.0), int2(36, 10));

    // Faint shards, each pulsing at its own phase
    float shard = Facets(p, int2(24, 8), forceField.params.x, 2);
    shard *= 0.5 + 0.5 * sin(TAU * (shard * 5.0 + s * 20.0));

    // The original's palette is pure blue, azure and cyan, at alphas of 0.10,
    // 0.20 and 0.16, and one in eight of its texels is clear. The veil goes
    // from blue to azure and the glow to cyan; the veil's thinnest parts are
    // holes. The thresholds give about the original's share of each, and so
    // its average color and alpha.
    float veil = smoothstep(0.39, 0.60, mottle);
    float glow = saturate(smoothstep(0.53, 0.74, streaks) + 0.2 * shard);
    float hole = smoothstep(0.29, 0.41, mottle);

    float3 rgb = lerp(float3(0.0, 0.0, 1.0), float3(0.26, 0.44, 1.0), veil);
    rgb = lerp(rgb, float3(0.51, 0.93, 1.0), glow);
    float alpha = lerp(lerp(0.10, 0.20, veil), 0.16, glow) * hole;

    if (forceField.params.y > 0.5)
        return float4(rgb * alpha, 1.0);

    return float4(rgb, alpha);
}
