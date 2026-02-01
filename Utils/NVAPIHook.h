#pragma once
#include "Hook.h"
#include "Common.h"
#include "DetourTxn.h"
#include "Nvapi64Dispatch.h"
#include "ProcAliasRegistry.h"

namespace NVAPI
{
    nvapi_QueryInterface_t OriginalNvAPI_QueryInterface;

    struct HookNVAPI : IHook {
        const std::wstring Name() const override { return L"NVAPI hooks"; }
        HookPhase   Phase() const override { return HookPhase::ON_DEMAND; }
        int         Priority() const override { return 1; }

        bool CanInstall(Context& ctx, IDetourApi& api) override {
            return api.GetModHandleW(L"nvapi64.dll") || ctx.nvapi.isEmbeddedNvapiUsed;
        }

        bool Install(Context& ctx, IDetourApi& api) override {
            if (OriginalNvAPI_QueryInterface) {
                return false;
            }

            DetourTxn txn(api);

            auto realNvapi = api.GetModHandleW(L"nvapi64.dll");
            auto nvapi = Common::GetModuleHandle();

            if (realNvapi) {
                OriginalNvAPI_QueryInterface = (nvapi_QueryInterface_t)api.GetProc(realNvapi, "nvapi_QueryInterface");
                LOG_INFO(L"[NVAPI] Real NVAPI Interface detected");
            }
            if (OriginalNvAPI_QueryInterface) {
                //if (!txn.attach((void**)&OriginalNvAPI_QueryInterface, (void*)&NvAPI_QueryInterface, "nvapi_QueryInterface")) return false;
            }

            static bool isHooked = false;
            if (!isHooked) {
                auto result = txn.commit();
                using FuncPtr = ProcAliasRegistry::FuncPtr;
                ProcAliasRegistry::Instance().RegisterAlias(
                    nvapi,
                    "nvapi_QueryInterface",
                    reinterpret_cast<FuncPtr>(NvAPI_QueryInterface));

                if (result) {
                    LOG_INFO(L"[NVAPI] Hooks applied successfully!");
                }
                else {
                    LOG_ERROR(L"[NVAPI] Failed to activate hooks");
                }
                isHooked = true;
            }
            return true;
        }
    };
}