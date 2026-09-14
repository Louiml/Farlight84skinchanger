#pragma once

#include <string>

namespace flenv
{
    void Load();
    std::string Get(const std::string& key, const std::string& fallback);
}
