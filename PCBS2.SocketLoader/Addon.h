#pragma once

#include "Paths.h"
#include "Log.h"
#include "Config.h"
#include "Runtime.h"
#include "SocketResolver.h"
#include "SocketPatcher.h"
#include "FilterProbe.h"
#include "HookManager.h"
#include "FilterHook.h"

namespace SocketLoader {

    class Addon {
    public:
        static Addon& Get();

        static const char* Name();
        static const char* Version();

        bool Initialize(const char* gameDir);
        void Tick();
        void Shutdown();

    private:
        Addon() = default;
        Addon(const Addon&) = delete;
        Addon& operator=(const Addon&) = delete;

        bool               m_initialized = false;
        Paths              m_paths;
        Logger             m_log;
        Config             m_config;
        Runtime            m_runtime;
        SocketResolver     m_socketResolver;
        SocketPatcher      m_socketPatcher;
        FilterProbe        m_filterProbe;
        FilterHook         m_filterHook;
        unsigned long long m_tickCount = 0;
        bool               m_targetsLogged = false;
        bool               m_resolveAttempted = false;
        bool               m_hookInit = false;
        bool               m_hookReady = false;
        bool               m_filterHookInstalled = false;
        bool               m_settled = false;
    };

}