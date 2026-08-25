# RPG Debugger

A Windows-only single-player development debugger for your own RPG. It provides a dark Win32 GUI, process selection/attachment, Lua scripting, and explicit memory read/write helpers.

## Features

- Windows x64 GUI
- Process enumeration and attach/detach
- `ReadProcessMemory` / `WriteProcessMemory` wrappers
- Embedded Lua 5.4.7
- Lua helpers for `i32`, `f32`, `u64`, and byte buffers
- Script editor and output console
- F5 script execution
- GitHub Actions build producing `RPGDebugger-Windows-x64.zip`

## Lua examples

```lua
-- Replace these addresses with addresses from your own development build.
local health = mem.read_i32("0x12345678")
mem.log("health = " .. health)
mem.write_i32("0x12345678", 100)

local speed = mem.read_f32("0x1234567C")
mem.write_f32("0x1234567C", speed * 2.0)
```

Available functions:

- `mem.read_i32(address)`
- `mem.read_f32(address)`
- `mem.read_u64(address)`
- `mem.read_bytes(address, length)`
- `mem.write_i32(address, value)`
- `mem.write_f32(address, value)`
- `mem.write_u64(address, value)`
- `mem.write_bytes(address, string)`
- `mem.log(message)`

Addresses accept Lua integers or strings such as `"0x12345678"`.

## Build

The repository includes a GitHub Actions workflow. Push to `main` or run **Actions → Build Windows EXE → Run workflow**. The workflow compiles the x64 Release executable and uploads `RPGDebugger-Windows-x64.zip` as an artifact.

For local Windows builds, install Visual Studio 2022 with the Desktop C++ workload and CMake, then run:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is produced at `build\\Release\\RPGDebugger.exe`.

## Safety / scope

This is intended for debugging software you own or are authorized to test. It does not contain DLL injection, anti-cheat bypasses, stealth, persistence, or multiplayer cheating features.
