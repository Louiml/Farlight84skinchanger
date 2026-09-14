#pragma once

#include <string>

namespace fl
{
    void InitLog(const std::string& path);
    void Log(const char* fmt, ...);
}
