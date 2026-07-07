#include "SocketPatcher.h"
#include "Log.h"

#include <string>
#include <cstring>
#include <cstdint>

namespace SocketLoader {

    namespace {

        const uintptr_t kArrayHeader = 0x20;  // il2cpp array data starts after the 0x20 header (x64)

        int MaxEnabledId(const std::vector<SocketEntry>& sockets) {
            int maxId = -1;
            for (size_t i = 0; i < sockets.size(); ++i) {
                if (sockets[i].enabled && sockets[i].id > maxId)
                    maxId = sockets[i].id;
            }
            return maxId;
        }

        void* ElementClassOf(const Il2CppApi& api, void* array) {
            if (!array)
                return nullptr;
            void* arrayClass = *(void**)array;  // klass ptr at offset 0
            if (!arrayClass)
                return nullptr;
            return api.class_get_element_class(arrayClass);
        }

        uintptr_t NewLength(uintptr_t oldLen, int maxId) {
            uintptr_t want = (maxId >= 0) ? (uintptr_t)(maxId + 1) : 0;
            return want > oldLen ? want : oldLen;
        }

        bool PatchNamesArray(const Il2CppApi& api, void* field, void* curArr,
            const std::vector<SocketEntry>& sockets, Logger& log) {
            const uintptr_t elemSize = 8;
            uintptr_t oldLen = api.array_length(curArr);
            uintptr_t newLen = NewLength(oldLen, MaxEnabledId(sockets));
            log.Info("Patch: s_names oldLen=" + std::to_string(oldLen) + " newLen=" + std::to_string(newLen));

            void* elemClass = ElementClassOf(api, curArr);
            if (!elemClass) { log.Error("Patch: s_names element class unavailable"); return false; }

            void* newArr = api.array_new(elemClass, newLen);
            if (!newArr) { log.Error("Patch: failed to allocate new s_names array"); return false; }

            std::memcpy((void*)((uintptr_t)newArr + kArrayHeader),
                (void*)((uintptr_t)curArr + kArrayHeader),
                (size_t)(oldLen * elemSize));

            // fill the gap ids with "" so nothing stays null
            void* emptyStr = api.string_new("");
            for (uintptr_t i = oldLen; i < newLen; ++i)
                *(void**)((uintptr_t)newArr + kArrayHeader + i * elemSize) = emptyStr;

            for (size_t i = 0; i < sockets.size(); ++i) {
                const SocketEntry& e = sockets[i];
                if (!e.enabled) {
                    log.Info("Patch: skip disabled socket [" + e.section + "]");
                    continue;
                }
                if (e.id < 0 || (uintptr_t)e.id >= newLen) {
                    log.Warn("Patch: socket id out of range: " + std::to_string(e.id));
                    continue;
                }
                void* str = api.string_new(e.displayName.c_str());
                *(void**)((uintptr_t)newArr + kArrayHeader + (uintptr_t)e.id * elemSize) = str;
                log.Info("Patch: s_names[" + std::to_string(e.id) + "] = \"" + e.displayName + "\"");
            }

            api.field_static_set_value(field, newArr);

            void* check = nullptr;
            api.field_static_get_value(field, &check);
            if (check == newArr) {
                log.Info("Patch: s_names updated and verified");
                return true;
            }
            log.Error("Patch: s_names verify failed (field not updated)");
            return false;
        }

        bool PatchUsedArray(const Il2CppApi& api, void* field, void* curArr,
            const std::vector<SocketEntry>& sockets, Logger& log) {
            const uintptr_t elemSize = 1;
            uintptr_t oldLen = api.array_length(curArr);
            uintptr_t newLen = NewLength(oldLen, MaxEnabledId(sockets));
            log.Info("Patch: s_used oldLen=" + std::to_string(oldLen) + " newLen=" + std::to_string(newLen));

            void* elemClass = ElementClassOf(api, curArr);
            if (!elemClass) { log.Error("Patch: s_used element class unavailable"); return false; }

            void* newArr = api.array_new(elemClass, newLen);
            if (!newArr) { log.Error("Patch: failed to allocate new s_used array"); return false; }

            std::memcpy((void*)((uintptr_t)newArr + kArrayHeader),
                (void*)((uintptr_t)curArr + kArrayHeader),
                (size_t)(oldLen * elemSize));

            for (size_t i = 0; i < sockets.size(); ++i) {
                const SocketEntry& e = sockets[i];
                if (!e.enabled)
                    continue;
                if (e.id < 0 || (uintptr_t)e.id >= newLen)
                    continue;
                *(uint8_t*)((uintptr_t)newArr + kArrayHeader + (uintptr_t)e.id * elemSize) = 1;
                log.Info("Patch: s_used[" + std::to_string(e.id) + "] = true");
            }

            api.field_static_set_value(field, newArr);

            void* check = nullptr;
            api.field_static_get_value(field, &check);
            if (check == newArr) {
                log.Info("Patch: s_used updated and verified");
                return true;
            }
            log.Error("Patch: s_used verify failed (field not updated)");
            return false;
        }

    }

    bool SocketPatcher::Poll(const Il2CppApi& api, const SocketTargetHandles& targets,
        const std::vector<SocketEntry>& sockets, Logger& log) {
        if (m_done || m_disabled)
            return m_done;

        if (!api.class_get_element_class || !api.array_new || !api.array_length ||
            !api.string_new || !api.field_static_get_value || !api.field_static_set_value) {
            log.Error("Patch: required IL2CPP exports missing, patching disabled");
            m_disabled = true;
            return false;
        }
        if (!targets.sNamesField || !targets.sUsedField) {
            log.Error("Patch: socket array fields not resolved, patching disabled");
            m_disabled = true;
            return false;
        }
        if (MaxEnabledId(sockets) < 0) {
            log.Info("Patch: no enabled custom sockets, nothing to patch");
            m_disabled = true;
            return false;
        }

        // arrays are filled by a static ctor that can run after resolve, so wait for them
        void* namesArr = nullptr;
        void* usedArr = nullptr;
        api.field_static_get_value(targets.sNamesField, &namesArr);
        api.field_static_get_value(targets.sUsedField, &usedArr);
        if (!namesArr || !usedArr) {
            if (!m_loggedWaiting) {
                log.Info("Patch: waiting for socket arrays to populate");
                m_loggedWaiting = true;
            }
            if (++m_pollCount > 6000) {
                log.Error("Patch: socket arrays never populated, giving up");
                m_disabled = true;
            }
            return false;
        }

        log.Info("Patch: applying custom sockets (PatchEnabled=true)");
        bool okNames = PatchNamesArray(api, targets.sNamesField, namesArr, sockets, log);
        bool okUsed = PatchUsedArray(api, targets.sUsedField, usedArr, sockets, log);

        m_done = true;
        if (okNames && okUsed)
            log.Info("Patch: custom sockets applied successfully");
        else
            log.Warn("Patch: completed with errors (see above)");
        return true;
    }

}