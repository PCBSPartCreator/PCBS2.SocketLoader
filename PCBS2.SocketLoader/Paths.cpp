#include "Paths.h"

#include <windows.h>

namespace SocketLoader {

    namespace {

        std::string WithTrailingBackslash(std::string p) {
            for (size_t i = 0; i < p.size(); ++i) {
                if (p[i] == '/')
                    p[i] = '\\';
            }
            if (!p.empty() && p.back() != '\\')
                p.push_back('\\');
            return p;
        }

    }

    Paths Paths::FromGameDir(const char* gameDir) {
        Paths p;
        p.gameDir = WithTrailingBackslash(gameDir ? gameDir : "");
        p.addonDir = p.gameDir + "addons\\PCBS2.SocketLoader\\";
        p.logFile = p.addonDir + "PCBS2.SocketLoader.log";
        p.configFile = p.addonDir + "PCBS2.SocketLoader.ini";
        p.socketsFile = p.addonDir + "sockets.ini";
        return p;
    }

    bool EnsureDirectory(const std::string& dir) {
        if (dir.empty())
            return false;

        std::string current;
        current.reserve(dir.size());

        for (size_t i = 0; i < dir.size(); ++i) {
            const char c = dir[i];
            current.push_back(c);

            if (c == '\\' || c == '/') {
                if (current.size() > 3) {  // skip drive roots like C:
                    std::string segment = current.substr(0, current.size() - 1);
                    CreateDirectoryA(segment.c_str(), nullptr);
                }
            }
        }

        if (dir.back() != '\\' && dir.back() != '/')
            CreateDirectoryA(dir.c_str(), nullptr);

        std::string check = dir;
        while (!check.empty() && (check.back() == '\\' || check.back() == '/'))
            check.pop_back();

        const DWORD attr = GetFileAttributesA(check.c_str());
        return attr != INVALID_FILE_ATTRIBUTES &&
            (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

}