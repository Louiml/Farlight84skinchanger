#pragma once

#include <string>
#include <unordered_map>

namespace flskin
{
    struct SkinMap
    {
        std::unordered_map<std::string, std::string> heroSkins;
        std::unordered_map<std::string, std::string> weaponSkins;
        std::unordered_map<std::string, std::string> vehicleSkins;
    };

    bool Load(const std::string& path, SkinMap& map);
    bool Save(const std::string& path, const SkinMap& map);
}
