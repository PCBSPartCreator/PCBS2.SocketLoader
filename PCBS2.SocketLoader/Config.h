#pragma once

#include "Paths.h"
#include "Log.h"

#include <string>
#include <vector>

namespace SocketLoader {

    struct AddonConfig {
        bool enableSockets = true;
        bool logConfig = false;
        bool logResolveSteps = false;
        bool resolveEnabled = true;
        bool patchEnabled = true;
        bool patchCoolerCompatibility = true;
        bool hookFilter = true;
        bool debugFilter = false;
    };

    struct SocketEntry {
        std::string section;
        int         id = -1;
        std::string displayName;
        bool        enabled = false;
        bool        valid = false;
    };

    struct Config {
        AddonConfig              addon;
        std::vector<SocketEntry> sockets;
    };

    void LoadConfig(const Paths& paths, Logger& log, Config& out);

}