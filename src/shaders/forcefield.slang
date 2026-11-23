// The force field of the map's border and of open beam gates, drawn by the
// SDL_GPU backend into a 256 x 256 layer of the texture array every frame, in
// place of the original's animation: a small patch of FX2.ILBM shifted around
// at 25 frames per second. The walls stretched the patch over each of their
// polygons, 150 units wide and 900 tall, so its texels showed as thin
// streaks, 2 units wide and 11 tall.
//
// Like the patch, the texture is a mosaic of sharp edged shards in the
// original's four colors, with no blends between them. The shards are tall
// and narrow, so that on the walls they are about the size and shape of the
// patch's, and they shimmer in the distance about as much, as neither has
// mipmaps. Nothing drifts: the shards churn in place, their edges sweeping
// back and forth and their corners circling. The texture tiles seamlessly,
// and the animation repeats every LOOP seconds without a jump: everything
// that moves makes a whole number of turns per loop. See world.slang for the
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

// Four numbers from 0 to 1 for a lattice point, the lattice wrapping around
// every n points so that the pattern tiles
float4 Hash4(int2 p, int2 n, uint seed)
{
    int2 w = ((p % n) + n) % n;
    uint h = HashU(uint(w.x) * 1597334677u ^ uint(w.y) * 3812015801u ^ (uint(n.x * 31 + n.y) + seed) * 2654435761u);
    uint g = HashU(h ^ 0x9e3779b9u);
    return float4(float(h & 0xFFFFu), float(h >> 16), float(g & 0xFFFFu), float(g >> 16)) / 65535.0;
}

// From -1 to 1, swinging back and forth a whole number of times per loop: at
// a random phase h.x, either way round by h.y, and from half to one and a
// half times `turns` by h.z
float Swing(float4 h, float s, float turns)
{
    return sin(TAU * (h.x + s * floor(turns * (0.5 + h.z)) * (h.y < 0.5 ? 1.0 : -1.0)));
}

// Smooth noise from 0 to 1 with n.x by n.y lattice cells per tile, whose
// lattice values swing up and down in place
float Noise(float2 uv, int2 n, uint seed, float s, float turns)
{
    float2 p = uv * float2(n);
    int2 i = int2(floor(p));
    float2 f = p - floor(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = Swing(Hash4(i, n, seed), s, turns);
    float b = Swing(Hash4(i + int2(1, 0), n, seed), s, turns);
    float c = Swing(Hash4(i + int2(0, 1), n, seed), s, turns);
    float d = Swing(Hash4(i + int2(1, 1), n, seed), s, turns);
    return 0.5 + 0.5 * lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// Shards: the cells nearest to points that circle in place, n.x by n.y of
// them per tile. Each cell's value ramps across it in its own direction and
// swings up and down, so that thresholds cut it along straight lines that
// sweep back and forth.
float Shards(float2 uv, int2 n, uint seed, float s, float turns, float slope)
{
    float2 p = uv * float2(n);
    int2 cell = int2(floor(p));
    float2 f = p - floor(p);

    float nearest = 8.0;
    int2 nearestCell = cell;
    float2 offset = float2(0.0, 0.0);
    float4 h = float4(0.0, 0.0, 0.0, 0.0);

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float4 k = Hash4(cell + int2(x, y), n, seed);
            float angle = TAU * (k.x + s * 2.0 * (k.y < 0.5 ? 1.0 : -1.0));
            float2 d = f - (float2(x, y) + 0.5 + 0.3 * float2(cos(angle), sin(angle)));
            float dist = dot(d, d);

            if (dist < nearest)
            {
                nearest = dist;
                nearestCell = cell + int2(x, y);
                offset = d;
                h = k;
            }
        }
    }

    float dir = TAU * h.z;
    return 0.5 + 0.3 * Swing(Hash4(nearestCell, n, seed + 1u), s, turns)
         + slope * dot(offset, float2(cos(dir), sin(dir)));
}

// Splinters, to fray the shards' edges: noise in thin streaks
float Splinters(float2 uv, uint seed, float s)
{
    return 0.12 * (Noise(uv, int2(96, 24), seed, s, 32.0) - 0.5)
         + 0.12 * (Noise(uv, int2(192, 12), seed + 1u, s, 32.0) - 0.5);
}

// The veil and the glow at uv, at s from 0 to 1 over the loop
float2 Fields(float2 uv, float s)
{
    // A warp that sways each point around in a little loop, by up to 2
    // texels across and 3 up and down
    float2 warp = float2(Noise(uv, int2(8, 4), 1u, s, 16.0), Noise(uv, int2(8, 4), 2u, s, 16.0)) - 0.5;
    float2 p = uv + float2(4.0, 6.0) / 256.0 * warp;

    // Cells of about 5 by 43 texels and 6 by 51, so 12 by 100 units and 15
    // by 120 on the walls
    float veil = Shards(p, int2(48, 6), 10u, s, 16.0, 0.6) + Splinters(p, 11u, s);
    float glow = Shards(p, int2(40, 5), 20u, s, 12.0, 0.6) + Splinters(p, 21u, s);
    return float2(veil, glow);
}

[shader("fragment")]
float4 fsForceField(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float2 field = Fields(uv, forceField.params.x / LOOP);

    // The original's palette: clear, pure blue, azure and cyan, at alphas of
    // 0.10, 0.20 and 0.16. The thresholds give each about its share of the
    // original's animation, so its average color and alpha: 12% clear, 28%
    // blue, 37% azure and 23% cyan.
    float4 color;

    if (field.y > 0.733)
        color = float4(130.0, 236.0, 255.0, 41.0) / 255.0;
    else if (field.x > 0.519)
        color = float4(66.0, 112.0, 255.0, 50.0) / 255.0;
    else if (field.x > 0.195)
        color = float4(0.0, 0.0, 255.0, 26.0) / 255.0;
    else
        color = float4(0.0, 0.0, 0.0, 0.0);

    if (forceField.params.y > 0.5)
        return float4(color.rgb * color.a, 1.0);

    return color;
}
