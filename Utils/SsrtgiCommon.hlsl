#ifndef SSRTGI_COMMON_HLSL
#define SSRTGI_COMMON_HLSL

// ============================================================
// IMPROVED SSRTGI COMMON - Extended for GTAO-style processing
// ============================================================

struct Params
{
    uint Width;
    uint Height;
    float RadiusPx;
    float AoStrength;
    float GiStrength;
    float DepthReject;
    float TemporalAlpha;
    float NormalizedDepth;
    uint FrameIndex;
    float FlipMotionVectors;
    float DepthInverted;
    float JitterX;
    float JitterY;
    
    // Camera parameters
    float CameraFOV;
    float CameraNear;
    float CameraFar;
    float CameraAspectRatio;
    
    // Motion vector scale
    float MVScaleX;
    float MVScaleY;
    
    // New parameters for UI control
    uint NumRays; // Ray count: 4, 8, 16, 32
    float FalloffEnd; // Distance falloff end (10-200)
};

cbuffer CB0 : register(b0)
{
    Params g;
}

Texture2D<float4> gColor : register(t0);
Texture2D<float> gDepth : register(t1);
Texture2D<float4> gHistory : register(t2);
Texture2D<float2> gMotionVectors : register(t3);
Texture2D<float> gDepthHistory : register(t4);
Texture2D<float2> gHZB : register(t5); // HZB pyramid: .x = minZ, .y = maxZ

RWTexture2D<float4> gOut0 : register(u0);
RWTexture2D<float4> gOut1 : register(u1);
RWTexture2D<float4> gHistOut : register(u2);

SamplerState gPointClamp : register(s0);

// ============================================
// CONSTANTS
// ============================================
static const float SSRTGI_EPS_DEPTH = 1e-6;
static const float SSRTGI_FAR_VALUE = 10000.0;
static const float SSRTGI_NEAR_VALUE = 0.01;
static const float PI = 3.14159265358979;
static const float HALF_PI = 1.5707963267949;
static const float TWO_PI = 6.28318530717959;

// ============================================
// TEXTURE DIMENSION HELPERS
// ============================================
uint2 GetDepthDimensions()
{
    uint w, h;
    gDepth.GetDimensions(w, h);
    if (w == 0 || h == 0)
        return uint2(g.Width, g.Height);
    return uint2(w, h);
}

uint2 GetColorDimensions()
{
    uint w, h;
    gColor.GetDimensions(w, h);
    if (w == 0 || h == 0)
        return uint2(g.Width, g.Height);
    return uint2(w, h);
}

uint2 GetMvDimensions()
{
    uint w, h;
    gMotionVectors.GetDimensions(w, h);
    if (w == 0 || h == 0)
        return uint2(g.Width, g.Height);
    return uint2(w, h);
}

// ============================================
// COORDINATE MAPPING HELPERS
// ============================================
bool MapToDepthCoordChecked(uint2 p, out int2 depthP)
{
    uint2 depthSize = GetDepthDimensions();
    
    if (depthSize.x == g.Width && depthSize.y == g.Height)
    {
        depthP = int2(p);
        return true;
    }
    
    float2 uv = (float2(p) + 0.5) / float2(g.Width, g.Height);
    int2 c = int2(uv * float2(depthSize));
    
    if (c.x < 0 || c.y < 0 || c.x >= int(depthSize.x) || c.y >= int(depthSize.y))
    {
        depthP = int2(0, 0);
        return false;
    }
    
    depthP = c;
    return true;
}

bool MapToColorCoordChecked(uint2 p, out int2 colorP)
{
    uint2 colorSize = GetColorDimensions();
    
    if (colorSize.x == g.Width && colorSize.y == g.Height)
    {
        colorP = int2(p);
        return true;
    }
    
    float2 uv = (float2(p) + 0.5) / float2(g.Width, g.Height);
    int2 c = int2(uv * float2(colorSize));
    
    if (c.x < 0 || c.y < 0 || c.x >= int(colorSize.x) || c.y >= int(colorSize.y))
    {
        colorP = int2(0, 0);
        return false;
    }
    
    colorP = c;
    return true;
}

bool MapToMvCoordChecked(uint2 p, out int2 mvP)
{
    uint2 mvSize = GetMvDimensions();
    
    if (mvSize.x == g.Width && mvSize.y == g.Height)
    {
        mvP = int2(p);
        return true;
    }
    
    float2 uv = (float2(p) + 0.5) / float2(g.Width, g.Height);
    int2 c = int2(uv * float2(mvSize));
    
    if (c.x < 0 || c.y < 0 || c.x >= int(mvSize.x) || c.y >= int(mvSize.y))
    {
        mvP = int2(0, 0);
        return false;
    }
    
    mvP = c;
    return true;
}

int2 MapToDepthCoord(uint2 p)
{
    int2 depthP;
    if (MapToDepthCoordChecked(p, depthP))
        return depthP;
    uint2 depthSize = GetDepthDimensions();
    return clamp(int2(p), int2(0, 0), int2(depthSize) - int2(1, 1));
}

int2 MapToColorCoord(uint2 p)
{
    int2 colorP;
    if (MapToColorCoordChecked(p, colorP))
        return colorP;
    uint2 colorSize = GetColorDimensions();
    return clamp(int2(p), int2(0, 0), int2(colorSize) - int2(1, 1));
}

int2 MapToMvCoord(uint2 p)
{
    int2 mvP;
    if (MapToMvCoordChecked(p, mvP))
        return mvP;
    uint2 mvSize = GetMvDimensions();
    return clamp(int2(p), int2(0, 0), int2(mvSize) - int2(1, 1));
}

// ============================================
// UTILITY FUNCTIONS
// ============================================
float2 PixelToUv(uint2 p)
{
    return (float2(p) + 0.5) / float2(g.Width, g.Height);
}

// High quality hash functions for noise generation
float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 Hash22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}

float3 Hash32(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return frac((p3.xxy + p3.yzz) * p3.zyx);
}

// Interleaved gradient noise - better quality than pure random
float InterleavedGradientNoise(float2 position)
{
    return frac(52.9829189 * frac(0.06711056 * position.x + 0.00583715 * position.y));
}

// R2 quasi-random sequence - better distribution
float2 R2Sequence(uint index)
{
    const float g = 1.32471795724474602596;
    const float a1 = 1.0 / g;
    const float a2 = 1.0 / (g * g);
    return frac(float2(a1, a2) * float(index));
}

// ============================================
// CAMERA PARAMETER HELPERS
// ============================================
float GetCameraFOV()
{
    return g.CameraFOV;
}
float GetCameraNear()
{
    return max(g.CameraNear, 0.001);
}
float GetCameraFar()
{
    return max(g.CameraFar, GetCameraNear() + 1.0);
}
float GetCameraAspect()
{
    return g.CameraAspectRatio;
}

// ============================================
// DEPTH UTILITIES
// ============================================

bool IsInvalidOrSkyDepth(float d)
{
    // NaN or negative
    if (isnan(d) || d < 0.0)
        return true;
    
    // Handle reversed-Z (most modern games use this)
    if (g.DepthInverted > 0.5)
    {
        // Reversed-Z: near=1.0, far=0.0
        // Sky/infinite distance is at or very close to 0
        // Valid geometry is between ~0.0001 and 1.0
        return d < 0.0001; // Sky threshold
    }
    else
    {
        // Standard-Z: near=0.0, far=1.0
        // Sky/infinite distance is at or very close to 1
        return d > 0.9999; // Sky threshold
    }
}

bool IsSkyDepth(float d)
{
    return IsInvalidOrSkyDepth(d);
}

float LinearizeDepth(float d)
{
    float cameraNear = GetCameraNear();
    float cameraFar = GetCameraFar();
    
    float rd = saturate(d);
    
    if (g.DepthInverted > 0.5)
    {
        // Reversed-Z: near=1.0, far=0.0
        // d=1 -> near, d=0 -> far
        float denom = cameraNear + rd * (cameraFar - cameraNear);
        if (denom < SSRTGI_EPS_DEPTH)
            return SSRTGI_FAR_VALUE;
        return (cameraNear * cameraFar) / denom;
    }
    else
    {
        // Standard-Z: near=0.0, far=1.0
        float denom = cameraFar - rd * (cameraFar - cameraNear);
        if (denom < SSRTGI_EPS_DEPTH)
            return SSRTGI_FAR_VALUE;
        return (cameraNear * cameraFar) / denom;
    }
}

float DepthLinear_SL(float d)
{
    float rd = saturate(d);
    
    if (g.DepthInverted > 0.5)
        return rcp(max(rd, SSRTGI_EPS_DEPTH));
    else
        return rcp(max(1.0 - rd, SSRTGI_EPS_DEPTH));
}

float RelativeLinearDepthDiff_SL(float a, float b)
{
    float la = DepthLinear_SL(a);
    float lb = DepthLinear_SL(b);
    return abs(la - lb) / max(min(la, lb), SSRTGI_EPS_DEPTH);
}

float RelativeLinearDepthDiff(float a, float b)
{
    float la = LinearizeDepth(a);
    float lb = LinearizeDepth(b);
    float minDepth = min(la, lb);
    if (minDepth < SSRTGI_NEAR_VALUE)
        return 0.0;
    return abs(la - lb) / minDepth;
}

// ============================================
// 3D RECONSTRUCTION
// ============================================
float3 ReconstructViewPos(float2 uv, float depth)
{
    float linearDepth = LinearizeDepth(depth);
    
    if (linearDepth >= SSRTGI_FAR_VALUE * 0.99)
        return float3(0, 0, SSRTGI_FAR_VALUE);
    
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    
    float aspect = GetCameraAspect();
    float tanHalfFov = tan(g.CameraFOV * 0.5);
    
    float3 viewPos;
    viewPos.x = ndc.x * tanHalfFov * aspect * linearDepth;
    viewPos.y = ndc.y * tanHalfFov * linearDepth;
    viewPos.z = linearDepth;
    
    return viewPos;
}

float2 ProjectToUV(float3 viewPos)
{
    if (viewPos.z <= SSRTGI_NEAR_VALUE)
        return float2(-1, -1);
    
    float aspect = GetCameraAspect();
    float tanHalfFov = tan(g.CameraFOV * 0.5);
    
    float denom = viewPos.z * tanHalfFov;
    if (denom < SSRTGI_EPS_DEPTH)
        return float2(-1, -1);
    
    float2 ndc;
    ndc.x = viewPos.x / (denom * aspect);
    ndc.y = -viewPos.y / denom;
    
    if (abs(ndc.x) > 2.0 || abs(ndc.y) > 2.0)
        return float2(-1, -1);
    
    return ndc * 0.5 + 0.5;
}

// ============================================
// NORMAL RECONSTRUCTION - IMPROVED VERSION
// ============================================
// Fixes for grazing angles and close-to-wall cases:
// 1. Better depth discontinuity detection using relative depth difference
// 2. Improved derivative selection for steep surfaces
// 3. More robust fallback that considers surface orientation
// 4. Removed overly aggressive threshold that rejected valid samples

float3 ReconstructNormal(uint2 p, float depthC)
{
    if (IsInvalidOrSkyDepth(depthC))
        return float3(0, 0, -1); // Facing camera
    
    float linDepthC = LinearizeDepth(depthC);
    
    uint depthW, depthH;
    gDepth.GetDimensions(depthW, depthH);
    
    int2 pInt = int2(p);
    int2 maxCoord = int2(depthW - 1, depthH - 1);
    
    // Sample cross pattern
    int2 coordL = clamp(pInt + int2(-1, 0), int2(0, 0), maxCoord);
    int2 coordR = clamp(pInt + int2(1, 0), int2(0, 0), maxCoord);
    int2 coordU = clamp(pInt + int2(0, -1), int2(0, 0), maxCoord);
    int2 coordD = clamp(pInt + int2(0, 1), int2(0, 0), maxCoord);
    
    float dL = gDepth.Load(int3(coordL, 0));
    float dR = gDepth.Load(int3(coordR, 0));
    float dU = gDepth.Load(int3(coordU, 0));
    float dD = gDepth.Load(int3(coordD, 0));
    
    // Linearize all depths for proper comparison
    float linL = LinearizeDepth(dL);
    float linR = LinearizeDepth(dR);
    float linU = LinearizeDepth(dU);
    float linD = LinearizeDepth(dD);
    
    // Check edge conditions
    bool atLeftEdge = (pInt.x <= 0);
    bool atRightEdge = (pInt.x >= maxCoord.x);
    bool atTopEdge = (pInt.y <= 0);
    bool atBottomEdge = (pInt.y >= maxCoord.y);
    
    // Compute UVs
    float2 invDims = 1.0 / float2(depthW, depthH);
    float2 uv = (float2(p) + 0.5) * invDims;
    float2 uvL = (float2(coordL) + 0.5) * invDims;
    float2 uvR = (float2(coordR) + 0.5) * invDims;
    float2 uvU = (float2(coordU) + 0.5) * invDims;
    float2 uvD = (float2(coordD) + 0.5) * invDims;
    
    // Reconstruct view positions
    float3 posC = ReconstructViewPos(uv, depthC);
    float3 posL = ReconstructViewPos(uvL, dL);
    float3 posR = ReconstructViewPos(uvR, dR);
    float3 posU = ReconstructViewPos(uvU, dU);
    float3 posD = ReconstructViewPos(uvD, dD);
    
    // ============================================
    // IMPROVED: Use relative depth difference for discontinuity detection
    // This works better for both near and far surfaces
    // ============================================
    // Threshold: 5% relative depth difference indicates a discontinuity
    const float DEPTH_DISCONTINUITY_THRESHOLD = 0.05;
    
    float relDiffL = abs(linL - linDepthC) / max(linDepthC, 0.001);
    float relDiffR = abs(linR - linDepthC) / max(linDepthC, 0.001);
    float relDiffU = abs(linU - linDepthC) / max(linDepthC, 0.001);
    float relDiffD = abs(linD - linDepthC) / max(linDepthC, 0.001);
    
    bool validL = !atLeftEdge && !IsInvalidOrSkyDepth(dL) && relDiffL < DEPTH_DISCONTINUITY_THRESHOLD;
    bool validR = !atRightEdge && !IsInvalidOrSkyDepth(dR) && relDiffR < DEPTH_DISCONTINUITY_THRESHOLD;
    bool validU = !atTopEdge && !IsInvalidOrSkyDepth(dU) && relDiffU < DEPTH_DISCONTINUITY_THRESHOLD;
    bool validD = !atBottomEdge && !IsInvalidOrSkyDepth(dD) && relDiffD < DEPTH_DISCONTINUITY_THRESHOLD;
    
    // ============================================
    // IMPROVED: Better derivative computation
    // Use central differences when possible, one-sided otherwise
    // ============================================
    float3 ddx = float3(0, 0, 0);
    float3 ddy = float3(0, 0, 0);
    bool hasDdx = false, hasDdy = false;
    
    // X derivative
    if (validL && validR)
    {
        // Central difference (most accurate)
        ddx = (posR - posL) * 0.5;
        hasDdx = true;
    }
    else if (validR)
    {
        // Forward difference
        ddx = posR - posC;
        hasDdx = true;
    }
    else if (validL)
    {
        // Backward difference
        ddx = posC - posL;
        hasDdx = true;
    }
    
    // Y derivative (screen Y is flipped)
    if (validU && validD)
    {
        // Central difference
        ddy = (posD - posU) * 0.5;
        hasDdy = true;
    }
    else if (validD)
    {
        // Forward difference
        ddy = posD - posC;
        hasDdy = true;
    }
    else if (validU)
    {
        // Backward difference
        ddy = posC - posU;
        hasDdy = true;
    }
    
    // ============================================
    // IMPROVED: Fallback for edge cases
    // When we can't compute proper derivatives, estimate based on view direction
    // ============================================
    float3 viewDir = normalize(posC);
    
    if (!hasDdx && !hasDdy)
    {
        // No valid neighbors at all - assume surface facing camera
        return -viewDir;
    }
    
    // If missing one derivative, try to estimate it
    if (!hasDdx && hasDdy)
    {
        // Estimate ddx perpendicular to ddy and view direction
        ddx = normalize(cross(ddy, viewDir)) * length(ddy);
        hasDdx = true;
    }
    else if (hasDdx && !hasDdy)
    {
        // Estimate ddy perpendicular to ddx and view direction
        ddy = normalize(cross(viewDir, ddx)) * length(ddx);
        hasDdy = true;
    }
    
    // Cross product gives normal
    // Order: ddy x ddx for correct winding (normal towards camera)
    float3 normal = cross(ddy, ddx);
    float len = length(normal);
    
    if (len < 0.0001)
    {
        // Degenerate case - derivatives are parallel
        return -viewDir;
    }
    
    normal /= len;
    
    // ============================================
    // IMPROVED: Robust orientation correction
    // ============================================
    // Ensure normal points towards camera (negative Z in view space generally)
    // But also consider the actual view direction for off-center pixels
    
    float NdotV = dot(normal, -viewDir);
    
    if (NdotV < 0.0)
    {
        // Normal pointing away from camera - flip it
        normal = -normal;
        NdotV = -NdotV;
    }
    
    // ============================================
    // IMPROVED: Handle extreme grazing angles better
    // Instead of replacing with view-facing normal, 
    // keep the computed normal but ensure it's valid
    // ============================================
    if (NdotV < 0.001)
    {
        // Extremely grazing angle - the surface is almost edge-on
        // Slightly bias the normal towards the camera to avoid numerical issues
        normal = normalize(normal + (-viewDir) * 0.1);
    }
    
    return normal;
}

// ============================================
// TANGENT SPACE HELPERS
// ============================================
void ComputeTangentBasis(float3 normal, out float3 tangent, out float3 bitangent)
{
    float3 up = abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    tangent = normalize(cross(up, normal));
    bitangent = cross(normal, tangent);
}

float3 TangentToWorld(float3 vec, float3 normal)
{
    float3 tangent, bitangent;
    ComputeTangentBasis(normal, tangent, bitangent);
    return tangent * vec.x + bitangent * vec.y + normal * vec.z;
}

// ============================================
// COLOR SPACE HELPERS
// ============================================
float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 ReinhardTonemap(float3 color)
{
    return color / (1.0 + color);
}

float3 ReinhardInverse(float3 color)
{
    return color / max(1.0 - color, 0.001);
}

// ACES approximation for better highlight preservation
float3 ACESFilm(float3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// ============================================
// SPHERE/CONE HELPERS
// ============================================

// Solid angle of a sphere cap from half-angle
float SphericalCapSolidAngle(float cosHalfAngle)
{
    return TWO_PI * (1.0 - cosHalfAngle);
}

// Approximate visibility from horizon angles 
float HorizonToVisibility(float h1, float h2, float n)
{
    // h1, h2 are horizon angles, n is the normal's elevation
    // Returns visibility as fraction of hemisphere
    float sinN = sin(n);
    float cosN = cos(n);
    
    // Integrate visibility over the slice
    float vis1 = -cos(2.0 * h1 - n) + cosN + 2.0 * h1 * sinN;
    float vis2 = -cos(2.0 * h2 - n) + cosN + 2.0 * h2 * sinN;
    
    return 0.25 * (vis1 + vis2);
}

#endif // SSRTGI_COMMON_HLSL
