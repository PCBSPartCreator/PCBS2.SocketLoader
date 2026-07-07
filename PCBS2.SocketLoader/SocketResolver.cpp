#include "SocketResolver.h"
#include "Log.h"

#include <string>
#include <cstring>

namespace SocketLoader {

    namespace {

        void* FindImage(const Il2CppApi& api, const char* imageName, Logger& log) {
            void* domain = api.domain_get();
            if (!domain) {
                log.Error("Resolve: il2cpp_domain_get returned null");
                return nullptr;
            }

            size_t count = 0;
            void** assemblies = api.domain_get_assemblies(domain, &count);
            if (!assemblies) {
                log.Error("Resolve: no assemblies returned");
                return nullptr;
            }

            for (size_t i = 0; i < count; ++i) {
                if (!assemblies[i])
                    continue;
                void* img = api.assembly_get_image(assemblies[i]);
                if (!img)
                    continue;
                const char* nm = api.image_get_name(img);
                if (nm && std::strcmp(nm, imageName) == 0)
                    return img;
            }
            return nullptr;
        }

        void* NativeMethodPtr(void* methodInfo) {
            if (!methodInfo)
                return nullptr;
            return *(void**)methodInfo;  // native fn ptr sits at MethodInfo offset 0
        }

        void LogHandle(Logger& log, bool logSteps, const std::string& what, void* handle) {
            if (handle) {
                if (logSteps)
                    log.Info("Resolve: " + what + " ok");
            }
            else {
                log.Error("Resolve: not found: " + what);
            }
        }

        void ResolvePartSocket(const Il2CppApi& api, Logger& log, bool logSteps, void* partClass,
            const char* className, int& outOffset, void*& outImportProp) {
            if (!partClass) {
                log.Error(std::string("Resolve: class not found: ") + className);
                return;
            }
            if (logSteps)
                log.Info(std::string("Resolve: class ") + className + " ok");

            void* field = api.class_get_field_from_name(partClass, SocketTargets::kSocketFieldOnPart);
            if (field) {
                outOffset = api.field_get_offset(field);
                if (logSteps)
                    log.Info(std::string("Resolve: ") + className + "." + SocketTargets::kSocketFieldOnPart +
                        " offset=" + std::to_string(outOffset));
            }
            else {
                log.Error(std::string("Resolve: field not found: ") + className + "." + SocketTargets::kSocketFieldOnPart);
            }

            void* mi = api.class_get_method_from_name(partClass, SocketTargets::kImportPropMethod, 3);
            outImportProp = NativeMethodPtr(mi);
            LogHandle(log, logSteps, std::string(className) + "." + SocketTargets::kImportPropMethod + "(3)", outImportProp);
        }

    }

    void SocketResolver::LogPlannedTargets(Logger& log) const {
        log.Info("SocketResolver: planned targets (no read performed yet)");
        log.Info(std::string("  image  = ") + SocketTargets::kImageName);
        log.Info(std::string("  class  = ") + SocketTargets::kCpuSocketExtClass +
            "  fields=" + SocketTargets::kSocketNamesField + "," + SocketTargets::kSocketUsedField +
            "  method=" + SocketTargets::kGetUINameMethod + "(1)");
        log.Info(std::string("  class  = ") + SocketTargets::kCpuPartClass +
            "  field=" + SocketTargets::kSocketFieldOnPart +
            "  method=" + SocketTargets::kImportPropMethod + "(3)");
        log.Info(std::string("  class  = ") + SocketTargets::kMbPartClass +
            "  field=" + SocketTargets::kSocketFieldOnPart +
            "  method=" + SocketTargets::kImportPropMethod + "(3)");
    }

    bool SocketResolver::Resolve(const Il2CppApi& api, Logger& log, bool logSteps) {
        m_handles = SocketTargetHandles();

        m_handles.image = FindImage(api, SocketTargets::kImageName, log);
        if (!m_handles.image) {
            log.Error(std::string("Resolve: image not found: ") + SocketTargets::kImageName);
            return false;
        }
        if (logSteps)
            log.Info(std::string("Resolve: image ") + SocketTargets::kImageName + " ok");

        m_handles.cpuSocketExtClass = api.class_from_name(m_handles.image, "", SocketTargets::kCpuSocketExtClass);
        if (m_handles.cpuSocketExtClass) {
            if (logSteps)
                log.Info(std::string("Resolve: class ") + SocketTargets::kCpuSocketExtClass + " ok");

            m_handles.sNamesField = api.class_get_field_from_name(m_handles.cpuSocketExtClass, SocketTargets::kSocketNamesField);
            m_handles.sUsedField = api.class_get_field_from_name(m_handles.cpuSocketExtClass, SocketTargets::kSocketUsedField);
            LogHandle(log, logSteps, std::string(SocketTargets::kCpuSocketExtClass) + "." + SocketTargets::kSocketNamesField, m_handles.sNamesField);
            LogHandle(log, logSteps, std::string(SocketTargets::kCpuSocketExtClass) + "." + SocketTargets::kSocketUsedField, m_handles.sUsedField);

            void* uiMi = api.class_get_method_from_name(m_handles.cpuSocketExtClass, SocketTargets::kGetUINameMethod, 1);
            m_handles.getUINameMethod = NativeMethodPtr(uiMi);
            LogHandle(log, logSteps, std::string(SocketTargets::kCpuSocketExtClass) + "." + SocketTargets::kGetUINameMethod + "(1)", m_handles.getUINameMethod);
        }
        else {
            log.Error(std::string("Resolve: class not found: ") + SocketTargets::kCpuSocketExtClass);
        }

        m_handles.cpuPartClass = api.class_from_name(m_handles.image, "", SocketTargets::kCpuPartClass);
        ResolvePartSocket(api, log, logSteps, m_handles.cpuPartClass, SocketTargets::kCpuPartClass,
            m_handles.cpuSocketOffset, m_handles.cpuImportProp);

        m_handles.mbPartClass = api.class_from_name(m_handles.image, "", SocketTargets::kMbPartClass);
        ResolvePartSocket(api, log, logSteps, m_handles.mbPartClass, SocketTargets::kMbPartClass,
            m_handles.mbSocketOffset, m_handles.mbImportProp);

        m_handles.resolved = (m_handles.sNamesField != nullptr && m_handles.sUsedField != nullptr);
        if (m_handles.resolved)
            log.Info("Resolve: socket targets resolved");
        else
            log.Warn("Resolve: core socket targets missing");
        return m_handles.resolved;
    }

}