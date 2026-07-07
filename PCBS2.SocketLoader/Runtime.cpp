#include "Runtime.h"
#include "Log.h"

namespace SocketLoader {

    static const int kMaxModuleWaitPolls = 3000;  // ~frames to wait for GameAssembly.dll

    void Runtime::Poll(Logger& log) {
        switch (m_phase) {
        case Phase::WaitingForModule: {
            if (!m_loggedWaiting) {
                log.Info("Runtime: waiting for GameAssembly.dll");
                m_loggedWaiting = true;
            }

            m_gameAssembly = GetModuleHandleA("GameAssembly.dll");
            if (m_gameAssembly) {
                log.Info("Runtime: GameAssembly.dll found");
                m_phase = Phase::ResolvingApi;
            }
            else if (++m_pollCount >= kMaxModuleWaitPolls) {
                log.Error("Runtime: GameAssembly.dll not found, giving up");
                m_phase = Phase::Failed;
            }
            break;
        }
        case Phase::ResolvingApi: {
            m_phase = m_api.Resolve(m_gameAssembly, log) ? Phase::AttachingThread : Phase::Failed;
            break;
        }
        case Phase::AttachingThread: {
            void* domain = m_api.domain_get();
            if (!domain) {
                log.Error("Runtime: il2cpp_domain_get returned null, cannot attach");
                m_phase = Phase::Failed;
                break;
            }

            // attach the tick thread before any il2cpp call, or the GC crashes ("collecting from unknown thread")
            m_api.thread_attach(domain);
            log.Info("Runtime: tick thread attached to IL2CPP domain");
            m_phase = Phase::Ready;
            break;
        }
        case Phase::Ready:
        case Phase::Failed:
            break;
        }
    }

}