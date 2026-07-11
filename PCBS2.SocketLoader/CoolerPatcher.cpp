#include "CoolerPatcher.h"
#include "Log.h"

#include <string>

namespace SocketLoader {

    namespace {

        const unsigned long long kPollIntervalMs = 100;
        const unsigned long long kStableDelayMs = 1500;
        const unsigned long long kWaitTimeoutMs = 60000;

        const int kAirCoolerType = 2;
        const int kLiquidCoolerType = 4;
        const int kCpuBlockType = 23;

        typedef void* (*GetParts_t)(
            void* instance,
            void* method);

        typedef bool (*ListContains_t)(
            void* list,
            int value,
            void* method);

        typedef void (*ListAdd_t)(
            void* list,
            int value,
            void* method);

        struct ListMethods {
            void* listClass = nullptr;
            void* containsMethod = nullptr;
            void* addMethod = nullptr;

            ListContains_t contains = nullptr;
            ListAdd_t add = nullptr;
        };

        bool HasEnabledSockets(
            const std::vector<SocketEntry>& sockets)
        {
            for (size_t i = 0; i < sockets.size(); ++i) {
                if (sockets[i].enabled && sockets[i].valid)
                    return true;
            }

            return false;
        }

        bool ResolveListMethods(
            const Il2CppApi& api,
            void* list,
            ListMethods& out,
            Logger& log)
        {
            out.listClass =
                Il2CppLayout::ObjectClass(list);

            if (!out.listClass) {
                log.Error(
                    "CoolerPatch: socket list has no IL2CPP class");
                return false;
            }

            out.containsMethod =
                api.class_get_method_from_name(
                    out.listClass,
                    "Contains",
                    1);

            out.addMethod =
                api.class_get_method_from_name(
                    out.listClass,
                    "Add",
                    1);

            out.contains =
                reinterpret_cast<ListContains_t>(
                    Il2CppLayout::MethodPointer(
                        out.containsMethod));

            out.add =
                reinterpret_cast<ListAdd_t>(
                    Il2CppLayout::MethodPointer(
                        out.addMethod));

            if (!out.containsMethod ||
                !out.addMethod ||
                !out.contains ||
                !out.add) {
                log.Error(
                    "CoolerPatch: failed to resolve "
                    "List<CpuSocket>.Contains/Add");
                return false;
            }

            return true;
        }

        bool AddSocketsToList(
            void* list,
            const std::vector<SocketEntry>& sockets,
            const ListMethods& methods,
            int& additions,
            Logger& log)
        {
            if (Il2CppLayout::ObjectClass(list) !=
                methods.listClass) {
                log.Error(
                    "CoolerPatch: unexpected socket list class");
                return false;
            }

            for (size_t i = 0; i < sockets.size(); ++i) {
                const SocketEntry& socket = sockets[i];

                if (!socket.enabled || !socket.valid)
                    continue;

                if (methods.contains(
                    list,
                    socket.id,
                    methods.containsMethod)) {
                    continue;
                }

                methods.add(
                    list,
                    socket.id,
                    methods.addMethod);

                if (!methods.contains(
                    list,
                    socket.id,
                    methods.containsMethod)) {
                    log.Error(
                        "CoolerPatch: failed to verify added socket " +
                        std::to_string(socket.id));
                    return false;
                }

                ++additions;
            }

            return true;
        }

        void* ReadObjectField(
            void* object,
            int offset)
        {
            if (!object || offset < 0)
                return nullptr;

            return *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(object) +
                static_cast<uintptr_t>(offset));
        }

        int ReadIntField(
            void* object,
            int offset)
        {
            return *reinterpret_cast<int*>(
                reinterpret_cast<uintptr_t>(object) +
                static_cast<uintptr_t>(offset));
        }

    }

    bool CoolerPatcher::Poll(
        const Il2CppApi& api,
        const SocketTargetHandles& targets,
        const std::vector<SocketEntry>& sockets,
        Logger& log)
    {
        if (m_done || m_disabled)
            return m_done;

        if (!HasEnabledSockets(sockets)) {
            log.Info(
                "CoolerPatch: no enabled custom sockets, "
                "nothing to patch");

            m_done = true;
            return true;
        }

        if (!targets.coolerCompatibilityResolved ||
            !targets.partsDatabaseInstanceField ||
            !targets.getPartsMethod ||
            !targets.coolerPartClass ||
            !targets.cpuBlockPartClass ||
            !targets.socketRequirementClass ||
            targets.partTypeOffset < 0 ||
            targets.coolerSocketRequirementOffset < 0 ||
            targets.cpuBlockSocketRequirementOffset < 0 ||
            targets.socketRequirementListOffset < 0 ||
            !api.field_static_get_value ||
            !api.array_length ||
            !api.class_get_method_from_name) {
            log.Error(
                "CoolerPatch: prerequisites missing, "
                "patching disabled");

            m_disabled = true;
            return false;
        }

        const unsigned long long now =
            GetTickCount64();

        if (m_waitStartedAt == 0)
            m_waitStartedAt = now;

        if (now < m_nextPollAt)
            return false;

        m_nextPollAt =
            now + kPollIntervalMs;

        if (now - m_waitStartedAt >
            kWaitTimeoutMs) {
            log.Error(
                "CoolerPatch: parts database did not "
                "become ready, giving up");

            m_disabled = true;
            return false;
        }

        void* database = nullptr;

        api.field_static_get_value(
            targets.partsDatabaseInstanceField,
            &database);

        if (!database) {
            if (!m_loggedWaitingForDatabase) {
                m_loggedWaitingForDatabase = true;

                log.Info(
                    "CoolerPatch: waiting for "
                    "PartsDatabase instance");
            }

            return false;
        }

        void* getPartsPointer =
            Il2CppLayout::MethodPointer(
                targets.getPartsMethod);

        if (!getPartsPointer) {
            log.Error(
                "CoolerPatch: PartsDatabase.GetParts "
                "has no native pointer");

            m_disabled = true;
            return false;
        }

        GetParts_t getParts =
            reinterpret_cast<GetParts_t>(
                getPartsPointer);

        void* parts = getParts(
            database,
            targets.getPartsMethod);

        if (!parts) {
            if (!m_loggedWaitingForParts) {
                m_loggedWaitingForParts = true;

                log.Info(
                    "CoolerPatch: waiting for "
                    "PartsDatabase.GetParts()");
            }

            return false;
        }

        const uintptr_t partCount =
            api.array_length(parts);

        if (partCount == 0) {
            if (!m_loggedWaitingForParts) {
                m_loggedWaitingForParts = true;

                log.Info(
                    "CoolerPatch: waiting for parts to load");
            }

            return false;
        }

        if (!Il2CppLayout::ArrayLayoutMatches(
            parts,
            partCount)) {
            log.Error(
                "CoolerPatch: GetParts returned an "
                "unexpected array layout");

            m_disabled = true;
            return false;
        }

        if (partCount != m_lastPartCount) {
            m_lastPartCount = partCount;
            m_stableSince = now;

            log.Info(
                "CoolerPatch: observed " +
                std::to_string(partCount) +
                " parts; waiting for database to stabilize");

            return false;
        }

        if (m_stableSince == 0) {
            m_stableSince = now;
            return false;
        }

        if (now - m_stableSince <
            kStableDelayMs) {
            return false;
        }

        void** const partData =
            Il2CppLayout::ArrayData<void*>(parts);

        ListMethods listMethods;
        bool listMethodsResolved = false;

        int airCoolers = 0;
        int liquidCoolers = 0;
        int cpuBlocks = 0;
        int updatedParts = 0;
        int additions = 0;
        int missingRequirements = 0;
        int unexpectedClasses = 0;

        for (uintptr_t i = 0;
            i < partCount;
            ++i) {
            void* part = partData[i];

            if (!part)
                continue;

            const int type = ReadIntField(
                part,
                targets.partTypeOffset);

            int requirementOffset = -1;
            void* expectedClass = nullptr;

            if (type == kAirCoolerType) {
                ++airCoolers;

                requirementOffset =
                    targets.coolerSocketRequirementOffset;

                expectedClass =
                    targets.coolerPartClass;
            }
            else if (type == kLiquidCoolerType) {
                ++liquidCoolers;

                requirementOffset =
                    targets.coolerSocketRequirementOffset;

                expectedClass =
                    targets.coolerPartClass;
            }
            else if (type == kCpuBlockType) {
                ++cpuBlocks;

                requirementOffset =
                    targets.cpuBlockSocketRequirementOffset;

                expectedClass =
                    targets.cpuBlockPartClass;
            }
            else {
                continue;
            }

            if (Il2CppLayout::ObjectClass(part) !=
                expectedClass) {
                ++unexpectedClasses;
                continue;
            }

            void* requirement =
                ReadObjectField(
                    part,
                    requirementOffset);

            if (!requirement) {
                ++missingRequirements;
                continue;
            }

            if (Il2CppLayout::ObjectClass(requirement) !=
                targets.socketRequirementClass) {
                ++unexpectedClasses;
                continue;
            }

            void* list =
                ReadObjectField(
                    requirement,
                    targets.socketRequirementListOffset);

            if (!list) {
                ++missingRequirements;
                continue;
            }

            if (!listMethodsResolved) {
                if (!ResolveListMethods(
                    api,
                    list,
                    listMethods,
                    log)) {
                    m_disabled = true;
                    return false;
                }

                listMethodsResolved = true;
            }

            const int additionsBefore =
                additions;

            if (!AddSocketsToList(
                list,
                sockets,
                listMethods,
                additions,
                log)) {
                m_disabled = true;
                return false;
            }

            if (additions != additionsBefore)
                ++updatedParts;
        }

        if (!listMethodsResolved) {
            log.Error(
                "CoolerPatch: no usable cooler "
                "socket lists were found");

            m_disabled = true;
            return false;
        }

        log.Info(
            "CoolerPatch: found " +
            std::to_string(airCoolers) +
            " air cooler(s), " +
            std::to_string(liquidCoolers) +
            " liquid cooler(s), and " +
            std::to_string(cpuBlocks) +
            " CPU water block(s)");

        log.Info(
            "CoolerPatch: updated " +
            std::to_string(updatedParts) +
            " part(s), added " +
            std::to_string(additions) +
            " socket compatibility entries");

        if (missingRequirements > 0) {
            log.Warn(
                "CoolerPatch: skipped " +
                std::to_string(missingRequirements) +
                " part(s) with missing socket requirements");
        }

        if (unexpectedClasses > 0) {
            log.Warn(
                "CoolerPatch: skipped " +
                std::to_string(unexpectedClasses) +
                " part(s) with unexpected IL2CPP classes");
        }

        m_done = true;

        log.Info(
            "CoolerPatch: completed successfully");

        return true;
    }

}