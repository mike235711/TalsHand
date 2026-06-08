#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "engine.h"
#include "magicmoves.h"
#include "zobrist_keys.h"
#include "precomputed_moves.h"

// ============================================================================
// Threefold-repetition behaviour.
//
// isDraw() (src/bitposition.cpp) detects TRUE threefold (3rd occurrence) by
// walking the StateInfo chain and counting zobrist-key matches; the search
// scores a detected repetition as 0 (worker.cpp). So the behaviour to test is:
//   * a side that is WINNING should AVOID a repetition (it has > 0 lines),
//   * a side that is LOSING should STEER INTO a repetition (0 beats a loss).
//
// searchFixedDepthWithScore returns (bestMove, score) with score from the
// side-to-move's perspective (a forced mate is ~+30000, a draw ~0). The score
// is the diagnostic: a clearly winning side returning ~0 would be settling for a
// draw it shouldn't (eval/search bug); a clearly losing side returning a large
// negative would have failed to find the saving repetition.
//
// NOTE: the losing-side perpetual cases only pass because reversibleMovesMade
// counts king moves — isDraw's `< 8` guard would otherwise bail before the 3rd
// occurrence of a king-shuffle perpetual. They are the regression guard for that.
//
// Each scenario prints move+score and uses CHECK (non-fatal) so a run reports
// every result even if one regresses.
// ============================================================================

namespace
{
    struct RepTestInitializer
    {
        RepTestInitializer()
        {
            initmagicmoves();
            zobrist_keys::initializeZobristNumbers();
        }
    };
    static RepTestInitializer rep_test_initializer;

    std::pair<std::string, int16_t> run(const std::string &name,
                                        const std::string &fen,
                                        const std::vector<std::string> &moves,
                                        int depth)
    {
        THEngine engine;
        engine.setPosition(fen, moves);
        auto [move, score] = engine.searchFixedDepthWithScore(static_cast<int8_t>(depth));
        std::cout << "[rep] " << name << "\n    fen=" << fen
                  << "\n    history_plies=" << moves.size()
                  << " depth=" << depth
                  << " => move=" << move << " score=" << score << std::endl;
        return {move, score};
    }
}

// Scenario A — WINNING side must AVOID the repetition.
// White is up a whole queen (KQN vs KN). Both sides have shuffled knights back
// to the base position once (base has now occurred twice). A repetition would
// throw away the win, so White must NOT settle for 0.
TEST_CASE("Repetition: winning side avoids the draw", "[repetition]")
{
    auto [move, score] = run("winning_avoids_rep",
                             "4k3/8/8/3n4/3N4/8/8/4K1Q1 w - - 10 30",
                             {"d4f3", "d5f6", "f3d4", "f6d5"}, 4);
    (void)move;
    INFO("Up a full queen but score=" << score << " (≈0 ⇒ wrongly settling for the repetition draw)");
    CHECK(score > 300); // clearly winning, must not accept the 0-score repetition
}

// Scenario B — LOSING side should FORCE the repetition (perpetual check).
// White is down two rooks for a queen but can perpetually check the black king
// (Qd8+/Qd3+, king oscillates g8<->h7). The history has already repeated the
// perpetual twice; the saving line draws by threefold (≈0) while every other
// move loses. Detecting it needs king moves to count toward reversibleMovesMade
// (the cycle is queen-check + king-move), so the 3rd occurrence at ply 8 reaches
// the `< 8` guard.
TEST_CASE("Repetition: losing side forces the perpetual draw", "[repetition]")
{
    auto [move, score] = run("losing_forces_perpetual",
                             "rr4k1/5pp1/7p/8/8/8/8/3Q3K w - - 20 60",
                             {"d1d8", "g8h7", "d8d3", "h7g8",
                              "d3d8", "g8h7", "d8d3", "h7g8"}, 6);
    (void)move;
    INFO("Down ~a rook but a perpetual draws; score=" << score
         << " (large negative ⇒ did NOT find/claim the saving repetition)");
    CHECK(std::abs(score) < 200); // should recognise the draw, not a loss
}

// Extra coverage — the same perpetual carried over more cycles; also a forced
// draw. (Before the king-move reversibleMovesMade fix only this longer version
// was detected, because the queen moves alone eventually reached the >=8 guard;
// the 8-ply case above now passes too.)
TEST_CASE("Repetition: long perpetual is also forced to a draw", "[repetition]")
{
    auto [move, score] = run("long_perpetual",
                             "rr4k1/5pp1/7p/8/8/8/8/3Q3K w - - 0 1",
                             {"d1d8", "g8h7", "d8d3", "h7g8",
                              "d3d8", "g8h7", "d8d3", "h7g8",
                              "d3d8", "g8h7", "d8d3", "h7g8",
                              "d3d8", "g8h7", "d8d3", "h7g8"}, 6);
    (void)move;
    INFO("score=" << score << " (≈0 ⇒ detection works once counter>=8, so the 8-ply"
         " failure is the king-move/reversibleMovesMade bug)");
    CHECK(std::abs(score) < 200);
}

// Detection probe — a NON-king repetition (rook shuffle) in an unequal
// position. White is up a queen; if White were FORCED to keep shuffling it
// would be a draw. Here we just observe that the engine still finds the win
// (sanity that the rook-shuffle history doesn't confuse it), and the printed
// score lets us see whether 0-scored repetition lines appear in the tree.
TEST_CASE("Repetition: non-king shuffle history, winning side", "[repetition]")
{
    auto [move, score] = run("nonking_shuffle_winning",
                             "r3k3/8/8/8/8/8/8/4K1QR w - - 10 30",
                             {"h1h4", "a8a5", "h4h1", "a5a8"}, 4);
    (void)move;
    INFO("score=" << score);
    CHECK(score > 300);
}
