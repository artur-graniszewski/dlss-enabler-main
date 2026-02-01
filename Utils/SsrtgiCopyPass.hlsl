#include "SsrtgiCommon.hlsl"

// Combined copy pass - copies both depth history and color output
// Color copy may be skipped if CopyResource was used (but shader still runs for depth)

Texture2D<float4> gColorComposite : register(t2); // composite result (source)
RWTexture2D<float4> gColorOut : register(u0); // final output (destination)
RWTexture2D<float> gDepthHistOut : register(u2); // depth history (destination)

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 p = dtid.xy;
    if (p.x >= g.Width || p.y >= g.Height)
        return;
    
    // Copy color: composite -> output
    float4 color = gColorComposite.Load(int3(p, 0));
    gColorOut[p] = color;
    
    // Copy depth: linearize and write to history buffer
    float depth = gDepth.Load(int3(p, 0));
    gDepthHistOut[p] = depth;
}
