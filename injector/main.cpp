#include <windows.h>
#include <tlhelp32.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    DWORD FindProcessIdByName(const char* name)
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        DWORD pid = 0;

        if (Process32First(snap, &pe))
        {
            do
            {
                if (_stricmp(pe.szExeFile, name) == 0)
                {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(snap, &pe));
        }

        CloseHandle(snap);
        return pid;
    }
}

int main(int argc, char** argv)
{
    bool wait = false;
    int firstArg = 1;

    if (argc >= 2 && _stricmp(argv[1], "--wait") == 0)
    {
        wait = true;
        firstArg = 2;
    }

    if (argc < firstArg + 2)
    {
        std::printf("Farlight Skin Changer injector\n");
        std::printf("Usage:\n");
        std::printf("  FlInjector.exe [--wait] <process name or PID> <full path to DLL>\n");
        std::printf("  --wait  keep polling until the target process appears (start injector before the game)\n");
        std::printf("Example:\n");
        std::printf("  FlInjector.exe notepad.exe D:\\FarlightSkinChanger\\build\\FarlightCore\\Release\\FarlightCore.dll\n");
        return 0;
    }

    const std::string target = argv[firstArg];
    const std::string dllArg = argv[firstArg + 1];

    DWORD pid = 0;
    if (std::isdigit(static_cast<unsigned char>(target[0])))
    {
        pid = static_cast<DWORD>(std::atoi(target.c_str()));
    }
    else
    {
        pid = FindProcessIdByName(target.c_str());
    }

    if (pid == 0)
    {
        if (!wait)
        {
            std::printf("[-] Process '%s' not found (is it running?)\n", target.c_str());
            return 1;
        }
        std::printf("[i] Waiting for '%s' to start (poll every 500 ms, up to 10 min)...\n",
                    target.c_str());
        for (int i = 0; i < 1200 && pid == 0; i++)
        {
            Sleep(500);
            pid = FindProcessIdByName(target.c_str());
        }
        if (pid == 0)
        {
            std::printf("[-] Timed out waiting for '%s'\n", target.c_str());
            return 1;
        }
    }
    std::printf("[+] Target: %s (PID %lu)\n", target.c_str(), pid);

    char dllPath[MAX_PATH]{};
    GetFullPathNameA(dllArg.c_str(), MAX_PATH, dllPath, nullptr);
    if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES)
    {
        std::printf("[-] DLL not found: %s\n", dllPath);
        return 1;
    }
    std::printf("[+] DLL: %s\n", dllPath);

    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);
    if (process == nullptr)
    {
        std::printf("[-] OpenProcess failed (error %lu). Try running as the same user, or elevated.\n",
                    GetLastError());
        return 1;
    }

    const SIZE_T bytes = std::strlen(dllPath) + 1;
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote == nullptr)
    {
        std::printf("[-] VirtualAllocEx failed (error %lu)\n", GetLastError());
        CloseHandle(process);
        return 1;
    }

    if (WriteProcessMemory(process, remote, dllPath, bytes, nullptr) == FALSE)
    {
        std::printf("[-] WriteProcessMemory failed (error %lu)\n", GetLastError());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }
    std::printf("[+] DLL path written into target process\n");

    const auto loadLibraryAddr = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA")));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibraryAddr, remote, 0, nullptr);
    if (thread == nullptr)
    {
        std::printf("[-] CreateRemoteThread failed (error %lu)\n", GetLastError());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }

    WaitForSingleObject(thread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);

    if (exitCode == 0)
    {
        std::printf("[-] LoadLibraryA in target returned NULL - injection likely blocked or failed\n");
        return 1;
    }

    std::printf("[+] Injection OK (LoadLibraryA returned %lu)\n", exitCode);
    std::printf("[i] Check for a FarlightCore.log next to the DLL\n");
    return 0;
}
