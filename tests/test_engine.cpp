#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp> // For timing
#include <iostream>
#include <vector>
#include <chrono>
#include <filesystem>
#include <string>
#include <algorithm>
#include <optional>
#include <fstream>
#include <string>
#include <unordered_map>
#include <sstream>

#include "engine.h"
#include "precomputed_moves.h"
#include "magicmoves.h"
#include "zobrist_keys.h"

namespace
{
    /**
     * @brief Parses a perft output file into a map of {move_sequence: node_count}.
     * This is the C++ equivalent of the parsing function in the Python script.
     */
    std::unordered_map<std::string, std::uint64_t> parsePerftFile(const std::string& filepath)
    {
        std::unordered_map<std::string, std::uint64_t> move_counts;
        std::ifstream file(filepath);
        if (!file.is_open()) {
            // Return an empty map if the file can't be opened. The comparison will fail later.
            return move_counts;
        }

        std::string line;
        while (std::getline(file, line))
        {
            // Simple trim
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);

            if (line.empty() || line.find("Total:") != std::string::npos) {
                continue;
            }

            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string move_seq = line.substr(0, colon_pos);
                // Simple trim for the move sequence
                move_seq.erase(move_seq.find_last_not_of(" \t\n\r") + 1);

                std::string count_str = line.substr(colon_pos + 1);
                try {
                    move_counts[move_seq] = std::stoull(count_str);
                } catch (const std::exception&) {
                    // Handle potential conversion error if the format is weird
                }
            }
        }
        return move_counts;
    }
    std::string comparePerftFiles(const std::string& correctFilepath, const std::string& testFilepath)
    {
        if (!std::filesystem::exists(correctFilepath)) {
            return "FATAL ERROR: The 'correct' data file was not found at path: " + correctFilepath;
        }
        if (!std::filesystem::exists(testFilepath)) {
            return "FATAL ERROR: The test output file was not found. Did the engine generate it? Path: " + testFilepath;
        }

        auto correctData = parsePerftFile(correctFilepath);
        if (correctData.empty()) {
            return "ERROR: Could not read or parse the correct data file: " + correctFilepath;
        }

        std::ifstream testFile(testFilepath);
        if (!testFile.is_open()) {
            return "ERROR: Could not open the test output file: " + testFilepath;
        }

        std::string line;
        int lineNum = 0;
        size_t testMovesParsed = 0;
        while (std::getline(testFile, line))
        {
            lineNum++;
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);

            if (line.empty() || line.find("Total:") != std::string::npos) {
                continue;
            }

            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                testMovesParsed++;
                std::string move_seq = line.substr(0, colon_pos);
                move_seq.erase(move_seq.find_last_not_of(" \t\n\r") + 1);
                
                std::uint64_t count = 0;
                try {
                    count = std::stoull(line.substr(colon_pos + 1));
                } catch(const std::exception&) { /* ignore */ }

                auto it = correctData.find(move_seq);

                // Check 1: Extra/illegal move found in test file
                if (it == correctData.end()) {
                    std::stringstream ss;
                    ss << "Mismatch found: Engine generated an extra/illegal move.\n"
                       << "  File: " << testFilepath << "\n"
                       << "  Line " << lineNum << ": \"" << line << "\"";
                    return ss.str();
                }

                // Check 2: Node count mismatch
                if (it->second != count) {
                    std::stringstream ss;
                    ss << "Mismatch found: Node count is incorrect.\n"
                       << "  File:     " << testFilepath << "\n"
                       << "  Line " << lineNum << ":  \"" << line << "\"\n"
                       << "  Expected: \"" << move_seq << ": " << it->second << "\"";
                    return ss.str();
                }
            }
        }

        // Check 3: Missing moves
        if (testMovesParsed < correctData.size()) {
            return "Mismatch found: Engine failed to generate all legal moves (found "
                   + std::to_string(testMovesParsed) + ", expected " + std::to_string(correctData.size()) + ").";
        }

        // If all checks pass, return an empty string for success
        return "";
    }
    std::string sanitize_fen(std::string fen) {
        std::replace(fen.begin(), fen.end(), '/', '_');
        std::replace(fen.begin(), fen.end(), ' ', '-');
        return fen;
    }
}


struct TestInitializer {
    TestInitializer() {
        initmagicmoves();
        zobrist_keys::initializeZobristNumbers();
    }
};

static TestInitializer initialize_tests;

const int DEPTH = 4;
const std::string CORRECT_DATA_DIR = "../../tests/perft_data_correct"; // Relative to build dir
const std::string TEST_OUTPUT_DIR = "."; // Output to current (build) dir

// FENs must match the order of the tests
const std::vector<std::string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"
};

TEST_CASE("AB Perft Total - Position 1 (Initial)")
{
    THEngine engine;
    engine.setPosition(fens[0], {});
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos1";
    // This part might crash, but it will generate a file first.
    REQUIRE(engine.perftTest(DEPTH, false, testFile) == 197281ULL);
}
TEST_CASE("AB Perft Compare - Position 1 (Initial)")
{
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos1";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[0]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    // This test only reads files, so it will not segfault.
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for AB Position 1. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("AB Perft Total - Position 2")
{
    THEngine engine;
    engine.setPosition(fens[1], {});
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos2";
    REQUIRE(engine.perftTest(DEPTH, false, testFile) == 4085603ULL);
}
TEST_CASE("AB Perft Compare - Position 2")
{
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos2";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[1]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for AB Position 2. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("AB Perft Total - Position 3")
{
    THEngine engine;
    engine.setPosition(fens[2], {});
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos3";
    REQUIRE(engine.perftTest(DEPTH, false, testFile) == 43238ULL);
}
TEST_CASE("AB Perft Compare - Position 3")
{
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos3";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[2]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for AB Position 3. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("AB Perft Total - Position 4")
{
    THEngine engine;
    engine.setPosition(fens[3], {});
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos4";
    REQUIRE(engine.perftTest(DEPTH, false, testFile) == 422333ULL);
}
TEST_CASE("AB Perft Compare - Position 4")
{
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos4";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[3]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for AB Position 4. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("AB Perft Total - Position 5")
{
    THEngine engine;
    engine.setPosition(fens[4], {});
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos5";
    REQUIRE(engine.perftTest(DEPTH, false, testFile) == 2103487ULL);
}
TEST_CASE("AB Perft Compare - Position 5")
{
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos5";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[4]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for AB Position 5. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("AB Perft Total - Position 6")
{
    THEngine engine;
    engine.setPosition(fens[5], {});
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos6";
    REQUIRE(engine.perftTest(DEPTH, false, testFile) == 3894594ULL);
}
TEST_CASE("AB Perft Compare - Position 6")
{
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos6";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[5]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for AB Position 6. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("QS Perft Total - Position 1 (Initial)")
{
    THEngine engine;
    engine.setPosition(fens[0], {});
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos1";
    // This part might crash, but it will generate a file first.
    REQUIRE(engine.perftTest(DEPTH, true, testFile) == 197281ULL);
}
TEST_CASE("QS Perft Compare - Position 1 (Initial)")
{
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos1";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[0]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    // This test only reads files, so it will not segfault.
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for QS Position 1. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("QS Perft Total - Position 2")
{
    THEngine engine;
    engine.setPosition(fens[1], {});
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos2";
    REQUIRE(engine.perftTest(DEPTH, true, testFile) == 4085603ULL);
}
TEST_CASE("QS Perft Compare - Position 2")
{
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos2";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[1]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for QS Position 2. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("QS Perft Total - Position 3")
{
    THEngine engine;
    engine.setPosition(fens[2], {});
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos3";
    REQUIRE(engine.perftTest(DEPTH, true, testFile) == 43238ULL);
}
TEST_CASE("QS Perft Compare - Position 3")
{
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos3";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[2]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for QS Position 3. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("QS Perft Total - Position 4")
{
    THEngine engine;
    engine.setPosition(fens[3], {});
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos4";
    REQUIRE(engine.perftTest(DEPTH, true, testFile) == 422333ULL);
}
TEST_CASE("QS Perft Compare - Position 4")
{
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos4";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[3]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for QS Position 4. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("QS Perft Total - Position 5")
{
    THEngine engine;
    engine.setPosition(fens[4], {});
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos5";
    REQUIRE(engine.perftTest(DEPTH, true, testFile) == 2103487ULL);
}
TEST_CASE("QS Perft Compare - Position 5")
{
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos5";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[4]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for QS Position 5. Check the file: " << testFile);
    REQUIRE(error == "");
}

TEST_CASE("QS Perft Total - Position 6")
{
    THEngine engine;
    engine.setPosition(fens[5], {});
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos6";
    REQUIRE(engine.perftTest(DEPTH, true, testFile) == 3894594ULL);
}
TEST_CASE("QS Perft Compare - Position 6")
{
    std::string testFile = TEST_OUTPUT_DIR + "/qs_test_pos6";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[5]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for QS Position 6. Check the file: " << testFile);
    REQUIRE(error == "");
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