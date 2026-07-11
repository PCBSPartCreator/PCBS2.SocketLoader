#pragma once

#include "Il2CppApi.h"

namespace SocketLoader {

    class Logger;

    namespace SocketTargets {
        static const char* kImageName = "Assembly-CSharp.dll";

        static const char* kCpuSocketExtClass = "CpuSocketExt";
        static const char* kSocketNamesField = "s_names"; // static string[]
        static const char* kSocketUsedField = "s_used";   // static bool[]

        static const char* kCpuPartClass = "PartDescCPU";
        static const char* kMbPartClass = "PartDescMotherboard";
        static const char* kSocketFieldOnPart = "m_socket";

        static const char* kPartsDatabaseClass = "PartsDatabase";
        static const char* kPartsDatabaseInstanceField = "s_instance";
        static const char* kGetPartsMethod = "GetParts";

        static const char* kPartDescClass = "PartDesc";
        static const char* kPartTypeField = "m_type";

        static const char* kCoolerPartClass = "PartDescCooler";
        static const char* kCpuBlockPartClass = "PartDescCPUBlock";
        static const char* kSocketRequirementClass = "SocketRequirement";
        static const char* kSocketRequirementField = "m_socket";
    }

    struct SocketTargetHandles {
        void* image = nullptr;

        void* sNamesField = nullptr;
        void* sUsedField = nullptr;

        void* cpuPartClass = nullptr;
        void* mbPartClass = nullptr;
        int   cpuSocketOffset = -1;
        int   mbSocketOffset = -1;

        void* partsDatabaseInstanceField = nullptr;
        void* getPartsMethod = nullptr;

        void* coolerPartClass = nullptr;
        void* cpuBlockPartClass = nullptr;
        void* socketRequirementClass = nullptr;

        int partTypeOffset = -1;
        int coolerSocketRequirementOffset = -1;
        int cpuBlockSocketRequirementOffset = -1;
        int socketRequirementListOffset = -1;

        bool resolved = false;
        bool coolerCompatibilityResolved = false;
    };

    class SocketResolver {
    public:
        void LogPlannedTargets(Logger& log) const;

        bool Resolve(const Il2CppApi& api, Logger& log, bool logSteps);

        const SocketTargetHandles& Handles() const { return m_handles; }

        bool IsResolved() const {
            return m_handles.resolved;
        }

        bool IsCoolerCompatibilityResolved() const {
            return m_handles.coolerCompatibilityResolved;
        }

    private:
        SocketTargetHandles m_handles;
    };

}