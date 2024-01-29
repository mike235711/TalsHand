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

inline std::vector<unsigned short> getBitIndices(uint64_t bitboard)
{
    if (bitboard == 0)
    {
        return {};
    }

    std::vector<unsigned short> indices;
    indices.reserve(32); // Optimize this value based on your use case

    while (bitboard)
    {
        // Use your lsb function to get the index of the least significant bit
        unsigned short lsbi = getLeastSignificantBitIndex(bitboard);
        indices.push_back(lsbi);

        // Remove the least significant bit from the bitboard
        bitboard &= bitboard - 1;
    }

    return indices;
}

inline bool hasOneOne(uint64_t bitboard) // Works
// Given a bitboard determine if bit has one one or not (for pins)
{
    // If the result is zero, it had only one 1-bit
    return (bitboard & (bitboard - 1)) == 0;
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