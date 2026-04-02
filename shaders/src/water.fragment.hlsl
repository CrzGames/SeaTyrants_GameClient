cbuffer Context : register(b0, space3)
{
    // params0: x=time, y=waveStrength, z=pixelAmplitude, w=tiling
    float4 params0;
    // params1: x=width, y=height, z=speed, w=foamIntensity
    float4 params1;
};

// Auto-bound by SDL_RenderTexture (tile-water-base).
Texture2D    u_texture0 : register(t0, space2);
SamplerState s0         : register(s0, space2);

// Additional samplers from SDL_GPURenderState.
Texture2D    u_texture1 : register(t1, space2); // tile-water-detail
SamplerState s1         : register(s1, space2);
Texture2D    u_texture2 : register(t2, space2); // tile-caustic
SamplerState s2         : register(s2, space2);
Texture2D    u_texture3 : register(t3, space2); // tile-foam-streaks (alpha)
SamplerState s3         : register(s3, space2);
Texture2D    u_texture4 : register(t4, space2); // water-macro
SamplerState s4         : register(s4, space2);

struct PSInput
{
    float4 v_color : COLOR0;
    float2 v_uv    : TEXCOORD0;
};

struct PSOutput
{
    float4 o_color : SV_Target;
};

float lum(float3 c)
{
    return dot(c, float3(0.299, 0.587, 0.114));
}

float2 rotate2(float2 p, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float3 sample4rgb(Texture2D tex, SamplerState smp, float2 uv, float2 px)
{
    float3 a = tex.Sample(smp, frac(uv + float2(-px.x, -px.y))).rgb;
    float3 b = tex.Sample(smp, frac(uv + float2( px.x, -px.y))).rgb;
    float3 c = tex.Sample(smp, frac(uv + float2(-px.x,  px.y))).rgb;
    float3 d = tex.Sample(smp, frac(uv + float2( px.x,  px.y))).rgb;
    return (a + b + c + d) * 0.25;
}

float sample4gray(Texture2D tex, SamplerState smp, float2 uv, float2 px)
{
    float a = lum(tex.Sample(smp, frac(uv + float2(-px.x, -px.y))).rgb);
    float b = lum(tex.Sample(smp, frac(uv + float2( px.x, -px.y))).rgb);
    float c = lum(tex.Sample(smp, frac(uv + float2(-px.x,  px.y))).rgb);
    float d = lum(tex.Sample(smp, frac(uv + float2( px.x,  px.y))).rgb);
    return (a + b + c + d) * 0.25;
}

float sample4alpha(Texture2D tex, SamplerState smp, float2 uv, float2 px)
{
    float a = tex.Sample(smp, frac(uv + float2(-px.x, -px.y))).a;
    float b = tex.Sample(smp, frac(uv + float2( px.x, -px.y))).a;
    float c = tex.Sample(smp, frac(uv + float2(-px.x,  px.y))).a;
    float d = tex.Sample(smp, frac(uv + float2( px.x,  px.y))).a;
    return (a + b + c + d) * 0.25;
}

PSOutput main(PSInput input)
{
    PSOutput o;

    float time         = params0.x;
    float waveStrength = max(params0.y, 0.0);
    float pixelAmp     = max(params0.z, 0.0);
    float tiling       = max(params0.w, 0.001);

    float2 resolution    = max(params1.xy, float2(1.0, 1.0));
    float  speed         = max(params1.z, 0.0);
    float  foamIntensity = saturate(params1.w);

    float2 uv = input.v_uv;
    float2 invRes = 1.0 / resolution;
    // Enforce a minimum flow speed so the ocean never looks frozen.
    float t = time * max(speed, 0.55);

    float tileBase    = max(tiling, 1.45);
    float tileDetail  = tileBase * 1.78;
    float tileCaustic = tileBase * 2.85;
    float tileFoam    = tileBase * 2.70;
    float macroTile   = 0.23;

    // Irregular motion field.
    float w1 = sin((uv.x * 6.73 + uv.y * 4.11) + t * 0.63);
    float w2 = sin((uv.y * 8.29 - uv.x * 5.37) - t * 0.57);
    float w3 = sin((uv.x + uv.y) * 5.41 + t * 0.49);
    float w4 = sin((uv.x * 2.17 - uv.y * 9.13) + t * 0.41);
    float waveMix = (w1 + w2 + w3 + w4) * 0.25;

    float2 ampUv = (pixelAmp * invRes) * (0.40 + 0.82 * waveStrength);
    float2 warp = float2(w1 - w2, w2 + w3) * (0.95 * ampUv);
    float2 warpHi = float2(w3 - w4, w1 + w2) * (1.60 * ampUv);
    // Filtering tuned to reduce temporal shimmer while keeping visible animation.
    float2 pxBase = invRes * 0.40;
    float2 pxDetail = invRes * 0.95;
    float2 pxCaustic = invRes * 0.55;

    // Macro-based random UV warp (breaks visible tiling lock).
    float2 rwUvA = frac(rotate2(uv * macroTile + float2( 0.0032 * t, -0.0024 * t),  0.67));
    float2 rwUvB = frac(rotate2(uv * (macroTile * 1.31) + float2(-0.0021 * t, 0.0031 * t), -1.08));
    float2 rwA = sample4rgb(u_texture4, s4, rwUvA, pxBase * 3.0).rg * 2.0 - 1.0;
    float2 rwB = sample4rgb(u_texture4, s4, rwUvB, pxBase * 3.5).rg * 2.0 - 1.0;
    float2 randomWarp = (rwA * 0.6 + rwB * 0.4) * 0.014;
    float2 drift = float2(0.022 * t, -0.016 * t);
    float2 uvFlow = uv + randomWarp + warp + drift;

    // Base water layer.
    float2 bUvA = frac(rotate2((uvFlow + warp * 0.55) * tileBase + float2( 0.030 * t,  0.022 * t),  0.22));
    float2 bUvB = frac(rotate2((uvFlow - warp * 0.35) * (tileBase * 1.31) + float2(-0.021 * t, 0.026 * t), -0.71));
    float3 baseA = sample4rgb(u_texture0, s0, bUvA, pxBase);
    float3 baseB = sample4rgb(u_texture0, s0, bUvB, pxBase * 1.08);
    float3 baseTex = lerp(baseA, baseB, 0.5);

    // Detail layer.
    float2 dUvA = frac(rotate2((uvFlow + warpHi * 0.90) * tileDetail + float2(-0.034 * t, 0.041 * t),  1.03));
    float2 dUvB = frac(rotate2((uvFlow - warpHi * 1.05) * (tileDetail * 1.37) + float2(0.043 * t, -0.036 * t), -0.95));
    float3 detA = sample4rgb(u_texture1, s1, dUvA, pxDetail * 0.95);
    float3 detB = sample4rgb(u_texture1, s1, dUvB, pxDetail * 1.05);
    float3 detailTex = lerp(detA, detB, 0.52);
    // Compress bright micro-speckles in detail map.
    float detailL = lum(detailTex);
    detailTex *= lerp(1.0, 0.82, saturate((detailL - 0.60) * 2.2));

    float3 waterTex = lerp(baseTex, detailTex, 0.20);
    float waterL = lum(waterTex);

    // Macro shade layer.
    float2 mUvA = frac(rotate2(uvFlow * 0.19 + float2( 0.004 * t, -0.003 * t),  0.51));
    float2 mUvB = frac(rotate2(uvFlow * 0.14 + float2(-0.003 * t,  0.004 * t), -0.86));
    float3 macroA = sample4rgb(u_texture4, s4, mUvA, pxBase * 2.0);
    float3 macroB = sample4rgb(u_texture4, s4, mUvB, pxBase * 2.4);
    float macroL = lum(lerp(macroA, macroB, 0.5));
    float macroShade = lerp(0.985, 1.015, macroL);

    // Caustic layers.
    float2 cUvA = frac((uvFlow + warpHi * 1.2) * tileCaustic + float2( 0.052 * t, -0.039 * t));
    float2 cUvB = frac(rotate2((uvFlow - warpHi * 0.9) * (tileCaustic * 1.23) + float2(-0.043 * t, 0.045 * t), 0.39));
    float2 cUvC = frac((uvFlow + warpHi * 1.7) * (tileCaustic * 1.59) + float2( 0.068 * t, -0.059 * t));

    float cA = sample4gray(u_texture2, s2, cUvA, pxCaustic * 1.05);
    float cB = sample4gray(u_texture2, s2, cUvB, pxCaustic * 1.20);
    float cC = sample4gray(u_texture2, s2, cUvC, pxCaustic * 0.95);

    float causticRaw = saturate(cA * 0.72 + cB * 0.58);
    float causticAA = saturate(fwidth(causticRaw) * 1.2 + 0.010);
    float causticSoft = smoothstep(0.45 - causticAA, 0.72 + causticAA, causticRaw);
    float causticLines = smoothstep(0.62 - causticAA, 0.86 + causticAA, causticRaw);
    float causticSpark = smoothstep(0.95 - causticAA, 0.995, cC);
    float caustic = (causticSoft * 0.30 + causticLines * 0.70 + causticSpark * 0.05) * (0.22 + 0.68 * foamIntensity);

    // Foam streaks from alpha channel.
    float2 fUvA = frac(rotate2((uvFlow - warp * 0.55) * tileFoam + float2( 0.056 * t, -0.047 * t),  0.27));
    float2 fUvB = frac(rotate2((uvFlow + warp * 0.80) * (tileFoam * 1.36) + float2(-0.049 * t, 0.055 * t), -0.62));
    float fA = sample4alpha(u_texture3, s3, fUvA, pxDetail * 1.0);
    float fB = sample4alpha(u_texture3, s3, fUvB, pxDetail * 1.2);
    float foamRaw = fA * 0.76 + fB * 0.56;
    float foamStreaks = smoothstep(0.60, 0.93, foamRaw);
    foamStreaks *= (0.08 + 0.35 * causticLines);
    foamStreaks *= (0.12 + 0.62 * foamIntensity);

    // Seafight-like palette.
    float3 deepColor  = float3(0.016, 0.125, 0.290);
    float3 midColor   = float3(0.030, 0.240, 0.470);
    float3 lightColor = float3(0.070, 0.355, 0.595);
    float3 glintColor = float3(0.820, 0.920, 1.000);
    float3 foamColor  = float3(0.900, 0.965, 1.000);

    float depthMask = saturate(
        0.18 +
        waterL * 0.52 +
        (waveMix * 0.5 + 0.5) * 0.12 +
        (macroL - 0.5) * 0.18
    );

    float3 ocean = lerp(deepColor, midColor, depthMask);
    ocean = lerp(ocean, lightColor, saturate((waterL - 0.42) * 0.55 + caustic * 0.32));
    ocean *= macroShade;

    float3 rgb = waterTex * ocean * (0.86 + 0.32 * waterL);
    float3 causticColor = float3(0.24, 0.64, 0.92);
    rgb = lerp(rgb, causticColor, saturate(caustic * 0.34));
    rgb = lerp(rgb, glintColor, saturate(caustic * 0.10));

    float crest = smoothstep(0.70, 0.95, (waveMix * 0.5 + 0.5) + (waterL - 0.5) * 0.20);
    float foam = crest * (0.02 + 0.06 * foamIntensity) + foamStreaks * 0.15;
    rgb = lerp(rgb, foamColor, saturate(foam));

    // Slow broad swell modulation for visible movement without pixel sparkle.
    float swell = 0.5 + 0.5 * sin((uvFlow.x * 2.3 + uvFlow.y * 1.7) + t * 0.55);
    rgb *= lerp(0.96, 1.04, swell);

    rgb = saturate(rgb) * input.v_color.rgb;
    o.o_color = float4(rgb, input.v_color.a);
    return o;
}
