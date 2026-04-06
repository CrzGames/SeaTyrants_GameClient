cbuffer Context : register(b0, space3)
{
    // params0: x=time, y=noiseScale, z=driftSpeed, w=cloudCoverage
    float4 params0;
    // params1: x=shadowStrength, y=alphaMax, z=edgeSoftness, w=blackCutoff
    float4 params1;
    // params2: x=viewRectX, y=viewRectY, z=viewRectW, w=viewRectH
    float4 params2;
    // params3: x=mapOriginX, y=mapOriginY, z=tileWidthPx, w=tileHeightPx
    float4 params3;
    // params4: x=playerTileX, y=playerTileY, z=viewRangeTiles, w=viewFalloffTiles
    float4 params4;
    // params5: x=shadowTintR, y=shadowTintG, z=shadowTintB, w=reserved
    float4 params5;
};

// Auto-bound by SDL_RenderTexture: cloud noise texture.
Texture2D    u_texture0 : register(t0, space2);
SamplerState s0         : register(s0, space2);

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

float hash12(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float2 hash22(float2 p)
{
    return float2(
        hash12(p + float2(17.7, 9.2)),
        hash12(p + float2(39.1, 73.4)));
}

void evaluateCloudSampleFromCell(
    float2 cellId,
    float2 uvLocal,
    float time,
    float blackCutoff,
    out float outCloudValue,
    out float outCloudPresence)
{
    // Full-texture sampling per group (no crop / no diagonal clipping).
    float flipX = step(0.5, hash12(cellId + float2(21.0, 57.0)));
    float flipY = step(0.5, hash12(cellId + float2(41.3, 8.4)));
    float swapUv = step(0.5, hash12(cellId + float2(63.7, 12.6)));
    float2 uvMirror = float2(
        lerp(uvLocal.x, 1.0 - uvLocal.x, flipX),
        lerp(uvLocal.y, 1.0 - uvLocal.y, flipY));
    // Clamp UV to avoid sampler wrap artifacts when shadow sampling shifts outside local cell.
    float2 uvSample = saturate(lerp(uvMirror, float2(uvMirror.y, uvMirror.x), swapUv));

    // User texture rule:
    // - pure black => no cloud
    // - any non-black color => cloud
    float3 cloudTex = u_texture0.Sample(s0, uvSample).rgb;
    float cloudValue = max(cloudTex.r, max(cloudTex.g, cloudTex.b));

    float uvEdge = min(
        min(uvSample.x, uvSample.y),
        min(1.0 - uvSample.x, 1.0 - uvSample.y));
    float edgeFade = smoothstep(0.02, 0.08, uvEdge);
    cloudValue *= edgeFade;

    float breathPhase = hash12(cellId + float2(81.2, 14.7)) * 6.2831853;
    float cloudThreshold = max(blackCutoff, 0.03);
    float edgeBandIn = smoothstep(cloudThreshold - 0.10, cloudThreshold + 0.03, cloudValue);
    float edgeBandOut = smoothstep(cloudThreshold + 0.03, cloudThreshold + 0.16, cloudValue);
    float contourBand = saturate(edgeBandIn - edgeBandOut);

    float breathT = (time * 0.95) + breathPhase;
    float2 contourOffset = float2(
        sin((uvSample.y * 22.0) + breathT),
        cos((uvSample.x * 21.0) - (breathT * 0.9)));
    contourOffset *= 0.012;

    float2 uvWarped = clamp(uvSample + contourOffset, 0.0, 1.0);
    float3 cloudTexWarped = u_texture0.Sample(s0, uvWarped).rgb;
    float cloudValueWarped = max(cloudTexWarped.r, max(cloudTexWarped.g, cloudTexWarped.b));
    cloudValueWarped *= edgeFade;

    float cloudValueAnimated = lerp(cloudValue, cloudValueWarped, 0.72 * contourBand);

    float contourField =
        (sin((uvSample.x * 24.0) + (time * 1.18) + breathPhase) * 0.5) +
        (sin((uvSample.y * 20.0) - (time * 1.03) + (breathPhase * 1.37)) * 0.5);
    float contourShift = contourField * 0.034 * contourBand;
    float cloudThresholdAnimated = cloudThreshold + contourShift;

    outCloudValue = cloudValueAnimated;
    outCloudPresence = smoothstep(
        cloudThresholdAnimated,
        cloudThresholdAnimated + 0.045,
        cloudValueAnimated);
}

PSOutput main(PSInput input)
{
    PSOutput o;

    float time          = params0.x;
    float noiseScale    = max(params0.y, 0.001);
    float driftSpeed    = max(params0.z, 0.0);
    float cloudCoverage = saturate(params0.w);

    float shadowStrength = saturate(params1.x);
    float alphaMax       = saturate(params1.y);
    float edgeSoftness   = max(params1.z, 0.001);
    float blackCutoff    = saturate(params1.w);

    float3 shadowTint = saturate(params5.xyz);

    float2 screenUv = input.v_uv;
    float2 screenPx = float2(
        params2.x + (screenUv.x * params2.z),
        params2.y + (screenUv.y * params2.w));
    float halfTileW = max(params3.z * 0.5, 0.001);
    float halfTileH = max(params3.w * 0.5, 0.001);
    float isoDx = (screenPx.x - params3.x) / halfTileW;
    float isoDy = (screenPx.y - params3.y) / halfTileH;
    float tileX = 0.5 * (isoDx + isoDy);
    float tileY = 0.5 * (isoDy - isoDx);
    float2 tilePos = float2(tileX, tileY);

    // Restrict clouds to player's visible range.
    float2 playerTile = params4.xy;
    float viewRangeTiles = max(params4.z, 0.0);
    float viewFalloffTiles = max(params4.w, 0.001);
    float revealStart = max(viewRangeTiles - viewFalloffTiles, 0.0);
    float revealEnd = viewRangeTiles + viewFalloffTiles;
    float distToPlayer = distance(float2(tileX, tileY), playerTile);
    float insideMask = 1.0 - smoothstep(revealStart, revealEnd, distToPlayer);
    insideMask = pow(saturate(insideMask), edgeSoftness);

    // Move clouds in world-space.
    float t = time * max(driftSpeed, 0.05);
    float2 baseWindDir = normalize(float2(0.92, -0.39));

    // Sparse world cells:
    // - more cloud groups can exist across the world map
    // - but density stays controlled (no tile-like mosaic everywhere)
    float densityScale = max(noiseScale, 0.001);
    float groupScale = 2.0;
    float cellSizeTiles = max(72.0, 96.0 / densityScale) / max(groupScale, 0.001);

    // Smooth world wind field: different headings across the map, no hard region seams.
    float windAngle =
        (sin(dot(tilePos, float2(0.0042, -0.0031))) * 0.70) +
        (cos(dot(tilePos, float2(-0.0026, 0.0037))) * 0.45);
    float2 windDir = normalize(rotate2(baseWindDir, windAngle));
    float cloudDriftTiles = t * 3.0;

    float2 advectedTilePos = tilePos - (windDir * cloudDriftTiles);
    float2 cellCoord = advectedTilePos / cellSizeTiles;
    float2 cellId = floor(cellCoord);
    float2 uvLocal = frac(cellCoord);

    // Around 2x less density than the old (0.18..0.34) rule.
    float densityProb = lerp(0.05, 0.10, cloudCoverage);
    float cellRnd = hash12(cellId + float2(3.7, 6.1));
    float cellActive = step(cellRnd, densityProb);

    float mainCloudValue = 0.0;
    float mainCloudPresence = 0.0;
    evaluateCloudSampleFromCell(
        cellId,
        uvLocal,
        time,
        blackCutoff,
        mainCloudValue,
        mainCloudPresence);

    // True projected shadow: evaluate cloud field at offset coordinates.
    float2 shadowOffsetTiles = float2(-0.20000, 10.00);
    float2 shadowOffsetCell = shadowOffsetTiles / max(cellSizeTiles, 0.001);
    float2 shadowCellCoord = cellCoord - shadowOffsetCell;
    float2 shadowCellId = floor(shadowCellCoord);
    float2 uvLocalShadow = frac(shadowCellCoord);
    float shadowCellRnd = hash12(shadowCellId + float2(3.7, 6.1));
    float shadowCellActive = step(shadowCellRnd, densityProb);

    float shadowCloudValue = 0.0;
    float shadowCloudPresence = 0.0;
    evaluateCloudSampleFromCell(
        shadowCellId,
        uvLocalShadow,
        time,
        blackCutoff,
        shadowCloudValue,
        shadowCloudPresence);

    float breathPhase = hash12(cellId + float2(81.2, 14.7)) * 6.2831853;
    float breathWave = sin((time * 0.62) + breathPhase);
    float breath01 = (breathWave * 0.5) + 0.5;

    float mainMask = saturate(mainCloudPresence * cellActive * insideMask);
    float shadowMask = saturate(shadowCloudPresence * shadowCellActive * insideMask);

    // Zero shadow on cloud body.
    float cloudPixelMask = step(0.00001, mainMask);
    float shadowMaskOnly = saturate(shadowMask * (1.0 - cloudPixelMask));

    float mainMaskDense = smoothstep(0.10, 0.45, mainMask);
    float alphaCloud = saturate(mainMaskDense * shadowStrength * alphaMax);
    alphaCloud *= lerp(0.93, 1.07, breath01);
    float alphaShadow = saturate(shadowMaskOnly * shadowStrength * alphaMax * 0.30);

    // Make clouds read as an overhead layer (bright cloud body, not dark shadow).
    float3 cloudTint = lerp(shadowTint, float3(0.96, 0.97, 0.99), 0.95);
    float3 cloudColor = cloudTint * lerp(0.98, 1.02, breath01);
    cloudColor *= lerp(0.97, 1.04, breath01);

    float3 shadowColor = lerp(float3(0.05, 0.07, 0.09), shadowTint, 0.30);

    float alpha = saturate(alphaCloud + (alphaShadow * (1.0 - alphaCloud)));
    float3 colorPM =
        (shadowColor * alphaShadow * (1.0 - alphaCloud)) +
        (cloudColor * alphaCloud);
    float3 color = (alpha > 0.0001) ? (colorPM / alpha) : cloudColor;

    o.o_color = float4(saturate(color * input.v_color.rgb), alpha * input.v_color.a);
    return o;
}
