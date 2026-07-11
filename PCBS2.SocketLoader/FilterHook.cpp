#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "FilterHook.h"

#include "HookManager.h"
#include "Log.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <string>
#include <vector>

namespace SocketLoader {

    namespace {

        constexpr const char* kFilterNamespace = "PCBS2.UI.Filters";
        constexpr const char* kFilterHolderClass = "FilterHolder";
        constexpr const char* kSocketFilterMethod = "SocketFilter";
        constexpr int kMaxSocketListSize = 512; // Guards against invalid managed list state.

        using SocketFilterFn = void* (*)(void* part, void* method);
        using ListGetCountFn = int (*)(void* list, void* method);
        using ListGetItemFn = int (*)(void* list, int index, void* method);

        struct CustomSocket {
            int id = -1;
            std::string name;
        };

        struct SocketListAccessors {
            void* listClass = nullptr;
            void* countMethod = nullptr;
            void* itemMethod = nullptr;
            ListGetCountFn getCount = nullptr;
            ListGetItemFn getItem = nullptr;

            bool Matches(void* klass) const {
                return klass &&
                    listClass == klass &&
                    getCount &&
                    getItem;
            }
        };

        struct SocketTables {
            void* const* names = nullptr;
            const uint8_t* used = nullptr;
            uintptr_t count = 0;
        };

        struct FilterArrayResult {
            void* array = nullptr;
            int totalSockets = 0;
            int customSockets = 0;
        };

        struct HookState {
            Logger* log = nullptr;
            Il2CppApi api;

            SocketFilterFn originalFilter = nullptr;
            SocketListAccessors listAccessors;
            std::vector<CustomSocket> customSockets;

            void* cpuClass = nullptr;
            void* motherboardClass = nullptr;
            void* coolerClass = nullptr;
            void* cpuBlockClass = nullptr;
            void* socketRequirementClass = nullptr;

            void* socketNamesField = nullptr;
            void* socketUsedField = nullptr;
            void* stringClass = nullptr;

            int cpuSocketOffset = -1;
            int motherboardSocketOffset = -1;
            int coolerRequirementOffset = -1;
            int cpuBlockRequirementOffset = -1;
            int requirementListOffset = -1;

            bool singleSocketLogged = false;
            bool coolerLogged = false;
            bool cpuBlockLogged = false;
            bool layoutErrorLogged = false;
            bool listErrorLogged = false;
        };

        HookState g_state;

        void LogErrorOnce(bool& flag, const std::string& message) {
            if (!g_state.log || flag)
                return;

            flag = true;
            g_state.log->Error(message);
        }

        bool IsExecutableProtection(DWORD protection) {
            if (protection & PAGE_GUARD)
                return false;

            switch (protection & 0xFF) {
            case PAGE_EXECUTE:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        bool IsInsideModule(void* address, HMODULE module) {
            if (!address || !module)
                return false;

            const uintptr_t base = reinterpret_cast<uintptr_t>(module);
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);

            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                return false;

            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
                base + static_cast<uintptr_t>(dos->e_lfanew));

            if (nt->Signature != IMAGE_NT_SIGNATURE)
                return false;

            const uintptr_t pointer = reinterpret_cast<uintptr_t>(address);
            const uintptr_t end =
                base + static_cast<uintptr_t>(nt->OptionalHeader.SizeOfImage);

            return pointer >= base && pointer < end;
        }

        bool IsExecutableGameAssemblyAddress(void* address) {
            HMODULE gameAssembly = GetModuleHandleA("GameAssembly.dll");

            if (!IsInsideModule(address, gameAssembly))
                return false;

            MEMORY_BASIC_INFORMATION memory = {};

            if (VirtualQuery(address, &memory, sizeof(memory)) == 0)
                return false;

            return memory.State == MEM_COMMIT &&
                IsExecutableProtection(memory.Protect);
        }

        const CustomSocket* FindCustomSocket(int id) {
            for (const CustomSocket& socket : g_state.customSockets) {
                if (socket.id == id)
                    return &socket;
            }

            return nullptr;
        }

        void* ReadObjectField(void* object, int offset) {
            if (!object || offset < 0)
                return nullptr;

            const uintptr_t address =
                reinterpret_cast<uintptr_t>(object) +
                static_cast<uintptr_t>(offset);

            return *reinterpret_cast<void**>(address);
        }

        int ReadIntField(void* object, int offset) {
            if (!object || offset < 0)
                return INT_MIN;

            const uintptr_t address =
                reinterpret_cast<uintptr_t>(object) +
                static_cast<uintptr_t>(offset);

            return *reinterpret_cast<int*>(address);
        }

        void* GetStringClass() {
            if (g_state.stringClass)
                return g_state.stringClass;

            if (!g_state.socketNamesField ||
                !g_state.api.field_static_get_value ||
                !g_state.api.class_get_element_class) {
                return nullptr;
            }

            void* namesArray = nullptr;
            g_state.api.field_static_get_value(
                g_state.socketNamesField,
                &namesArray);

            void* arrayClass = Il2CppLayout::ObjectClass(namesArray);
            if (!arrayClass)
                return nullptr;

            g_state.stringClass =
                g_state.api.class_get_element_class(arrayClass);

            return g_state.stringClass;
        }

        int GetSingleSocket(void* part) {
            void* partClass = Il2CppLayout::ObjectClass(part);

            if (partClass == g_state.cpuClass)
                return ReadIntField(part, g_state.cpuSocketOffset);

            if (partClass == g_state.motherboardClass)
                return ReadIntField(part, g_state.motherboardSocketOffset);

            return INT_MIN;
        }

        void* GetSocketList(void* part) {
            void* partClass = Il2CppLayout::ObjectClass(part);
            int requirementOffset = -1;

            if (partClass == g_state.coolerClass)
                requirementOffset = g_state.coolerRequirementOffset;
            else if (partClass == g_state.cpuBlockClass)
                requirementOffset = g_state.cpuBlockRequirementOffset;
            else
                return nullptr;

            void* requirement = ReadObjectField(part, requirementOffset);

            if (!requirement ||
                Il2CppLayout::ObjectClass(requirement) !=
                g_state.socketRequirementClass) {
                return nullptr;
            }

            return ReadObjectField(
                requirement,
                g_state.requirementListOffset);
        }

        bool ResolveSocketListAccessors(void* list) {
            if (!list || !g_state.api.class_get_method_from_name)
                return false;

            void* listClass = Il2CppLayout::ObjectClass(list);
            if (!listClass)
                return false;

            if (g_state.listAccessors.Matches(listClass))
                return true;

            SocketListAccessors accessors;
            accessors.listClass = listClass;
            accessors.countMethod =
                g_state.api.class_get_method_from_name(
                    listClass,
                    "get_Count",
                    0);
            accessors.itemMethod =
                g_state.api.class_get_method_from_name(
                    listClass,
                    "get_Item",
                    1);
            accessors.getCount =
                reinterpret_cast<ListGetCountFn>(
                    Il2CppLayout::MethodPointer(accessors.countMethod));
            accessors.getItem =
                reinterpret_cast<ListGetItemFn>(
                    Il2CppLayout::MethodPointer(accessors.itemMethod));

            const bool valid =
                accessors.countMethod &&
                accessors.itemMethod &&
                accessors.getCount &&
                accessors.getItem &&
                IsExecutableGameAssemblyAddress(
                    reinterpret_cast<void*>(accessors.getCount)) &&
                IsExecutableGameAssemblyAddress(
                    reinterpret_cast<void*>(accessors.getItem));

            if (!valid) {
                LogErrorOnce(
                    g_state.listErrorLogged,
                    "FilterHook: failed to resolve List<CpuSocket>.Count/Item");
                return false;
            }

            g_state.listAccessors = accessors;
            return true;
        }

        bool ReadSocketTables(SocketTables& tables) {
            tables = SocketTables();

            if (!g_state.socketNamesField ||
                !g_state.socketUsedField ||
                !g_state.api.field_static_get_value ||
                !g_state.api.array_length) {
                return false;
            }

            void* namesArray = nullptr;
            void* usedArray = nullptr;

            g_state.api.field_static_get_value(
                g_state.socketNamesField,
                &namesArray);
            g_state.api.field_static_get_value(
                g_state.socketUsedField,
                &usedArray);

            if (!namesArray || !usedArray)
                return false;

            const uintptr_t namesLength =
                g_state.api.array_length(namesArray);
            const uintptr_t usedLength =
                g_state.api.array_length(usedArray);

            if (!Il2CppLayout::ArrayLayoutMatches(
                namesArray,
                namesLength) ||
                !Il2CppLayout::ArrayLayoutMatches(
                    usedArray,
                    usedLength)) {
                LogErrorOnce(
                    g_state.layoutErrorLogged,
                    "FilterHook: socket tables use an unexpected IL2CPP array layout");
                return false;
            }

            tables.names =
                Il2CppLayout::ArrayData<void*>(namesArray);
            tables.used =
                Il2CppLayout::ArrayData<uint8_t>(usedArray);
            tables.count =
                std::min(namesLength, usedLength);

            return tables.names &&
                tables.used &&
                tables.count > 0;
        }

        void* CreateStringArray(const std::vector<void*>& values) {
            if (values.empty() ||
                !g_state.api.array_new ||
                !g_state.api.array_length) {
                return nullptr;
            }

            void* stringClass = GetStringClass();
            if (!stringClass)
                return nullptr;

            void* result =
                g_state.api.array_new(stringClass, values.size());

            if (!result)
                return nullptr;

            const uintptr_t length =
                g_state.api.array_length(result);

            if (length != values.size() ||
                !Il2CppLayout::ArrayLayoutMatches(result, length)) {
                LogErrorOnce(
                    g_state.layoutErrorLogged,
                    "FilterHook: generated filter array uses an unexpected IL2CPP layout");
                return nullptr;
            }

            void** data = Il2CppLayout::ArrayData<void*>(result);
            std::copy(values.begin(), values.end(), data);
            return result;
        }

        void* CreateSingleSocketFilter(const CustomSocket& socket) {
            if (!g_state.api.string_new)
                return nullptr;

            void* name = g_state.api.string_new(socket.name.c_str());
            if (!name)
                return nullptr;

            return CreateStringArray(std::vector<void*>{ name });
        }

        FilterArrayResult CreateSocketListFilter(void* socketList) {
            FilterArrayResult result;

            if (!ResolveSocketListAccessors(socketList))
                return result;

            const int listCount =
                g_state.listAccessors.getCount(
                    socketList,
                    g_state.listAccessors.countMethod);

            if (listCount <= 0)
                return result;

            if (listCount > kMaxSocketListSize) {
                LogErrorOnce(
                    g_state.listErrorLogged,
                    "FilterHook: socket list returned an invalid item count");
                return result;
            }

            SocketTables tables;
            if (!ReadSocketTables(tables))
                return result;

            std::vector<int> socketIds;
            std::vector<void*> names;
            socketIds.reserve(static_cast<size_t>(listCount));
            names.reserve(static_cast<size_t>(listCount));

            for (int i = 0; i < listCount; ++i) {
                const int socketId =
                    g_state.listAccessors.getItem(
                        socketList,
                        i,
                        g_state.listAccessors.itemMethod);

                if (socketId < 0 ||
                    static_cast<uintptr_t>(socketId) >= tables.count ||
                    tables.used[socketId] == 0 ||
                    std::find(
                        socketIds.begin(),
                        socketIds.end(),
                        socketId) != socketIds.end()) {
                    continue;
                }

                void* socketName = tables.names[socketId];

                if (!socketName ||
                    Il2CppLayout::StringLength(socketName) <= 0) {
                    continue;
                }

                socketIds.push_back(socketId);
                names.push_back(socketName);

                if (FindCustomSocket(socketId))
                    ++result.customSockets;
            }

            if (result.customSockets == 0)
                return result;

            result.array = CreateStringArray(names);
            result.totalSockets =
                result.array
                ? static_cast<int>(names.size())
                : 0;

            return result;
        }

        void LogSingleSocket(const CustomSocket& socket) {
            if (!g_state.log || g_state.singleSocketLogged)
                return;

            g_state.singleSocketLogged = true;
            g_state.log->Info(
                "FilterHook: SocketFilter returns \"" +
                socket.name +
                "\" for custom socket " +
                std::to_string(socket.id));
        }

        void LogSocketList(
            void* partClass,
            const FilterArrayResult& result)
        {
            if (!g_state.log)
                return;

            if (partClass == g_state.coolerClass &&
                !g_state.coolerLogged) {
                g_state.coolerLogged = true;
                g_state.log->Info(
                    "FilterHook: cooler filter returns " +
                    std::to_string(result.totalSockets) +
                    " socket value(s), including " +
                    std::to_string(result.customSockets) +
                    " custom socket(s)");
            }
            else if (partClass == g_state.cpuBlockClass &&
                !g_state.cpuBlockLogged) {
                g_state.cpuBlockLogged = true;
                g_state.log->Info(
                    "FilterHook: CPU water block filter returns " +
                    std::to_string(result.totalSockets) +
                    " socket value(s), including " +
                    std::to_string(result.customSockets) +
                    " custom socket(s)");
            }
        }

        void* DetourSocketFilter(void* part, void* method) {
            if (part) {
                const int socketId = GetSingleSocket(part);
                const CustomSocket* customSocket =
                    socketId != INT_MIN
                    ? FindCustomSocket(socketId)
                    : nullptr;

                if (customSocket) {
                    void* result =
                        CreateSingleSocketFilter(*customSocket);

                    if (result) {
                        LogSingleSocket(*customSocket);
                        return result;
                    }
                }

                void* socketList = GetSocketList(part);

                if (socketList) {
                    const FilterArrayResult result =
                        CreateSocketListFilter(socketList);

                    if (result.array) {
                        LogSocketList(
                            Il2CppLayout::ObjectClass(part),
                            result);
                        return result.array;
                    }
                }
            }

            return g_state.originalFilter
                ? g_state.originalFilter(part, method)
                : nullptr; // Preserve the native filter for every untouched part.
        }

        bool HasCorePrerequisites(
            const Il2CppApi& api,
            const SocketTargetHandles& targets)
        {
            return
                targets.image &&
                targets.sNamesField &&
                targets.sUsedField &&
                targets.cpuPartClass &&
                targets.mbPartClass &&
                targets.cpuSocketOffset >= 0 &&
                targets.mbSocketOffset >= 0 &&
                api.class_from_name &&
                api.class_get_method_from_name &&
                api.field_static_get_value &&
                api.array_new &&
                api.array_length &&
                api.class_get_element_class &&
                api.string_new;
        }

        bool HasCoolerPrerequisites(
            const SocketTargetHandles& targets)
        {
            return
                targets.coolerCompatibilityResolved &&
                targets.coolerPartClass &&
                targets.cpuBlockPartClass &&
                targets.socketRequirementClass &&
                targets.coolerSocketRequirementOffset >= 0 &&
                targets.cpuBlockSocketRequirementOffset >= 0 &&
                targets.socketRequirementListOffset >= 0;
        }

        void LoadCustomSockets(
            const std::vector<SocketEntry>& sockets)
        {
            g_state.customSockets.clear();

            for (const SocketEntry& socket : sockets) {
                if (!socket.enabled ||
                    !socket.valid ||
                    socket.displayName.empty()) {
                    continue;
                }

                const auto duplicate = std::find_if(
                    g_state.customSockets.begin(),
                    g_state.customSockets.end(),
                    [&socket](const CustomSocket& current) {
                        return current.id == socket.id;
                    });

                if (duplicate == g_state.customSockets.end()) {
                    CustomSocket customSocket;
                    customSocket.id = socket.id;
                    customSocket.name = socket.displayName;
                    g_state.customSockets.push_back(customSocket);
                }
            }
        }

        void InitializeState(
            const Il2CppApi& api,
            const SocketTargetHandles& targets,
            const std::vector<SocketEntry>& sockets,
            Logger& log,
            bool coolerReady)
        {
            g_state = HookState();
            g_state.api = api;
            g_state.log = &log;

            g_state.cpuClass = targets.cpuPartClass;
            g_state.motherboardClass = targets.mbPartClass;
            g_state.cpuSocketOffset = targets.cpuSocketOffset;
            g_state.motherboardSocketOffset =
                targets.mbSocketOffset;
            g_state.socketNamesField = targets.sNamesField;
            g_state.socketUsedField = targets.sUsedField;

            if (coolerReady) {
                g_state.coolerClass = targets.coolerPartClass;
                g_state.cpuBlockClass = targets.cpuBlockPartClass;
                g_state.socketRequirementClass =
                    targets.socketRequirementClass;
                g_state.coolerRequirementOffset =
                    targets.coolerSocketRequirementOffset;
                g_state.cpuBlockRequirementOffset =
                    targets.cpuBlockSocketRequirementOffset;
                g_state.requirementListOffset =
                    targets.socketRequirementListOffset;
            }

            LoadCustomSockets(sockets);
        }

        bool ResolveHookTarget(
            const Il2CppApi& api,
            const SocketTargetHandles& targets,
            Logger& log,
            void*& target)
        {
            target = nullptr;

            void* filterHolder = api.class_from_name(
                targets.image,
                kFilterNamespace,
                kFilterHolderClass);

            if (!filterHolder) {
                log.Error(
                    std::string("FilterHook: class not found: ") +
                    kFilterNamespace +
                    "." +
                    kFilterHolderClass);
                return false;
            }

            void* method = api.class_get_method_from_name(
                filterHolder,
                kSocketFilterMethod,
                1);

            if (!method) {
                log.Error(
                    "FilterHook: method not found: FilterHolder.SocketFilter");
                return false;
            }

            target = Il2CppLayout::MethodPointer(method);

            if (!target) {
                log.Error(
                    "FilterHook: null native pointer for SocketFilter");
                return false;
            }

            if (!IsExecutableGameAssemblyAddress(target)) {
                log.Error(
                    "FilterHook: SocketFilter native pointer is not a valid "
                    "executable address inside GameAssembly.dll");
                return false;
            }

            return true;
        }

    }

    bool FilterHook::Install(
        const Il2CppApi& api,
        const SocketTargetHandles& targets,
        const std::vector<SocketEntry>& sockets,
        Logger& log)
    {
        if (m_installed)
            return true;

        if (!HasCorePrerequisites(api, targets)) {
            log.Error(
                "FilterHook: core prerequisites missing, cannot install");
            return false;
        }

        const bool coolerReady =
            HasCoolerPrerequisites(targets);

        if (!coolerReady) {
            log.Warn(
                "FilterHook: cooler compatibility targets are incomplete; "
                "cooler filter extension is disabled");
        }

        void* target = nullptr;

        if (!ResolveHookTarget(api, targets, log, target))
            return false;

        InitializeState(
            api,
            targets,
            sockets,
            log,
            coolerReady);

        if (g_state.customSockets.empty()) {
            log.Warn(
                "FilterHook: no valid custom sockets, hook was not installed");
            return false;
        }

        if (!Hook::Create(
            target,
            reinterpret_cast<void*>(&DetourSocketFilter),
            reinterpret_cast<void**>(&g_state.originalFilter),
            kSocketFilterMethod,
            log)) {
            return false;
        }

        const bool enabled =
            Hook::Enable(
                target,
                kSocketFilterMethod,
                log);

        m_installed = enabled;

        if (enabled) {
            log.Info(
                "FilterHook: installed for " +
                std::to_string(g_state.customSockets.size()) +
                " custom socket(s)");
        }
        else {
            log.Error(
                "FilterHook: hook was created but could not be enabled");
        }

        return enabled;
    }

}