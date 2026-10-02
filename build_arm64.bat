@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM  Build Script for Minecraft PE 0.6.1 (ARM64 D3D11) (NOT TESTED!!!)
REM  For Windows with llvm-mingw toolchain
REM ==============================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

if "%MINGW%"=="" (
    if exist "C:\llvm-mingw" set "MINGW=C:\llvm-mingw"
    if exist "%USERPROFILE%\llvm-mingw" set "MINGW=%USERPROFILE%\llvm-mingw"
    if exist "%ROOT%\toolchain" set "MINGW=%ROOT%\toolchain"
)

if "%MINGW%"=="" (
    echo [ERROR] MINGW path not set. Please set MINGW environment variable.
    exit /b 1
)

set "SRC=%ROOT%\src"
set "BUILD=%ROOT%\build"
set "OBJ=%BUILD%\obj-arm64"
set "LIBSA64=%ROOT%\libs\arm64"
set "GLPORT=%ROOT%\d3d11_port"
set "OUT=%BUILD%\MinecraftWinARM64_D3D11.exe"
set "DIST=%ROOT%\dist_arm64"
set "LIST=%ROOT%\compile_list.txt"

set "CXX=%MINGW%\bin\aarch64-w64-mingw32-clang++.exe"
set "PATH=%MINGW%\bin;%PATH%"

if not exist "%CXX%" (
    echo [ERROR] Compiler not found: %CXX%
    exit /b 1
)

if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%DIST%" mkdir "%DIST%"

echo === 1/3 Compiling D3D11 shim ===
%CXX% -c -w -Wno-c++11-narrowing -O2 -std=gnu++11 -DWIN32 -DNO_EGL -DGLEW_STATIC -Dd3d11_port -DMCPE_D3D11_NO_LOG -I "%GLPORT%\include" "%GLPORT%\d3d11_gl.cpp" -o "%OBJ%\d3d11_gl.o"
if errorlevel 1 ( echo [ERROR] Failed to compile D3D11 shim & exit /b 1 )

echo === 2/3 Compiling game sources ===
for /f "usebackq tokens=1,2 delims=	" %%a in ("%LIST%") do (
    if not exist "%OBJ%\%%b" (
        %CXX% -c -w -Wno-c++11-narrowing -O2 -std=gnu++11 -DWIN32 -DNO_EGL -DGLEW_STATIC -Dd3d11_port -I "%GLPORT%\include" -I "%LIBSA64%\include" -I "%SRC%" "%ROOT%\%%a" -o "%OBJ%\%%b"
        if errorlevel 1 ( echo [ERROR] Failed compiling %%a & exit /b 1 )
    )
)

echo === 3/3 Linking executable ===
set "OBJS="
for /f "usebackq tokens=1,2 delims=	" %%a in ("%LIST%") do (
    set "OBJS=!OBJS! %OBJ%\%%b"
)

%CXX% -mwindows -static -s -o "%OUT%" !OBJS! "%OBJ%\d3d11_gl.o" -L "%LIBSA64%" -lpng -lzlib -ld3d11 -ldxgi -luser32 -lgdi32 -lws2_32 -lshell32 -ladvapi32 -lwinmm -lole32 -loleaut32 -luuid -limm32 -lcomdlg32
if errorlevel 1 ( echo [ERROR] Link failed & exit /b 1 )

echo === Packaging to dist ===
copy /Y "%OUT%" "%DIST%\" >nul
if not exist "%DIST%\data" xcopy /E /I /Y "%ROOT%\data" "%DIST%\data" >nul

echo === Done! Built: %OUT%
endlocal
exit /b 0
