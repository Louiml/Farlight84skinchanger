#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace flpak
{
    struct PakFooter
    {
        bool parsed = false;
        uint32_t version = 0;
        uint64_t value1 = 0;
        uint64_t value2 = 0;
        uint64_t value3 = 0;
        uint8_t hash[20]{};
        uint8_t flag = 0;
        std::string methods;
    };

    struct PakEntry
    {
        std::string name;
        uint64_t size = 0;
        PakFooter footer;
    };

    bool ParseFooter(const std::string& path, PakFooter& out);
    std::vector<PakEntry> ScanDirectory(const std::string& dir);
}
