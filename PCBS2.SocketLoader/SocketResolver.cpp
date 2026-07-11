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

        void LogHandle(Logger& log, bool logSteps, const std::string& what, void* handle) {
            if (handle) {
                if (logSteps)
                    log.Info("Resolve: " + what + " ok");
            }
            else {
                log.Error("Resolve: not found: " + what);
            }
        }

        void ResolveFieldOffset(
            const Il2CppApi& api,
            Logger& log,
            bool logSteps,
            void* klass,
            const char* className,
            const char* fieldName,
            int& outOffset)
        {
            if (!klass) {
                log.Error(std::string("Resolve: class not found: ") + className);
                return;
            }

            if (logSteps)
                log.Info(std::string("Resolve: class ") + className + " ok");

            void* field = api.class_get_field_from_name(
                klass,
                fieldName);

            if (!field) {
                log.Error(
                    std::string("Resolve: field not found: ") +
                    className + "." + fieldName);
                return;
            }

            outOffset = api.field_get_offset(field);

            if (logSteps) {
                log.Info(
                    std::string("Resolve: ") +
                    className + "." + fieldName +
                    " offset=" +
                    std::to_string(outOffset));
            }
        }

    }

    void SocketResolver::LogPlannedTargets(Logger& log) const {
        log.Info("SocketResolver: planned targets (no read performed yet)");
        log.Info(std::string("  image  = ") + SocketTargets::kImageName);

        log.Info(
            std::string("  class  = ") +
            SocketTargets::kCpuSocketExtClass +
            "  fields=" +
            SocketTargets::kSocketNamesField + "," +
            SocketTargets::kSocketUsedField);

        log.Info(
            std::string("  class  = ") +
            SocketTargets::kCpuPartClass +
            "  field=" +
            SocketTargets::kSocketFieldOnPart);

        log.Info(
            std::string("  class  = ") +
            SocketTargets::kMbPartClass +
            "  field=" +
            SocketTargets::kSocketFieldOnPart);

        log.Info(
            std::string("  class  = ") +
            SocketTargets::kPartsDatabaseClass +
            "  field=" +
            SocketTargets::kPartsDatabaseInstanceField +
            "  method=" +
            SocketTargets::kGetPartsMethod + "(0)");

        log.Info(
            std::string("  class  = ") +
            SocketTargets::kPartDescClass +
            "  field=" +
            SocketTargets::kPartTypeField);

        log.Info(
            std::string("  class  = ") +
            SocketTargets::kCoolerPartClass +
            "  field=" +
            SocketTargets::kSocketRequirementField);

        log.Info(
            std::string("  class  = ") +
            SocketTargets::kCpuBlockPartClass +
            "  field=" +
            SocketTargets::kSocketRequirementField);

        log.Info(
            std::string("  class  = ") +
            SocketTargets::kSocketRequirementClass +
            "  field=" +
            SocketTargets::kSocketRequirementField);
    }

    bool SocketResolver::Resolve(const Il2CppApi& api, Logger& log, bool logSteps) {
        m_handles = SocketTargetHandles();

        m_handles.image = FindImage(
            api,
            SocketTargets::kImageName,
            log);

        if (!m_handles.image) {
            log.Error(
                std::string("Resolve: image not found: ") +
                SocketTargets::kImageName);
            return false;
        }

        if (logSteps) {
            log.Info(
                std::string("Resolve: image ") +
                SocketTargets::kImageName +
                " ok");
        }

        void* cpuSocketExtClass = api.class_from_name(
            m_handles.image,
            "",
            SocketTargets::kCpuSocketExtClass);

        if (cpuSocketExtClass) {
            if (logSteps)
                log.Info(std::string("Resolve: class ") + SocketTargets::kCpuSocketExtClass + " ok");

            m_handles.sNamesField = api.class_get_field_from_name(
                cpuSocketExtClass,
                SocketTargets::kSocketNamesField);

            m_handles.sUsedField = api.class_get_field_from_name(
                cpuSocketExtClass,
                SocketTargets::kSocketUsedField);

            LogHandle(
                log,
                logSteps,
                std::string(SocketTargets::kCpuSocketExtClass) + "." +
                SocketTargets::kSocketNamesField,
                m_handles.sNamesField);

            LogHandle(
                log,
                logSteps,
                std::string(SocketTargets::kCpuSocketExtClass) + "." +
                SocketTargets::kSocketUsedField,
                m_handles.sUsedField);
        }
        else {
            log.Error(std::string("Resolve: class not found: ") + SocketTargets::kCpuSocketExtClass);
        }

        m_handles.cpuPartClass = api.class_from_name(
            m_handles.image,
            "",
            SocketTargets::kCpuPartClass);

        ResolveFieldOffset(
            api,
            log,
            logSteps,
            m_handles.cpuPartClass,
            SocketTargets::kCpuPartClass,
            SocketTargets::kSocketFieldOnPart,
            m_handles.cpuSocketOffset);

        m_handles.mbPartClass = api.class_from_name(
            m_handles.image,
            "",
            SocketTargets::kMbPartClass);

        ResolveFieldOffset(
            api,
            log,
            logSteps,
            m_handles.mbPartClass,
            SocketTargets::kMbPartClass,
            SocketTargets::kSocketFieldOnPart,
            m_handles.mbSocketOffset);

        void* partsDatabaseClass = api.class_from_name(
            m_handles.image,
            "",
            SocketTargets::kPartsDatabaseClass);

        if (partsDatabaseClass) {
            if (logSteps) {
                log.Info(
                    std::string("Resolve: class ") +
                    SocketTargets::kPartsDatabaseClass + " ok");
            }

            m_handles.partsDatabaseInstanceField =
                api.class_get_field_from_name(
                    partsDatabaseClass,
                    SocketTargets::kPartsDatabaseInstanceField);

            m_handles.getPartsMethod =
                api.class_get_method_from_name(
                    partsDatabaseClass,
                    SocketTargets::kGetPartsMethod,
                    0);

            LogHandle(
                log,
                logSteps,
                std::string(SocketTargets::kPartsDatabaseClass) + "." +
                SocketTargets::kPartsDatabaseInstanceField,
                m_handles.partsDatabaseInstanceField);

            LogHandle(
                log,
                logSteps,
                std::string(SocketTargets::kPartsDatabaseClass) + "." +
                SocketTargets::kGetPartsMethod + "(0)",
                m_handles.getPartsMethod);
        }
        else {
            log.Error(
                std::string("Resolve: class not found: ") +
                SocketTargets::kPartsDatabaseClass);
        }

        void* partDescClass = api.class_from_name(
            m_handles.image,
            "",
            SocketTargets::kPartDescClass);

        ResolveFieldOffset(
            api,
            log,
            logSteps,
            partDescClass,
            SocketTargets::kPartDescClass,
            SocketTargets::kPartTypeField,
            m_handles.partTypeOffset);

        m_handles.coolerPartClass = api.class_from_name(
            m_handles.image,
            "",
            SocketTargets::kCoolerPartClass);

        ResolveFieldOffset(
            api,
            log,
            logSteps,
            m_handles.coolerPartClass,
            SocketTargets::kCoolerPartClass,
            SocketTargets::kSocketRequirementField,
            m_handles.coolerSocketRequirementOffset);

        m_handles.cpuBlockPartClass = api.class_from_name(
            m_handles.image,
            "",
            SocketTargets::kCpuBlockPartClass);

        ResolveFieldOffset(
            api,
            log,
            logSteps,
            m_handles.cpuBlockPartClass,
            SocketTargets::kCpuBlockPartClass,
            SocketTargets::kSocketRequirementField,
            m_handles.cpuBlockSocketRequirementOffset);

        m_handles.socketRequirementClass = api.class_from_name(
            m_handles.image,
            "",
            SocketTargets::kSocketRequirementClass);

        ResolveFieldOffset(
            api,
            log,
            logSteps,
            m_handles.socketRequirementClass,
            SocketTargets::kSocketRequirementClass,
            SocketTargets::kSocketRequirementField,
            m_handles.socketRequirementListOffset);

        m_handles.resolved = (
            m_handles.sNamesField != nullptr &&
            m_handles.sUsedField != nullptr);

        m_handles.coolerCompatibilityResolved = (
            m_handles.partsDatabaseInstanceField != nullptr &&
            m_handles.getPartsMethod != nullptr &&
            m_handles.coolerPartClass != nullptr &&
            m_handles.cpuBlockPartClass != nullptr &&
            m_handles.socketRequirementClass != nullptr &&
            m_handles.partTypeOffset >= 0 &&
            m_handles.coolerSocketRequirementOffset >= 0 &&
            m_handles.cpuBlockSocketRequirementOffset >= 0 &&
            m_handles.socketRequirementListOffset >= 0);

        if (m_handles.resolved)
            log.Info("Resolve: socket targets resolved");
        else
            log.Warn("Resolve: core socket targets missing");

        if (m_handles.coolerCompatibilityResolved) {
            log.Info("Resolve: cooler compatibility targets resolved");
        }
        else {
            log.Warn(
                "Resolve: cooler compatibility targets missing; "
                "cooler patch will be unavailable");
        }

        return m_handles.resolved;
    }

}