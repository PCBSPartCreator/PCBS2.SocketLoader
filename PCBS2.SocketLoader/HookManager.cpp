#include "HookManager.h"
#include "Log.h"

#include <windows.h>
#include <string>

namespace SocketLoader {
    namespace Hook {

        namespace {
            using XPL_CreateHookFn = bool (*)(void* target, void* detour, void** original);
            XPL_CreateHookFn s_xplCreateHook = nullptr;
            bool s_ready = false;
        }

        bool Init(Logger& log) {
            if (s_ready)
                return true;

            HMODULE xpl = GetModuleHandleA("version.dll");
            if (!xpl) {
                log.Error("Hook: version.dll (PCBS2.XPL) not loaded");
                return false;
            }

            s_xplCreateHook = reinterpret_cast<XPL_CreateHookFn>(
                GetProcAddress(xpl, "XPL_CreateHook"));
            if (!s_xplCreateHook) {
                log.Error("Hook: XPL_CreateHook not exported by PCBS2.XPL");
                return false;
            }

            s_ready = true;
            log.Info("Hook: using PCBS2.XPL hook service");
            return true;
        }

        bool Create(void* target, void* detour, void** original, const char* name, Logger& log) {
            if (!s_ready || !s_xplCreateHook) {
                log.Error(std::string("Hook: Create called before Init for ") + name);
                return false;
            }
            if (!target) {
                log.Error(std::string("Hook: null target for ") + name);
                return false;
            }

            if (s_xplCreateHook(target, detour, original)) {   // creates and enables in one call
                log.Info(std::string("Hook: created + enabled hook for ") + name);
                return true;
            }
            log.Error(std::string("Hook: XPL_CreateHook failed for ") + name);
            return false;
        }

        bool Enable(void* /*target*/, const char* name, Logger& log) {
            return true;   // Create already enabled it
        }

        void Shutdown(Logger& /*log*/) {
            // MinHook belongs to PCBS2.XPL; uninitializing here would kill every other addon's hooks
        }

    }
}