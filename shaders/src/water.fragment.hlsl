cbuffer Context : register(b0, space3)
{
    // params0: x=time, y=waveStrength, z=pixelAmplitude, w=tiling
    float4 params0;
    // params1: x=width, y=height, z=speed, w=foamIntensity
    float4 params1;
};

// Auto-bound by SDL_RenderTexture.
Texture2D    u_texture0 : register(t0, space2); // tile-water-base
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

float2 rotate2(float2 p, float a)
{
    float s = sin(a);
    float c = cos(a);
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

    float time = params0.x;
    float waveStrength = max(params0.y, 0.0);
    float pixelAmp = max(params0.z, 0.0);
    float tiling = max(params0.w, 0.001);

    float2 resolution = max(params1.xy, float2(1.0, 1.0));
    float speed = max(params1.z, 0.0);
    float foamIntensity = saturate(params1.w);

    float2 uv = input.v_uv;
    float2 invRes = 1.0 / resolution;
    float t = time * max(speed, 0.15);

    float tileBase = max(tiling, 1.4);
    float tileDetail = tileBase * 2.0;
    float tileCaustic = tileBase * 3.4;
    float tileFoam = tileBase * 2.4;

    // Global motion field.
    float w1 = sin((uv.x * 7.0 + uv.y * 4.0) + t * 0.68);
    float w2 = sin((uv.y * 8.0 - uv.x * 5.0) - t * 0.59);
    float w3 = sin((uv.x + uv.y) * 6.0 + t * 0.46);
    float waveMix = (w1 + w2 + w3) / 3.0;

    float ampUv = (pixelAmp / max(resolution.x, resolution.y)) * (0.24 + 0.66 * waveStrength);
    float2 warp = float2(w1 - w2, w2 + w3) * (0.88 * ampUv);
    float2 warpHi = float2(w3 - w1, w1 + w2) * (1.45 * ampUv);
    float2 px = invRes * 0.20;

    // Base ocean color texture.
    float2 bUvA = frac(rotate2((uv + warp) * tileBase + float2( 0.016 * t,  0.011 * t),  0.18));
    float2 bUvB = frac(rotate2((uv - warp) * (tileBase * 1.27) + float2(-0.012 * t, 0.015 * t), -0.63));
    float3 baseA = sample4rgb(u_texture0, s0, bUvA, px);
    float3 baseB = sample4rgb(u_texture0, s0, bUvB, px * 1.08);
    float3 baseTex = lerp(baseA, baseB, 0.5);

    // Fine detail.
    float2 dUvA = frac(rotate2((uv + warpHi * 0.7) * tileDetail + float2(-0.012 * t, 0.018 * t),  0.82));
    float2 dUvB = frac(rotate2((uv - warpHi * 0.9) * (tileDetail * 1.38) + float2(0.015 * t, -0.013 * t), -1.03));
    float3 detA = sample4rgb(u_texture1, s1, dUvA, px * 0.85);
    float3 detB = sample4rgb(u_texture1, s1, dUvB, px * 0.98);
    float3 detailTex = lerp(detA, detB, 0.52);

    float3 waterTex = lerp(baseTex, detailTex, 0.40);
    float waterL = lum(waterTex);
    float waterContrast = saturate(0.50 + (waterL - 0.50) * 1.08);

    // Macro map to break large-scale repetition.
    float2 mUvA = frac(rotate2(uv * 0.26 + float2( 0.004 * t, -0.003 * t),  0.54));
    float2 mUvB = frac(rotate2(uv * 0.19 + float2(-0.003 * t,  0.004 * t), -0.91));
    float3 macroA = sample4rgb(u_texture4, s4, mUvA, px * 2.0);
    float3 macroB = sample4rgb(u_texture4, s4, mUvB, px * 2.3);
    float macroL = lum(lerp(macroA, macroB, 0.5));
    float macroShade = lerp(0.92, 1.04, macroL);

    // Caustic lines and sparkles.
    float2 cUvA = frac((uv + warpHi * 1.2) * tileCaustic + float2( 0.085 * t, -0.067 * t));
    float2 cUvB = frac(rotate2((uv - warpHi) * (tileCaustic * 1.20) + float2(-0.070 * t, 0.079 * t), 0.39));
    float2 cUvC = frac((uv + warpHi * 1.8) * (tileCaustic * 1.65) + float2(0.114 * t, -0.103 * t));

    float cA = sample4gray(u_texture2, s2, cUvA, px * 1.15);
    float cB = sample4gray(u_texture2, s2, cUvB, px * 1.35);
    float cC = sample4gray(u_texture2, s2, cUvC, px * 1.00);

    float causticRaw = saturate(cA * 0.66 + cB * 0.40);
    float causticLines = smoothstep(0.70, 0.93, causticRaw);
    float causticSpark = smoothstep(0.90, 0.995, cC);
    float caustic = (causticLines * 0.82 + causticSpark * 0.26) * (0.40 + 0.88 * foamIntensity);

    // Foam streaks use ALPHA channel (RGB is white by design in this texture).
    float2 fUvA = frac(rotate2((uv - warp * 0.55) * tileFoam + float2( 0.051 * t, -0.043 * t),  0.29));
    float2 fUvB = frac(rotate2((uv + warp * 0.75) * (tileFoam * 1.33) + float2(-0.045 * t, 0.052 * t), -0.60));
    float fA = sample4alpha(u_texture3, s3, fUvA, px * 1.0);
    float fB = sample4alpha(u_texture3, s3, fUvB, px * 1.2);
    float foamRaw = fA * 0.70 + fB * 0.55;
    float foamStreaks = smoothstep(0.62, 0.92, foamRaw);
    foamStreaks *= (0.20 + 0.80 * causticLines);
    foamStreaks *= (0.30 + 0.75 * foamIntensity);

    // Palette close to Seafight style.
    float3 deepColor  = float3(0.018, 0.150, 0.330);
    float3 midColor   = float3(0.045, 0.275, 0.505);
    float3 lightColor = float3(0.090, 0.400, 0.640);
    float3 glintColor = float3(0.820, 0.920, 1.000);
    float3 foamColor  = float3(0.900, 0.965, 1.000);

    float depthMask = saturate(0.24 + waterContrast * 0.44 + (waveMix * 0.5 + 0.5) * 0.14);
    float3 ocean = lerp(deepColor, midColor, depthMask);
    ocean = lerp(ocean, lightColor, saturate(caustic * 0.32 + (waterContrast - 0.52) * 0.11));
    ocean *= macroShade;

    float3 rgb = ocean * (0.80 + 0.34 * waterContrast);
    rgb = lerp(rgb, glintColor, saturate(caustic * 0.42));

    float crest = smoothstep(0.73, 0.95, waveMix * 0.5 + 0.5 + (waterContrast - 0.5) * 0.16);
    float foam = crest * (0.04 + 0.10 * foamIntensity) + foamStreaks * 0.30;
    rgb = lerp(rgb, foamColor, saturate(foam));

    float sparklePulse = 0.5 + 0.5 * sin((uv.x * 186.0 + uv.y * 149.0) + t * 2.2);
    float microSpark = smoothstep(0.995, 0.9997, sparklePulse) * causticSpark * (0.08 + 0.20 * foamIntensity);
    rgb += microSpark * float3(0.16, 0.19, 0.22);

    rgb = saturate(rgb) * input.v_color.rgb;
    o.o_color = float4(rgb, input.v_color.a);
    return o;
}
