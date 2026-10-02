# Minecraft Pocket Edition 0.6.1 for Windows RT / Surface RT

[![Platform](https://img.shields.io/badge/Platform-Windows%20RT%20%7C%20ARM32%20(ARMv7)-blue.svg)]()
[![Graphics](https://img.shields.io/badge/Graphics-Direct3D%2011%20(Hardware)-brightgreen.svg)]()
[![Language](https://img.shields.io/badge/Language-C%2B%2B11-orange.svg)]()

A native port of **Minecraft Pocket Edition v0.6.1** for Microsoft Surface RT and Windows RT 8.1 ARM32 devices, featuring native hardware acceleration via a custom **Direct3D 11** backend, full touch and keyboard/mouse input, fixed multiplayer, and comprehensive quality-of-life improvements.

---

---

## Repository Structure

```
.
├── src/               # Game core source code (client, server, world, raknet, etc.)
├── d3d11_port/        # Direct3D 11 graphics shim & OpenGL ES emulation headers
├── data/              # Game assets (textures, audio, fonts, lang files)
├── libs/              # Precompiled static libraries and headers (libpng, zlib, glew)
├── project/           # Visual Studio solution / project files
├── build_arm32.sh     # Linux cross-compilation script (using llvm-mingw)
├── build_arm32.bat    # Windows build script for ARM32
├── build_arm64.bat    # Windows build script for ARM64
├── compile_list.txt   # File compilation list
└── README.md
```

---

## Building from Source

### Prerequisites

You need the **LLVM-MinGW** cross-compiler toolchain supporting `armv7-w64-mingw32` (for ARM32 / Windows RT) or `aarch64-w64-mingw32` (for ARM64).

- Download LLVM-MinGW: [github.com/mstorsjo/llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases)

### Building on Linux (Cross-compile)

1. Make sure `LLVM_MINGW_ROOT` points to your toolchain directory:
   ```bash
   export LLVM_MINGW_ROOT=/path/to/llvm-mingw
   ```
2. Run the build script:
   ```bash
   chmod +x build_arm32.sh
   ./build_arm32.sh --release
   ```
   *For debug build with verbose logging:*
   ```bash
   ./build_arm32.sh --debug
   ```

The compiled binary and assets will be output to the `dist/` directory.

### Building on Windows

1. Set `MINGW` environment variable to your LLVM-MinGW folder:
   ```cmd
   set MINGW=C:\llvm-mingw
   ```
2. Run:
   ```cmd
   build_arm32.bat
   ```

---

## Running on Windows RT

1. Ensure has **Windows RT Jailbreak** enabled to run desktop applications.
2. Copy the entire `dist/` folder containing:
   - `MinecraftWinARM32_D3D11.exe`
   - `data/` folder
3. Sign the MinecraftWinARM32_D3D11.exe file (use SignTool) for Windows RT
3. Launch `MinecraftWinARM32_D3D11.exe`.

---

## License & Credits

- Original Minecraft Pocket Edition by **Mojang / Microsoft**.
- Windows RT / ARM32 Direct3D 11 port and fixes by project contributors.
- This project is intended for historical preservation and educational purposes.
