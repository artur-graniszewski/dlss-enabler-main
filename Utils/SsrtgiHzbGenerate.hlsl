// HZB Generation - Single Pass Downsampler (SPD) version
// Generates ALL mip levels in a single dispatch using groupshared memory
// Much faster than multi-pass approach

cbuffer HzbParams : register(b0)
{
    uint Width; // Full resolution width
    uint Height; // Full resolution height
    uint NumMips; // Number of mip levels to generate
    uint Pad0;
    float DepthInverted; // 1.0 = reversed-Z
    float CameraNear;
    float CameraFar;
    uint Pad1;
};

Texture2D<float> gDepthSrc : register(t0);

// UAVs for each mip level (up to 6 mips in single pass)
RWTexture2D<float2> gHzbMip0 : register(u0);
RWTexture2D<float2> gHzbMip1 : register(u1);
RWTexture2D<float2> gHzbMip2 : register(u2);
RWTexture2D<float2> gHzbMip3 : register(u3);
RWTexture2D<float2> gHzbMip4 : register(u4);
RWTexture2D<float2> gHzbMip5 : register(u5);

// Groupshared memory for hierarchical reduction
// 32x32 covers mip0 output from a 64x64 region of depth buffer
groupshared float2 gs_HZB[32][32];

// Constants
static const float HZB_FAR_VALUE = 10000.0;
static const float HZB_NEAR_VALUE = 0.001;

// Check if depth value represents sky/invalid
bool IsInvalidDepth(float d)
{
    if (DepthInverted > 0.5)
        return d <= 0.0001;
    else
        return d >= 0.9999;
}

// Convert raw depth to linear viewZ
float LinearizeDepth(float d)
{
    float n = max(CameraNear, 0.01);
    float f = max(CameraFar, n + 1.0);
    
    d = saturate(d);
    d = max(d, 0.0001);
    
    float linearZ;
    if (DepthInverted > 0.5)
        linearZ = (f * n) / (n + d * (f - n));
    else
        linearZ = (f * n) / (f - d * (f - n));
    
    return clamp(linearZ, HZB_NEAR_VALUE, HZB_FAR_VALUE);
}

// Reduce 4 depth samples to min/max
float2 Reduce4(float2 a, float2 b, float2 c, float2 d)
{
    float minZ = min(min(a.x, b.x), min(c.x, d.x));
    float maxZ = max(max(a.y, b.y), max(c.y, d.y));
    return float2(minZ, maxZ);
}

// Load and linearize depth, return (minZ, maxZ) for single sample
float2 LoadDepthAsMinMax(int2 coord)
{
    coord = clamp(coord, int2(0, 0), int2(Width - 1, Height - 1));
    float d = gDepthSrc.Load(int3(coord, 0));
    
    if (IsInvalidDepth(d))
        return float2(HZB_FAR_VALUE, HZB_FAR_VALUE);
    
    float linearZ = LinearizeDepth(d);
    return float2(linearZ, linearZ);
}

// Write to appropriate mip UAV
void WriteMip(uint mip, uint2 coord, float2 value)
{
    switch (mip)
    {
        case 0:
            gHzbMip0[coord] = value;
            break;
        case 1:
            gHzbMip1[coord] = value;
            break;
        case 2:
            gHzbMip2[coord] = value;
            break;
        case 3:
            gHzbMip3[coord] = value;
            break;
        case 4:
            gHzbMip4[coord] = value;
            break;
        case 5:
            gHzbMip5[coord] = value;
            break;
    }
}

// 64x64 threads per group - each thread handles one pixel of mip 0
// Then threads cooperate to build higher mips
[numthreads(32, 32, 1)]
void main(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    // ===========================================
    // MIP 0: Each thread reduces 2x2 depth to one HZB pixel
    // ===========================================
    uint2 mip0Coord = gid.xy * 32 + gtid.xy;
    uint2 depthCoord = mip0Coord * 2;
    
    // Calculate mip 0 dimensions
    uint mip0W = (Width + 1) / 2;
    uint mip0H = (Height + 1) / 2;
    
    float2 result;
    
    if (mip0Coord.x < mip0W && mip0Coord.y < mip0H)
    {
        // Load 4 depth samples
        float2 d0 = LoadDepthAsMinMax(depthCoord + int2(0, 0));
        float2 d1 = LoadDepthAsMinMax(depthCoord + int2(1, 0));
        float2 d2 = LoadDepthAsMinMax(depthCoord + int2(0, 1));
        float2 d3 = LoadDepthAsMinMax(depthCoord + int2(1, 1));
        
        result = Reduce4(d0, d1, d2, d3);
        
        // Write mip 0
        WriteMip(0, mip0Coord, result);
    }
    else
    {
        result = float2(HZB_FAR_VALUE, HZB_FAR_VALUE);
    }
    
    // Store in groupshared for subsequent mips
    gs_HZB[gtid.y][gtid.x] = result;
    GroupMemoryBarrierWithGroupSync();
    
    // ===========================================
    // MIP 1: 16x16 threads active (every other thread)
    // ===========================================
    if (NumMips < 2)
        return;
    
    if ((gtid.x & 1) == 0 && (gtid.y & 1) == 0)
    {
        uint2 idx = gtid.xy;
        float2 v0 = gs_HZB[idx.y + 0][idx.x + 0];
        float2 v1 = gs_HZB[idx.y + 0][idx.x + 1];
        float2 v2 = gs_HZB[idx.y + 1][idx.x + 0];
        float2 v3 = gs_HZB[idx.y + 1][idx.x + 1];
        
        result = Reduce4(v0, v1, v2, v3);
        gs_HZB[gtid.y][gtid.x] = result;
        
        uint2 mip1Coord = gid.xy * 16 + gtid.xy / 2;
        uint mip1W = (mip0W + 1) / 2;
        uint mip1H = (mip0H + 1) / 2;
        
        if (mip1Coord.x < mip1W && mip1Coord.y < mip1H)
            WriteMip(1, mip1Coord, result);
    }
    GroupMemoryBarrierWithGroupSync();
    
    // ===========================================
    // MIP 2: 8x8 threads active
    // ===========================================
    if (NumMips < 3)
        return;
    
    if ((gtid.x & 3) == 0 && (gtid.y & 3) == 0)
    {
        uint2 idx = gtid.xy;
        float2 v0 = gs_HZB[idx.y + 0][idx.x + 0];
        float2 v1 = gs_HZB[idx.y + 0][idx.x + 2];
        float2 v2 = gs_HZB[idx.y + 2][idx.x + 0];
        float2 v3 = gs_HZB[idx.y + 2][idx.x + 2];
        
        result = Reduce4(v0, v1, v2, v3);
        gs_HZB[gtid.y][gtid.x] = result;
        
        uint2 mip2Coord = gid.xy * 8 + gtid.xy / 4;
        uint mip2W = ((Width + 1) / 2 + 3) / 4;
        uint mip2H = ((Height + 1) / 2 + 3) / 4;
        
        if (mip2Coord.x < mip2W && mip2Coord.y < mip2H)
            WriteMip(2, mip2Coord, result);
    }
    GroupMemoryBarrierWithGroupSync();
    
    // ===========================================
    // MIP 3: 4x4 threads active
    // ===========================================
    if (NumMips < 4)
        return;
    
    if ((gtid.x & 7) == 0 && (gtid.y & 7) == 0)
    {
        uint2 idx = gtid.xy;
        float2 v0 = gs_HZB[idx.y + 0][idx.x + 0];
        float2 v1 = gs_HZB[idx.y + 0][idx.x + 4];
        float2 v2 = gs_HZB[idx.y + 4][idx.x + 0];
        float2 v3 = gs_HZB[idx.y + 4][idx.x + 4];
        
        result = Reduce4(v0, v1, v2, v3);
        gs_HZB[gtid.y][gtid.x] = result;
        
        uint2 mip3Coord = gid.xy * 4 + gtid.xy / 8;
        uint mip3W = ((Width + 1) / 2 + 7) / 8;
        uint mip3H = ((Height + 1) / 2 + 7) / 8;
        
        if (mip3Coord.x < mip3W && mip3Coord.y < mip3H)
            WriteMip(3, mip3Coord, result);
    }
    GroupMemoryBarrierWithGroupSync();
    
    // ===========================================
    // MIP 4: 2x2 threads active
    // ===========================================
    if (NumMips < 5)
        return;
    
    if ((gtid.x & 15) == 0 && (gtid.y & 15) == 0)
    {
        uint2 idx = gtid.xy;
        float2 v0 = gs_HZB[idx.y + 0][idx.x + 0];
        float2 v1 = gs_HZB[idx.y + 0][idx.x + 8];
        float2 v2 = gs_HZB[idx.y + 8][idx.x + 0];
        float2 v3 = gs_HZB[idx.y + 8][idx.x + 8];
        
        result = Reduce4(v0, v1, v2, v3);
        gs_HZB[gtid.y][gtid.x] = result;
        
        uint2 mip4Coord = gid.xy * 2 + gtid.xy / 16;
        uint mip4W = ((Width + 1) / 2 + 15) / 16;
        uint mip4H = ((Height + 1) / 2 + 15) / 16;
        
        if (mip4Coord.x < mip4W && mip4Coord.y < mip4H)
            WriteMip(4, mip4Coord, result);
    }
    GroupMemoryBarrierWithGroupSync();
    
    // ===========================================
    // MIP 5: 1 thread active per group
    // ===========================================
    if (NumMips < 6)
        return;
    
    if (gtid.x == 0 && gtid.y == 0)
    {
        float2 v0 = gs_HZB[0][0];
        float2 v1 = gs_HZB[0][16];
        float2 v2 = gs_HZB[16][0];
        float2 v3 = gs_HZB[16][16];
        
        result = Reduce4(v0, v1, v2, v3);
        
        uint2 mip5Coord = gid.xy;
        uint mip5W = ((Width + 1) / 2 + 31) / 32;
        uint mip5H = ((Height + 1) / 2 + 31) / 32;
        
        if (mip5Coord.x < mip5W && mip5Coord.y < mip5H)
            WriteMip(5, mip5Coord, result);
    }
}
