cmake_minimum_required(VERSION 3.28)      # need 3.24+ for C++23 on AppleClang
project(TalsHand VERSION 0.1 LANGUAGES CXX)

# one source of truth for the language level
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TALSHAND_BUILD_TESTS "Build unit tests with Catch2" ON)

add_subdirectory(src)

if(TALSHAND_BUILD_TESTS AND EXISTS ${CMAKE_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()
