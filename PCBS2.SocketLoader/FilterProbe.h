#pragma once

#include "Il2CppApi.h"
#include "SocketResolver.h"

namespace SocketLoader {

    class Logger;

    // read-only: dumps the filter socket array and our s_names entry
    class FilterProbe {
    public:
        void Poll(const Il2CppApi& api, const SocketTargetHandles& targets, Logger& log);

    private:
        bool      m_resolved = false;
        bool      m_disabled = false;
        void* m_field = nullptr;
        void* m_lastArr = nullptr;
        uintptr_t m_lastLen = (uintptr_t)-1;
    };

}