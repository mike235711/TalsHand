// Create tables for precomputed moves for bishops and rooks.
#include <cstdint> // For fixed sized integers
#include <vector> // For std::vector
#include "bit_utils.h" // For bit indeces, generateSubits and generateSubvectors
#include "magics.h"
#include <map> // For std::map

uint64_t generateRookUnfullRays(unsigned short square) // Works
// Given a square give the bit representing the ray mask of the rook
// excluding the edge squares.
{
    uint64_t moves = 0;
    int r = square / 8;
    int c = square % 8;
    int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; // Vertical and horizontal
    for (int i = 0; i < 4; ++i)
    {
        int dr = directions[i][0];
        int dc = directions[i][1];
        int nr = r + dr;
        int nc = c + dc;

        if (r == 0 && c == 0) //Then we can go to the 1st row and 1st column, but not the 8th row or 8th column
            {
                while (0 <= nr && nr < 7 && 0 <= nc && nc < 7)
                {
                    moves |= 1ULL << (nr * 8 + nc);
                    nr += dr;
                    nc += dc;
                }
            }
        else if (r == 0 && c == 7)
        {
            while (0 <= nr && nr < 7 && 0 < nc && nc <= 7)
            {
                moves |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
        }
        else if (r == 7 && c == 0)
        {
            while (0 < nr && nr <= 7 && 0 <= nc && nc < 7)
            {
                moves |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
        }
        else if (r == 7 && c == 7)
        {
            while (0 < nr && nr <= 7 && 0 < nc && nc <= 7)
            {
                moves |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
        }
        else if (r == 0)
        {
            while (0 <= nr && nr < 7 && 0 < nc && nc < 7)
            {
                moves |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
        }
        else if (r == 7)
        {
            while (0 < nr && nr <= 7 && 0 < nc && nc < 7)
            {
                moves |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
        }
        else if (c == 0)
        {
            while (0 < nr && nr < 7 && 0 <= nc && nc < 7)
            {
                moves |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
        }
        else if (c == 7)
        {
            while (0 < nr && nr < 7 && 0 < nc && nc <= 7)
            {
                moves |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
        }
        else
        {
            while (0 < nr && nr < 7 && 0 < nc && nc < 7)
            {
                moves |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
        }
    }
    return moves;
}

uint64_t generateBishopUnfullRays(unsigned short square) // Works
// Given a square give the bit representing the ray mask of the bishop
// excluding the edge squares.
{
    uint64_t moves = 0;
    int r = square / 8;
    int c = square % 8;
    int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}; // Diagonals

    for (int i = 0; i < 4; ++i) // For each direction
    {
        int dr = directions[i][0];
        int dc = directions[i][1];
        int nr = r + dr;
        int nc = c + dc;

        while (0 < nr && nr < 7 && 0 < nc && nc < 7)
        {
            moves |= 1ULL << (nr * 8 + nc);
            nr += dr;
            nc += dc;
        }
    }

    return moves;
}

std::vector<uint64_t> generateBishopBlockerConfigurations(unsigned short square) // Works
// Given a square get all the possible blocker configurations for bishops
// They don't include the exterior squares
{
        uint64_t ray_mask{generateBishopUnfullRays(square)};
        return generateSubbits(ray_mask);
}

std::vector<uint64_t> generateRookBlockerConfigurations(unsigned short square) // Works
// Given a square get all the possible blocker configurations for rooks
// They don't include the exterior squares
{
        uint64_t ray_mask{generateRookUnfullRays(square)};
        return generateSubbits(ray_mask);
}

uint64_t getBishopValidMovesIncludingCaptures(unsigned short square, uint64_t blockers_bit) // Works
// Given a square and a blockers bit get the bitboard representing the moveable squares including blocker capture
{
    uint64_t moves_bit{};
    int r = square / 8;
    int c = square % 8;
    int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}; // Diagonals
    for (int i = 0; i < 4; ++i)                                  // For each direction
    {
        int dr = directions[i][0];
        int dc = directions[i][1];
        int nr = r + dr;
        int nc = c + dc;

        while (0 <= nr && nr <= 7 && 0 <= nc && nc <= 7)
        {
            uint64_t pos_bit = 1ULL << (nr * 8 + nc);
            moves_bit |= pos_bit;

            if ((blockers_bit & pos_bit) != 0)
            {
                // If there's a blocker, include this square as a capture but stop looking further in this direction.
                break;
            }

            nr += dr;
            nc += dc;
        }
    }
    return moves_bit;
}

uint64_t getRookValidMovesIncludingCaptures(unsigned short square, uint64_t blockers_bit) // Works
// Given a square and a blockers bit get the bitboard representing the moveable squares including blocker capture
{
    uint64_t moves_bit{};
    int r = square / 8;
    int c = square % 8;
    int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; // Diagonals
    for (int i = 0; i < 4; ++i)                                  // For each direction
    {
        int dr = directions[i][0];
        int dc = directions[i][1];
        int nr = r + dr;
        int nc = c + dc;

        while (0 <= nr && nr <= 7 && 0 <= nc && nc <= 7)
        {
            uint64_t pos_bit = 1ULL << (nr * 8 + nc);
            moves_bit |= pos_bit;

            if ((blockers_bit & pos_bit) != 0)
            {
                // If there's a blocker, include this square as a capture but stop looking further in this direction.
                break;
            }

            nr += dr;
            nc += dc;
        }
    }
    return moves_bit;
}

std::vector<std::map<uint64_t, uint64_t>> getBishopLongPrecomputedTable() // Works
{
    std::vector<std::map<uint64_t, uint64_t>> table{};
    for (unsigned short square = 0; square < 64; ++square) // For each square
    {
        std::map<uint64_t, uint64_t> blockers_moves_map{}; // A map (dictionary) of blocker_bit : moves_bit
        std::vector<uint64_t> blockers_for_square{generateBishopBlockerConfigurations(square)}; // A vector of all the blocker_bits at the given square
        for (uint64_t blocker: blockers_for_square)
        {
            blockers_moves_map.insert(std::make_pair(blocker, getBishopValidMovesIncludingCaptures(square, blocker)));
        }
        table.push_back(blockers_moves_map);
    }
    return table;
}

std::vector<std::map<uint64_t, uint64_t>> getRookLongPrecomputedTable() // Works
{
    std::vector<std::map<uint64_t, uint64_t>> table{};
    for (unsigned short square = 0; square < 64; ++square) // For each square
    {
        std::map<uint64_t, uint64_t> blockers_moves_map{};
        std::vector<uint64_t> blockers_for_square{generateRookBlockerConfigurations(square)};
        for (uint64_t blocker : blockers_for_square)
        {
            blockers_moves_map.insert(std::make_pair(blocker, getRookValidMovesIncludingCaptures(square, blocker)));
        }
        table.push_back(blockers_moves_map);
    }
    return table;
}
