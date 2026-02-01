#include "SsrtgiCommon.hlsl"

// ============================================================
// HALF-RES DENOISE - PASS 1: DOWNSAMPLE + BILATERAL
// ============================================================
// Runs at half resolution
// Input: gHistory (t2) - full-res gather output (bound as SRV)
// Output: gOut1 (u1) - half-res buffer
// ============================================================

// Kernel size selection: 0 = 5x5 (radius 2), 1 = 7x7 (radius 3)
#define USE_7X7_KERNEL 1

#if USE_7X7_KERNEL
#define KERNEL_RADIUS 3     // 7x7 kernel = effective 14x14 at full-res
#else
#define KERNEL_RADIUS 2     // 5x5 kernel = effective 10x10 at full-res
#endif

#define DEPTH_SENSITIVITY 50.0
#define COLOR_SENSITIVITY 3.0

// Gaussian weight calculation - works for any radius
float GetGaussWeight(int dx, int dy)
{
    // Sigma roughly radius/2 for good falloff
    float sigma = float(KERNEL_RADIUS) * 0.5;
    float sigma2 = sigma * sigma;
    float dist2 = float(dx * dx + dy * dy);
    return exp(-dist2 / (2.0 * sigma2));
}

float DepthWeight(float dz)
{
    return rcp(1.0 + DEPTH_SENSITIVITY * dz);
}

float ColorWeight(float lumDiff)
{
    return rcp(1.0 + COLOR_SENSITIVITY * lumDiff);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    // Output is half resolution
    uint2 halfP = dtid.xy;
    
    uint fullW = g.Width;
    uint fullH = g.Height;
    uint halfW = (fullW + 1) / 2;
    uint halfH = (fullH + 1) / 2;
    
    if (halfP.x >= halfW || halfP.y >= halfH)
        return;
    
    // Map to full-res center (2x2 block center)
    uint2 fullP = halfP * 2;
    
    // Get center depth (best of 2x2 block - closest non-sky)
    float depths2x2[4];
    float4 colors2x2[4];
    
    [unroll]
    for (int dy = 0; dy < 2; dy++)
    {
        [unroll]
        for (int dx = 0; dx < 2; dx++)
        {
            uint2 fp = min(fullP + uint2(dx, dy), uint2(fullW - 1, fullH - 1));
            int idx = dy * 2 + dx;
            // Read from gHistory (t2) - bound as SRV
            colors2x2[idx] = gHistory.Load(int3(fp, 0));
            depths2x2[idx] = gDepth.Load(int3(MapToDepthCoord(fp), 0));
        }
    }
    
    // Find best center (closest to camera, non-sky)
    float centerDepth = 1.0;
    float4 centerColor = colors2x2[0];
    int validCount = 0;
    
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        if (!IsInvalidOrSkyDepth(depths2x2[i]))
        {
            if (depths2x2[i] < centerDepth)
            {
                centerDepth = depths2x2[i];
                centerColor = colors2x2[i];
            }
            validCount++;
        }
    }
    
    // Sky check
    if (validCount == 0)
    {
        gOut1[halfP] = centerColor;
        return;
    }
    
    float centerLinDepth = LinearizeDepth(centerDepth);
    float centerLum = dot(centerColor.rgb, float3(0.299, 0.587, 0.114));
    
    // ============================================
    // BILATERAL FILTER AT HALF-RES
    // ============================================
    // 5x5 at half-res = 10x10 at full-res
    // 7x7 at half-res = 14x14 at full-res
    
    float3 sumGI = float3(0, 0, 0);
    float sumAO = 0.0;
    float sumWeight = 0.0;
    
    [unroll]
    for (int ky = -KERNEL_RADIUS; ky <= KERNEL_RADIUS; ky++)
    {
        [unroll]
        for (int kx = -KERNEL_RADIUS; kx <= KERNEL_RADIUS; kx++)
        {
            // Map kernel offset to full-res (step by 2)
            int2 sampleFullP = int2(fullP) + int2(kx * 2, ky * 2);
            sampleFullP = clamp(sampleFullP, int2(0, 0), int2(fullW - 1, fullH - 1));
            
            // Read from gHistory (t2) - bound as SRV
            float4 sampleColor = gHistory.Load(int3(sampleFullP, 0));
            float sampleDepth = gDepth.Load(int3(MapToDepthCoord(uint2(sampleFullP)), 0));
            
            if (IsInvalidOrSkyDepth(sampleDepth))
                continue;
            
            float sampleLinDepth = LinearizeDepth(sampleDepth);
            
            // Spatial weight (Gaussian)
            float wS = GetGaussWeight(kx, ky);
            
            // Depth weight
            float dz = abs(centerLinDepth - sampleLinDepth) / max(centerLinDepth, 0.001);
            float wZ = DepthWeight(dz);
            
            // Color weight
            float sampleLum = dot(sampleColor.rgb, float3(0.299, 0.587, 0.114));
            float lumDiff = abs(centerLum - sampleLum);
            float wC = ColorWeight(lumDiff);
            
            float weight = wS * wZ * wC;
            
            sumGI += sampleColor.rgb * weight;
            sumAO += sampleColor.a * weight;
            sumWeight += weight;
        }
    }
    
    float invWeight = (sumWeight > 0.0001) ? rcp(sumWeight) : 1.0;
    gOut1[halfP] = float4(sumGI * invWeight, sumAO * invWeight);
}
