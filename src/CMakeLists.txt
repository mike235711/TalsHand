# Source files
set(SOURCES
    accumulation.cpp
    bitposition.cpp
    engine.cpp
    magicmoves.cpp
    move_selectors.cpp
    network.cpp
    nnueu_scalar.cpp
    precomputed_moves.cpp
    threadpool.cpp
    worker.cpp
    zobrist_keys.cpp
)

# Core library
add_library(talshand STATIC ${SOURCES})

# Add the compile definition for the verbose debug build
if(ENABLE_VERBOSE_DEBUG)
  message(STATUS "Verbose debugging enabled (VERBOSE_DEBUG defined)")
  target_compile_definitions(talshand PUBLIC VERBOSE_DEBUG)
endif()

# Modern way to set config-specific options
target_compile_options(talshand PRIVATE
  # -mcpu=apple-m2 was unconditional, so this target simply did not compile anywhere else --
  # gcc on x86 rejects the value outright. Kept as the default for the machine this engine is
  # developed and benchmarked on, with a portable Release for everyone else.
  $<$<CONFIG:Release>:-Ofast>
  $<$<AND:$<CONFIG:Release>,$<BOOL:${TALSHAND_APPLE_SILICON}>>:-mcpu=apple-m2>
  $<$<AND:$<CONFIG:Release>,$<NOT:$<BOOL:${TALSHAND_APPLE_SILICON}>>>:-march=native>
  $<$<CONFIG:Debug>:-g>
)

# Link-time optimisation for Release
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok)
if(ipo_ok)
    set_property(TARGET talshand PROPERTY INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Release>:YES>)
endif()

# Public interface include path
target_include_directories(talshand PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Convenient alias
add_library(Talshand::lib ALIAS talshand)

# Stand-alone executable
add_executable(talshand_exe main.cpp)
target_link_libraries(talshand_exe PRIVATE Talshand::lib)

# Post-build command to copy models
add_custom_command(TARGET talshand_exe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/models
            $<TARGET_FILE_DIR:talshand_exe>/../models)