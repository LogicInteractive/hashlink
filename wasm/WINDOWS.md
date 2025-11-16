# HashLink WASM - Windows Guide

This guide covers running the HashLink WASM POC on Windows.

## Prerequisites

### 1. Install Emscripten SDK

```cmd
# Download Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install and activate latest version
emsdk install latest
emsdk activate latest

# Set up environment (run this each time, or add to startup)
emsdk_env.bat
```

**Important**: You need to run `emsdk_env.bat` in each new command prompt before building.

To make it permanent, add Emscripten to your PATH:
1. Open System Properties → Environment Variables
2. Add to PATH: `C:\path\to\emsdk`
3. Add to PATH: `C:\path\to\emsdk\upstream\emscripten`

### 2. Install Haxe

**Option A: Using Chocolatey**
```cmd
choco install haxe
```

**Option B: Manual Install**
1. Download installer from https://haxe.org/download/
2. Run the installer
3. Haxe will be added to PATH automatically

Verify installation:
```cmd
haxe --version
```

### 3. Install CMake

**Option A: Using Chocolatey**
```cmd
choco install cmake
```

**Option B: Manual Install**
1. Download from https://cmake.org/download/
2. Run installer and select "Add to PATH"

Verify installation:
```cmd
cmake --version
```

### 4. Install Python (for HTTP server)

**Option A: Using Chocolatey**
```cmd
choco install python
```

**Option B: Manual Install**
1. Download from https://www.python.org/downloads/
2. Run installer and check "Add to PATH"

Verify installation:
```cmd
python --version
```

## Quick Start (Windows)

```cmd
# 1. Open Command Prompt and activate Emscripten
cd C:\path\to\emsdk
emsdk_env.bat

# 2. Navigate to hashlink directory
cd C:\path\to\hashlink

# 3. Build libhl for WASM
wasm\build_wasm.bat

# 4. Run the test
wasm\run_test.bat

# 5. Open browser to http://localhost:8080/test.html
```

## Using PowerShell

All the batch scripts work in PowerShell too:

```powershell
# Navigate to hashlink
cd C:\path\to\hashlink

# Build
.\wasm\build_wasm.bat

# Test
.\wasm\run_test.bat
```

## Using Git Bash (Alternative)

If you have Git for Windows installed, you can use the Linux scripts:

```bash
# In Git Bash
cd /c/path/to/hashlink

# Build
./wasm/build_wasm.sh

# Test
./wasm/run_test.sh
```

## Using WSL (Windows Subsystem for Linux)

You can also use WSL for a full Linux environment:

```bash
# In WSL
cd /mnt/c/path/to/hashlink

# Follow Linux instructions from README.md
./wasm/build_wasm.sh
./wasm/run_test.sh
```

## Manual Build (Windows CMD)

If the scripts don't work, here's the manual process:

### Build libhl.a

```cmd
# Activate Emscripten
cd C:\path\to\emsdk
emsdk_env.bat

# Create build directory
cd C:\path\to\hashlink
mkdir build-wasm
cd build-wasm

# Configure
emcmake cmake .. ^
    -DCMAKE_TOOLCHAIN_FILE=../cmake/Emscripten.cmake ^
    -DCMAKE_BUILD_TYPE=MinSizeRel ^
    -DWITH_VM=OFF ^
    -DBUILD_SHARED_LIBS=OFF

# Build
emmake make -j%NUMBER_OF_PROCESSORS%
```

### Compile Test

```cmd
# Go to wasm directory
cd C:\path\to\hashlink\wasm

# Compile Haxe to C
haxe -hl test.c -main Test

# Compile C to WASM
emcc test.c -o test.html ^
    -I..\src ^
    -I..\include\pcre ^
    -L..\build-wasm\bin ^
    -lhl ^
    -s WASM=1 ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s EXPORTED_RUNTIME_METHODS="[\"ccall\",\"cwrap\"]" ^
    -s EXPORTED_FUNCTIONS="[\"_main\"]" ^
    --shell-file test_template.html ^
    -Oz

# Start HTTP server
python -m http.server 8080
```

Then open http://localhost:8080/test.html in your browser.

## Troubleshooting

### "emcc is not recognized"

You forgot to run `emsdk_env.bat`. Run it in your current command prompt:
```cmd
cd C:\path\to\emsdk
emsdk_env.bat
```

### "haxe is not recognized"

Haxe is not in your PATH. Either:
- Reinstall Haxe and check "Add to PATH"
- Or manually add `C:\HaxeToolkit\haxe` to PATH

### "cmake is not recognized"

CMake is not in your PATH. Either:
- Reinstall CMake and check "Add to PATH"
- Or manually add `C:\Program Files\CMake\bin` to PATH

### "python is not recognized"

Python is not installed or not in PATH. Either:
- Install Python and check "Add to PATH"
- Or use an alternative HTTP server (see below)

### Alternative HTTP Servers (if Python not available)

**Using Node.js http-server:**
```cmd
npm install -g http-server
cd C:\path\to\hashlink\wasm
http-server -p 8080
```

**Using PHP:**
```cmd
cd C:\path\to\hashlink\wasm
php -S localhost:8080
```

**Using Visual Studio Code:**
- Install "Live Server" extension
- Right-click test.html → "Open with Live Server"

### Build fails with "cannot find -lhl"

The libhl.a library wasn't built successfully. Check:
1. Did `build_wasm.bat` complete without errors?
2. Does `build-wasm\bin\libhl.a` exist?
3. Try rebuilding:
   ```cmd
   rmdir /s /q build-wasm
   wasm\build_wasm.bat
   ```

### WASM file very large (>10MB)

The optimization flags might not be working. Try:
```cmd
# Rebuild with explicit optimization
emcc test.c -o test.html ... -Oz -flto --closure 1
```

### Browser shows blank page

1. Check browser console (F12) for errors
2. Make sure you're using HTTP server (not file://)
3. Check that all files exist:
   - test.html
   - test.js
   - test.wasm

### Runtime error: "memory access out of bounds"

Increase stack/memory size:
```cmd
emcc ... -s TOTAL_STACK=10485760 -s TOTAL_MEMORY=33554432
```

## Windows-Specific Notes

### Line Endings

Git might convert line endings. If you get errors about `\r`, run:
```cmd
git config core.autocrlf false
```

Then re-clone or reset the files.

### Path Separators

The scripts use forward slashes `/` internally but Windows paths with backslashes `\` work fine in commands.

### Long Paths

If you get "path too long" errors, enable long path support:
1. Run as Administrator: `gpedit.msc`
2. Navigate to: Local Computer Policy → Computer Configuration → Administrative Templates → System → Filesystem
3. Enable "Enable Win32 long paths"

Or use a shorter path like `C:\hl\` instead of deeply nested directories.

### Antivirus

Some antivirus software might slow down compilation. Add these to exclusions:
- Emscripten SDK directory
- HashLink build directory
- Your project directory

## File Locations (Windows)

After building, you'll find:

```
C:\path\to\hashlink\
├── build-wasm\
│   └── bin\
│       └── libhl.a          # WASM runtime library
└── wasm\
    ├── test.c               # Generated from Haxe
    ├── test.html            # Test page
    ├── test.js              # Emscripten runtime
    └── test.wasm            # Compiled WASM module
```

## Performance Tips

### Use MinGW Make (Faster)

Instead of `emmake make`, you can use MinGW make:

1. Install MinGW: `choco install mingw`
2. Use: `emmake mingw32-make -j%NUMBER_OF_PROCESSORS%`

### Use Ninja (Even Faster)

1. Install Ninja: `choco install ninja`
2. Configure with: `emcmake cmake .. -G Ninja ...`
3. Build with: `emmake ninja`

### Parallel Builds

Use all CPU cores:
```cmd
set NUMBER_OF_PROCESSORS=8
emmake make -j%NUMBER_OF_PROCESSORS%
```

## Success Criteria

You know it's working when:
1. ✅ `build_wasm.bat` completes without errors
2. ✅ `libhl.a` is created in `build-wasm\bin\`
3. ✅ `run_test.bat` completes without errors
4. ✅ `test.wasm` is created in `wasm\`
5. ✅ Browser shows test output in console
6. ✅ "All tests completed successfully!" appears

## Getting Help

If you're stuck:
1. Check this guide's troubleshooting section
2. See main `wasm\README.md` for general info
3. Check Emscripten docs: https://emscripten.org/docs/
4. Open an issue with error messages

## Next Steps

Once the POC works:
1. Try compiling your own Haxe projects
2. Experiment with optimization flags
3. Test with larger applications
4. Integrate into your build pipeline

Happy WASM development on Windows! 🎉
