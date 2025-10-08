#include <catch2/catch_all.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <iomanip>

#include "engine.h"
#include "precomputed_moves.h"
#include "magicmoves.h"
#include "zobrist_keys.h"

// Define a default depth in case the file is compiled without the CMake flag.
#ifndef MAX_TACTICS_DEPTH
  #define MAX_TACTICS_DEPTH 5
#endif

// This struct ensures initialization code runs once before any tests start.
struct TestInitializer {
    TestInitializer() {
        initmagicmoves();
        zobrist_keys::initializeZobristNumbers();
    }
};
static TestInitializer initialize_tactics_tests;

// A helper function to run and time a single tactic test to avoid code repetition.
// While not strictly necessary, it keeps each TEST_CASE block cleaner.
void run_tactic_test(const std::string& name, const std::string& fen, const std::string& bestMove, int maxDepth = MAX_TACTICS_DEPTH) {
    THEngine engine;
    engine.setPosition(fen, {});
    std::string moveFound;

    std::cout << "\n--- Testing Tactic: " << name << " ---" << std::endl;
    std::cout << "FEN: " << fen << std::endl;

    for (int depth = 2; depth <= maxDepth; ++depth) {
        auto start_time = std::chrono::high_resolution_clock::now();
        moveFound = engine.searchFixedDepth(depth);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;

        std::cout << "  Depth " << depth
                  << ": Found '" << moveFound << "' in "
                  << std::fixed << std::setprecision(4) << elapsed.count() << " seconds."
                  << std::endl;

        if (depth == maxDepth) {
            REQUIRE(moveFound == bestMove);
        }
    }
}

TEST_CASE("Tactic 1", "[tactics][timed]") {
    #ifndef NDEBUG
        run_tactic_test("1", "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1", "a1a6", 5);
    #else
        run_tactic_test("1", "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1", "a1a6", 12);
    #endif
}

TEST_CASE("Tactic 2", "[tactics][timed]") {
    #ifndef NDEBUG
        run_tactic_test("2", "rR6/p7/KnPk4/P7/8/8/8/8 w - - 0 1", "c6c7", 5);
    #else
        run_tactic_test("2", "rR6/p7/KnPk4/P7/8/8/8/8 w - - 0 1", "c6c7", 14);
    #endif
}

TEST_CASE("Tactic 3", "[tactics][timed]") {
    #ifndef NDEBUG
        run_tactic_test("3", "1b1q4/8/P2p4/1N1Pp2p/5P1k/7P/1B1P3K/8 w - - 0 1", "b2d4", 5);
    #else
        run_tactic_test("3", "1b1q4/8/P2p4/1N1Pp2p/5P1k/7P/1B1P3K/8 w - - 0 1", "b2d4", 11);
    #endif
}

TEST_CASE("Tactic 4", "[tactics][timed]") {
    #ifndef NDEBUG
        run_tactic_test("4", "2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19", "c6b6", 5);
    #else
        run_tactic_test("4", "2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19", "c6b6", 10);
    #endif
}

TEST_CASE("Tactic 5", "[tactics][timed]") {
    #ifndef NDEBUG
        run_tactic_test("5", "rn2kb1r/1bq2pp1/pp3n1p/4p3/2PQ1B1P/2N3P1/PP2PPB1/2KR3R w kq - 0 12", "f4e5", 5);
    #else
        run_tactic_test("5", "rn2kb1r/1bq2pp1/pp3n1p/4p3/2PQ1B1P/2N3P1/PP2PPB1/2KR3R w kq - 0 12", "f4e5", 10);
    #endif
}

TEST_CASE("Tactic 6", "[tactics][timed]") {
    #ifndef NDEBUG
        run_tactic_test("6", "3k2rr/4b3/p3Qpq1/P2pn3/1p1Nb3/6B1/1PP1B2P/3R1RK1 b - - 0 25", "h8h2", 5);
    #else
        run_tactic_test("6", "3k2rr/4b3/p3Qpq1/P2pn3/1p1Nb3/6B1/1PP1B2P/3R1RK1 b - - 0 25", "h8h2", 9);
    #endif
}

TEST_CASE("Tactic 7", "[tactics][timed]") {
    #ifndef NDEBUG
        run_tactic_test("7", "4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1", "b2b8", 5);
    #else
        run_tactic_test("7", "4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1", "b2b8", 10);
    #endif
}