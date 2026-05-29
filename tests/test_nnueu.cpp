#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "bitposition.h"
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
