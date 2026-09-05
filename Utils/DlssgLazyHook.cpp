#include "NgxProvider.h"
#include "DlssgProxy.h"
#include "DlssgRouter.h"
#include "RoutePolicy.h"
#include "../Core/Context.h"

extern std::unique_ptr<DLSSG::DlssgProxy>  dlssgModule;
extern std::unique_ptr<NGX::NgxProvider>   ngxProvider;
std::unique_ptr<DLSSG::DlssgRouter>        dlssgRouter;
extern std::unique_ptr<BackendManager>     ngxBackends;

NgxRuntimeState ngxRuntimeState;
DefaultLogger globalLogger;
DefaultBackendLoader globalNgxLoader;

void InitializeDlssgHooks()
{
    static bool done = false;
    if (done) {
        return;
    }
    done = true;

    if (!ngxBackends) {
        ngxBackends = std::make_unique<BackendManager>(globalNgxLoader, globalLogger);
    }
    if (ctx.ngx.isEmbeddedDlssgUsed) {
        SetProcResolverPrefix("DLSSG_");
    }
    IProcResolver& resolver = GetPrefixedProcResolver();

    // Capability gates.
    const bool hwHasDlssg = ctx.ngx.isDlssgSupportedByHardware;
    const bool userDisabled = ctx.ngx.isDlssgDisabled;
    // NOTE: add `bool isHybridMfgEnabled` to ctx.ngx (Context.h) and load it
    //       from your INI like the other DLSSG flags.
    const bool hybridRequested = ctx.ngx.isHybridMfgEnabled;

    // Hybrid mode requires THREE things:
    //   1. User asked for it.
    //   2. DLSSG hardware is actually present (otherwise there's nothing to fan out to).
    //   3. Embedded DLSSG is in use, so DlssgProxy resolves the FSR3 backend through
    //      the DLSSG_-prefixed resolver while NgxFrontend resolves the native DLSSG
    //      through the unprefixed resolver. Without the prefix split, both backends
    //      would resolve to the same DLL and "fanout" would just call native twice.
    const bool useHybrid = hybridRequested
        && hwHasDlssg
        && !userDisabled
        && ctx.ngx.isEmbeddedDlssgUsed;

    // FSR3 proxy is needed in three cases:
    //   1. No DLSSG hardware -> proxy is the only frame-gen path (legacy mode).
    //   2. User disabled DLSSG -> proxy substitutes for it (legacy mode).
    //   3. Hybrid mode -> proxy is the FSR3 brick that fills non-midpoint subframes.
    const bool needFsr3Proxy = useHybrid || !hwHasDlssg || userDisabled;

    if (!dlssgModule && needFsr3Proxy) {
        dlssgModule = std::make_unique<DLSSG::DlssgProxy>(
            *ngxBackends, globalLogger, ngxRuntimeState, resolver);
    }

    if (!ngxProvider) {
        ngxProvider = std::make_unique<NGX::NgxProvider>(
            globalLogger, ngxRuntimeState, resolver);
    }

    // Router is constructed only in hybrid mode. NgxFrontend checks
    // `dlssgRouter && dlssgRouter->IsActive()` before delegating, so leaving
    // it null is the safe legacy path that takes the existing DLSSG REDIRECT
    // branches unchanged.
    if (!dlssgRouter && useHybrid) {
        auto policy = std::make_unique<DLSSG::MidpointDlssgPolicy>();
        dlssgRouter = std::make_unique<DLSSG::DlssgRouter>(
            globalLogger, std::move(policy));
    }
}