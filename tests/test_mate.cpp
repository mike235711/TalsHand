#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <string>
#include <vector>

#include "engine.h"
#include "magicmoves.h"
#include "zobrist_keys.h"
#include "precomputed_moves.h"

// ============================================================================
// "Mate in X" puzzles.
//
// Each puzzle is a position with a forced mate for the side to move. Searching
// to a depth that covers the mate must:
//   * report a mate score (large magnitude: the engine scores mate as
//     ~30000 minus the distance, so a found forced win is >= 29000), and
//   * return the known mating first move.
// This validates both search correctness (it finds the forced win) and that
// mate detection / scoring works.
// ============================================================================

namespace
{
    struct MateTestInitializer
    {
        MateTestInitializer()
        {
            initmagicmoves();
            zobrist_keys::initializeZobristNumbers();
        }
    };
    static MateTestInitializer mate_test_initializer;

    // Engine scores a forced mate as roughly +-30000 (offset by the distance to
    // mate). A comfortable threshold for "this is a forced mate for the side to
    // move" is 29000.
    constexpr int16_t MATE_THRESHOLD = 29000;

    struct MatePuzzle
    {
        std::string name;
        std::string fen;
        std::string bestMove; // expected mating first move (UCI)
        int depth;            // search depth (>= plies-to-mate)
    };

    void run_mate_puzzle(const MatePuzzle &p)
    {
        THEngine engine;
        engine.setPosition(p.fen, {});

        auto [move, score] = engine.searchFixedDepthWithScore(static_cast<int8_t>(p.depth));

        std::cout << "[mate] " << p.name << " | fen=" << p.fen
                  << " | depth=" << p.depth << " | move=" << move
                  << " | score=" << score << std::endl;

        INFO("Puzzle: " << p.name << " | FEN: " << p.fen
                        << " | found move: " << move << " | score: " << score);

        // The engine must recognise a forced mate for the side to move.
        REQUIRE(score >= MATE_THRESHOLD);
        // ... and play the known mating move.
        REQUIRE(move == p.bestMove);
    }

    // Forced mates for the side to move. Move strings are filled in/confirmed
    // from the engine's own output (a position only passes if the engine reports
    // a genuine mate score, so wrong/non-mate entries cannot slip through).
    const std::vector<MatePuzzle> PUZZLES = {
        {"Back-rank mate in 1 (a)", "6k1/5ppp/8/8/8/8/8/R6K w - - 0 1", "a1a8", 3},
        {"Back-rank mate in 1 (b)", "7k/6pp/8/8/8/8/8/5R1K w - - 0 1", "f1f8", 3},
        {"Rook box mate in 1", "k7/8/1K6/8/8/8/8/7R w - - 0 1", "h1h8", 3},
        {"Back-rank mate in 1 (c)", "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1", "a1a8", 3},
        {"WAC.001 (mate)", "2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1", "g3g6", 7},
        {"Legall-style mate in 2", "r1b2k1r/ppp1bppp/8/1B1Q4/5q2/2P5/PPP2PPP/R3R1K1 w - - 1 0", "d5d8", 5},
    };
}

TEST_CASE("Mate puzzles are solved", "[mate]")
{
    for (const MatePuzzle &p : PUZZLES)
    {
        SECTION(p.name)
        {
            run_mate_puzzle(p);
        }
    }
}
