#include "SsrtgiCommon.hlsl"

// Depth-only copy pass
// Used when CopyResource handles color (fast path)
// Only linearizes and copies depth to history

RWTexture2D<float> gDepthHistOut : register(u2);

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 p = dtid.xy;
    if (p.x >= g.Width || p.y >= g.Height)
        return;
    
    // Copy depth: linearize and write to history buffer
    float depth = gDepth.Load(int3(p, 0));
    gDepthHistOut[p] = depth;
}
