#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "bitposition.h"
#include "bit_utils.h"
#include "network.h"
#include "accumulation.h"
#include "move_selectors.h"
#include "magicmoves.h"
#include "zobrist_keys.h"
#include "precomputed_moves.h"

// ============================================================================
// Tests for NNUEU loading and (incremental) accumulation.
//
// The NNUEU first/second layer output ("accumulator") is updated incrementally
// as moves are made/unmade. The central correctness property is that this
// incremental accumulator is always identical to the one obtained by computing
// it from scratch for the same position. These tests verify:
//   1. The weights load from disk and are non-trivial (not a silent zero-fill).
//   2. After an arbitrary line of legal moves, the incrementally-updated
//      accumulator and the network evaluation match a fresh recomputation.
//   3. Evaluation is invariant under a make/unmake round trip.
// ============================================================================

namespace
{
    struct NNUEUTestInitializer
    {
        NNUEUTestInitializer()
        {
            initmagicmoves();
            zobrist_keys::initializeZobristNumbers();
        }
    };
    static NNUEUTestInitializer nnueu_test_initializer;

    // Relative to the test working directory (build_*/tests), same convention as
    // the perft tests which read "../../tests/perft_data_correct".
    const std::string MODEL_DIR = "../../models/NNUEU_quantized_model_v4_param_350_epoch_10/";

    const std::vector<std::string> START_FENS = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",        // start position
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", // Kiwipete
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    };

    // Return the first legal move in the position (Move(0) if none). Sets the
    // blockers/check-bits the move makers rely on, mirroring perft_recursive.
    Move firstLegalMove(BitPosition &pos)
    {
        pos.setBlockersAndPinsInAB();
        pos.setCheckBits();
        if (!pos.getIsCheck())
        {
            ABMoveSelectorNotCheck sel(pos, Move(0));
            sel.init_all();
            return sel.select_legal();
        }
        pos.setCheckInfo();
        ABMoveSelectorCheck sel(pos, Move(0));
        sel.init();
        return sel.select_legal();
    }
}

TEST_CASE("NNUEU weights load from disk and are non-trivial", "[nnueu]")
{
    REQUIRE(std::filesystem::exists(MODEL_DIR));

    auto transformer = std::make_unique<NNUEU::Transformer>();
    REQUIRE(transformer->load(MODEL_DIR));

    NNUEU::Network network;
    REQUIRE(network.load(MODEL_DIR));

    // load_*_array silently returns zeros if a file is missing, so loading
    // returning true is not enough: confirm real data was read by checking that
    // the weights are not entirely zero.
    bool firstLayerNonZero = false;
    for (int i = 0; i < NNUEU::F_MAP && !firstLayerNonZero; ++i)
        for (int k = 0; k < NNUEU::FIRST_OUT; ++k)
            if (transformer->weights.firstW[i][k] != 0)
            {
                firstLayerNonZero = true;
                break;
            }
    REQUIRE(firstLayerNonZero);

    bool secondLayerNonZero = false;
    for (int s = 0; s < 64 && !secondLayerNonZero; ++s)
        for (int k = 0; k < NNUEU::SECOND_OUT; ++k)
            if (transformer->weights.second1[s][k] != 0)
            {
                secondLayerNonZero = true;
                break;
            }
    REQUIRE(secondLayerNonZero);

    // The inverted weights are a permutation of the originals, so they must also
    // contain non-zero entries.
    bool invertedNonZero = false;
    for (int i = 0; i < NNUEU::F_MAP && !invertedNonZero; ++i)
        for (int k = 0; k < NNUEU::FIRST_OUT; ++k)
            if (transformer->weights.firstWInv[i][k] != 0)
            {
                invertedNonZero = true;
                break;
            }
    REQUIRE(invertedNonZero);
}

TEST_CASE("NNUEU incremental accumulation matches a fresh recomputation", "[nnueu]")
{
    REQUIRE(std::filesystem::exists(MODEL_DIR));

    auto transformer = std::make_unique<NNUEU::Transformer>();
    REQUIRE(transformer->load(MODEL_DIR));
    NNUEU::Network network;
    REQUIRE(network.load(MODEL_DIR));

    constexpr int MAX_PLIES = 24;

    for (const std::string &fen : START_FENS)
    {
        StateInfo rootState;
        std::deque<StateInfo> states; // stable addresses across make/unmake

        BitPosition pos;
        pos.fromFen(fen, &rootState);

        NNUEU::AccumulatorStack acc;
        acc.reset(pos, *transformer);

        int pliesPlayed = 0;
        for (int ply = 0; ply < MAX_PLIES; ++ply)
        {
            Move move = firstLegalMove(pos);
            if (move.getData() == 0)
                break; // mate / stalemate: end of this line

            states.emplace_back();
            NNUEU::NNUEUChange change = pos.makeMove(move, states.back());
            acc.push(change);
            ++pliesPlayed;

            // Drive the incremental update of acc.top() for the side to move.
            network.evaluate(pos, acc, *transformer);

            // Ground truth: accumulator built from scratch for the same position.
            NNUEU::AccumulatorState fresh;
            fresh.initialize(pos, *transformer);

            // evaluate() only updates the perspective it actually uses for the
            // forward pass (the `not getTurn()` accumulator, matching the engine's
            // own debug verifyTopAgainstFresh check); the other perspective is left
            // lazily uncomputed, so we only compare the computed one.
            const int perspective = not pos.getTurn();
            INFO("FEN " << fen << " | ply " << ply << " | move " << move.toString());
            for (int i = 0; i < NNUEU::FIRST_OUT; ++i)
                REQUIRE(acc.top().inputTurn[perspective][i] == fresh.inputTurn[perspective][i]);
        }

        INFO("FEN " << fen);
        REQUIRE(pliesPlayed >= 5); // sanity: we actually walked a non-trivial line
    }
}

TEST_CASE("NNUEU evaluation is invariant under make/unmake", "[nnueu]")
{
    REQUIRE(std::filesystem::exists(MODEL_DIR));

    auto transformer = std::make_unique<NNUEU::Transformer>();
    REQUIRE(transformer->load(MODEL_DIR));
    NNUEU::Network network;
    REQUIRE(network.load(MODEL_DIR));

    for (const std::string &fen : START_FENS)
    {
        StateInfo rootState;
        BitPosition pos;
        pos.fromFen(fen, &rootState);

        NNUEU::AccumulatorStack acc;
        acc.reset(pos, *transformer);

        // The engine only ever evaluates inside the search tree (after at least
        // one move), never at the bare root, so play one setup move first.
        Move setup = firstLegalMove(pos);
        REQUIRE(setup.getData() != 0);
        StateInfo setupState;
        acc.push(pos.makeMove(setup, setupState));

        const int16_t evalBefore = network.evaluate(pos, acc, *transformer);

        // Make and then unmake a second move; the evaluation of the original
        // node must be exactly restored.
        Move move = firstLegalMove(pos);
        REQUIRE(move.getData() != 0);

        StateInfo st;
        acc.push(pos.makeMove(move, st));
        network.evaluate(pos, acc, *transformer); // exercise the pushed state

        pos.unmakeMove(move);
        acc.pop();

        const int16_t evalAfter = network.evaluate(pos, acc, *transformer);

        INFO("FEN " << fen << " | setup " << setup.toString() << " | move " << move.toString());
        REQUIRE(evalAfter == evalBefore);
    }
}

// ============================================================================
// King-in-the-input incrementality (NNUEU_F_MAP 704/768) -- and the change record's capacity.
//
// The whole point of putting a king in the feature transformer is that a king move stays an
// ORDINARY add/remove: one feature leaves, one arrives, on the same incremental path every
// other piece already uses. Nothing must ever recompute. The failure mode is silent -- the
// accumulator drifts away from the true position and the engine just evaluates a different
// board -- so the property has to be asserted, not reasoned about.
//
// CASTLING is the case that motivated widening NNUEUChange: it moves the king AND the rook, two
// features that each need their own (add, remove) pair. With a single-pair record the two
// writes overwrote each other. The lines below therefore castle in every direction, and also
// move and capture with the king, promote, and take en passant (the promotion path REPLACES
// pair 0 while castling APPENDS to it, and getting those two the same way breaks one of them).
//
// The test runs at every F_MAP: at 640 a king move records no change at all, which is just as
// much a property worth pinning down.
namespace
{
    // Weights are SYNTHESISED, not loaded from a net directory, on purpose:
    //   * the shipped nets do not all match the build's compile-time widths, and a narrow CSV
    //     read into a wide accumulator leaves the appended king planes ZERO -- which would make
    //     this test pass by checking that nothing happens;
    //   * arbitrary weights cannot cancel a mis-applied update the way one trained net might.
    void fillSyntheticWeights(NNUEU::Transformer &t)
    {
        // Deterministic and specific to (plane, square, lane), so a change applied to the wrong
        // feature index cannot land on an equal value by accident. Magnitudes stay small enough
        // that ~32 active features plus the bias cannot overflow int16.
        auto h = [](int plane, int sq, int k) {
            return static_cast<int16_t>(((plane * 7919 + sq * 104729 + k * 1301) % 251) - 125);
        };
        for (int p = 0; p < NNUEU::N_PLANES; ++p)
            for (int s = 0; s < 64; ++s)
                for (int k = 0; k < NNUEU::FIRST_OUT; ++k)
                {
                    const int16_t v = h(p, s, k);
                    t.weights.firstW[p * 64 + s][k] = v;
                    // firstWInv is the opposite perspective's table, built exactly the way
                    // load_inverted_int16_2D_array1 builds it: mirror the PLANE (which is what
                    // swaps the two king planes at F_MAP 768) and flip the square.
                    t.weights.firstWInv[NNUEU::mirrorPlane(p) * 64 + invertIndex(s)][k] = v;
                }
        for (int k = 0; k < NNUEU::FIRST_OUT; ++k)
            t.weights.firstBias[k] = static_cast<int16_t>((k * 37) % 61 - 30);
    }

    // Same enumeration the UCI layer uses (engine.cpp findMoveFromString): the AB selectors emit
    // the full legal move set, castling and en-passant included.
    Move findMoveByString(BitPosition &pos, const std::string &uci)
    {
        pos.setBlockersAndPinsInAB();
        pos.setCheckBits();
        Move move;
        if (pos.getIsCheck())
        {
            pos.setCheckInfo();
            ABMoveSelectorCheck sel(pos, Move(0));
            sel.init();
            while ((move = sel.select_legal()) != Move(0))
                if (move.toString() == uci)
                    return move;
            return Move(0);
        }
        ABMoveSelectorNotCheck sel(pos, Move(0));
        sel.init_all();
        while ((move = sel.select_legal()) != Move(0))
            if (move.toString() == uci)
                return move;
        return Move(0);
    }

    struct IncrementalLine
    {
        const char *what;
        const char *fen;
        std::vector<std::string> moves;
        // makeMove and makeCapture record the change SEPARATELY (quiescence uses the second),
        // so both need covering: a king branch fixed in one and missed in the other is invisible
        // outside qsearch. Lines with this set apply their moves through makeCapture, which
        // accepts only captures and queen promotions.
        bool viaCapture = false;
    };
}

TEST_CASE("NNUEU king moves stay incremental and castling records both pairs", "[nnueu]")
{
    auto transformer = std::make_unique<NNUEU::Transformer>();
    fillSyntheticWeights(*transformer);

    const std::vector<IncrementalLine> lines = {
        {"white O-O then black O-O-O",
         "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1", {"e1g1", "e8c8"}},
        {"white O-O-O then black O-O",
         "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1", {"e1c1", "e8g8"}},
        {"plain king walks, both colours",
         "8/8/8/3k4/8/3K4/8/8 w - - 0 1", {"d3c3", "d5c5", "c3b3", "c5b5", "b3a3", "b5a5"}},
        {"kings on and around opposite corners (a1/h8 exercise the extreme feature indices)",
         "7k/8/8/8/8/8/8/K7 w - - 0 1", {"a1a2", "h8h7", "a2b2", "h7g7", "b2b1", "g7g8"}},
        {"king captures",
         "8/8/8/8/3p4/3K4/8/7k w - - 0 1", {"d3d4", "h1g1", "d4d5", "g1f1"}},
        {"rook capture kills castling rights, king still moves",
         "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", {"a1a8", "e8f7", "a8a7", "f7f6"}},
        {"promotion (pair 0 is REPLACED, not appended) then the new queen moves",
         "1n6/P7/8/8/8/8/7k/K7 w - - 0 1", {"a7a8q", "h2h3", "a8b8", "h3h4"}},
        // Reached by a real double push rather than an ep-square FEN field, so the passant right
        // is set by makeMove itself -- the way the search always meets it.
        {"en passant (the captured pawn is not on the destination square)",
         "4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1", {"d7d5", "e5d6", "e8d8", "e1e2"}},
        {"king capture through the QUIESCENCE path", "8/8/8/8/3p4/3K4/8/7k w - - 0 1",
         {"d3d4"}, true},
        {"capture-promotion through the QUIESCENCE path", "1n6/P7/8/8/8/8/7k/K7 w - - 0 1",
         {"a7b8q"}, true},
    };

    for (const IncrementalLine &line : lines)
    {
        StateInfo rootState;
        std::deque<StateInfo> states; // stable addresses across the whole line

        BitPosition pos;
        pos.fromFen(line.fen, &rootState);

        NNUEU::AccumulatorStack acc;
        acc.reset(pos, *transformer);

        for (const std::string &uci : line.moves)
        {
            Move move = findMoveByString(pos, uci);
            INFO(line.what << " | FEN " << line.fen << " | move " << uci);
            REQUIRE(move.getData() != 0);

            states.emplace_back();
            acc.push(line.viaCapture ? pos.makeCapture(move, states.back())
                                     : pos.makeMove(move, states.back()));

            // Catch BOTH perspectives up incrementally. forward_update_incremental(begin, turn)
            // updates the `not turn` accumulator, so the two calls cover both -- and neither of
            // them may fall back to a recompute; there is no code path that could.
            for (bool turn : {false, true})
                acc.forward_update_incremental(acc.findLastComputedNode(turn), turn, *transformer);

            // Ground truth: the same position built from scratch, i.e. exactly what setting the
            // board from its FEN would produce.
            NNUEU::AccumulatorState fresh;
            fresh.initialize(pos, *transformer);

            for (int perspective = 0; perspective < 2; ++perspective)
            {
                int firstBad = -1;
                for (int i = 0; i < NNUEU::FIRST_OUT && firstBad < 0; ++i)
                    if (acc.top().inputTurn[perspective][i] != fresh.inputTurn[perspective][i])
                        firstBad = i;
                INFO("perspective " << perspective << " first mismatching lane " << firstBad);
                REQUIRE(firstBad == -1);
            }
        }
    }
}
