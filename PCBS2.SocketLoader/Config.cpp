#include "Config.h"
#include "Ini.h"

#include <windows.h>
#include <cstdlib>
#include <cctype>

namespace SocketLoader {

    namespace {

        bool FileExists(const std::string& path) {
            DWORD attr = GetFileAttributesA(path.c_str());
            return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }

        bool WriteTextFile(const std::string& path, const std::string& content) {
            HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE)
                return false;
            DWORD written = 0;
            BOOL ok = WriteFile(h, content.data(), (DWORD)content.size(), &written, nullptr);
            CloseHandle(h);
            return ok && written == content.size();
        }

        const char* kDefaultAddonIni =
            ";===============================================\r\n"
            ";  PCBS2.SocketLoader - Config\r\n"
            ";===============================================\r\n"
            "\r\n"
            "[General]\r\n"
            "EnableSockets=true\r\n"
            "LogConfig=false\r\n"
            "\r\n"
            "[Runtime]\r\n"
            "ResolveEnabled=true\r\n"
            "PatchEnabled=true\r\n"
            "PatchCoolerCompatibility=true\r\n"
            "HookFilter=true\r\n"
            "LogResolveSteps=false\r\n"
            "DebugFilter=false\r\n";

        const char* kDefaultSocketsIni =
            ";===============================================\r\n"
            ";  Custom CPU / Mainboard sockets\r\n"
            ";===============================================\r\n"
            "; One line per socket:  <Id> = <DisplayName>\r\n"
            "; Id must be 100-999. Comment a line out (;) to disable it.\r\n"
            "\r\n"
            "[Sockets]\r\n"
            ";100 = AM6\r\n";

        bool ParseInt(const std::string& s, int& out) {
            if (s.empty())
                return false;
            char* end = nullptr;
            long v = std::strtol(s.c_str(), &end, 10);
            if (end == s.c_str() || *end != '\0')
                return false;
            out = (int)v;
            return true;
        }

        const char* BoolText(bool b) { return b ? "true" : "false"; }

        void EnsureConfigFiles(const Paths& paths, Logger& log) {
            EnsureDirectory(paths.addonDir);

            if (!FileExists(paths.configFile)) {
                if (WriteTextFile(paths.configFile, kDefaultAddonIni))
                    log.Info("Created default config: " + paths.configFile);
                else
                    log.Error("Failed to create config: " + paths.configFile);
            }
            if (!FileExists(paths.socketsFile)) {
                if (WriteTextFile(paths.socketsFile, kDefaultSocketsIni))
                    log.Info("Created default sockets config: " + paths.socketsFile);
                else
                    log.Error("Failed to create sockets config: " + paths.socketsFile);
            }
        }

        void LoadAddonConfig(const std::string& path, Logger& log, AddonConfig& cfg) {
            IniFile ini;
            if (!ini.Load(path)) {
                log.Warn("Config not readable, using defaults: " + path);
                return;
            }

            for (size_t i = 0; i < ini.Sections().size(); ++i) {
                const IniSection& s = ini.Sections()[i];
                bool b = false;

                if (IniEquals(s.name, "General")) {
                    if (s.Has("EnableSockets") && IniParseBool(s.GetString("EnableSockets"), b))     cfg.enableSockets = b;
                    if (s.Has("LogConfig") && IniParseBool(s.GetString("LogConfig"), b))         cfg.logConfig = b;
                }
                else if (IniEquals(s.name, "Runtime")) {
                    if (s.Has("LogResolveSteps") &&
                        IniParseBool(s.GetString("LogResolveSteps"), b))
                        cfg.logResolveSteps = b;

                    if (s.Has("ResolveEnabled") &&
                        IniParseBool(s.GetString("ResolveEnabled"), b))
                        cfg.resolveEnabled = b;

                    if (s.Has("DebugFilter") &&
                        IniParseBool(s.GetString("DebugFilter"), b))
                        cfg.debugFilter = b;

                    if (s.Has("HookFilter") &&
                        IniParseBool(s.GetString("HookFilter"), b))
                        cfg.hookFilter = b;

                    if (s.Has("PatchEnabled") &&
                        IniParseBool(s.GetString("PatchEnabled"), b))
                        cfg.patchEnabled = b;

                    if (s.Has("PatchCoolerCompatibility") &&
                        IniParseBool(
                            s.GetString("PatchCoolerCompatibility"),
                            b))
                        cfg.patchCoolerCompatibility = b;
                }
            }

            log.Info(
                std::string("Config: EnableSockets=") +
                BoolText(cfg.enableSockets) +
                " PatchEnabled=" +
                BoolText(cfg.patchEnabled) +
                " PatchCoolerCompatibility=" +
                BoolText(cfg.patchCoolerCompatibility));
        }

        void NormalizeAddonConfig(
            Logger& log,
            AddonConfig& cfg)
        {
            if (!cfg.enableSockets)
                return;

            if (!cfg.resolveEnabled) {
                cfg.patchEnabled = false;
                cfg.patchCoolerCompatibility = false;
                cfg.hookFilter = false;
                cfg.debugFilter = false;

                log.Warn(
                    "Config: ResolveEnabled=false; "
                    "PatchEnabled, PatchCoolerCompatibility, "
                    "HookFilter and DebugFilter were disabled");

                return;
            }

            if (!cfg.patchEnabled &&
                cfg.patchCoolerCompatibility) {
                cfg.patchCoolerCompatibility = false;

                log.Warn(
                    "Config: PatchEnabled=false; "
                    "PatchCoolerCompatibility was disabled");
            }
        }

        void LoadSockets(const std::string& path, Logger& log, bool logEntries, std::vector<SocketEntry>& out) {
            IniFile ini;
            if (!ini.Load(path)) {
                log.Warn("Sockets config not readable: " + path);
                return;
            }

            const IniSection* sec = nullptr;
            for (size_t i = 0; i < ini.Sections().size(); ++i) {
                if (IniEquals(ini.Sections()[i].name, "Sockets")) { sec = &ini.Sections()[i]; break; }
            }
            if (!sec) {
                log.Warn("Sockets config has no [Sockets] section");
                return;
            }

            const std::vector<std::pair<std::string, std::string>>& rows = sec->Values();
            for (size_t i = 0; i < rows.size(); ++i) {
                const std::string& idStr = rows[i].first;   // key is the socket id
                const std::string& name = rows[i].second;  // value is the display name

                int id = -1;
                if (!ParseInt(idStr, id)) {
                    log.Warn("Socket skipped: invalid Id \"" + idStr + "\"");
                    continue;
                }
                if (id < 100 || id > 999) {
                    log.Warn("Socket skipped: Id " + std::to_string(id) + " out of range 100-999");
                    continue;
                }
                if (name.empty()) {
                    log.Warn("Socket [" + idStr + "] skipped: missing name");
                    continue;
                }

                bool dup = false;
                for (size_t j = 0; j < out.size(); ++j) {
                    if (out[j].id == id) { dup = true; break; }
                }
                if (dup) {
                    log.Warn("Socket skipped: duplicate Id " + std::to_string(id));
                    continue;
                }

                SocketEntry e;
                e.section = idStr;
                e.id = id;
                e.displayName = name;
                e.enabled = true;   // listed = enabled; comment the line out to disable
                e.valid = true;
                out.push_back(e);

                if (logEntries)
                    log.Info("Socket ok: Id=" + std::to_string(e.id) + " Name=\"" + e.displayName + "\"");
            }
        }

        void LogConfigSummary(const Config& cfg, Logger& log) {
            log.Raw("");
            log.Raw("------------------- Summary -------------------");
            log.Info(std::string("Sockets:      ") + (cfg.addon.enableSockets ? "enabled" : "disabled") +
                ", " + std::to_string(cfg.sockets.size()) + " valid");
            log.Info(
                std::string("ResolveEnabled:          ") +
                BoolText(cfg.addon.resolveEnabled));

            log.Info(
                std::string("PatchEnabled:            ") +
                BoolText(cfg.addon.patchEnabled));

            log.Info(
                std::string("PatchCoolerCompatibility: ") +
                BoolText(
                    cfg.addon.patchCoolerCompatibility));

            log.Info(
                std::string("HookFilter:              ") +
                BoolText(cfg.addon.hookFilter));

            if (cfg.addon.enableSockets && cfg.sockets.empty())
                log.Warn("Sockets enabled but no valid entries were loaded");

            log.Raw("----------------------------------------------");
        }

    }

    void LoadConfig(const Paths& paths, Logger& log, Config& out) {
        log.Info("Loading configuration");
        EnsureConfigFiles(paths, log);

        LoadAddonConfig(paths.configFile, log, out.addon);
        NormalizeAddonConfig(log, out.addon);

        if (out.addon.enableSockets)
            LoadSockets(paths.socketsFile, log, out.addon.logConfig, out.sockets);
        else
            log.Info("Sockets disabled by config");

        LogConfigSummary(out, log);
    }

}