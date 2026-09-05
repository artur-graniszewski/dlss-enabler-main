#include "SsrtgiCommon.hlsl"

// ============================================================
// RAY MARCHING SSRTGI - FIXED BANDING/PATTERN VERSION
// ============================================================
// Fixes:
// 1. Temporal noise using FrameIndex
// 2. Better spatial hash with blue noise characteristics  
// 3. Per-ray jitter to break up patterns
// 4. Interleaved gradient noise for better distribution
// ============================================================

#define DEBUG_MODE 0

// ============================================================
// RAY MARCHING CONFIGURATION
// ============================================================
// NumRays is controlled via constant buffer (g.NumRays)
// FalloffEnd is only used in Composite shader, Gather uses fixed value
static const int MAX_STEPS_MAX = 4;
static const int MAX_STEPS_MIN = 4;

// Depth thresholds for adaptive quality
static const float DEPTH_NEAR = 5.0; // Full quality below this
static const float DEPTH_FAR = 30.0; // Minimum quality above this

// Distance falloff parameters (fixed in Gather, variable FalloffEnd only in Composite)
static const float FALLOFF_START_DIST = 15.0;
static const float FALLOFF_END_DIST = 150.0; // Fixed for Gather shader
static const float FALLOFF_MIN_STRENGTH = 0.15;

static const int MAX_RAY_DIRS = 32;
static const float3 RAY_DIRS[32] =
{
    float3(0.707, 0.000, 0.707), float3(0.500, 0.500, 0.707),
    float3(0.000, 0.707, 0.707), float3(-0.500, 0.500, 0.707),
    float3(-0.707, 0.000, 0.707), float3(-0.500, -0.500, 0.707),
    float3(0.000, -0.707, 0.707), float3(0.500, -0.500, 0.707),
    float3(0.866, 0.000, 0.500), float3(0.612, 0.612, 0.500),
    float3(0.000, 0.866, 0.500), float3(-0.612, 0.612, 0.500),
    float3(-0.866, 0.000, 0.500), float3(-0.612, -0.612, 0.500),
    float3(0.000, -0.866, 0.500), float3(0.612, -0.612, 0.500),
    float3(0.500, 0.000, 0.866), float3(0.354, 0.354, 0.866),
    float3(0.000, 0.500, 0.866), float3(-0.354, 0.354, 0.866),
    float3(-0.500, 0.000, 0.866), float3(-0.354, -0.354, 0.866),
    float3(0.000, -0.500, 0.866), float3(0.354, -0.354, 0.866),
    float3(0.940, 0.000, 0.342), float3(0.664, 0.664, 0.342),
    float3(0.000, 0.940, 0.342), float3(-0.664, 0.664, 0.342),
    float3(-0.940, 0.000, 0.342), float3(-0.664, -0.664, 0.342),
    float3(0.000, -0.940, 0.342), float3(0.664, -0.664, 0.342)
};

// ============================================================
// IMPROVED NOISE FUNCTIONS - eliminates banding patterns
// ============================================================

// Interleaved Gradient Noise - bardzo dobra dystrybucja, szybka
float IGN(float2 pixelCoord, uint frameIndex)
{
    // Rotate sampling pattern each frame
    float2 rotatedCoord = pixelCoord;
    uint framePhase = frameIndex % 64;
    
    // Magic numbers from Jorge Jimenez's presentation
    float noise = frac(52.9829189 * frac(0.06711056 * rotatedCoord.x + 0.00583715 * rotatedCoord.y));
    
    // Add temporal variation
    noise = frac(noise + float(framePhase) * 0.6180339887); // Golden ratio
    
    return noise;
}

// R2 sequence - quasi-random, low discrepancy
float2 R2Seq(uint index)
{
    // Plastic constant based sequence
    const float g = 1.32471795724474602596;
    const float a1 = 1.0 / g;
    const float a2 = 1.0 / (g * g);
    return frac(float2(a1, a2) * float(index) + 0.5);
}

// Better spatial hash with temporal component
float SpatialTemporalHash(float2 p, uint frame)
{
    // Use different hash per frame to break temporal patterns
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    float hash = frac((p3.x + p3.y) * p3.z);
    
    // Add temporal jitter using golden ratio
    hash = frac(hash + float(frame % 32) * 0.6180339887);
    
    return hash;
}

// Per-ray hash for additional variation
float RayHash(float2 p, uint rayIndex, uint frame)
{
    float2 seed = p + float2(rayIndex * 7.3, rayIndex * 11.7);
    seed += float(frame % 16) * float2(1.23, 2.34);
    
    float3 p3 = frac(float3(seed.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

// ============================================================
// HELPER FUNCTIONS
// ============================================================

void BuildTangentFrame(float3 N, out float3 T, out float3 B)
{
    float3 up = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

float CalculateDistanceAttenuation(float linearDepth)
{
    if (linearDepth <= FALLOFF_START_DIST)
        return 1.0;
    float t = saturate((linearDepth - FALLOFF_START_DIST) / (FALLOFF_END_DIST - FALLOFF_START_DIST));
    t = t * t * (3.0 - 2.0 * t);
    return lerp(1.0, FALLOFF_MIN_STRENGTH, t);
}

float3 HeatmapColor(float t)
{
    t = saturate(t);
    if (t < 0.33)
        return lerp(float3(0, 0, 1), float3(0, 1, 0), t / 0.33);
    if (t < 0.66)
        return lerp(float3(0, 1, 0), float3(1, 1, 0), (t - 0.33) / 0.33);
    return lerp(float3(1, 1, 0), float3(1, 0, 0), (t - 0.66) / 0.34);
}

// Cosine-weighted hemisphere sample
float3 CosineSampleHemisphere(float2 u)
{
    float r = sqrt(u.x);
    float theta = 2.0 * PI * u.y;
    
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(max(0.0, 1.0 - u.x));
    
    return float3(x, y, z);
}

// ============================================================
// MAIN SHADER
// ============================================================

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 p = dtid.xy;
    
    uint outW, outH;
    gOut0.GetDimensions(outW, outH);
    if (p.x >= outW || p.y >= outH)
        return;
    
    const float cameraNear = max(g.CameraNear, 0.001);
    const float cameraFar = max(g.CameraFar, cameraNear + 1.0);
    const float cameraAspect = g.CameraAspectRatio;
    const float tanHalfFov = tan(g.CameraFOV * 0.5);
    const float invTanHalfFov = 1.0 / tanHalfFov;
    const float nf = cameraNear * cameraFar;
    const float fMinusN = cameraFar - cameraNear;
    const bool depthInverted = (g.DepthInverted > 0.5);
    const float depthInvFloat = depthInverted ? 1.0 : 0.0;
    
    // ============================================================
    // FIX: Use frame index for temporal variation
    // ============================================================
    const uint frameIndex = g.FrameIndex;
    
    uint2 depthDims = GetDepthDimensions();
    uint2 colorDims = GetColorDimensions();
    float2 outDimsF = float2(outW, outH);
    float2 depthDimsF = float2(depthDims);
    float2 colorDimsF = float2(colorDims);
    float2 invDepthDims = 1.0 / depthDimsF;

#define LINEARIZE_DEPTH_FAST(d, outLinear) { \
    float rd = saturate(d); \
    float denomA = cameraNear + rd * fMinusN; \
    float denomB = cameraFar - rd * fMinusN; \
    float denom = lerp(denomB, denomA, depthInvFloat); \
    outLinear = nf / max(denom, 0.0001); \
}

#define RECONSTRUCT_VIEW_POS(uv, linD, outPos) { \
    float2 ndc = (uv) * 2.0 - 1.0; \
    ndc.y = -ndc.y; \
    outPos = float3(ndc.x * tanHalfFov * cameraAspect * linD, ndc.y * tanHalfFov * linD, linD); \
}

#define PROJECT_TO_UV(viewPos, outUv, outValid) { \
    float invZ = 1.0 / max(viewPos.z, 0.0001); \
    float2 ndc = float2(viewPos.x * invZ * invTanHalfFov / cameraAspect, -viewPos.y * invZ * invTanHalfFov); \
    outUv = ndc * 0.5 + 0.5; \
    outValid = all(outUv >= 0.0 && outUv <= 1.0); \
}

    int2 dp = MapToDepthCoord(p);
    float depthRaw = gDepth.Load(int3(dp, 0));
    
    if (IsInvalidOrSkyDepth(depthRaw))
    {
        gOut0[p] = float4(0, 0, 0, 1);
        return;
    }
    
    float2 uv = (float2(dp) + 0.5) * invDepthDims;
    float linearDepth;
    LINEARIZE_DEPTH_FAST(depthRaw, linearDepth);
    
    float3 viewPos;
    RECONSTRUCT_VIEW_POS(uv, linearDepth, viewPos);
    
    // Normal reconstruction
    float3 normal;
    {
        int2 pInt = int2(dp);
        int2 maxCoord = int2(depthDims) - 1;
        
        float dL = gDepth.Load(int3(clamp(pInt + int2(-1, 0), int2(0, 0), maxCoord), 0));
        float dR = gDepth.Load(int3(clamp(pInt + int2(1, 0), int2(0, 0), maxCoord), 0));
        float dU = gDepth.Load(int3(clamp(pInt + int2(0, -1), int2(0, 0), maxCoord), 0));
        float dD = gDepth.Load(int3(clamp(pInt + int2(0, 1), int2(0, 0), maxCoord), 0));
        
        float linL, linR, linU, linD_n;
        LINEARIZE_DEPTH_FAST(dL, linL);
        LINEARIZE_DEPTH_FAST(dR, linR);
        LINEARIZE_DEPTH_FAST(dU, linU);
        LINEARIZE_DEPTH_FAST(dD, linD_n);
        
        float3 posC, posL, posR, posU, posD;
        RECONSTRUCT_VIEW_POS(uv, linearDepth, posC);
        RECONSTRUCT_VIEW_POS((float2(clamp(pInt + int2(-1,0), int2(0,0), maxCoord)) + 0.5) * invDepthDims, linL, posL);
        RECONSTRUCT_VIEW_POS((float2(clamp(pInt + int2(1,0), int2(0,0), maxCoord)) + 0.5) * invDepthDims, linR, posR);
        RECONSTRUCT_VIEW_POS((float2(clamp(pInt + int2(0,-1), int2(0,0), maxCoord)) + 0.5) * invDepthDims, linU, posU);
        RECONSTRUCT_VIEW_POS((float2(clamp(pInt + int2(0,1), int2(0,0), maxCoord)) + 0.5) * invDepthDims, linD_n, posD);
        
        float3 ddx = (abs(linR - linearDepth) < abs(linL - linearDepth)) ? (posR - posC) : (posC - posL);
        float3 ddy = (abs(linD_n - linearDepth) < abs(linU - linearDepth)) ? (posD - posC) : (posC - posU);
        
        normal = normalize(cross(ddy, ddx));
    }
    
    float3 V = normalize(-viewPos);
    if (dot(normal, V) < 0.0)
        normal = -normal;

#if DEBUG_MODE == 1
    gOut0[p] = float4(normal * 0.5 + 0.5, 1.0);
    return;
#endif

    float distanceAttenuation = CalculateDistanceAttenuation(linearDepth);

#if DEBUG_MODE == 8
    gOut0[p] = float4(distanceAttenuation, distanceAttenuation * 0.5, 0.0, 1.0);
    return;
#endif
    
    float3 tangent, bitangent;
    BuildTangentFrame(normal, tangent, bitangent);
    
    // ============================================================
    // FIX: Improved noise with temporal variation
    // ============================================================
    float spatialHash = SpatialTemporalHash(float2(p), frameIndex);
    float ignNoise = IGN(float2(p), frameIndex);
    
    // Combine both noise sources for better coverage
    float rotationAngle = TWO_PI * ignNoise;
    
    // Frame-varying direction permutation
    uint dirPermutation = uint(spatialHash * 32.0) + (frameIndex % 8) * 4;
    
    float cosRot, sinRot;
    sincos(rotationAngle, sinRot, cosRot);
    
    // ============================================================
    // ADAPTIVE QUALITY based on depth
    // Near objects: more rays, more steps (better quality)
    // Far objects: fewer rays, fewer steps (better performance)
    // ============================================================
    float depthFactor = saturate((linearDepth - DEPTH_NEAR) / (DEPTH_FAR - DEPTH_NEAR));
    depthFactor = depthFactor * depthFactor; // Quadratic falloff
    
    // Use g.NumRays from UI (4, 8, 16, 32), with adaptive reduction for far objects
    int numRaysBase = int(g.NumRays);
    int numRaysMin = max(numRaysBase / 2, 4); // Minimum is half of selected, but at least 4
    int numRays = int(lerp(float(numRaysBase), float(numRaysMin), depthFactor));
    
    // ============================================================
    // ADAPTIVE RAY MARCHING CONFIGURATION
    // ============================================================
    // Key insight: Use small steps near origin for detail, 
    // then accelerate for distant occlusion
    // ============================================================
    
    // Calculate base parameters
    float worldRadius = g.RadiusPx * 0.15;
    worldRadius = clamp(worldRadius, 0.5, 150.0);
    float maxDist = min(worldRadius, linearDepth * 0.5);
    
    // Base step size scales with distance (like original), but we accelerate from there
    // For close objects: small steps. For far objects: start larger but still accelerate.
    const int BASE_STEPS = 4;
    float baseStepSize = maxDist / float(BASE_STEPS);
    
    // Minimum step size - scales with linearDepth for close objects
    // At linearDepth=1: minStep=0.05, at linearDepth=10: minStep=0.15
    float minStepSize = clamp(baseStepSize * 0.5, 0.02, 0.2);
    
    const float STEP_ACCELERATION = 1.35; // Each step is 35% larger than previous
    
    // Thickness scales with distance for proper occlusion detection
    float baseThickness = minStepSize * 2.0 + sqrt(linearDepth) * 0.02;
    
    float3 startPos = viewPos + normal * (minStepSize * 0.5);
    
    // Statistics
    float aoAccum = 0.0;
    float3 giAccum = float3(0, 0, 0);
    int validRays = 0;
    int hitCount = 0;
    int totalSteps = 0;
    int skyEarlyOuts = 0;

    // ============================================================
    // GI-DERIVED FOG INFERENCE
    // Accumulate raw (un-falloff'd) hit colors and squares for
    // variance estimation. Uniform variance across hits + similarity
    // to origin pixel color = strong fog signal.
    // ============================================================
    float3 hitColorSum = float3(0, 0, 0);
    float3 hitColorSqSum = float3(0, 0, 0);
    
    const bool needColorRemap = (colorDims.x != outW || colorDims.y != outH);
    float2 colorType = needColorRemap ? colorDimsF : outDimsF;
    
    // Maximum steps - enough to cover maxDist with acceleration
    // With acceleration 1.35, after N steps we've traveled: minStep * (1.35^N - 1) / 0.35
    const int MAX_STEPS_ADAPTIVE = 24; // Allows covering large distances efficiently
    
    [loop]
    for (int ray = 0; ray < numRays; ray++)
    {
        // ============================================================
        // FIX: Per-ray jitter using R2 sequence + temporal offset
        // ============================================================
        float2 r2 = R2Seq(ray + frameIndex * numRaysBase);
        float rayJitter = RayHash(float2(p), ray, frameIndex);
        
        uint dirIndex = (uint(ray * 2) + dirPermutation + uint(rayJitter * 8.0)) & 31u;
        float3 localDir = RAY_DIRS[dirIndex];
        
        // ============================================================
        // FIX: Per-ray rotation variation
        // ============================================================
        float rayRotation = rotationAngle + ray * (TWO_PI / float(numRays)) * 0.5 + rayJitter * 0.3;
        float rayCosr, raySinr;
        sincos(rayRotation, raySinr, rayCosr);
        
        float2 rotated;
        rotated.x = localDir.x * rayCosr - localDir.y * raySinr;
        rotated.y = localDir.x * raySinr + localDir.y * rayCosr;
        localDir.xy = rotated;
        
        // ============================================================
        // FIX: Add slight elevation variation per ray
        // ============================================================
        float elevationJitter = (r2.y - 0.5) * 0.15;
        localDir.z = saturate(localDir.z + elevationJitter);
        localDir = normalize(localDir);
        
        float3 rayDir = tangent * localDir.x + bitangent * localDir.y + normal * localDir.z;
        
        if (dot(rayDir, normal) < 0.1)
            continue;
        
        validRays++;
        
        // ============================================================
        // ADAPTIVE RAY MARCHING
        // Start with small steps, accelerate as we go further
        // This preserves fine details near the surface while still
        // catching distant occluders efficiently
        // ============================================================
        float stepJitter = frac(rayJitter + r2.x);
        float currentStepSize = minStepSize;
        float t = currentStepSize * stepJitter;
        
        bool hit = false;
        float hitDist = maxDist;
        float3 hitColor = float3(0, 0, 0);
        
        [loop]
        for (int step = 0; step < MAX_STEPS_ADAPTIVE && t < maxDist; step++)
        {
            totalSteps++;
            float3 samplePos = startPos + rayDir * t;
            if (samplePos.z <= cameraNear) 
                break;
            
            float2 sampleUv;
            bool uvValid;
            PROJECT_TO_UV(samplePos, sampleUv, uvValid);
            if (!uvValid) 
                break;
            
            int2 depthCoord = int2(sampleUv * depthDimsF);
            depthCoord = clamp(depthCoord, int2(0, 0), int2(depthDims) - 1);
            
            float sampledDepth = gDepth.Load(int3(depthCoord, 0));
            
            if (IsInvalidOrSkyDepth(sampledDepth))
            {
                skyEarlyOuts++;
                break;
            }
            
            float sampledZ;
            LINEARIZE_DEPTH_FAST(sampledDepth, sampledZ);
            float diff = samplePos.z - sampledZ;
            
            // Thickness grows with step size for proper detection at distance
            float adaptiveThickness = currentStepSize * 2.0 + sqrt(linearDepth) * 0.02;
            
            if (diff > 0.0 && diff < adaptiveThickness)
            {
                hit = true;
                hitDist = t;
                int2 colorCoord = int2(sampleUv * colorType);
                colorCoord = clamp(colorCoord, int2(0, 0), int2(colorDims) - 1);
                hitColor = gColor.Load(int3(colorCoord, 0)).rgb;
                break;
            }
            
            // Advance with current step size, then accelerate
            t += currentStepSize;
            currentStepSize *= STEP_ACCELERATION;
            
            // Cap step size to avoid huge jumps
            currentStepSize = min(currentStepSize, maxDist * 0.25);
        }
        
        if (hit)
        {
            hitCount++;
            float falloff = 1.0 - (hitDist / maxDist);
            falloff = falloff * falloff;
            aoAccum += falloff;
            giAccum += hitColor * falloff;

            // Raw stats (un-weighted) for fog variance signal
            hitColorSum += hitColor;
            hitColorSqSum += hitColor * hitColor;
        }
    }
    
    // ============================================================
    // GI-DERIVED FOG CONFIDENCE
    // ============================================================
    // Two signals:
    //   colorMatch: how similar the average hit color is to origin
    //               pixel color. Fog filters everything to the same
    //               tint, so high match suggests veiled scene.
    //   colorUnif:  how low the variance among hit colors is. Fog
    //               makes all directions look the same.
    // Combined: confidence that AO/GI here is bogus.
    //
    // Cost: 1 extra texture load (origin color) + ~15 ALU.
    // Requires hitCount >= 4 to have meaningful statistics.
    // ============================================================
    float fogConfidence = 0.0;
    float colorMatchDbg = 0.0;
    float colorUnifDbg = 0.0;

    if (hitCount >= 4 && linearDepth > 5.0)
    {
        float invHits = 1.0 / float(hitCount);
        float3 meanHit = hitColorSum * invHits;
        float3 varHit = max(hitColorSqSum * invHits - meanHit * meanHit, 0.0);

        // Luma variance is more stable than per-channel
        const float3 lumWeights = float3(0.299, 0.587, 0.114);
        float lumaVar = dot(varHit, lumWeights);

        // Sample origin pixel color (1 extra load - this is the only
        // new texture access added by the entire fog inference path)
        int2 originColorCoord = int2((float2(dp) + 0.5) * invDepthDims * colorType);
        originColorCoord = clamp(originColorCoord, int2(0, 0), int2(colorDims) - 1);
        float3 originColor = gColor.Load(int3(originColorCoord, 0)).rgb;

        // colorMatch: 1.0 = identical to origin (fog!), 0.0 = very different
        // Loosened from *4.0 to *2.5 to catch less-saturated fog
        float matchDist = length(meanHit - originColor);
        float colorMatch = saturate(1.0 - matchDist * 2.5);

        // colorUnif: 1.0 = all hits same color (fog!), 0.0 = high variance
        // Loosened from *200.0 to *100.0 to catch fog with some background bleed-through
        float colorUnif = saturate(1.0 - lumaVar * 100.0);

        // Bounce-light suppression: if mean hit is significantly BRIGHTER
        // than origin in same direction, this is real GI bounce, not fog.
        // Fog tends to be similar luminance to origin (both filtered).
        float originLuma = dot(originColor, lumWeights);
        float meanHitLuma = dot(meanHit, lumWeights);
        float lumaRatio = meanHitLuma / max(originLuma, 0.01);
        float notBounce = saturate(2.0 - lumaRatio); // 1.0 if hit<=origin, 0.0 if hit>=2*origin

        // Distance ramp - low fog confidence in foreground
        float distWeight = saturate((linearDepth - 5.0) / 15.0);

        fogConfidence = colorMatch * colorUnif * notBounce * distWeight;

        colorMatchDbg = colorMatch;
        colorUnifDbg = colorUnif;
    }

    // ============================================================
    // DEBUG MODES
    // ============================================================
#if DEBUG_MODE == 2
    float aoVis = 1.0;
    if (validRays > 0)
        aoVis = 1.0 - (aoAccum / float(validRays)) * g.AoStrength;
    gOut0[p] = float4(aoVis, aoVis, aoVis, 1.0);
    return;
#endif

#if DEBUG_MODE == 3
    float3 giVis = float3(0, 0, 0);
    if (hitCount > 0)
        giVis = (giAccum / float(hitCount)) * g.GiStrength;
    gOut0[p] = float4(giVis, 1.0);
    return;
#endif

#if DEBUG_MODE == 4
    float hitRatio = (validRays > 0) ? float(hitCount) / float(validRays) : 0.0;
    gOut0[p] = float4(HeatmapColor(hitRatio), 1.0);
    return;
#endif

#if DEBUG_MODE == 5
    float maxPossibleSteps = float(validRays * MAX_STEPS);
    float stepEfficiency = (maxPossibleSteps > 0) ? float(totalSteps) / maxPossibleSteps : 0.0;
    gOut0[p] = float4(HeatmapColor(stepEfficiency), 1.0);
    return;
#endif

#if DEBUG_MODE == 6
    float skyRatio = (validRays > 0) ? float(skyEarlyOuts) / float(validRays) : 0.0;
    gOut0[p] = float4(1.0 - skyRatio, skyRatio, 0.0, 1.0);
    return;
#endif

#if DEBUG_MODE == 7
    float depthVis = saturate(linearDepth / cameraFar);
    gOut0[p] = float4(HeatmapColor(depthVis), 1.0);
    return;
#endif

#if DEBUG_MODE == 10
    // Color match standalone: green = origin matches mean hit (fog-like)
    gOut0[p] = float4(1.0 - colorMatchDbg, colorMatchDbg, 0.0, 1.0);
    return;
#endif

#if DEBUG_MODE == 11
    // Color uniformity standalone: green = all hits same color (fog-like)
    gOut0[p] = float4(1.0 - colorUnifDbg, colorUnifDbg, 0.0, 1.0);
    return;
#endif

#if DEBUG_MODE == 12
    // Combined fog confidence: green = high fog confidence (AO will be suppressed)
    gOut0[p] = float4(1.0 - fogConfidence, fogConfidence, 0.0, 1.0);
    return;
#endif
    
    // ============================================================
    // NORMAL OUTPUT
    // ============================================================
    // Apply fog confidence: lift AO toward 1.0 and tone down GI
    // proportionally to confidence that this pixel is veiled by fog.
    // GI gets *0.7 cap so it never goes fully off (some bounce is
    // genuinely uniform e.g. enclosed bright rooms).
    // ============================================================
    // FOG SUPPRESSION
    // ============================================================
    // NOTE: Composite re-amplifies AO by aoStrengthMult=2.8 and pow(ao,1.6),
    // which means even ao=0.95 from here ends up as ~20% darkening.
    // To actually kill AO in fog, we must push ao toward (or above) 1.0
    // aggressively. We lerp to 1.2 (clamped) so that even fogConfidence~0.5
    // produces a clean ao=1.0 that survives Composite's amplification.
    float aoSuppress = saturate(fogConfidence * 2.0);
    float giSuppress = fogConfidence * 0.7;

    float ao = 1.0;
    if (validRays > 0)
    {
        float aoRaw = 1.0 - (aoAccum / float(validRays)) * g.AoStrength * distanceAttenuation;
        aoRaw = clamp(aoRaw, 0.02, 1.0);
        // Lerp toward 1.2 (not 1.0) then clamp - this ensures Composite's
        // aoStrengthMult=2.8 can't pull a "suppressed" pixel back into darkness.
        ao = saturate(lerp(aoRaw, 1.2, aoSuppress));
    }
    
    float3 gi = float3(0, 0, 0);
    if (hitCount > 0)
    {
        gi = (giAccum / float(hitCount)) * g.GiStrength * distanceAttenuation;
        float lum = dot(gi, float3(0.299, 0.587, 0.114));
        if (lum > 0.5)
            gi *= 0.5 / lum;
        gi *= (1.0 - giSuppress);
    }
    
    gOut0[p] = float4(gi, ao);
}
