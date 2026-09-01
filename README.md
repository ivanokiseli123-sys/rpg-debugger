# NeonDebugger

A Windows x64 developer scripting/debugging sandbox with a dark neon interface inspired by executor-style tools. It is intended for software you own or are authorized to test.

## Features

- Windows x64 desktop GUI
- Lua scripting editor and output console
- Process selection and attach/detach for authorized development/debugging
- Explicit memory read/write helpers for your own test software
- No Roblox injection, anti-cheat bypass, stealth, persistence, or multiplayer cheating

## Build the EXE

GitHub Actions builds the Windows x64 Release package on pushes to `main` and can also be started manually from **Actions → Build Windows EXE → Run workflow**.

The workflow produces `NeonDebugger-Windows-x64.zip`, containing `NeonDebugger.exe`.

A version tag such as `v1.0.0` also creates a GitHub Release with the ZIP attached.

## Local build

Install Visual Studio 2022 with the Desktop C++ workload and CMake, then:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is produced at `build\\Release\\NeonDebugger.exe`.

## Scope

This project is a legitimate development/debugging sandbox. It does not implement Roblox exploit injection or anti-cheat bypass functionality.
