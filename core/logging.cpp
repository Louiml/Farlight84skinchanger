#include "logging.h"

#include <windows.h>

#include <cstdio>
#include <cstdarg>
#include <share.h>

namespace
{
    FILE* g_log = nullptr;
}

namespace fl
{
    void InitLog(const std::string& path)
    {
        if (g_log)
        {
            std::fclose(g_log);
        }
        g_log = _fsopen(path.c_str(), "a", _SH_DENYNO);
    }

    void Log(const char* fmt, ...)
    {
        if (!g_log)
        {
            return;
        }

        SYSTEMTIME st{};
        GetLocalTime(&st);
        std::fprintf(g_log, "[%02u:%02u:%02u] ", st.wHour, st.wMinute, st.wSecond);

        va_list ap{};
        va_start(ap, fmt);
        std::vfprintf(g_log, fmt, ap);
        va_end(ap);

        std::fputc('\n', g_log);
        std::fflush(g_log);
    }
}
