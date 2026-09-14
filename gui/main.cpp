#include "installer.h"
#include "env.h"
#include "library.h"
#include "pak_inventory.h"
#include "skin_map.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tlhelp32.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include "style.h"

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <share.h>
#include <string>
#include <unordered_map>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;
    IDXGISwapChain* g_swapChain = nullptr;
    ID3D11RenderTargetView* g_rtv = nullptr;

    std::string g_exeDir;
    std::string g_pakDir;
    std::string g_backupDir;
    std::string g_skinMapPath;
    std::string g_ghidraLogPath;

    flskin::SkinMap g_map;
    std::vector<flpak::PakEntry> g_paks;
    ULONGLONG g_lastScan = 0;
    ULONGLONG g_lastLog = 0;
    std::string g_logTail;
    std::string g_status;
    std::string g_verifyMsg;
    bool g_verifyOk = false;
    std::string g_patchOutput;
    int g_selectedPak = -1;

    flibrary::ItemLibrary g_library;
    int g_heroComboIdx = 0;
    int g_weaponComboIdx = 0;
    int g_vehicleComboIdx = 0;
    int g_heroSkinCombo = 0;
    int g_weaponSkinCombo = 0;
    int g_vehicleSkinCombo = 0;
    int g_heroLastItem = -1;
    int g_weaponLastItem = -1;
    int g_vehicleLastItem = -1;
    char g_heroSkin[64]{};
    char g_weaponSkin[64]{};
    char g_vehicleSkin[64]{};

    FILE* g_debug = nullptr;

    void GuiLog(const char* fmt, ...)
    {
        if (g_debug == nullptr)
        {
            return;
        }
        va_list ap{};
        va_start(ap, fmt);
        std::vfprintf(g_debug, fmt, ap);
        va_end(ap);
        std::fputc('\n', g_debug);
        std::fflush(g_debug);
    }

    std::string ExeDir()
    {
        char path[MAX_PATH]{};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        const std::string s(path);
        return s.substr(0, s.find_last_of("\\/") + 1);
    }

    bool JavaRunning()
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        bool found = false;
        if (Process32First(snap, &pe))
        {
            do
            {
                if (_stricmp(pe.szExeFile, "java.exe") == 0)
                {
                    found = true;
                    break;
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        return found;
    }

    std::string ReadLogTail(const std::string& path)
    {
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return "";
        }
        LARGE_INTEGER size{};
        GetFileSizeEx(file, &size);
        constexpr uint64_t window = 4096;
        const uint64_t start = size.QuadPart > static_cast<LONGLONG>(window)
                                   ? static_cast<uint64_t>(size.QuadPart) - window
                                   : 0;
        const uint64_t len = static_cast<uint64_t>(size.QuadPart) - start;
        SetFilePointer(file, static_cast<LONG>(start), nullptr, FILE_BEGIN);
        std::string buf(len, '\0');
        DWORD read = 0;
        ReadFile(file, buf.data(), static_cast<DWORD>(len), &read, nullptr);
        CloseHandle(file);
        buf.resize(read);
        return buf;
    }

    void RescanPaks()
    {
        g_paks = flpak::ScanDirectory(g_pakDir);
        if (g_selectedPak >= static_cast<int>(g_paks.size()))
        {
            g_selectedPak = -1;
        }
    }

    bool RunCapture(const std::string& cmdline, std::string& output)
    {
        output.clear();

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        HANDLE readEnd = nullptr;
        HANDLE writeEnd = nullptr;
        if (!CreatePipe(&readEnd, &writeEnd, &sa, 0))
        {
            return false;
        }
        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = writeEnd;
        si.hStdError = writeEnd;
        PROCESS_INFORMATION pi{};
        std::vector<char> cmd(cmdline.begin(), cmdline.end());
        cmd.push_back('\0');

        const BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        CloseHandle(writeEnd);
        if (!ok)
        {
            CloseHandle(readEnd);
            return false;
        }

        char buf[4096];
        DWORD n = 0;
        while (ReadFile(readEnd, buf, sizeof(buf), &n, nullptr) && n > 0)
        {
            output.append(buf, n);
            if (output.size() > 65536)
            {
                output.erase(0, output.size() - 65536);
            }
        }
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(readEnd);
        return true;
    }

    int LargestPakIndex()
    {
        int best = -1;
        uint64_t bestSize = 0;
        for (int i = 0; i < static_cast<int>(g_paks.size()); i++)
        {
            if (g_paks[i].size > bestSize)
            {
                bestSize = g_paks[i].size;
                best = i;
            }
        }
        return best;
    }

    void GeneratePatchedPak()
    {
        if (g_paks.empty())
        {
            RescanPaks();
        }
        int target = g_selectedPak >= 0 && g_selectedPak < static_cast<int>(g_paks.size())
                         ? g_selectedPak
                         : LargestPakIndex();
        if (target < 0)
        {
            g_patchOutput = "[-] no paks found in " + g_pakDir;
            return;
        }

        flskin::Save(g_skinMapPath, g_map);

        const std::string pakName = g_paks[target].name;
        std::string base = pakName;
        if (base.size() > 4 && _stricmp(base.substr(base.size() - 4).c_str(), ".pak") == 0)
        {
            base = base.substr(0, base.size() - 4);
        }
        const std::string outDir = g_exeDir + "patched";
        CreateDirectoryA(outDir.c_str(), nullptr);
        const std::string outPak = outDir + "\\" + base + ".patched.pak";

        const std::string tool = g_exeDir + "..\\..\\FlPakTool\\Release\\FlPakTool.exe";
        const std::string cmd = "\"" + tool + "\" patch \"" + g_pakDir + "\\" + pakName +
                                "\" --manifest \"" + g_skinMapPath + "\" --out \"" + outPak + "\"";

        if (!RunCapture(cmd, g_patchOutput))
        {
            g_patchOutput = "[-] failed to launch FlPakTool (is it built?)\n" + cmd;
            return;
        }
        g_patchOutput += "\n[gui] output: " + outPak;
        GuiLog("patch run against %s - see Swap tab log", pakName.c_str());
    }

    bool ComboFromVector(const char* label, int* current, const std::vector<flibrary::Item>& items)
    {
        const auto getter = [](void* data, int idx) -> const char* {
            const auto* v = static_cast<const std::vector<flibrary::Item>*>(data);
            if (idx < 0 || idx >= static_cast<int>(v->size()))
            {
                return nullptr;
            }
            return (*v)[idx].name.c_str();
        };
        return ImGui::Combo(label, current, getter,
                            const_cast<std::vector<flibrary::Item>*>(&items),
                            static_cast<int>(items.size()));
    }

    bool ComboFromStrings(const char* label, int* current, const std::vector<std::string>& items)
    {
        const auto getter = [](void* data, int idx) -> const char* {
            const auto* v = static_cast<const std::vector<std::string>*>(data);
            if (idx < 0 || idx >= static_cast<int>(v->size()))
            {
                return nullptr;
            }
            return (*v)[idx].c_str();
        };
        return ImGui::Combo(label, current, getter,
                            const_cast<std::vector<std::string>*>(&items),
                            static_cast<int>(items.size()));
    }

    void DrawSwapEditor(const char* id, const std::vector<flibrary::Item>& items, int& comboIdx,
                        int& skinComboIdx, int& lastItemIdx, char* skinBuf,
                        std::unordered_map<std::string, std::string>& target)
    {
        if (items.empty())
        {
            ImGui::TextDisabled("library is empty");
            return;
        }
        if (comboIdx >= static_cast<int>(items.size()))
        {
            comboIdx = 0;
        }
        if (lastItemIdx != comboIdx)
        {
            lastItemIdx = comboIdx;
            skinComboIdx = 0;
        }

        ImGui::PushID(id);
        ComboFromVector("Item", &comboIdx, items);
        const auto& item = items[comboIdx];

        if (!item.skins.empty())
        {
            if (skinComboIdx >= static_cast<int>(item.skins.size()))
            {
                skinComboIdx = 0;
            }
            ComboFromStrings("Render as", &skinComboIdx, item.skins);
            ImGui::SameLine();
            ImGui::TextDisabled("%zu known skins", item.skins.size());
            ImGui::SameLine();
            if (ImGui::Button("Add"))
            {
                target[item.name] = item.skins[skinComboIdx];
            }
        }
        else
        {
            ImGui::InputText("Render as (skin name)", skinBuf, IM_ARRAYSIZE(skinBuf));
            ImGui::SameLine();
            if (ImGui::Button("Add") && skinBuf[0] != '\0')
            {
                target[item.name] = skinBuf;
                skinBuf[0] = '\0';
            }
        }

        if (!target.empty() && ImGui::BeginTable(id, 3, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Item");
            ImGui::TableSetupColumn("Renders as");
            ImGui::TableSetupColumn("");
            ImGui::TableHeadersRow();
            for (auto it = target.begin(); it != target.end();)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(it->first.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(it->second.c_str());
                ImGui::TableNextColumn();
                ImGui::PushID(it->first.c_str());
                if (ImGui::SmallButton("Remove"))
                {
                    it = target.erase(it);
                }
                else
                {
                    ++it;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopID();
    }

    void DrawSwapTab()
    {
        ImGui::TextWrapped(
            "Pick an item from the library, enter the skin name it should render as, and add it. "
            "The pak patcher (phase D) applies this set on next game launch.");
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Capsulers (Heroes)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawSwapEditor("heroes", g_library.heroes, g_heroComboIdx, g_heroSkinCombo,
                           g_heroLastItem, g_heroSkin, g_map.heroSkins);
        }
        if (ImGui::CollapsingHeader("Weapons", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawSwapEditor("weapons", g_library.weapons, g_weaponComboIdx, g_weaponSkinCombo,
                           g_weaponLastItem, g_weaponSkin, g_map.weaponSkins);
        }
        if (ImGui::CollapsingHeader("Vehicles", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawSwapEditor("vehicles", g_library.vehicles, g_vehicleComboIdx, g_vehicleSkinCombo,
                           g_vehicleLastItem, g_vehicleSkin, g_map.vehicleSkins);
        }
        ImGui::Spacing();
        if (ImGui::Button("Save swap set"))
        {
            g_status = flskin::Save(g_skinMapPath, g_map) ? "swap set saved to skinmap.json"
                                                         : "save failed (check folder permissions)";
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", g_status.c_str());
        ImGui::Spacing();
        ImGui::Separator();

        const size_t totalEntries = g_map.heroSkins.size() + g_map.weaponSkins.size() +
                                     g_map.vehicleSkins.size();
        ImGui::Text("Pak generation");
        ImGui::Spacing();
        ImGui::BeginDisabled(totalEntries == 0);
        if (ImGui::Button("Generate patched pak"))
        {
            GeneratePatchedPak();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (totalEntries == 0)
        {
            ImGui::TextDisabled("add at least one swap entry above to enable");
        }
        else
        {
            int target = g_selectedPak >= 0 && g_selectedPak < static_cast<int>(g_paks.size())
                             ? g_selectedPak
                             : LargestPakIndex();
            if (target >= 0)
            {
                ImGui::TextDisabled("target pak: %s (pick another on the Paks tab)",
                                    g_paks[target].name.c_str());
            }
        }

        if (!g_patchOutput.empty())
        {
            ImGui::Spacing();
            ImGui::BeginChild("patchlog", ImVec2(0.0f, 180.0f), ImGuiChildFlags_Borders);
            ImGui::TextUnformatted(g_patchOutput.c_str());
            ImGui::EndChild();
        }
    }

    void DrawLibraryTab()
    {
        ImGui::Text("Item library - %d Capsulers, %d weapons, %d vehicles",
                    static_cast<int>(g_library.heroes.size()),
                    static_cast<int>(g_library.weapons.size()),
                    static_cast<int>(g_library.vehicles.size()));
        ImGui::TextDisabled(
            "Source: Farlight 84 wiki (current roster). Per-item skin lists arrive with the phase B/C asset extraction.");
        ImGui::Spacing();

        const auto drawSection = [](const char* title, const char* groupColumn,
                                    const std::vector<flibrary::Item>& items) {
            if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }
            if (ImGui::BeginTable(title, 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn(groupColumn);
                ImGui::TableSetupColumn("Skins");
                ImGui::TableHeadersRow();
                for (const auto& item : items)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(item.group.c_str());
                    ImGui::TableNextColumn();
                    if (item.skins.empty())
                    {
                        ImGui::TextDisabled("-");
                    }
                    else
                    {
                        ImGui::Text("%zu", item.skins.size());
                    }
                }
                ImGui::EndTable();
            }
        };

        drawSection("Capsulers (Heroes)", "Role", g_library.heroes);
        drawSection("Weapons", "Class", g_library.weapons);
        drawSection("Vehicles", "Class", g_library.vehicles);
    }

    void DrawPaksTab()
    {
        const ULONGLONG now = GetTickCount64();
        if (now - g_lastScan > 10000)
        {
            g_lastScan = now;
            RescanPaks();
        }
        if (ImGui::Button("Rescan now"))
        {
            RescanPaks();
        }
        ImGui::SameLine();
        ImGui::Text("%d paks in %s", static_cast<int>(g_paks.size()), g_pakDir.c_str());
        ImGui::Spacing();

        if (g_paks.empty())
        {
            ImGui::TextWrapped("No hot-patch paks found. Is the game folder path correct / has the game run once?");
            return;
        }

        if (ImGui::BeginTable("paks", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Ver");
            ImGui::TableSetupColumn("Value1");
            ImGui::TableSetupColumn("Value2");
            ImGui::TableSetupColumn("Value3");
            ImGui::TableSetupColumn("Flag");
            ImGui::TableSetupColumn("Methods");
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (int i = 0; i < static_cast<int>(g_paks.size()); i++)
            {
                const auto& pak = g_paks[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable(pak.name.c_str(), g_selectedPak == i, ImGuiSelectableFlags_SpanAllColumns))
                {
                    g_selectedPak = i;
                }
                ImGui::TableNextColumn();
                if (pak.size >= 1024 * 1024)
                {
                    ImGui::Text("%.1f MB", pak.size / (1024.0 * 1024.0));
                }
                else
                {
                    ImGui::Text("%.1f KB", pak.size / 1024.0);
                }
                ImGui::TableNextColumn();
                ImGui::Text(pak.footer.parsed ? "%u" : "-", pak.footer.version);
                ImGui::TableNextColumn();
                ImGui::Text(pak.footer.parsed ? "0x%llX" : "-", static_cast<unsigned long long>(pak.footer.value1));
                ImGui::TableNextColumn();
                ImGui::Text(pak.footer.parsed ? "0x%llX" : "-", static_cast<unsigned long long>(pak.footer.value2));
                ImGui::TableNextColumn();
                ImGui::Text(pak.footer.parsed ? "0x%llX" : "-", static_cast<unsigned long long>(pak.footer.value3));
                ImGui::TableNextColumn();
                ImGui::Text(pak.footer.parsed ? "%u" : "-", pak.footer.flag);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(pak.footer.methods.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("Footer field names are provisional - semantics get locked in phase B (index decryption).");

        if (g_selectedPak >= 0 && g_selectedPak < static_cast<int>(g_paks.size()))
        {
            const auto& f = g_paks[g_selectedPak].footer;
            if (f.parsed && ImGui::TreeNode("Selected pak footer hash"))
            {
                char hex[64]{};
                for (int i = 0; i < 20; i++)
                {
                    std::snprintf(hex + i * 3, 4, "%02X ", f.hash[i]);
                }
                ImGui::TextUnformatted(hex);
                ImGui::TreePop();
            }
        }
    }

    void DrawInstallTab()
    {
        ImGui::Text("Game mount folder: %s", g_pakDir.c_str());
        ImGui::Text("Backup folder:      %s", g_backupDir.c_str());
        ImGui::Spacing();

        const bool haveBackups = flinstall::BackupsExist(g_backupDir);
        ImGui::Text("Originals backed up: %s", haveBackups ? "yes - originals are safe" : "NO - back up before anything else");
        ImGui::Spacing();

        if (ImGui::Button("Backup originals"))
        {
            const auto r = flinstall::BackupOriginals(g_pakDir, g_backupDir);
            g_status = r.message;
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore originals"))
        {
            const auto r = flinstall::RestoreOriginals(g_pakDir, g_backupDir);
            g_status = r.message;
        }
        ImGui::SameLine();
        if (ImGui::Button("Verify backups"))
        {
            const auto v = flinstall::VerifyBackups(g_pakDir, g_backupDir);
            g_verifyOk = v.ok;
            g_verifyMsg = v.message;
            GuiLog("verify: %s", v.message.c_str());
        }
        ImGui::TextDisabled("%s", g_status.c_str());
        if (!g_verifyMsg.empty())
        {
            if (g_verifyOk)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.4f, 1.0f));
            }
            ImGui::TextWrapped("%s", g_verifyMsg.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        ImGui::Separator();

        static char patchPath[MAX_PATH] = "";
        ImGui::InputText("Patched pak path", patchPath, IM_ARRAYSIZE(patchPath));
        ImGui::SameLine();
        ImGui::BeginDisabled(patchPath[0] == '\0' || GetFileAttributesA(patchPath) == INVALID_FILE_ATTRIBUTES);
        if (ImGui::Button("Install into game"))
        {
            const auto r = flinstall::InstallPatchedPak(patchPath, g_pakDir);
            g_status = r.message;
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("Install places a patched pak into the mount folder. Patched paks are produced by the phase D patch engine.");
    }

    void DrawAnalysisTab()
    {
        if (ImGui::BeginTable("pipeline", 2, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Step");
            ImGui::TableSetupColumn("State");
            ImGui::TableHeadersRow();
            const bool java = JavaRunning();
            const char* steps[6][2] = {
                {"1. Ghidra static verdict (pak signing on/off)", java ? "RUNNING - see log tail below" : "idle / finished"},
                {"2. AES key extraction from binary", "pending phase B"},
                {"3. Index decrypt + format parse", "pending phase B"},
                {"4. Cosmetic asset map", "pending phase C"},
                {"5. Patch engine (FlPakTool)", "pending phase D"},
                {"6. Live validation", "pending phase E"},
            };
            for (const auto& step : steps)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(step[0]);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(step[1]);
            }
            ImGui::EndTable();
        }

        const ULONGLONG now = GetTickCount64();
        if (now - g_lastLog > 2000)
        {
            g_lastLog = now;
            g_logTail = ReadLogTail(g_ghidraLogPath);
        }
        ImGui::Spacing();
        ImGui::Text("Ghidra log tail (auto-refresh 2s):");
        ImGui::BeginChild("log", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(g_logTail.c_str());
        ImGui::EndChild();
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam) != 0)
    {
        return 1;
    }
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    flenv::Load();

    using SetDpiCtxFn = BOOL(WINAPI*)(void*);
    const HMODULE user32 = GetModuleHandleA("user32.dll");
    bool dpiAware = false;
    if (user32 != nullptr)
    {
        const auto setDpiCtx =
            reinterpret_cast<SetDpiCtxFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setDpiCtx != nullptr)
        {
            dpiAware = setDpiCtx(reinterpret_cast<void*>(-4)) != FALSE;
        }
    }
    if (!dpiAware)
    {
        SetProcessDPIAware();
    }

    float dpiScale = 1.0f;
    {
        const HDC dc = GetDC(nullptr);
        const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(nullptr, dc);
        if (dpi > 0)
        {
            dpiScale = dpi / 96.0f;
        }
    }

    RECT workArea{};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);
    int winW = static_cast<int>(1180.0f * dpiScale);
    int winH = static_cast<int>(780.0f * dpiScale);
    const int maxW = static_cast<int>((workArea.right - workArea.left) * 0.95f);
    const int maxH = static_cast<int>((workArea.bottom - workArea.top) * 0.95f);
    if (maxW > 0 && winW > maxW) winW = maxW;
    if (maxH > 0 && winH > maxH) winH = maxH;

    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = "FlSkinGui";
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "FlSkinGui", "Farlight Skin Studio - pak route tool",
                                (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME),
                                CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr)
    {
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);

    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    const int clientW = static_cast<int>(clientRect.right - clientRect.left);
    const int clientH = static_cast<int>(clientRect.bottom - clientRect.top);

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferDesc.Width = clientW > 0 ? clientW : winW;
    scd.BufferDesc.Height = clientH > 0 ? clientH : winH;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;
    scd.OutputWindow = hwnd;
    scd.Windowed = TRUE;
    scd.SampleDesc.Count = 1;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               nullptr, 0, D3D11_SDK_VERSION, &scd, &g_swapChain,
                                               &g_device, nullptr, &g_context);
    if (FAILED(hr))
    {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                           nullptr, 0, D3D11_SDK_VERSION, &scd, &g_swapChain,
                                           &g_device, nullptr, &g_context);
    }
    if (FAILED(hr))
    {
        MessageBoxA(hwnd, "D3D11 init failed", "Farlight Skin Studio", MB_ICONERROR);
        return 1;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
    backBuffer->Release();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    flstyle::Apply(dpiScale);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    g_exeDir = ExeDir();
    const char* profile = std::getenv("USERPROFILE");
    const std::string defaultPakDir =
        (profile != nullptr ? std::string(profile) : std::string())
        + "\\AppData\\Local\\Solarland\\Saved_Steam\\Paks";
    g_pakDir = flenv::Get("FL_PAK_DIR", defaultPakDir);
    g_backupDir = g_exeDir + "backup_originals";
    g_skinMapPath = g_exeDir + "skinmap.json";
    g_ghidraLogPath = flenv::Get("FL_GHIDRA_LOG",
                                 g_exeDir + "..\\..\\..\\tools\\ghidra_analysis.log");
    flskin::Load(g_skinMapPath, g_map);
    flibrary::Load(g_library);
    g_paks = flpak::ScanDirectory(g_pakDir);
    g_lastScan = GetTickCount64();

    fopen_s(&g_debug, (g_exeDir + "FlSkinGui.log").c_str(), "w");
    if (g_debug != nullptr)
    {
        std::fclose(g_debug);
        g_debug = _fsopen((g_exeDir + "FlSkinGui.log").c_str(), "w", _SH_DENYNO);
    }
    GuiLog("Farlight Skin Studio started");
    GuiLog("dpi: aware=%d scale=%.2f window=%dx%d client=%dx%d",
           dpiAware ? 1 : 0, dpiScale, winW, winH, clientW, clientH);
    GuiLog("device=%p context=%p swapchain=%p rtv=%p",
           static_cast<void*>(g_device), static_cast<void*>(g_context),
           static_cast<void*>(g_swapChain), static_cast<void*>(g_rtv));
    GuiLog("pakdir: %s", g_pakDir.c_str());
    GuiLog("initial pak scan: %d paks", static_cast<int>(g_paks.size()));
    GuiLog("skinmap: %zu hero / %zu weapon / %zu vehicle entries",
           g_map.heroSkins.size(), g_map.weaponSkins.size(), g_map.vehicleSkins.size());
    GuiLog("library: %zu capsulers / %zu weapons / %zu vehicles",
           g_library.heroes.size(), g_library.weapons.size(), g_library.vehicles.size());
    size_t totalSkins = 0;
    for (const auto& item : g_library.heroes) totalSkins += item.skins.size();
    for (const auto& item : g_library.weapons) totalSkins += item.skins.size();
    for (const auto& item : g_library.vehicles) totalSkins += item.skins.size();
    GuiLog("library: %zu total known skins", totalSkins);

    const float clearColor[4] = {15.0f / 255.0f, 17.0f / 255.0f, 21.0f / 255.0f, 1.0f};

    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();

        static bool s_loggedFirstFrame = false;
        if (!s_loggedFirstFrame)
        {
            s_loggedFirstFrame = true;
            GuiLog("first frame rendering: display %.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("root", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 bandPos = ImGui::GetCursorScreenPos();
            const float bandWidth = ImGui::GetContentRegionAvail().x;
            const float bandHeight = 54.0f;
            dl->AddRectFilled(bandPos, ImVec2(bandPos.x + bandWidth, bandPos.y + bandHeight),
                              IM_COL32(23, 26, 35, 255), 8.0f);
            dl->AddRectFilled(ImVec2(bandPos.x + 8.0f, bandPos.y + 9.0f),
                              ImVec2(bandPos.x + 12.0f, bandPos.y + bandHeight - 9.0f),
                              IM_COL32(255, 179, 71, 255), 2.0f);
            if (flstyle::Bold != nullptr)
            {
                dl->AddText(flstyle::Bold, 21.0f, ImVec2(bandPos.x + 26.0f, bandPos.y + 7.0f),
                            IM_COL32(238, 242, 252, 255), "FARLIGHT SKIN STUDIO");
                dl->AddText(flstyle::Small, 13.0f, ImVec2(bandPos.x + 27.0f, bandPos.y + 32.0f),
                            IM_COL32(148, 156, 175, 255), "pak route - offline research build");
            }
            else
            {
                dl->AddText(ImVec2(bandPos.x + 26.0f, bandPos.y + 8.0f),
                            IM_COL32(238, 242, 252, 255), "FARLIGHT SKIN STUDIO");
                dl->AddText(ImVec2(bandPos.x + 27.0f, bandPos.y + 32.0f),
                            IM_COL32(148, 156, 175, 255), "pak route - offline research build");
            }
            ImGui::Dummy(ImVec2(0.0f, bandHeight + 12.0f));
        }

        if (ImGui::BeginTabBar("maintabs"))
        {
            if (ImGui::BeginTabItem("Swap Selection"))
            {
                DrawSwapTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Library"))
            {
                DrawLibraryTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Paks"))
            {
                DrawPaksTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Install"))
            {
                DrawInstallTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Analysis"))
            {
                DrawAnalysisTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

    ImGui::Render();
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_context->ClearRenderTargetView(g_rtv, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapChain->Present(1, 0);
    }

    flskin::Save(g_skinMapPath, g_map);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_rtv) g_rtv->Release();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    DestroyWindow(hwnd);
    return 0;
}
