#pragma once

#include "Il2CppApi.h"
#include "SocketResolver.h"
#include "Config.h"

namespace SocketLoader {

    class Logger;

    // registers custom sockets by extending CpuSocketExt.s_names/s_used - no hooks
    class SocketPatcher {
    public:
        bool Poll(const Il2CppApi& api, const SocketTargetHandles& targets,
            const std::vector<SocketEntry>& sockets, Logger& log);

        bool IsDone() const { return m_done; }
        bool Finished() const { return m_done || m_disabled; }

    private:
        bool m_done = false;
        bool m_disabled = false;
        bool m_loggedWaiting = false;
        int  m_pollCount = 0;
    };
}