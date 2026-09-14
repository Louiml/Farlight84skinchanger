#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace fl
{
    struct Config
    {
        uint32_t menuKey = 0x21;
        bool heroEnabled = true;
        bool weaponEnabled = true;
        bool vehicleEnabled = true;
        std::unordered_map<std::string, std::string> heroSkins;
        std::unordered_map<std::string, std::string> weaponSkins;
        std::unordered_map<std::string, std::string> vehicleSkins;
        std::unordered_map<std::string, uint64_t> offsets;
    };

    bool LoadConfig(const std::string& path, Config& cfg);
    bool SaveConfig(const std::string& path, const Config& cfg);
}
