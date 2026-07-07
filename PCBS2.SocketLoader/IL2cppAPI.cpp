#include "Il2CppApi.h"
#include "Log.h"

#include <string>

namespace SocketLoader {

    namespace {

        template <typename T>
        void ResolveOne(HMODULE mod, const char* name, T& fn, Logger& log, int& missing, bool critical) {
            fn = reinterpret_cast<T>(GetProcAddress(mod, name));
            if (fn)
                return;
            if (critical) {
                log.Error(std::string("IL2CPP export missing (critical): ") + name);
                ++missing;
            }
            else {
                log.Warn(std::string("IL2CPP export missing (optional): ") + name);
            }
        }

    }

    bool Il2CppApi::Resolve(HMODULE mod, Logger& log) {
        int missing = 0;

        ResolveOne(mod, "il2cpp_domain_get", domain_get, log, missing, true);
        ResolveOne(mod, "il2cpp_domain_get_assemblies", domain_get_assemblies, log, missing, true);
        ResolveOne(mod, "il2cpp_assembly_get_image", assembly_get_image, log, missing, true);
        ResolveOne(mod, "il2cpp_image_get_name", image_get_name, log, missing, true);
        ResolveOne(mod, "il2cpp_class_from_name", class_from_name, log, missing, true);
        ResolveOne(mod, "il2cpp_class_get_field_from_name", class_get_field_from_name, log, missing, true);
        ResolveOne(mod, "il2cpp_field_get_offset", field_get_offset, log, missing, true);
        ResolveOne(mod, "il2cpp_field_static_get_value", field_static_get_value, log, missing, true);
        ResolveOne(mod, "il2cpp_field_static_set_value", field_static_set_value, log, missing, true);
        ResolveOne(mod, "il2cpp_class_get_method_from_name", class_get_method_from_name, log, missing, true);
        ResolveOne(mod, "il2cpp_array_new", array_new, log, missing, true);
        ResolveOne(mod, "il2cpp_array_length", array_length, log, missing, true);
        ResolveOne(mod, "il2cpp_string_new", string_new, log, missing, true);
        ResolveOne(mod, "il2cpp_thread_attach", thread_attach, log, missing, true);
        ResolveOne(mod, "il2cpp_thread_detach", thread_detach, log, missing, false);
        ResolveOne(mod, "il2cpp_class_get_element_class", class_get_element_class, log, missing, false);

        if (missing == 0) {
            log.Info("IL2CPP API resolved (all critical exports found)");
            return true;
        }
        log.Error("IL2CPP API incomplete: " + std::to_string(missing) + " critical export(s) missing");
        return false;
    }

}