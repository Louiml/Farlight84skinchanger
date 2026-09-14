#include "pak.h"

#include <windows.h>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    void PrintFooter(const pak::Footer& f, uint64_t fileSize)
    {
        std::printf("file size:   %llu bytes\n", static_cast<unsigned long long>(fileSize));
        std::printf("parsed:      %s\n", f.parsed ? "yes" : "NO");
        if (!f.parsed)
        {
            return;
        }
        std::printf("version:     %u\n", f.version);
        std::printf("value1:      0x%llX\n", static_cast<unsigned long long>(f.value1));
        std::printf("value2:      0x%llX\n", static_cast<unsigned long long>(f.value2));
        std::printf("value3:      0x%llX\n", static_cast<unsigned long long>(f.value3));
        std::printf("flag:        %u\n", f.flag);
        std::printf("methods:     %s\n", f.methods.c_str());
        std::printf("hash:        ");
        for (int i = 0; i < 20; i++)
        {
            std::printf("%02X", f.hash[i]);
        }
        std::printf("\n");
    }

    int CmdFooter(const std::string& pakPath)
    {
        pak::Footer f{};
        uint64_t fileSize = 0;
        if (!pak::ParseFooter(pakPath, f, fileSize))
        {
            std::printf("[-] footer parse failed for %s\n", pakPath.c_str());
            return 1;
        }
        PrintFooter(f, fileSize);
        return 0;
    }

    int CmdIndex(const std::string& pakPath, const std::string& keyHex)
    {
        pak::Footer f{};
        uint64_t fileSize = 0;
        if (!pak::ParseFooter(pakPath, f, fileSize))
        {
            std::printf("[-] footer parse failed\n");
            return 1;
        }
        std::printf("[pak] %s (version %u)\n", pakPath.c_str(), f.version);

        uint8_t key[32]{};
        bool haveKey = false;
        if (!keyHex.empty())
        {
            if (!pak::ParseHexString(keyHex, key))
            {
                std::printf("[-] bad key, expected 64 hex chars\n");
                return 1;
            }
            haveKey = true;
        }

        bool anyIndex = false;
        for (const auto& region : pak::CandidateRegions(f, fileSize))
        {
            std::vector<uint8_t> data;
            if (!pak::ReadBytes(pakPath, region.offset, region.size, data))
            {
                std::printf("[region %s] unreadable (offset 0x%llX size 0x%llX)\n",
                            region.name.c_str(),
                            static_cast<unsigned long long>(region.offset),
                            static_cast<unsigned long long>(region.size));
                continue;
            }
            std::printf("[region %s] offset=0x%llX size=0x%llX\n",
                        region.name.c_str(),
                        static_cast<unsigned long long>(region.offset),
                        static_cast<unsigned long long>(region.size));

            if (!haveKey)
            {
                pak::PrintHead("  raw", data, 128);
                continue;
            }

            std::vector<uint8_t> ecb = data;
            if (pak::Aes256DecryptEcb(ecb, key))
            {
                pak::PrintHead("  ecb", ecb, 128);
                if (pak::LooksLikeIndex(ecb))
                {
                    std::printf("[!!!] ECB DECRYPT LOOKS LIKE A VALID INDEX (mount point found)\n");
                    anyIndex = true;
                }
            }
            else
            {
                std::printf("  ecb: decrypt failed\n");
            }

            std::vector<uint8_t> cbc = data;
            if (pak::Aes256DecryptCbc(cbc, key))
            {
                pak::PrintHead("  cbc", cbc, 128);
                if (pak::LooksLikeIndex(cbc))
                {
                    std::printf("[!!!] CBC DECRYPT LOOKS LIKE A VALID INDEX (mount point found)\n");
                    anyIndex = true;
                }
            }
            else
            {
                std::printf("  cbc: decrypt failed\n");
            }
        }

        if (!haveKey)
        {
            std::printf("[i] no key given - pass --key <64 hex chars> to attempt AES-256 decryption\n");
        }
        return anyIndex ? 0 : 2;
    }

    int CmdSelfTest()
    {
        uint8_t key[32]{};
        for (int i = 0; i < 32; i++)
        {
            key[i] = static_cast<uint8_t>(i * 7 + 3);
        }

        std::vector<uint8_t> plain;
        const char* probe = "FarlightSkinChanger AES selftest block #1";
        for (int block = 0; block < 2; block++)
        {
            for (const char* p = probe; *p != '\0'; p++)
            {
                plain.push_back(static_cast<uint8_t>(*p));
            }
            while (plain.size() % 16 != 0)
            {
                plain.push_back(0x41);
            }
        }

        int failures = 0;

        std::vector<uint8_t> ecb = plain;
        if (pak::Aes256EncryptEcb(ecb, key) && pak::Aes256DecryptEcb(ecb, key) && ecb == plain)
        {
            std::printf("[+] AES-256-ECB round trip OK\n");
        }
        else
        {
            std::printf("[-] AES-256-ECB round trip FAILED\n");
            failures++;
        }

        std::vector<uint8_t> cbc = plain;
        if (pak::Aes256EncryptCbc(cbc, key) && pak::Aes256DecryptCbc(cbc, key) && cbc == plain)
        {
            std::printf("[+] AES-256-CBC round trip OK\n");
        }
        else
        {
            std::printf("[-] AES-256-CBC round trip FAILED\n");
            failures++;
        }

        std::vector<uint8_t> mount(512, 0x41);
        const char* mp = "../../../Solarland/Content/";
        for (const char* p = mp; *p != '\0'; p++)
        {
            mount[static_cast<size_t>(p - mp)] = static_cast<uint8_t>(*p);
        }
        std::printf("[+] mount-point detector: %s\n",
                    pak::LooksLikeIndex(mount) ? "OK (detected)" : "BROKEN (not detected)");
        if (!pak::LooksLikeIndex(mount))
        {
            failures++;
        }

        std::printf(failures == 0 ? "[+] SELFTEST PASS\n" : "[-] SELFTEST FAIL\n");
        return failures == 0 ? 0 : 1;
    }
    int CmdAnalyze(const std::string& pakPath, const std::string& keyHex)
    {
        pak::Footer f{};
        uint64_t fileSize = 0;
        if (!pak::ParseFooter(pakPath, f, fileSize))
        {
            std::printf("[-] footer parse failed\n");
            return 1;
        }
        std::printf("[pak] %s (version %u, %llu bytes)\n", pakPath.c_str(), f.version,
                    static_cast<unsigned long long>(fileSize));

        if (keyHex.empty())
        {
            std::printf("[-] no key given - pass --key <64 hex> (phase B extraction)\n");
            return 1;
        }
        uint8_t key[32]{};
        if (!pak::ParseHexString(keyHex, key))
        {
            std::printf("[-] bad key format\n");
            return 1;
        }

        pak::DecryptedIndex idx;
        if (!pak::FindDecryptedIndex(pakPath, f, fileSize, key, idx))
        {
            std::printf("[-] key did not decrypt any index region (wrong key or unknown layout)\n");
            return 4;
        }

        std::printf("[+] INDEX DECRYPTED via %s, region %s (%zu bytes)\n",
                    idx.mode.c_str(), idx.regionName.c_str(), idx.data.size());
        pak::PrintHead("decrypted index head", idx.data, 256);

        int printed = 0;
        for (const auto& s : pak::ExtractStrings(idx.data, 5))
        {
            std::printf("  str: %s\n", s.c_str());
            if (++printed >= 60)
            {
                std::printf("  ... (truncated)\n");
                break;
            }
        }
        return 0;
    }

    int CmdPatch(const std::string& inputPak, const std::string& manifestPath,
                 const std::string& outPak, const std::string& keyHex)
    {
        std::printf("[patch] input:    %s\n", inputPak.c_str());
        std::printf("[patch] manifest: %s\n", manifestPath.c_str());
        std::printf("[patch] output:   %s\n", outPak.c_str());

        size_t swaps = 0;
        try
        {
            std::ifstream file(manifestPath);
            if (!file.is_open())
            {
                std::printf("[-] manifest not found: %s\n", manifestPath.c_str());
                return 1;
            }
            const nlohmann::json j = nlohmann::json::parse(file);
            for (const char* cat : {"hero_skins", "weapon_skins", "vehicle_skins"})
            {
                const auto it = j.find(cat);
                if (it != j.end() && it->is_object())
                {
                    for (auto e = it->begin(); e != it->end(); ++e)
                    {
                        std::printf("[patch] swap: %s -> %s\n", e.key().c_str(),
                                    e.value().get<std::string>().c_str());
                        swaps++;
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            std::printf("[-] manifest parse error: %s\n", e.what());
            return 1;
        }
        std::printf("[patch] %zu swap entries loaded\n", swaps);
        if (swaps == 0)
        {
            std::printf("[-] manifest has no swap entries - add items in the GUI first\n");
            return 1;
        }

        pak::EnsureParentDir(outPak);
        if (!pak::CopyFileBytes(inputPak, outPak))
        {
            std::printf("[-] cannot copy pak to output (error %lu)\n", GetLastError());
            return 1;
        }
        std::printf("[patch] output pak created (byte-identical baseline)\n");

        if (keyHex.empty())
        {
            std::printf("[GATE] awaiting AES key (phase B: verdict + key extraction in progress)\n");
            std::printf("[GATE] output is currently an unmodified copy - no swap applied yet\n");
            return 3;
        }

        uint8_t key[32]{};
        if (!pak::ParseHexString(keyHex, key))
        {
            std::printf("[-] bad key format\n");
            return 1;
        }

        pak::Footer f{};
        uint64_t fileSize = 0;
        if (!pak::ParseFooter(inputPak, f, fileSize))
        {
            std::printf("[-] footer parse failed\n");
            return 1;
        }

        pak::DecryptedIndex idx;
        if (!pak::FindDecryptedIndex(inputPak, f, fileSize, key, idx))
        {
            std::printf("[GATE] key did not decrypt the index - cannot map assets yet (phase B)\n");
            return 4;
        }

        std::printf("[GATE] index decrypted (%s, %s) - entry walker pending format validation (phase B)\n",
                    idx.mode.c_str(), idx.regionName.c_str());
        std::printf("[GATE] once the entry format is validated, swap payloads get written here\n");
        return 5;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("FlPakTool - Farlight pak research tool\n");
        std::printf("Usage:\n");
        std::printf("  FlPakTool footer <pak>\n");
        std::printf("  FlPakTool index <pak> [--key <64 hex>]\n");
        std::printf("  FlPakTool analyze <pak> --key <64 hex>\n");
        std::printf("  FlPakTool keyscan <pak> <exe> [--stride 4]\n");
        std::printf("  FlPakTool selftest\n");
        std::printf("  FlPakTool patch <input.pak> --manifest <skinmap.json> --out <output.pak> [--key <64 hex>]\n");
        return 0;
    }

    const std::string cmd = argv[1];
    if (cmd == "selftest")
    {
        return CmdSelfTest();
    }
    if (cmd == "footer" && argc >= 3)
    {
        return CmdFooter(argv[2]);
    }
    if (cmd == "index" && argc >= 3)
    {
        std::string keyHex;
        for (int i = 3; i < argc - 1; i++)
        {
            if (std::strcmp(argv[i], "--key") == 0)
            {
                keyHex = argv[i + 1];
            }
        }
        return CmdIndex(argv[2], keyHex);
    }
    if (cmd == "analyze" && argc >= 3)
    {
        std::string keyHex;
        for (int i = 3; i < argc - 1; i++)
        {
            if (std::strcmp(argv[i], "--key") == 0)
            {
                keyHex = argv[i + 1];
            }
        }
        return CmdAnalyze(argv[2], keyHex);
    }
    if (cmd == "patch" && argc >= 3)
    {
        std::string manifest;
        std::string out;
        std::string keyHex;
        for (int i = 3; i < argc - 1; i++)
        {
            if (std::strcmp(argv[i], "--manifest") == 0) manifest = argv[i + 1];
            else if (std::strcmp(argv[i], "--out") == 0) out = argv[i + 1];
            else if (std::strcmp(argv[i], "--key") == 0) keyHex = argv[i + 1];
        }
        if (manifest.empty() || out.empty())
        {
            std::printf("[-] patch requires --manifest and --out\n");
            return 1;
        }
        return CmdPatch(argv[2], manifest, out, keyHex);
    }

    if (cmd == "keyscan" && argc >= 4)
    {
        size_t stride = 4;
        for (int i = 4; i < argc - 1; i++)
        {
            if (std::strcmp(argv[i], "--stride") == 0)
            {
                stride = static_cast<size_t>(std::atoi(argv[i + 1]));
                if (stride == 0)
                {
                    stride = 1;
                }
            }
        }
        return pak::ScanForKeys(argv[2], argv[3], stride);
    }

    std::printf("[-] unknown command or missing arguments\n");
    return 1;
}
