// =============================================================================
// SsrtgiListener.h - SSRTGI Post-Processing Effect Registration
// =============================================================================
//
// Call SsrtgiListener::Register() at application startup to enable SSRTGI
// post-processing for DLSS SuperSampling features.
//
// =============================================================================

#pragma once

#include <string>

namespace SsrtgiListener
{
    // =============================================================================
    // Configuration
    // =============================================================================

    // Set the directory where CSO shader files are located
    void SetCsoDirectory(const std::wstring& path);
    const std::wstring& GetCsoDirectory();

    // Enable/disable shader hot-reload (useful for development)
    void SetHotReloadEnabled(bool enabled);
    bool IsHotReloadEnabled();

    // =============================================================================
    // Registration
    // =============================================================================

    // Register SSRTGI as a listener for NGX feature events
    // Call this once at application startup, after setting configuration
    void Register();

    // Unregister listeners (use with caution - may affect other listeners)
    void Unregister();
}
