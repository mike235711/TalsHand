#include "zobrist_keys.h"

namespace zobrist_keys
{
    std::array<uint64_t, 64> whitePawnZobristNumbers;
    std::array<uint64_t, 64> whiteKnightZobristNumbers;
    std::array<uint64_t, 64> whiteBishopZobristNumbers;
    std::array<uint64_t, 64> whiteRookZobristNumbers;
    std::array<uint64_t, 64> whiteQueenZobristNumbers;
    std::array<uint64_t, 64> whiteKingZobristNumbers;
    std::array<uint64_t, 64> blackPawnZobristNumbers;
    std::array<uint64_t, 64> blackKnightZobristNumbers;
    std::array<uint64_t, 64> blackBishopZobristNumbers;
    std::array<uint64_t, 64> blackRookZobristNumbers;
    std::array<uint64_t, 64> blackQueenZobristNumbers;
    std::array<uint64_t, 64> blackKingZobristNumbers;
    uint64_t blackToMoveZobristNumber;
    std::array<uint64_t, 16> castlingRightsZobristNumbers;
    std::array<uint64_t, 64> passantSquaresZobristNumbers;

    // Function to generate random numbers
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
        // Generate random numbers
        const size_t totalNumbers = 801;
        uint64_t seed = 71262;
        auto randomNumbers = generateRandomNumbers(totalNumbers, seed);

        // Fill arrays with random numbers
        for (size_t i = 0; i < 64; ++i)
        {
            whitePawnZobristNumbers[i] = randomNumbers[i];
            whiteKnightZobristNumbers[i] = randomNumbers[64 + i];
            whiteBishopZobristNumbers[i] = randomNumbers[64 * 2 + i];
            whiteRookZobristNumbers[i] = randomNumbers[64 * 3 + i];
            whiteQueenZobristNumbers[i] = randomNumbers[64 * 4 + i];
            whiteKingZobristNumbers[i] = randomNumbers[64 * 5 + i];
            blackPawnZobristNumbers[i] = randomNumbers[64 * 6 + i];
            blackKnightZobristNumbers[i] = randomNumbers[64 * 7 + i];
            blackBishopZobristNumbers[i] = randomNumbers[64 * 8 + i];
            blackRookZobristNumbers[i] = randomNumbers[64 * 9 + i];
            blackQueenZobristNumbers[i] = randomNumbers[64 * 10 + i];
            blackKingZobristNumbers[i] = randomNumbers[64 * 11 + i];
        }
        // Initialize zobrist number turn
        blackToMoveZobristNumber = randomNumbers[12 * 64];

        // 16 Castling numbers, one for each combination
        for (size_t i = 0; i < 16; ++i)
        {
            castlingRightsZobristNumbers[i] = randomNumbers[(12 * 64) + 1 + i];
        }

        // Initialize passant squares zobrist numbers
        for (size_t i = 0; i < 8; ++i)
        {
            passantSquaresZobristNumbers[16 + i] = randomNumbers[(12 * 64) + 17 + i];
            passantSquaresZobristNumbers[40 + i] = randomNumbers[(12 * 64) + 25 + i];
        }
    }
    template <size_t N>
    void printZobristArray(const std::array<uint64_t, N> &arr, const std::string &name)
    {
        std::cout << name << ":\n";
        for (size_t i = 0; i < N; ++i)
        {
            std::cout << arr[i] << " ";
        }
        std::cout << "\n";
    }

    void printAllZobristKeys()
    {
        printZobristArray(whitePawnZobristNumbers, "whitePawnZobristNumbers");
        printZobristArray(whiteKnightZobristNumbers, "whiteKnightZobristNumbers");
        printZobristArray(whiteBishopZobristNumbers, "whiteBishopZobristNumbers");
        printZobristArray(whiteRookZobristNumbers, "whiteRookZobristNumbers");
        printZobristArray(whiteQueenZobristNumbers, "whiteQueenZobristNumbers");
        printZobristArray(whiteKingZobristNumbers, "whiteKingZobristNumbers");
        printZobristArray(blackPawnZobristNumbers, "blackPawnZobristNumbers");
        printZobristArray(blackKnightZobristNumbers, "blackKnightZobristNumbers");
        printZobristArray(blackBishopZobristNumbers, "blackBishopZobristNumbers");
        printZobristArray(blackRookZobristNumbers, "blackRookZobristNumbers");
        printZobristArray(blackQueenZobristNumbers, "blackQueenZobristNumbers");
        printZobristArray(blackKingZobristNumbers, "blackKingZobristNumbers");
        std::cout << "blackToMoveZobristNumber: " << blackToMoveZobristNumber << "\n";
        printZobristArray(castlingRightsZobristNumbers, "castlingRightsZobristNumbers");
        printZobristArray(passantSquaresZobristNumbers, "passantSquaresZobristNumbers");
    }
}
