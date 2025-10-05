#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp> // For timing
#include <iostream>
#include <vector>
#include <chrono>
#include <filesystem>
#include <string>
#include <algorithm>
#include <optional>
#include "engine.h"
#include "precomputed_moves.h"
#include "magicmoves.h"
#include "zobrist_keys.h"


struct TestInitializer {
    TestInitializer() {
        initmagicmoves();
        zobrist_keys::initializeZobristNumbers();
    }
};

static TestInitializer initialize_tests;

TEST_CASE("Perft Tests")
{
    THEngine engine;

    SECTION("Position 1")
    {
        engine.setPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {});
        REQUIRE(engine.perftTest(4, false, "ab_test_pos1") == 197281ULL);
    }
    SECTION("Position 2")
    {
        engine.setPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", {});
        REQUIRE(engine.perftTest(4, false, "ab_test_pos2") == 4085603ULL);
    }
    SECTION("Position 3")
    {
        engine.setPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {});
        REQUIRE(engine.perftTest(4, false, "ab_test_pos3") == 43238ULL);
    }
    SECTION("Position 4")
    {
        engine.setPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {});
        REQUIRE(engine.perftTest(4, false, "ab_test_pos4") == 422333ULL);
    }
    SECTION("Position 5")
    {
        engine.setPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {});
        REQUIRE(engine.perftTest(4, false, "ab_test_pos5") == 2103487ULL);
    }
    SECTION("Position 6")
    {
        engine.setPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {});
        REQUIRE(engine.perftTest(4, false, "ab_test_pos6") == 3894594ULL);
    }
}

TEST_CASE("Quiescence Search Perft Tests")
{
    THEngine engine;

    SECTION("Position 1")
    {
        engine.setPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {});
        REQUIRE(engine.perftTest(4, true, "qs_test_pos1") == 197281ULL);
    }
    SECTION("Position 2")
    {
        engine.setPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", {});
        REQUIRE(engine.perftTest(4, true, "qs_test_pos2") == 4085603ULL);
    }
    SECTION("Position 3")
    {
        engine.setPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {});
        REQUIRE(engine.perftTest(4, true, "qs_test_pos3") == 43238ULL);
    }
    SECTION("Position 4")
    {
        engine.setPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {});
        REQUIRE(engine.perftTest(4, true, "qs_test_pos4") == 422333ULL);
    }
    SECTION("Position 5")
    {
        engine.setPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {});
        REQUIRE(engine.perftTest(4, true, "qs_test_pos5") == 2103487ULL);
    }
    SECTION("Position 6")
    {
        engine.setPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {});
        REQUIRE(engine.perftTest(4, true, "qs_test_pos6") == 3894594ULL);
    }
}

struct Tactic
{
    std::string fen;
    std::string bestMove;
};

TEST_CASE("Tactics Tests (Depth-Limited)")
{
    THEngine engine;

    std::vector<Tactic> tactics = {
        {"kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1", "a1a6"},
        {"rR6/p7/KnPk4/P7/8/8/8/8 w - - 0 1", "c6c7"},
        {"1b1q4/8/P2p4/1N1Pp2p/5P1k/7P/1B1P3K/8 w - - 0 1", "b2b4"},
        {"2r2rk1/1b3ppp/p1qpp3/1P6/1Pn1P2b/2NB1P1P/1BP1R1P1/R2Q2K1 b - - 0 19", "c6b6"},
        {"rn2kb1r/1bq2pp1/pp3n1p/4p3/2PQ1B1P/2N3P1/PP2PPB1/2KR3R w kq - 0 12", "f4e5"},
        {"3k2rr/4b3/p3Qpq1/P2pn3/1p1Nb3/6B1/1PP1B2P/3R1RK1 b - - 0 25", "h8h2"},
        {"4k3/Q6n/8/8/8/8/PR5P/4K1NR w K - 0 1", "b2b8"}};

    auto total_start_time = std::chrono::high_resolution_clock::now();

    // Loop through depths
    for (int depth = 2; depth <= 5; ++depth)
    {
        std::cout << "--- Testing at Depth " << depth << " ---" << std::endl;

        for (const auto &tactic : tactics)
        {
            std::cout << "  FEN: " << tactic.fen << std::endl;
            engine.setPosition(tactic.fen, {});
            std::string moveFound = engine.searchFixedDepth(depth);

            std::cout << " | Move Found: " << moveFound << std::endl;

            if (depth == 5)
            {
                REQUIRE(moveFound == tactic.bestMove);
            }
        }
    }

    auto total_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = total_end_time - total_start_time;

    // Print the total time taken for all tactics and depths
    std::cout << "\n==========================================" << std::endl;
    std::cout << "Total Tactics Test Time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "==========================================" << std::endl;
}