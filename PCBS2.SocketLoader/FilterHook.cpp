#include "FilterHook.h"
#include "HookManager.h"
#include "Log.h"

#include <string>
#include <vector>
#include <cstdint>
#include <climits>

namespace SocketLoader {

    namespace {

        const char* kFilterNamespace = "PCBS2.UI.Filters";
        const char* kFilterHolder = "FilterHolder";

        const uintptr_t kArrayHeader = 0x20;

        Logger* g_log = nullptr;
        Il2CppApi g_api;

        void* g_cpuClass = nullptr;
        void* g_mbClass = nullptr;
        int   g_cpuOffset = -1;
        int   g_mbOffset = -1;
        void* g_sNamesField = nullptr;
        void* g_stringClass = nullptr;
        bool  g_logged = false;

        struct CustomSocket { int id; std::string name; };
        std::vector<CustomSocket> g_sockets;

        typedef void* (*SocketFilter_t)(void* part, void* method);
        SocketFilter_t g_origSocketFilter = nullptr;

        const std::string* CustomNameForId(int id) {
            for (size_t i = 0; i < g_sockets.size(); ++i)
                if (g_sockets[i].id == id)
                    return &g_sockets[i].name;
            return nullptr;
        }

        // System.String class, taken from the already-patched s_names array
        void* StringClass() {
            if (g_stringClass)
                return g_stringClass;
            if (!g_sNamesField || !g_api.field_static_get_value || !g_api.class_get_element_class)
                return nullptr;
            void* arr = nullptr;
            g_api.field_static_get_value(g_sNamesField, &arr);
            if (!arr)
                return nullptr;
            g_stringClass = g_api.class_get_element_class(*(void**)arr);
            return g_stringClass;
        }

        int PartSocket(void* part) {
            void* cls = *(void**)part;
            if (cls == g_cpuClass && g_cpuOffset >= 0)
                return *(int*)((uintptr_t)part + (uintptr_t)g_cpuOffset);
            if (cls == g_mbClass && g_mbOffset >= 0)
                return *(int*)((uintptr_t)part + (uintptr_t)g_mbOffset);
            return INT_MIN;
        }

        void* DetourSocketFilter(void* part, void* method) {
            if (part) {
                int socket = PartSocket(part);
                const std::string* name = (socket != INT_MIN) ? CustomNameForId(socket) : nullptr;
                if (name) {
                    void* strClass = StringClass();
                    if (strClass && g_api.array_new && g_api.string_new) {
                        void* arr = g_api.array_new(strClass, 1);
                        if (arr) {
                            *(void**)((uintptr_t)arr + kArrayHeader) = g_api.string_new(name->c_str());
                            if (g_log && !g_logged) {
                                g_logged = true;
                                g_log->Info("FilterHook: SocketFilter returns \"" + *name +
                                    "\" for custom socket " + std::to_string(socket));
                            }
                            return arr;  // a String[] already is an IEnumerable<string>
                        }
                    }
                }
            }
            return g_origSocketFilter(part, method);
        }

    }

    bool FilterHook::Install(const Il2CppApi& api, const SocketTargetHandles& targets,
        const std::vector<SocketEntry>& sockets, Logger& log) {
        if (m_installed)
            return true;
        if (!targets.image || !api.class_from_name || !api.class_get_method_from_name) {
            log.Error("FilterHook: prerequisites missing, cannot install");
            return false;
        }

        void* klass = api.class_from_name(targets.image, kFilterNamespace, kFilterHolder);
        if (!klass) {
            log.Error(std::string("FilterHook: class not found: ") + kFilterNamespace + "." + kFilterHolder);
            return false;
        }
        void* mi = api.class_get_method_from_name(klass, "SocketFilter", 1);
        if (!mi) {
            log.Error("FilterHook: method not found: FilterHolder.SocketFilter");
            return false;
        }
        void* fn = *(void**)mi;  // native ptr at MethodInfo offset 0
        if (!fn) {
            log.Error("FilterHook: null native pointer for SocketFilter");
            return false;
        }

        g_api = api;
        g_log = &log;
        g_cpuClass = targets.cpuPartClass;
        g_mbClass = targets.mbPartClass;
        g_cpuOffset = targets.cpuSocketOffset;
        g_mbOffset = targets.mbSocketOffset;
        g_sNamesField = targets.sNamesField;
        g_sockets.clear();
        for (size_t i = 0; i < sockets.size(); ++i) {
            if (sockets[i].enabled && !sockets[i].displayName.empty()) {
                CustomSocket cs;
                cs.id = sockets[i].id;
                cs.name = sockets[i].displayName;
                g_sockets.push_back(cs);
            }
        }

        if (!Hook::Create(fn, reinterpret_cast<void*>(&DetourSocketFilter),
            reinterpret_cast<void**>(&g_origSocketFilter), "SocketFilter", log))
            return false;
        bool ok = Hook::Enable(fn, "SocketFilter", log);

        m_installed = true;
        log.Info(std::string("FilterHook: installed for ") + std::to_string(g_sockets.size()) +
            " custom socket(s) (" + (ok ? "ok" : "enable failed") + ")");
        return ok;
    }

}