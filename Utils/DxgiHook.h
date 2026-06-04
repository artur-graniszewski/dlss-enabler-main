#pragma once
#include "Hook.h"
#include "DetourTxn.h"
#include "DxgiProxy.h"
#include "SwapchainProxy.h"
#include "Wrapped_Swapchain.h"
#include "Common.h"


namespace DXGI
{
    CreateDXGIFactory_t  orgCreateDXGIFactory;
    CreateDXGIFactory1_t orgCreateDXGIFactory1;
    CreateDXGIFactory2_t orgCreateDXGIFactory2;

    // Handle of the dxgi.dll instance we currently have hooks installed against.
    // Promoted from a function-local static so that Uninstall and CanInstall can both see it.
    HMODULE lastInstalledDxgi = nullptr;

    struct HookDxgi : IHook {
        const std::wstring Name() const override { return L"DXGI hooks"; }
        HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
        int         Priority() const override { return 50; }

        bool CanInstall(Context& ctx, IDetourApi& api) override {
            HMODULE dxgi = api.GetModHandleW(L"dxgi.dll");

            // dxgi.dll has been unloaded. Any previously installed detours died together
            // with the module's memory pages, so clear the "spoofed" flag and let a
            // future load trigger a fresh install.
            // NOTE: we deliberately do NOT touch orgCreateDXGIFactory* here - those
            // pointers may still be read concurrently by detour handlers on other
            // threads. They will be overwritten by the next successful Install().
            if (!dxgi) {
                if (ctx.emulation.isDxgiSpoofed) {
                    ctx.emulation.isDxgiSpoofed = false;
                    lastInstalledDxgi = nullptr;
                }
                return false;
            }

            // Already hooked against the current dxgi.dll - nothing to do.
            if (ctx.emulation.isDxgiSpoofed) {
                return false;
            }

            // dxgi.dll was swapped underneath us (old module unloaded, a different
            // instance loaded). Old hooks are already dead along with the old module;
            // we cannot detach them cleanly. Just forget the old handle so that the
            // subsequent Install proceeds against the new module.
            // Again: org* pointers are left alone - Install will overwrite them.
            if (lastInstalledDxgi && lastInstalledDxgi != dxgi) {
                lastInstalledDxgi = nullptr;
            }

            // External dxgi.dll advertising its own GPU spoofing via DlssEnablerInit.
            // Feature is currently frozen (see "&& false"); kept structurally so it
            // can be re-enabled by removing the "&& false".
            if (api.GetProc(dxgi, "DlssEnablerInit") && false) {
                return false;
            }

            return true;
        }

        bool Install(Context&, IDetourApi& api) override
        {
            HMODULE dxgi = api.GetModHandleW(L"dxgi.dll");
            if (!dxgi) {
                LOG_WARNING(L"[DXGI] Missing dxgi.dll file, no detours applied");
                return false;
            }

            // Guard against re-entry with the same module handle.
            if (lastInstalledDxgi == dxgi) {
                return false;
            }

            LOG_INFO(L"[DXGI] Hooking DXGI");

            FARPROC maybeInit = api.GetProc(dxgi, "DlssEnablerInit");
            if (maybeInit && false) {
                // ctx.isLoadedByDxgi = true;
                LOG_INFO(L"[DXGI] GPU spoofing provided by external dxgi.dll file (DlssEnablerInit present)");
                // return true;
            }

            auto pCreateDXGIFactory = reinterpret_cast<CreateDXGIFactory_t>(api.GetProc(dxgi, "CreateDXGIFactory"));
            auto pCreateDXGIFactory1 = reinterpret_cast<CreateDXGIFactory1_t>(api.GetProc(dxgi, "CreateDXGIFactory1"));
            auto pCreateDXGIFactory2 = reinterpret_cast<CreateDXGIFactory2_t>(api.GetProc(dxgi, "CreateDXGIFactory2"));

            if (!pCreateDXGIFactory && !pCreateDXGIFactory1 && !pCreateDXGIFactory2) {
                LOG_ERROR(L"[DXGI] Failed to find CreateDXGIFactory* in dxgi.dll");
                return false;
            }

            DetourTxn txn(api);
            bool ok = true;

            if (pCreateDXGIFactory) {
                orgCreateDXGIFactory = pCreateDXGIFactory;
                ok &= txn.attach(reinterpret_cast<void**>(&orgCreateDXGIFactory), reinterpret_cast<void*>(DXGI::CreateDXGIFactory), "CreateDXGIFactory");
            }

            if (pCreateDXGIFactory1) {
                orgCreateDXGIFactory1 = pCreateDXGIFactory1;
                ok &= txn.attach(reinterpret_cast<void**>(&orgCreateDXGIFactory1), reinterpret_cast<void*>(DXGI::CreateDXGIFactory1), "CreateDXGIFactory1");
            }

            if (pCreateDXGIFactory2) {
                orgCreateDXGIFactory2 = pCreateDXGIFactory2;
                ok &= txn.attach(reinterpret_cast<void**>(&orgCreateDXGIFactory2), reinterpret_cast<void*>(DXGI::CreateDXGIFactory2), "CreateDXGIFactory2");
            }

            if (!ok) {
                return false;
            }

            if (!txn.commit()) {
                return false;
            }

            LOG_INFO(L"[DXGI] Built-in GPU spoofing enabled");

            lastInstalledDxgi = dxgi;
            ctx.emulation.isDxgiSpoofed = true;

            // Raise the subsystem flags BEFORE InitDxgi: the lazy attach points
            // (AttachToFactory / AttachToAdapter / EnsureSwapChainDetours / DetourSwapChain1)
            // may fire as soon as the game starts using the hooked CreateDXGIFactory*.
            EnableDxgiHooks();
            EnableSwapchainHooks();

            InitDxgi(dxgi);
            return true;
        }

        void Uninstall(Context&, IDetourApi& api) override
        {
            // Nothing was installed.
            if (!lastInstalledDxgi) {
                return;
            }

            LOG_INFO(L"[DXGI] Unhooking DXGI");

            // Step 0: neutralize trig lambdas on every live wrapper the game
            // may still be holding. Lambdas can capture pointers into our DLL
            // (FFX state, Streamline context, ImGui, etc.); once we start
            // tearing those subsystems down, a late Present on a still-held
            // wrapper would invoke a stale capture and crash. After this
            // returns, Present / Present1 / ResizeBuffers / ResizeBuffers1
            // on every existing wrapper will skip the trig dispatch entirely
            // and just forward to the underlying swapchain.
            DetachAllSwapchainTriggers();

            // Step 1: stop the lazy attach points from installing anything new.
            // From now on AttachToFactory / AttachToAdapter / EnsureSwapChainDetours /
            // DetourSwapChain1 / DetourPresent / DetourInitThread are no-ops.
            // In-flight calls that already entered their critical sections will
            // re-check the flag after acquiring their mutex and bail out.
            DisableSwapchainHooks();
            DisableDxgiHooks();

            // Step 2: detach vtable-level hooks installed lazily by the subsystems.
            // Order: swapchain first (it depends on factory having been hooked),
            // then adapter/factory. Each function takes its own mutexes to
            // serialize with any Attach* still completing.
            TeardownSwapchainProxy();
            TeardownDxgi();

            // Step 3: detach the export-level hooks (CreateDXGIFactory*), reverse
            // order of Install.
            DetourTxn txn(api);
            bool ok = true;
            if (orgCreateDXGIFactory2) {
                ok &= txn.detach(reinterpret_cast<void**>(&orgCreateDXGIFactory2), reinterpret_cast<void*>(DXGI::CreateDXGIFactory2), "CreateDXGIFactory2");
            }
            if (orgCreateDXGIFactory1) {
                ok &= txn.detach(reinterpret_cast<void**>(&orgCreateDXGIFactory1), reinterpret_cast<void*>(DXGI::CreateDXGIFactory1), "CreateDXGIFactory1");
            }
            if (orgCreateDXGIFactory) {
                ok &= txn.detach(reinterpret_cast<void**>(&orgCreateDXGIFactory), reinterpret_cast<void*>(DXGI::CreateDXGIFactory), "CreateDXGIFactory");
            }

            if (!ok || !txn.commit()) {
                LOG_ERROR(L"[DXGI] Failed to detach DXGI export hooks");
                // Don't return - still need to clear state so a retry can proceed.
            }

            // Step 4: clear export-level trampoline pointers and state.
            // Safe to null now: export hooks are detached, no in-flight handler
            // can read them anymore (the detour entry points in dxgi.dll have
            // been restored to the original bytes).
            //
            // Note: vtable-level trampolines (orgGetDesc*, orgEnumAdapters*,
            // originalPresent*, orgCreateSwapChain*) are intentionally left
            // non-null inside their owning translation units - see the comments
            // in TeardownDxgi() and TeardownSwapchainProxy().
            orgCreateDXGIFactory = nullptr;
            orgCreateDXGIFactory1 = nullptr;
            orgCreateDXGIFactory2 = nullptr;
            lastInstalledDxgi = nullptr;
            ctx.emulation.isDxgiSpoofed = false;

            LOG_INFO(L"[DXGI] Built-in GPU spoofing disabled");
            // TODO: if a ShutdownDxgi() counterpart to InitDxgi() exists, call it here.
        }
    };
}