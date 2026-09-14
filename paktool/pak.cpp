#include "pak.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstdio>
#include <cstring>

#pragma pack(push, 1)
struct PESectionCandidate
{
    char name[8];
    uint32_t virtualSize;
    uint32_t virtualAddress;
    uint32_t sizeOfRawData;
    uint32_t pointerToRawData;
    uint32_t relocs;
    uint32_t lineNumbers;
    uint16_t numRelocs;
    uint16_t numLineNumbers;
    uint32_t characteristics;
};
#pragma pack(pop)

namespace
{
    bool OpenForRead(const std::string& path, HANDLE& file, uint64_t& size)
    {
        file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        LARGE_INTEGER sz{};
        if (!GetFileSizeEx(file, &sz))
        {
            CloseHandle(file);
            return false;
        }
        size = static_cast<uint64_t>(sz.QuadPart);
        return true;
    }

    struct BCryptReleaser
    {
        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_KEY_HANDLE key = nullptr;
        ~BCryptReleaser()
        {
            if (key != nullptr) BCryptDestroyKey(key);
            if (alg != nullptr) BCryptCloseAlgorithmProvider(alg, 0);
        }
    };

    bool Aes256Run(std::vector<uint8_t>& data, const uint8_t keyBytes[32], bool ecb, bool encrypt)
    {
        if (data.empty() || (data.size() % 16) != 0)
        {
            return false;
        }

        BCryptReleaser r;
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&r.alg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
        {
            return false;
        }

        const wchar_t* chain = ecb ? BCRYPT_CHAIN_MODE_ECB : BCRYPT_CHAIN_MODE_CBC;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(r.alg, BCRYPT_CHAINING_MODE,
                                              reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(chain)),
                                              static_cast<ULONG>(sizeof(wchar_t) * (wcslen(chain) + 1)), 0)))
        {
            return false;
        }

        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(r.alg, &r.key, nullptr, 0,
                                                       const_cast<PUCHAR>(keyBytes), 32, 0)))
        {
            return false;
        }

        std::vector<uint8_t> iv(16, 0);
        ULONG done = 0;
        NTSTATUS status;
        if (encrypt)
        {
            status = BCryptEncrypt(r.key, data.data(), static_cast<ULONG>(data.size()), nullptr,
                                   ecb ? nullptr : iv.data(),
                                   ecb ? 0 : static_cast<ULONG>(iv.size()),
                                   data.data(), static_cast<ULONG>(data.size()), &done, 0);
        }
        else
        {
            status = BCryptDecrypt(r.key, data.data(), static_cast<ULONG>(data.size()), nullptr,
                                   ecb ? nullptr : iv.data(),
                                   ecb ? 0 : static_cast<ULONG>(iv.size()),
                                   data.data(), static_cast<ULONG>(data.size()), &done, 0);
        }
        if (!BCRYPT_SUCCESS(status) || done != data.size())
        {
            return false;
        }
        return true;
    }
}

namespace pak
{
    bool ParseFooter(const std::string& path, Footer& out, uint64_t& fileSize)
    {
        out = Footer{};
        fileSize = 0;

        HANDLE file = INVALID_HANDLE_VALUE;
        uint64_t total = 0;
        if (!OpenForRead(path, file, total))
        {
            return false;
        }

        const uint64_t tailStart = total > 512 ? total - 512 : 0;
        const uint64_t tailLen = total - tailStart;
        SetFilePointer(file, static_cast<LONG>(tailStart), nullptr, FILE_BEGIN);
        std::vector<uint8_t> tail(static_cast<size_t>(tailLen));
        DWORD read = 0;
        const BOOL ok = ReadFile(file, tail.data(), static_cast<DWORD>(tailLen), &read, nullptr);
        CloseHandle(file);
        if (!ok || read != tailLen)
        {
            return false;
        }
        fileSize = total;

        int magicIdx = -1;
        for (int i = static_cast<int>(tail.size()) - 4; i >= 0; i--)
        {
            if (tail[i] == 0xE1 && tail[i + 1] == 0x12 && tail[i + 2] == 0x6F && tail[i + 3] == 0x5A)
            {
                magicIdx = i;
                break;
            }
        }
        if (magicIdx < 0 || magicIdx + 213 > static_cast<int>(tail.size()))
        {
            return false;
        }

        const uint8_t* f = tail.data() + magicIdx;
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

    bool ReadBytes(const std::string& path, uint64_t offset, uint64_t size, std::vector<uint8_t>& out)
    {
        out.clear();
        HANDLE file = INVALID_HANDLE_VALUE;
        uint64_t total = 0;
        if (!OpenForRead(path, file, total))
        {
            return false;
        }
        if (offset >= total || size == 0 || offset + size > total || size > (512ull * 1024 * 1024))
        {
            CloseHandle(file);
            return false;
        }

        SetFilePointer(file, static_cast<LONG>(offset), nullptr, FILE_BEGIN);
        out.resize(static_cast<size_t>(size));
        DWORD read = 0;
        const BOOL ok = ReadFile(file, out.data(), static_cast<DWORD>(size), &read, nullptr);
        CloseHandle(file);
        if (!ok || read != size)
        {
            out.clear();
            return false;
        }
        return true;
    }

    std::vector<Region> CandidateRegions(const Footer& f, uint64_t fileSize)
    {
        std::vector<Region> regions;
        const auto add = [&](const char* name, uint64_t off, uint64_t size) {
            if (size > 0 && off < fileSize && off + size <= fileSize && size <= (64ull * 1024 * 1024))
            {
                regions.push_back({name, off, size});
            }
        };
        add("A (offset=v1, size=v3)", f.value1, f.value3);
        add("B (offset=v2, size=v3)", f.value2, f.value3);
        add("C (offset=v1, size=v2)", f.value1, f.value2);
        return regions;
    }

    bool Aes256DecryptEcb(std::vector<uint8_t>& data, const uint8_t key[32])
    {
        return Aes256Run(data, key, true, false);
    }

    bool Aes256DecryptCbc(std::vector<uint8_t>& data, const uint8_t key[32])
    {
        return Aes256Run(data, key, false, false);
    }

    bool Aes256EncryptEcb(std::vector<uint8_t>& data, const uint8_t key[32])
    {
        return Aes256Run(data, key, true, true);
    }

    bool Aes256EncryptCbc(std::vector<uint8_t>& data, const uint8_t key[32])
    {
        return Aes256Run(data, key, false, true);
    }

    bool ParseHexString(const std::string& hex, uint8_t out[32])
    {
        if (hex.size() != 64)
        {
            return false;
        }
        for (int i = 0; i < 32; i++)
        {
            unsigned int byte = 0;
            if (sscanf_s(hex.c_str() + i * 2, "%2x", &byte) != 1)
            {
                return false;
            }
            out[i] = static_cast<uint8_t>(byte);
        }
        return true;
    }

    void PrintHead(const std::string& label, const std::vector<uint8_t>& data, size_t bytes)
    {
        const size_t n = data.size() < bytes ? data.size() : bytes;
        std::printf("%s (%zu bytes):\n", label.c_str(), data.size());
        for (size_t i = 0; i < n; i += 16)
        {
            std::printf("  %04X  ", static_cast<unsigned>(i));
            for (size_t j = 0; j < 16 && i + j < n; j++)
            {
                std::printf("%02X ", data[i + j]);
            }
            std::printf(" ");
            for (size_t j = 0; j < 16 && i + j < n; j++)
            {
                const uint8_t c = data[i + j];
                std::printf("%c", (c >= 32 && c <= 126) ? static_cast<char>(c) : '.');
            }
            std::printf("\n");
        }
    }

    bool LooksLikeIndex(const std::vector<uint8_t>& data)
    {
        if (data.size() < 16)
        {
            return false;
        }
        bool sawMountPattern = false;
        for (size_t i = 0; i + 3 < data.size() && i < 512; i++)
        {
            if (data[i] == '.' && data[i + 1] == '.' && data[i + 2] == '/' && data[i + 3] == '.')
            {
                sawMountPattern = true;
                break;
            }
        }
        if (!sawMountPattern)
        {
            return false;
        }
        int bestRun = 0;
        int run = 0;
        for (size_t i = 0; i < data.size() && i < 256; i++)
        {
            const uint8_t c = data[i];
            if (c >= 32 && c <= 126 && c != 0)
            {
                run++;
                if (run > bestRun)
                {
                    bestRun = run;
                }
            }
            else
            {
                run = 0;
            }
        }
        return bestRun >= 16;
    }

    bool FindDecryptedIndex(const std::string& pakPath, const Footer& f, uint64_t fileSize,
                            const uint8_t key[32], DecryptedIndex& out)
    {
        for (const auto& region : CandidateRegions(f, fileSize))
        {
            std::vector<uint8_t> data;
            if (!ReadBytes(pakPath, region.offset, region.size, data))
            {
                continue;
            }
            std::vector<uint8_t> ecb = data;
            if (Aes256DecryptEcb(ecb, key) && LooksLikeIndex(ecb))
            {
                out.data = std::move(ecb);
                out.regionName = region.name;
                out.mode = "AES-256-ECB";
                return true;
            }
            std::vector<uint8_t> cbc = data;
            if (Aes256DecryptCbc(cbc, key) && LooksLikeIndex(cbc))
            {
                out.data = std::move(cbc);
                out.regionName = region.name;
                out.mode = "AES-256-CBC";
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> ExtractStrings(const std::vector<uint8_t>& data, size_t minLength)
    {
        std::vector<std::string> strings;
        std::string current;
        for (const uint8_t b : data)
        {
            if (b >= 32 && b <= 126)
            {
                current.push_back(static_cast<char>(b));
            }
            else
            {
                if (current.size() >= minLength)
                {
                    strings.push_back(current);
                }
                current.clear();
            }
        }
        if (current.size() >= minLength)
        {
            strings.push_back(current);
        }
        return strings;
    }

    bool CopyFileBytes(const std::string& from, const std::string& to)
    {
        return CopyFileA(from.c_str(), to.c_str(), FALSE) != FALSE;
    }

    void EnsureParentDir(const std::string& path)
    {
        const auto slash = path.find_last_of("\\/");
        if (slash == std::string::npos || slash == 0)
        {
            return;
        }
        const std::string dir = path.substr(0, slash);
        std::string current;
        size_t pos = 0;
        while (pos <= dir.size())
        {
            const size_t next = dir.find_first_of("\\/", pos);
            current = (next == std::string::npos) ? dir : dir.substr(0, next);
            if (!current.empty() && GetFileAttributesA(current.c_str()) == INVALID_FILE_ATTRIBUTES)
            {
                CreateDirectoryA(current.c_str(), nullptr);
            }
            if (next == std::string::npos)
            {
                break;
            }
            pos = next + 1;
        }
    }

    bool ParsePEDataSections(const std::string& exePath, std::vector<DataSection>& out)
    {
        out.clear();
        HANDLE file = CreateFileA(exePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        auto readAt = [&](uint64_t offset, void* buf, size_t len) -> bool {
            SetFilePointer(file, static_cast<LONG>(offset), nullptr, FILE_BEGIN);
            DWORD read = 0;
            return ReadFile(file, buf, static_cast<DWORD>(len), &read, nullptr) && read == len;
        };

        uint8_t dos[64]{};
        bool ok = readAt(0, dos, sizeof(dos)) && dos[0] == 'M' && dos[1] == 'Z';
        uint32_t peOffset = 0;
        if (ok)
        {
            std::memcpy(&peOffset, dos + 0x3C, 4);
            uint32_t sig[1]{};
            ok = readAt(peOffset, sig, 4) && sig[0] == 0x00004550;
        }

        uint16_t numSections = 0;
        uint16_t sizeOptional = 0;
        if (ok)
        {
            uint8_t fileHeader[20]{};
            ok = readAt(peOffset + 4, fileHeader, sizeof(fileHeader));
            if (ok)
            {
                std::memcpy(&numSections, fileHeader + 2, 2);
                std::memcpy(&sizeOptional, fileHeader + 16, 2);
            }
        }

        if (ok)
        {
            const uint64_t sectionTable = peOffset + 24 + sizeOptional;
            for (uint16_t i = 0; i < numSections && ok; i++)
            {
                PESectionCandidate sec{};
                ok = readAt(sectionTable + i * sizeof(PESectionCandidate), &sec, sizeof(sec));
                if (!ok)
                {
                    break;
                }
                constexpr uint32_t kDiscardable = 0x02000000;
                if ((sec.characteristics & kDiscardable) == 0 && sec.sizeOfRawData > 0)
                {
                    DataSection ds;
                    ds.fileOffset = sec.pointerToRawData;
                    ds.size = sec.sizeOfRawData;
                    std::memcpy(ds.name, sec.name, 8);
                    ds.name[8] = '\0';
                    out.push_back(ds);
                }
            }
        }

        CloseHandle(file);
        return ok && !out.empty();
    }

    bool LooksLikeMountPointFirstBlock(const uint8_t d[16])
    {
        for (int i = 0; i + 3 < 16; i++)
        {
            if (d[i] == '.' && d[i + 1] == '.' && d[i + 2] == '/' && d[i + 3] == '.')
            {
                int printable = 0;
                for (int j = i; j < 16; j++)
                {
                    const uint8_t c = d[j];
                    if (c >= 32 && c <= 126 && c != 0)
                    {
                        printable++;
                    }
                }
                if (printable >= 8)
                {
                    return true;
                }
            }
        }
        return false;
    }

    namespace
    {
        bool DecryptBlock16(BCRYPT_ALG_HANDLE alg, const uint8_t key[32], const uint8_t in[16],
                            uint8_t out[16])
        {
            BCRYPT_KEY_HANDLE kh = nullptr;
            if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(alg, &kh, nullptr, 0,
                                                            const_cast<PUCHAR>(key), 32, 0)))
            {
                return false;
            }
            ULONG done = 0;
            const NTSTATUS st = BCryptDecrypt(kh, const_cast<PUCHAR>(in), 16, nullptr, nullptr, 0,
                                              out, 16, &done, 0);
            BCryptDestroyKey(kh);
            return BCRYPT_SUCCESS(st) && done == 16;
        }
    }

    int ScanForKeys(const std::string& pakPath, const std::string& exePath, size_t stride)
    {
        Footer f{};
        uint64_t fileSize = 0;
        if (!ParseFooter(pakPath, f, fileSize))
        {
            std::printf("[-] pak footer parse failed\n");
            return -1;
        }

        struct Target
        {
            std::vector<uint8_t> block;
            std::string regionName;
        };
        std::vector<Target> targets;
        for (const auto& region : CandidateRegions(f, fileSize))
        {
            std::vector<uint8_t> block;
            if (ReadBytes(pakPath, region.offset, 16, block))
            {
                targets.push_back({std::move(block), region.name});
                std::printf("[scan] known-plaintext target %s @ 0x%llX\n",
                            region.name.c_str(), static_cast<unsigned long long>(region.offset));
            }
        }
        if (targets.empty())
        {
            std::printf("[-] no index candidate regions to target\n");
            return -1;
        }

        uint64_t exeSize = 0;
        {
            HANDLE probe = CreateFileA(exePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (probe == INVALID_HANDLE_VALUE)
            {
                std::printf("[-] cannot open exe\n");
                return -2;
            }
            LARGE_INTEGER sz{};
            GetFileSizeEx(probe, &sz);
            exeSize = static_cast<uint64_t>(sz.QuadPart);
            CloseHandle(probe);
        }

        std::vector<uint8_t> haystack;
        if (!ReadBytes(exePath, 0, exeSize, haystack) || haystack.size() < 32)
        {
            std::printf("[-] cannot read exe (%.1f MB)\n", exeSize / (1024.0 * 1024.0));
            return -2;
        }
        std::printf("[scan] whole file: %.1f MB, stride %zu\n",
                    exeSize / (1024.0 * 1024.0), stride);

        BCRYPT_ALG_HANDLE alg = nullptr;
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
        {
            std::printf("[-] AES provider failed\n");
            return -3;
        }

        int verifiedHits = 0;
        uint64_t trials = 0;
        const uint64_t candidates = (haystack.size() - 32) / stride + 1;
        std::printf("[scan] %llu candidates\n", static_cast<unsigned long long>(candidates));

        for (uint64_t ci = 0; ci < candidates; ci++)
        {
            const uint8_t* keyPtr = haystack.data() + ci * stride;
            for (const auto& t : targets)
            {
                uint8_t dec[16]{};
                if (!DecryptBlock16(alg, keyPtr, t.block.data(), dec))
                {
                    continue;
                }
                trials++;
                if (LooksLikeMountPointFirstBlock(dec))
                {
                    uint8_t key[32]{};
                    std::memcpy(key, keyPtr, 32);
                    std::printf("[HIT] candidate key at exe 0x%llX (%s)\n",
                                static_cast<unsigned long long>(ci * stride),
                                t.regionName.c_str());
                    std::printf("[HIT] key hex: ");
                    for (int b = 0; b < 32; b++)
                    {
                        std::printf("%02x", key[b]);
                    }
                    std::printf("\n");
                    DecryptedIndex idx;
                    if (FindDecryptedIndex(pakPath, f, fileSize, key, idx))
                    {
                        std::printf("[!!!] VERIFIED - index decrypts (%s, %s, %zu bytes)\n",
                                    idx.mode.c_str(), idx.regionName.c_str(), idx.data.size());
                        PrintHead("verified index head", idx.data, 256);
                        int shown = 0;
                        for (const auto& str : ExtractStrings(idx.data, 5))
                        {
                            std::printf("  str: %s\n", str.c_str());
                            if (++shown >= 60)
                            {
                                break;
                            }
                        }
                        verifiedHits++;
                    }
                    else
                    {
                        std::printf("[hit] first-block matched but region did not fully validate (weak)\n");
                    }
                }
            }
            if ((ci & 0xFFFFFF) == 0 && ci > 0)
            {
                std::printf("[scan] ... %.0f%%\n", 100.0 * ci / candidates);
            }
        }

        BCryptCloseAlgorithmProvider(alg, 0);
        std::printf("[scan] done: %llu AES trials, %d verified keys\n",
                    static_cast<unsigned long long>(trials), verifiedHits);
        return verifiedHits;
    }
}
