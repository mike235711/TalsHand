#include "zobrist_keys.h"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <random>

namespace zobrist_keys
{
    uint64_t whitePawnZobristNumbers[64];
    uint64_t whiteKnightZobristNumbers[64];
    uint64_t whiteBishopZobristNumbers[64];
    uint64_t whiteRookZobristNumbers[64];
    uint64_t whiteQueenZobristNumbers[64];
    uint64_t whiteKingZobristNumbers[64];
    uint64_t blackPawnZobristNumbers[64];
    uint64_t blackKnightZobristNumbers[64];
    uint64_t blackBishopZobristNumbers[64];
    uint64_t blackRookZobristNumbers[64];
    uint64_t blackQueenZobristNumbers[64];
    uint64_t blackKingZobristNumbers[64];
    uint64_t blackToMoveZobristNumber;
    uint64_t castlingRightsZobristNumbers[16];
    uint64_t passantSquaresZobristNumbers[64];

    std::vector<uint64_t> generateRandomNumbers(size_t count, uint64_t seed)
    {
        std::unordered_set<uint64_t> randomNumbers;
        std::mt19937_64 eng(seed);
        std::uniform_int_distribution<uint64_t> distr(1, UINT64_MAX - 1);

        while (randomNumbers.size() < count)
        {
            randomNumbers.insert(distr(eng));
        }

        return std::vector<uint64_t>(randomNumbers.begin(), randomNumbers.end());
    }

    void initializeZobristNumbers()
    {
        const size_t totalNumbers = 801;
        uint64_t seed = 71262;
        auto randomNumbers = generateRandomNumbers(totalNumbers, seed);

        for (size_t i = 0; i < 64; ++i)
        {
            whitePawnZobristNumbers[i] = randomNumbers[i];
            whiteKnightZobristNumbers[i] = randomNumbers[64 + i];
            whiteBishopZobristNumbers[i] = randomNumbers[128 + i];
            whiteRookZobristNumbers[i] = randomNumbers[192 + i];
            whiteQueenZobristNumbers[i] = randomNumbers[256 + i];
            whiteKingZobristNumbers[i] = randomNumbers[320 + i];
            blackPawnZobristNumbers[i] = randomNumbers[384 + i];
            blackKnightZobristNumbers[i] = randomNumbers[448 + i];
            blackBishopZobristNumbers[i] = randomNumbers[512 + i];
            blackRookZobristNumbers[i] = randomNumbers[576 + i];
            blackQueenZobristNumbers[i] = randomNumbers[640 + i];
            blackKingZobristNumbers[i] = randomNumbers[704 + i];
        }

        blackToMoveZobristNumber = randomNumbers[768];

        for (size_t i = 0; i < 16; ++i)
        {
            castlingRightsZobristNumbers[i] = randomNumbers[769 + i];
        }

        for (size_t i = 0; i < 8; ++i)
        {
            passantSquaresZobristNumbers[16 + i] = randomNumbers[785 + i];
            passantSquaresZobristNumbers[40 + i] = randomNumbers[793 + i];
        }
    }

    void printArray(const uint64_t *arr, size_t size, const std::string &name)
    {
        std::cout << name << ":\n";
        for (size_t i = 0; i < size; ++i)
        {
            std::cout << arr[i] << " ";
        }
        std::cout << "\n";
    }

    void printAllZobristKeys()
    {
        printArray(whitePawnZobristNumbers, 64, "whitePawnZobristNumbers");
        printArray(whiteKnightZobristNumbers, 64, "whiteKnightZobristNumbers");
        printArray(whiteBishopZobristNumbers, 64, "whiteBishopZobristNumbers");
        printArray(whiteRookZobristNumbers, 64, "whiteRookZobristNumbers");
        printArray(whiteQueenZobristNumbers, 64, "whiteQueenZobristNumbers");
        printArray(whiteKingZobristNumbers, 64, "whiteKingZobristNumbers");
        printArray(blackPawnZobristNumbers, 64, "blackPawnZobristNumbers");
        printArray(blackKnightZobristNumbers, 64, "blackKnightZobristNumbers");
        printArray(blackBishopZobristNumbers, 64, "blackBishopZobristNumbers");
        printArray(blackRookZobristNumbers, 64, "blackRookZobristNumbers");
        printArray(blackQueenZobristNumbers, 64, "blackQueenZobristNumbers");
        printArray(blackKingZobristNumbers, 64, "blackKingZobristNumbers");
        std::cout << "blackToMoveZobristNumber: " << blackToMoveZobristNumber << "\n";
        printArray(castlingRightsZobristNumbers, 16, "castlingRightsZobristNumbers");
        printArray(passantSquaresZobristNumbers, 64, "passantSquaresZobristNumbers");
    }
}
