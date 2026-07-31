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
set(NNUEU_FIRST_OUT 32 CACHE STRING "NNUEU accumulator width: 8, 32 or 512")
add_compile_definitions(NNUEU_FIRST_OUT=${NNUEU_FIRST_OUT})
# Head widths: 2nd-/3rd-layer output. Default 4/4 (w8/w32 nets); N512 uses 32/32.
set(NNUEU_SECOND_OUT 4 CACHE STRING "NNUEU 2nd-layer output width")
set(NNUEU_THIRD_OUT 4 CACHE STRING "NNUEU 3rd-layer output width")
add_compile_definitions(NNUEU_SECOND_OUT=${NNUEU_SECOND_OUT})
add_compile_definitions(NNUEU_THIRD_OUT=${NNUEU_THIRD_OUT})
# Dual activation (CReLU + SqrCReLU head) for the "_sq" cloud nets. 0 = single CReLU (default).
set(NNUEU_F_MAP 640 CACHE STRING "NNUEU input features: 640 king-free, 704 not-turn king, 768 both kings")
add_compile_definitions(NNUEU_F_MAP=${NNUEU_F_MAP})
set(NNUEU_SPLIT_FT 0 CACHE STRING "NNUEU split-ft head (1 = each projection reads its own accumulator half)")
add_compile_definitions(NNUEU_SPLIT_FT=${NNUEU_SPLIT_FT})
set(NNUEU_DUAL_ACT 0 CACHE STRING "NNUEU dual activation head (1 = CReLU+SqrCReLU)")
add_compile_definitions(NNUEU_DUAL_ACT=${NNUEU_DUAL_ACT})
# psqt_l3 (famC): the 8 phase-bucketed PSQT sums also enter the THIRD layer as 8 extra input
# lanes (third_layer_weights.csv rows widen to HEAD_CONCAT+8). 0 = classic head (default).
set(NNUEU_PSQT_L3 0 CACHE STRING "NNUEU psqt_l3 head (1 = 8 PSQT lanes appended to the 3rd-layer input)")
add_compile_definitions(NNUEU_PSQT_L3=${NNUEU_PSQT_L3})
# third_phase (famD): the 3rd layer's OUTPUT stack widens to 8 phase-bucketed weight sets, gathered
# by the SAME piece-count phase bucket psqt_sf already uses (third_layer_weights.csv rows widen to
# 8*THIRD_OUT_W, bucket-major). Composes with NNUEU_PSQT_L3 (which widens the INPUT stride
# instead): with both on, third_layer_weights.csv is 8*THIRD_OUT_W rows of THIRD_IN columns.
# 0 = classic single-stack third layer (default).
set(NNUEU_THIRD_PHASE 0 CACHE STRING "NNUEU third_phase head (1 = 8 phase-bucketed 3rd-layer weight stacks)")
add_compile_definitions(NNUEU_THIRD_PHASE=${NNUEU_THIRD_PHASE})
# famE: HEAD_SUM sums the two perspectives' RAW (pre-activation) 2nd-layer outputs into ONE vector
# and activates ONLY the sum, instead of activating each perspective separately and concatenating.
# 0 = classic concat head (default, every existing arm). Needs NNUEU_FIRST_OUT >= 128.
set(NNUEU_HEAD_SUM 0 CACHE STRING "NNUEU famE head (1 = sum-then-activate the two perspectives instead of concat)")
add_compile_definitions(NNUEU_HEAD_SUM=${NNUEU_HEAD_SUM})
# famE: HEAD_SKIP carves the LAST lane of each perspective's raw 2nd-layer output out of the sum
# above and folds it straight into the final output, bypassing the 3rd/final layers (a linear
# residual/factorizer connection, unclamped). Only meaningful with NNUEU_HEAD_SUM=1.
# 0 = no skip scalar, all lanes are summed (default).
set(NNUEU_HEAD_SKIP 0 CACHE STRING "NNUEU famE skip-scalar residual (1 = last 2nd-layer lane bypasses to the final output)")
add_compile_definitions(NNUEU_HEAD_SKIP=${NNUEU_HEAD_SKIP})

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
