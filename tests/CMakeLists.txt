# --- Setup Catch2 Testing Framework ---
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)

# --- Conditional Test Registration ---

# Scenario 1: Standard Debug Build
if(CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG" AND NOT ENABLE_VERBOSE_DEBUG)
    message(STATUS "Registering standard debug tests (perft_tests)")

    add_executable(perft_tests
        "test_bit_utils.cpp"
        "test_perft.cpp"
    )
    target_link_libraries(perft_tests PRIVATE
        Catch2::Catch2WithMain
        Talshand::lib
    )
    catch_discover_tests(perft_tests)
endif()

# Scenario 2: Debug Verbose Build (Tactics up to Depth 5)
if(CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG" AND ENABLE_VERBOSE_DEBUG)
    message(STATUS "Registering verbose debug tests (tactics_tests to depth 5)")

    add_executable(tactics_tests test_tactics.cpp)
    target_link_libraries(tactics_tests PRIVATE
        Catch2::Catch2WithMain
        Talshand::lib
    )
    # Define the max depth for this build type
    
    catch_discover_tests(tactics_tests)
endif()

# Scenario 3: Release Build (Tactics up to Depth 8)
if(CMAKE_BUILD_TYPE_UPPER STREQUAL "RELEASE")
    message(STATUS "Registering release-only tests (tactics_tests to depth 8)")

    # Use test_tactics.cpp for the release build as requested
    add_executable(release_tactics_tests test_tactics.cpp)
    target_link_libraries(release_tactics_tests PRIVATE
        Catch2::Catch2WithMain
        Talshand::lib
    )
    # Define the max depth for this build type
    
    catch_discover_tests(release_tactics_tests)

    # Performance perft tests (nodes/sec) enabled in Release build only
    message(STATUS "Registering perft performance tests (perft_perf_tests)")
    add_executable(perft_perf_tests
        test_perft_perf.cpp
    )
    target_link_libraries(perft_perf_tests PRIVATE
        Catch2::Catch2WithMain
        Talshand::lib
    )
    catch_discover_tests(perft_perf_tests)
endif()