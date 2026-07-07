#include "FilterProbe.h"
#include "Log.h"

#include <string>
#include <cstdint>

namespace SocketLoader {

    namespace {

        const uintptr_t kArrayHeader = 0x20;

        const char* kFilterNamespace = "PCBS2.UI.Filters";
        const char* kFilterClass = "FilterHolder";
        const char* kSocketsField = "motherboardSockets";

        // il2cpp System.String (x64): length at 0x10, utf-16 chars at 0x14
        std::string ReadString(void* strObj) {
            if (!strObj)
                return "(null)";
            int len = *(int*)((uintptr_t)strObj + 0x10);
            if (len < 0 || len > 512)
                return "(bad len)";
            const uint16_t* chars = (const uint16_t*)((uintptr_t)strObj + 0x14);
            std::string out;
            for (int i = 0; i < len; ++i) {
                uint16_t c = chars[i];
                out += (c >= 32 && c < 127) ? (char)c : '?';
            }
            return out;
        }

        void LogSocketName(const Il2CppApi& api, void* namesField, int index, Logger& log) {
            if (!namesField)
                return;
            void* arr = nullptr;
            api.field_static_get_value(namesField, &arr);
            if (!arr) {
                log.Info("FilterProbe: s_names is null");
                return;
            }
            uintptr_t len = api.array_length(arr);
            std::string name = "(out of range)";
            if (index >= 0 && (uintptr_t)index < len) {
                void* strObj = *(void**)((uintptr_t)arr + kArrayHeader + (uintptr_t)index * 8);
                name = ReadString(strObj);
            }
            log.Info("FilterProbe: s_names len=" + std::to_string(len) +
                " [" + std::to_string(index) + "]=\"" + name + "\"");
        }

    }

    void FilterProbe::Poll(const Il2CppApi& api, const SocketTargetHandles& targets, Logger& log) {
        if (m_disabled)
            return;

        if (!api.class_from_name || !api.class_get_field_from_name ||
            !api.field_static_get_value || !api.array_length) {
            log.Error("FilterProbe: required IL2CPP exports missing, disabled");
            m_disabled = true;
            return;
        }
        if (!targets.image) {
            log.Error("FilterProbe: no assembly image, disabled");
            m_disabled = true;
            return;
        }

        if (!m_resolved) {
            void* klass = api.class_from_name(targets.image, kFilterNamespace, kFilterClass);
            if (!klass) {
                log.Error(std::string("FilterProbe: class not found: ") + kFilterNamespace + "." + kFilterClass);
                m_disabled = true;
                return;
            }
            m_field = api.class_get_field_from_name(klass, kSocketsField);
            if (!m_field) {
                log.Error(std::string("FilterProbe: field not found: ") + kSocketsField);
                m_disabled = true;
                return;
            }
            log.Info(std::string("FilterProbe: resolved ") + kFilterClass + "." + kSocketsField +
                ", waiting for filter to open");
            m_resolved = true;
        }

        void* arr = nullptr;
        api.field_static_get_value(m_field, &arr);
        if (!arr)  // filter not built yet
            return;

        uintptr_t len = api.array_length(arr);
        if (arr == m_lastArr && len == m_lastLen)  // nothing changed since last log
            return;

        m_lastArr = arr;
        m_lastLen = len;

        std::string values;
        // CpuSocket is an int enum - 4 bytes each
        const uintptr_t cap = len < 128 ? len : 128;
        for (uintptr_t i = 0; i < cap; ++i) {
            int v = *(int*)((uintptr_t)arr + kArrayHeader + i * 4);
            if (!values.empty())
                values += ", ";
            values += std::to_string(v);
        }
        if (len > cap)
            values += ", ...";

        log.Info("FilterProbe: motherboardSockets len=" + std::to_string(len) + " values=[" + values + "]");

        LogSocketName(api, targets.sNamesField, 100, log);
    }

}