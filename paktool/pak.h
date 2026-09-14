#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pak
{
    struct Footer
    {
        bool parsed = false;
        uint32_t version = 0;
        uint64_t value1 = 0;
        uint64_t value2 = 0;
        uint64_t value3 = 0;
        uint8_t hash[20]{};
        uint8_t flag = 0;
        std::string methods;
    };

    struct Region
    {
        std::string name;
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    bool ParseFooter(const std::string& path, Footer& out, uint64_t& fileSize);
    bool ReadBytes(const std::string& path, uint64_t offset, uint64_t size, std::vector<uint8_t>& out);
    std::vector<Region> CandidateRegions(const Footer& f, uint64_t fileSize);
    bool Aes256DecryptEcb(std::vector<uint8_t>& data, const uint8_t key[32]);
    bool Aes256DecryptCbc(std::vector<uint8_t>& data, const uint8_t key[32]);
    bool Aes256EncryptEcb(std::vector<uint8_t>& data, const uint8_t key[32]);
    bool Aes256EncryptCbc(std::vector<uint8_t>& data, const uint8_t key[32]);
    bool ParseHexString(const std::string& hex, uint8_t out[32]);
    void PrintHead(const std::string& label, const std::vector<uint8_t>& data, size_t bytes);
    bool LooksLikeIndex(const std::vector<uint8_t>& data);

    struct DecryptedIndex
    {
        std::vector<uint8_t> data;
        std::string regionName;
        std::string mode;
    };

    bool FindDecryptedIndex(const std::string& pakPath, const Footer& f, uint64_t fileSize,
                            const uint8_t key[32], DecryptedIndex& out);
    std::vector<std::string> ExtractStrings(const std::vector<uint8_t>& data, size_t minLength);
    bool CopyFileBytes(const std::string& from, const std::string& to);
    void EnsureParentDir(const std::string& path);

    struct DataSection
    {
        uint64_t fileOffset = 0;
        uint64_t size = 0;
        char name[9]{};
    };

    bool ParsePEDataSections(const std::string& exePath, std::vector<DataSection>& out);
    bool LooksLikeMountPointFirstBlock(const uint8_t d[16]);
    int ScanForKeys(const std::string& pakPath, const std::string& exePath, size_t stride);
}
