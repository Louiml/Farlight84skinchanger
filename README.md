# Farlight Skin Changer

A skin changer for Farlight 84, and the tooling built while trying to build one. The short version: the game ships two kernel anti-cheat drivers (Lilith LLH and NetEase NEP), and they block every route into the game process. Tested normal injection, elevated injection, and injection timed at process start. All denied, for admin accounts too. So the work moved to the game's own files on disk, and this repo is the result.

## What's in here

Six Visual Studio projects. Open `FarlightSkinChanger.sln` in VS 2022 (C++ workload, x64) and build Release. Dependencies (ImGui 1.91.8, MinHook, nlohmann/json) are vendored as plain files under `third_party/`. No submodules.

- **FarlightCore** is the injected DLL. MinHook wrapper, IDA-style pattern scanner, JSON config, and an ImGui overlay that hooks D3D11 Present without needing game offsets. It runs in any D3D11 process that lacks kernel protection.
- **FlInjector** injects DLLs via CreateRemoteThread and LoadLibraryA. Has a `--wait` mode that polls until the target process appears.
- **FlSkinGui** is the desktop app. Item library (15 Capsulers, 21 weapons, 13 vehicles, 47 skins scraped from the game's own patch notes), pak browser with parsed footers, backup/verify/restore with SHA-256, and the patch engine runner.
- **FlPakTool** is the pak research CLI. Footer parser, AES-256 ECB/CBC via BCrypt, a mount point detector, and a key scanner that ran 408 million AES trials over the game binary hunting for the pak encryption key.
- **FlTestWindow** is a D3D11 test harness. Inject FarlightCore into it to verify the whole overlay chain without touching the game.
- **tools/** holds the Ghidra headless launcher, the analysis scripts (light profile, signing verdict, deep decompiler probe), and the library generator.

## Setup

Copy `.env.example` to `.env` and fill in your local paths. The GUI reads it at startup, and the Ghidra launcher refuses to run without it. Real environment variables take precedence over the file.

## What works

The GUI, the CLI, the injection framework, and the test chain all run end to end. The library generator scrapes the game's 36 patch note pages, merges a manual skins file, and emits the C++ data header. The Generate button in the GUI runs the patch engine, which loads the swap set, copies the target pak, then stops there, because no swap can be written until the pak index can be read.

## What is blocked

Farlight's paks use a customized UE4 V9 format. The index is encrypted. A stride-1 scan of all 195 MB of the shipping executable, trying every byte position as an AES key candidate against a known-plaintext mount point, found nothing. The key is derived at runtime, or the index region layout guess is wrong. Either way the answer sits in the code, which is what the Ghidra scripts exist to read. The signing question, whether a modified pak could even pass a signature check, is still open. It is the go or no-go gate for the whole pak route.

Injection into the game itself is off the table while those two drivers are loaded. This repo does not include an anti-cheat bypass and never will.

## Legal

Skin changers break the game's terms of service. If you point any of this at an online game, use a throwaway account and accept the ban risk. The repository is research tooling for reading game files that sit on your own disk.

## Credits

ImGui, MinHook, and nlohmann/json are vendored under `third_party/`. Dumper-7 is not included in the repo, clone it yourself from Encryqed's repository if you want the SDK dumper.
