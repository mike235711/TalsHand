cmake_minimum_required(VERSION 3.28)      # need 3.24+ for C++23 on AppleClang
project(TalsHand VERSION 0.1 LANGUAGES CXX)

# one source of truth for the language level
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TALSHAND_BUILD_TESTS "Build unit tests with Catch2" ON)
option(ENABLE_VERBOSE_DEBUG "Enable verbose (Level 2) debugging. Requires a Debug build." OFF)

add_subdirectory(src)

if(TALSHAND_BUILD_TESTS AND EXISTS ${CMAKE_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()

# =========================================================================================
# Compiler and Linker Flags
# =========================================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build type detected: Applying -Ofast and LTO")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -flto")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build type detected: Applying -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
else()
    message(STATUS "Build type is not Release or Debug. Using default flags.")
endif()
