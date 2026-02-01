// =============================================================================
// SettingsPersistence.h - INI-based Settings Persistence for DLSS Enabler
// =============================================================================
// 
// Handles saving and loading UI settings from dlss-enabler.ini file.
// Tracks changes and provides UI prompt to persist or ignore changes.
//
// USAGE:
//   1. Call SettingsPersistence::Init() at startup (after ctx is initialized)
//   2. Call SettingsPersistence::Load() to load saved settings
//   3. Call SettingsPersistence::CheckForChanges() every frame to detect changes
//   4. Call SettingsPersistence::RenderPersistPrompt() to show save dialog
//   5. Call SettingsPersistence::Save() to persist settings to INI
// =============================================================================

#pragma once

#include <Windows.h>
#include <string>

namespace SettingsPersistence
{
    // Initialize the persistence system
    // Should be called after ctx is initialized
    void Init();

    // Load settings from INI file
    // Returns true if file was loaded successfully
    bool Load();

    // Save current settings to INI file
    // Returns true if file was saved successfully
    bool Save();

    // Check if any tracked settings have changed since last save/load
    // Call this every frame to detect changes
    void CheckForChanges();

    // Returns true if there are unsaved changes
    bool HasUnsavedChanges();

    // Clear the unsaved changes flag (called after Save or when user clicks Ignore)
    void ClearUnsavedChanges();

    // Render the "Settings Changed" prompt window
    // Should be called every frame when sidebar/menu is visible
    // Shows only when HasUnsavedChanges() returns true
    void RenderPersistPrompt();

    // Get the full path to the INI file
    std::wstring GetIniFilePath();

    // Snapshot current settings (for comparison)
    // Called automatically after Load() and Save()
    void SnapshotCurrentSettings();
}
