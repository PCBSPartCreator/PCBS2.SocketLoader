#pragma once

#include "Il2CppApi.h"
#include "SocketResolver.h"
#include "Config.h"

namespace SocketLoader {

    class Logger;

    class CoolerPatcher {
    public:
        bool Poll(
            const Il2CppApi& api,
            const SocketTargetHandles& targets,
            const std::vector<SocketEntry>& sockets,
            Logger& log);

        bool IsDone() const { return m_done; }
        bool Finished() const { return m_done || m_disabled; }

    private:
        bool m_done = false;
        bool m_disabled = false;
        bool m_loggedWaitingForDatabase = false;
        bool m_loggedWaitingForParts = false;

        uintptr_t m_lastPartCount = 0;

        unsigned long long m_waitStartedAt = 0;
        unsigned long long m_stableSince = 0;
        unsigned long long m_nextPollAt = 0;
    };

}