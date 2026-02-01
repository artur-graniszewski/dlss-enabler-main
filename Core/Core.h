#pragma once
#include <wtypes.h>

class Core {
public:
	static void Initialize(HINSTANCE hModule);
	static void Finish(HINSTANCE hModule);
private:
	static void ShowHelp();
	static void ShowVersion();
	static void ShowDiagnostics();
};

/*
TODO:

NVAPI
- Check why FakeNVAPI is not detected by Optiscaler
- Check why FakeNVAPI hangs on XeLL
- check which NVAPI functions are incomplete in Fake NVAPI
- check which NVAPI org pointers are not used as variables
*/