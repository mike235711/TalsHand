include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
include(CTest)

# --- Standard Test Executable (Level 1 Debug) ---
set(STANDARD_TEST_SOURCES
    "test_bit_utils.cpp"
    "test_perft.cpp"
)
add_executable(unit_tests ${STANDARD_TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE
    Catch2::Catch2WithMain
    Talshand::lib
)
catch_discover_tests(unit_tests PROPERTIES TIMEOUT 10000)


# --- Tactics Test Executable (Conditional Level 2 Debug) ---
add_executable(tactics_tests test_tactics.cpp)
target_link_libraries(tactics_tests PRIVATE
    Catch2::Catch2WithMain
    Talshand::lib
)

# This is the key change! Apply the definition directly to the test executable.
# It's now controlled by the CMake option you defined.
if(ENABLE_VERBOSE_DEBUG)
    message(STATUS "Verbose debugging enabled for tactics_tests target.")
    target_compile_definitions(tactics_tests PRIVATE VERBOSE_DEBUG)
endif()

catch_discover_tests(tactics_tests PROPERTIES TIMEOUT 10000)