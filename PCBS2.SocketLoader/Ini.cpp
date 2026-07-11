#include "Ini.h"

#include <windows.h>
#include <cctype>

namespace SocketLoader {

    std::string IniTrim(const std::string& s) {
        const char* ws = " \t\r\n";
        size_t a = s.find_first_not_of(ws);
        if (a == std::string::npos)
            return "";
        size_t b = s.find_last_not_of(ws);
        return s.substr(a, b - a + 1);
    }

    bool IniEquals(const std::string& a, const std::string& b) {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
                return false;
        }
        return true;
    }

    bool IniParseBool(const std::string& s, bool& out) {
        if (IniEquals(s, "true") || IniEquals(s, "1") || IniEquals(s, "yes")) { out = true;  return true; }
        if (IniEquals(s, "false") || IniEquals(s, "0") || IniEquals(s, "no")) { out = false; return true; }
        return false;
    }

    void IniSection::Set(const std::string& key, const std::string& value) {
        for (size_t i = 0; i < m_values.size(); ++i) {
            if (IniEquals(m_values[i].first, key)) {
                m_values[i].second = value;
                return;
            }
        }
        m_values.push_back(std::make_pair(key, value));
    }

    bool IniSection::Has(const std::string& key) const {
        for (size_t i = 0; i < m_values.size(); ++i)
            if (IniEquals(m_values[i].first, key))
                return true;
        return false;
    }

    std::string IniSection::GetString(const std::string& key, const std::string& def) const {
        for (size_t i = 0; i < m_values.size(); ++i)
            if (IniEquals(m_values[i].first, key))
                return m_values[i].second;
        return def;
    }

    bool IniFile::Load(const std::string& path) {
        HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        std::string data;
        char buf[4096];
        DWORD read = 0;
        while (ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0)
            data.append(buf, read);
        CloseHandle(h);

        m_sections.clear();
        int currentIndex = -1;

        size_t pos = 0;
        while (pos < data.size()) {
            size_t nl = data.find('\n', pos);
            size_t len = (nl == std::string::npos ? data.size() : nl) - pos;
            std::string t = IniTrim(data.substr(pos, len));

            if (t.empty() || t[0] == ';' || t[0] == '#') {
                if (nl == std::string::npos) break;
                pos = nl + 1;
                continue;
            }

            if (t[0] == '[') {
                size_t end = t.find(']');
                if (end != std::string::npos) {  // ignore a line with no closing bracket
                    IniSection sec;
                    sec.name = IniTrim(t.substr(1, end - 1));
                    m_sections.push_back(sec);
                    currentIndex = (int)m_sections.size() - 1;
                }
            }
            else {
                size_t eq = t.find('=');
                if (eq != std::string::npos && currentIndex >= 0) {  // skip keys before any section
                    std::string key = IniTrim(t.substr(0, eq));
                    std::string value = IniTrim(t.substr(eq + 1));
                    if (!key.empty())
                        m_sections[currentIndex].Set(key, value);
                }
            }

            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
        return true;
    }

}