#ifndef ZOBRIST_KEYS_H
#define ZOBRIST_KEYS_H

#include <vector>
#include <unordered_set>
#include <random>
#include <iostream>
#include <cstdint>

namespace zobrist_keys
{
    extern uint64_t pieceZobristNumbers[2][6][64];
    extern uint64_t blackToMoveZobristNumber;
    extern uint64_t castlingRightsZobristNumbers[16];
    extern uint64_t passantSquaresZobristNumbers[64];

    std::vector<uint64_t> generateRandomNumbers(size_t count, uint64_t seed);
    void initializeZobristNumbers();

    void printAllZobristKeys();
}

#endif // ZOBRIST_KEYS_H
