#pragma once

#include <string>
#include <vector>
#include <utility>

namespace SocketLoader {

    class IniSection {
    public:
        std::string name;

        void Set(const std::string& key, const std::string& value);
        bool Has(const std::string& key) const;
        std::string GetString(const std::string& key, const std::string& def = "") const;

        const std::vector<std::pair<std::string, std::string>>& Values() const { return m_values; }

    private:
        std::vector<std::pair<std::string, std::string>> m_values;
    };

    class IniFile {
    public:
        bool Load(const std::string& path);

        const std::vector<IniSection>& Sections() const { return m_sections; }

    private:
        std::vector<IniSection> m_sections;
    };

    std::string IniTrim(const std::string& s);
    bool IniEquals(const std::string& a, const std::string& b);  // case-insensitive
    bool IniParseBool(const std::string& s, bool& out);  // true/false/1/0/yes/no

}