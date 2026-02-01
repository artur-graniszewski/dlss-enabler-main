#include "SsrtgiCommon.hlsl"

// ============================================================
// HALF-RES DENOISE - PASS 2: EDGE-AWARE UPSCALE
// ============================================================
// Runs at full resolution
// Input: gHistory (t2) = half-res denoised buffer (from pass 1, bound as SRV)
// Output: gOut1 (u1) = full-res result
// Uses depth-weighted bilinear for sharp edges
// ============================================================

#define DEPTH_SENSITIVITY 100.0  // Stricter for upscale

float DepthWeight(float dz)
{
    return rcp(1.0 + DEPTH_SENSITIVITY * dz);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 fullP = dtid.xy;
    
    uint fullW = g.Width;
    uint fullH = g.Height;
    
    // Get actual half-res dimensions from the texture itself
    uint halfW, halfH;
    gHistory.GetDimensions(halfW, halfH);
    
    if (fullP.x >= fullW || fullP.y >= fullH)
        return;
    
    // Get full-res depth for edge-aware upscale
    float fullDepth = gDepth.Load(int3(MapToDepthCoord(fullP), 0));
    
    if (IsInvalidOrSkyDepth(fullDepth))
    {
        // Sky: just sample nearest half-res
        uint2 halfP = min(fullP / 2, uint2(halfW - 1, halfH - 1));
        gOut1[fullP] = gHistory.Load(int3(halfP, 0));
        return;
    }
    
    float fullLinDepth = LinearizeDepth(fullDepth);
    
    // Map full-res pixel to half-res continuous coords
    // fullP / 2.0 gives us position in half-res space
    float2 halfCoord = (float2(fullP) + 0.5) / 2.0 - 0.5;
    int2 halfBase = int2(floor(halfCoord));
    float2 frac_part = frac(halfCoord);
    
    // Sample 4 half-res neighbors with depth-aware weights
    float4 result = float4(0, 0, 0, 0);
    float totalWeight = 0.0;
    
    [unroll]
    for (int dy = 0; dy < 2; dy++)
    {
        [unroll]
        for (int dx = 0; dx < 2; dx++)
        {
            int2 hp = clamp(halfBase + int2(dx, dy), int2(0, 0), int2(halfW - 1, halfH - 1));
            
            // Sample half-res color from gHistory (t2)
            float4 halfColor = gHistory.Load(int3(hp, 0));
            
            // Get depth at corresponding full-res position (center of 2x2 block)
            uint2 corrFullP = min(uint2(hp) * 2, uint2(fullW - 1, fullH - 1));
            float halfDepth = gDepth.Load(int3(MapToDepthCoord(corrFullP), 0));
            
            // Bilinear weight
            float bx = (dx == 0) ? (1.0 - frac_part.x) : frac_part.x;
            float by = (dy == 0) ? (1.0 - frac_part.y) : frac_part.y;
            float bilinearW = bx * by;
            
            // Depth weight for edge-aware upscale
            float depthW = 1.0;
            if (!IsInvalidOrSkyDepth(halfDepth))
            {
                float halfLinDepth = LinearizeDepth(halfDepth);
                float dz = abs(fullLinDepth - halfLinDepth) / max(fullLinDepth, 0.001);
                depthW = DepthWeight(dz);
            }
            else
            {
                // Sky neighbor - reduce weight significantly
                depthW = 0.1;
            }
            
            float weight = bilinearW * depthW;
            result += halfColor * weight;
            totalWeight += weight;
        }
    }
    
    // Normalize
    if (totalWeight > 0.0001)
    {
        result *= rcp(totalWeight);
    }
    else
    {
        // Fallback: nearest neighbor
        uint2 nearestHalf = min(fullP / 2, uint2(halfW - 1, halfH - 1));
        result = gHistory.Load(int3(nearestHalf, 0));
    }
    
    gOut1[fullP] = result;
}
