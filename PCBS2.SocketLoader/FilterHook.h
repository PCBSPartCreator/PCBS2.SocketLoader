#pragma once

#include "Config.h"
#include "Il2CppApi.h"
#include "SocketResolver.h"

namespace SocketLoader {

    class Logger;

    class FilterHook {
    public:
        bool Install(
            const Il2CppApi& api,
            const SocketTargetHandles& targets,
            const std::vector<SocketEntry>& sockets,
            Logger& log);

    private:
        bool m_installed = false;
    };

}