#pragma once

#include "Il2CppApi.h"

namespace SocketLoader {

    class Logger;

    namespace SocketTargets {
        static const char* kImageName = "Assembly-CSharp.dll";

        static const char* kCpuSocketExtClass = "CpuSocketExt";
        static const char* kSocketNamesField = "s_names";  // static string[]
        static const char* kSocketUsedField = "s_used";  // static bool[]
        static const char* kGetUINameMethod = "GetUIName";  // argc 1

        static const char* kCpuPartClass = "PartDescCPU";
        static const char* kMbPartClass = "PartDescMotherboard";
        static const char* kSocketFieldOnPart = "m_socket";  // instance int
        static const char* kImportPropMethod = "ImportProp";  // argc 3
    }

    struct SocketTargetHandles {
        void* image = nullptr;
        void* cpuSocketExtClass = nullptr;
        void* sNamesField = nullptr;
        void* sUsedField = nullptr;
        void* getUINameMethod = nullptr;  // native method ptr
        void* cpuPartClass = nullptr;
        void* mbPartClass = nullptr;
        int   cpuSocketOffset = -1;
        int   mbSocketOffset = -1;
        void* cpuImportProp = nullptr;
        void* mbImportProp = nullptr;
        bool  resolved = false;
    };

    class SocketResolver {
    public:
        void LogPlannedTargets(Logger& log) const;

        bool Resolve(const Il2CppApi& api, Logger& log, bool logSteps);

        const SocketTargetHandles& Handles() const { return m_handles; }
        bool IsResolved() const { return m_handles.resolved; }

    private:
        SocketTargetHandles m_handles;
    };

}