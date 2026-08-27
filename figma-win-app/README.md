# Neon Debugger — Figma-style Windows app

This directory contains the Windows C++/Dear ImGui implementation based on the supplied Figma ZIP.

## Build

Visual Studio 2022 + CMake:

```powershell
cmake -S figma-win-app -B figma-win-app/build -A x64
cmake --build figma-win-app/build --config Release
```

GitHub Actions builds `NeonDebugger-Windows-x64.zip` automatically on this branch. The ZIP contains the real `NeonDebugger.exe` executable at its root.

## Implemented

- Neon dark UI with cyan/pink accents
- Left control sidebar
- Process selector/status area
- Lua editor with line numbers
- Lua 5.4 embedded runtime
- Console with timestamps and severity colors
- Save/load scripts
- Execute button / F5-style execution
- Wallpaper chooser using `SystemParametersInfoW`
- Random wallpaper from the user's Pictures folder
- Attach/detach to the designated `NeonDebugTarget.exe` test process
- Safe read/write/module/process Lua helpers for that designated development target
- Auto-attach watcher
- Kill button restricted to the designated development target

## Important scope

The supplied design described Roblox DLL injection and arbitrary Roblox memory manipulation. Those external-client injection/executor capabilities are intentionally not implemented. The app instead provides the same debugger/editor workflow against `NeonDebugTarget.exe`, a process you own and launch for development/testing.
