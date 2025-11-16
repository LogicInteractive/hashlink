@echo off
REM HashLink WASM Test Runner for Windows
REM Compiles Haxe test to C, then to WASM, and serves via HTTP

setlocal enabledelayedexpansion

echo =================================
echo HashLink WASM Test Runner (Windows)
echo =================================
echo.

REM Check for required tools
where haxe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: haxe not found!
    echo Please install Haxe from https://haxe.org/download/
    exit /b 1
)

where emcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: emcc not found!
    echo Please install and activate Emscripten ^(see wasm/README.md^)
    exit /b 1
)

echo [+] Found Haxe
haxe --version 2>&1
echo [+] Found Emscripten
emcc --version | findstr "emcc"
echo.

REM Get directories
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_ROOT%\build-wasm

REM Check for libhl.a
if not exist "%BUILD_DIR%\bin\libhl.a" (
    echo ERROR: libhl.a not found!
    echo Please run: build_wasm.bat
    exit /b 1
)

echo [+] Found libhl.a
for %%A in ("%BUILD_DIR%\bin\libhl.a") do echo   Size: %%~zA bytes
echo.

REM Step 1: Compile Haxe to C
echo Step 1: Compiling Haxe to C...
cd /d "%SCRIPT_DIR%"

haxe -hl test.c -main Test

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Haxe compilation failed!
    exit /b 1
)

if not exist "test.c" (
    echo ERROR: test.c not generated!
    exit /b 1
)

for /f %%A in ('find /c /v "" ^< test.c') do set LINE_COUNT=%%A
echo [+] Generated test.c ^(%LINE_COUNT% lines^)
echo.

REM Step 2: Compile C to WASM
echo Step 2: Compiling C to WASM...
echo   Optimization: -Oz ^(size^)
echo   Closure: enabled
echo   Memory: growth allowed
echo.

emcc test.c -o test.html ^
    -I"%PROJECT_ROOT%/src" ^
    -I"%PROJECT_ROOT%/include/pcre" ^
    -L"%BUILD_DIR%/bin" ^
    -lhl ^
    -s WASM=1 ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s EXPORTED_RUNTIME_METHODS="[\"ccall\",\"cwrap\"]" ^
    -s EXPORTED_FUNCTIONS="[\"_main\"]" ^
    -s TOTAL_STACK=5242880 ^
    -s TOTAL_MEMORY=16777216 ^
    --shell-file test_template.html ^
    -Oz ^
    --closure 1 ^
    2>&1 | findstr /V "warning:"

if %ERRORLEVEL% NEQ 0 if not exist "test.wasm" (
    echo ERROR: WASM compilation failed!
    exit /b 1
)

if not exist "test.wasm" (
    echo ERROR: WASM compilation failed!
    exit /b 1
)

echo.
echo [+] Generated test.html
for %%A in ("test.js") do echo [+] Generated test.js ^(%%~zA bytes^)
for %%A in ("test.wasm") do echo [+] Generated test.wasm ^(%%~zA bytes^)
echo.

REM Step 3: Serve via HTTP
echo =================================
echo Build Successful!
echo =================================
echo.
echo To test in browser:
echo.
echo   1. Start HTTP server:
echo      cd %SCRIPT_DIR%
echo      python -m http.server 8080
echo.
echo   2. Open in browser:
echo      http://localhost:8080/test.html
echo.
echo   3. Check browser console for output
echo.

REM Check for Python
where python >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set /p ANSWER="Start HTTP server now? (y/n) "
    if /i "!ANSWER!"=="y" (
        echo.
        echo Starting server on http://localhost:8080
        echo Press Ctrl+C to stop
        echo.
        python -m http.server 8080
    )
) else (
    echo Note: Python not found. Install Python to use built-in HTTP server.
    echo Alternative: Use any HTTP server to serve the wasm directory.
)

endlocal
