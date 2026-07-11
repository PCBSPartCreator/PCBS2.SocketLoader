#include "Addon.h"

#include <string>

namespace SocketLoader {

    Addon& Addon::Get() {
        static Addon instance;
        return instance;
    }

    const char* Addon::Name() { return "PCBS2.SocketLoader"; }
    const char* Addon::Version() { return "1.0.1"; }

    bool Addon::Initialize(const char* gameDir) {
        if (m_initialized)
            return true;

        m_paths = Paths::FromGameDir(gameDir);

        if (!EnsureDirectory(m_paths.addonDir))  // no log yet if this fails
            return false;

        if (!m_log.Open(m_paths.logFile))
            return false;

        m_log.Raw("==================================================");
        m_log.Raw(std::string(" ") + Name() + " v" + Version());
        m_log.Raw("==================================================");
        m_log.Info("Initialize");
        m_log.Info("Game directory:  " + m_paths.gameDir);
        m_log.Info("Addon directory: " + m_paths.addonDir);
        m_log.Info("Log file:        " + m_paths.logFile);

        LoadConfig(m_paths, m_log, m_config);

        m_initialized = true;
        m_log.Info("Initialize complete");
        return true;
    }

    void Addon::Tick() {
        if (m_settled || !m_initialized)
            return;

        ++m_tickCount;

        if (!m_config.addon.enableSockets || 
            !m_config.addon.resolveEnabled) {
            m_settled = true;
            return;
        }

        m_runtime.Poll(m_log);

        if (m_runtime.GetPhase() == Runtime::Phase::Failed) {
            m_settled = true;
            return;
        }

        if (!m_runtime.IsReady())
            return;

        // one setup step per tick - doing it all in one tick froze a frame for ~30ms
        if (!m_targetsLogged) {
            if (m_config.addon.logResolveSteps)
                m_socketResolver.LogPlannedTargets(m_log);
            m_targetsLogged = true;
            return;
        }

        if (m_config.addon.hookFilter && !m_hookInit) {
            m_hookInit = true;
            m_hookReady = Hook::Init(m_log);

            if (!m_hookReady)
                m_filterHookAttempted = true;

            return;
        }

        if (m_config.addon.resolveEnabled && !m_resolveAttempted) {
            m_resolveAttempted = true;

            if (!m_socketResolver.Resolve(
                    m_runtime.Api(),
                    m_log,
                    m_config.addon.logResolveSteps)) {
                m_settled = true;
            }
            
            return;
        }

        if (m_hookReady && m_config.addon.hookFilter && m_socketResolver.IsResolved() &&
            !m_filterHookAttempted) {
            m_filterHookAttempted = true;
            m_filterHookInstalled = m_filterHook.Install(
                m_runtime.Api(), 
                m_socketResolver.Handles(),
                m_config.sockets, 
                m_log);
            return;
        }

        if (m_config.addon.patchEnabled &&
            m_socketResolver.IsResolved() &&
            !m_socketPatcher.Finished()) {
            m_socketPatcher.Poll(
                m_runtime.Api(),
                m_socketResolver.Handles(),
                m_config.sockets,
                m_log);

            return;
        }

        if (m_config.addon.patchCoolerCompatibility &&
            !m_config.sockets.empty() &&
            m_socketPatcher.Finished() &&
            !m_socketPatcher.IsDone() &&
            !m_coolerPatchSkipped) {
            m_coolerPatchSkipped = true;

            m_log.Warn(
                "CoolerPatch: skipped because the core "
                "socket patch did not complete successfully");

            return;
        }

        if (m_config.addon.patchCoolerCompatibility &&
            !m_config.sockets.empty() &&
            m_socketPatcher.IsDone() &&
            !m_coolerPatcher.Finished()) {
            m_coolerPatcher.Poll(
                m_runtime.Api(),
                m_socketResolver.Handles(),
                m_config.sockets,
                m_log);

            return;
        }

        if (m_config.addon.debugFilter &&
            m_socketResolver.IsResolved()) {
            m_filterProbe.Poll(
                m_runtime.Api(),
                m_socketResolver.Handles(),
                m_log);
        }

        // setup is one-time; XPL ticks this per il2cpp_runtime_invoke, so go idle once nothing is left
        if (!m_config.addon.debugFilter
            && (!m_config.addon.resolveEnabled ||
                m_resolveAttempted)
            && (!m_config.addon.patchEnabled ||
                m_socketPatcher.Finished())
            && (!m_config.addon.patchCoolerCompatibility ||
                m_config.sockets.empty() ||
                m_coolerPatcher.Finished() ||
                m_coolerPatchSkipped)
            && (!m_config.addon.hookFilter ||
                m_config.sockets.empty() ||
                m_filterHookAttempted ||
                (m_resolveAttempted &&
                    !m_socketResolver.IsResolved())))
        {
            m_settled = true;
        }
    }

    void Addon::Shutdown() {
        if (m_initialized) {
            m_log.Info("Shutdown");
            m_log.Info("Total ticks: " + std::to_string(m_tickCount));
            m_initialized = false;
        }
        m_log.Close();
    }

}