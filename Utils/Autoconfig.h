#pragma once
#define CMD_EXISTS(cmd) (strstr(GetCommandLineA(), cmd) != nullptr)

class Autoconfig
{
public:
	static bool PreloadUpscaler(bool forceLoad = false);
	static HMODULE GetNGXLibrary();
	static HMODULE GetNVAPILibrary();
	static bool InitializeFrameGeneration();
	static bool Initialize();

private:
	static void CheckCommandLineParams();
	static void CheckIniFile();
}; 

