#pragma once

#ifndef NVAPI_DISABLE_GPU_SPOOFING
#define NVAPI_DISABLE_GPU_SPOOFING() ((void)0)
#endif
#ifndef NVAPI_ENABLE_GPU_SPOOFING
#define NVAPI_ENABLE_GPU_SPOOFING()  ((void)0)
#endif

// Optional compile-time switch to entirely disable the guard in this build.
// Define NGX_NO_GPU_SPOOF_GUARD in your project settings if desired.
#ifdef NGX_NO_GPU_SPOOF_GUARD
struct ScopedGpuSpoofing {
    explicit ScopedGpuSpoofing(bool /*reenable*/ = true) {}
    ~ScopedGpuSpoofing() {}
};
#else
struct ScopedGpuSpoofing {
    bool reenable;
    explicit ScopedGpuSpoofing(bool reenable = true) : reenable(reenable) {
        NVAPI_DISABLE_GPU_SPOOFING();
    }
    ~ScopedGpuSpoofing() {
        if (reenable) NVAPI_ENABLE_GPU_SPOOFING();
    }
};
#endif