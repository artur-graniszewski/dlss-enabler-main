#include "SsrtgiCommon.hlsl"

// ============================================================
// SSRTGI TEMPORAL ACCUMULATION - DLSS-FRIENDLY VERSION
// ============================================================
// Features:
// - Motion vector reprojection
// - Variance-based neighborhood clamping
// - Depth rejection for disocclusion detection
// - Adaptive alpha based on confidence
//
// DLSS-FRIENDLY improvements:
// - Separate GI/AO alpha (different stability needs)
// - Soft chroma clamp (hard Y only) - reduces "plastic" look
// - Diff-based responsiveness (pseudo reactive mask)
// - Higher base responsiveness for better DLSS temporal
// - Depth history confidence fallback
// ============================================================

// Master toggle
#define TEMPORAL_ENABLED 1

// Debug: write fixed color to test if histOut is working
#define DEBUG_FIXED_OUTPUT 0

// Debug: just pass through current without any processing
#define DEBUG_PASSTHROUGH 0

// Debug: show raw history (what's in gHistory/t2)
#define DEBUG_SHOW_HISTORY 0

// Debug: show alpha values (R=alphaGI, G=alphaAO, B=0)
#define DEBUG_SHOW_ALPHA 0

// Debug: show disocclusion info (R=depthDiff, G=historyValid, B=disoccluded)
#define DEBUG_SHOW_DISOCCLUSION 0

// Debug: set to 1 to test if gOut1 reads correctly
#define DEBUG_TEST_GOUT1 0

// Debug: show motion vectors magnitude and direction
#define DEBUG_SHOW_MOTION 0

// DLSS mode: 0 = Classic (stable), 1 = DLSS-friendly (responsive)
#define DLSS_FRIENDLY 0

// ============================================================
// TUNING PARAMETERS
// ============================================================

#if DLSS_FRIENDLY
    // DLSS-friendly: more responsive, let DLSS do the heavy temporal lifting
static const float BASE_ALPHA_GI = 0.25;
static const float BASE_ALPHA_AO = 0.22;
static const float CLAMP_EXPANSION = 1.60;
static const float CHROMA_CLAMP_STRENGTH = 0.35; // 0 = no clamp, 1 = hard clamp
#else
    // Classic: maximum stability (may ghost with DLSS)
static const float BASE_ALPHA_GI = 0.03;
static const float BASE_ALPHA_AO = 0.05;
static const float CLAMP_EXPANSION = 1.25;
static const float CHROMA_CLAMP_STRENGTH = 1.0;
#endif

// Disocclusion detection threshold (relative depth difference)
static const float DEPTH_REJECT_THRESHOLD = 0.1;

// ============================================================
// COLOR SPACE CONVERSION
// ============================================================

float3 RGBToYCoCg(float3 rgb)
{
    float Y = dot(rgb, float3(0.25, 0.5, 0.25));
    float Co = dot(rgb, float3(0.5, 0.0, -0.5));
    float Cg = dot(rgb, float3(-0.25, 0.5, -0.25));
    return float3(Y, Co, Cg);
}

float3 YCoCgToRGB(float3 ycocg)
{
    float Y = ycocg.x;
    float Co = ycocg.y;
    float Cg = ycocg.z;
    return float3(Y + Co - Cg, Y + Cg, Y - Co - Cg);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 p = dtid.xy;
    
    uint outW, outH;
    gHistOut.GetDimensions(outW, outH);
    
    if (p.x >= outW || p.y >= outH)
        return;

#if DEBUG_FIXED_OUTPUT
    // Test: write magenta to verify temporal output reaches Composite
    gHistOut[p] = float4(1.0, 0.0, 1.0, 0.5);
    return;
#endif

    // Current frame data (from denoise pass)
    float4 current = gOut1[p];

#if DEBUG_PASSTHROUGH
    // Test: just pass through gOut1 without processing
    gHistOut[p] = current;
    return;
#endif
    
#if DEBUG_TEST_GOUT1
    // Test: Check if gOut1 has valid dimensions and data
    // If gOut1 is bound correctly, we should see:
    // - Cyan where GI luminance < 0.1 (dark/no GI)
    // - Yellow where GI luminance >= 0.1 (has GI)
    float lum = dot(current.rgb, float3(0.299, 0.587, 0.114));
    if (lum < 0.1)
        gHistOut[p] = float4(0, 1, 1, 1); // Cyan = no/dark GI
    else
        gHistOut[p] = float4(1, 1, 0, 1); // Yellow = has GI
    return;
#endif

#if !TEMPORAL_ENABLED
    // Temporal disabled - just pass through
    gHistOut[p] = current;
    return;
#else
    
    float3 currentGI = current.rgb;
    float currentAO = current.a;
    
    // Get current depth for disocclusion detection
    int2 depthCoord = MapToDepthCoord(p);
    float depthRaw = gDepth.Load(int3(depthCoord, 0));
    float currentDepth = LinearizeDepth(depthRaw);
    
    // Sky check - no temporal for sky pixels
    if (IsInvalidOrSkyDepth(depthRaw))
    {
        gHistOut[p] = current;
        return;
    }
    
    // ===========================================
    // REPROJECTION using motion vectors
    // ===========================================
    float2 uv = (float2(p) + 0.5) / float2(outW, outH);
    float2 motion = float2(0, 0);
    
    // DEBUG: Motion vectors disabled for testing
#define DISABLE_MOTION_VECTORS 0
    
    // Clamp motion to reasonable values (prevents camera sway ghosting)
#define MOTION_CLAMP_ENABLED 0
#define MOTION_MAX_UV 0.02  // Max 2% of screen per frame
    
    // DEBUG: Show crosshair moving with average motion
#define DEBUG_MOTION_CROSSHAIR 0
    
#if !DISABLE_MOTION_VECTORS
    uint mvW, mvH;
    gMotionVectors.GetDimensions(mvW, mvH);
    if (mvW > 0 && mvH > 0)
    {
        int2 mvCoord = int2(uv * float2(mvW, mvH));
        mvCoord = clamp(mvCoord, int2(0, 0), int2(mvW - 1, mvH - 1));
        
        // Read raw motion vectors
        motion = gMotionVectors.Load(int3(mvCoord, 0)).xy;
        
        // ============================================================
        // MVScale interpretation (from DLSS documentation):
        //
        // MVScale = resolution (e.g. 1129, 635):
        //   MV are in UV space (0-1). DLSS multiplies by resolution
        //   to get pixel motion. For our reprojection we use UV directly.
        //   › Don't multiply, use raw MV
        //
        // MVScale = 1.0:
        //   MV are already in the format DLSS expects (pixel space).
        //   For UV reprojection we need to divide by resolution.
        //   › Divide by MV buffer resolution
        //
        // MVScale negative:
        //   MV direction is inverted (forward vs backward vectors)
        //   › Apply sign correction
        // ============================================================
        
        float absScaleX = abs(g.MVScaleX);
        float absScaleY = abs(g.MVScaleY);
        
        // Check if MVScale ? resolution (MV in UV space)
        // or MVScale ? 1 (MV in pixel space)
        bool mvInUVSpace = (absScaleX > 10.0 || absScaleY > 10.0);
        
        if (mvInUVSpace)
        {
            // MV already in UV space - use directly for reprojection
            // (DLSS would multiply by resolution, but we want UV)
        }
        else
        {
            // MV in pixel space - convert to UV
            motion /= float2(mvW, mvH);
        }
        
        // Handle negative MVScale (inverted direction)
        if (g.MVScaleX < 0.0)
            motion.x = -motion.x;
        if (g.MVScaleY < 0.0)
            motion.y = -motion.y;
        
        if (g.FlipMotionVectors > 0.5)
            motion.y = -motion.y;
        
#if MOTION_CLAMP_ENABLED
        float motionLen = length(motion);
        if (motionLen > MOTION_MAX_UV)
            motion *= MOTION_MAX_UV / motionLen;
#endif
    }
#endif
    
#if DEBUG_MOTION_CROSSHAIR
    {
        // Sample motion from center of screen
        uint mvW, mvH;
        gMotionVectors.GetDimensions(mvW, mvH);
        float2 centerMotion = float2(0, 0);
        float2 rawMotionValue = float2(0, 0);
        
        if (mvW > 0 && mvH > 0)
        {
            int2 centerCoord = int2(mvW / 2, mvH / 2);
            rawMotionValue = gMotionVectors.Load(int3(centerCoord, 0)).xy;
            
            // No MVScale - motion is already in UV
            centerMotion = rawMotionValue;
            if (g.FlipMotionVectors > 0.5)
                centerMotion.y = -centerMotion.y;
        }
        
        float2 pixelUV = uv;
        float thickness = 3.0 / float(outW);
        float armLength = 50.0 / float(outW);
        
        // Yellow crosshair: motion in UV (should be small movements now)
        float2 yellowCrosshair = float2(0.5, 0.5) + centerMotion;
        
        bool onYellowV = abs(pixelUV.x - yellowCrosshair.x) < thickness && 
                         abs(pixelUV.y - yellowCrosshair.y) < armLength;
        bool onYellowH = abs(pixelUV.y - yellowCrosshair.y) < thickness && 
                         abs(pixelUV.x - yellowCrosshair.x) < armLength;
        
        if (onYellowV || onYellowH)
        {
            gHistOut[p] = float4(1.0, 1.0, 0.0, 1.0);
            return;
        }
        
        // Green = static center reference
        float2 staticCenter = float2(0.5, 0.5);
        float smallThick = 2.0 / float(outW);
        float smallArm = 30.0 / float(outW);
        
        bool onStaticV = abs(pixelUV.x - staticCenter.x) < smallThick && 
                         abs(pixelUV.y - staticCenter.y) < smallArm;
        bool onStaticH = abs(pixelUV.y - staticCenter.y) < smallThick && 
                         abs(pixelUV.x - staticCenter.x) < smallArm;
        
        if (onStaticV || onStaticH)
        {
            gHistOut[p] = float4(0.0, 1.0, 0.0, 1.0);
            return;
        }
        
        // Debug info in top-left: raw motion × 100 (should be small now)
        if (p.x < 200 && p.y < 50)
        {
            float4 current = gOut1[p];
            float debugR = saturate(abs(rawMotionValue.x) * 100.0);
            float debugG = saturate(abs(rawMotionValue.y) * 100.0);
            gHistOut[p] = float4(debugR, debugG, 0.0, current.a);
            return;
        }
    }
#endif
    
    float2 historyUV = uv + motion;
    bool historyValid = (historyUV.x >= 0.0 && historyUV.x <= 1.0 &&
                         historyUV.y >= 0.0 && historyUV.y <= 1.0);

#if DEBUG_SHOW_MOTION
    // Visualize motion vectors: R=X motion, G=Y motion, B=magnitude
    // Scale by 10 to make small motions visible
    float2 motionVis = motion * 10.0 + 0.5;  // 0.5 = no motion (gray)
    float magnitude = length(motion) * 50.0;
    gHistOut[p] = float4(motionVis.x, motionVis.y, magnitude, 1.0);
    return;
#endif
    
    // ===========================================
    // SAMPLE HISTORY with bilinear filtering
    // ===========================================
    float4 history = float4(0, 0, 0, 1);
    float historyDepth = 0;
    bool hasDepthHistory = false;
    
    if (historyValid)
    {
        float2 historyCoord = historyUV * float2(outW, outH) - 0.5;
        int2 historyCoordInt = int2(floor(historyCoord));
        float2 historyFrac = frac(historyCoord);
        
        int2 c00 = clamp(historyCoordInt, int2(0, 0), int2(outW - 1, outH - 1));
        int2 c10 = clamp(historyCoordInt + int2(1, 0), int2(0, 0), int2(outW - 1, outH - 1));
        int2 c01 = clamp(historyCoordInt + int2(0, 1), int2(0, 0), int2(outW - 1, outH - 1));
        int2 c11 = clamp(historyCoordInt + int2(1, 1), int2(0, 0), int2(outW - 1, outH - 1));
        
        float4 h00 = gHistory[c00];
        float4 h10 = gHistory[c10];
        float4 h01 = gHistory[c01];
        float4 h11 = gHistory[c11];
        
        float4 h0 = lerp(h00, h10, historyFrac.x);
        float4 h1 = lerp(h01, h11, historyFrac.x);
        history = lerp(h0, h1, historyFrac.y);

#if DEBUG_SHOW_HISTORY
        // Show raw history value
        gHistOut[p] = float4(history.rgb, 1);
        return;
#endif
        
        // Sample history depth
        uint dhW, dhH;
        gDepthHistory.GetDimensions(dhW, dhH);
        if (dhW > 0 && dhH > 0)
        {
            hasDepthHistory = true;
            int2 depthHistCoord = int2(historyUV * float2(dhW, dhH));
            depthHistCoord = clamp(depthHistCoord, int2(0, 0), int2(dhW - 1, dhH - 1));
            float depthHistRaw = gDepthHistory.Load(int3(depthHistCoord, 0));
            historyDepth = LinearizeDepth(depthHistRaw);
        }
        else
        {
            historyDepth = currentDepth;
        }
    }
    
    // ===========================================
    // NEIGHBORHOOD CLAMPING (variance-based)
    // ===========================================
    float3 neighborMin = float3(9999, 9999, 9999);
    float3 neighborMax = float3(-9999, -9999, -9999);
    float3 neighborSum = float3(0, 0, 0);
    float3 neighborSumSq = float3(0, 0, 0);
    float aoMin = 9999;
    float aoMax = -9999;
    
    [unroll]
    for (int dy = -1; dy <= 1; dy++)
    {
        [unroll]
        for (int dx = -1; dx <= 1; dx++)
        {
            int2 sampleCoord = int2(p) + int2(dx, dy);
            sampleCoord = clamp(sampleCoord, int2(0, 0), int2(outW - 1, outH - 1));
            
            float4 sample_val = gOut1[sampleCoord];
            float3 sampleYCoCg = RGBToYCoCg(sample_val.rgb);
            
            neighborMin = min(neighborMin, sampleYCoCg);
            neighborMax = max(neighborMax, sampleYCoCg);
            neighborSum += sampleYCoCg;
            neighborSumSq += sampleYCoCg * sampleYCoCg;
            
            aoMin = min(aoMin, sample_val.a);
            aoMax = max(aoMax, sample_val.a);
        }
    }
    
    float3 neighborMean = neighborSum / 9.0;
    float3 neighborVar = (neighborSumSq / 9.0) - (neighborMean * neighborMean);
    float3 neighborStd = sqrt(max(neighborVar, float3(0.0001, 0.0001, 0.0001)));
    
    float3 clampMin = neighborMean - CLAMP_EXPANSION * neighborStd;
    float3 clampMax = neighborMean + CLAMP_EXPANSION * neighborStd;
    
    // ===========================================
    // CLAMP HISTORY - DLSS-FRIENDLY VERSION
    // ===========================================
    float3 historyYCoCg = RGBToYCoCg(history.rgb);
    
    // Hard clamp luminance (Y) - always needed
    historyYCoCg.x = clamp(historyYCoCg.x, clampMin.x, clampMax.x);
    
    // Soft clamp chroma (Co, Cg) - reduces "plastic" look with DLSS
    float2 chromaOrig = historyYCoCg.yz;
    float2 chromaClamped = clamp(chromaOrig, clampMin.yz, clampMax.yz);
    historyYCoCg.yz = lerp(chromaOrig, chromaClamped, CHROMA_CLAMP_STRENGTH);
    
    float3 clampedHistoryRGB = YCoCgToRGB(historyYCoCg);
    float clampedHistoryAO = clamp(history.a, aoMin, aoMax);
    
    // ===========================================
    // DISOCCLUSION DETECTION
    // ===========================================
    float depthDiff = abs(currentDepth - historyDepth) / max(currentDepth, 0.001);
    bool disoccluded = (depthDiff > DEPTH_REJECT_THRESHOLD) || !historyValid;

#if DEBUG_SHOW_DISOCCLUSION
    // R = depthDiff (clamped to 0-1), G = historyValid, B = disoccluded
    gHistOut[p] = float4(saturate(depthDiff), historyValid ? 1.0 : 0.0, disoccluded ? 1.0 : 0.0, 1);
    return;
#endif
    
    // ===========================================
    // ADAPTIVE ALPHA - SEPARATE GI/AO
    // ===========================================
    float alphaGI = BASE_ALPHA_GI;
    float alphaAO = BASE_ALPHA_AO;
    
    if (disoccluded)
    {
        alphaGI = 1.0;
        alphaAO = 1.0;
    }
    else
    {
        float variance = dot(neighborStd, float3(1.0, 0.5, 0.5));
        
        // Variance-based responsiveness
#if DLSS_FRIENDLY
        alphaGI = lerp(BASE_ALPHA_GI, 0.45, saturate(variance * 10.0));
        alphaAO = lerp(BASE_ALPHA_AO, 0.35, saturate(variance * 8.0));
#else
        alphaGI = lerp(BASE_ALPHA_GI, 0.30, saturate(variance * 10.0));
        alphaAO = lerp(BASE_ALPHA_AO, 0.25, saturate(variance * 8.0));
#endif
        
        // Motion-based responsiveness
        float motionLen = length(motion) * float(outW);
#if DLSS_FRIENDLY
        alphaGI = lerp(alphaGI, 0.60, saturate(motionLen * 0.5));
        alphaAO = lerp(alphaAO, 0.50, saturate(motionLen * 0.4));
#else
        alphaGI = lerp(alphaGI, 0.40, saturate(motionLen * 0.5));
        alphaAO = lerp(alphaAO, 0.35, saturate(motionLen * 0.4));
#endif
        
#if DLSS_FRIENDLY
        // Diff-based responsiveness (pseudo reactive mask)
        // Responds faster when current differs significantly from history
        float lumCur = dot(currentGI, float3(0.299, 0.587, 0.114));
        float lumHist = dot(clampedHistoryRGB, float3(0.299, 0.587, 0.114));
        float lumDiff = abs(lumCur - lumHist) / max(max(lumCur, lumHist), 0.001);
        float diffFactor = saturate(lumDiff * 2.5);
        alphaGI = max(alphaGI, lerp(BASE_ALPHA_GI, 0.65, diffFactor));
        
        // Depth history confidence fallback
        // If no depth history, reduce trust in history buffer
        if (!hasDepthHistory)
        {
            float depthConfidence = 0.4;
            alphaGI = lerp(alphaGI, 0.5, 1.0 - depthConfidence);
            alphaAO = lerp(alphaAO, 0.4, 1.0 - depthConfidence);
        }
#endif
    }
    
    // ===========================================
    // BLEND CURRENT AND HISTORY (separate GI/AO)
    // ===========================================
#if DEBUG_SHOW_ALPHA
    gHistOut[p] = float4(alphaGI, alphaAO, 0, 1);
    return;
#endif

    float3 finalGI = lerp(clampedHistoryRGB, currentGI, alphaGI);
    float finalAO = lerp(clampedHistoryAO, currentAO, alphaAO);
    
    gHistOut[p] = float4(finalGI, finalAO);
    
#endif // TEMPORAL_ENABLED
}
