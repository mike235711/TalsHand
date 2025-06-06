#ifndef BIT_UTILS_H
#define BIT_UTILS_H

#include <cstdint> // For fixed sized integers
#include <vector>
// Here we use inline because this permits the compiler copy inline the function
// definitions whenever the functions name appears in the project.

inline int getLeastSignificantBitIndex(uint64_t bitboard)
{
    if (bitboard == 0)
        return 65;
    else
        return __builtin_ctzll(bitboard);
}

inline int getLeastSignificantBitIndexx(uint64_t bitboard)
{
    return __builtin_ctzll(bitboard);
}

inline int popLeastSignificantBit(uint64_t &b)
{
    const int_least32_t s = getLeastSignificantBitIndexx(b);
    b &= b - 1;
    return s;
}

inline int invertIndex(int index)
{
    int newRow{7 - (index / 8)};
    return newRow * 8 + (index % 8);
}

inline std::vector<int> getBitIndices(uint64_t bitboard)
{
    if (bitboard == 0)
    {
        return {};
    }

    std::vector<int> indices;
    indices.reserve(32); 

    while (bitboard)
    {
        // Get the index of the least significant bit
        int lsbi = getLeastSignificantBitIndex(bitboard);
        indices.push_back(lsbi);

        // Remove the least significant bit from the bitboard
        bitboard &= bitboard - 1;
    }

    return indices;
}

inline bool hasOneOne(uint64_t bitboard)
{
    return (bitboard & (bitboard - 1)) == 0;
}

inline std::vector<std::vector<int>> generateSubvectors(const std::vector<int> &vec) // Works
// Returns all subvectors from a vector. Used in generate_subbits.
{
    std::vector<std::vector<int>> subvectors;
    int n = vec.size();
    unsigned int powSetSize = 1 << n; // There are 2^n subsets

    for (unsigned int counter = 0; counter < powSetSize; ++counter)
    {
        std::vector<int> subvec;
        for (int j = 0; j < n; ++j)
        {
            // Check if jth element is in the current subset
            if (counter & (1 << j))
            {
                subvec.push_back(vec[j]);
            }
        }
        subvectors.push_back(subvec);
    }

    return subvectors;
}

inline std::vector<uint64_t> generateSubbits(uint64_t bit) // Works
// Returns the all the subbits of a given bit. Used in generate_blocker_configurations.
{
    std::vector<int> indeces{getBitIndices(bit)};                                 // Indeces of 1's in bit
    std::vector<std::vector<int>> subvector_indeces{generateSubvectors(indeces)}; // Vector of subvectors containing all the indeces of the subbits
    std::vector<uint64_t> subbits{};
    for (std::vector<int> indeces : subvector_indeces)
    {
        uint64_t bit{};
        for (int index : indeces)
        {
            bit |= (1ULL << index);
        }
        subbits.push_back(bit);
    }
    return subbits;
}

// Helper function to count the number of set bits in a 64-bit integer
inline int countBits(uint64_t bitboard)
{
    int count = 0;
    while (bitboard)
    {
        bitboard &= (bitboard - 1); // Clear the least significant bit set
        count++;
    }
    return count;
}
#endif // BIT_UTILS_H