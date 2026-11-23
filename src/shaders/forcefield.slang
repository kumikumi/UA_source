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
// mipmaps. Nothing moves: as in the original, from one moment to the next
// some patches are there and others gone. Every couple of seconds, each at
// its own time, a shard flares up in a tenth of a second to a new shape and
// color, then fades on its own down through the colors, crumbling, until it
// is dim or clear; and the shards' frayed edges crackle. The texture tiles
// seamlessly, and the animation repeats every LOOP seconds without a jump:
// everything flares a whole number of times per loop. See world.slang for the
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

// A random number for a lattice point, the lattice wrapping around every n
// points so that the pattern tiles
uint Key(int2 p, int2 n, uint seed)
{
    int2 w = ((p % n) + n) % n;
    return HashU(uint(w.x) * 1597334677u ^ uint(w.y) * 3812015801u ^ (uint(n.x * 31 + n.y) + seed) * 2654435761u);
}

// Four numbers from 0 to 1 from a random number
float4 Rand4(uint key)
{
    uint g = HashU(key ^ 0x9e3779b9u);
    return float4(float(key & 0xFFFFu), float(key >> 16), float(g & 0xFFFFu), float(g >> 16)) / 65535.0;
}

// A lattice point's states over time: it holds one for a while, then quickly
// changes to the next, a whole number of times per loop, from half to one and
// a half times `steps`, at a random phase. The two states as random numbers,
// how far time is into the step, from 0 to 1, and the step's length in
// seconds.
struct Step
{
    uint a;
    uint b;
    float f;
    float seconds;
};

Step Steps(uint key, float s, float steps)
{
    float4 h = Rand4(key);
    int n = max(int(steps * (0.5 + h.z)), 1);
    float x = s * float(n) + h.x;
    int k = int(floor(x));

    Step step;
    step.a = HashU(key + uint(k % n) * 0x9e3779b9u);
    step.b = HashU(key + uint((k + 1) % n) * 0x9e3779b9u);
    step.f = x - floor(x);
    step.seconds = LOOP / float(n);
    return step;
}

// From 0 to 1, how far the change to the step's next state has got, the
// change taking the last `seconds` of the step
float Change(Step step, float seconds)
{
    return smoothstep(1.0 - seconds / step.seconds, 1.0, step.f);
}

// From -1 to 1, a state's level: as likely as the sine of a random angle
float Level(uint state)
{
    return sin(TAU * float(state & 0xFFFFu) / 65535.0);
}

// Smooth noise from 0 to 1 with n.x by n.y lattice cells per tile, whose
// lattice values step from one random level to the next
float Noise(float2 uv, int2 n, uint seed, float s, float steps)
{
    float2 p = uv * float2(n);
    int2 i = int2(floor(p));
    float2 f = p - floor(p);
    f = f * f * (3.0 - 2.0 * f);

    float v[4];
    for (int j = 0; j < 4; j++)
    {
        Step step = Steps(Key(i + int2(j & 1, j >> 1), n, seed), s, steps);
        v[j] = lerp(Level(step.a), Level(step.b), Change(step, 0.15));
    }

    return 0.5 + 0.5 * lerp(lerp(v[0], v[1], f.x), lerp(v[2], v[3], f.x), f.y);
}

// A shard's value at offset from its cell's point, `age` seconds after it
// flared up: a level, which fades from a random peak down to the lowest, and
// a ramp across the cell in a random direction. The fade's time constant is
// from a third to nearly twice `fade` seconds, varying in splinters across
// the shard, so that it crumbles as it fades.
float Shard(uint state, float2 uv, float2 offset, float slope, float age, float fade)
{
    float peak = float(state & 0xFFFFu) / 65535.0;
    float level = peak;

    if (age > 0.0)
    {
        // Still noise, shifted by the state so that each crumbles differently
        float2 shift = Rand4(state).zw;
        float crumble = Noise(uv + shift, int2(96, 24), state, 0.0, 1.0);
        level = lerp(-1.0, peak, exp(-age / (fade * (0.3 + 1.4 * crumble))));
    }

    float dir = TAU * float(state >> 16) / 65535.0;
    return 0.5 + 0.3 * level + slope * dot(offset, float2(cos(dir), sin(dir)));
}

// Shards: the cells nearest to n.x by n.y points per tile. Each cell's value
// ramps across it, so that thresholds cut it along straight lines. Every so
// often a cell quickly flares up to a new level and ramp, then fades: a patch
// appears, changes its shape and color, dims and vanishes.
float Shards(float2 uv, int2 n, uint seed, float s, float steps, float slope, float fade)
{
    float2 p = uv * float2(n);
    int2 cell = int2(floor(p));
    float2 f = p - floor(p);

    float nearest = 8.0;
    int2 nearestCell = cell;
    float2 offset = float2(0.0, 0.0);

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float angle = TAU * Rand4(Key(cell + int2(x, y), n, seed)).x;
            float2 d = f - (float2(x, y) + 0.5 + 0.3 * float2(cos(angle), sin(angle)));
            float dist = dot(d, d);

            if (dist < nearest)
            {
                nearest = dist;
                nearestCell = cell + int2(x, y);
                offset = d;
            }
        }
    }

    // The flare up to the next state takes the last 0.1 seconds of the step
    Step step = Steps(Key(nearestCell, n, seed + 1u), s, steps);
    float now = Shard(step.a, uv, offset, slope, step.f * step.seconds, fade);
    float next = Shard(step.b, uv, offset, slope, 0.0, fade);
    return lerp(now, next, Change(step, 0.1));
}

// Splinters, to fray the shards' edges: noise in thin streaks
float Splinters(float2 uv, uint seed, float s)
{
    return 0.12 * (Noise(uv, int2(96, 24), seed, s, 48.0) - 0.5)
         + 0.12 * (Noise(uv, int2(192, 12), seed + 1u, s, 48.0) - 0.5);
}

// The veil and the glow at uv, at s from 0 to 1 over the loop
float2 Fields(float2 uv, float s)
{
    // Cells of about 5 by 43 texels and 6 by 51, so 12 by 100 units and 15
    // by 120 on the walls. Both flare about every 2 seconds; the veil fades
    // over a couple of seconds, the glow's quicker flashes in under one.
    float veil = Shards(uv, int2(48, 6), 10u, s, 32.0, 0.6, 2.0) + Splinters(uv, 11u, s);
    float glow = Shards(uv, int2(40, 5), 20u, s, 32.0, 0.6, 0.8) + Splinters(uv, 21u, s);
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

    if (field.y > 0.545)
        color = float4(130.0, 236.0, 255.0, 41.0) / 255.0;
    else if (field.x > 0.479)
        color = float4(66.0, 112.0, 255.0, 50.0) / 255.0;
    else if (field.x > 0.228)
        color = float4(0.0, 0.0, 255.0, 26.0) / 255.0;
    else
        color = float4(0.0, 0.0, 0.0, 0.0);

    if (forceField.params.y > 0.5)
        return float4(color.rgb * color.a, 1.0);

    return color;
}
