// Display-resolution HUD for the Q4 capture harness.
//
// This pass runs after reconstruction and tone mapping, writes the display
// target directly, and works in integer display pixels. It samples nothing:
// there is no scene texture, no depth, no fog and no upscale, so HUD texels are
// one-to-one with display pixels and the overlay cannot enter scene history.

cbuffer Hud : register(b0)
{
    float2 extent;  // display width/height in pixels
    float time;     // frame index (for a blinking element)
    float mode;     // 1 = HUD enabled
};

struct VSOutput
{
    float4 position : SV_Position;
};

VSOutput HudVS(uint vertexId : SV_VertexID)
{
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    VSOutput output;
    output.position =
        float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float InRect(float2 p, float2 lo, float2 hi)
{
    return (p.x >= lo.x && p.x < hi.x && p.y >= lo.y && p.y < hi.y) ? 1.0 : 0.0;
}

float4 PSHud(VSOutput input) : SV_Target
{
    if (mode < 0.5)
    {
        discard;
    }
    float2 p = input.position.xy;
    float4 color = 0.0;
    const float3 cyan = float3(0.15, 0.85, 1.0);
    const float3 amber = float3(1.0, 0.62, 0.12);

    // Bottom-left: health plate with an integer-pixel border and cells.
    float2 healthLo = float2(24.0, extent.y - 58.0);
    float2 healthHi = float2(238.0, extent.y - 24.0);
    if (InRect(p, healthLo, healthHi) > 0.5)
    {
        bool border = p.x < healthLo.x + 2.0 || p.x >= healthHi.x - 2.0 ||
                      p.y < healthLo.y + 2.0 || p.y >= healthHi.y - 2.0;
        color = border ? float4(cyan, 0.9) : float4(0.02, 0.10, 0.14, 0.55);
        // Ten 20-pixel cells, eight filled.
        float cellIndex = floor((p.x - healthLo.x - 8.0) / 20.0);
        if (!border && p.x > healthLo.x + 8.0 && p.x < healthHi.x - 8.0 &&
            p.y > healthLo.y + 8.0 && p.y < healthHi.y - 8.0 &&
            cellIndex >= 0.0 && cellIndex < 10.0 &&
            p.x - healthLo.x - 8.0 - cellIndex * 20.0 > 3.0)
        {
            if (cellIndex < 8.0)
            {
                color = float4(cyan * 0.85, 0.92);
            }
        }
    }

    // Bottom-right: three ammo blocks; the third blinks on an integer cadence.
    for (int block = 0; block < 3; ++block)
    {
        float2 lo = float2(extent.x - 190.0 + block * 56.0, extent.y - 54.0);
        float2 hi = lo + float2(44.0, 26.0);
        if (InRect(p, lo, hi) > 0.5)
        {
            bool filled = block < 2 || fmod(floor(time), 2.0) < 0.5;
            bool border = p.x < lo.x + 2.0 || p.x >= hi.x - 2.0 ||
                          p.y < lo.y + 2.0 || p.y >= hi.y - 2.0;
            color = border ? float4(amber, 0.9)
                           : (filled ? float4(amber * 0.55, 0.55)
                                     : float4(0.02, 0.06, 0.08, 0.45));
        }
    }

    // Top-left: four integer meter rows.
    for (int row = 0; row < 4; ++row)
    {
        float2 lo = float2(24.0, 24.0 + row * 14.0);
        float2 hi = lo + float2(160.0, 8.0);
        if (InRect(p, lo, hi) > 0.5)
        {
            float fill = 0.35 + 0.15 * row;
            color = (p.x - lo.x) < 160.0 * fill ? float4(cyan * 0.75, 0.8)
                                                 : float4(0.03, 0.08, 0.10, 0.4);
        }
    }

    // Top-right: compass ticks, one pixel wide.
    for (int tick = 0; tick < 9; ++tick)
    {
        float x = extent.x - 200.0 + tick * 22.0;
        float height = (tick == 4) ? 14.0 : 7.0;
        if (p.x >= x && p.x < x + 1.0 && p.y >= 24.0 &&
            p.y < 24.0 + height)
        {
            color = float4(cyan, 0.85);
        }
    }

    // Centre crosshair: four one-pixel arms with a gap.
    float2 centre = floor(extent * 0.5);
    bool horizontal = (p.y >= centre.y && p.y < centre.y + 1.0) &&
                      ((p.x >= centre.x - 9.0 && p.x < centre.x - 3.0) ||
                       (p.x >= centre.x + 3.0 && p.x < centre.x + 9.0));
    bool vertical = (p.x >= centre.x && p.x < centre.x + 1.0) &&
                    ((p.y >= centre.y - 9.0 && p.y < centre.y - 3.0) ||
                     (p.y >= centre.y + 3.0 && p.y < centre.y + 9.0));
    if (horizontal || vertical)
    {
        color = float4(cyan, 0.95);
    }

    if (color.a <= 0.0)
    {
        discard;
    }
    return color;
}
