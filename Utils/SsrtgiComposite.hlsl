#include "SsrtgiCommon.hlsl"

// ============================================================
// SSRTGI COMPOSITE - OPTIMIZED + DISTANCE FALLOFF FIX
// ============================================================
// Optimizations:
// 1. Removed all debug modes (compile-time dead code elimination)
// 2. Precomputed constants
// 3. Reduced redundant calculations
// 4. Removed unused helper functions
// 5. [NEW] Additional distance-based falloff for final compositing
// ============================================================

// Set to 1 to enable debug modes, 0 for production (faster)
#define ENABLE_DEBUG_MODES 0

// Set to 1 to read from gOut1 (Denoise output) instead of gHistory (Temporal output)
// Use this when testing without Temporal pass
#define READ_FROM_DENOISE 0

#if ENABLE_DEBUG_MODES
// DEBUG_MODE options:
// 0 = Split-screen (left=original, right=effect)
// 2 = AO only
// 6 = Full effect (no split)
// 16 = RAW GI passthrough (from temporal)
// 17 = RAW AO from alpha
// 18 = Energy comparison: RED = surface brighter, GREEN = GI brighter
// 19 = GI vs Bounce comparison
// 23 = [NEW] Distance attenuation visualization
// --- TEMPORAL DEBUG (20-29) ---
// 20 = Motion vectors (red=X, green=Y, gray=no motion)
// 21 = GI difference: current vs history (white=difference)
// 22 = Temporal stability test (shows if history is working)
#define DEBUG_MODE 0
#endif

// ============================================================
// [NEW] COMPOSITE DISTANCE FALLOFF CONFIGURATION
// ============================================================
// These work in addition to gather falloff for extra control
// COMPOSITE_FALLOFF_END now comes from g.FalloffEnd (constant buffer)
static const float COMPOSITE_FALLOFF_START = 10.8; // Distance where falloff begins
static const float COMPOSITE_MIN_EFFECT = 0.25; // Minimum effect multiplier at far distance

// ============================================================
// [NEW] DISTANCE ATTENUATION FOR COMPOSITE PASS
// ============================================================
float CompositeDistanceAttenuation(float linearDepth)
{
    // Smooth falloff between start and g.FalloffEnd
    float t = saturate((linearDepth - COMPOSITE_FALLOFF_START) /
                       (g.FalloffEnd - COMPOSITE_FALLOFF_START));
    
    // Smooth step for gradual transition
    t = t * t * (3.0 - 2.0 * t);
    
    return lerp(1.0, COMPOSITE_MIN_EFFECT, t);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 px = dtid.xy;
    if (px.x >= g.Width || px.y >= g.Height)
        return;
    
    // Load all inputs once
    float4 baseColor = gColor.Load(int3(px, 0));
    
#if READ_FROM_DENOISE
    float4 ssrtgi = gOut1.Load(int3(px, 0)); // Read from Denoise output
#else
    float4 ssrtgi = gHistory.Load(int3(px, 0)); // Read from Temporal output
#endif

    float depthRaw = gDepth.Load(int3(px, 0));
    
    // Early out for sky/invalid
    if (IsInvalidOrSkyDepth(depthRaw))
    {
        gHistOut[px] = baseColor;
        return;
    }
    

    // ============================================================
    // [NEW] Calculate linear depth for distance attenuation
    // ============================================================
    float linearDepth = LinearizeDepth(depthRaw);
    float distAttenuation = CompositeDistanceAttenuation(linearDepth);
    
    // Extract GI and AO (saturate once for safety)
    float3 gi = ssrtgi.rgb;
    float ao = saturate(ssrtgi.a);
    
    // ===========================================
    // PRECOMPUTED CONSTANTS
    // ===========================================
    const float aoPower = 1.6;
    const float aoStrengthMult = 2.8;
    const float3 lumWeights = float3(0.299, 0.587, 0.114);
    
#if ENABLE_DEBUG_MODES
#if DEBUG_MODE == 16
    gHistOut[px] = float4(gi, 1);
    return;
#elif DEBUG_MODE == 17
    gHistOut[px] = float4(ao, ao, ao, 1);
    return;
#elif DEBUG_MODE == 23
    // [NEW] Distance attenuation visualization
    // Shows how much effect is applied based on distance
    // Green = full effect, Red = attenuated
    {
        float attVis = distAttenuation;
        gHistOut[px] = float4(1.0 - attVis, attVis, 0.0, 1.0);
        return;
    }
#elif DEBUG_MODE == 18
    // Energy comparison visualization with susceptibility factors
    float surfaceEnergy = dot(baseColor.rgb, lumWeights);
    float giEnergy = dot(gi, lumWeights);
    
    // SUSCEPTIBILITY FACTOR 1: COLOR SIMILARITY
    float3 surfaceChroma = baseColor.rgb / max(surfaceEnergy, 0.01);
    float3 giChroma = gi / max(giEnergy, 0.01);
    float colorSimilarity = dot(normalize(surfaceChroma + 0.001), normalize(giChroma + 0.001));
    float colorBoost = lerp(1.0, 2.0, saturate(colorSimilarity * 0.5 + 0.5));
    
    // SUSCEPTIBILITY FACTOR 2: GRAY SURFACE BOOST
    float maxChannel = max(max(baseColor.r, baseColor.g), baseColor.b);
    float minChannel = min(min(baseColor.r, baseColor.g), baseColor.b);
    float saturation = (maxChannel > 0.001) ? (maxChannel - minChannel) / maxChannel : 0.0;
    float grayBoost = lerp(5.5, 1.0, saturation);
    
    // SUSCEPTIBILITY FACTOR 3: DARKNESS (with AO)
    float surfaceEnergyWithAO = surfaceEnergy * ao;
    float darknessBoost = lerp(2.0, 1.0, saturate(surfaceEnergyWithAO * 2.0));
    
    // COMBINED SUSCEPTIBILITY
    float susceptibility = colorBoost * grayBoost * darknessBoost;
    
    if (giEnergy > 0.001)
    {
        float diff = giEnergy - surfaceEnergy;
        float absDiff = abs(diff);
        float intensity = saturate(absDiff * 2.0 * susceptibility);
        
        if (diff > 0.0)
            gHistOut[px] = float4(0, intensity, 0, 1);
        else
            gHistOut[px] = float4(intensity, 0, 0, 1);
    }
    else
    {
        gHistOut[px] = float4(0.1, 0.1, 0.1, 1);
    }
    return;
#elif DEBUG_MODE == 19
    float surfaceEnergy = dot(baseColor.rgb, lumWeights);
    float giEnergy = dot(gi, lumWeights);
    
    if (giEnergy > 0.001)
    {
        if (px.x < g.Width / 2)
        {
            gHistOut[px] = float4(gi, 1);
        }
        else
        {
            float3 surfaceAlbedo = baseColor.rgb / max(surfaceEnergy, 0.01);
            surfaceAlbedo = saturate(surfaceAlbedo);
            float3 bounce = gi * surfaceAlbedo;
            bounce = min(bounce, gi);
            gHistOut[px] = float4(bounce, 1);
        }
    }
    else
    {
        gHistOut[px] = float4(0.1, 0.1, 0.1, 1);
    }
    return;
#elif DEBUG_MODE == 2
    float aoVizDbg = pow(ao, aoPower);
    float aoMixDbg = lerp(1.0, aoVizDbg, g.AoStrength * aoStrengthMult);
    gHistOut[px] = float4(aoMixDbg, aoMixDbg, aoMixDbg, 1);
    return;
#elif DEBUG_MODE == 20
    {
        float2 uv = (float2(px) + 0.5) / float2(g.Width, g.Height);
        uint mvW, mvH;
        gMotionVectors.GetDimensions(mvW, mvH);
        float2 motion = float2(0, 0);
        if (mvW > 0 && mvH > 0)
        {
            int2 mvCoord = int2(uv * float2(mvW, mvH));
            mvCoord = clamp(mvCoord, int2(0, 0), int2(mvW - 1, mvH - 1));
            motion = gMotionVectors.Load(int3(mvCoord, 0)).xy;
        }
        float2 motionVis = motion * 50.0 + 0.5;
        gHistOut[px] = float4(motionVis.x, motionVis.y, 0.5, 1);
        return;
    }
#elif DEBUG_MODE == 21
    {
        float giLumDbg = dot(gi, lumWeights);
        gHistOut[px] = float4(giLumDbg * 2.0, giLumDbg * 2.0, giLumDbg * 2.0, 1);
        return;
    }
#elif DEBUG_MODE == 22
    {
        float flash = frac(float(g.FrameIndex) * 0.5);
        if (flash < 0.25)
            gHistOut[px] = float4(gi * 3.0, 1);
        else
            gHistOut[px] = float4(gi * 3.0, 1);
        return;
    }
#elif DEBUG_MODE == 0
    if (px.x < g.Width / 2)
    {
        gHistOut[px] = baseColor;
        return;
    }
#endif
#endif
    
    // ===========================================
    // GI PROCESSING - with susceptibility-based color bleeding
    // ===========================================
    float3 baseLin = baseColor.rgb;
    float baseLuma = dot(baseLin, lumWeights);
    float giLum = dot(gi, lumWeights);
    
    float3 result;
    if (giLum > 0.001)
    {
        // SUSCEPTIBILITY FACTOR 1: COLOR SIMILARITY
        float3 surfaceChroma = baseLin / max(baseLuma, 0.01);
        float3 giChroma = gi / max(giLum, 0.01);
        float colorSimilarity = dot(normalize(surfaceChroma + 0.001), normalize(giChroma + 0.001));
        float colorBoost = lerp(1.0, 2.0, saturate(colorSimilarity * 0.5 + 0.5));
        
        // SUSCEPTIBILITY FACTOR 2: GRAY SURFACE BOOST
        float maxChannel = max(max(baseLin.r, baseLin.g), baseLin.b);
        float minChannel = min(min(baseLin.r, baseLin.g), baseLin.b);
        float saturation = (maxChannel > 0.001) ? (maxChannel - minChannel) / maxChannel : 0.0;
        float grayBoost = lerp(1.5, 1.0, saturation);
        
        // SUSCEPTIBILITY FACTOR 3: DARKNESS (with AO)
        float surfaceEnergyWithAO = baseLuma * ao;
        float darknessBoost = lerp(0.5, 0.5, saturate(surfaceEnergyWithAO));
        
        // COMBINED SUSCEPTIBILITY (max ~6x for dark gray)
        float susceptibility = colorBoost * grayBoost * darknessBoost;
        
        // PHYSICALLY CORRECT COLOR BLEEDING
        float3 surfaceAlbedo = baseLin / max(baseLuma, 0.01);
        surfaceAlbedo = saturate(surfaceAlbedo);
        
        // ENERGY-BASED BOUNCE
        float giEnergy = giLum;
        float3 bounce = gi * surfaceAlbedo;
        bounce = min(bounce, gi);
        
        // ============================================================
        // [MODIFIED] Apply distance attenuation to GI strength
        // ============================================================
        float bounceStrength = g.GiStrength * susceptibility * giEnergy * 0.25;
        
        result = baseLin + bounce * bounceStrength;
    }
    else
    {
        result = baseLin;
    }
    
    // ===========================================
    // AO PROCESSING
    // ===========================================
    // ============================================================
    // [MODIFIED] Apply distance attenuation to AO strength
    // ============================================================
    float aoStrengthFinal = g.AoStrength * aoStrengthMult * distAttenuation;
    
    float aoVal = pow(ao, aoPower);
    float aoFactor = lerp(1.0, aoVal, aoStrengthFinal);
    
    // Clamp minimum (preserve some color in dark areas)
    aoFactor = max(aoFactor, 0.65);
    //aoFactor = max(aoFactor, 0.35 * (1.0 - distAttenuation));
    
    // Apply AO
    result *= aoFactor;
    
    // Ensure non-negative
    result = max(result, 0.0);
    
    gHistOut[px] = float4(result, baseColor.a);
}
