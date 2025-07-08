include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.0
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Find all .cpp files in the current directory
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(unit_tests ${TEST_SOURCES})

target_link_libraries(unit_tests PRIVATE
    Catch2::Catch2WithMain
    Talshand::lib
)

include(CTest)
catch_discover_tests(unit_tests)


