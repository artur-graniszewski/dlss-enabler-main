#pragma once
#include <d3dkmthk.h>

using PFN_D3DKMTQueryAdapterInfo = NTSTATUS(WINAPI*)(D3DKMT_QUERYADAPTERINFO*);
using PFN_D3DKMTEnumAdapters2 = NTSTATUS(WINAPI*)(D3DKMT_ENUMADAPTERS2* pAdapterInfo);

extern PFN_D3DKMTQueryAdapterInfo gOrigQueryAdapterInfo;
extern PFN_D3DKMTEnumAdapters2    gOrigEnumAdapters2;

NTSTATUS WINAPI MyD3DKMTEnumAdapters2(D3DKMT_ENUMADAPTERS2* pAdapterInfo);
NTSTATUS WINAPI MyD3DKMTQueryAdapterInfo(D3DKMT_QUERYADAPTERINFO* pInfo);

