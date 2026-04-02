cbuffer Context : register(b0, space3)
{
    // params0: x=time, y=waveStrength, z=pixelAmplitude, w=tiling
    float4 params0;
    // params1: x=width, y=height, z=speed, w=foamIntensity
    float4 params1;
    // params2: x=colorMode (0=blue legacy, 1=neutral),
    //          y=fresnelStrength, z=sunGlintStrength, w=whitecapBoost
    float4 params2;
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
    // Oscillating drift: avoids a single dominant travel direction across the whole ocean.
    float2 drift = float2(0.022 * sin(t * 0.37), 0.019 * cos(t * 0.31));
    float2 uvFlow = uv + randomWarp + warp + drift;

    // Add local "chop" so the water does not look laminar/flat when observed up close.
    float chop1 = abs(sin((uv.x * 21.0 - uv.y * 15.0) + t * 1.85));
    float chop2 = abs(sin((uv.x * 18.0 + uv.y * 12.0) - t * 1.45));
    float chop = pow(saturate(chop1 * 0.55 + chop2 * 0.45), 2.0);
    float2 chopDir = normalize(float2(w1 - w3, w2 + w4) + 1e-5);
    uvFlow += chopDir * (ampUv * 0.85) * (chop - 0.35);

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

    // Large-scale wind gust field (very subtle), used to break monotony.
    float2 windDir = normalize(float2(0.86, -0.51));
    float2 gUvA = frac(uvFlow * 0.085 + windDir * (0.028 * t));
    float2 gUvB = frac(rotate2(uvFlow * 0.061 + windDir * (-0.019 * t), 0.90));
    float gustNoise = lum(lerp(sample4rgb(u_texture4, s4, gUvA, pxBase * 2.8),
                               sample4rgb(u_texture4, s4, gUvB, pxBase * 3.2), 0.45));
    float gustBands = 0.5 + 0.5 * sin(dot(uvFlow, windDir) * 6.8 + t * 0.42 + gustNoise * 4.2);
    float gust = smoothstep(0.58, 0.90, gustBands) * (0.35 + 0.65 * gustNoise);

    // Caustic layers:
    // add local advection + breathing scale so motion feels less "flat translation".
    float2 cPhaseUvA = frac(rotate2(uvFlow * 0.078 + float2( 0.006 * t, -0.004 * t),  0.41));
    float2 cPhaseUvB = frac(rotate2(uvFlow * 0.061 + float2(-0.005 * t,  0.005 * t), -0.73));
    float2 cPhaseA = sample4rgb(u_texture4, s4, cPhaseUvA, pxBase * 2.8).rg * 2.0 - 1.0;
    float2 cPhaseB = sample4rgb(u_texture4, s4, cPhaseUvB, pxBase * 3.2).rg * 2.0 - 1.0;
    float2 cPhase = cPhaseA * 0.58 + cPhaseB * 0.42;
    float2 cPhaseCross = float2(cPhase.y, -cPhase.x);

    float causticBreath = 0.88 + 0.12 * sin(t * 0.84 + dot(cPhase, float2(1.65, -1.25)));
    float2 cParallax = (cPhase * 0.010 + cPhaseCross * 0.006) * (0.75 + 0.25 * causticBreath);

    float2 cDriftA = float2( 0.052 * t, -0.039 * t) + cPhase * (0.020 * sin(t * 0.51));
    float2 cDriftB = float2(-0.043 * t,  0.045 * t) + cPhaseCross * (0.018 * cos(t * 0.47));
    float2 cDriftC = float2( 0.068 * t, -0.059 * t) + (cPhaseA - cPhaseB) * (0.016 * sin(t * 0.59));

    float2 cUvA = frac((uvFlow + warpHi * 1.2 + cParallax) * (tileCaustic * (0.985 + 0.035 * causticBreath)) + cDriftA);
    float2 cUvB = frac(rotate2((uvFlow - warpHi * 0.9 - cParallax) * (tileCaustic * 1.23 * (1.015 - 0.030 * causticBreath)) + cDriftB, 0.39));
    float2 cUvC = frac((uvFlow + warpHi * 1.7 + cParallax * 1.25) * (tileCaustic * 1.59 * (0.995 + 0.025 * causticBreath)) + cDriftC);

    float cA = sample4gray(u_texture2, s2, cUvA, pxCaustic * 1.05);
    float cB = sample4gray(u_texture2, s2, cUvB, pxCaustic * 1.20);
    float cC = sample4gray(u_texture2, s2, cUvC, pxCaustic * 0.95);

    float causticRaw = saturate(cA * 0.72 + cB * 0.58);
    float causticAA = saturate(fwidth(causticRaw) * 1.2 + 0.010);
    float causticSoft = smoothstep(0.45 - causticAA, 0.72 + causticAA, causticRaw);
    float causticLines = smoothstep(0.62 - causticAA, 0.86 + causticAA, causticRaw);
    float causticSpark = smoothstep(0.95 - causticAA, 0.995, cC);
    float caustic = (causticSoft * 0.30 + causticLines * 0.70 + causticSpark * 0.05) * (0.22 + 0.68 * foamIntensity);
    caustic *= (0.90 + 0.25 * gust);
    caustic *= (0.92 + 0.20 * chop);

    // Foam streaks from alpha channel.
    float2 fUvA = frac(rotate2((uvFlow - warp * 0.55) * tileFoam + float2( 0.056 * t, -0.047 * t),  0.27));
    float2 fUvB = frac(rotate2((uvFlow + warp * 0.80) * (tileFoam * 1.36) + float2(-0.049 * t, 0.055 * t), -0.62));
    float fA = sample4alpha(u_texture3, s3, fUvA, pxDetail * 1.0);
    float fB = sample4alpha(u_texture3, s3, fUvB, pxDetail * 1.2);
    float foamRaw = fA * 0.76 + fB * 0.56;
    float foamStreaks = smoothstep(0.60, 0.93, foamRaw);
    foamStreaks *= (0.08 + 0.35 * causticLines);
    foamStreaks *= (0.12 + 0.62 * foamIntensity);
    foamStreaks *= (0.92 + 0.22 * gust);

    // Pseudo surface normal from wave field + micro variation
    // to produce angle-dependent fresnel/specular response.
    float slopeWaveX = (w1 - w2) * 0.58 + (w3 - w4) * 0.42;
    float slopeWaveY = (w2 + w3) * 0.52 + (w1 - w4) * 0.48;
    float slopeMicroX = ddx(waterL) * 20.0;
    float slopeMicroY = ddy(waterL) * 20.0;
    float slopeScale = 0.22 + waveStrength * 0.72 + chop * 0.35;
    float2 slope = (float2(slopeWaveX, slopeWaveY) + float2(slopeMicroX, slopeMicroY)) * slopeScale;
    float slopeEnergy = saturate(length(slope) * 0.42);

    float3 normalWS = normalize(float3(-slope.x, -slope.y, 1.0));
    float3 viewDir = normalize(float3(0.0, -0.26, 0.97));
    float3 sunDir  = normalize(float3(0.34, -0.48, 0.81));
    float3 halfDir = normalize(viewDir + sunDir);

    float nDotV = saturate(dot(normalWS, viewDir));
    float nDotL = saturate(dot(normalWS, sunDir));
    float nDotH = saturate(dot(normalWS, halfDir));

    float rough = saturate(0.22 + (1.0 - chop) * 0.42 + (1.0 - gust) * 0.18);
    float specTight = pow(nDotH, lerp(320.0, 88.0, rough));
    float specWide  = pow(nDotH, lerp(88.0, 24.0, rough));
    float specular = nDotL * (specTight * (0.24 + 0.76 * chop) + specWide * 0.11);

    float fresnel = 0.020 + (1.0 - 0.020) * pow(1.0 - nDotV, 5.0);
    fresnel = saturate(fresnel + slopeEnergy * 0.06);

    float2 sparkleUv = frac(rotate2(uvFlow * 1.81 + float2(0.113 * t, -0.087 * t), 0.37));
    float sparkleNoise = sample4gray(u_texture4, s4, sparkleUv, pxBase * 0.95);
    float sparkleMask = smoothstep(0.62, 0.96, sparkleNoise + causticSpark * 0.35);

    // Blue legacy palette (kept for the default look).
    float3 deepColorBlue  = float3(0.016, 0.125, 0.290);
    float3 midColorBlue   = float3(0.030, 0.240, 0.470);
    float3 lightColorBlue = float3(0.070, 0.355, 0.595);
    float3 glintColor = float3(0.820, 0.920, 1.000);
    float3 foamColor  = float3(0.900, 0.965, 1.000);
    float neutralColorMode = saturate(params2.x);
    float fresnelStrength  = max(params2.y, 0.0);
    float sunGlintStrength = max(params2.z, 0.0);
    float whitecapBoost    = max(params2.w, 0.0);

    float depthMask = saturate(
        0.18 +
        waterL * 0.52 +
        (waveMix * 0.5 + 0.5) * 0.12 +
        (macroL - 0.5) * 0.18
    );

    float3 oceanBlue = lerp(deepColorBlue, midColorBlue, depthMask);
    oceanBlue = lerp(oceanBlue, lightColorBlue, saturate((waterL - 0.42) * 0.55 + caustic * 0.32));
    oceanBlue = lerp(oceanBlue, lightColorBlue, gust * 0.06);
    oceanBlue *= macroShade;

    float3 skyColor = lerp(float3(0.18, 0.34, 0.55), float3(0.52, 0.72, 0.90), saturate(normalWS.y * 0.5 + 0.5));
    float3 sunColor = float3(1.000, 0.930, 0.780);
    float fresnelBlend = fresnel * (0.44 + 0.56 * causticSoft);
    float glintTerm = specular * sparkleMask;
    float subsurface = pow(saturate(1.0 - nDotL), 1.85) * (0.16 + 0.44 * (1.0 - depthMask)) * (0.22 + 0.55 * waveStrength);

    float3 rgbBlue = waterTex * oceanBlue * (0.86 + 0.32 * waterL);
    float3 causticColorBlue = float3(0.24, 0.64, 0.92);
    rgbBlue = lerp(rgbBlue, causticColorBlue, saturate(caustic * 0.34));
    rgbBlue = lerp(rgbBlue, glintColor, saturate(caustic * 0.10));
    rgbBlue = lerp(rgbBlue, skyColor, saturate(fresnelBlend * (0.16 * fresnelStrength)));
    rgbBlue += sunColor * glintTerm * (0.22 * sunGlintStrength);
    rgbBlue += float3(0.016, 0.130, 0.170) * subsurface;
    rgbBlue *= lerp(0.985, 1.030, gust);

    // Neutral shading path for non-blue water variants (green/brown/red/amber).
    // This avoids multiplying by a blue tint, which can make warm palettes too dark.
    float neutralDepthMask = saturate(0.30 + waterL * 0.56 + (macroL - 0.5) * 0.12);
    float neutralGain = lerp(0.90, 1.18, neutralDepthMask) * macroShade;
    float3 rgbNeutral = waterTex * neutralGain;
    float3 causticColorNeutral = float3(0.92, 0.96, 1.00);
    rgbNeutral = lerp(rgbNeutral, causticColorNeutral, saturate(caustic * 0.26));
    rgbNeutral = lerp(rgbNeutral, glintColor, saturate(caustic * 0.08));
    rgbNeutral = lerp(rgbNeutral, skyColor, saturate(fresnelBlend * (0.11 * fresnelStrength)));
    rgbNeutral += sunColor * glintTerm * (0.17 * sunGlintStrength);
    rgbNeutral += float3(0.055, 0.085, 0.100) * (subsurface * 0.85);
    rgbNeutral *= lerp(0.990, 1.034, gust);

    float3 rgb = lerp(rgbBlue, rgbNeutral, neutralColorMode);

    float crest = smoothstep(0.70, 0.95, (waveMix * 0.5 + 0.5) + (waterL - 0.5) * 0.20);
    float whitecaps = smoothstep(0.36, 0.92, slopeEnergy * (1.05 + 0.55 * waveStrength) + causticLines * 0.18);
    whitecaps *= (0.10 + 0.52 * gust) * (0.40 + 0.60 * foamIntensity) * whitecapBoost;

    float foam = crest * (0.02 + 0.06 * foamIntensity) + foamStreaks * 0.15;
    foam += chop * 0.035 * foamIntensity;
    foam += whitecaps * 0.20;
    rgb = lerp(rgb, foamColor, saturate(foam));

    // Slow broad swell modulation for visible movement without pixel sparkle.
    float swell = 0.5 + 0.5 * sin((uvFlow.x * 2.3 + uvFlow.y * 1.7) + t * 0.55);
    rgb *= lerp(0.96, 1.04, swell);

    rgb = saturate(rgb) * input.v_color.rgb;
    o.o_color = float4(rgb, input.v_color.a);
    return o;
}
