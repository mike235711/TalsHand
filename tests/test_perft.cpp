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
#include <cstdint>

#include "engine.h"
#include "precomputed_moves.h"
#include "magicmoves.h"
#include "zobrist_keys.h"

namespace
{
    // This helper function is correct and remains unchanged.
    std::unordered_map<std::string, std::uint64_t> getSubPerft(const std::string& filepath, const std::string& move_prefix)
    {
        std::unordered_map<std::string, std::uint64_t> move_counts;
        std::ifstream file(filepath);
        if (!file.is_open()) return move_counts;

        std::string prefix_with_space = move_prefix + " ";
        std::string line;

        while (std::getline(file, line))
        {
            // We are looking for lines that are children of move_prefix.
            // e.g., if move_prefix is "e2e4", we want "e2e4 e7e5: ..."
            // but we do NOT want "e2e4: ..." which is the total for the parent.
            if (line.rfind(prefix_with_space, 0) == 0) {
                size_t colon_pos = line.find(':');
                if (colon_pos == std::string::npos) continue;
                
                std::string full_move_seq = line.substr(0, colon_pos);
                size_t sub_move_start = prefix_with_space.length();
                
                // This logic finds the next token, e.g., "e7e5" in "e2e4 e7e5".
                // It correctly handles the end of the string if there are no more spaces.
                size_t sub_move_end = full_move_seq.find(' ', sub_move_start);
                std::string sub_move = full_move_seq.substr(sub_move_start, sub_move_end - sub_move_start);

                try {
                    std::string count_str = line.substr(colon_pos + 1);
                    move_counts[sub_move] = std::stoull(count_str);
                } catch (const std::exception&) {}
            }
        }
        return move_counts;
    }

    // Use the original, robust parser that gets all lines.
    std::unordered_map<std::string, std::uint64_t> parsePerftFile(const std::string& filepath)
    {
        std::unordered_map<std::string, std::uint64_t> move_counts;
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return move_counts;
        }

        std::string line;
        while (std::getline(file, line))
        {
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);

            if (line.empty() || line.find("Total:") != std::string::npos) {
                continue;
            }

            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string move_seq = line.substr(0, colon_pos);
                move_seq.erase(move_seq.find_last_not_of(" \t\n\r") + 1);
                try {
                    move_counts[move_seq] = std::stoull(line.substr(colon_pos + 1));
                } catch (const std::exception&) {}
            }
        }
        return move_counts;
    }

    std::string comparePerftFiles(const std::string& correctFilepath, const std::string& testFilepath)
    {
        if (!std::filesystem::exists(correctFilepath)) {
            return "FATAL ERROR: The 'correct' data file was not found: " + correctFilepath;
        }
        if (!std::filesystem::exists(testFilepath)) {
            return "FATAL ERROR: The test output file was not found: " + testFilepath;
        }

        // Parse the entire correct file into a map for quick lookups.
        auto correctData = parsePerftFile(correctFilepath);
        if (correctData.empty()) {
            return "ERROR: Could not read or parse correct file: " + correctFilepath;
        }

        auto unseenCorrectMoves = correctData;
        std::ifstream testFile(testFilepath);
        if (!testFile.is_open()) {
            return "ERROR: Could not open test file: " + testFilepath;
        }

        std::string line;
        int lineNum = 0;
        while (std::getline(testFile, line))
        {
            lineNum++;
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);

            if (line.empty() || line.find("Total:") != std::string::npos) continue;

            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string move_seq = line.substr(0, colon_pos);
                move_seq.erase(move_seq.find_last_not_of(" \t\n\r") + 1);

                uint64_t count = 0;
                try {
                    count = std::stoull(line.substr(colon_pos + 1));
                } catch(const std::exception&) {}

                auto it = correctData.find(move_seq);

                // Case A: The move sequence from the test file doesn't exist in the correct file.
                if (it == correctData.end()) {
                    std::stringstream ss;
                    ss << "Mismatch found: Engine generated an extra/illegal move sequence.\n"
                    << "  File: " << testFilepath << "\n"
                    << "  Line " << lineNum << ": \"" << line << "\"";
                    return ss.str();
                }

                unseenCorrectMoves.erase(move_seq); // Mark this line as seen.

                // Case B: The move sequence exists, but the total node count is wrong. 
                // This can only mean that within this move sequence, a move is missing.
                if (it->second != count) {
                    std::stringstream ss;

                    auto correctSubPerft = getSubPerft(correctFilepath, move_seq);
                    auto testSubPerft = getSubPerft(testFilepath, move_seq);

                    // Print if any of correctSubPerft or testSubPerft is empty (which is unexpected).
                    if (correctSubPerft.empty()) {
                        ss << "ERROR: Could not extract child moves from correct file for move sequence '" << move_seq << "'.\n";
                    }
                    if (testSubPerft.empty()) {     
                        ss << "ERROR: Could not extract child moves from test file for move sequence '" << move_seq << "'.\n";
                    }

                    // Iterate through the correct children to find what's missing
                    for (const auto& correct_pair : correctSubPerft) {
                        auto test_it = testSubPerft.find(correct_pair.first);
                        if (test_it == testSubPerft.end()) {
                            ss << "Mismatch found: Node count is incorrect.\n"
                            << "  File:     " << testFilepath << "\n"
                            << "  Line " << lineNum << ":  \"" << line << "\"\n"
                            << "  Expected: \"" << move_seq << ": " << it->second << "\"\n"
                            << "ANALYSIS of child moves for '" << move_seq << "':\n"
                            << "  - Missing child move: " << correct_pair.first << ")\n";
                        }
                    }
                    return ss.str();
                }
            }
        }
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
    REQUIRE(engine.perftTest(DEPTH, testFile) ==197281ULL);
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
    REQUIRE(engine.perftTest(DEPTH, testFile) ==4085603ULL);
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
    REQUIRE(engine.perftTest(DEPTH, testFile) ==43238ULL);
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
    REQUIRE(engine.perftTest(DEPTH, testFile) ==422333ULL);
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
    REQUIRE(engine.perftTest(DEPTH, testFile) ==2103487ULL);
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
    REQUIRE(engine.perftTest(DEPTH, testFile) ==3894594ULL);
}
TEST_CASE("AB Perft Compare - Position 6")
{
    std::string testFile = TEST_OUTPUT_DIR + "/ab_test_pos6";
    std::string correctFile = CORRECT_DATA_DIR + "/perft_" + sanitize_fen(fens[5]) + "_depth_" + std::to_string(DEPTH) + ".txt";
    
    std::string error = comparePerftFiles(correctFile, testFile);
    INFO("File comparison failed for AB Position 6. Check the file: " << testFile);
    REQUIRE(error == "");
}

// =============================================================================
// QS capture-selector consistency.
//
// Instead of reconstructing the full move tree from QS captures + QS non-captures
// and comparing to perft, we walk the AB-legal tree and assert that, at every
// node, the quiescence capture selectors emit exactly the AB-legal moves that are
// captures or queen promotions (BitPosition::isQSCaptureOrQueenProm). The node
// count is cross-checked against the known perft reference.
// =============================================================================
TEST_CASE("QS Capture Consistency - Position 1 (Initial)")
{
    THEngine engine;
    engine.setPosition(fens[0], {});
    auto result = engine.qsCaptureConsistencyTest(DEPTH);
    INFO("QS capture set disagreed with AB-legal captures at " << result.mismatches << " node(s) for Position 1.");
    REQUIRE(result.mismatches == 0ULL);
    REQUIRE(result.nodes == 197281ULL);
}
TEST_CASE("QS Capture Consistency - Position 2")
{
    THEngine engine;
    engine.setPosition(fens[1], {});
    auto result = engine.qsCaptureConsistencyTest(DEPTH);
    INFO("QS capture set disagreed with AB-legal captures at " << result.mismatches << " node(s) for Position 2.");
    REQUIRE(result.mismatches == 0ULL);
    REQUIRE(result.nodes == 4085603ULL);
}
TEST_CASE("QS Capture Consistency - Position 3")
{
    THEngine engine;
    engine.setPosition(fens[2], {});
    auto result = engine.qsCaptureConsistencyTest(DEPTH);
    INFO("QS capture set disagreed with AB-legal captures at " << result.mismatches << " node(s) for Position 3.");
    REQUIRE(result.mismatches == 0ULL);
    REQUIRE(result.nodes == 43238ULL);
}
TEST_CASE("QS Capture Consistency - Position 4")
{
    THEngine engine;
    engine.setPosition(fens[3], {});
    auto result = engine.qsCaptureConsistencyTest(DEPTH);
    INFO("QS capture set disagreed with AB-legal captures at " << result.mismatches << " node(s) for Position 4.");
    REQUIRE(result.mismatches == 0ULL);
    REQUIRE(result.nodes == 422333ULL);
}
TEST_CASE("QS Capture Consistency - Position 5")
{
    THEngine engine;
    engine.setPosition(fens[4], {});
    auto result = engine.qsCaptureConsistencyTest(DEPTH);
    INFO("QS capture set disagreed with AB-legal captures at " << result.mismatches << " node(s) for Position 5.");
    REQUIRE(result.mismatches == 0ULL);
    REQUIRE(result.nodes == 2103487ULL);
}
TEST_CASE("QS Capture Consistency - Position 6")
{
    THEngine engine;
    engine.setPosition(fens[5], {});
    auto result = engine.qsCaptureConsistencyTest(DEPTH);
    INFO("QS capture set disagreed with AB-legal captures at " << result.mismatches << " node(s) for Position 6.");
    REQUIRE(result.mismatches == 0ULL);
    REQUIRE(result.nodes == 3894594ULL);
}
