#ifndef ZOBRIST_KEYS_H
#define ZOBRIST_KEYS_H

#include <array>
#include <vector>
#include <unordered_set>
#include <random>
#include <iostream>

namespace zobrist_keys
{
    extern std::array<uint64_t, 64> whitePawnZobristNumbers;
    extern std::array<uint64_t, 64> whiteKnightZobristNumbers;
    extern std::array<uint64_t, 64> whiteBishopZobristNumbers;
    extern std::array<uint64_t, 64> whiteRookZobristNumbers;
    extern std::array<uint64_t, 64> whiteQueenZobristNumbers;
    extern std::array<uint64_t, 64> whiteKingZobristNumbers;
    extern std::array<uint64_t, 64> blackPawnZobristNumbers;
    extern std::array<uint64_t, 64> blackKnightZobristNumbers;
    extern std::array<uint64_t, 64> blackBishopZobristNumbers;
    extern std::array<uint64_t, 64> blackRookZobristNumbers;
    extern std::array<uint64_t, 64> blackQueenZobristNumbers;
    extern std::array<uint64_t, 64> blackKingZobristNumbers;
    extern uint64_t blackToMoveZobristNumber;
    extern std::array<uint64_t, 16> castlingRightsZobristNumbers;
    extern std::array<uint64_t, 64> passantSquaresZobristNumbers;

    std::vector<uint64_t> generateRandomNumbers(size_t count, uint64_t seed);
    void initializeZobristNumbers();

    void printAllZobristKeys();
}

#endif // ZOBRIST_KEYS_H
