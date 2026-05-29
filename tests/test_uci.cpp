#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <sstream>
#include <string>

#include "engine.h"
#include "magicmoves.h"
#include "zobrist_keys.h"
#include "precomputed_moves.h"

// ============================================================================
// UCI protocol tests.
//
// THEngine::readUci() reads commands from std::cin and writes responses to
// std::cout, looping until "quit" or end-of-input. We drive it in-process by
// temporarily swapping the std::cin / std::cout stream buffers for string
// streams, feeding a command script and inspecting the produced output. This
// verifies the handshake and command handling a GUI relies on.
// ============================================================================

namespace
{
    struct UciTestInitializer
    {
        UciTestInitializer()
        {
            initmagicmoves();
            zobrist_keys::initializeZobristNumbers();
        }
    };
    static UciTestInitializer uci_test_initializer;

    // Run a UCI command script through a fresh engine and return everything it
    // wrote to std::cout. The script is consumed until EOF (readUci's loop ends
    // when the input stream is exhausted, exactly as it would on a closed pipe).
    std::string runUci(const std::string &commands)
    {
        THEngine engine; // constructor loads the NNUEU weights

        std::istringstream input(commands);
        std::ostringstream output;

        std::streambuf *oldCin = std::cin.rdbuf(input.rdbuf());
        std::streambuf *oldCout = std::cout.rdbuf(output.rdbuf());

        engine.readUci();

        std::cin.rdbuf(oldCin);
        std::cout.rdbuf(oldCout);

        return output.str();
    }

    bool contains(const std::string &haystack, const std::string &needle)
    {
        return haystack.find(needle) != std::string::npos;
    }
}

TEST_CASE("UCI handshake responds with id and uciok", "[uci]")
{
    const std::string out = runUci("uci\n");
    INFO("engine output:\n" << out);
    REQUIRE(contains(out, "id name"));
    REQUIRE(contains(out, "id author"));
    REQUIRE(contains(out, "uciok"));
}

TEST_CASE("UCI isready responds with readyok", "[uci]")
{
    const std::string out = runUci("isready\n");
    INFO("engine output:\n" << out);
    REQUIRE(contains(out, "readyok"));
}

TEST_CASE("UCI ignores unknown commands and keeps responding", "[uci]")
{
    // A junk line must not crash the loop; a following isready must still work.
    const std::string out = runUci("notacommand with args\nisready\n");
    INFO("engine output:\n" << out);
    REQUIRE(contains(out, "readyok"));
}

TEST_CASE("UCI accepts position startpos with moves", "[uci]")
{
    // After setting a position the engine must remain responsive (readyok).
    const std::string out = runUci(
        "position startpos moves e2e4 e7e5 g1f3\n"
        "isready\n");
    INFO("engine output:\n" << out);
    REQUIRE(contains(out, "readyok"));
}

TEST_CASE("UCI accepts position fen", "[uci]")
{
    const std::string out = runUci(
        "position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1\n"
        "isready\n");
    INFO("engine output:\n" << out);
    REQUIRE(contains(out, "readyok"));
}

TEST_CASE("UCI accepts setoption commands", "[uci]")
{
    const std::string out = runUci(
        "setoption name Hash value 32\n"
        "setoption name Threads value 1\n"
        "isready\n");
    INFO("engine output:\n" << out);
    REQUIRE(contains(out, "readyok"));
}

TEST_CASE("UCI quit stops the loop before later commands", "[uci]")
{
    // "isready" after "quit" must NOT be processed: the loop must break on quit.
    const std::string out = runUci(
        "uci\n"
        "quit\n"
        "isready\n");
    INFO("engine output:\n" << out);
    REQUIRE(contains(out, "uciok"));
    REQUIRE_FALSE(contains(out, "readyok"));
}

TEST_CASE("UCI full game round trip produces a bestmove", "[uci]")
{
    // A typical GUI exchange: handshake, set a position, then search under a
    // small time budget. The engine must answer with a "bestmove".
    const std::string out = runUci(
        "uci\n"
        "isready\n"
        "position startpos\n"
        "go wtime 300 btime 300\n");
    INFO("engine output:\n" << out);
    REQUIRE(contains(out, "uciok"));
    REQUIRE(contains(out, "readyok"));
    REQUIRE(contains(out, "bestmove "));

    // The token after "bestmove " must look like a UCI move (e.g. "e2e4").
    const std::size_t pos = out.find("bestmove ");
    std::string move = out.substr(pos + 9);
    move = move.substr(0, move.find_first_of(" \n\r"));
    INFO("parsed bestmove: '" << move << "'");
    REQUIRE(move.size() >= 4);
    REQUIRE(move.size() <= 5);
    REQUIRE((move[0] >= 'a' && move[0] <= 'h'));
    REQUIRE((move[1] >= '1' && move[1] <= '8'));
}
