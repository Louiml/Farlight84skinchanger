#pragma once

#include <string>
#include <vector>

namespace flibrary
{
    struct Item
    {
        std::string name;
        std::string group;
        std::vector<std::string> skins;
    };

    struct ItemLibrary
    {
        std::vector<Item> heroes;
        std::vector<Item> weapons;
        std::vector<Item> vehicles;
    };

    void Load(ItemLibrary& out);
}
