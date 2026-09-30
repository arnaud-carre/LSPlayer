# Repository Guidelines

## Project Structure & Module Organization

Light Speed Player combines a desktop MOD converter with Amiga 68000 replay code. `src/` contains the C++ converter, encoder/decoder, Paula simulation, WAV writer, and Visual Studio solution. `src/external/` vendors micromod and Shrinkler; preserve their licenses and keep dependency changes focused. Root `LightSpeedPlayer*.asm` files implement standard, CIA-driven, and micro playback. `Example/` contains assembly integration examples and a sample MOD with converted assets. `png/` holds documentation images; `versions.txt` records releases. There is no dedicated test directory.

## Build, Test, and Development Commands

Use Visual Studio 2022 with the v143 C++ toolset and Windows SDK. Open `src/LSPConvert.sln`, or run from a Developer PowerShell:

```powershell
msbuild src/LSPConvert.sln /m /p:Configuration=Release /p:Platform=x64
```

This builds the root `LSPConvert.exe`; use `Configuration=Debug` for `LSPConvert_d.exe`. Release builds replace the tracked executable, so review that change before committing. The README also documents CMake, but `CMakeLists.txt` is absent from the current working tree.

Run a conversion with explicit scratch outputs to preserve checked-in examples:

```powershell
.\LSPConvert.exe Example/rink-a-dink.mod -lsmusic "$env:TEMP/lsp-check.lsmusic" -lsbank "$env:TEMP/lsp-check.lsbank" -amigapreview -wav "$env:TEMP/lsp-check.wav"
```

## Coding Style & Naming Conventions

Follow adjacent C++ code: tabs for indentation, braces on separate lines, PascalCase types/functions, and `m_` member prefixes. Match existing constant conventions locally. Keep assembly labels, instruction alignment, and register conventions consistent with surrounding code. No formatter or lint configuration is checked in; avoid unrelated reformatting.

## Testing Guidelines

No automated test framework, test naming convention, or coverage threshold is configured. Build affected configurations and compare conversion outputs and WAV previews against a baseline. Exercise relevant options such as `-micro`, `-insane`, `-adpcm`, and `-looppreview`, directing generated files to scratch paths. For player changes, validate playback on an Amiga emulator or hardware and report timing, tempo, looping, and sample behavior.

## Commit & Pull Request Guidelines

History uses short descriptive subjects and version-oriented release commits, without a mandatory prefix. Keep commits focused. PRs should explain the behavior change, affected playback modes, reproduction commands, and validation results. Link relevant issues and include audio or timing evidence when useful. Update README options or `versions.txt` for user-visible changes. Preserve existing working-tree edits and review generated assets and executable changes deliberately.
