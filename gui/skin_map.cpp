#include "skin_map.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace
{
    using nlohmann::json;

    void LoadInto(const json& parent, const char* key, std::unordered_map<std::string, std::string>& out)
    {
        const auto it = parent.find(key);
        if (it == parent.end() || !it->is_object())
        {
            return;
        }
        for (auto item = it->begin(); item != it->end(); ++item)
        {
            if (item.value().is_string())
            {
                out[item.key()] = item.value().get<std::string>();
            }
        }
    }

    json DumpFrom(const std::unordered_map<std::string, std::string>& in)
    {
        json obj = json::object();
        for (const auto& [k, v] : in)
        {
            obj[k] = v;
        }
        return obj;
    }
}

namespace flskin
{
    bool Load(const std::string& path, SkinMap& map)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return false;
        }
        try
        {
            const json j = json::parse(file);
            LoadInto(j, "hero_skins", map.heroSkins);
            LoadInto(j, "weapon_skins", map.weaponSkins);
            LoadInto(j, "vehicle_skins", map.vehicleSkins);
        }
        catch (const std::exception&)
        {
            return false;
        }
        return true;
    }

    bool Save(const std::string& path, const SkinMap& map)
    {
        try
        {
            json j;
            j["hero_skins"] = DumpFrom(map.heroSkins);
            j["weapon_skins"] = DumpFrom(map.weaponSkins);
            j["vehicle_skins"] = DumpFrom(map.vehicleSkins);

            std::ofstream file(path);
            if (!file.is_open())
            {
                return false;
            }
            file << j.dump(4) << std::endl;
        }
        catch (const std::exception&)
        {
            return false;
        }
        return true;
    }
}
