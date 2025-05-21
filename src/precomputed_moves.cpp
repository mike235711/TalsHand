#include <cstdint> // For fixed sized integers
#include "precomputed_moves.h"
#include <iostream>

namespace precomputed_moves
{
    ///////////////////////////
    // For Debugging Purposes
    ///////////////////////////

    void pretty_print_bitboard(uint64_t bitboard)
    {
        for (int rank = 7; rank >= 0; --rank)
        {
            for (int file = 0; file < 8; ++file)
            {
                std::cout << ((bitboard & (1ULL << (rank * 8 + file))) ? "1 " : "0 ");
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }

    void pretty_print_all()
    {
        // std::cout << "Knight Moves:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square " << square << ":\n";
        //     pretty_print_bitboard(knight_moves[square]);
        // }

        // std::cout << "King Moves:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square " << square << ":\n";
        //     pretty_print_bitboard(king_moves[square]);
        // }

        // std::cout << "White Pawn Attacks:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square " << square << ":\n";
        //     pretty_print_bitboard(pawn_attacks[0][square]);
        // }

        // std::cout << "Black Pawn Attacks:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square " << square << ":\n";
        //     pretty_print_bitboard(pawn_attacks[1][square]);
        // }

        // std::cout << "Bishop Unfull Rays:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square " << square << ":\n";
        //     pretty_print_bitboard(bishop_unfull_rays[square]);
        // }

        // std::cout << "Rook Unfull Rays:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square " << square << ":\n";
        //     pretty_print_bitboard(rook_unfull_rays[square]);
        // }

        // std::cout << "Bishop Full Rays:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square " << square << ":\n";
        //     pretty_print_bitboard(bishop_full_rays[square]);
        // }

        // std::cout << "Rook Full Rays:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square " << square << ":\n";
        //     pretty_print_bitboard(rook_full_rays[square]);
        // }

        // std::cout << "Bishop One Blocker:\n";
        // for (int square = 0; square < 64; ++square)
        // {
        //     std::cout << "Square 1 " << square << ":\n";
        //     for (int square2 = 0; square2 < 64; ++square2)
        //     {
        //         std::cout << "Square 2 " << square2 << ":\n";
        //         pretty_print_bitboard(precomputedBishopMovesTableOneBlocker[square][square2]);
        //     }
        // }

    }
}