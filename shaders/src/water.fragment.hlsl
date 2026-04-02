cbuffer Context : register(b0, space3) {
    float4 params0; // x=time, y=strength, z=px_amp, w=tiling
    float4 params1; // x=width, y=height, z=speed, w=unused
};

// Garder t0/s0 (auto-bind par SDL) si tu veux, mais on ne l'utilise plus ici
Texture2D    u_texture0 : register(t0, space2);
SamplerState s0         : register(s0, space2);

// <-- CEUX-CI seront tes bindings "sampler_bindings" côté C -->
Texture2D    u_texture1 : register(t1, space2);
SamplerState s1         : register(s1, space2);

struct PSInput { float4 v_color : COLOR0; float2 v_uv : TEXCOORD0; };
struct PSOutput { float4 o_color : SV_Target; };
static const float PI = 3.14159265f;

PSOutput main(PSInput input) {
    PSOutput o;

    float  time       = params0.x;
    float  strength   = params0.y;
    float  px_amp     = params0.z;
    float  tiling     = params0.w;
    float2 resolution = params1.xy;
    float  speed      = params1.z;

    // Défilement doux (courant)
    float2 baseUV = input.v_uv + float2(time * 0.02 * speed, time * 0.013 * speed);

    // Tiling SANS frac -> le sampler REPEAT fera la répétition
    float2 uv = baseUV * tiling;

    // Amplitude en UV (pixels -> [0..1])
    float2 invRes = 1.0 / max(resolution, float2(1.0, 1.0));
    float2 ampUV  = px_amp * invRes * strength;

    // (option) désynchroniser un peu chaque tuile pour éviter l’effet “en phase”
    float2 tileId = floor(uv); // index entier de tuile
    float  hash   = frac(sin(dot(tileId, float2(12.9898,78.233))) * 43758.5453);
    float  phase  = hash * 6.2831853; // 0..2π

    float s1a = sin( (uv.x * 10.0 + time * 1.20) * PI + phase );
    float s2a = sin( (uv.y * 14.0 - time * 0.90) * PI + phase * 0.7 );
    float s3a = sin( ((uv.x+uv.y) * 8.0 + time * 0.65) * PI + phase * 1.3 );

    float2 offset = float2(s1a - s2a, s2a + s3a) * 0.5 * ampUV;

    // échantillonnage sur TON binding (t1/s1) avec REPEAT
    float4 c = u_texture1.Sample(s1, uv + offset) * input.v_color;

    // petite “respiration” chromatique
    c.rg += 0.008 * sin(time * 1.7 + (uv.x + uv.y) * 20.0);

    o.o_color = c;
    return o;
}
