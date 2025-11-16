# Emscripten/WASM Toolchain for HashLink
# Usage: emcmake cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Emscripten.cmake ..

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_PROCESSOR wasm)

# Emscripten paths (assumes emsdk is sourced)
if(NOT DEFINED ENV{EMSCRIPTEN})
    message(FATAL_ERROR "EMSCRIPTEN environment variable not set. Please run: source /path/to/emsdk/emsdk_env.sh")
endif()

set(EMSCRIPTEN $ENV{EMSCRIPTEN})
set(CMAKE_C_COMPILER "${EMSCRIPTEN}/emcc")
set(CMAKE_CXX_COMPILER "${EMSCRIPTEN}/em++")
set(CMAKE_AR "${EMSCRIPTEN}/emar" CACHE FILEPATH "Emscripten ar")
set(CMAKE_RANLIB "${EMSCRIPTEN}/emranlib" CACHE FILEPATH "Emscripten ranlib")

# Platform configuration
set(CMAKE_EXECUTABLE_SUFFIX ".js")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")

# HashLink-specific configuration for WASM
set(WITH_VM OFF CACHE BOOL "Disable VM for WASM (no JIT support)" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries for WASM" FORCE)

# Emscripten compiler flags
set(CMAKE_C_FLAGS_INIT "-DHL_EMSCRIPTEN -DHL_NO_THREADS")
set(CMAKE_CXX_FLAGS_INIT "-DHL_EMSCRIPTEN -DHL_NO_THREADS")

# Optimization flags
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
set(CMAKE_C_FLAGS_MINSIZEREL "-Oz -DNDEBUG -flto")

# Find root path for libraries
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

message(STATUS "Configured for Emscripten/WASM build")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Flags: ${CMAKE_C_FLAGS_INIT}")
message(STATUS "  WITH_VM: ${WITH_VM}")
message(STATUS "  Threading: DISABLED (HL_NO_THREADS)")
