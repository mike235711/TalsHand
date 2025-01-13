#ifndef ZOBRIST_KEYS_H
#define ZOBRIST_KEYS_H

#include <vector>
#include <unordered_set>
#include <random>
#include <iostream>
#include <cstdint>

namespace zobrist_keys
{
    extern uint64_t whitePawnZobristNumbers[64];
    extern uint64_t whiteKnightZobristNumbers[64];
    extern uint64_t whiteBishopZobristNumbers[64];
    extern uint64_t whiteRookZobristNumbers[64];
    extern uint64_t whiteQueenZobristNumbers[64];
    extern uint64_t whiteKingZobristNumbers[64];
    extern uint64_t blackPawnZobristNumbers[64];
    extern uint64_t blackKnightZobristNumbers[64];
    extern uint64_t blackBishopZobristNumbers[64];
    extern uint64_t blackRookZobristNumbers[64];
    extern uint64_t blackQueenZobristNumbers[64];
    extern uint64_t blackKingZobristNumbers[64];
    extern uint64_t blackToMoveZobristNumber;
    extern uint64_t castlingRightsZobristNumbers[16];
    extern uint64_t passantSquaresZobristNumbers[64];

    std::vector<uint64_t> generateRandomNumbers(size_t count, uint64_t seed);
    void initializeZobristNumbers();

    void printAllZobristKeys();
}

#endif // ZOBRIST_KEYS_H
