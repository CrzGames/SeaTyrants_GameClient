// fogofwar.fragment — simple: sample texture only, no effects

// p0.x = tiling (1 = 1:1, >1 = répète davantage)
// p0.y = opacity (0..1)
// p0.z,w = unused
// p3.rgb = tint (1,1,1 pour aucune teinte)
cbuffer Params : register(b0, space3)
{
    float4 p0;    // tiling, opacity, -, -
    float4 p1;    // unused
    float4 p2;    // unused
    float4 p3;    // tint.r, tint.g, tint.b, -
};

Texture2D    u_tex  : register(t1, space2);
SamplerState u_samp : register(s1, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv    : TEXCOORD0; // 0..1 sur mapRect
};

float4 main(PSInput IN) : SV_Target
{
    float tiling  = max(p0.x, 0.0001);
    float opacity = saturate(p0.y);

    // UV * tiling -> répétition si le sampler est en REPEAT
    float2 uv = IN.v_uv * tiling;

    float4 tex = u_tex.Sample(u_samp, uv);

    // teinte optionnelle (p3.rgb) — mettre (1,1,1) pour neutre
    float3 color = tex.rgb * p3.rgb;
    float  alpha = tex.a * opacity;

    return float4(color, alpha); // non prémultiplié
}
