#ifndef BIT_UTILS_H
#define BIT_UTILS_H

#include <cstdint> // For fixed sized integers
// Here we use inline because this permits the compiler copy inline the function
// definitions whenever the functions name appears in the project.

inline unsigned short getLeastSignificantBitIndex(uint64_t bitboard) // Works
// Give a bitboard get the index of the first 1 appearing in the bit
{
    unsigned short index {0};
    while ((bitboard & 1) == 0)
    {
        bitboard >>= 1;
        ++index;
    }
    return index;
}

inline std::vector<unsigned short> getBitIndices(uint64_t bitboard) // Works
// Given a bitboard get the indices of all the 1's in the bitboard
{
    if (bitboard == 0)
    {
        return std::vector<unsigned short>{};
    }
    std::vector<unsigned short> indices{};
    unsigned short index{0};
    while (index < 64)
    {
        if ((bitboard & 1) == 1)
        {
            indices.push_back(index);
        }
        bitboard >>= 1;
        ++index;
    }
    return indices;
}

inline bool hasOneOne(uint64_t bitboard)
// Given a bitboard determine if bit has one one or not (for pins)
{
    if (bitboard == 0)
    {
        return false;
    }

    // Turn off the rightmost 1-bit
    uint64_t turnedOffBitboard = bitboard & (bitboard - 1);

    // If the result is zero, it had only one 1-bit
    return turnedOffBitboard == 0;
}

inline std::vector<std::vector<unsigned short>> generateSubvectors(const std::vector<unsigned short> &vec) // Works
// Returns all subvectors from a vector. Used in generate_subbits.
{
    std::vector<std::vector<unsigned short>> subvectors;
    int n = vec.size();
    unsigned int powSetSize = 1 << n; // There are 2^n subsets

    for (unsigned int counter = 0; counter < powSetSize; ++counter)
    {
        std::vector<unsigned short> subvec;
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
    std::vector<unsigned short> indeces{getBitIndices(bit)};                                 // Indeces of 1's in bit
    std::vector<std::vector<unsigned short>> subvector_indeces{generateSubvectors(indeces)}; // Vector of subvectors containing all the indeces of the subbits
    std::vector<uint64_t> subbits{};
    for (std::vector<unsigned short> indeces : subvector_indeces)
    {
        uint64_t bit{};
        for (unsigned short index : indeces)
        {
            bit |= (1ULL << index);
        }
        subbits.push_back(bit);
    }
    return subbits;
}
#endif // BIT_UTILS_H