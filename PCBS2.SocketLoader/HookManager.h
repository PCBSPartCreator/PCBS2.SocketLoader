#pragma once

namespace SocketLoader {

	class Logger;

	namespace Hook {

		bool Init(Logger& log);

		bool Create(void* target, void* detour, void** original, const char* name, Logger& log);

		bool Enable(void* target, const char* name, Logger& log);

		void Shutdown(Logger& log);

	}

}