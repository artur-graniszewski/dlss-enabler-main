#include "SsrtgiCommon.hlsl"

static const int NUM_RAYS = 8;
static const int MAX_ITERATIONS = 56;
static const int MAX_MIP = 3;

// HZB texture with mip chain - stores LINEAR depth
// .x = minZ (closest), .y = maxZ (farthest)
Texture2D<float2> gHzb : register(t5);

struct RayResult
{
    bool hit;
    bool valid;
    float3 color;
    float distance;
};

// Test ray against HZB cell (HZB stores LINEAR depth now)
// .x = minZ (closest), .y = maxZ (farthest)
// Returns: -1 = miss (ray behind), 0 = miss (ray in front), 1 = potential intersection
int TestHzbIntersection(float2 uv, float rayZ, int mipLevel, float thickness)
{
    // Clamp mip level
    mipLevel = clamp(mipLevel, 0, MAX_MIP);
    
    // HZB mip 0 is half-res, so shift by mipLevel + 1
    int2 coord = int2(uv * float2(g.Width, g.Height)) >> (mipLevel + 1);
    coord = max(coord, int2(0, 0));
    float2 hzb = gHzb.Load(int3(coord, mipLevel));
    
    // HZB stores linear depth: .x = minZ (closest), .y = maxZ (farthest)
    float closestZ = hzb.x;
    float farthestZ = hzb.y;
    
    // Tolerance based on thickness and mip level (not rayZ!)
    float tolerance = thickness * 2.0 * float(1 << mipLevel);
    
    // Conservative test
    if (rayZ > farthestZ + tolerance) return -1;  // Behind
    if (rayZ < closestZ - tolerance) return 0;    // In front
    
    return 1;  // Potential intersection
}

RayResult MarchRayHZB(float3 origin, float3 direction, float maxDist, float3 originColor, float3 normal, float linearDepth)
{
    RayResult result;
    result.hit = false;
    result.valid = false;
    result.color = float3(0, 0, 0);
    result.distance = maxDist;
    
    // Dynamic thickness for hit detection
    float baseThickness = 0.05 + 0.02 * linearDepth;
    float angleFactor = 1.0 + 2.0 * (1.0 - abs(dot(direction, float3(0, 0, 1))));
    float normalFactor = 1.0 + 1.5 * (1.0 - abs(normal.z));
    float thickness = baseThickness * angleFactor * normalFactor;
    
    // Check if ray start is valid
    if (origin.z <= 0.01) return result;
    
    // March in view space - classic coarse-to-fine HZB traversal
    float t = 0.0;
    float stepSize = maxDist / float(MAX_ITERATIONS);
    int mipLevel = MAX_MIP;
    int onScreenSteps = 0;
    float prevDepthDiff = -1000.0;
    
    for (int iter = 0; iter < MAX_ITERATIONS && t < maxDist; iter++)
    {
        float3 rayPos = origin + direction * t;
        
        if (rayPos.z <= 0.01)
        {
            t += stepSize;
            continue;
        }
        
        float2 uv = ProjectToUV(rayPos);
        
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        {
            t += stepSize * float(1 << mipLevel);
            continue;
        }
        
        onScreenSteps++;
        
        // Pass thickness to HZB test
        int hzbResult = TestHzbIntersection(uv, rayPos.z, mipLevel, thickness);
        
        if (hzbResult != 1)
        {
            // Miss - jump scaled by mip level (classic formula)
            float jump = stepSize * float(1 << mipLevel);
            t += jump;
            
            // Go coarser gradually (not instant MAX_MIP!)
            if (mipLevel < MAX_MIP) mipLevel++;
        }
        else
        {
            // Potential hit - go finer gradually (not instant mip 0!)
            if (mipLevel > 0)
            {
                mipLevel--;
                // Small advance to avoid stuck
                t += stepSize * 0.5;
            }
            else
            {
                // At mip 0 - do actual depth test
                int2 sampleCoord = int2(uv * float2(g.Width, g.Height));
                sampleCoord = clamp(sampleCoord, int2(0, 0), int2(g.Width - 1, g.Height - 1));
                
                float sampledDepthRaw = gDepth.Load(int3(sampleCoord, 0));
                
                if (!IsInvalidOrSkyDepth(sampledDepthRaw))
                {
                    float sampledLinear = LinearizeDepth(sampledDepthRaw);
                    float depthDiff = rayPos.z - sampledLinear;
                    
                    // Hit detection
                    bool crossed = (prevDepthDiff < 0.0 && depthDiff >= 0.0 && depthDiff < thickness * 3.0);
                    bool close = (abs(depthDiff) < thickness);
                    
                    if (crossed || close)
                    {
                        // Binary refinement
                        float refinedT = t;
                        if (crossed && t > stepSize)
                        {
                            float tMin = t - stepSize;
                            float tMax = t;
                            for (int r = 0; r < 3; r++)
                            {
                                float tMid = (tMin + tMax) * 0.5;
                                float3 midPos = origin + direction * tMid;
                                if (midPos.z > 0.01)
                                {
                                    float2 midUV = ProjectToUV(midPos);
                                    if (midUV.x >= 0.0 && midUV.x <= 1.0 && midUV.y >= 0.0 && midUV.y <= 1.0)
                                    {
                                        int2 midCoord = int2(midUV * float2(g.Width, g.Height));
                                        midCoord = clamp(midCoord, int2(0,0), int2(g.Width-1, g.Height-1));
                                        float midDepthRaw = gDepth.Load(int3(midCoord, 0));
                                        if (!IsInvalidOrSkyDepth(midDepthRaw))
                                        {
                                            float midLinear = LinearizeDepth(midDepthRaw);
                                            if (midPos.z - midLinear < 0.0)
                                                tMin = tMid;
                                            else
                                                tMax = tMid;
                                        }
                                    }
                                }
                            }
                            refinedT = (tMin + tMax) * 0.5;
                        }
                        
                        // Sample color at refined position
                        float3 finalPos = origin + direction * refinedT;
                        float2 finalUV = ProjectToUV(finalPos);
                        int2 finalCoord = int2(finalUV * float2(g.Width, g.Height));
                        finalCoord = clamp(finalCoord, int2(0,0), int2(g.Width-1, g.Height-1));
                        
                        float3 hitColor = gColor.Load(int3(finalCoord, 0)).rgb;
                        
                        // Soft clamp
                        float hitLum = dot(hitColor, float3(0.299, 0.587, 0.114));
                        float maxLum = 2.0;
                        if (hitLum > maxLum)
                        {
                            float knee = maxLum * 0.8;
                            float compressed = knee + (hitLum - knee) / (1.0 + (hitLum - knee) / maxLum);
                            hitColor *= compressed / hitLum;
                        }
                        
                        result.hit = true;
                        result.valid = true;
                        result.distance = refinedT;
                        result.color = hitColor;
                        return result;
                    }
                    
                    prevDepthDiff = depthDiff;
                }
                
                // Advance by stepSize, stay at low mip for local search
                t += stepSize;
                // Don't reset to MAX_MIP - stay at mip 1 for nearby geometry
                mipLevel = 1;
            }
        }
    }
    
    // Ray is valid if it stayed on screen for at least 1 step
    result.valid = (onScreenSteps >= 1);
    return result;
}
// Pre-computed cosine-weighted hemisphere directions (8 rays)
static const float2 g_RayDirs[8] = {
    float2(0.0, 1.0),
    float2(0.7071, 0.7071),
    float2(1.0, 0.0),
    float2(0.7071, -0.7071),
    float2(0.0, -1.0),
    float2(-0.7071, -0.7071),
    float2(-1.0, 0.0),
    float2(-0.7071, 0.7071)
};

static const float g_RayElevations[8] = {
    0.25, 0.45, 0.30, 0.50, 0.20, 0.55, 0.35, 0.40
};

float3 GenerateRayFixed(int rayIndex, float3 normal, float rotation)
{
    float2 baseDir = g_RayDirs[rayIndex];
    float cosRot = cos(rotation);
    float sinRot = sin(rotation);
    float2 rotatedDir = float2(
        baseDir.x * cosRot - baseDir.y * sinRot,
        baseDir.x * sinRot + baseDir.y * cosRot
    );
    
    float cosTheta = g_RayElevations[rayIndex];
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    float3 localDir = float3(rotatedDir.x * sinTheta, rotatedDir.y * sinTheta, cosTheta);
    
    float3 up = abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    
    return tangent * localDir.x + bitangent * localDir.y + normal * localDir.z;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 p = dtid.xy;
    if (p.x >= g.Width || p.y >= g.Height)
        return;
    
    float depthRaw = gDepth.Load(int3(p, 0));
    
    if (IsInvalidOrSkyDepth(depthRaw))
    {
        gOut0[p] = float4(0, 0, 0, 1);
        return;
    }
    
    float2 uv = PixelToUv(p);
    float linearDepth = LinearizeDepth(depthRaw);
    float3 viewPos = ReconstructViewPos(uv, depthRaw);
    float3 normal = ReconstructNormal(p, depthRaw);
    float3 originColor = gColor.Load(int3(p, 0)).rgb;
    
    float rayMaxDist = 120 * linearDepth * 0.01;
    rayMaxDist = clamp(rayMaxDist, 0.5, 50.0);
    
    float goldenAngle = 2.39996323;
    float frameRotation = float(g.FrameIndex) * goldenAngle;
    float pixelNoise = Hash12(float2(p) + float(g.FrameIndex) * 0.1) * 6.28318;
    float rotation = frameRotation + pixelNoise;
    
    float3 giAccum = float3(0, 0, 0);
    int hitCount = 0;
    int validRayCount = 0;
    
    for (int ray = 0; ray < NUM_RAYS; ray++)
    {
        float3 rayDir = GenerateRayFixed(ray, normal, rotation);
        
        if (rayDir.z < 0.0)
        {
            rayDir.z = -rayDir.z;
            rayDir = normalize(rayDir);
        }
        
        float3 rayOrigin = viewPos + normal * 0.02;
        RayResult hit = MarchRayHZB(rayOrigin, rayDir, rayMaxDist, originColor, normal, linearDepth);
        
        if (hit.valid)
        {
            validRayCount++;
            
            if (hit.hit)
            {
                hitCount++;
                
                float distNorm = hit.distance / rayMaxDist;
                float falloff = pow(1.0 - distNorm, 3.0);
                
                float3 tinted = hit.color;
                giAccum += tinted * falloff;
            }
        }
    }
    
    float3 gi = float3(0, 0, 0);
    if (hitCount > 0)
    {
        gi = giAccum / float(hitCount);
        // GiStrength applied in Composite, not here
        gi = min(gi, 1.0);
    }
    
    float ao = 1.0;
    if (validRayCount > 0)
    {
        float hitRatio = float(hitCount) / float(validRayCount);
        // Stronger raw AO - Composite will fine-tune with AoStrength
        ao = 1.0 - hitRatio * 1.5;
    }
    ao = clamp(ao, 0.05, 1.0);
    
    float3 viewDir = normalize(-viewPos);
    float NdotV = abs(dot(normal, viewDir));
    float grazingFade = smoothstep(0.05, 0.2, NdotV);
    
    ao = lerp(lerp(1.0, ao, 0.5), ao, grazingFade);
    gi *= lerp(0.3, 1.0, grazingFade);
    
    gOut0[p] = float4(gi, ao);
}
