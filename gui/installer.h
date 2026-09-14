#pragma once

#include <cstddef>
#include <string>

namespace flinstall
{
    struct Result
    {
        bool ok = false;
        std::string message;
    };

    struct VerifyResult
    {
        bool ok = false;
        size_t total = 0;
        size_t matched = 0;
        size_t mismatched = 0;
        size_t missing = 0;
        std::string message;
    };

    bool BackupsExist(const std::string& backupDir);
    Result BackupOriginals(const std::string& pakDir, const std::string& backupDir);
    Result RestoreOriginals(const std::string& pakDir, const std::string& backupDir);
    Result InstallPatchedPak(const std::string& patchedPakPath, const std::string& pakDir);
    VerifyResult VerifyBackups(const std::string& pakDir, const std::string& backupDir);
}
