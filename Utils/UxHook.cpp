// =============================================================================
// UxHook.cpp - D3D11/D3D12 DXGI Hook with UxImGui Overlay (v12 - Dynamic WndProc)
// =============================================================================
// Changes from v11:
// - WndProc hook is now DYNAMIC: hooked only when our menu is open, unhooked when closed
// - This prevents infinite recursion with OptiScaler's WndProc hook
// - Added inHookedWndProc guard as additional safety against recursion
// =============================================================================

#include "UxHook.h"
#include "SettingsMenu.h"
#include "MenuAnimations.h"
#include "Common.h"
#include "../Core/Context.h"

// UxImGui - Our isolated UxImGui with UxImGui:: namespace
// These are modified copies with GUxImGui->GUxImGui and UxImGui::->UxImGui::
#include "UxImGui/imgui.h"
#include "UxImGui/imgui_internal.h"
#include "UxImGui/imgui_impl_win32.h"
#include "UxImGui/imgui_impl_dx11.h"
#include "UxImGui/imgui_impl_dx12.h"

#include <mutex>
#include <atomic>
#include <sstream>

// Forward declaration for WndProc hook
static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declare menu state functions
void UxHook_SetMenuOpen(bool open);
bool UxHook_IsMenuOpen();

// =============================================================================
// Cursor Hook System - Using Microsoft Detours (exactly like OptiScaler)
// Based on OptiScaler's menu_common.cpp implementation
// =============================================================================
#include "../Detours/detours.h"

namespace CursorHook
{
    // Function pointer types (matching OptiScaler exactly)
    typedef BOOL(WINAPI* PFN_SetCursorPos)(int X, int Y);
    typedef BOOL(WINAPI* PFN_ClipCursor)(const RECT* lpRect);
    typedef void (WINAPI* PFN_mouse_event)(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, ULONG_PTR dwExtraInfo);
    typedef UINT(WINAPI* PFN_SendInput)(UINT cInputs, LPINPUT pInputs, int cbSize);
    typedef LRESULT(WINAPI* PFN_SendMessageW)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    typedef BOOL(WINAPI* PFN_GetCursorPos)(LPPOINT lpPoint);
    typedef HCURSOR(WINAPI* PFN_SetCursor)(HCURSOR hCursor);
    typedef UINT(WINAPI* PFN_GetRawInputData)(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);

    // Original function pointers (will be filled by Detours)
    static PFN_SetCursorPos pfn_SetCursorPos = nullptr;
    static PFN_SetCursorPos pfn_SetPhysicalCursorPos = nullptr;
    static PFN_ClipCursor pfn_ClipCursor = nullptr;
    static PFN_mouse_event pfn_mouse_event = nullptr;
    static PFN_SendInput pfn_SendInput = nullptr;
    static PFN_SendMessageW pfn_SendMessageW = nullptr;
    static PFN_GetCursorPos pfn_GetCursorPos = nullptr;
    static PFN_SetCursor pfn_SetCursor = nullptr;
    static PFN_GetRawInputData pfn_GetRawInputData = nullptr;

    // Hook status flags
    static bool pfn_SetCursorPos_hooked = false;
    static bool pfn_SetPhysicalCursorPos_hooked = false;
    static bool pfn_GetCursorPos_hooked = false;
    static bool pfn_ClipCursor_hooked = false;
    static bool pfn_mouse_event_hooked = false;
    static bool pfn_SendInput_hooked = false;
    static bool pfn_SendMessageW_hooked = false;
    static bool pfn_SetCursor_hooked = false;
    static bool pfn_GetRawInputData_hooked = false;

    // Saved cursor state
    static RECT _cursorLimit = {};
    static POINT _lastPoint = {};
    static HCURSOR _savedCursor = nullptr;
    static bool _restoreCursorPending = false;
    static bool g_HooksInstalled = false;

    // =========================================================================
    // Hook functions (matching OptiScaler's implementation exactly)
    // =========================================================================

    static LRESULT WINAPI hkSendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
    {
        if (UxHook_IsMenuOpen() && Msg == 0x0020)
            return TRUE;
        return pfn_SendMessageW(hWnd, Msg, wParam, lParam);
    }

    static BOOL WINAPI hkSetPhysicalCursorPos(int x, int y)
    {
        if (UxHook_IsMenuOpen())
            return TRUE;
        return pfn_SetPhysicalCursorPos(x, y);
    }

    static BOOL WINAPI hkGetPhysicalCursorPos(LPPOINT lpPoint)
    {
        if (UxHook_IsMenuOpen())
        {
            lpPoint->x = _lastPoint.x;
            lpPoint->y = _lastPoint.y;
            return TRUE;
        }
        return pfn_SetPhysicalCursorPos ? pfn_GetCursorPos(lpPoint) : FALSE;
    }

    static BOOL WINAPI hkGetCursorPos(LPPOINT lpPoint)
    {
        if (UxHook_IsMenuOpen())
        {
            lpPoint->x = _lastPoint.x;
            lpPoint->y = _lastPoint.y;
            return TRUE;
        }
        return pfn_GetCursorPos(lpPoint);
    }

    static BOOL GetRealCursorPos(LPPOINT lpPoint)
    {
        if (pfn_GetCursorPos)
            return pfn_GetCursorPos(lpPoint);
        return ::GetCursorPos(lpPoint);
    }

    static BOOL WINAPI hkSetCursorPos(int x, int y)
    {
        if (UxHook_IsMenuOpen())
            return TRUE;
        return pfn_SetCursorPos(x, y);
    }

    static BOOL WINAPI hkClipCursor(const RECT* lpRect)
    {
        if (UxHook_IsMenuOpen())
            return TRUE;
        return pfn_ClipCursor(lpRect);
    }

    static void WINAPI hkmouse_event(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, ULONG_PTR dwExtraInfo)
    {
        if (UxHook_IsMenuOpen())
            return;
        pfn_mouse_event(dwFlags, dx, dy, dwData, dwExtraInfo);
    }

    static UINT WINAPI hkSendInput(UINT cInputs, LPINPUT pInputs, int cbSize)
    {
        if (UxHook_IsMenuOpen())
            return TRUE;
        return pfn_SendInput(cInputs, pInputs, cbSize);
    }

    static UINT WINAPI hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader)
    {
        UINT result = pfn_GetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);

        if (UxHook_IsMenuOpen() && pData != nullptr && result != (UINT)-1)
        {
            RAWINPUT* raw = (RAWINPUT*)pData;
            if (raw->header.dwType == RIM_TYPEMOUSE)
            {
                raw->data.mouse.lLastX = 0;
                raw->data.mouse.lLastY = 0;
                raw->data.mouse.usButtonFlags = 0;
                raw->data.mouse.usButtonData = 0;
            }
        }

        return result;
    }

    static HCURSOR WINAPI hkSetCursor(HCURSOR hCursor)
    {
        if (UxHook_IsMenuOpen())
        {
            pfn_SetCursor(NULL);
            return NULL;
        }
        return pfn_SetCursor(hCursor);
    }

    // =========================================================================
    // AttachHooks - Using Detours (exactly like OptiScaler)
    // =========================================================================
    static void AttachHooks()
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        pfn_SetPhysicalCursorPos = reinterpret_cast<PFN_SetCursorPos>(
            DetourFindFunction("user32.dll", "SetPhysicalCursorPos"));
        pfn_SetCursorPos = reinterpret_cast<PFN_SetCursorPos>(
            DetourFindFunction("user32.dll", "SetCursorPos"));
        pfn_ClipCursor = reinterpret_cast<PFN_ClipCursor>(
            DetourFindFunction("user32.dll", "ClipCursor"));
        pfn_mouse_event = reinterpret_cast<PFN_mouse_event>(
            DetourFindFunction("user32.dll", "mouse_event"));
        pfn_SendInput = reinterpret_cast<PFN_SendInput>(
            DetourFindFunction("user32.dll", "SendInput"));
        pfn_SendMessageW = reinterpret_cast<PFN_SendMessageW>(
            DetourFindFunction("user32.dll", "SendMessageW"));
        pfn_GetCursorPos = reinterpret_cast<PFN_GetCursorPos>(
            DetourFindFunction("user32.dll", "GetCursorPos"));
        pfn_SetCursor = reinterpret_cast<PFN_SetCursor>(
            DetourFindFunction("user32.dll", "SetCursor"));
        pfn_GetRawInputData = reinterpret_cast<PFN_GetRawInputData>(
            DetourFindFunction("user32.dll", "GetRawInputData"));

        if (pfn_SetPhysicalCursorPos && (pfn_SetPhysicalCursorPos != pfn_SetCursorPos))
            pfn_SetPhysicalCursorPos_hooked = (DetourAttach(&(PVOID&)pfn_SetPhysicalCursorPos, hkSetPhysicalCursorPos) == 0);

        if (pfn_SetCursorPos)
            pfn_SetCursorPos_hooked = (DetourAttach(&(PVOID&)pfn_SetCursorPos, hkSetCursorPos) == 0);

        if (pfn_GetCursorPos)
            pfn_GetCursorPos_hooked = (DetourAttach(&(PVOID&)pfn_GetCursorPos, hkGetCursorPos) == 0);

        if (pfn_ClipCursor)
            pfn_ClipCursor_hooked = (DetourAttach(&(PVOID&)pfn_ClipCursor, hkClipCursor) == 0);

        if (pfn_mouse_event)
            pfn_mouse_event_hooked = (DetourAttach(&(PVOID&)pfn_mouse_event, hkmouse_event) == 0);

        if (pfn_SendInput)
            pfn_SendInput_hooked = (DetourAttach(&(PVOID&)pfn_SendInput, hkSendInput) == 0);

        if (pfn_SendMessageW)
            pfn_SendMessageW_hooked = (DetourAttach(&(PVOID&)pfn_SendMessageW, hkSendMessageW) == 0);

        if (pfn_SetCursor)
            pfn_SetCursor_hooked = (DetourAttach(&(PVOID&)pfn_SetCursor, hkSetCursor) == 0);

        if (pfn_GetRawInputData)
            pfn_GetRawInputData_hooked = (DetourAttach(&(PVOID&)pfn_GetRawInputData, hkGetRawInputData) == 0);

        DetourTransactionCommit();

        LOG_TRACE(L"[UxHook] CursorHook: Detours attached");
    }

    // =========================================================================
    // DetachHooks - Using Detours (exactly like OptiScaler)
    // =========================================================================
    static void DetachHooks()
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        if (pfn_SetPhysicalCursorPos_hooked)
            DetourDetach(&(PVOID&)pfn_SetPhysicalCursorPos, hkSetPhysicalCursorPos);

        if (pfn_SetCursorPos_hooked)
            DetourDetach(&(PVOID&)pfn_SetCursorPos, hkSetCursorPos);

        if (pfn_GetCursorPos_hooked)
            DetourDetach(&(PVOID&)pfn_GetCursorPos, hkGetCursorPos);

        if (pfn_ClipCursor_hooked)
            DetourDetach(&(PVOID&)pfn_ClipCursor, hkClipCursor);

        if (pfn_mouse_event_hooked)
            DetourDetach(&(PVOID&)pfn_mouse_event, hkmouse_event);

        if (pfn_SendInput_hooked)
            DetourDetach(&(PVOID&)pfn_SendInput, hkSendInput);

        if (pfn_SendMessageW_hooked)
            DetourDetach(&(PVOID&)pfn_SendMessageW, hkSendMessageW);

        if (pfn_SetCursor_hooked)
            DetourDetach(&(PVOID&)pfn_SetCursor, hkSetCursor);

        if (pfn_GetRawInputData_hooked)
            DetourDetach(&(PVOID&)pfn_GetRawInputData, hkGetRawInputData);

        pfn_SetPhysicalCursorPos_hooked = false;
        pfn_SetCursorPos_hooked = false;
        pfn_GetCursorPos_hooked = false;
        pfn_ClipCursor_hooked = false;
        pfn_mouse_event_hooked = false;
        pfn_SendInput_hooked = false;
        pfn_SendMessageW_hooked = false;
        pfn_SetCursor_hooked = false;
        pfn_GetRawInputData_hooked = false;

        pfn_SetPhysicalCursorPos = nullptr;
        pfn_SetCursorPos = nullptr;
        pfn_ClipCursor = nullptr;
        pfn_mouse_event = nullptr;
        pfn_SendInput = nullptr;
        pfn_SendMessageW = nullptr;
        pfn_SetCursor = nullptr;

        DetourTransactionCommit();

        LOG_TRACE(L"[UxHook] CursorHook: Detours detached");
    }

    // =========================================================================
    // Public API
    // =========================================================================
    static void Install()
    {
        if (g_HooksInstalled) return;

        AttachHooks();
        g_HooksInstalled = true;

        LOG_INFO(L"[UxHook] CursorHook: Install complete");
    }

    static void Uninstall()
    {
        if (!g_HooksInstalled) return;

        DetachHooks();
        g_HooksInstalled = false;
    }

    static void OnMenuOpen()
    {
        if (pfn_ClipCursor_hooked)
        {
            GetClipCursor(&_cursorLimit);
            pfn_ClipCursor(nullptr);
        }
        else
        {
            GetClipCursor(&_cursorLimit);
            ClipCursor(nullptr);
        }

        GetCursorPos(&_lastPoint);
        while (ShowCursor(TRUE) < 0) {}

        LOG_DEBUG(L"[UxHook] CursorHook: Menu opened, cursor released");
    }

    static void OnMenuClose()
    {
        if (_cursorLimit.right > _cursorLimit.left && _cursorLimit.bottom > _cursorLimit.top)
        {
            if (pfn_ClipCursor_hooked)
                pfn_ClipCursor(&_cursorLimit);
            else
                ClipCursor(&_cursorLimit);
        }

        _restoreCursorPending = false;
        _savedCursor = nullptr;

        ShowCursor(FALSE);

        LOG_DEBUG(L"[UxHook] CursorHook: Menu closed, cursor restored");
    }

    static void SendSetCursorToGame(HWND hwnd)
    {
        if (hwnd)
        {
            PostMessage(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
            LOG_TRACE(L"[UxHook] CursorHook: Sent WM_SETCURSOR to game window");
        }
    }
}

// =============================================================================
// Global State
// =============================================================================

namespace
{
    UxHook::RenderState g_State;
    std::recursive_mutex g_Mutex;
    bool g_Initialized = false;

    UxImGuiContext* g_UxImGuiContext = nullptr;

    constexpr ULONGLONG POPUP_DELAY_MS = 3000;
    constexpr ULONGLONG POPUP_DURATION_MS = 10000;

#define MENU_TOGGLE_KEY (SettingsMenu::GetMenuToggleKey())

    static constexpr UINT NUM_BACK_BUFFERS = 3;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> g_CommandAllocators[NUM_BACK_BUFFERS];
    UINT64 g_FrameCount = 0;
    ID3D12CommandQueue* g_SwapChainCommandQueue = nullptr;

    std::atomic<bool> g_OverlayDisabled{ false };
    IDXGISwapChain* g_InitializedSwapChain = nullptr;

    UxHook::GraphicsAPI g_ForceAPI = UxHook::GraphicsAPI::Unknown;
}

// =============================================================================
// D3D12 Descriptor Heap Allocator
// =============================================================================

static DescriptorHeapAllocator g_SrvDescriptorAllocator;

static void SrvDescriptorAllocCallback(UxImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
{
    g_SrvDescriptorAllocator.Alloc(out_cpu, out_gpu);
}

static void SrvDescriptorFreeCallback(UxImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
{
    g_SrvDescriptorAllocator.Free(cpu, gpu);
}

// =============================================================================
// Shared Helper Functions
// =============================================================================

// External: force Streamline to apply current mfgEnforcedMode (defined in StreamlineProxy.cpp)
extern void StreamlineProxy_ForceApplyMfgMode();

namespace
{
    void UpdateImGuiIO(HWND targetWindow, bool menuOpen, LARGE_INTEGER& lastTime, LARGE_INTEGER& freq)
    {
        UxImGuiIO& io = UxImGui::GetIO();

        RECT rect = { 0, 0, 0, 0 };
        if (targetWindow)
            GetClientRect(targetWindow, &rect);
        io.DisplaySize = UxImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

        if (freq.QuadPart == 0)
            QueryPerformanceFrequency(&freq);

        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);

        if (lastTime.QuadPart > 0)
            io.DeltaTime = (float)(currentTime.QuadPart - lastTime.QuadPart) / (float)freq.QuadPart;
        else
            io.DeltaTime = 1.0f / 60.0f;

        if (io.DeltaTime <= 0.0f)
            io.DeltaTime = 1.0f / 60.0f;

        lastTime = currentTime;

        if (menuOpen)
        {
            io.MouseDrawCursor = true;

            POINT mousePos;
            if (CursorHook::GetRealCursorPos(&mousePos))
            {
                if (targetWindow && ScreenToClient(targetWindow, &mousePos))
                {
                    io.MousePos.x = (float)mousePos.x;
                    io.MousePos.y = (float)mousePos.y;
                }
            }

            io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
        }
        else
        {
            io.MouseDrawCursor = false;
            io.MousePos = UxImVec2(-FLT_MAX, -FLT_MAX);
            io.MouseDown[0] = io.MouseDown[1] = io.MouseDown[2] = false;
        }
    }

    // =========================================================================
    // Dynamic WndProc hook/unhook helpers
    // =========================================================================
    void HookWndProc(HWND targetWindow)
    {
        if (!targetWindow || g_State.OriginalWndProc)
            return;  // Already hooked or no window

        WNDPROC current = (WNDPROC)GetWindowLongPtrW(targetWindow, GWLP_WNDPROC);
        if (current != HookedWndProc)
        {
            g_State.OriginalWndProc = (WNDPROC)SetWindowLongPtrW(
                targetWindow, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
            LOG_INFO(L"[UxHook] WndProc hooked (menu opening)");
        }
    }

    void UnhookWndProc()
    {
        if (!g_State.OriginalWndProc || !g_State.TargetWindow)
            return;  // Not hooked

        // Verify we're still the current WndProc before restoring
        // If someone else hooked after us, don't restore (would break their chain)
        WNDPROC current = (WNDPROC)GetWindowLongPtrW(g_State.TargetWindow, GWLP_WNDPROC);
        if (current == HookedWndProc)
        {
            SetWindowLongPtrW(g_State.TargetWindow, GWLP_WNDPROC, (LONG_PTR)g_State.OriginalWndProc);
            LOG_INFO(L"[UxHook] WndProc unhooked (menu closed)");
        }
        else
        {
            LOG_WARNING(L"[UxHook] WndProc was re-hooked by another module, skipping restore");
        }

        g_State.OriginalWndProc = nullptr;
    }

    // =========================================================================
    // Handle menu toggle key and cursor control with animations
    // =========================================================================
    void HandleMenuToggle(bool& showMenu, bool& keyWasPressed, HWND targetWindow)
    {
        HWND foreground = GetForegroundWindow();
        bool windowFocused = (foreground == targetWindow);
        // Don't trigger menu toggle when Ctrl is held (Ctrl+~ is a hotkey for monitoring bar toggle)
        bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        bool keyIsPressed = windowFocused && !ctrlHeld && (GetAsyncKeyState(MENU_TOGGLE_KEY) & 0x8000) != 0;

        if (keyIsPressed && !keyWasPressed)
        {
            if (!showMenu)
            {
                // Opening menu
                showMenu = true;
                LOG_INFO(L"[UxHook] Menu OPENING");

                MenuAnimations::StartSidePanelOpen();
                UxHook_SetMenuOpen(true);
                CursorHook::OnMenuOpen();

                // Hook WndProc NOW - only while menu is open
                HookWndProc(targetWindow);
            }
            else
            {
                // Closing menu - start animation
                LOG_INFO(L"[UxHook] Menu CLOSING");
                MenuAnimations::StartSidePanelClose();
            }
        }
        keyWasPressed = keyIsPressed;

        // Check if close animation finished
        if (showMenu && MenuAnimations::IsSidePanelFullyClosed())
        {
            showMenu = false;
            LOG_TRACE(L"[UxHook] Menu CLOSED (animation complete)");

            // Unhook WndProc FIRST - stop intercepting messages
            UnhookWndProc();

            UxHook_SetMenuOpen(false);
            CursorHook::OnMenuClose();
            CursorHook::SendSetCursorToGame(targetWindow);
            SettingsMenu::OnMenuClosed();
        }
    }

    // =========================================================================
    // Global hotkeys (polled each frame, independent of menu state / WndProc)
    //
    // Gated by ctx.areHotKeysEnabled. All hotkeys require Ctrl + key combo and
    // only fire when the game window is in focus.
    //
    //   Ctrl+~            toggle monitoring bar (ctx.isMonitoringEnabled)
    //                     -- gated additionally by ctx.isUiEnabled, because the
    //                     monitoring bar / side panel cannot render without UI
    //   Ctrl+1..Ctrl+6    set ctx.nvapi.mfgEnforcedMode = N (x1..x6)
    //                     -- works regardless of ctx.isUiEnabled; these are
    //                     pure state toggles and don't need the overlay
    //
    // Ctrl+5 / Ctrl+6 require sl.dlss_g >= 2.11 (same check as the combo box).
    //
    // IMPORTANT: This function is called from RenderOverlay BEFORE the
    // isUiEnabled early-out, so that MFG mode hotkeys remain available even
    // when the in-game UI is disabled.
    // =========================================================================

    void HandleHotkeys(HWND targetWindow)
    {
        if (!ctx.areHotKeysEnabled)
            return;

        HWND foreground = GetForegroundWindow();
        if (foreground != targetWindow)
            return;

        // Must hold Ctrl (either Left or Right)
        bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (!ctrlHeld)
            return;

        // Edge detection: [0]=~, [1..6]=Ctrl+1..Ctrl+6
        static bool s_wasPressed[7] = { false, false, false, false, false, false, false };

        auto edge = [](int vk, int slot) -> bool {
            bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
            bool fired = now && !s_wasPressed[slot];
            s_wasPressed[slot] = now;
            return fired;
            };

        // ---- Ctrl+~ : toggle monitoring bar ----
        // Only meaningful when UI is enabled; still poll edge() so we keep the
        // press/release state consistent and don't fire spuriously when UI is
        // re-enabled with the key still held down.
        {
            bool tildeEdge = edge(SettingsMenu::GetMenuToggleKey(), 0);
            if (tildeEdge && ctx.isUiEnabled)
            {
                ctx.isMonitoringEnabled = !ctx.isMonitoringEnabled;
                LOG_INFO(ctx.isMonitoringEnabled
                    ? L"[UxHook] Hotkey Ctrl+~: monitoring bar ON"
                    : L"[UxHook] Hotkey Ctrl+~: monitoring bar OFF");
            }
        }

        // ---- Ctrl+1..Ctrl+6 : MFG mode override ----
        // Validate 5X/6X against sl.dlss_g >= 2.11, mirroring the combo-box logic
        // in SettingsMenu.cpp (RenderSidePanel "MFG Mode Override" block).
        auto dlssg211Supported = []() -> bool {
            const std::wstring& dlssgVer = ctx.streamline.dlssgVersion;
            int dmaj = 0, dmin = 0;
            if (swscanf_s(dlssgVer.c_str(), L"%d.%d", &dmaj, &dmin) >= 2)
                return (dmaj > 2) || (dmaj == 2 && dmin >= 11);
            return false;
            };

        // Streamline must be loaded & >= 2.7.x for any override to be legal
        auto streamlineOk = []() -> bool {
            const std::wstring& ver = ctx.streamline.interposerVersion;
            int major = 0, minor = 0;
            if (swscanf_s(ver.c_str(), L"%d.%d", &major, &minor) >= 2)
            {
                bool loaded = (major > 0 || minor > 0);
                bool versionOk = (major > 2) || (major == 2 && minor >= 7);
                return loaded && versionOk;
            }
            return false;
            };

        for (int n = 1; n <= 6; ++n)
        {
            int vk = '0' + n;  // VK_1..VK_6 are '1'..'6' (0x31..0x36)
            if (!edge(vk, n))
                continue;

            if (!streamlineOk())
            {
                LOG_WARNING(L"[UxHook] Hotkey Ctrl+" + std::to_wstring(n) + L" ignored: Streamline not loaded or < 2.7");
                continue;
            }
            if (n >= 5 && !dlssg211Supported())
            {
                LOG_WARNING(L"[UxHook] Hotkey Ctrl+" + std::to_wstring(n) + L" ignored: sl.dlss_g < 2.11 (x5/x6 unsupported)");
                continue;
            }

            if (ctx.nvapi.mfgEnforcedMode == n)
            {
                LOG_DEBUG(L"[UxHook] Hotkey Ctrl+" + std::to_wstring(n) + L" already at x" + std::to_wstring(n));
             
                continue;
            }

            ctx.nvapi.mfgEnforcedMode = n;
            LOG_INFO(L"[UxHook] Hotkey Ctrl+" + std::to_wstring(n) + L": mfgEnforcedMode = x" + std::to_wstring(n));

            // Apply immediately via Streamline. Any non-zero nvapi value (including x1)
            // goes through ForceApply; the side-panel combo does the same thing.
            ::StreamlineProxy_ForceApplyMfgMode();
        }
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

namespace UxHook
{
    RenderState& GetState() { return g_State; }

    GraphicsAPI GetDetectedAPI() { return g_State.API; }

    // =========================================================================
    // API Detection
    // =========================================================================
    static GraphicsAPI DetectGraphicsAPI(IDXGISwapChain* pSwapChain)
    {
        if (g_ForceAPI != GraphicsAPI::Unknown)
        {
            LOG_INFO(g_ForceAPI == GraphicsAPI::D3D11 ?
                L"[UxHook] Using FORCED: D3D11" : L"[UxHook] Using FORCED: D3D12");
            return g_ForceAPI;
        }

        Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
        Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device;

        HRESULT hr11 = pSwapChain->GetDevice(IID_PPV_ARGS(&d3d11Device));
        HRESULT hr12 = pSwapChain->GetDevice(IID_PPV_ARGS(&d3d12Device));

        {
            std::wstringstream ss;
            ss << L"[UxHook] Detection: D3D11=" << (SUCCEEDED(hr11) ? L"YES" : L"NO")
                << L" D3D12=" << (SUCCEEDED(hr12) ? L"YES" : L"NO");
            LOG_DEBUG(ss.str());
        }

        if (g_SwapChainCommandQueue != nullptr)
        {
            LOG_DEBUG(L"[UxHook] Detected: D3D12 (CommandQueue was set)");
            return GraphicsAPI::D3D12;
        }

        if (SUCCEEDED(hr12) && FAILED(hr11))
        {
            LOG_INFO(L"[UxHook] Detected: D3D12");
            return GraphicsAPI::D3D12;
        }

        if (SUCCEEDED(hr11) && FAILED(hr12))
        {
            LOG_INFO(L"[UxHook] Detected: D3D11");
            return GraphicsAPI::D3D11;
        }

        if (SUCCEEDED(hr11) && SUCCEEDED(hr12))
        {
            LOG_WARNING(L"[UxHook] Both APIs detected - using D3D11 (might be D3D11on12)");
            return GraphicsAPI::D3D11;
        }

        LOG_WARNING(L"[UxHook] Detection FAILED - Unknown API");
        return GraphicsAPI::Unknown;
    }

    void ForceGraphicsAPI(GraphicsAPI api)
    {
        g_ForceAPI = api;
        LOG_WARNING(api == GraphicsAPI::D3D11 ?
            L"[UxHook] ForceGraphicsAPI: D3D11" :
            L"[UxHook] ForceGraphicsAPI: D3D12");
    }

    // =========================================================================
    // D3D11 Functions
    // =========================================================================
    static bool CreateRenderTarget_D3D11()
    {
        auto& state = g_State;

        state.D3D11RenderTargetView.Reset();

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(state.SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        {
            LOG_WARNING(L"[UxHook] D3D11 CreateRenderTarget: FAILED GetBuffer");
            return false;
        }

        if (FAILED(state.D3D11Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &state.D3D11RenderTargetView)))
        {
            LOG_WARNING(L"[UxHook] D3D11 CreateRenderTarget: FAILED CreateRenderTargetView");
            return false;
        }

        return true;
    }

    static void ReleaseRenderTarget_D3D11()
    {
        g_State.D3D11RenderTargetView.Reset();
    }

    static bool InitializeImGui_D3D11(IDXGISwapChain* pSwapChain)
    {
        auto& state = g_State;

        LOG_DEBUG(L"[UxHook] InitializeImGui_D3D11: Starting...");

        if (FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&state.D3D11Device))))
        {
            LOG_WARNING(L"[UxHook] D3D11 Init: FAILED GetDevice");
            return false;
        }

        state.D3D11Device->GetImmediateContext(&state.D3D11Context);
        if (!state.D3D11Context)
        {
            LOG_WARNING(L"[UxHook] D3D11 Init: FAILED GetImmediateContext");
            return false;
        }

        pSwapChain->QueryInterface(IID_PPV_ARGS(&state.SwapChain));

        DXGI_SWAP_CHAIN_DESC swapChainDesc;
        if (FAILED(pSwapChain->GetDesc(&swapChainDesc)))
        {
            LOG_WARNING(L"[UxHook] D3D11 Init: FAILED GetDesc");
            return false;
        }

        state.TargetWindow = swapChainDesc.OutputWindow;

        if (!state.TargetWindow || !IsWindow(state.TargetWindow))
        {
            LOG_WARNING(L"[UxHook] D3D11 Init: Invalid HWND from SwapChain!");
            return false;
        }

        {
            std::wstringstream ss;
            ss << L"[UxHook] D3D11 Init: HWND=0x" << std::hex << (uintptr_t)state.TargetWindow
                << L" BufferCount=" << std::dec << swapChainDesc.BufferCount;
            LOG_WARNING(ss.str());
        }

        if (!CreateRenderTarget_D3D11())
        {
            LOG_WARNING(L"[UxHook] D3D11 Init: FAILED CreateRenderTarget");
            return false;
        }

        // UxImGui setup
        UxImGuiContext* existingContext = UxImGui::GetCurrentContext();
        g_UxImGuiContext = UxImGui::CreateContext();
        UxImGui::SetCurrentContext(g_UxImGuiContext);

        UxImGuiIO& io = UxImGui::GetIO();
        io.ConfigFlags |= UxImGuiConfigFlags_NavEnableKeyboard;
        UxImGui::StyleColorsDark();

        io.BackendPlatformName = "imgui_impl_win32_uxhook";
        io.BackendFlags |= UxImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= UxImGuiBackendFlags_HasSetMousePos;

        if (!UxImGui_ImplDX11_Init(state.D3D11Device.Get(), state.D3D11Context.Get()))
        {
            LOG_WARNING(L"[UxHook] D3D11 Init: FAILED UxImGui_ImplDX11_Init");
            UxImGui::DestroyContext(g_UxImGuiContext);
            g_UxImGuiContext = nullptr;
            if (existingContext) UxImGui::SetCurrentContext(existingContext);
            return false;
        }

        // NOTE: WndProc is NOT hooked here anymore.
        // It will be hooked dynamically when our menu opens (HandleMenuToggle)
        // This avoids infinite recursion with OptiScaler's WndProc hook.
        state.OriginalWndProc = nullptr;
        LOG_INFO(L"[UxHook] D3D11 Init: WndProc hook DEFERRED (will hook on menu open)");

        state.API = GraphicsAPI::D3D11;
        state.StartTime = GetTickCount64();
        state.Initialized = true;
        g_Initialized = true;
        g_InitializedSwapChain = pSwapChain;

        SettingsMenu::SetTargetWindow(state.TargetWindow);

        // Install cursor hooks (Detours) - these are always active
        CursorHook::Install();

        if (existingContext)
            UxImGui::SetCurrentContext(existingContext);

        LOG_INFO(L"[UxHook] InitializeImGui_D3D11: SUCCESS!");
        return true;
    }

    static void RenderFrame_D3D11()
    {
        auto& state = g_State;

        if (!state.D3D11Context || !state.D3D11RenderTargetView)
        {
            LOG_WARNING(L"[UxHook] RenderFrame_D3D11: Missing context or RTV!");
            return;
        }

        static bool showMenu = false;
        static bool keyWasPressed = false;
        static LARGE_INTEGER lastTime11 = {};
        static LARGE_INTEGER freq11 = {};
        static bool animationsInitialized = false;

        if (!animationsInitialized)
        {
            MenuAnimations::Init();
            animationsInitialized = true;
        }

        UxImGui_ImplDX11_NewFrame();

        UpdateImGuiIO(state.TargetWindow, showMenu, lastTime11, freq11);

        UxImGui::NewFrame();

        MenuAnimations::Update(UxImGui::GetIO().DeltaTime);

        SettingsMenu::RenderMonitoringOverlay();

        HandleMenuToggle(showMenu, keyWasPressed, state.TargetWindow);
        // HandleHotkeys intentionally moved to RenderOverlay so it runs even
        // when ctx.isUiEnabled == false.

        if (showMenu || MenuAnimations::IsSidePanelAnimating())
        {
            SettingsMenu::Render(&showMenu);
        }

        UxImGui::Render();
        UxImDrawData* drawData = UxImGui::GetDrawData();

        if (drawData && drawData->TotalVtxCount > 0)
        {
            state.D3D11Context->OMSetRenderTargets(1, state.D3D11RenderTargetView.GetAddressOf(), nullptr);
            UxImGui_ImplDX11_RenderDrawData(drawData);
        }
    }

    // =========================================================================
    // D3D12 Functions
    // =========================================================================
    static bool CreateRenderTargets_D3D12()
    {
        auto& state = g_State;

        DXGI_SWAP_CHAIN_DESC desc;
        if (FAILED(state.SwapChain->GetDesc(&desc)))
            return false;

        state.BufferCount = desc.BufferCount;
        if (state.BufferCount > RenderState::MaxBufferCount)
            state.BufferCount = RenderState::MaxBufferCount;

        if (!state.RtvHeap)
        {
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.NumDescriptors = RenderState::MaxBufferCount;
            rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            if (FAILED(state.D3D12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&state.RtvHeap))))
                return false;
        }

        UINT rtvDescriptorSize = state.D3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = state.RtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < state.BufferCount; i++)
        {
            state.RtvHandles[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;

            if (FAILED(state.SwapChain->GetBuffer(i, IID_PPV_ARGS(&state.RenderTargets[i]))))
                return false;

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = desc.BufferDesc.Format;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            state.D3D12Device->CreateRenderTargetView(state.RenderTargets[i].Get(), &rtvDesc, state.RtvHandles[i]);
        }

        return true;
    }

    static void ReleaseRenderTargets_D3D12()
    {
        for (UINT i = 0; i < UxHook::RenderState::MaxBufferCount; i++)
            g_State.RenderTargets[i].Reset();
    }

    static void WaitForGpu_D3D12()
    {
        auto& state = g_State;

        if (!state.GameCommandQueue || !state.Fence || !state.FenceEvent)
            return;

        UINT64 fenceValue = ++state.FenceValue;
        state.GameCommandQueue->Signal(state.Fence.Get(), fenceValue);

        if (state.Fence->GetCompletedValue() < fenceValue)
        {
            state.Fence->SetEventOnCompletion(fenceValue, state.FenceEvent);
            WaitForSingleObject(state.FenceEvent, 5000);
        }
    }

    static bool InitializeImGui_D3D12(IDXGISwapChain* pSwapChain)
    {
        auto& state = g_State;

        LOG_INFO(L"[UxHook] InitializeImGui_D3D12: Starting...");

        if (FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&state.D3D12Device))))
        {
            LOG_WARNING(L"[UxHook] D3D12 Init: FAILED GetDevice");
            return false;
        }

        if (g_SwapChainCommandQueue)
        {
            state.GameCommandQueue = g_SwapChainCommandQueue;
        }
        else
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

            if (FAILED(state.D3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&state.OwnCommandQueue))))
            {
                LOG_WARNING(L"[UxHook] D3D12 Init: FAILED CreateCommandQueue");
                return false;
            }
            state.GameCommandQueue = state.OwnCommandQueue.Get();
        }

        pSwapChain->QueryInterface(IID_PPV_ARGS(&state.SwapChain));

        DXGI_SWAP_CHAIN_DESC swapChainDesc;
        pSwapChain->GetDesc(&swapChainDesc);
        state.TargetWindow = swapChainDesc.OutputWindow;

        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = 64;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (FAILED(state.D3D12Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&state.SrvHeap))))
            return false;

        g_SrvDescriptorAllocator.Create(state.D3D12Device.Get(), state.SrvHeap.Get());

        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            if (FAILED(state.D3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_CommandAllocators[i]))))
                return false;
        }

        if (FAILED(state.D3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            g_CommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&state.CommandList))))
            return false;
        state.CommandList->Close();

        if (FAILED(state.D3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&state.Fence))))
            return false;
        state.FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

        if (!CreateRenderTargets_D3D12())
            return false;

        // UxImGui setup
        UxImGuiContext* existingContext = UxImGui::GetCurrentContext();
        g_UxImGuiContext = UxImGui::CreateContext();
        UxImGui::SetCurrentContext(g_UxImGuiContext);

        UxImGuiIO& io = UxImGui::GetIO();
        io.ConfigFlags |= UxImGuiConfigFlags_NavEnableKeyboard;
        UxImGui::StyleColorsDark();

        io.BackendPlatformName = "imgui_impl_win32_uxhook";
        io.BackendFlags |= UxImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= UxImGuiBackendFlags_HasSetMousePos;

        DXGI_FORMAT rtvFormat = swapChainDesc.BufferDesc.Format;
        if (rtvFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        else if (rtvFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) rtvFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

        UxImGui_ImplDX12_InitInfo initInfo = {};
        initInfo.Device = state.D3D12Device.Get();
        initInfo.CommandQueue = state.GameCommandQueue;
        initInfo.NumFramesInFlight = NUM_BACK_BUFFERS;
        initInfo.RTVFormat = rtvFormat;
        initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
        initInfo.SrvDescriptorHeap = state.SrvHeap.Get();
        initInfo.SrvDescriptorAllocFn = SrvDescriptorAllocCallback;
        initInfo.SrvDescriptorFreeFn = SrvDescriptorFreeCallback;

        if (!UxImGui_ImplDX12_Init(&initInfo))
        {
            UxImGui::DestroyContext(g_UxImGuiContext);
            g_UxImGuiContext = nullptr;
            if (existingContext) UxImGui::SetCurrentContext(existingContext);
            return false;
        }

        // NOTE: WndProc is NOT hooked here anymore.
        // It will be hooked dynamically when our menu opens (HandleMenuToggle)
        // This avoids infinite recursion with OptiScaler's WndProc hook.
        state.OriginalWndProc = nullptr;
        LOG_INFO(L"[UxHook] D3D12 Init: WndProc hook DEFERRED (will hook on menu open)");

        state.API = GraphicsAPI::D3D12;
        state.StartTime = GetTickCount64();
        state.Initialized = true;
        g_Initialized = true;
        g_InitializedSwapChain = pSwapChain;

        SettingsMenu::SetTargetWindow(state.TargetWindow);

        // Install cursor hooks (Detours) - these are always active
        CursorHook::Install();

        if (existingContext)
            UxImGui::SetCurrentContext(existingContext);

        LOG_INFO(L"[UxHook] InitializeImGui_D3D12: SUCCESS!");
        return true;
    }

    static void RenderFrame_D3D12()
    {
        auto& state = g_State;

        UINT backBufferIndex = 0;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
        if (SUCCEEDED(state.SwapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
            backBufferIndex = swapChain3->GetCurrentBackBufferIndex();

        UINT frameIndex = g_FrameCount % NUM_BACK_BUFFERS;
        g_FrameCount++;

        if (state.Fence->GetCompletedValue() < state.FenceValue)
        {
            state.Fence->SetEventOnCompletion(state.FenceValue, state.FenceEvent);
            WaitForSingleObject(state.FenceEvent, 100);
        }

        g_CommandAllocators[frameIndex]->Reset();
        state.CommandList->Reset(g_CommandAllocators[frameIndex].Get(), nullptr);

        UxImGui::SetCurrentContext(g_UxImGuiContext);

        static bool showMenu = false;
        static bool keyWasPressed = false;
        static LARGE_INTEGER lastTime12 = {};
        static LARGE_INTEGER freq12 = {};
        static bool animationsInitialized = false;

        if (!animationsInitialized)
        {
            MenuAnimations::Init();
            animationsInitialized = true;
        }

        UxImGui_ImplDX12_NewFrame();

        UpdateImGuiIO(state.TargetWindow, showMenu, lastTime12, freq12);

        UxImGui::NewFrame();

        MenuAnimations::Update(UxImGui::GetIO().DeltaTime);

        SettingsMenu::RenderMonitoringOverlay();

        HandleMenuToggle(showMenu, keyWasPressed, state.TargetWindow);
        // HandleHotkeys intentionally moved to RenderOverlay so it runs even
        // when ctx.isUiEnabled == false.

        if (showMenu || MenuAnimations::IsSidePanelAnimating())
        {
            if (UxImGui::GetCurrentContext() != g_UxImGuiContext)
            {
                LOG_WARNING(L"[UxHook] D3D12: Context stolen BEFORE SettingsMenu::Render!");
                UxImGui::SetCurrentContext(g_UxImGuiContext);
            }

            SettingsMenu::Render(&showMenu);
        }

        if (UxImGui::GetCurrentContext() != g_UxImGuiContext)
        {
            LOG_WARNING(L"[UxHook] D3D12: Context stolen BEFORE UxImGui::Render!");
            UxImGui::SetCurrentContext(g_UxImGuiContext);
        }

        UxImGui::Render();

        if (UxImGui::GetCurrentContext() != g_UxImGuiContext)
        {
            LOG_WARNING(L"[UxHook] D3D12: Context stolen AFTER UxImGui::Render!");
            UxImGui::SetCurrentContext(g_UxImGuiContext);
        }

        UxImDrawData* drawData = UxImGui::GetDrawData();

        if (drawData && drawData->TotalVtxCount > 0 && state.CommandList)
        {
            if (!state.RenderTargets[backBufferIndex] || !state.SrvHeap)
            {
                LOG_WARNING(L"[UxHook] D3D12: RenderTarget or SrvHeap became invalid!");
            }
            else
            {
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = state.RenderTargets[backBufferIndex].Get();
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                state.CommandList->ResourceBarrier(1, &barrier);

                state.CommandList->OMSetRenderTargets(1, &state.RtvHandles[backBufferIndex], FALSE, nullptr);

                ID3D12DescriptorHeap* heaps[] = { state.SrvHeap.Get() };
                state.CommandList->SetDescriptorHeaps(1, heaps);

                UxImGui_ImplDX12_RenderDrawData(drawData, state.CommandList.Get());

                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
                state.CommandList->ResourceBarrier(1, &barrier);
            }
        }

        state.CommandList->Close();

        ID3D12CommandList* cmdLists[] = { state.CommandList.Get() };
        state.GameCommandQueue->ExecuteCommandLists(1, cmdLists);

        state.FenceValue++;
        state.GameCommandQueue->Signal(state.Fence.Get(), state.FenceValue);
    }

    // =========================================================================
    // PUBLIC API
    // =========================================================================
    void SetSwapChainCommandQueue(ID3D12CommandQueue* pQueue)
    {
        std::lock_guard<std::recursive_mutex> lock(g_Mutex);
        g_SwapChainCommandQueue = pQueue;
        if (pQueue)
            LOG_DEBUG(L"[UxHook] SetSwapChainCommandQueue: Queue stored");
    }

    void OnSwapChainAboutToBeCreated(HWND hwnd)
    {
        std::lock_guard<std::recursive_mutex> lock(g_Mutex);

        if (!g_Initialized)
            return;

        if (g_State.TargetWindow && hwnd && g_State.TargetWindow != hwnd)
            return;

        LOG_DEBUG(L"[UxHook] OnSwapChainAboutToBeCreated: Releasing overlay refs");

        // Unhook WndProc if hooked (menu might be open during swapchain recreation)
        UnhookWndProc();

        if (g_State.API == GraphicsAPI::D3D12)
            WaitForGpu_D3D12();

        if (g_UxImGuiContext)
        {
            UxImGuiContext* existingContext = UxImGui::GetCurrentContext();
            UxImGui::SetCurrentContext(g_UxImGuiContext);

            if (g_State.API == GraphicsAPI::D3D11)
                UxImGui_ImplDX11_InvalidateDeviceObjects();
            else if (g_State.API == GraphicsAPI::D3D12)
                UxImGui_ImplDX12_InvalidateDeviceObjects();

            if (existingContext && existingContext != g_UxImGuiContext)
                UxImGui::SetCurrentContext(existingContext);
        }

        if (g_State.API == GraphicsAPI::D3D11)
            ReleaseRenderTarget_D3D11();
        else if (g_State.API == GraphicsAPI::D3D12)
            ReleaseRenderTargets_D3D12();

        g_State.SwapChain.Reset();
        g_State.Initialized = false;
        g_Initialized = false;
        g_InitializedSwapChain = nullptr;

        LOG_DEBUG(L"[UxHook] OnSwapChainAboutToBeCreated: Done");
    }

    void RenderOverlay(IDXGISwapChain* pSwapChain)
    {
        // ---------------------------------------------------------------------
        // Global hotkeys run BEFORE any UI / overlay gating.
        //
        // Rationale: Ctrl+1..Ctrl+6 toggles MFG mode and must work even when
        // the in-game overlay is disabled (ctx.isUiEnabled=false). These
        // hotkeys are pure state flips -- they poll GetAsyncKeyState and don't
        // touch ImGui, so they're safe to call before initialization.
        //
        // Ctrl+~ (monitoring bar toggle) is internally gated by isUiEnabled
        // inside HandleHotkeys, since it has no effect without UI.
        //
        // TargetWindow is normally set during D3D11/D3D12 ImGui init; when UI
        // is disabled that init never runs, so we fall back to the swapchain's
        // own HWND for the foreground-window check.
        // ---------------------------------------------------------------------
        {
            HWND hotkeyHwnd = g_State.TargetWindow;
            if (!hotkeyHwnd && pSwapChain)
            {
                DXGI_SWAP_CHAIN_DESC desc = {};
                if (SUCCEEDED(pSwapChain->GetDesc(&desc)))
                    hotkeyHwnd = desc.OutputWindow;
            }
            if (hotkeyHwnd)
                HandleHotkeys(hotkeyHwnd);
        }

        if (!ctx.isUiEnabled) {
            return;
        }
        std::lock_guard<std::recursive_mutex> lock(g_Mutex);

        if (g_OverlayDisabled.load())
            return;

        static bool firstCall = true;
        if (firstCall)
        {
            LOG_TRACE(L"[UxHook] RenderOverlay: First call!");
            firstCall = false;
        }

        // Detect SwapChain change
        if (g_Initialized && g_InitializedSwapChain && pSwapChain != g_InitializedSwapChain)
        {
            LOG_WARNING(L"[UxHook] RenderOverlay: SwapChain CHANGED!");

            std::wstringstream ss;
            ss << L"[UxHook] Old SwapChain=" << (void*)g_InitializedSwapChain
                << L", New SwapChain=" << (void*)pSwapChain;
            LOG_WARNING(ss.str());

            // Unhook WndProc before cleanup
            UnhookWndProc();

            if (g_State.API == GraphicsAPI::D3D12)
                WaitForGpu_D3D12();

            if (g_UxImGuiContext)
            {
                UxImGui::SetCurrentContext(g_UxImGuiContext);

                if (g_State.API == GraphicsAPI::D3D11)
                    UxImGui_ImplDX11_InvalidateDeviceObjects();
                else if (g_State.API == GraphicsAPI::D3D12)
                    UxImGui_ImplDX12_InvalidateDeviceObjects();
            }

            if (g_State.API == GraphicsAPI::D3D11)
                ReleaseRenderTarget_D3D11();
            else if (g_State.API == GraphicsAPI::D3D12)
                ReleaseRenderTargets_D3D12();

            g_State.SwapChain.Reset();
            g_State.Initialized = false;
            g_Initialized = false;
            g_InitializedSwapChain = nullptr;

            LOG_WARNING(L"[UxHook] SwapChain change cleanup complete, will reinitialize");
        }

        auto& state = g_State;

        // Initialize if needed
        if (!state.Initialized)
        {
            LOG_INFO(L"[UxHook] RenderOverlay: Need to initialize...");

            GraphicsAPI api = DetectGraphicsAPI(pSwapChain);

            std::wstringstream ss;
            ss << L"[UxHook] Detected API: " << (api == GraphicsAPI::D3D11 ? L"D3D11" :
                api == GraphicsAPI::D3D12 ? L"D3D12" : L"Unknown");
            LOG_INFO(ss.str());

            bool success = false;
            if (api == GraphicsAPI::D3D11)
                success = InitializeImGui_D3D11(pSwapChain);
            else if (api == GraphicsAPI::D3D12)
                success = InitializeImGui_D3D12(pSwapChain);

            if (!success)
            {
                LOG_ERROR(L"[UxHook] RenderOverlay: Initialization FAILED!");
                return;
            }

            LOG_INFO(L"[UxHook] RenderOverlay: Initialization SUCCESS!");
        }

        if (!g_UxImGuiContext)
            return;

        static int logCounter = 0;
        if (logCounter++ % 300 == 0)
        {
            std::wstringstream ss;
            ss << L"[UxHook] RenderOverlay [TID:" << GetCurrentThreadId() << L"]: g_UxImGuiContext="
                << (void*)g_UxImGuiContext << L", Initialized=" << g_Initialized;
            LOG_TRACE(ss.str());
        }

        UxImGui::SetCurrentContext(g_UxImGuiContext);

        if (state.API == GraphicsAPI::D3D11)
            RenderFrame_D3D11();
        else if (state.API == GraphicsAPI::D3D12)
            RenderFrame_D3D12();
    }

    void OnResizeBuffers()
    {
        std::lock_guard<std::recursive_mutex> lock(g_Mutex);

        LOG_TRACE(L"[UxHook] OnResizeBuffers");

        if (g_State.API == GraphicsAPI::D3D12)
            WaitForGpu_D3D12();

        if (g_State.API == GraphicsAPI::D3D11)
            ReleaseRenderTarget_D3D11();
        else if (g_State.API == GraphicsAPI::D3D12)
            ReleaseRenderTargets_D3D12();
    }

    void OnResizeBuffersComplete(IDXGISwapChain* pSwapChain)
    {
        std::lock_guard<std::recursive_mutex> lock(g_Mutex);

        LOG_TRACE(L"[UxHook] OnResizeBuffersComplete");

        if (g_State.Initialized && pSwapChain)
        {
            pSwapChain->QueryInterface(IID_PPV_ARGS(&g_State.SwapChain));

            if (g_State.API == GraphicsAPI::D3D11)
                CreateRenderTarget_D3D11();
            else if (g_State.API == GraphicsAPI::D3D12)
                CreateRenderTargets_D3D12();
        }
    }

    void DisableOverlay()
    {
        g_OverlayDisabled = true;
        LOG_INFO(L"[UxHook] Overlay DISABLED");
    }

    bool IsOverlayDisabled()
    {
        return g_OverlayDisabled.load();
    }
}

// =============================================================================
// WndProc Hook - Block mouse input to game when our menu is open
// Only active while menu is open (hooked/unhooked dynamically)
// =============================================================================

namespace { bool g_OurMenuIsOpen = false; }

void UxHook_SetMenuOpen(bool open) { g_OurMenuIsOpen = open; }
bool UxHook_IsMenuOpen() { return g_OurMenuIsOpen; }

static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Safety guard against infinite recursion if WndProc chain forms a cycle
    // (e.g. OptiScaler re-hooks while our menu is open)
    // Flag stays TRUE during CallWindowProcW so re-entry is detected
    static thread_local bool inHookedWndProc = false;

    if (inHookedWndProc)
        return DefWindowProcW(hWnd, msg, wParam, lParam);

    inHookedWndProc = true;

    if (!g_State.OriginalWndProc)
    {
        inHookedWndProc = false;
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    // Always forward shutdown messages immediately
    if (msg == WM_QUIT || msg == WM_CLOSE || msg == WM_DESTROY ||
        (msg == WM_SYSCOMMAND && wParam == SC_CLOSE))
    {
        LRESULT result = CallWindowProcW(g_State.OriginalWndProc, hWnd, msg, wParam, lParam);
        inHookedWndProc = false;
        return result;
    }

    // ONLY intercept input when OUR menu is open
    if (g_OurMenuIsOpen && g_UxImGuiContext && g_State.Initialized && !g_OverlayDisabled.load())
    {
        static int debugCounter = 0;
        if (debugCounter++ % 600 == 0)
        {
            LOG_TRACE(L"[UxHook] WndProc: Menu OPEN, processing input");
        }

        UxImGuiContext* prev = UxImGui::GetCurrentContext();
        UxImGui::SetCurrentContext(g_UxImGuiContext);

        UxImGuiIO& io = UxImGui::GetIO();

        io.MouseDrawCursor = true;
        io.WantCaptureKeyboard = true;
        io.WantCaptureMouse = true;

        SetCursor(NULL);

        bool handled = false;
        switch (msg)
        {
        case WM_SETCURSOR:
            SetCursor(NULL);
            if (prev && prev != g_UxImGuiContext) UxImGui::SetCurrentContext(prev);
            inHookedWndProc = false;
            return TRUE;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            if (wParam != MENU_TOGGLE_KEY)
                handled = true;
            break;
        case WM_CHAR:
            if (wParam > 0 && wParam < 0x10000)
                io.AddInputCharacterUTF16((unsigned short)wParam);
            handled = true;
            break;
        case WM_MOUSEWHEEL:
            io.MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
            handled = true;
            break;
        case WM_MOUSEHWHEEL:
            io.MouseWheelH += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
            handled = true;
            break;
        }

        if (handled)
        {
            if (prev && prev != g_UxImGuiContext) UxImGui::SetCurrentContext(prev);
            inHookedWndProc = false;
            return TRUE;
        }

        switch (msg)
        {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
            if (prev && prev != g_UxImGuiContext) UxImGui::SetCurrentContext(prev);
            inHookedWndProc = false;
            return TRUE;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (prev && prev != g_UxImGuiContext) UxImGui::SetCurrentContext(prev);
            inHookedWndProc = false;
            return TRUE;

        case WM_MOUSEMOVE:
            if (prev && prev != g_UxImGuiContext) UxImGui::SetCurrentContext(prev);
            inHookedWndProc = false;
            return TRUE;

        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
            if (prev && prev != g_UxImGuiContext) UxImGui::SetCurrentContext(prev);
            inHookedWndProc = false;
            return TRUE;

        case WM_INPUT:
            if (prev && prev != g_UxImGuiContext) UxImGui::SetCurrentContext(prev);
            inHookedWndProc = false;
            return TRUE;

        default:
            break;
        }

        if (prev && prev != g_UxImGuiContext) UxImGui::SetCurrentContext(prev);
    }

    LRESULT result = CallWindowProcW(g_State.OriginalWndProc, hWnd, msg, wParam, lParam);
    inHookedWndProc = false;
    return result;
}

// =============================================================================
// Public API
// =============================================================================

bool UxInit()
{
    LOG_TRACE(L"[UxHook] UxInit() called - v12 Dynamic WndProc");
    return true;
}

void UxShutdown()
{
    std::lock_guard<std::recursive_mutex> lock(g_Mutex);

    LOG_TRACE(L"[UxHook] UxShutdown()");

    if (g_State.Initialized)
    {
        if (g_State.API == UxHook::GraphicsAPI::D3D12)
            UxHook::WaitForGpu_D3D12();

        // Unhook WndProc if still hooked (menu was open during shutdown)
        UnhookWndProc();

        // Uninstall cursor hooks
        CursorHook::Uninstall();

        if (g_UxImGuiContext)
        {
            UxImGui::SetCurrentContext(g_UxImGuiContext);

            if (g_State.API == UxHook::GraphicsAPI::D3D11)
                UxImGui_ImplDX11_Shutdown();
            else if (g_State.API == UxHook::GraphicsAPI::D3D12)
                UxImGui_ImplDX12_Shutdown();

            UxImGui::DestroyContext(g_UxImGuiContext);
            g_UxImGuiContext = nullptr;
        }

        if (g_State.API == UxHook::GraphicsAPI::D3D12)
        {
            g_SrvDescriptorAllocator.Destroy();

            if (g_State.FenceEvent)
                CloseHandle(g_State.FenceEvent);

            for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
                g_CommandAllocators[i].Reset();
        }

        g_State = UxHook::RenderState();
    }

    g_SwapChainCommandQueue = nullptr;
    g_InitializedSwapChain = nullptr;
    g_OverlayDisabled = false;
    g_Initialized = false;
    g_FrameCount = 0;

    LOG_INFO(L"[UxHook] Shutdown complete");
}