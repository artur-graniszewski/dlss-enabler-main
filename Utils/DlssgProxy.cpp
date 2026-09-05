#include "DlssgProxy.h"
#include "NvngxCommon.h"          // NVSDK_NGX_SUCCEED, ctx, helpers
#include "../Core/Context.h" 
#include "NgxLogHelpers.h"
#include "ScopedGpuSpoofing.h"
#include "Common.h"
#include "OverdriveController.h"
#include <cmath>
#include <cstdio>
#include <unordered_set>
#include <mutex>

namespace DLSSG
{
    enum class ColorSpaceClass { Unknown, SDR8, HDR10, HDRfp };

    static ColorSpaceClass ClassifyColorSpace(DXGI_FORMAT fmt) {
        switch (fmt) {
            // 8-bit integer, SDR
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return ColorSpaceClass::SDR8;

            // 10-bit integer, HDR10 (PQ)
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
            return ColorSpaceClass::HDR10;

            // float, scRGB linear HDR
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
            return ColorSpaceClass::HDRfp;

        default:
            return ColorSpaceClass::Unknown;
        }
    }

    class FrameGenerationHelper
    {
    public:
        // =============================================================================
        // ShouldGenerateFrame - Determines if frame should be generated in dynamic mode
        // =============================================================================
        //
        // Uses:
        //   - ctx.ngx.dynamicFrameGenerationThreshold (FPS threshold, e.g. 60)
        //   - ctx.ngx.isDynamicFrameGenerationStartingOnThreshold (STARTS or STOPS on threshold)
        //   - ctx.reflex.potentialFps (current potential FPS)
        //
        // Logic:
        //   - STARTS (true):  Generate when FPS <= threshold (help when FPS is low)
        //   - STOPS (false):  Generate when FPS > threshold (stop when FPS is low)
        //
        // Returns:
        //   true  - should generate frame
        //   false - should NOT generate frame
        //
        static bool ShouldGenerateFrame()
        {
            if (ctx.ngx.isDynamicFrameGenerationStartingOnThreshold)
            {
                // Frame generation STARTS when FPS drops to or below threshold
                // (generate frames to boost low FPS)
                return (ctx.reflex.currentFps <= ctx.ngx.dynamicFrameGenerationThreshold);
            }
            else
            {
                // Frame generation STOPS when FPS drops to or below threshold
                // (stop generating when GPU is struggling)
                return (ctx.reflex.currentFps > ctx.ngx.dynamicFrameGenerationThreshold);
            }
        }
    };

    // =============================================================================
// MFG: Compute ClipToPrevClip matrix from camera data
// Enable this feature to reconstruct the matrix when games don't provide it
// =============================================================================
#define MFG_COMPUTE_CLIP_TO_PREV_CLIP 1

#if MFG_COMPUTE_CLIP_TO_PREV_CLIP
    struct MFG_CameraData
    {
        float posX, posY, posZ;
        float fwdX, fwdY, fwdZ;
        float upX, upY, upZ;
        float rightX, rightY, rightZ;
        float fov;
        float nearPlane;
        float farPlane;
        float aspectRatio;
        bool depthInverted;
        bool valid;
    };

    static MFG_CameraData g_mfgPrevCamera = {};
    static bool g_mfgPrevCameraValid = false;
    static float g_mfgComputedClipToPrevClip[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    static void MFG_MatrixMultiply(float result[16], const float a[16], const float b[16])
    {
        for (int row = 0; row < 4; row++)
        {
            for (int col = 0; col < 4; col++)
            {
                result[row * 4 + col] =
                    a[row * 4 + 0] * b[0 * 4 + col] +
                    a[row * 4 + 1] * b[1 * 4 + col] +
                    a[row * 4 + 2] * b[2 * 4 + col] +
                    a[row * 4 + 3] * b[3 * 4 + col];
            }
        }
    }

    static void MFG_BuildViewMatrix(float result[16], const MFG_CameraData& cam)
    {
        result[0] = cam.rightX;  result[1] = cam.upX;  result[2] = -cam.fwdX;  result[3] = 0;
        result[4] = cam.rightY;  result[5] = cam.upY;  result[6] = -cam.fwdY;  result[7] = 0;
        result[8] = cam.rightZ;  result[9] = cam.upZ;  result[10] = -cam.fwdZ; result[11] = 0;
        result[12] = -(cam.rightX * cam.posX + cam.rightY * cam.posY + cam.rightZ * cam.posZ);
        result[13] = -(cam.upX * cam.posX + cam.upY * cam.posY + cam.upZ * cam.posZ);
        result[14] = -(-cam.fwdX * cam.posX + -cam.fwdY * cam.posY + -cam.fwdZ * cam.posZ);
        result[15] = 1;
    }

    static void MFG_BuildProjectionMatrix(float result[16], float fovY, float aspect, float nearZ, float farZ, bool depthInverted)
    {
        float tanHalfFov = tanf(fovY * 0.5f);
        float f = 1.0f / tanHalfFov;

        memset(result, 0, sizeof(float) * 16);

        result[0] = f / aspect;
        result[5] = f;

        if (depthInverted)
        {
            // Reverse-Z projection: near maps to 1.0, far maps to 0.0
            if (farZ > 0 && farZ < 1e10f)
            {
                // Finite far plane reverse-Z
                result[10] = nearZ / (farZ - nearZ);
                result[14] = (nearZ * farZ) / (farZ - nearZ);
            }
            else
            {
                // Infinite far plane reverse-Z
                result[10] = 0.0f;
                result[14] = nearZ;
            }
        }
        else
        {
            // Standard D3D projection: near maps to 0.0, far maps to 1.0
            if (farZ > 0 && farZ < 1e10f)
            {
                result[10] = farZ / (nearZ - farZ);
                result[14] = (nearZ * farZ) / (nearZ - farZ);
            }
            else
            {
                result[10] = -1.0f;
                result[14] = -nearZ;
            }
        }
        result[11] = -1.0f;
    }

    static bool MFG_IsMatrixIdentity(const float m[16])
    {
        // Identity matrix:
        // [0]  [1]  [2]  [3]     1  0  0  0
        // [4]  [5]  [6]  [7]     0  1  0  0
        // [8]  [9]  [10] [11]    0  0  1  0
        // [12] [13] [14] [15]    0  0  0  1

        const float eps = 0.001f;

        // Check diagonal (should be 1)
        if (fabsf(m[0] - 1.0f) > eps) return false;
        if (fabsf(m[5] - 1.0f) > eps) return false;
        if (fabsf(m[10] - 1.0f) > eps) return false;
        if (fabsf(m[15] - 1.0f) > eps) return false;

        // Check off-diagonal (should be 0)
        if (fabsf(m[1]) > eps) return false;
        if (fabsf(m[2]) > eps) return false;
        if (fabsf(m[3]) > eps) return false;
        if (fabsf(m[4]) > eps) return false;
        if (fabsf(m[6]) > eps) return false;
        if (fabsf(m[7]) > eps) return false;
        if (fabsf(m[8]) > eps) return false;
        if (fabsf(m[9]) > eps) return false;
        if (fabsf(m[11]) > eps) return false;
        if (fabsf(m[12]) > eps) return false;
        if (fabsf(m[13]) > eps) return false;
        if (fabsf(m[14]) > eps) return false;

        return true;
    }

    static bool MFG_MatrixInvert(float result[16], const float m[16])
    {
        float inv[16];

        inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
        inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
        inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
        inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

        if (fabsf(det) < 1e-10f)
            return false;

        det = 1.0f / det;

        for (int i = 0; i < 16; i++)
            result[i] = inv[i] * det;

        return true;
    }

    static MFG_CameraData MFG_ReadCameraData(NVSDK_NGX_Parameter* parameters)
    {
        MFG_CameraData cam = {};

        parameters->Get("DLSSG.CameraPosX", &cam.posX);
        parameters->Get("DLSSG.CameraPosY", &cam.posY);
        parameters->Get("DLSSG.CameraPosZ", &cam.posZ);

        parameters->Get("DLSSG.CameraFwdX", &cam.fwdX);
        parameters->Get("DLSSG.CameraFwdY", &cam.fwdY);
        parameters->Get("DLSSG.CameraFwdZ", &cam.fwdZ);

        parameters->Get("DLSSG.CameraUpX", &cam.upX);
        parameters->Get("DLSSG.CameraUpY", &cam.upY);
        parameters->Get("DLSSG.CameraUpZ", &cam.upZ);

        parameters->Get("DLSSG.CameraRightX", &cam.rightX);
        parameters->Get("DLSSG.CameraRightY", &cam.rightY);
        parameters->Get("DLSSG.CameraRightZ", &cam.rightZ);

        parameters->Get("DLSSG.CameraFOV", &cam.fov);
        parameters->Get("DLSSG.CameraNear", &cam.nearPlane);
        parameters->Get("DLSSG.CameraFar", &cam.farPlane);
        parameters->Get("DLSSG.CameraAspectRatio", &cam.aspectRatio);

        // Read depth inversion flag — critical for correct projection matrix
        unsigned int depthInv = 0;
        parameters->Get("DLSSG.DepthInverted", &depthInv);
        cam.depthInverted = (depthInv != 0);

        // If aspect ratio not provided, try to compute from render size
        if (cam.aspectRatio <= 0.0f)
        {
            unsigned int width = 0, height = 0;
            parameters->Get("DLSSG.BackbufferWidth", &width);
            parameters->Get("DLSSG.BackbufferHeight", &height);
            if (width > 0 && height > 0)
                cam.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
            else
                cam.aspectRatio = 16.0f / 9.0f; // fallback
        }

        float fwdLen = sqrtf(cam.fwdX * cam.fwdX + cam.fwdY * cam.fwdY + cam.fwdZ * cam.fwdZ);
        cam.valid = (fwdLen > 0.9f && fwdLen < 1.1f && cam.fov > 0.01f && cam.nearPlane > 0.0f);

        return cam;
    }

    static void MFG_ComputeClipToPrevClip(const MFG_CameraData& current, const MFG_CameraData& prev)
    {
        float currentView[16], prevView[16];
        float currentProj[16], prevProj[16];
        float currentViewProj[16], prevViewProj[16];
        float invCurrentViewProj[16];

        MFG_BuildViewMatrix(currentView, current);
        MFG_BuildViewMatrix(prevView, prev);

        MFG_BuildProjectionMatrix(currentProj, current.fov, current.aspectRatio, current.nearPlane, current.farPlane, current.depthInverted);
        MFG_BuildProjectionMatrix(prevProj, prev.fov, prev.aspectRatio, prev.nearPlane, prev.farPlane, prev.depthInverted);

        // MFG FIX: Post-multiply convention → VP = V × P
        // Shader does prevClip = curClip_row × M, so matrices must be row-major
        // with V applied first: point × V × P = point × VP
        MFG_MatrixMultiply(currentViewProj, currentView, currentProj);
        MFG_MatrixMultiply(prevViewProj, prevView, prevProj);

        // DEBUG: Log full ViewProj matrices
        static bool fullLogged = false;
        if (!fullLogged) {
            fullLogged = true;

            LOG_DEBUG(L"=== Full ViewProj Debug ===");
            LOG_DEBUG(L"CurrentViewProj:");
            LOG_DEBUG(L"  [" + std::to_wstring(currentViewProj[0]) + L", " + std::to_wstring(currentViewProj[1]) + L", " + std::to_wstring(currentViewProj[2]) + L", " + std::to_wstring(currentViewProj[3]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(currentViewProj[4]) + L", " + std::to_wstring(currentViewProj[5]) + L", " + std::to_wstring(currentViewProj[6]) + L", " + std::to_wstring(currentViewProj[7]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(currentViewProj[8]) + L", " + std::to_wstring(currentViewProj[9]) + L", " + std::to_wstring(currentViewProj[10]) + L", " + std::to_wstring(currentViewProj[11]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(currentViewProj[12]) + L", " + std::to_wstring(currentViewProj[13]) + L", " + std::to_wstring(currentViewProj[14]) + L", " + std::to_wstring(currentViewProj[15]) + L"]");

            LOG_DEBUG(L"PrevViewProj:");
            LOG_DEBUG(L"  [" + std::to_wstring(prevViewProj[0]) + L", " + std::to_wstring(prevViewProj[1]) + L", " + std::to_wstring(prevViewProj[2]) + L", " + std::to_wstring(prevViewProj[3]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(prevViewProj[4]) + L", " + std::to_wstring(prevViewProj[5]) + L", " + std::to_wstring(prevViewProj[6]) + L", " + std::to_wstring(prevViewProj[7]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(prevViewProj[8]) + L", " + std::to_wstring(prevViewProj[9]) + L", " + std::to_wstring(prevViewProj[10]) + L", " + std::to_wstring(prevViewProj[11]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(prevViewProj[12]) + L", " + std::to_wstring(prevViewProj[13]) + L", " + std::to_wstring(prevViewProj[14]) + L", " + std::to_wstring(prevViewProj[15]) + L"]");

            // Find which elements differ
            LOG_DEBUG(L"Differences (curr - prev):");
            for (int i = 0; i < 16; i++) {
                float diff = currentViewProj[i] - prevViewProj[i];
                if (fabsf(diff) > 0.0001f) {
                    LOG_DEBUG(L"  [" + std::to_wstring(i) + L"]: " + std::to_wstring(diff));
                }
            }
        }

        if (!MFG_MatrixInvert(invCurrentViewProj, currentViewProj))
        {
            LOG_DEBUG(L"ERROR: Matrix inversion failed!");
            memset(g_mfgComputedClipToPrevClip, 0, sizeof(g_mfgComputedClipToPrevClip));
            g_mfgComputedClipToPrevClip[0] = g_mfgComputedClipToPrevClip[5] = g_mfgComputedClipToPrevClip[10] = g_mfgComputedClipToPrevClip[15] = 1.0f;
            return;
        }

        // DEBUG: Log inverse
        static bool invLogged = false;
        if (!invLogged) {
            invLogged = true;
            LOG_DEBUG(L"InvCurrentViewProj:");
            LOG_DEBUG(L"  [" + std::to_wstring(invCurrentViewProj[0]) + L", " + std::to_wstring(invCurrentViewProj[1]) + L", " + std::to_wstring(invCurrentViewProj[2]) + L", " + std::to_wstring(invCurrentViewProj[3]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(invCurrentViewProj[4]) + L", " + std::to_wstring(invCurrentViewProj[5]) + L", " + std::to_wstring(invCurrentViewProj[6]) + L", " + std::to_wstring(invCurrentViewProj[7]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(invCurrentViewProj[8]) + L", " + std::to_wstring(invCurrentViewProj[9]) + L", " + std::to_wstring(invCurrentViewProj[10]) + L", " + std::to_wstring(invCurrentViewProj[11]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(invCurrentViewProj[12]) + L", " + std::to_wstring(invCurrentViewProj[13]) + L", " + std::to_wstring(invCurrentViewProj[14]) + L", " + std::to_wstring(invCurrentViewProj[15]) + L"]");
        }

        // MFG FIX: clipToPrevClip = inv(VP_curr) × VP_prev
        // For post-multiply: curClip × inv(VP_curr) → world → × VP_prev → prevClip
        MFG_MatrixMultiply(g_mfgComputedClipToPrevClip, invCurrentViewProj, prevViewProj);

        // DEBUG: Final result
        static bool resultLogged = false;
        if (!resultLogged) {
            resultLogged = true;
            LOG_DEBUG(L"Final ClipToPrevClip:");
            LOG_DEBUG(L"  [" + std::to_wstring(g_mfgComputedClipToPrevClip[0]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[1]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[2]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[3]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(g_mfgComputedClipToPrevClip[4]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[5]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[6]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[7]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(g_mfgComputedClipToPrevClip[8]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[9]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[10]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[11]) + L"]");
            LOG_DEBUG(L"  [" + std::to_wstring(g_mfgComputedClipToPrevClip[12]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[13]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[14]) + L", " + std::to_wstring(g_mfgComputedClipToPrevClip[15]) + L"]");
        }
    }

    static void MFG_UpdateClipToPrevClipMatrix(NVSDK_NGX_Parameter* parameters)
    {
        MFG_CameraData currentCamera = MFG_ReadCameraData(parameters);

        if (currentCamera.valid && g_mfgPrevCameraValid)
        {
            float posDiff = fabsf(currentCamera.posX - g_mfgPrevCamera.posX) +
                fabsf(currentCamera.posY - g_mfgPrevCamera.posY) +
                fabsf(currentCamera.posZ - g_mfgPrevCamera.posZ);
            float fwdDiff = fabsf(currentCamera.fwdX - g_mfgPrevCamera.fwdX) +
                fabsf(currentCamera.fwdY - g_mfgPrevCamera.fwdY) +
                fabsf(currentCamera.fwdZ - g_mfgPrevCamera.fwdZ);

            // DEBUG: Log position changes periodically
            static int logCounter = 0;
            if (false && logCounter++ % 120 == 0) {  // Every ~2 seconds at 60fps
                LOG_DEBUG(L"Camera Pos: (" + std::to_wstring(currentCamera.posX) + L", " +
                    std::to_wstring(currentCamera.posY) + L", " + std::to_wstring(currentCamera.posZ) + L")");
                LOG_DEBUG(L"  posDiff=" + std::to_wstring(posDiff) + L", fwdDiff=" + std::to_wstring(fwdDiff));
            }

            if (posDiff > 1e-6f || fwdDiff > 1e-6f)
            {
                MFG_ComputeClipToPrevClip(currentCamera, g_mfgPrevCamera);
            }
        }

        if (currentCamera.valid)
        {
            g_mfgPrevCamera = currentCamera;
            g_mfgPrevCameraValid = true;
        }
    }


#endif // MFG_COMPUTE_CLIP_TO_PREV_CLIP

    static constexpr wchar_t kModule[] = L"DLSSG";

    // ===== Logging helpers =====
    void DlssgProxy::LogInfo(const wchar_t* entry, const std::wstring& message) { logger.Info(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
    void DlssgProxy::LogWarning(const wchar_t* entry, const std::wstring& message) { logger.Warning(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
    void DlssgProxy::LogError(const wchar_t* entry, const std::wstring& message) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": " + message); }
    void DlssgProxy::LogNoBackend(const wchar_t* entry) { logger.Error(L"[" + std::wstring(kModule) + L"] " + std::wstring(entry) + L": backend entrypoint not found"); }

    void DlssgProxy::OnCreate()
    {
        ctx.ngx.isFrameGenerationActive = true;
        //if (ctx.reflex.desiredFpsLimit > 0) {
        //    ctx.reflex.desiredFpsLimit /= 2;
        //}
    }

    void DlssgProxy::OnRelease()
    {
        ctx.ngx.isFrameGenerationActive = false;
        //if (ctx.reflex.desiredFpsLimit > 0) {
        //    ctx.reflex.desiredFpsLimit *= 2;
        //}

        ctx.ngx.isDuplicatingFrames = false;
        ctx.ngx.framesGenerated = 0;
        ctx.ngx.maxFramesGenerated = 1;
        ctx.ngx.isGeneratingFrames = false;

        ctx.ngx.lastEvaluationTimeMsec = 0.0f;
        state.isFgEvaluated.store(false);
#if MFG_COMPUTE_CLIP_TO_PREV_CLIP
        // Reset camera tracking on release
        g_mfgPrevCameraValid = false;
#endif
    }

    // ===== D3D12: CreateFeature (fully implemented) =====
    NVSDK_NGX_Result DlssgProxy::CreateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle** outHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_CreateFeature");

        NGX_LOG_CALL;

        NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

        NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);

        result = proxy(cmdList, featureId, parameters, outHandle);
        auto handle = (outHandle && *outHandle) ? *outHandle : nullptr;

        if (NVSDK_NGX_SUCCEED(result)) {
            OnCreate();
        }

        NGX_LOG_RESULT_WITH_HANDLE_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::EvaluateD3D12(
        ID3D12GraphicsCommandList* cmdList,
        const NVSDK_NGX_Handle* featureHandle,
        NVSDK_NGX_Parameter* parameters,
        PFN_NVSDK_NGX_ProgressCallback callback)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_EvaluateFeature");
        // Latch first-call; we reset this later in Destroy/Release path.
        const bool isFirstCall = !state.isFgEvaluated.exchange(true);
        NGX_RESOLVE_PROXY_ONCE(ID3D12GraphicsCommandList*,
            const NVSDK_NGX_Handle*,
            const NVSDK_NGX_Parameter*,
            PFN_NVSDK_NGX_ProgressCallback);
        if (isFirstCall) {
            NGX_LOG_CALL;
        }

        uint32_t flags = ctx.flags;
        

        // === MFG Texture Inventory Debug ===
        // Log NGX texture inputs/outputs once per unique feature ID. Runs BEFORE
        // any DLSSG.* parameter mutation (HUDLess/DispatchFlags) so the dump
        // reflects exactly what the game supplied.
        {
            static std::unordered_set<unsigned int> g_loggedFeatureIds;
            static std::mutex g_loggedFeatureIdsMutex;

            const unsigned int featureId = featureHandle ? featureHandle->Id : 0u;

            uint32_t isJittered = 0;
            float jitX = 0.0f, jitY = 0.0f;
            parameters->Get("DLSSG.MvecJittered", &isJittered);
            parameters->Get("DLSSG.JitterOffsetX", &jitX);
            parameters->Get("DLSSG.JitterOffsetY", &jitY);

            //if (jitX != 0.0f || jitY != 0.0f && !isJittered) {
            //    static bool isJitterBugReported = false;
            //    if (!isJitterBugReported) {
            //        LOG_WARNING(L"[DLSSG] Jitter reported to be disabled, but Jitter offset provided, overrding the flag!");
            //        isJitterBugReported = true;
            //    }
            //    parameters->Set("DLSSG.MvecJittered", (uint32_t)1);
            //}



            bool shouldLog = false;
            {
                std::lock_guard<std::mutex> lock(g_loggedFeatureIdsMutex);
                shouldLog = g_loggedFeatureIds.insert(featureId).second;
            }

            if (shouldLog) {
                auto fmtFormat = [](DXGI_FORMAT f) -> std::wstring {
                    switch (f) {
                        // Common color formats
                    case DXGI_FORMAT_R8G8B8A8_TYPELESS:        return L"R8G8B8A8_TYPELESS";
                    case DXGI_FORMAT_R8G8B8A8_UNORM:           return L"R8G8B8A8_UNORM";
                    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:      return L"R8G8B8A8_UNORM_SRGB";
                    case DXGI_FORMAT_B8G8R8A8_TYPELESS:        return L"B8G8R8A8_TYPELESS";
                    case DXGI_FORMAT_B8G8R8A8_UNORM:           return L"B8G8R8A8_UNORM";
                    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:      return L"B8G8R8A8_UNORM_SRGB";
                    case DXGI_FORMAT_B8G8R8X8_UNORM:           return L"B8G8R8X8_UNORM";
                        // 10/11-bit HDR
                    case DXGI_FORMAT_R10G10B10A2_TYPELESS:     return L"R10G10B10A2_TYPELESS";
                    case DXGI_FORMAT_R10G10B10A2_UNORM:        return L"R10G10B10A2_UNORM";
                    case DXGI_FORMAT_R10G10B10A2_UINT:         return L"R10G10B10A2_UINT";
                    case DXGI_FORMAT_R11G11B10_FLOAT:          return L"R11G11B10_FLOAT";
                        // 16-bit float (HDR + MV)
                    case DXGI_FORMAT_R16G16B16A16_TYPELESS:    return L"R16G16B16A16_TYPELESS";
                    case DXGI_FORMAT_R16G16B16A16_FLOAT:       return L"R16G16B16A16_FLOAT";
                    case DXGI_FORMAT_R16G16B16A16_UNORM:       return L"R16G16B16A16_UNORM";
                    case DXGI_FORMAT_R16G16_TYPELESS:          return L"R16G16_TYPELESS";
                    case DXGI_FORMAT_R16G16_FLOAT:             return L"R16G16_FLOAT";
                    case DXGI_FORMAT_R16G16_UNORM:             return L"R16G16_UNORM";
                    case DXGI_FORMAT_R16_TYPELESS:             return L"R16_TYPELESS";
                    case DXGI_FORMAT_R16_FLOAT:                return L"R16_FLOAT";
                    case DXGI_FORMAT_R16_UNORM:                return L"R16_UNORM";
                        // 32-bit float
                    case DXGI_FORMAT_R32G32B32A32_TYPELESS:    return L"R32G32B32A32_TYPELESS";
                    case DXGI_FORMAT_R32G32B32A32_FLOAT:       return L"R32G32B32A32_FLOAT";
                    case DXGI_FORMAT_R32G32_TYPELESS:          return L"R32G32_TYPELESS";
                    case DXGI_FORMAT_R32G32_FLOAT:             return L"R32G32_FLOAT";
                    case DXGI_FORMAT_R32_TYPELESS:             return L"R32_TYPELESS";
                    case DXGI_FORMAT_R32_FLOAT:                return L"R32_FLOAT";
                    case DXGI_FORMAT_R32_UINT:                 return L"R32_UINT";
                        // Depth + Stencil families
                    case DXGI_FORMAT_D32_FLOAT:                return L"D32_FLOAT";
                    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:     return L"D32_FLOAT_S8X24_UINT";
                    case DXGI_FORMAT_R32G8X24_TYPELESS:        return L"R32G8X24_TYPELESS";
                    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: return L"R32_FLOAT_X8X24_TYPELESS";
                    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:  return L"X32_TYPELESS_G8X24_UINT";
                    case DXGI_FORMAT_R24G8_TYPELESS:           return L"R24G8_TYPELESS";
                    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:    return L"R24_UNORM_X8_TYPELESS";
                    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:     return L"X24_TYPELESS_G8_UINT";
                    case DXGI_FORMAT_D24_UNORM_S8_UINT:        return L"D24_UNORM_S8_UINT";
                    case DXGI_FORMAT_D16_UNORM:                return L"D16_UNORM";
                        // Small / 8-bit
                    case DXGI_FORMAT_R8G8_TYPELESS:            return L"R8G8_TYPELESS";
                    case DXGI_FORMAT_R8G8_UNORM:               return L"R8G8_UNORM";
                    case DXGI_FORMAT_R8_TYPELESS:              return L"R8_TYPELESS";
                    case DXGI_FORMAT_R8_UNORM:                 return L"R8_UNORM";
                    case DXGI_FORMAT_A8_UNORM:                 return L"A8_UNORM";
                    case DXGI_FORMAT_UNKNOWN:                  return L"UNKNOWN";
                    default:
                        return L"<DXGI_FORMAT_" + std::to_wstring(static_cast<unsigned int>(f)) + L">";
                    }
                    };

                auto fmtFlags = [](D3D12_RESOURCE_FLAGS f) -> std::wstring {
                    std::wstring s;
                    if (f & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)        s += L"RT|";
                    if (f & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)        s += L"DS|";
                    if (f & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)    s += L"UAV|";
                    if (f & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)      s += L"DENY_SRV|";
                    if (f & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)        s += L"XA|";
                    if (f & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) s += L"SIM|";
                    if (s.empty()) s = L"NONE";
                    else s.pop_back(); // trailing |
                    return s;
                    };

                auto fmtDimension = [](D3D12_RESOURCE_DIMENSION d) -> const wchar_t* {
                    switch (d) {
                    case D3D12_RESOURCE_DIMENSION_UNKNOWN:   return L"UNKNOWN";
                    case D3D12_RESOURCE_DIMENSION_BUFFER:    return L"BUFFER";
                    case D3D12_RESOURCE_DIMENSION_TEXTURE1D: return L"TEX1D";
                    case D3D12_RESOURCE_DIMENSION_TEXTURE2D: return L"TEX2D";
                    case D3D12_RESOURCE_DIMENSION_TEXTURE3D: return L"TEX3D";
                    default:                                 return L"<other>";
                    }
                    };

                auto fmtLayout = [](D3D12_TEXTURE_LAYOUT l) -> const wchar_t* {
                    switch (l) {
                    case D3D12_TEXTURE_LAYOUT_UNKNOWN:                 return L"UNKNOWN";
                    case D3D12_TEXTURE_LAYOUT_ROW_MAJOR:               return L"ROW_MAJOR";
                    case D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE:  return L"64KB_UNDEF_SWIZZLE";
                    case D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE:   return L"64KB_STD_SWIZZLE";
                    default:                                           return L"<other>";
                    }
                    };

                auto fmtHeapType = [](D3D12_HEAP_TYPE t) -> const wchar_t* {
                    switch (t) {
                    case D3D12_HEAP_TYPE_DEFAULT:  return L"DEFAULT";
                    case D3D12_HEAP_TYPE_UPLOAD:   return L"UPLOAD";
                    case D3D12_HEAP_TYPE_READBACK: return L"READBACK";
                    case D3D12_HEAP_TYPE_CUSTOM:   return L"CUSTOM";
                    default:                       return L"<other>";
                    }
                    };

                auto fmtCpuPage = [](D3D12_CPU_PAGE_PROPERTY p) -> const wchar_t* {
                    switch (p) {
                    case D3D12_CPU_PAGE_PROPERTY_UNKNOWN:       return L"UNK";
                    case D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE: return L"NA";
                    case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE: return L"WC";
                    case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK:    return L"WB";
                    default:                                    return L"<o>";
                    }
                    };

                auto fmtMemPool = [](D3D12_MEMORY_POOL p) -> const wchar_t* {
                    switch (p) {
                    case D3D12_MEMORY_POOL_UNKNOWN: return L"UNK";
                    case D3D12_MEMORY_POOL_L0:      return L"L0";  // System / shared (iGPU + dGPU host)
                    case D3D12_MEMORY_POOL_L1:      return L"L1";  // Dedicated VRAM (dGPU only)
                    default:                        return L"<o>";
                    }
                    };

                auto logTexture = [&](const char* key, const wchar_t* role, const char* subrectPrefix = nullptr) {
                    ID3D12Resource* res = nullptr;
                    auto r = parameters->Get(key, &res);
                    std::wstring keyW(key, key + strlen(key));

                    if (r != NVSDK_NGX_Result_Success || res == nullptr) {
                        LOG_DEBUG(L"  [" + std::wstring(role) + L"] " + keyW + L" = <null>");
                        return;
                    }

                    D3D12_RESOURCE_DESC desc = res->GetDesc();

                    // Line 1: identity + dimensions
                    LOG_DEBUG(
                        L"  [" + std::wstring(role) + L"] " + keyW +
                        L" ptr=0x" + std::to_wstring(reinterpret_cast<uintptr_t>(res)) +
                        L" " + std::wstring(fmtDimension(desc.Dimension)) +
                        L" " + std::to_wstring(desc.Width) + L"x" + std::to_wstring(desc.Height) +
                        L" arr=" + std::to_wstring(desc.DepthOrArraySize) +
                        L" mips=" + std::to_wstring(desc.MipLevels) +
                        L" samples=" + std::to_wstring(desc.SampleDesc.Count) +
                        L"/" + std::to_wstring(desc.SampleDesc.Quality));

                    // Line 2: format + flags + layout + alignment
                    LOG_DEBUG(
                        L"        fmt=" + fmtFormat(desc.Format) +
                        L" flags=" + fmtFlags(desc.Flags) +
                        L" layout=" + std::wstring(fmtLayout(desc.Layout)) +
                        L" align=" + std::to_wstring(desc.Alignment));

                    // Line 3: heap properties (may fail for reserved/placed resources)
                    D3D12_HEAP_PROPERTIES heapProps = {};
                    D3D12_HEAP_FLAGS      heapFlags = D3D12_HEAP_FLAG_NONE;
                    HRESULT hr = res->GetHeapProperties(&heapProps, &heapFlags);
                    if (SUCCEEDED(hr)) {
                        std::wstring hf;
                        if (heapFlags & D3D12_HEAP_FLAG_SHARED)                       hf += L"SHARED|";
                        if (heapFlags & D3D12_HEAP_FLAG_DENY_BUFFERS)                 hf += L"DENY_BUF|";
                        if (heapFlags & D3D12_HEAP_FLAG_ALLOW_DISPLAY)                hf += L"DISPLAY|";
                        if (heapFlags & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER)         hf += L"XA|";
                        if (heapFlags & D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES)          hf += L"DENY_RT_DS|";
                        if (heapFlags & D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)      hf += L"DENY_NON_RT_DS|";
                        if (heapFlags & D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES) hf += L"ALLOW_ALL|";
                        if (hf.empty()) hf = L"NONE";
                        else hf.pop_back();

                        LOG_DEBUG(
                            L"        heap=" + std::wstring(fmtHeapType(heapProps.Type)) +
                            L" cpuPage=" + std::wstring(fmtCpuPage(heapProps.CPUPageProperty)) +
                            L" pool=" + std::wstring(fmtMemPool(heapProps.MemoryPoolPreference)) +
                            L" node=" + std::to_wstring(heapProps.CreationNodeMask) + L"/" + std::to_wstring(heapProps.VisibleNodeMask) +
                            L" hflags=" + hf);
                    }
                    else {
                        // Most likely a reserved (tiled) or placed resource, or one with no committed heap
                        LOG_DEBUG(
                            L"        heap=<unavailable hr=0x" + std::to_wstring(static_cast<uint32_t>(hr)) + L"> (placed/reserved?)");
                    }

                    // Line 4 (optional): per-resource subrect from DLSSG.<prefix>Subrect{BaseX,BaseY,Width,Height}.
                    // Only emitted when a subrectPrefix is supplied — i.e. for resources that may carry
                    // subrect/letterbox metadata (Backbuffer, HUDLess, UI, Depth, MVecs, BidirectionalDistortionField,
                    // NoWarp). Outputs (OutputInterpolated/OutputReal) skip this line — they have no subrect params
                    // in the DLSSG handshake.
                    //
                    // When all four fields are zero, the param block was not provided by the game — log "(none)"
                    // so the absence is explicit and distinguishable from "subrect zero-sized".
                    if (subrectPrefix != nullptr) {
                        char keyBaseX[128], keyBaseY[128], keyW[128], keyH[128];
                        snprintf(keyBaseX, sizeof(keyBaseX), "DLSSG.%sSubrectBaseX", subrectPrefix);
                        snprintf(keyBaseY, sizeof(keyBaseY), "DLSSG.%sSubrectBaseY", subrectPrefix);
                        snprintf(keyW, sizeof(keyW), "DLSSG.%sSubrectWidth", subrectPrefix);
                        snprintf(keyH, sizeof(keyH), "DLSSG.%sSubrectHeight", subrectPrefix);

                        uint32_t srBaseX = 0, srBaseY = 0, srW = 0, srH = 0;
                        parameters->Get(keyBaseX, &srBaseX);
                        parameters->Get(keyBaseY, &srBaseY);
                        parameters->Get(keyW, &srW);
                        parameters->Get(keyH, &srH);

                        if (srBaseX == 0 && srBaseY == 0 && srW == 0 && srH == 0) {
                            LOG_DEBUG(L"        subrect=<none>");
                        }
                        else {
                            LOG_DEBUG(
                                L"        subrect=base(" + std::to_wstring(srBaseX) + L"," + std::to_wstring(srBaseY) +
                                L") size(" + std::to_wstring(srW) + L"x" + std::to_wstring(srH) + L")");
                        }
                    }
                    };

                LOG_DEBUG(L"=== MFG Texture Inventory (FeatureID=" + std::to_wstring(featureId) + L") ===");

                // Inputs (read by FSR3 OF / FI). Third arg is the DLSSG.<prefix>Subrect* base-name
                // for the per-resource subrect line. Resources without subrect support omit the arg.
                logTexture("DLSSG.Backbuffer", L"IN ", "Backbuffer");
                logTexture("DLSSG.HUDLess", L"IN ", "HUDLess");
                logTexture("DLSSG.Depth", L"IN ", "Depth");
                logTexture("DLSSG.MVecs", L"IN ", "MVecs");
                logTexture("DLSSG.UI", L"IN ", "UI");
                logTexture("DLSSG.BidirectionalDistortionField", L"IN ", "BidirectionalDistortionField");

                // Outputs (written by FSR3 FI) — no subrect support in DLSSG handshake spec.
                logTexture("DLSSG.OutputInterpolated", L"OUT");
                logTexture("DLSSG.OutputReal", L"OUT");

                // Subrects + auxiliary scalar/flag inputs (give context for the textures above).
                // Per-resource subrects are also logged inline by logTexture; the [SUB] summary
                // duplicates W/H here in compact form for at-a-glance reading and adds the BaseX/Y
                // offsets which can reveal letterbox positioning (e.g. Pragmata: Backbuffer base(26,0)).
                uint32_t bbSubBX = 0, bbSubBY = 0, bbSubW = 0, bbSubH = 0;
                uint32_t hudSubBX = 0, hudSubBY = 0, hudlessSubW = 0, hudlessSubH = 0;
                uint32_t uiSubBX = 0, uiSubBY = 0, uiSubW = 0, uiSubH = 0;
                uint32_t depthSubBX = 0, depthSubBY = 0, depthSubW = 0, depthSubH = 0;
                uint32_t mvecSubBX = 0, mvecSubBY = 0, mvecSubW = 0, mvecSubH = 0;
                uint32_t hdr = 0, depthInv = 0, mvecJit = 0, mvecDil = 0, ortho = 0;
                uint32_t lowPrecDF = 0, mfCount = 0, mfIndex = 0, isRecording = 0;
                float mvScaleX = 0.0f, mvScaleY = 0.0f;
                parameters->Get("DLSSG.BackbufferSubrectBaseX", &bbSubBX);
                parameters->Get("DLSSG.BackbufferSubrectBaseY", &bbSubBY);
                parameters->Get("DLSSG.BackbufferSubrectWidth", &bbSubW);
                parameters->Get("DLSSG.BackbufferSubrectHeight", &bbSubH);
                parameters->Get("DLSSG.HUDLessSubrectBaseX", &hudSubBX);
                parameters->Get("DLSSG.HUDLessSubrectBaseY", &hudSubBY);
                parameters->Get("DLSSG.HUDLessSubrectWidth", &hudlessSubW);
                parameters->Get("DLSSG.HUDLessSubrectHeight", &hudlessSubH);
                parameters->Get("DLSSG.UISubrectBaseX", &uiSubBX);
                parameters->Get("DLSSG.UISubrectBaseY", &uiSubBY);
                parameters->Get("DLSSG.UISubrectWidth", &uiSubW);
                parameters->Get("DLSSG.UISubrectHeight", &uiSubH);
                parameters->Get("DLSSG.DepthSubrectBaseX", &depthSubBX);
                parameters->Get("DLSSG.DepthSubrectBaseY", &depthSubBY);
                parameters->Get("DLSSG.DepthSubrectWidth", &depthSubW);
                parameters->Get("DLSSG.DepthSubrectHeight", &depthSubH);
                parameters->Get("DLSSG.MVecsSubrectBaseX", &mvecSubBX);
                parameters->Get("DLSSG.MVecsSubrectBaseY", &mvecSubBY);
                parameters->Get("DLSSG.MVecsSubrectWidth", &mvecSubW);
                parameters->Get("DLSSG.MVecsSubrectHeight", &mvecSubH);
                parameters->Get("DLSSG.ColorBuffersHDR", &hdr);
                parameters->Get("DLSSG.DepthInverted", &depthInv);
                parameters->Get("DLSSG.MvecJittered", &mvecJit);
                parameters->Get("DLSSG.MvecDilated", &mvecDil);
                parameters->Get("DLSSG.OrthoProjection", &ortho);
                parameters->Get("DLSSG.BidirectionalDistortionField.IsLowPrecision", &lowPrecDF);
                parameters->Get("DLSSG.MultiFrameCount", &mfCount);
                parameters->Get("DLSSG.MultiFrameIndex", &mfIndex);
                parameters->Get("DLSSG.IsRecording", &isRecording);
                parameters->Get("DLSSG.MvecScaleX", &mvScaleX);
                parameters->Get("DLSSG.MvecScaleY", &mvScaleY);

                // Helper: compact "base(x,y)+size(WxH)" or "<none>" if all four are zero.
                auto fmtSubrect = [](uint32_t bx, uint32_t by, uint32_t w, uint32_t h) -> std::wstring {
                    if (bx == 0 && by == 0 && w == 0 && h == 0) return L"<none>";
                    return L"base(" + std::to_wstring(bx) + L"," + std::to_wstring(by) +
                        L")+size(" + std::to_wstring(w) + L"x" + std::to_wstring(h) + L")";
                    };
                LOG_DEBUG(L"  [SUB] BB=" + fmtSubrect(bbSubBX, bbSubBY, bbSubW, bbSubH) +
                    L" HUD=" + fmtSubrect(hudSubBX, hudSubBY, hudlessSubW, hudlessSubH) +
                    L" UI=" + fmtSubrect(uiSubBX, uiSubBY, uiSubW, uiSubH));
                LOG_DEBUG(L"  [SUB] Depth=" + fmtSubrect(depthSubBX, depthSubBY, depthSubW, depthSubH) +
                    L" MVecs=" + fmtSubrect(mvecSubBX, mvecSubBY, mvecSubW, mvecSubH));
                LOG_DEBUG(L"  [FLG] HDR=" + std::to_wstring(hdr) +
                    L" DepthInv=" + std::to_wstring(depthInv) +
                    L" MvecJit=" + std::to_wstring(mvecJit) +
                    L" MvecDil=" + std::to_wstring(mvecDil) +
                    L" Ortho=" + std::to_wstring(ortho) +
                    L" DF.LowPrec=" + std::to_wstring(lowPrecDF) +
                    L" Recording=" + std::to_wstring(isRecording));
                LOG_DEBUG(L"  [MFG] Count=" + std::to_wstring(mfCount) +
                    L" Index=" + std::to_wstring(mfIndex) +
                    L" MvScale=(" + std::to_wstring(mvScaleX) + L"," + std::to_wstring(mvScaleY) + L")" +
                    L" IsJittered=(" + std::to_wstring(isJittered) + L")" +
                    L" Jitter=(" + std::to_wstring(jitX) + L"," + std::to_wstring(jitY) + L")");
                LOG_DEBUG(L"=== End Texture Inventory (FeatureID=" + std::to_wstring(featureId) + L") ===");
            }
        }

        if (ctx.ngx.isDynamicFrameGenerationEnabled) {
            ctx.ngx.isGeneratingFrames = FrameGenerationHelper::ShouldGenerateFrame();
        }
        else {
            ctx.ngx.isGeneratingFrames = true;
        }

        // check for resource color-space mismatch
        if (!ctx.ngx.isUiTextureEnabled) {
            ID3D12Resource* backBuffer = nullptr;
            ID3D12Resource* hudLess = nullptr;
            auto r1 = parameters->Get("DLSSG.Backbuffer", &backBuffer);
            auto r2 = parameters->Get("DLSSG.HUDLess", &hudLess);
            if (r1 == NVSDK_NGX_Result_Success && r2 == NVSDK_NGX_Result_Success
                && backBuffer != nullptr && hudLess != nullptr) {
                const D3D12_RESOURCE_DESC descBB = backBuffer->GetDesc();
                const D3D12_RESOURCE_DESC descHL = hudLess->GetDesc();

                const ColorSpaceClass csBB = ClassifyColorSpace(descBB.Format);
                const ColorSpaceClass csHL = ClassifyColorSpace(descHL.Format);

                // Mismatch only when both formats are known AND map to different
                // color spaces (e.g. HDR10 backbuffer vs SDR8 HUDless).
                // A TYPELESS-vs-UNORM difference inside one family is NOT a mismatch.
                const bool colorSpaceMismatch =
                    csBB != ColorSpaceClass::Unknown &&
                    csHL != ColorSpaceClass::Unknown &&
                    csBB != csHL;

                static bool textureMismatchReported = false;
                if (colorSpaceMismatch) {
                    if (ctx.ngx.hudDetectionMode == 0) {
                        flags |= 0x04000000; // temporal HUD pinning
                        if (!textureMismatchReported) {
                            textureMismatchReported = true;
                            LOG_WARNING(L"[DLSSG] HUDless/Backbuffer color-space mismatch, enabling HUD detection feature");
                        }
                    }
                    parameters->Set("DLSSG.HUDLess", reinterpret_cast<void**>(nullptr));
                }
            }
        }


        {
            // Workaround mostly for final fantasy xvi
            uint32_t depthInverted = 0;
            float cameraNear = 0;
            float cameraFar = 0;
            parameters->Get("DLSSG.DepthInverted", &depthInverted);
            parameters->Get("DLSSG.CameraNear", &cameraNear);
            parameters->Get("DLSSG.CameraFar", &cameraFar);

            if (cameraNear == 0)
            {
                if (depthInverted)
                    cameraNear = 100000.0f;
                else
                    cameraNear = 0.1f;

                parameters->Set("DLSSG.CameraNear", cameraNear);
            }

            if (cameraFar == 0)
            {
                if (depthInverted)
                    cameraFar = 0.1f;
                else
                    cameraFar = 100000.0f;

                parameters->Set("DLSSG.CameraFar", cameraFar);
            }
            else if (std::isinf(cameraFar))
            {
                cameraFar = 100000.0f;
                parameters->Set("DLSSG.CameraFar", cameraFar);
            }

            // if (uint32_t LowresMvec = 0; InParameters->Get("DLSSG.run_lowres_mvec_pass", &LowresMvec) ==
            // NVSDK_NGX_Result_Success && LowresMvec == 1) {
            //parameters->Set("DLSSG.MVecsSubrectWidth", 0U);
            //parameters->Set("DLSSG.MVecsSubrectHeight", 0U);
            //}
        }

        int frameIndex = 1;
        int frameMax = 1;
        parameters->Get("DLSSG.MultiFrameIndex", &frameIndex);
        parameters->Get("DLSSG.MultiFrameCount", &frameMax);

        static bool cameraDataLogged = false;
        if (!cameraDataLogged) {
            cameraDataLogged = true;

            float posX = 0, posY = 0, posZ = 0;
            float fwdX = 0, fwdY = 0, fwdZ = 0;
            float upX = 0, upY = 0, upZ = 0;
            float rightX = 0, rightY = 0, rightZ = 0;
            float fov = 0, nearPlane = 0, farPlane = 0, aspectRatio = 0;

            parameters->Get("DLSSG.CameraPosX", &posX);
            parameters->Get("DLSSG.CameraPosY", &posY);
            parameters->Get("DLSSG.CameraPosZ", &posZ);
            parameters->Get("DLSSG.CameraFwdX", &fwdX);
            parameters->Get("DLSSG.CameraFwdY", &fwdY);
            parameters->Get("DLSSG.CameraFwdZ", &fwdZ);
            parameters->Get("DLSSG.CameraUpX", &upX);
            parameters->Get("DLSSG.CameraUpY", &upY);
            parameters->Get("DLSSG.CameraUpZ", &upZ);
            parameters->Get("DLSSG.CameraRightX", &rightX);
            parameters->Get("DLSSG.CameraRightY", &rightY);
            parameters->Get("DLSSG.CameraRightZ", &rightZ);
            parameters->Get("DLSSG.CameraFOV", &fov);
            parameters->Get("DLSSG.CameraNear", &nearPlane);
            parameters->Get("DLSSG.CameraFar", &farPlane);
            parameters->Get("DLSSG.CameraAspectRatio", &aspectRatio);

            float fwdLen = sqrtf(fwdX * fwdX + fwdY * fwdY + fwdZ * fwdZ);

            LogInfo(kEntry, L"=== MFG Camera Data Debug ===");
            LogInfo(kEntry, L"  Pos: " + std::to_wstring(posX) + L", " + std::to_wstring(posY) + L", " + std::to_wstring(posZ));
            LogInfo(kEntry, L"  Fwd: " + std::to_wstring(fwdX) + L", " + std::to_wstring(fwdY) + L", " + std::to_wstring(fwdZ) + L" (len=" + std::to_wstring(fwdLen) + L")");
            LogInfo(kEntry, L"  Up:  " + std::to_wstring(upX) + L", " + std::to_wstring(upY) + L", " + std::to_wstring(upZ));
            LogInfo(kEntry, L"  Right: " + std::to_wstring(rightX) + L", " + std::to_wstring(rightY) + L", " + std::to_wstring(rightZ));
            LogInfo(kEntry, L"  FOV: " + std::to_wstring(fov) + L", Near: " + std::to_wstring(nearPlane) + L", Far: " + std::to_wstring(farPlane));
            LogInfo(kEntry, L"  AspectRatio: " + std::to_wstring(aspectRatio));

            bool valid = (fwdLen > 0.9f && fwdLen < 1.1f && fov > 0.01f && nearPlane > 0.0f);
            LogInfo(kEntry, L"  Valid: " + std::wstring(valid ? L"YES" : L"NO"));
        }


        static int posLogCounter = 0;
        posLogCounter++;
        if (posLogCounter <= 300 && posLogCounter % 30 == 0) {  // Co 30 klatek = ~co 0.5 sek
            float posX = 0, posY = 0, posZ = 0;
            parameters->Get("DLSSG.CameraPosX", &posX);
            parameters->Get("DLSSG.CameraPosY", &posY);
            parameters->Get("DLSSG.CameraPosZ", &posZ);

        }

#if MFG_COMPUTE_CLIP_TO_PREV_CLIP
        // MFG: Compute ClipToPrevClip matrix from camera data
        if (frameIndex == 1)
        {
            MFG_UpdateClipToPrevClipMatrix(parameters);
        }

        // Check if game provides valid ClipToPrevClip
        float (*gameClipToPrevClip)[4] = nullptr;
        parameters->Get("DLSSG.ClipToPrevClip", reinterpret_cast<void**>(&gameClipToPrevClip));

        bool useComputedMatrix = false;
        if (gameClipToPrevClip != nullptr)
        {
            bool gameMatrixIsIdentity = MFG_IsMatrixIdentity(reinterpret_cast<float*>(gameClipToPrevClip));
            if (gameMatrixIsIdentity && g_mfgPrevCameraValid)
            {
                useComputedMatrix = true;
            }
        }
        else if (g_mfgPrevCameraValid)
        {
            useComputedMatrix = true;
        }

        static bool matrixDebugLogged = false;
        if (!matrixDebugLogged) {
            matrixDebugLogged = true;

            float (*gameClipToPrevClip)[4] = nullptr;
            parameters->Get("DLSSG.ClipToPrevClip", reinterpret_cast<void**>(&gameClipToPrevClip));


            if (gameClipToPrevClip) {
                bool isIdentity = MFG_IsMatrixIdentity(reinterpret_cast<float*>(gameClipToPrevClip));
                //LogInfo(kEntry, L"  gameClipToPrevClip isIdentity: " + std::wstring(isIdentity ? L"YES" : L"NO"));
            }

            bool computedIsIdentity = MFG_IsMatrixIdentity(g_mfgComputedClipToPrevClip);
        }

        if (useComputedMatrix)
        {
            parameters->Set("DLSSG.ClipToPrevClip", const_cast<float*>(g_mfgComputedClipToPrevClip));

            static bool computedMatrixLogged = false;
            if (!computedMatrixLogged && !MFG_IsMatrixIdentity(g_mfgComputedClipToPrevClip))
            {
                computedMatrixLogged = true;
                LogInfo(kEntry, L"MFG: Using computed ClipToPrevClip matrix");
            }
        }
#endif // MFG_COMPUTE_CLIP_TO_PREV_CLIP

        // Dispatch PRE-EVALUATE event (for effects like SSRTGI)
        if (ctx.ngx.isHudlessMaskEnabled) {
            flags |= 0x02000000;
        }

        //flags |= 0x40000000; // disable OF!
        if (ctx.overdriveMode == 1 || ctx.ngx.isPerformanceModeEnabled) {
            // performance mode
            flags |= 0x40000000;
        }

        if (!ctx.ngx.isFullScreenMenuDetectionEnabled) {
            flags != 0x20000000;
        }

        if (ctx.ngx.isHudInterpolationEnabled || ctx.ngx.isGhostBustingEnabled) {
            //flags |= 0x08000000;  
        }

        if (ctx.ngx.isGhostBustingEnabled) {
            flags |= 0x00100000;
        }

        if (ctx.ngx.hudDetectionMode > 0) {
            flags |= 0x10000000; //!ctx.ngx.isUiTextureEnabled
            parameters->Set("DLSSG.HUDLess", reinterpret_cast<void**>(nullptr));
        }

        if (ctx.ngx.hudDetectionMode == 2) {
            flags |= 0x04000000;
        }

        if (!ctx.ngx.isUiTextureEnabled) {
            flags |= 0x10000000;
        } 

        if (ctx.ngx.isUiPinningEnabled || ctx.streamline.forceLoadDLSSG) {
            if (ctx.ngx.hudDetectionMode == 0) {
                flags |= 0x04000000;
            }
            parameters->Set("DLSSG.HUDLess", reinterpret_cast<void**>(nullptr));
        }

        parameters->Set("DLSSG.DispatchFlags", flags);

        static bool isHudTested = false;
        if (!isHudTested) {
            isHudTested = true;
            ID3D12Resource* hud = nullptr;
            auto result = parameters->Get("DLSSG.UI", &hud);
            if (result == NVSDK_NGX_Result_Success && hud != nullptr) {
                LogInfo(kEntry, L"DLSSG provides HUD resource");
            }
            else {
                LogWarning(kEntry, L"DLSSG is missing HUD resource");
            }
        }
        //LogWarning(kEntry, L"DLSSG frame #" + std::to_wstring(frameIndex) + L" out of " + std::to_wstring(frameMax));
        if (!ctx.ngx.isGeneratingFrames) {
            parameters->Set("DLSSG.Reset", 1);
            ctx.ngx.isDuplicatingFrames = true;
            ctx.ngx.framesGenerated = 0;
            ctx.ngx.maxFramesGenerated = max(frameMax, 1);
        }
        else {
            ctx.ngx.framesGenerated = max(frameMax, 1);
            ctx.ngx.isDuplicatingFrames = false;
            ctx.ngx.maxFramesGenerated = max(frameMax, 1);
        }
        ctx.ngx.isFrameGenerationActive = true;
        static bool isRetryTried = false;
        result = proxy(cmdList, featureHandle, parameters, callback);
        ctx.ngx.lastEvaluationTimeMsec = Common::GetCurrentTimeMsec();
        if (isFirstCall || !NVSDK_NGX_SUCCEED(result)) {
            // Only log failures after the first call
            if (!NVSDK_NGX_SUCCEED(result)) {
                LogWarning(kEntry, L"DLSSG failed for " + std::to_wstring(featureHandle->Id));
                // try once more
                if (!isRetryTried && !isFirstCall) {
                    isRetryTried = true;
                    result = proxy(cmdList, featureHandle, parameters, callback);
                }
                if (NVSDK_NGX_SUCCEED(result)) {
                    isRetryTried = false;
                    LogWarning(kEntry, L"DLSSG retry succeeded for " + std::to_wstring(featureHandle->Id));
                }
            }
            NGX_LOG_RESULT_AND_RETURN;
        }
        return result;
    }

    NVSDK_NGX_Result DlssgProxy::GetFeatureRequirementsD3D12(
        IDXGIAdapter* adapter,
        NVSDK_NGX_FeatureDiscoveryInfo* discoveryInfo,
        NVSDK_NGX_FeatureRequirement* requirementInfo)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_GetFeatureRequirements");

        NGX_LOG_CALL;
        LogInfo(kEntry, L"Checking feature requirements for feature ID: " + std::to_wstring(discoveryInfo->FeatureID));

        NGX_RESOLVE_PROXY_ONCE(IDXGIAdapter*, NVSDK_NGX_FeatureDiscoveryInfo*, NVSDK_NGX_FeatureRequirement*);

        result = proxy(adapter, discoveryInfo, requirementInfo);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::PopulateParametersD3D12(NVSDK_NGX_Parameter* InParams)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_PopulateParameters_Impl");

        NGX_LOG_CALL;
        NVAPI_DISABLE_GPU_SPOOFING();

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);

        result = proxy(InParams);
        InParams->Set("FrameGeneration.Available", 1);
        InParams->Set("FrameGeneration.FeatureInitResult", 1);
        InParams->Set("FrameGeneration.MinDriverVersionMajor", 10);
        InParams->Set("FrameGeneration.MinDriverVersionMinor", 10);
        InParams->Set("FrameGeneration.NeedsUpdatedDriver", 0);
        InParams->Set("FrameInterpolation.Available", 1);
        InParams->Set("FrameInterpolation.FeatureInitResult", 1);
        InParams->Set("FrameInterpolation.MinDriverVersionMajor", 10);
        InParams->Set("FrameInterpolation.MinDriverVersionMinor", 10);
        InParams->Set("FrameInterpolation.NeedsUpdatedDriver", 0);
        InParams->Set("FrameInterpolation.GhostbusterVersionMajor", 3);
        InParams->Set("FrameInterpolation.GhostbusterVersionMinor", 5);
        InParams->Set("DLSSG.MultiFrameCountMax", 5);
        //InParams->Set("DLSSG.NumFramesToGenerateMax", 5);
        //InParams->Set("DLSSEnabler.Available", 1); //

        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ReleaseD3D12(NVSDK_NGX_Handle* instanceHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_ReleaseFeature");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);

        result = proxy(instanceHandle);
        if (NVSDK_NGX_SUCCEED(result)) {
            OnRelease();
        }
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitD3D12Ext(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        ID3D12Device* device,
        NVSDK_NGX_Version sdkVersion,
        const NVSDK_NGX_Parameter* parameters)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_Init_Ext");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long,
            const wchar_t*,
            ID3D12Device*,
            NVSDK_NGX_Version,
            const NVSDK_NGX_Parameter*);

        // @todo: according to preliminary research, NUKEM mod fails at init under linux and does nothing good here, so we disable the call
        // @todo: fixme
        //result = proxy(applicationId, applicationDataPath, device, sdkVersion, parameters);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitD3D12(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        ID3D12Device* device,
        NVSDK_NGX_Version sdkVersion)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_Init");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long,
            const wchar_t*,
            ID3D12Device*,
            NVSDK_NGX_Version);

        // @todo: according to preliminary research, NUKEM mod fails at init under linux and does nothing good here, so we disable the call
        // @todo: fixme
        //result = proxy(applicationId, applicationDataPath, device, sdkVersion);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ShutdownD3D12()
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_Shutdown");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE();

        result = proxy();
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ShutdownVulkan()
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Shutdown");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE();

        result = proxy();
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ShutdownVulkan_1(void* LogicalDevice)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Shutdown1");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(void*);

        result = proxy(LogicalDevice);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::ShutdownD3D12_1(ID3D12Device* device)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_Shutdown1");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(ID3D12Device*);

        result = proxy(device);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::GetDriverVersionEx(
        uint32_t* versions,
        uint32_t inputVersionCount,
        uint32_t* totalDriverVersionCount)
    {
        NGX_INIT_CALL("NVSDK_NGX_GetDriverVersionEx");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(uint32_t*, uint32_t, uint32_t*);

        result = proxy(versions, inputVersionCount, totalDriverVersionCount);
        NGX_LOG_RESULT_AND_RETURN;
    }

    uint32_t DlssgProxy::GetApplicationId()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetApplicationId");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    uint32_t DlssgProxy::GetApiVersion()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetAPIVersion");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    uint32_t DlssgProxy::GetDriverVersion()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetDriverVersion");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    NVSDK_NGX_Result DlssgProxy::GetScratchBufferSizeD3D12(
        NVSDK_NGX_Feature featureId,
        const NVSDK_NGX_Parameter* parameters,
        size_t* outSizeInBytes)
    {
        NGX_INIT_CALL("NVSDK_NGX_D3D12_GetScratchBufferSize");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, size_t*);

        result = proxy(featureId, parameters, outSizeInBytes);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::GetScratchBufferSizeVulkan(
        NVSDK_NGX_Feature featureId,
        const NVSDK_NGX_Parameter* parameters,
        size_t* outSizeInBytes)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_GetScratchBufferSize");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Feature, const NVSDK_NGX_Parameter*, size_t*);

        result = proxy(featureId, parameters, outSizeInBytes);
        NGX_LOG_RESULT_AND_RETURN;
    }


    uint32_t DlssgProxy::GetGpuArchitecture()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetGPUArchitecture");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = 10;// proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    uint32_t DlssgProxy::GetSnippetVersion()
    {
        NGX_INIT_CALL_INT("NVSDK_NGX_GetSnippetVersion");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE_INT();

        const uint32_t result = proxy();
        NGX_LOG_RESULT_AND_RETURN_INT;
    }

    NVSDK_NGX_Result DlssgProxy::CreateVulkan(
        void* cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle** outHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_CreateFeature");

        NGX_LOG_CALL;

        NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

        NGX_RESOLVE_PROXY_ONCE(void*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
        result = proxy(cmdBuffer, featureId, parameters, outHandle);
        auto handle = (outHandle && *outHandle) ? *outHandle : nullptr;

        if (NVSDK_NGX_SUCCEED(result)) {
            OnCreate();
        }

        NGX_LOG_RESULT_WITH_HANDLE_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::CreateVulkan1(
        const VkDevice device,
        void* cmdBuffer,
        NVSDK_NGX_Feature featureId,
        NVSDK_NGX_Parameter* parameters,
        NVSDK_NGX_Handle** outHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_CreateFeature1");

        NGX_LOG_CALL;

        NGX_VALIDATE_FEATURE_ID(NVSDK_NGX_Feature_FrameGeneration);

        NGX_RESOLVE_PROXY_ONCE(const VkDevice, void*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);

        result = proxy(device, cmdBuffer, featureId, parameters, outHandle);
        auto handle = (outHandle && *outHandle) ? *outHandle : nullptr;

        if (NVSDK_NGX_SUCCEED(result)) {
            OnCreate();
        }
        NGX_LOG_RESULT_WITH_HANDLE_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::EvaluateVulkan(
        void* cmdBuffer,
        const NVSDK_NGX_Handle* featureHandle,
        NVSDK_NGX_Parameter* parameters,
        PFN_NVSDK_NGX_ProgressCallback callback)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_EvaluateFeature");

        const bool isFirstCall = !state.isFgEvaluated.exchange(true);

        NGX_RESOLVE_PROXY_ONCE(void*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, PFN_NVSDK_NGX_ProgressCallback);

        int frameIndex = 1;
        int frameMax = 1;

        parameters->Get("DLSSG.MultiFrameIndex", &frameIndex);
        parameters->Get("DLSSG.MultiFrameCount", &frameMax);

        if (OverdriveController::GetDynamicFrameGenerationEnabled()) {
            ctx.ngx.isGeneratingFrames = FrameGenerationHelper::ShouldGenerateFrame();
        }
        else {
            ctx.ngx.isGeneratingFrames = true;
        }

        if (!ctx.ngx.isGeneratingFrames) {
            parameters->Set("DLSSG.Reset", 1);
            ctx.ngx.isDuplicatingFrames = true;
            ctx.ngx.framesGenerated = 0;
            ctx.ngx.maxFramesGenerated = max(frameMax, 1);
        }
        else {
            ctx.ngx.framesGenerated = max(frameMax, 1);
            ctx.ngx.isDuplicatingFrames = false;
            ctx.ngx.maxFramesGenerated = max(frameMax, 1);
        }

        ctx.ngx.isFrameGenerationActive = true;

        if (isFirstCall) { NGX_LOG_CALL; }
        result = proxy(cmdBuffer, featureHandle, parameters, callback);
        ctx.ngx.lastEvaluationTimeMsec = Common::GetCurrentTimeMsec();

        if (!NVSDK_NGX_SUCCEED(result)) {
            LogWarning(kEntry, L"DLSSG failed for " + std::to_wstring(featureHandle->Id));
        }

        if (isFirstCall) { NGX_LOG_RESULT_AND_RETURN; }
        else if (!NVSDK_NGX_SUCCEED(result)) { NGX_LOG_RESULT_AND_RETURN; }

        return result;
    }

    NVSDK_NGX_Result DlssgProxy::ReleaseVulkan(NVSDK_NGX_Handle* instanceHandle)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_ReleaseFeature");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Handle*);

        result = proxy(instanceHandle);

        if (NVSDK_NGX_SUCCEED(result)) {
            OnRelease();
        }
        NGX_LOG_RESULT_AND_RETURN;
    }


    NVSDK_NGX_Result DlssgProxy::PopulateParametersVulkan(NVSDK_NGX_Parameter* InParams)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_PopulateParameters_Impl");

        NGX_LOG_CALL;

        NGX_RESOLVE_PROXY_ONCE(NVSDK_NGX_Parameter*);

        result = proxy(InParams);
        InParams->Set("FrameGeneration.Available", 1);
        InParams->Set("FrameGeneration.FeatureInitResult", 1);
        InParams->Set("FrameGeneration.MinDriverVersionMajor", 10);
        InParams->Set("FrameGeneration.MinDriverVersionMinor", 10);
        InParams->Set("FrameGeneration.NeedsUpdatedDriver", 0);
        InParams->Set("FrameInterpolation.Available", 1);
        InParams->Set("FrameInterpolation.FeatureInitResult", 1);
        InParams->Set("FrameInterpolation.MinDriverVersionMajor", 10);
        InParams->Set("FrameInterpolation.MinDriverVersionMinor", 10);
        InParams->Set("FrameInterpolation.NeedsUpdatedDriver", 0);
        InParams->Set("DLSSG.MultiFrameCountMax", 1);
        //InParams->Set("DLSSEnabler.Available", 1); //

        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitVulkan(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        void* instance,
        void* physicalDevice,
        void* device,
        void* getInstanceProcAddr,
        void* getDeviceProcAddr,
        const NVSDK_NGX_FeatureCommonInfo* featureInfo,
        NVSDK_NGX_Version sdkVersion)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init");

        NGX_LOG_CALL;
        ScopedGpuSpoofing guard;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*, void*, void*, void*, void*, void*, const NVSDK_NGX_FeatureCommonInfo*, NVSDK_NGX_Version);

        result = proxy(applicationId, applicationDataPath, instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr, featureInfo, sdkVersion);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitVulkanExt(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        void* instance, void* physicalDevice, void* device,
        NVSDK_NGX_Version sdkVersion,
        const NVSDK_NGX_FeatureCommonInfo* featureInfo)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init_Ext");

        NGX_LOG_CALL;
        ScopedGpuSpoofing guard;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long,
            const wchar_t*,
            void*, void*, void*,
            NVSDK_NGX_Version,
            const NVSDK_NGX_FeatureCommonInfo*);

        result = proxy(applicationId, applicationDataPath,
            instance, physicalDevice, device,
            sdkVersion, featureInfo);

        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::InitVulkanExt2(
        unsigned long long applicationId,
        const wchar_t* applicationDataPath,
        void* instance, void* physicalDevice, void* device,
        void* getInstanceProcAddr, void* getDeviceProcAddr,
        NVSDK_NGX_Version sdkVersion,
        const NVSDK_NGX_FeatureCommonInfo* featureInfo)
    {
        NGX_INIT_CALL("NVSDK_NGX_VULKAN_Init_Ext2");

        NGX_LOG_CALL;
        ScopedGpuSpoofing guard;

        NGX_RESOLVE_PROXY_ONCE(unsigned long long, const wchar_t*, void*, void*, void*, void*, void*, NVSDK_NGX_Version, const NVSDK_NGX_FeatureCommonInfo*);

        result = proxy(applicationId, applicationDataPath, instance, physicalDevice, device, getInstanceProcAddr, getDeviceProcAddr, sdkVersion, featureInfo);
        NGX_LOG_RESULT_AND_RETURN;
    }

    NVSDK_NGX_Result DlssgProxy::EstimateVRAMCallback(uint32_t mvecDepthWidth, uint32_t mvecDepthHeight,
        uint32_t colorWidth, uint32_t colorHeight,
        uint32_t colorBufferFormat,
        uint32_t mvecBufferFormat, uint32_t depthBufferFormat,
        uint32_t hudLessBufferFormat, uint32_t uiBufferFormat, size_t* EstimatedSize)
    {
        if (EstimatedSize) {
            const size_t renderPixels = (size_t)mvecDepthWidth * mvecDepthHeight;
            const size_t outputPixels = (size_t)colorWidth * colorHeight;

            const uint32_t ofBlockSize = 8;
            const size_t ofPixels =
                ((colorWidth + ofBlockSize - 1) / ofBlockSize) *
                ((colorHeight + ofBlockSize - 1) / ofBlockSize);
            const uint32_t ofLevels = 5;

            const size_t bpp4 = 4; // RGBA8 / R32F / RG16F
            const size_t bpp2 = 2; // R16F / R8

            *EstimatedSize =
                // user-facing resources (output res, 4 bpp)
                // color, depth, hudless, UI,
                // output interpolated, output real,
                // previous output, calculated depth,
                // bidirectional distortion field,
                // optical flow MV, disocclusion mask,
                // motion vectors input
                outputPixels * bpp4 * 12

                // FI internal (output res)
                + outputPixels * bpp4 * 4       // GameMVFieldXY, OpticalFlowMVFieldXY,

                // atomic counters, spare
                + outputPixels * bpp2 * 2       // disocclusion masks (prev + curr)

                // FI internal (render res)
                + renderPixels * bpp4 * 3       // dilated depth, dilated MVs,
                // estimated previous depth

                // optical flow pyramid
                +ofPixels * bpp4 * (ofLevels * 3 + 1) // 2x luminance + vector pyramid + SCD
                ;
        }

        return NVSDK_NGX_Result_Success;
    }

    void DlssgProxy::PopulateParameters(NVSDK_NGX_Parameter* Parameters)
    {
        Parameters->Set("Enable.OFA", 1);
        Parameters->Set("DLSSG.EnableInterp", 1);
        Parameters->Set("SynchronousInit", 1);
        Parameters->Set("DLSSG.CameraPinholeOffsetX", 0.0f);
        Parameters->Set("DLSSG.CameraPinholeOffsetY", 0.0f);
        Parameters->Set("DLSSG.EstimateVRAMCallback", &DlssgProxy::EstimateVRAMCallback);
        Parameters->Set(NVSDK_NGX_Parameter_FrameInterpolation_NeedsUpdatedDriver, 0);
        Parameters->Set(NVSDK_NGX_Parameter_FrameInterpolation_FeatureInitResult, 1);
        Parameters->Set(NVSDK_NGX_Parameter_FrameInterpolation_MinDriverVersionMajor, 10);
    }

    HMODULE DlssgProxy::GetBackend()
    {
        return backends.GetFrameGen();
    }
}