#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp> // For timing
#include <iostream>
#include <vector>
#include <chrono>
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
        REQUIRE(engine.perftTest(1, false) == 20);
        REQUIRE(engine.perftTest(2, false) == 400);
        REQUIRE(engine.perftTest(3, false) == 8902);
        REQUIRE(engine.perftTest(4, false) == 197281);
        REQUIRE(engine.perftTest(5, false) == 4865609);
    }
    SECTION("Position 2")
    {
        engine.setPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", {});
        REQUIRE(engine.perftTest(1, false) == 48);
        REQUIRE(engine.perftTest(2, false) == 2039);
        REQUIRE(engine.perftTest(3, false) == 97862);
        REQUIRE(engine.perftTest(4, false) == 4085603);
        REQUIRE(engine.perftTest(5, false) == 193690690);
    }
    SECTION("Position 3")
    {
        engine.setPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {});
        REQUIRE(engine.perftTest(1, false) == 14);
        REQUIRE(engine.perftTest(2, false) == 191);
        REQUIRE(engine.perftTest(3, false) == 2812);
        REQUIRE(engine.perftTest(4, false) == 43238);
        REQUIRE(engine.perftTest(5, false) == 674624);
    }
    SECTION("Position 4")
    {
        engine.setPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {});
        REQUIRE(engine.perftTest(1, false) == 6);
        REQUIRE(engine.perftTest(2, false) == 264);
        REQUIRE(engine.perftTest(3, false) == 9467);
        REQUIRE(engine.perftTest(4, false) == 422333);
        REQUIRE(engine.perftTest(5, false) == 15833292);
    }
    SECTION("Position 5")
    {
        engine.setPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {});
        REQUIRE(engine.perftTest(1, false) == 44);
        REQUIRE(engine.perftTest(2, false) == 1486);
        REQUIRE(engine.perftTest(3, false) == 62379);
        REQUIRE(engine.perftTest(4, false) == 2103487);
        REQUIRE(engine.perftTest(5, false) == 89941194);
    }
    SECTION("Position 6")
    {
        engine.setPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {});
        REQUIRE(engine.perftTest(1, false) == 46);
        REQUIRE(engine.perftTest(2, false) == 2079);
        REQUIRE(engine.perftTest(3, false) == 89890);
        REQUIRE(engine.perftTest(4, false) == 389594);
        REQUIRE(engine.perftTest(5, false) == 164075551);
    }
}

TEST_CASE("Quiescence Search Perft Tests")
{
    THEngine engine;

    SECTION("Position 1")
    {
        engine.setPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {});
        REQUIRE(engine.perftTest(1, true) == 20);
        REQUIRE(engine.perftTest(2, true) == 400);
        REQUIRE(engine.perftTest(3, true) == 8902);
        REQUIRE(engine.perftTest(4, true) == 197281);
        REQUIRE(engine.perftTest(5, true) == 4865609);
    }
    SECTION("Position 2")
    {
        engine.setPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", {});
        REQUIRE(engine.perftTest(1, true) == 48);
        REQUIRE(engine.perftTest(2, true) == 2039);
        REQUIRE(engine.perftTest(3, true) == 97862);
        REQUIRE(engine.perftTest(4, true) == 4085603);
        REQUIRE(engine.perftTest(5, true) == 193690690);
    }
    SECTION("Position 3")
    {
        engine.setPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {});
        REQUIRE(engine.perftTest(1, true) == 14);
        REQUIRE(engine.perftTest(2, true) == 191);
        REQUIRE(engine.perftTest(3, true) == 2812);
        REQUIRE(engine.perftTest(4, true) == 43238);
        REQUIRE(engine.perftTest(5, true) == 674624);
    }
    SECTION("Position 4")
    {
        engine.setPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {});
        REQUIRE(engine.perftTest(1, true) == 6);
        REQUIRE(engine.perftTest(2, true) == 264);
        REQUIRE(engine.perftTest(3, true) == 9467);
        REQUIRE(engine.perftTest(4, true) == 422333);
        REQUIRE(engine.perftTest(5, true) == 15833292);
    }
    SECTION("Position 5")
    {
        engine.setPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {});
        REQUIRE(engine.perftTest(1, true) == 44);
        REQUIRE(engine.perftTest(2, true) == 1486);
        REQUIRE(engine.perftTest(3, true) == 62379);
        REQUIRE(engine.perftTest(4, true) == 2103487);
        REQUIRE(engine.perftTest(5, true) == 89941194);
    }
    SECTION("Position 6")
    {
        engine.setPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {});
        REQUIRE(engine.perftTest(1, true) == 46);
        REQUIRE(engine.perftTest(2, true) == 2079);
        REQUIRE(engine.perftTest(3, true) == 89890);
        REQUIRE(engine.perftTest(4, true) == 389594);
        REQUIRE(engine.perftTest(5, true) == 164075551);
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
            engine.setPosition(tactic.fen, {});
            std::string moveFound = engine.searchFixedDepth(depth);

            std::cout << "  FEN: " << tactic.fen << " | Move Found: " << moveFound << std::endl;

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