// SsrtgiDepthMinMax.hlsl
// Parallel reduction to compute min/max depth values

Texture2D<float> gDepth : register(t0);
RWByteAddressBuffer gMinMaxOut : register(u0);

cbuffer CB0 : register(b0)
{
    uint Width;
    uint Height;
    float DepthInverted; // 0 = standard (sky~1), 1 = reversed (sky~0)
    uint _Pad0;
};

groupshared float gs_Min[256];
groupshared float gs_Max[256];

[numthreads(16, 16, 1)]
void main(uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID, uint3 dtid : SV_DispatchThreadID)
{
    uint flatIdx = gtid.y * 16 + gtid.x;

    // First group, first thread initializes output buffer
    if (gid.x == 0 && gid.y == 0 && flatIdx == 0)
    {
        gMinMaxOut.Store(0, asuint(1.0f)); // min starts at 1.0
        gMinMaxOut.Store(4, asuint(0.0f)); // max starts at 0.0
    }
    
    // Barrier to ensure init is done (only works within group, but first group will init)
    GroupMemoryBarrierWithGroupSync();

    float depth = 0.0f;
    bool inBounds = (dtid.x < Width && dtid.y < Height);

    if (inBounds)
        depth = gDepth.Load(int3(dtid.xy, 0));

    const float eps = 1e-5f;

    // Check if valid and not sky
    bool valid = inBounds && isfinite(depth) && (depth >= 0.0f) && (depth <= 1.0f);

    if (valid)
    {
        // Detect sky based on depth convention
        bool isSky;
        if (DepthInverted > 0.5f)
            isSky = (depth <= eps); // Reversed-Z: sky ~ 0
        else
            isSky = (depth >= (1.0f - eps)); // Standard-Z: sky ~ 1

        valid = !isSky;
    }

    // For invalid/sky pixels, use values that won't affect min/max
    gs_Min[flatIdx] = valid ? depth : 1.0f;
    gs_Max[flatIdx] = valid ? depth : 0.0f;

    GroupMemoryBarrierWithGroupSync();

    // Parallel reduction
    [unroll]
    for (uint stride = 128; stride > 0; stride >>= 1)
    {
        if (flatIdx < stride)
        {
            gs_Min[flatIdx] = min(gs_Min[flatIdx], gs_Min[flatIdx + stride]);
            gs_Max[flatIdx] = max(gs_Max[flatIdx], gs_Max[flatIdx + stride]);
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // Thread 0 writes result using atomics
    if (flatIdx == 0)
    {
        float localMin = gs_Min[0];
        float localMax = gs_Max[0];

        // Only write if we found valid geometry
        if (localMin < 1.0f && localMax > 0.0f)
        {
            uint minBits = asuint(localMin);
            uint maxBits = asuint(localMax);

            uint dummy;
            gMinMaxOut.InterlockedMin(0, minBits, dummy);
            gMinMaxOut.InterlockedMax(4, maxBits, dummy);
        }
    }
}
