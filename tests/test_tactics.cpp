#include <catch2/catch_all.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <iomanip>
#include <fstream>
#include <ctime>

#include "engine.h"
#include "precomputed_moves.h"
#include "magicmoves.h"
#include "zobrist_keys.h"


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
void run_tactic_test(const std::string& name, const std::string& fen, const std::string& bestMove, int maxDepth) {
    #ifdef NDEBUG
        static std::ofstream results_file = [] {
            const std::string filename = "tactic_results.csv";
            std::ofstream file(filename, std::ios_base::app); // Open in append mode

            if (file.is_open()) {
                // If the file is empty, write the header.
                file.seekp(0, std::ios::end);
                if (file.tellp() == 0) {
                    file << "TacticName,FEN,Depth,MoveFound,TimeTakenSeconds,Timestamp\n";
                }
            }
            return file;
        }();
    #endif

    THEngine engine;
    engine.setPosition(fen, {});
    std::string moveFound;

    std::cout << "\n--- Testing Tactic: " << name << " ---" << std::endl;
    std::cout << "FEN: " << fen << std::endl;

    for (int depth = 2; depth <= maxDepth; ++depth) {
        // Clear the transposition table so each fixed-depth search is a clean,
        // reproducible measurement of true depth-N strength — not one polluted by the
        // shallower searches that ran before it (which, on a deep quiet-win-vs-draw
        // position like Tactic 3, can graft misleading shallow draw scores).
        engine.setTTSize();
        auto start_time = std::chrono::high_resolution_clock::now();
        moveFound = engine.searchFixedDepth(depth);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;

        std::cout << "  Depth " << depth
                  << ": Found '" << moveFound << "' in "
                  << std::fixed << std::setprecision(4) << elapsed.count() << " seconds."
                  << std::endl;

        #ifdef NDEBUG
            if (results_file.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto in_time_t = std::chrono::system_clock::to_time_t(now);
                std::stringstream timestamp_ss;
                timestamp_ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");

                results_file << name << ","
                             << "\"" << fen << "\","
                             << depth << ","
                             << moveFound << ","
                             << std::fixed << std::setprecision(4) << elapsed.count() << ","
                             << timestamp_ss.str()
                             << "\n";
            }
        #endif

        if (depth == maxDepth) {
            REQUIRE(moveFound == bestMove);
        }
    }
}

void run_time_limited_tactic_test(const std::string& name, const std::string& fen, const std::string& bestMove, int timeLimitMs) {
    THEngine engine;
    engine.setPosition(fen, {});
    std::string moveFound;

    std::cout << "\n--- Testing Tactic (Time-Limited): " << name << " ---" << std::endl;
    std::cout << "FEN: " << fen << std::endl;
    std::cout << "Time Limit: " << timeLimitMs << "ms" << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();
    moveFound = engine.searchWithTimeConstraint(timeLimitMs);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "  Found '" << moveFound << "' in "
              << std::fixed << std::setprecision(4) << elapsed.count() << " seconds."
              << std::endl;

    REQUIRE(moveFound == bestMove);
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
        // This is a deep quiet win (Bd4) against an exposed but materially-up
        // defender. Aggressive search reductions defer it by a ply or two at fixed
        // depth: null-move pruning (v0.3.10) needed depth 12, and late-move
        // reductions (v0.3.11) need depth 13. The engine reaches that depth far
        // faster than before, and the time-limited variant below confirms the find
        // under real time control.
        run_tactic_test("3", "1b1q4/8/P2p4/1N1Pp2p/5P1k/7P/1B1P3K/8 w - - 0 1", "b2d4", 13);
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
        run_tactic_test("6", "3k2rr/4b3/p3Qpq1/P2pn3/1p1Nb3/6B1/1PP1B2P/3R1RK1 b - - 0 25", "h8h2", 10);
    #endif
}

TEST_CASE("Tactic 7", "[tactics][timed]") {
    #ifndef NDEBUG
        run_tactic_test("7", "4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1", "b2b8", 5);
    #else
        run_tactic_test("7", "4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1", "b2b8", 10);
    #endif
}


TEST_CASE("Tactic 1 Time-Limited", "[tactics][time-limited]") {
    #ifdef NDEBUG
        run_time_limited_tactic_test("1", "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1", "a1a6", 30000);
    #endif
}

TEST_CASE("Tactic 2 Time-Limited", "[tactics][time-limited]") {
    #ifdef NDEBUG
        run_time_limited_tactic_test("2", "rR6/p7/KnPk4/P7/8/8/8/8 w - - 0 1", "c6c7", 30000);
    #endif
}

TEST_CASE("Tactic 3 Time-Limited", "[tactics][time-limited]") {
    #ifdef NDEBUG
        run_time_limited_tactic_test("3", "1b1q4/8/P2p4/1N1Pp2p/5P1k/7P/1B1P3K/8 w - - 0 1", "b2d4", 30000);
    #endif
}

TEST_CASE("Tactic 4 Time-Limited", "[tactics][time-limited]") {
    #ifdef NDEBUG
        run_time_limited_tactic_test("4", "2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19", "c6b6", 30000);
    #endif
}

TEST_CASE("Tactic 5 Time-Limited", "[tactics][time-limited]") {
    #ifdef NDEBUG
        run_time_limited_tactic_test("5", "rn2kb1r/1bq2pp1/pp3n1p/4p3/2PQ1B1P/2N3P1/PP2PPB1/2KR3R w kq - 0 12", "f4e5", 30000);
    #endif
}

TEST_CASE("Tactic 6 Time-Limited", "[tactics][time-limited]") {
    #ifdef NDEBUG
        run_time_limited_tactic_test("6", "3k2rr/4b3/p3Qpq1/P2pn3/1p1Nb3/6B1/1PP1B2P/3R1RK1 b - - 0 25", "h8h2", 30000);
    #endif
}

TEST_CASE("Tactic 7 Time-Limited", "[tactics][time-limited]") {
    #ifdef NDEBUG
        run_time_limited_tactic_test("7", "4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1", "b2b8", 30000);
    #endif
}

