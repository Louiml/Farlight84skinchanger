#include "config.h"

#include "logging.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <string>

namespace
{
    using nlohmann::json;

    void LoadStringMap(const json& parent, const char* key, std::unordered_map<std::string, std::string>& out)
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

    void SaveStringMap(json& parent, const char* key, const std::unordered_map<std::string, std::string>& in)
    {
        json obj = json::object();
        for (const auto& [k, v] : in)
        {
            obj[k] = v;
        }
        parent[key] = obj;
    }
}

namespace fl
{
    bool LoadConfig(const std::string& path, Config& cfg)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            Log("config: '%s' not found, defaults in effect", path.c_str());
            return false;
        }

        try
        {
            const json j = json::parse(file);
            cfg.menuKey = j.value("menu_key", cfg.menuKey);
            cfg.heroEnabled = j.value("hero_enabled", true);
            cfg.weaponEnabled = j.value("weapon_enabled", true);
            cfg.vehicleEnabled = j.value("vehicle_enabled", true);
            LoadStringMap(j, "hero_skins", cfg.heroSkins);
            LoadStringMap(j, "weapon_skins", cfg.weaponSkins);
            LoadStringMap(j, "vehicle_skins", cfg.vehicleSkins);

            const auto offsets = j.find("offsets");
            if (offsets != j.end() && offsets->is_object())
            {
                for (auto item = offsets->begin(); item != offsets->end(); ++item)
                {
                    cfg.offsets[item.key()] =
                        std::stoull(item.value().get<std::string>(), nullptr, 0);
                }
            }
        }
        catch (const std::exception& e)
        {
            Log("config: parse error: %s", e.what());
            return false;
        }

        Log("config: loaded '%s' (%zu offsets, %zu hero, %zu weapon, %zu vehicle)",
            path.c_str(), cfg.offsets.size(), cfg.heroSkins.size(), cfg.weaponSkins.size(),
            cfg.vehicleSkins.size());
        return true;
    }

    bool SaveConfig(const std::string& path, const Config& cfg)
    {
        try
        {
            json j;
            j["menu_key"] = cfg.menuKey;
            j["hero_enabled"] = cfg.heroEnabled;
            j["weapon_enabled"] = cfg.weaponEnabled;
            j["vehicle_enabled"] = cfg.vehicleEnabled;
            SaveStringMap(j, "hero_skins", cfg.heroSkins);
            SaveStringMap(j, "weapon_skins", cfg.weaponSkins);
            SaveStringMap(j, "vehicle_skins", cfg.vehicleSkins);

            json offsets = json::object();
            for (const auto& [name, value] : cfg.offsets)
            {
                char buf[32]{};
                sprintf_s(buf, "0x%llX", static_cast<unsigned long long>(value));
                offsets[name] = std::string(buf);
            }
            j["offsets"] = offsets;

            std::ofstream file(path);
            if (!file.is_open())
            {
                Log("config: cannot open '%s' for writing", path.c_str());
                return false;
            }
            file << j.dump(4) << std::endl;
        }
        catch (const std::exception& e)
        {
            Log("config: save error: %s", e.what());
            return false;
        }
        return true;
    }
}
