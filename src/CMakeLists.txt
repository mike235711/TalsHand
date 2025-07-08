# Source files
set(SOURCES
    accumulation.cpp
    bitposition.cpp
    engine.cpp
    magicmoves.cpp
    move_selectors.cpp
    network.cpp
    nnueu.cpp
    nnueu_scalar.cpp
    precomputed_moves.cpp
    threadpool.cpp
    worker.cpp
    zobrist_keys.cpp
)

# Core library
add_library(talshand STATIC ${SOURCES})

# Replicate the flags you were using by hand
target_compile_options(talshand PRIVATE
  $<$<CONFIG:Release>:-Ofast>
  $<$<CONFIG:Release>:-mcpu=apple-m2>
)

# Add debug flags for Debug builds
target_compile_options(talshand PRIVATE
  $<$<CONFIG:Debug>:-g>
)

# Link-time optimisation should only be for Release
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok)
if(ipo_ok)
    set_property(TARGET talshand PROPERTY INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Release>:YES>)
endif()

# Link-time optimisation if the toolchain supports it
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok)
if(ipo_ok)
    set_target_properties(talshand PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE YES)
endif()

# Public interface include path (adjust if you have an include/ dir)
target_include_directories(talshand PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Convenient alias for downstream targets
add_library(Talshand::lib ALIAS talshand)

# Stand-alone executable
add_executable(talshand_exe main.cpp)
target_link_libraries(talshand_exe PRIVATE Talshand::lib)

add_custom_command(TARGET talshand_exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/models
            $<TARGET_FILE_DIR:talshand_exe>/../models)