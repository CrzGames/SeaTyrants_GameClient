cbuffer Context : register(b0, space3)
{
    // params0: x=time, y=waveStrength, z=pixelAmplitude, w=tiling
    float4 params0;

    // params1: x=width, y=height, z=speed, w=foamIntensity
    float4 params1;
};

// Auto-bound by SDL_RenderTexture (water #1).
Texture2D    u_texture0 : register(t0, space2);
SamplerState s0         : register(s0, space2);

// Additional samplers from SDL_GPURenderState:
// t1/s1 = water #2, t2/s2 = caustic.
Texture2D    u_texture1 : register(t1, space2);
SamplerState s1         : register(s1, space2);
Texture2D    u_texture2 : register(t2, space2);
SamplerState s2         : register(s2, space2);

struct PSInput
{
    float4 v_color : COLOR0;
    float2 v_uv    : TEXCOORD0;
};

struct PSOutput
{
    float4 o_color : SV_Target;
};

float saturate01(float v)
{
    return saturate(v);
}

float2 rotate2(float2 p, float a)
{
    float s = sin(a);
    float c = cos(a);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float3 sample4(Texture2D tex, SamplerState smp, float2 uv, float2 px)
{
    float3 a = tex.Sample(smp, frac(uv + float2(-px.x, -px.y))).rgb;
    float3 b = tex.Sample(smp, frac(uv + float2( px.x, -px.y))).rgb;
    float3 c = tex.Sample(smp, frac(uv + float2(-px.x,  px.y))).rgb;
    float3 d = tex.Sample(smp, frac(uv + float2( px.x,  px.y))).rgb;
    return (a + b + c + d) * 0.25;
}

float sample4mono(Texture2D tex, SamplerState smp, float2 uv, float2 px)
{
    float a = tex.Sample(smp, frac(uv + float2(-px.x, -px.y))).r;
    float b = tex.Sample(smp, frac(uv + float2( px.x, -px.y))).g;
    float c = tex.Sample(smp, frac(uv + float2(-px.x,  px.y))).r;
    float d = tex.Sample(smp, frac(uv + float2( px.x,  px.y))).g;
    return (a + b + c + d) * 0.25;
}

PSOutput main(PSInput input)
{
    PSOutput o;

    float  time          = params0.x;
    float  waveStrength  = max(params0.y, 0.0);
    float  pixelAmp      = max(params0.z, 0.0);
    float  tiling        = max(params0.w, 0.001);

    float2 resolution    = max(params1.xy, float2(1.0, 1.0));
    float  speed         = max(params1.z, 0.0);
    float  foamIntensity = saturate01(params1.w);

    float2 uv = input.v_uv;
    float2 invRes = 1.0 / resolution;
    float  t = time * max(speed, 0.15);

    // Keep true tiling density so 128x128 textures still produce enough detail on fullscreen maps.
    float tileBase = max(tiling, 0.5);

    // Broad wave movement used for UV warping.
    float wf1 = sin((uv.x * 11.0 + uv.y * 4.0) + t * 0.78);
    float wf2 = sin((uv.y * 10.0 - uv.x * 3.0) - t * 0.69);
    float wf3 = sin((uv.x + uv.y) * 7.0 + t * 0.52);
    float waveMix = (wf1 + 0.75 * wf2 + 0.55 * wf3) / 2.30;

    float ampUv = (pixelAmp / max(resolution.x, resolution.y)) * (0.35 + 0.85 * waveStrength);
    float2 warp = float2(wf1 - wf2, wf3 + wf2) * (0.95 * ampUv);

    // Light softening only (too much blur makes ocean look flat).
    float2 px = invRes * 0.35;

    // Water texture #1 (tile-water1) layered in two rotated flows.
    float2 w1uvA = frac(rotate2((uv + warp) * tileBase + float2( 0.026 * t,  0.016 * t),  0.22));
    float2 w1uvB = frac(rotate2((uv - warp * 0.9) * (tileBase * 1.41) + float2(-0.018 * t, 0.022 * t), -0.73));
    float3 w1A = sample4(u_texture0, s0, w1uvA, px);
    float3 w1B = sample4(u_texture0, s0, w1uvB, px * 1.2);
    float3 water1 = lerp(w1A, w1B, 0.50);

    // Water texture #2 (tile-water2) layered with different angle/scale.
    float2 w2uvA = frac(rotate2((uv + warp * 0.7) * (tileBase * 1.08) + float2(-0.014 * t, 0.020 * t), 0.91));
    float2 w2uvB = frac(rotate2((uv - warp * 1.1) * (tileBase * 1.77) + float2( 0.017 * t,-0.013 * t),-1.16));
    float3 w2A = sample4(u_texture1, s1, w2uvA, px * 0.9);
    float3 w2B = sample4(u_texture1, s1, w2uvB, px * 1.15);
    float3 water2 = lerp(w2A, w2B, 0.48);

    // Combined water detail signal.
    float3 waterTex = lerp(water1, water2, 0.56);
    float waterLuma = dot(waterTex, float3(0.299, 0.587, 0.114));

    // Caustic (tile-caustic): subtle high-frequency highlights only.
    float2 cuvA = frac((uv + warp * 1.9) * (tileBase * 2.10) + float2( 0.103 * t, -0.081 * t));
    float2 cuvB = frac(rotate2((uv - warp * 1.4) * (tileBase * 1.62) + float2(-0.079 * t, 0.098 * t), 0.41));

    float2 cuvC = frac((uv + warp * 2.7) * (tileBase * 3.05) + float2(0.147 * t, -0.133 * t));

    float ca = sample4mono(u_texture2, s2, cuvA, px * 1.45);
    float cb = sample4mono(u_texture2, s2, cuvB, px * 1.80);
    float cc = sample4mono(u_texture2, s2, cuvC, px * 1.10);

    float causticRaw = saturate01(ca * 0.70 + cb * 0.50);
    float causticMain = smoothstep(0.60, 0.90, causticRaw);
    float causticSparkle = smoothstep(0.78, 0.985, cc);
    float caustic = saturate01(causticMain + causticSparkle * 0.48);
    caustic *= (0.55 + 0.95 * foamIntensity);

    // Seafight-like palette.
    float3 deepColor   = float3(0.020, 0.170, 0.360);
    float3 midColor    = float3(0.070, 0.330, 0.560);
    float3 lightColor  = float3(0.140, 0.490, 0.700);
    float3 foamColor   = float3(0.900, 0.965, 1.000);
    float3 glintColor  = float3(0.840, 0.940, 1.000);

    float waterDetail = saturate01((waterLuma - 0.42) * 1.65 + 0.50);
    float depthMask = saturate01(0.22 + waterDetail * 0.58 + (waveMix * 0.5 + 0.5) * 0.20);

    float3 ocean = lerp(deepColor, midColor, depthMask);
    ocean = lerp(ocean, lightColor, saturate01((waterDetail - 0.45) * 0.95 + caustic * 0.40));

    // Drive brightness mostly from the texture content to keep organic look.
    float detailFactor = 0.74 + 0.62 * waterDetail;
    float3 rgb = ocean * detailFactor;

    // Controlled reflective glints.
    rgb = lerp(rgb, glintColor, caustic * 0.48);

    float crest = smoothstep(0.72, 0.95, waveMix * 0.5 + 0.5 + (waterDetail - 0.5) * 0.22);
    float foam = crest * (0.12 + 0.26 * foamIntensity);
    rgb = lerp(rgb, foamColor, foam);

    // Extra white streaks to get the classic sea-sparkle feel.
    float streakField = abs(sin((uv.x * 120.0 + uv.y * 34.0) + t * 1.1));
    float streakMask = smoothstep(0.955, 0.997, streakField) * causticMain;
    float whiteSheen = caustic * (0.12 + 0.16 * (waveMix * 0.5 + 0.5)) + streakMask * 0.35;
    rgb += whiteSheen * float3(0.18, 0.20, 0.22);

    rgb = saturate(rgb);
    rgb *= input.v_color.rgb;

    o.o_color = float4(rgb, input.v_color.a);
    return o;
}
