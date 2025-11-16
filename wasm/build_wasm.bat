@echo off
REM HashLink WASM Build Script for Windows
REM Builds libhl.a with Emscripten for WebAssembly target

setlocal enabledelayedexpansion

echo =================================
echo HashLink WASM Build (Windows)
echo =================================
echo.

REM Check for Emscripten
where emcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: emcc not found!
    echo.
    echo Please install and activate Emscripten:
    echo   git clone https://github.com/emscripten-core/emsdk.git
    echo   cd emsdk
    echo   emsdk install latest
    echo   emsdk activate latest
    echo   emsdk_env.bat
    echo.
    exit /b 1
)

echo [+] Found Emscripten
emcc --version | findstr "emcc"
echo.

REM Get script directory and project root
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_ROOT%\build-wasm

REM Create build directory
echo Creating build directory: %BUILD_DIR%
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Configure with CMake
echo.
echo Configuring with CMake...
echo   Toolchain: cmake/Emscripten.cmake
echo   Build type: MinSizeRel
echo   WITH_VM: OFF
echo   Threading: DISABLED
echo.

emcmake cmake "%PROJECT_ROOT%" ^
    -DCMAKE_TOOLCHAIN_FILE="%PROJECT_ROOT%/cmake/Emscripten.cmake" ^
    -DCMAKE_BUILD_TYPE=MinSizeRel ^
    -DWITH_VM=OFF ^
    -DBUILD_SHARED_LIBS=OFF

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed!
    exit /b 1
)

REM Build
echo.
echo Building libhl.a...
echo.

emmake make -j%NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    exit /b 1
)

REM Check output
echo.
echo =================================
echo Build Complete!
echo =================================
echo.

if exist "%BUILD_DIR%\bin\libhl.a" (
    echo [+] libhl.a created
    echo   Location: %BUILD_DIR%\bin\libhl.a
    for %%A in ("%BUILD_DIR%\bin\libhl.a") do echo   Size: %%~zA bytes
) else (
    echo [!] ERROR: libhl.a not found!
    exit /b 1
)

echo.
echo Next steps:
echo   cd %SCRIPT_DIR%
echo   run_test.bat
echo.

endlocal
