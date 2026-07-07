#pragma once

#include <string>

namespace SocketLoader {

    struct Paths {
        std::string gameDir;
        std::string addonDir;  // <game>\addons\PCBS2.SocketLoader
        std::string logFile;  // addonDir + PCBS2.SocketLoader.log
        std::string configFile;  // addonDir + PCBS2.SocketLoader.ini
        std::string socketsFile;  // addonDir + sockets.ini

        static Paths FromGameDir(const char* gameDir);
    };

    bool EnsureDirectory(const std::string& dir);

}