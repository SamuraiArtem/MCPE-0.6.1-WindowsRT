#include "log.h"

#if defined(WIN32)

#include <cstdarg>
#include <cstdio>
#include <ctime>

// The app is built as a GUI app (-mwindows), so stdout/stderr go
// nowhere. Everything that used to be printed is written to
// mcpe_log.txt so a crash (or a hang) can actually be diagnosed from the
// device.
static const long LOG_MAX_BYTES = 2 * 1024 * 1024;

void winLog(const char* fmt, ...) {
	static FILE* logFile = 0;
	static bool checkedSize = false;

	if (!logFile) {
		logFile = fopen("mcpe_log.txt", "w");
		if (!logFile)
			return;
		setvbuf(logFile, 0, _IOLBF, 4096);
	}

	if (!checkedSize) {
		checkedSize = true;
		fseek(logFile, 0, SEEK_END);
		if (ftell(logFile) > LOG_MAX_BYTES) {
			fclose(logFile);
			logFile = fopen("mcpe_log.txt", "w");
			if (!logFile)
				return;
			setvbuf(logFile, 0, _IOLBF, 4096);
			fprintf(logFile, "--- log truncated ---\n");
		}
	}

	va_list args;
	va_start(args, fmt);
	vfprintf(logFile, fmt, args);
	va_end(args);
	fflush(logFile);
}

#endif // WIN32
