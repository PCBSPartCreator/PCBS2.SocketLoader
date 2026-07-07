#pragma once

#include <windows.h>
#include <string>
#include <ctime>

namespace SocketLoader {

    class Logger {
    public:
        Logger() { InitializeCriticalSection(&m_cs); }
        ~Logger() { Close(); DeleteCriticalSection(&m_cs); }

        bool Open(const std::string& path) {
            m_file = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            return m_file != INVALID_HANDLE_VALUE;
        }

        void Info(const std::string& msg) { Write("INFO", msg); }
        void Warn(const std::string& msg) { Write("WARN", msg); }
        void Error(const std::string& msg) { Write("ERROR", msg); }

        bool IsOpen() const { return m_file != INVALID_HANDLE_VALUE; }

        void Close() {
            if (m_file != INVALID_HANDLE_VALUE) {
                CloseHandle(m_file);
                m_file = INVALID_HANDLE_VALUE;
            }
        }

        // no prefix - banners and separators
        void Raw(const std::string& msg) { WriteLine(msg + "\n"); }

    private:
        void Write(const char* level, const std::string& msg) {
            WriteLine(Timestamp() + " [" + level + "] " + msg + "\n");
        }

        // the SocketFilter detour logs from the game thread, so writes can race the tick thread
        void WriteLine(const std::string& line) {
            EnterCriticalSection(&m_cs);
            if (m_file != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(m_file, line.data(), (DWORD)line.size(), &written, nullptr);
            }
            LeaveCriticalSection(&m_cs);
        }

        static std::string Timestamp() {
            std::time_t t = std::time(nullptr);
            std::tm tm{};
            localtime_s(&tm, &t);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            return std::string(buf);
        }

        CRITICAL_SECTION m_cs;
        HANDLE m_file = INVALID_HANDLE_VALUE;
    };

}