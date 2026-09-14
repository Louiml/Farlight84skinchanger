#include "env.h"

#include <windows.h>

#include <cstdlib>
#include <fstream>
#include <unordered_map>

namespace
{
    std::unordered_map<std::string, std::string> g_fileEnv;
    bool g_loaded = false;

    std::string Trim(const std::string& s)
    {
        const size_t start = s.find_first_not_of(" \t\r");
        if (start == std::string::npos)
        {
            return "";
        }
        const size_t end = s.find_last_not_of(" \t\r");
        return s.substr(start, end - start + 1);
    }
}

namespace flenv
{
    void Load()
    {
        if (g_loaded)
        {
            return;
        }
        g_loaded = true;

        char path[MAX_PATH]{};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string dir(path);
        dir = dir.substr(0, dir.find_last_of("\\/") + 1);

        for (int depth = 0; depth < 4; depth++)
        {
            std::ifstream file(dir + ".env");
            if (!file.is_open())
            {
                dir += "..\\";
                continue;
            }
            std::string line;
            while (std::getline(file, line))
            {
                const std::string trimmed = Trim(line);
                if (trimmed.empty() || trimmed[0] == '#')
                {
                    continue;
                }
                const auto eq = trimmed.find('=');
                if (eq == std::string::npos)
                {
                    continue;
                }
                std::string value = Trim(trimmed.substr(eq + 1));
                if (value.size() >= 2
                    && (value.front() == '"' || value.front() == '\''))
                {
                    value = value.substr(1, value.size() - 2);
                }
                g_fileEnv[Trim(trimmed.substr(0, eq))] = value;
            }
            return;
        }
    }

    std::string Get(const std::string& key, const std::string& fallback)
    {
        const char* system = std::getenv(key.c_str());
        if (system != nullptr && system[0] != '\0')
        {
            return system;
        }
        const auto it = g_fileEnv.find(key);
        if (it != g_fileEnv.end())
        {
            return it->second;
        }
        return fallback;
    }
}
