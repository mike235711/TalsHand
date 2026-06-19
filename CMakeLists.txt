cmake_minimum_required(VERSION 3.28)      # need 3.24+ for C++23 on AppleClang
project(TalsHand VERSION 0.1 LANGUAGES CXX)

# one source of truth for the language level
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TALSHAND_BUILD_TESTS "Build unit tests with Catch2" ON)
option(ENABLE_VERBOSE_DEBUG "Enable verbose (Level 2) debugging. Requires a Debug build." OFF)

# NNUEU accumulator width (8 or 32). Default 32 = w32_wdl0 baseline (v0.3.8).
# Build the legacy width-8 net with -DNNUEU_FIRST_OUT=8.
set(NNUEU_FIRST_OUT 32 CACHE STRING "NNUEU accumulator width: 8 or 32")
add_compile_definitions(NNUEU_FIRST_OUT=${NNUEU_FIRST_OUT})

add_subdirectory(src)

if(TALSHAND_BUILD_TESTS AND EXISTS ${CMAKE_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

# =========================================================================================
# Compiler and Linker Flags
# =========================================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build: applying -Ofast, -funroll-loops, architecture-specific march, and LTO")
    # Base optimization level
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -funroll-loops")
    # Architecture specific: Apple Silicon / generic ARM64 vs others
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        # Use armv8.2-a with dot product (benefits NNUE accumulation if using int8/int16 kernels)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.2-a+dotprod")
    endif()
    # Link-Time Optimization
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -flto")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build type detected: Applying -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
else()
    message(STATUS "Build type is not Release or Debug. Using default flags.")
endif()
