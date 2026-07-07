#pragma once

#include <windows.h>

#include "Il2CppApi.h"

namespace SocketLoader {

    class Logger;

    class Runtime {
    public:
        enum class Phase {
            WaitingForModule,  // GameAssembly.dll not loaded yet
            ResolvingApi,  // resolving il2cpp exports
            AttachingThread,  // attaching the tick thread
            Ready,  // safe to call il2cpp from the tick
            Failed
        };

        void Poll(Logger& log);

        Phase            GetPhase() const { return m_phase; }
        bool             IsReady()  const { return m_phase == Phase::Ready; }
        const Il2CppApi& Api()      const { return m_api; }

    private:
        Phase     m_phase = Phase::WaitingForModule;
        HMODULE   m_gameAssembly = nullptr;
        Il2CppApi m_api;
        int       m_pollCount = 0;
        bool      m_loggedWaiting = false;
    };

}