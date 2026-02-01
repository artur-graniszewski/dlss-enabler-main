#pragma once
#include "Hook.h"
#include "DetourTxn.h"
#include "DxgiProxy.h"
#include "Common.h"


namespace DXGI
{
    CreateDXGIFactory_t orgCreateDXGIFactory;
    CreateDXGIFactory1_t orgCreateDXGIFactory1;
    CreateDXGIFactory2_t orgCreateDXGIFactory2;

    struct HookDxgi : IHook {
        const std::wstring Name() const override { return L"DXGI hooks"; }
        HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
        int         Priority() const override { return 50; }

        bool CanInstall(Context& ctx, IDetourApi& api) override {
            static HMODULE lastDxgi = nullptr;
            static bool isDxgiChecked = false;
            static bool isInstallable = false;
            auto dxgi = api.GetModHandleW(L"dxgi.dll");

            if (lastDxgi != dxgi && lastDxgi != nullptr) {
                // either the first installation or a new instance of dxgi.dll
                isDxgiChecked = false;
                isInstallable = false;
                ctx.emulation.isDxgiSpoofed = false;
            }

            if (ctx.emulation.isDxgiSpoofed) {
                return false;
            }

            if (!isDxgiChecked) {
                if (!dxgi) {
                    return false;
                }
                isDxgiChecked = true;
                if (api.GetProc(dxgi, "DlssEnablerInit") && false) {
                    isInstallable = false;
                    ctx.emulation.isDxgiSpoofed = true;
                    //ctx.isLoadedByDxgi = true;
                    //LOG_INFO(L"[DXGI] GPU spoofing provided by external dxgi.dll file");
                }
                else {
                    isInstallable = true;
                }
            }
            return isInstallable;
        }

        bool Install(Context&, IDetourApi& api)
        {
            static HMODULE lastDxgi;
            HMODULE dxgi = api.GetModHandleW(L"dxgi.dll");
            if (!dxgi) {
                LOG_WARNING(L"[DXGI] Missing dxgi.dll file, no detours applied");
                return false;
            }

            if (lastDxgi == dxgi) {
                return false;
            }

            LOG_INFO(L"[DXGI] Hooking DXGI");
            lastDxgi = dxgi;

            FARPROC maybeInit = api.GetProc(dxgi, "DlssEnablerInit");
            if (maybeInit && false) {
                //ctx.isLoadedByDxgi = true;
                LOG_INFO(L"[DXGI] GPU spoofing provided by external dxgi.dll file (DlssEnablerInit present)");
                //return true;
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

            ctx.emulation.isDxgiSpoofed = true;
            InitDxgi(dxgi);
            return true;
        }
    };
}