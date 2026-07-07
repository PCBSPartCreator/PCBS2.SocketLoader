#pragma once

#include "Il2CppApi.h"
#include "SocketResolver.h"
#include "Config.h"

namespace SocketLoader {

    class Logger;

    // hooks FilterHolder.SocketFilter so custom sockets show up and match in the shop filter
    class FilterHook {
    public:
        bool Install(const Il2CppApi& api, const SocketTargetHandles& targets,
            const std::vector<SocketEntry>& sockets, Logger& log);

    private:
        bool m_installed = false;
    };

}