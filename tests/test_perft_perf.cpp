#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <algorithm>
#include <cstdlib>

#include "engine.h"
#include "precomputed_moves.h"
#include "magicmoves.h"
#include "zobrist_keys.h"

namespace {
    struct TestInitializerPerf {
        TestInitializerPerf() {
            initmagicmoves();
            zobrist_keys::initializeZobristNumbers();
        }
    } initializerPerf;

    std::string sanitize_fen(std::string fen) {
        std::replace(fen.begin(), fen.end(), '/', '_');
        std::replace(fen.begin(), fen.end(), ' ', '-');
        return fen;
    }

    // Same canonical FENs used in correctness tests
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"
    };
}

// Depth for performance benchmarking (override with env TALSHAND_PERFT_DEPTH)
static int get_perf_depth() {
    const char* env = std::getenv("TALSHAND_PERFT_DEPTH");
    if (env && *env) {
        int d = std::atoi(env);
        if (d > 0) return d;
    }
    return 6;
}

// Helper to time a lambda and return elapsed seconds
template <typename F>
static double time_seconds(F&& fn) {
    auto start = std::chrono::steady_clock::now();
    fn();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

// We measure nodes per second for AB perft (quiescent=false). This does not assert on speed; it reports it.
TEST_CASE("Perft performance (AB) - nodes/sec per position") {
    const int PERF_DEPTH = get_perf_depth();
    for (size_t i = 0; i < fens.size(); ++i) {
        SECTION("FEN #" + std::to_string(i+1) + " - " + sanitize_fen(fens[i])) {
            THEngine engine;            
            engine.setPosition(fens[i], {});

            std::uint64_t nodes = 0;
            double secs = time_seconds([&](){
                nodes = engine.perftTest(PERF_DEPTH, std::nullopt);
            });

            // Protect against division by zero in pathological cases
            double nps = (secs > 0.0) ? (static_cast<double>(nodes) / secs) : 0.0;

            // Print to stdout (captured by ctest --verbose) and also via Catch INFO
          std::cout << "[Perft-AB] Position " << (i+1) << ": depth=" << PERF_DEPTH
                      << ", nodes=" << nodes << ", time=" << secs << "s"
                      << ", nps=" << static_cast<long long>(nps) << std::endl;
            INFO("[Perft-AB] Position " << (i+1) << ": depth=" << PERF_DEPTH
                 << ", nodes=" << nodes << ", time=" << secs << "s"
                 << ", nps=" << static_cast<long long>(nps));

            // Do not REQUIRE on nps; this is informational to track regressions manually or via dashboards.
            REQUIRE(nodes > 0ULL);
        }
    }
}

// Exercise the quiescence capture selectors over the whole tree (via the
// QS-capture-consistency walk) and report nodes/sec. Also asserts the QS capture
// set stays consistent with the AB-legal captures at this (typically deeper) depth.
TEST_CASE("QS capture-consistency walk - nodes/sec per position") {
    const int PERF_DEPTH = get_perf_depth();
    for (size_t i = 0; i < fens.size(); ++i) {
        SECTION("FEN #" + std::to_string(i+1) + " - " + sanitize_fen(fens[i])) {
            THEngine engine;
            engine.setPosition(fens[i], {});

            THEngine::QSConsistencyResult result{0, 0};
            double secs = time_seconds([&](){
                result = engine.qsCaptureConsistencyTest(PERF_DEPTH);
            });

            double nps = (secs > 0.0) ? (static_cast<double>(result.nodes) / secs) : 0.0;

          std::cout << "[QS-consistency] Position " << (i+1) << ": depth=" << PERF_DEPTH
                      << ", nodes=" << result.nodes << ", mismatches=" << result.mismatches
                      << ", time=" << secs << "s, nps=" << static_cast<long long>(nps) << std::endl;
            INFO("[QS-consistency] Position " << (i+1) << ": depth=" << PERF_DEPTH
                 << ", nodes=" << result.nodes << ", mismatches=" << result.mismatches
                 << ", time=" << secs << "s, nps=" << static_cast<long long>(nps));

            REQUIRE(result.mismatches == 0ULL);
            REQUIRE(result.nodes > 0ULL);
        }
    }
}
