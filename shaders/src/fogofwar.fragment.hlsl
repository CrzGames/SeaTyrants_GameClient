cbuffer Context : register(b0, space3)
{
    // params0: x=time, y=noiseScale, z=driftSpeed, w=fogIntensity
    float4 params0;
    // params1: x=revealMin, y=revealMax, z=edgeBoost, w=noiseContrast
    float4 params1;
    // params2: x=tintR, y=tintG, z=tintB, w=alphaMax
    float4 params2;
};

// Auto-bound by SDL_RenderTexture:
// visibility mask texture (0=hidden, 1=visible in RGB or A).
Texture2D    u_texture0 : register(t0, space2);
SamplerState s0         : register(s0, space2);

// Additional sampler from SDL_GPURenderState:
// fog noise texture (for example assets/images/cloud-noise.png).
Texture2D    u_texture1 : register(t1, space2);
SamplerState s1         : register(s1, space2);

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

float sample4gray(Texture2D tex, SamplerState smp, float2 uv, float2 px)
{
    float a = lum(tex.Sample(smp, frac(uv + float2(-px.x, -px.y))).rgb);
    float b = lum(tex.Sample(smp, frac(uv + float2( px.x, -px.y))).rgb);
    float c = lum(tex.Sample(smp, frac(uv + float2(-px.x,  px.y))).rgb);
    float d = lum(tex.Sample(smp, frac(uv + float2( px.x,  px.y))).rgb);
    return (a + b + c + d) * 0.25;
}

PSOutput main(PSInput input)
{
    PSOutput o;

    float time         = params0.x;
    float noiseScale   = max(params0.y, 0.001);
    float driftSpeed   = max(params0.z, 0.0);
    float fogIntensity = saturate(params0.w);

    float revealMin    = saturate(params1.x);
    float revealMax    = max(revealMin + 0.001, saturate(params1.y));
    float edgeBoost    = saturate(params1.z);
    float noiseContrast = max(params1.w, 0.001);

    float3 fogTint = saturate(params2.xyz);
    float alphaMax = saturate(params2.w);

    float2 uv = input.v_uv;
    float t = time * max(driftSpeed, 0.18);

    // Animate the visibility mask itself so cloud masses drift across the map.
    float2 maskUvA = frac(rotate2(uv * (noiseScale * 0.72) + float2(0.022 * t, -0.015 * t), 0.19));
    float2 maskUvB = frac(rotate2(uv * (noiseScale * 0.49) + float2(-0.013 * t, 0.019 * t), -0.54));
    float4 visibilitySample = lerp(u_texture0.Sample(s0, maskUvA), u_texture0.Sample(s0, maskUvB), 0.42);
    float visibilityFromColor = lum(visibilitySample.rgb);
    // Many authored grayscale masks are fully opaque (alpha=1 everywhere).
    // In that case alpha should not override the visibility signal from RGB.
    float useAlphaMask = step(visibilitySample.a, 0.999);
    float visibilityRaw = lerp(visibilityFromColor, visibilitySample.a, useAlphaMask);
    float visibility = smoothstep(revealMin, revealMax, saturate(visibilityRaw));
    float hidden = 1.0 - visibility;

    float2 px = float2(1.0 / 1024.0, 1.0 / 1024.0);
    float2 nUvA = frac(rotate2(uv * noiseScale + float2(0.030 * t, -0.022 * t),  0.41));
    float2 nUvB = frac(rotate2(uv * (noiseScale * 1.71) + float2(-0.020 * t, 0.026 * t), -0.93));

    float nA = sample4gray(u_texture1, s1, nUvA, px * 1.00);
    float nB = sample4gray(u_texture1, s1, nUvB, px * 1.25);
    float fogNoise = saturate(lerp(nA, nB, 0.46));
    fogNoise = pow(saturate((fogNoise - 0.5) * noiseContrast + 0.5), 1.05);

    float wisps = smoothstep(0.30, 0.82, fogNoise);
    float hollow = smoothstep(0.58, 0.95, fogNoise);

    float baseAlpha = hidden * lerp(0.24, 0.78, wisps) * fogIntensity;

    // Highlight transition zone between visible and hidden areas.
    float edgeBand = pow(saturate(visibility * (1.0 - visibility) * 4.0), 0.72);
    float edgeAlpha = edgeBand * edgeBoost * (0.40 + 0.60 * hollow) * fogIntensity;

    float alpha = saturate((baseAlpha + edgeAlpha) * max(alphaMax, 0.001));

    float fogLum = lerp(0.92, 1.18, wisps);
    float3 fogColor = fogTint * fogLum;
    fogColor = lerp(fogColor, fogColor + float3(0.05, 0.06, 0.07), edgeBand * 0.35);

    fogColor *= input.v_color.rgb;
    alpha *= input.v_color.a;

    o.o_color = float4(saturate(fogColor), alpha);
    return o;
}
