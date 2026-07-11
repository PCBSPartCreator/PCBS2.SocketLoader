#include "FilterProbe.h"
#include "Log.h"

#include <string>
#include <cstdint>

namespace SocketLoader {

    namespace {

        const char* kFilterNamespace = "PCBS2.UI.Filters";
        const char* kFilterClass = "FilterHolder";
        const char* kSocketsField = "motherboardSockets";

        // il2cpp System.String (x64): length at 0x10, utf-16 chars at 0x14
        std::string ReadString(void* strObj) {
            if (!strObj)
                return "(null)";

            const int32_t len =
                Il2CppLayout::StringLength(strObj);

            if (len < 0 || len > 512)
                return "(bad len)";

            const uint16_t* chars =
                Il2CppLayout::StringChars(strObj);

            if (!chars)
                return "(bad data)";

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
            const uintptr_t len = api.array_length(arr);
            std::string name = "(out of range)";

            if (!Il2CppLayout::ArrayLayoutMatches(arr, len)) {
                log.Error(
                    "FilterProbe: s_names uses an unexpected "
                    "IL2CPP array layout");
                return;
            }

            if (index >= 0 &&
                static_cast<uintptr_t>(index) < len) {
                void* const* data =
                    Il2CppLayout::ArrayData<void*>(arr);

                void* strObj =
                    data[static_cast<uintptr_t>(index)];

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

        const uintptr_t len = api.array_length(arr);

        if (!Il2CppLayout::ArrayLayoutMatches(arr, len)) {
            log.Error(
                "FilterProbe: motherboardSockets uses an unexpected "
                "IL2CPP array layout");
            m_disabled = true;
            return;
        }

        if (arr == m_lastArr && len == m_lastLen)
            return;

        m_lastArr = arr;
        m_lastLen = len;

        std::string values;

        const uintptr_t cap = len < 128 ? len : 128;
        const int* const data =
            Il2CppLayout::ArrayData<int>(arr);

        for (uintptr_t i = 0; i < cap; ++i) {
            const int v = data[i];
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