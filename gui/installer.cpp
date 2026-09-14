#include "installer.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{
    std::vector<std::string> ListPaks(const std::string& dir)
    {
        std::vector<std::string> names;
        const std::string pattern = dir + "\\*";
        WIN32_FIND_DATAA fd{};
        HANDLE find = FindFirstFileA(pattern.c_str(), &fd);
        if (find == INVALID_HANDLE_VALUE)
        {
            return names;
        }
        do
        {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                continue;
            }
            const std::string name = fd.cFileName;
            if (name.size() >= 4 && _stricmp(name.substr(name.size() - 4).c_str(), ".pak") == 0)
            {
                names.push_back(name);
            }
        } while (FindNextFileA(find, &fd));
        FindClose(find);
        return names;
    }

    bool EnsureDir(const std::string& dir)
    {
        if (GetFileAttributesA(dir.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            return true;
        }
        return CreateDirectoryA(dir.c_str(), nullptr) != FALSE;
    }

    bool Sha256File(const std::string& path, uint8_t out[32])
    {
        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        HANDLE file = INVALID_HANDLE_VALUE;
        bool ok = false;

        do
        {
            if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            {
                break;
            }
            if (!BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0)))
            {
                break;
            }

            file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                break;
            }

            std::vector<uint8_t> buffer(1024 * 1024);
            DWORD read = 0;
            for (;;)
            {
                if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) || read == 0)
                {
                    break;
                }
                if (!BCRYPT_SUCCESS(BCryptHashData(hash, buffer.data(), read, 0)))
                {
                    break;
                }
            }
            ok = BCRYPT_SUCCESS(BCryptFinishHash(hash, out, 32, 0));
        } while (false);

        if (file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(file);
        }
        if (hash != nullptr)
        {
            BCryptDestroyHash(hash);
        }
        if (alg != nullptr)
        {
            BCryptCloseAlgorithmProvider(alg, 0);
        }
        return ok;
    }
}

namespace flinstall
{
    bool BackupsExist(const std::string& backupDir)
    {
        if (GetFileAttributesA(backupDir.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }
        return !ListPaks(backupDir).empty();
    }

    Result BackupOriginals(const std::string& pakDir, const std::string& backupDir)
    {
        if (!EnsureDir(backupDir))
        {
            return {false, "cannot create backup folder " + backupDir};
        }
        const auto paks = ListPaks(pakDir);
        if (paks.empty())
        {
            return {false, "no paks found in " + pakDir};
        }
        int copied = 0;
        for (const auto& name : paks)
        {
            const std::string src = pakDir + "\\" + name;
            const std::string dst = backupDir + "\\" + name;
            if (GetFileAttributesA(dst.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                continue;
            }
            if (!CopyFileA(src.c_str(), dst.c_str(), FALSE))
            {
                return {false, "backup failed on " + name + " (error " + std::to_string(GetLastError()) + ")"};
            }
            copied++;
        }
        return {true, "backup complete: " + std::to_string(paks.size()) + " paks known safe (" +
                      std::to_string(copied) + " newly copied)"};
    }

    Result RestoreOriginals(const std::string& pakDir, const std::string& backupDir)
    {
        const auto paks = ListPaks(backupDir);
        if (paks.empty())
        {
            return {false, "no backups in " + backupDir};
        }
        for (const auto& name : paks)
        {
            if (!CopyFileA((backupDir + "\\" + name).c_str(), (pakDir + "\\" + name).c_str(), FALSE))
            {
                return {false, "restore failed on " + name + " (error " + std::to_string(GetLastError()) + ")"};
            }
        }
        return {true, "restored " + std::to_string(paks.size()) + " original paks into the game mount folder"};
    }

    Result InstallPatchedPak(const std::string& patchedPakPath, const std::string& pakDir)
    {
        const std::string name = patchedPakPath.substr(patchedPakPath.find_last_of("\\/") + 1);
        const std::string dst = pakDir + "\\" + name;
        if (!CopyFileA(patchedPakPath.c_str(), dst.c_str(), FALSE))
        {
            return {false, "install failed (error " + std::to_string(GetLastError()) + ")"};
        }
        return {true, "installed " + name + " into " + pakDir};
    }

    VerifyResult VerifyBackups(const std::string& pakDir, const std::string& backupDir)
    {
        VerifyResult result;
        const auto paks = ListPaks(pakDir);
        result.total = paks.size();
        if (paks.empty())
        {
            result.message = "no paks found in " + pakDir;
            return result;
        }

        std::string problems;
        for (const auto& name : paks)
        {
            const std::string src = pakDir + "\\" + name;
            const std::string dst = backupDir + "\\" + name;

            if (GetFileAttributesA(dst.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                result.missing++;
                if (problems.size() < 400)
                {
                    problems += "\n  missing backup: " + name;
                }
                continue;
            }

            uint8_t srcHash[32]{};
            uint8_t dstHash[32]{};
            if (!Sha256File(src, srcHash) || !Sha256File(dst, dstHash)
                || std::memcmp(srcHash, dstHash, 32) != 0)
            {
                result.mismatched++;
                if (problems.size() < 400)
                {
                    problems += "\n  differs from backup: " + name;
                }
                continue;
            }
            result.matched++;
        }

        result.ok = result.matched == result.total;
        result.message = std::to_string(result.matched) + "/" + std::to_string(result.total)
                         + " originals match their backups (SHA-256)" + problems;
        return result;
    }
}
