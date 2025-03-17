#include "zobrist_keys.h"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <random>

namespace zobrist_keys
{
    uint64_t pieceZobristNumbers[2][6][64];
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
        uint64_t seed = 71272;
        auto randomNumbers = generateRandomNumbers(totalNumbers, seed);

        for (size_t i = 0; i < 64; ++i)
        {
            pieceZobristNumbers[0][0][i] = randomNumbers[i];
            pieceZobristNumbers[0][1][i] = randomNumbers[64 + i];
            pieceZobristNumbers[0][2][i] = randomNumbers[128 + i];
            pieceZobristNumbers[0][3][i] = randomNumbers[192 + i];
            pieceZobristNumbers[0][4][i] = randomNumbers[256 + i];
            pieceZobristNumbers[0][5][i] = randomNumbers[320 + i];
            pieceZobristNumbers[1][0][i] = randomNumbers[384 + i];
            pieceZobristNumbers[1][1][i] = randomNumbers[448 + i];
            pieceZobristNumbers[1][2][i] = randomNumbers[512 + i];
            pieceZobristNumbers[1][3][i] = randomNumbers[576 + i];
            pieceZobristNumbers[1][4][i] = randomNumbers[640 + i];
            pieceZobristNumbers[1][5][i] = randomNumbers[704 + i];
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
        std::cout << "blackToMoveZobristNumber: " << blackToMoveZobristNumber << "\n";
        printArray(castlingRightsZobristNumbers, 16, "castlingRightsZobristNumbers");
        printArray(passantSquaresZobristNumbers, 64, "passantSquaresZobristNumbers");
    }
}
