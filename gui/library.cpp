#include "library.h"

#include <nlohmann/json.hpp>

#include "library_data.h"

namespace
{
    void LoadItems(const nlohmann::json& parent, const char* key, std::vector<flibrary::Item>& out)
    {
        const auto it = parent.find(key);
        if (it == parent.end() || !it->is_array())
        {
            return;
        }
        for (const auto& item : *it)
        {
            if (!item.is_object() || !item.contains("name"))
            {
                continue;
            }

            flibrary::Item entry;
            entry.name = item.value("name", "");
            entry.group = item.value("group", "");

            const auto skins = item.find("skins");
            if (skins != item.end())
            {
                if (skins->is_array())
                {
                    for (const auto& skin : *skins)
                    {
                        if (skin.is_string())
                        {
                            entry.skins.push_back(skin.get<std::string>());
                        }
                    }
                }
                else if (skins->is_string())
                {
                    entry.skins.push_back(skins->get<std::string>());
                }
            }

            out.push_back(std::move(entry));
        }
    }
}

namespace flibrary
{
    void Load(ItemLibrary& out)
    {
        out = ItemLibrary{};
        try
        {
            const nlohmann::json j = nlohmann::json::parse(kLibraryJson);
            LoadItems(j, "heroes", out.heroes);
            LoadItems(j, "weapons", out.weapons);
            LoadItems(j, "vehicles", out.vehicles);
        }
        catch (const std::exception&)
        {
            out = ItemLibrary{};
        }
    }
}
