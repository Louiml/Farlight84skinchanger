#include "pak_inventory.h"

#include <windows.h>

#include <cstring>

namespace
{
    constexpr uint8_t kMagic[4] = {0xE1, 0x12, 0x6F, 0x5A};
}

namespace flpak
{
    bool ParseFooter(const std::string& path, PakFooter& out)
    {
        out = PakFooter{};
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file, &size) || size.QuadPart < 512)
        {
            CloseHandle(file);
            return false;
        }

        const uint64_t tailStart = static_cast<uint64_t>(size.QuadPart) - 512;
        SetFilePointer(file, static_cast<LONG>(tailStart & 0xFFFFFFFF), nullptr, FILE_BEGIN);

        uint8_t buf[512]{};
        DWORD read = 0;
        const BOOL ok = ReadFile(file, buf, 512, &read, nullptr);
        CloseHandle(file);
        if (!ok || read != 512)
        {
            return false;
        }

        int magicIdx = -1;
        for (int i = 512 - 4; i >= 0; i--)
        {
            if (buf[i] == kMagic[0] && buf[i + 1] == kMagic[1] &&
                buf[i + 2] == kMagic[2] && buf[i + 3] == kMagic[3])
            {
                magicIdx = i;
                break;
            }
        }
        if (magicIdx < 0 || magicIdx + 213 > 512)
        {
            return false;
        }

        const uint8_t* f = buf + magicIdx;
        out.version = *reinterpret_cast<const uint32_t*>(f + 4);
        out.value1 = *reinterpret_cast<const uint64_t*>(f + 8);
        out.value2 = *reinterpret_cast<const uint64_t*>(f + 16);
        out.value3 = *reinterpret_cast<const uint64_t*>(f + 24);
        std::memcpy(out.hash, f + 32, 20);
        out.flag = f[52];
        out.methods.clear();
        for (int m = 0; m < 5; m++)
        {
            const char* method = reinterpret_cast<const char*>(f + 53 + m * 32);
            if (method[0] != '\0')
            {
                if (!out.methods.empty())
                {
                    out.methods += ",";
                }
                out.methods += method;
            }
        }
        out.parsed = true;
        return true;
    }

    std::vector<PakEntry> ScanDirectory(const std::string& dir)
    {
        std::vector<PakEntry> result;
        const std::string pattern = dir + "\\*";
        WIN32_FIND_DATAA fd{};
        HANDLE find = FindFirstFileA(pattern.c_str(), &fd);
        if (find == INVALID_HANDLE_VALUE)
        {
            return result;
        }
        do
        {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                continue;
            }
            const std::string name = fd.cFileName;
            if (name.size() < 4 || _stricmp(name.substr(name.size() - 4).c_str(), ".pak") != 0)
            {
                continue;
            }
            PakEntry entry;
            entry.name = name;
            entry.size = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
            ParseFooter(dir + "\\" + name, entry.footer);
            result.push_back(entry);
        } while (FindNextFileA(find, &fd));
        FindClose(find);
        return result;
    }
}
