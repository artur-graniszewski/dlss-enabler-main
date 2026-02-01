#include "SsrtgiCommon.hlsl"

// ============================================================
// BILATERAL DENOISE - WITH MULTIPLE MODES
// ============================================================
// Feature toggles:
//   DENOISE_MODE 0 = Bilateral filter (default, best quality)
//   DENOISE_MODE 1 = Jitter shift (let DLSS/FSR do temporal averaging)
//   DENOISE_MODE 2 = Pass-through (no processing)
//   DENOISE_MODE 3 = Fast bilateral (depth+color only, no normals, very stable)
//   DENOISE_MODE 4 = Blue noise bilateral (stochastic sampling, good for temporal)
//
// Mode 3 is optimized for:
// - ~3x faster than Mode 0
// - Very stable with temporal jitter (no shimmer)
// - Good edge preservation via tight depth threshold
// - 25 samples (cross+diagonals) vs 49 samples (7x7)
//
// Mode 4 (Blue Noise) features:
// - Stochastic sampling with blue noise distribution
// - Per-pixel random offsets (no visible patterns)
// - Temporal variation for accumulation
// - Good balance of quality and performance
// ============================================================

#define DENOISE_MODE 4

// Mode 0 parameters
#define KERNEL_RADIUS 4
#define SIGMA_DEPTH   0.05
#define SIGMA_NORMAL  8.0
#define SIGMA_COLOR   0.3

// Mode 3 parameters (tighter for cleaner edges)
#define FAST_KERNEL_RADIUS 4
#define FAST_SIGMA_DEPTH   0.13
#define FAST_SIGMA_COLOR   0.35

// Mode 4 (Blue Noise) parameters
#define BLUE_NOISE_SAMPLES 16
#define BLUE_NOISE_RADIUS 5.0
#define BLUE_NOISE_SIGMA_DEPTH 0.10
#define BLUE_NOISE_SIGMA_COLOR 0.30

// Precomputed 2D Gaussian weights dla 7x7 (sigma=3.0)
static const float GAUSS_2D[4][4] =
{
    { 0.0566, 0.0521, 0.0406, 0.0267 },
    { 0.0521, 0.0479, 0.0374, 0.0246 },
    { 0.0406, 0.0374, 0.0291, 0.0192 },
    { 0.0267, 0.0246, 0.0192, 0.0126 }
};

// 1D Gaussian for fast bilateral cross pattern
static const float GAUSS_1D[5] = { 0.2270, 0.1946, 0.1216, 0.0541, 0.0162 };

// ============================================================
// BLUE NOISE GENERATION (R2 sequence - quasi-blue noise)
// ============================================================
// R2 sequence produces well-distributed 2D points
static const float PLASTIC_CONSTANT = 1.32471795724; // Plastic constant
static const float R2_A1 = 0.7548776662466927; // 1/phi2
static const float R2_A2 = 0.5698402909980532; // 1/phi2^2

// Hash function for per-pixel seed
uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

// R2 sequence point generation
float2 R2Sequence(uint index, float2 offset)
{
    return frac(offset + float2(index * R2_A1, index * R2_A2));
}

// Convert uniform [0,1]^2 to disk with radius r
float2 UniformToDisk(float2 uv, float radius)
{
    float r = sqrt(uv.x) * radius;
    float theta = uv.y * 6.28318530718;
    return float2(r * cos(theta), r * sin(theta));
}

// Interleaved gradient noise (good for temporal)
float InterleavedGradientNoise(float2 screenPos, uint frameIndex)
{
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(magic.z * frac(dot(screenPos + float(frameIndex) * 5.588238, magic.xy)));
}

float GetGaussianWeight(int dx, int dy)
{
    return GAUSS_2D[abs(dy)][abs(dx)];
}

// Bilinear sample z gOut0 (dla jitter shift)
float4 SampleBilinear(float2 uv)
{
    float2 texSize = float2(g.Width, g.Height);
    float2 texelPos = uv * texSize - 0.5;
    
    int2 p00 = int2(floor(texelPos));
    float2 f = frac(texelPos);
    
    int2 p10 = p00 + int2(1, 0);
    int2 p01 = p00 + int2(0, 1);
    int2 p11 = p00 + int2(1, 1);
    
    p00 = clamp(p00, int2(0, 0), int2(g.Width - 1, g.Height - 1));
    p10 = clamp(p10, int2(0, 0), int2(g.Width - 1, g.Height - 1));
    p01 = clamp(p01, int2(0, 0), int2(g.Width - 1, g.Height - 1));
    p11 = clamp(p11, int2(0, 0), int2(g.Width - 1, g.Height - 1));
    
    float4 c00 = gOut0[p00];
    float4 c10 = gOut0[p10];
    float4 c01 = gOut0[p01];
    float4 c11 = gOut0[p11];
    
    float4 c0 = lerp(c00, c10, f.x);
    float4 c1 = lerp(c01, c11, f.x);
    return lerp(c0, c1, f.y);
}

// ============================================================
// FAST EDGE WEIGHT - DEPTH ONLY (for Mode 3)
// ============================================================
float FastEdgeWeight(float centerLinDepth, float sampleLinDepth)
{
    float depthDiff = abs(centerLinDepth - sampleLinDepth);
    float relDiff = depthDiff / max(centerLinDepth, 0.01);
    
    // Hard cutoff at 3% relative difference
    if (relDiff > 0.03)
        return 0.0;
    
    return exp(-depthDiff / FAST_SIGMA_DEPTH);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 p = dtid.xy;
    
    if (p.x >= g.Width || p.y >= g.Height)
        return;

#if DENOISE_MODE == 2
    // ============================================
    // MODE 2: Pass-through
    // ============================================
    gOut1[p] = gOut0[p];
    return;
    
#elif DENOISE_MODE == 1
    // ============================================
    // MODE 1: Jitter shift for DLSS/FSR temporal
    // ============================================
    float2 uv = (float2(p) + 0.5) / float2(g.Width, g.Height);
    float2 jitterPx = float2(g.JitterX, g.JitterY);
    float2 jitterUV = jitterPx / float2(g.Width, g.Height);
    float2 shiftedUV = uv - jitterUV;
    
    float4 result = SampleBilinear(shiftedUV);
    gOut1[p] = result;
    return;
    
#elif DENOISE_MODE == 3
    // ============================================
    // MODE 3: Fast Bilateral (depth+color only)
    // ============================================
    // No normal reconstruction = very stable with jitter
    // Cross pattern + diagonals = good coverage
    // Tight thresholds = clean edges
    // ============================================
    
    float4 centerColor = gOut0[p];
    
    int2 depthCoord = MapToDepthCoord(p);
    float centerDepth = gDepth.Load(int3(depthCoord, 0));
    
    // Early out: sky/invalid
    if (IsInvalidOrSkyDepth(centerDepth))
    {
        gOut1[p] = centerColor;
        return;
    }
    
    float centerLinDepth = LinearizeDepth(centerDepth);
    float centerLum = dot(centerColor.rgb, float3(0.299, 0.587, 0.114));
    
    // Early out: empty GI/AO region
    if (centerLum < 0.001 && centerColor.a > 0.995)
    {
        int2 maxC = int2(g.Width - 1, g.Height - 1);
        float4 n0 = gOut0[clamp(int2(p) + int2(-2, 0), int2(0,0), maxC)];
        float4 n1 = gOut0[clamp(int2(p) + int2(2, 0), int2(0,0), maxC)];
        float4 n2 = gOut0[clamp(int2(p) + int2(0, -2), int2(0,0), maxC)];
        float4 n3 = gOut0[clamp(int2(p) + int2(0, 2), int2(0,0), maxC)];
        
        float totalGI = dot(n0.rgb + n1.rgb + n2.rgb + n3.rgb, float3(0.299, 0.587, 0.114));
        if (totalGI < 0.02)
        {
            gOut1[p] = centerColor;
            return;
        }
    }
    
    // Start accumulation
    float3 sumGI = centerColor.rgb * GAUSS_1D[0];
    float sumAO = centerColor.a * GAUSS_1D[0];
    float sumWeight = GAUSS_1D[0];
    
    int2 maxCoord = int2(g.Width - 1, g.Height - 1);
    
    // HORIZONTAL SAMPLES
    [unroll]
    for (int dx = 1; dx <= FAST_KERNEL_RADIUS; dx++)
    {
        float wSpatial = GAUSS_1D[dx];
        
        // Left
        int2 posL = clamp(int2(p) + int2(-dx, 0), int2(0, 0), maxCoord);
        float depthL = gDepth.Load(int3(MapToDepthCoord(posL), 0));
        
        if (!IsInvalidOrSkyDepth(depthL))
        {
            float linL = LinearizeDepth(depthL);
            float wEdge = FastEdgeWeight(centerLinDepth, linL);
            
            if (wEdge > 0.001)
            {
                float4 colorL = gOut0[posL];
                float lumL = dot(colorL.rgb, float3(0.299, 0.587, 0.114));
                float wColor = exp(-abs(centerLum - lumL) / FAST_SIGMA_COLOR);
                
                float w = wSpatial * wEdge * wColor;
                sumGI += colorL.rgb * w;
                sumAO += colorL.a * w;
                sumWeight += w;
            }
        }
        
        // Right
        int2 posR = clamp(int2(p) + int2(dx, 0), int2(0, 0), maxCoord);
        float depthR = gDepth.Load(int3(MapToDepthCoord(posR), 0));
        
        if (!IsInvalidOrSkyDepth(depthR))
        {
            float linR = LinearizeDepth(depthR);
            float wEdge = FastEdgeWeight(centerLinDepth, linR);
            
            if (wEdge > 0.001)
            {
                float4 colorR = gOut0[posR];
                float lumR = dot(colorR.rgb, float3(0.299, 0.587, 0.114));
                float wColor = exp(-abs(centerLum - lumR) / FAST_SIGMA_COLOR);
                
                float w = wSpatial * wEdge * wColor;
                sumGI += colorR.rgb * w;
                sumAO += colorR.a * w;
                sumWeight += w;
            }
        }
    }
    
    // VERTICAL SAMPLES
    [unroll]
    for (int dy = 1; dy <= FAST_KERNEL_RADIUS; dy++)
    {
        float wSpatial = GAUSS_1D[dy];
        
        // Up
        int2 posU = clamp(int2(p) + int2(0, -dy), int2(0, 0), maxCoord);
        float depthU = gDepth.Load(int3(MapToDepthCoord(posU), 0));
        
        if (!IsInvalidOrSkyDepth(depthU))
        {
            float linU = LinearizeDepth(depthU);
            float wEdge = FastEdgeWeight(centerLinDepth, linU);
            
            if (wEdge > 0.001)
            {
                float4 colorU = gOut0[posU];
                float lumU = dot(colorU.rgb, float3(0.299, 0.587, 0.114));
                float wColor = exp(-abs(centerLum - lumU) / FAST_SIGMA_COLOR);
                
                float w = wSpatial * wEdge * wColor;
                sumGI += colorU.rgb * w;
                sumAO += colorU.a * w;
                sumWeight += w;
            }
        }
        
        // Down
        int2 posD = clamp(int2(p) + int2(0, dy), int2(0, 0), maxCoord);
        float depthD = gDepth.Load(int3(MapToDepthCoord(posD), 0));
        
        if (!IsInvalidOrSkyDepth(depthD))
        {
            float linD = LinearizeDepth(depthD);
            float wEdge = FastEdgeWeight(centerLinDepth, linD);
            
            if (wEdge > 0.001)
            {
                float4 colorD = gOut0[posD];
                float lumD = dot(colorD.rgb, float3(0.299, 0.587, 0.114));
                float wColor = exp(-abs(centerLum - lumD) / FAST_SIGMA_COLOR);
                
                float w = wSpatial * wEdge * wColor;
                sumGI += colorD.rgb * w;
                sumAO += colorD.a * w;
                sumWeight += w;
            }
        }
    }
    
    // DIAGONAL SAMPLES (8 samples for better isotropy)
    static const int2 DIAGONALS[8] = {
        int2(-1, -1), int2(1, -1), int2(-1, 1), int2(1, 1),
        int2(-2, -2), int2(2, -2), int2(-2, 2), int2(2, 2)
    };
    static const float DIAG_WEIGHTS[8] = {
        0.1946 * 0.707, 0.1946 * 0.707, 0.1946 * 0.707, 0.1946 * 0.707,
        0.1216 * 0.707, 0.1216 * 0.707, 0.1216 * 0.707, 0.1216 * 0.707
    };
    
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        int2 samplePos = clamp(int2(p) + DIAGONALS[i], int2(0, 0), maxCoord);
        float sampleDepth = gDepth.Load(int3(MapToDepthCoord(samplePos), 0));
        
        if (!IsInvalidOrSkyDepth(sampleDepth))
        {
            float sampleLinDepth = LinearizeDepth(sampleDepth);
            float wEdge = FastEdgeWeight(centerLinDepth, sampleLinDepth);
            
            if (wEdge > 0.001)
            {
                float4 sampleColor = gOut0[samplePos];
                float sampleLum = dot(sampleColor.rgb, float3(0.299, 0.587, 0.114));
                float wColor = exp(-abs(centerLum - sampleLum) / FAST_SIGMA_COLOR);
                
                float w = DIAG_WEIGHTS[i] * wEdge * wColor;
                sumGI += sampleColor.rgb * w;
                sumAO += sampleColor.a * w;
                sumWeight += w;
            }
        }
    }
    
    float invWeight = 1.0 / sumWeight;
    gOut1[p] = float4(sumGI * invWeight, sumAO * invWeight);
    return;

#elif DENOISE_MODE == 0
    // ============================================
    // MODE 0: Bilateral filter (default, best quality)
    // ============================================
    float4 centerColor = gOut0[p];
    
    int2 depthCoord = MapToDepthCoord(p);
    float centerDepth = gDepth.Load(int3(depthCoord, 0));
    
    // Early out: sky/invalid depth
    if (IsInvalidOrSkyDepth(centerDepth))
    {
        gOut1[p] = centerColor;
        return;
    }
    
    // Early out: je?li GI i AO s? "puste"
    float centerGILum = dot(centerColor.rgb, float3(0.299, 0.587, 0.114));
    bool isEmpty = (centerGILum < 0.001 && centerColor.a > 0.995);
    
    if (isEmpty)
    {
        float4 neighbors = float4(0, 0, 0, 0);
        neighbors += gOut0[clamp(int2(p) + int2(-2, 0), int2(0, 0), int2(g.Width - 1, g.Height - 1))];
        neighbors += gOut0[clamp(int2(p) + int2(2, 0), int2(0, 0), int2(g.Width - 1, g.Height - 1))];
        neighbors += gOut0[clamp(int2(p) + int2(0, -2), int2(0, 0), int2(g.Width - 1, g.Height - 1))];
        neighbors += gOut0[clamp(int2(p) + int2(0, 2), int2(0, 0), int2(g.Width - 1, g.Height - 1))];
        neighbors *= 0.25;
        
        float neighborsGI = dot(neighbors.rgb, float3(0.299, 0.587, 0.114));
        
        if (neighborsGI < 0.005 && neighbors.a > 0.99)
        {
            gOut1[p] = centerColor;
            return;
        }
    }
    
    float3 centerNormal = ReconstructNormal(p, centerDepth);
    float centerLum = centerGILum;
    
    // Start with center sample
    float3 sumGI = centerColor.rgb * GAUSS_2D[0][0];
    float sumAO = centerColor.a * GAUSS_2D[0][0];
    float sumWeight = GAUSS_2D[0][0];
    
    // Bilateral filter
    for (int dy = -KERNEL_RADIUS; dy <= KERNEL_RADIUS; dy++)
    {
        for (int dx = -KERNEL_RADIUS; dx <= KERNEL_RADIUS; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;
            
            int2 samplePos = int2(p) + int2(dx, dy);
            samplePos = clamp(samplePos, int2(0, 0), int2(g.Width - 1, g.Height - 1));
            
            int2 sampleDepthCoord = MapToDepthCoord(samplePos);
            float sampleDepth = gDepth.Load(int3(sampleDepthCoord, 0));
            
            if (IsInvalidOrSkyDepth(sampleDepth))
                continue;
            
            float depthDiff = abs(centerDepth - sampleDepth);
            if (depthDiff > SIGMA_DEPTH * 5.0)
                continue;
            
            float4 sampleColor = gOut0[uint2(samplePos)];
            float3 sampleNormal = ReconstructNormal(samplePos, sampleDepth);
            
            float normalDot = dot(centerNormal, sampleNormal);
            if (normalDot < 0.1)
                continue;
            
            float wSpatial = GetGaussianWeight(dx, dy);
            float wDepth = exp(-depthDiff / SIGMA_DEPTH);
            float wNormal = pow(max(0.0, normalDot), SIGMA_NORMAL);
            
            float sampleLum = dot(sampleColor.rgb, float3(0.299, 0.587, 0.114));
            float lumDiff = abs(centerLum - sampleLum);
            float wColor = exp(-lumDiff / SIGMA_COLOR);
            
            float weight = wSpatial * wDepth * wNormal * wColor;
            
            if (weight < 0.001)
                continue;
            
            sumGI += sampleColor.rgb * weight;
            sumAO += sampleColor.a * weight;
            sumWeight += weight;
        }
    }
    
    float invWeight = 1.0 / sumWeight;
    gOut1[p] = float4(sumGI * invWeight, sumAO * invWeight);
    
#elif DENOISE_MODE == 4
    // ============================================
    // MODE 4: Blue Noise Bilateral
    // ============================================
    // Stochastic sampling with R2 sequence for well-distributed points
    // Combines with temporal accumulation for high quality output
    // ADAPTIVE: Larger radius for brighter pixels to hide HDR noise
    
    // DEBUG: Set to 1 to visualize adaptive radius detection

    
    float4 centerColor = gOut0[p];
    
    int2 depthCoord = MapToDepthCoord(p);
    float depthRaw = gDepth.Load(int3(depthCoord, 0));
    
    if (IsInvalidOrSkyDepth(depthRaw))
    {
        gOut1[p] = centerColor;
        return;
    }
    
    float centerDepth = LinearizeDepth(depthRaw);
    float centerLum = dot(centerColor.rgb, float3(0.299, 0.587, 0.114));
    #define DEBUG_ADAPTIVE_RADIUS 0
    // ============================================
    // ADAPTIVE RADIUS based on luminance
    // Brighter = more blur to hide HDR noise
    // ============================================
    float lumFactor = saturate(centerLum / 10.5); // 0-1 for lum 0-1.5
    float adaptiveRadius = lerp(BLUE_NOISE_RADIUS, BLUE_NOISE_RADIUS * 4.0, lumFactor * lumFactor);
    uint adaptiveSamples = lerp(BLUE_NOISE_SAMPLES, BLUE_NOISE_SAMPLES + 16, lumFactor);
    
    // Per-pixel random seed based on position and frame
    uint seed = WangHash(p.x + p.y * g.Width + g.FrameIndex * 1337);
    float2 pixelOffset = float2(
        float(seed & 0xFFFF) / 65535.0,
        float((seed >> 16) & 0xFFFF) / 65535.0
    );
    
    // Temporal rotation for better accumulation
    float temporalAngle = float(g.FrameIndex % 8) * 0.785398163; // 45 degree steps
    float cosA = cos(temporalAngle);
    float sinA = sin(temporalAngle);
    
    float3 sumGI = centerColor.rgb;
    float sumAO = centerColor.a;
    float sumWeight = 1.0;
    
    int2 maxCoord = int2(g.Width - 1, g.Height - 1);
    
    [loop]
    for (uint i = 0; i < adaptiveSamples; i++)
    {
        // Generate R2 sequence point
        float2 r2Point = R2Sequence(i, pixelOffset);
        
        // Convert to disk distribution with adaptive radius
        float2 diskOffset = UniformToDisk(r2Point, adaptiveRadius);
        
        // Apply temporal rotation
        float2 rotatedOffset = float2(
            diskOffset.x * cosA - diskOffset.y * sinA,
            diskOffset.x * sinA + diskOffset.y * cosA
        );
        
        int2 samplePos = p + int2(round(rotatedOffset));
        samplePos = clamp(samplePos, int2(0, 0), maxCoord);
        
        // Skip center pixel
        if (all(samplePos == p))
            continue;
        
        // Sample depth
        int2 sampleDepthCoord = MapToDepthCoord(samplePos);
        float sampleDepthRaw = gDepth.Load(int3(sampleDepthCoord, 0));
        
        if (IsInvalidOrSkyDepth(sampleDepthRaw))
            continue;
        
        float sampleDepth = LinearizeDepth(sampleDepthRaw);
        float depthDiff = abs(centerDepth - sampleDepth) / max(centerDepth, 0.001);
        
        // Reject if depth difference too large
        if (depthDiff > 0.05)
            continue;
        
        // Sample color
        float4 sampleColor = gOut0[samplePos];
        float sampleLum = dot(sampleColor.rgb, float3(0.299, 0.587, 0.114));
        float lumDiff = abs(centerLum - sampleLum);
        
        // Calculate weights - use adaptive radius for spatial weight
        float dist = length(float2(samplePos - p));
        float wSpatial = exp(-dist * dist / (2.0 * adaptiveRadius * adaptiveRadius));
        float wDepth = exp(-depthDiff / BLUE_NOISE_SIGMA_DEPTH);
        float wColor = exp(-lumDiff / BLUE_NOISE_SIGMA_COLOR);
        
        float weight = wSpatial * wDepth * wColor;
        
        if (weight < 0.001)
            continue;
        
        sumGI += sampleColor.rgb * weight;
        sumAO += sampleColor.a * weight;
        sumWeight += weight;
    }
    
    float invWeight = 1.0 / sumWeight;
    
#if DEBUG_ADAPTIVE_RADIUS
    // Visualize adaptive radius: Black = no boost, Orange/Yellow = max boost
    float boostVis = lumFactor * lumFactor;
    gOut1[p] = float4(boostVis * 3.0, boostVis * 1.5, 0.0, 0.5);
#else
    gOut1[p] = float4(sumGI * invWeight, sumAO * invWeight);
#endif

#endif
}