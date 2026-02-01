#include "ProcResolver.h"
#include <mutex>

static DefaultProcResolver gResolver;
static std::once_flag gOnce;

IProcResolver& GetProcResolver() {
    return gResolver;
}

std::unique_ptr<PrefixedProcResolver> MakePrefixedResolver(const std::string& prefix, GetProcAddress_t proc) {
    auto r = std::make_unique<PrefixedProcResolver>(prefix, /*allowFallback=*/true);
    r->Install(proc);
    return r;
}

static PrefixedProcResolver gPrefixedResolver;

IProcResolver& GetPrefixedProcResolver() {
    return gPrefixedResolver;
}

void SetProcResolverPrefix(const char* prefix) {
    gPrefixedResolver.setPrefix(prefix ? std::string(prefix) : std::string());
}

void SetProcResolverAllowFallback(bool allow) {
    gPrefixedResolver.setAllowFallback(allow);
}

void InstallOriginalGetProcAddress(GetProcAddress_t proc) {
    gResolver.Install(proc);
    gPrefixedResolver.Install(proc);
}