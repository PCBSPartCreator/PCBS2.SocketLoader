#include <windows.h>

#include "Addon.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(hModule);
    return TRUE;
}

extern "C" __declspec(dllexport) const char* XPL_GetName() {
    return SocketLoader::Addon::Name();
}

extern "C" __declspec(dllexport) const char* XPL_GetVersion() {
    return SocketLoader::Addon::Version();
}

extern "C" __declspec(dllexport) bool XPL_Initialize(const char* gameDir) {
    return SocketLoader::Addon::Get().Initialize(gameDir);
}

extern "C" __declspec(dllexport) void XPL_Tick() {
    SocketLoader::Addon::Get().Tick();
}

extern "C" __declspec(dllexport) void XPL_Shutdown() {
    SocketLoader::Addon::Get().Shutdown();
}