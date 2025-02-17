#include <cstdint> // For fixed sized integers
#include "precomputed_moves.h"
#include <iostream>

namespace precomputed_moves
{
    // Initialize precomputed move tables (generated using my python move generator)
    uint64_t knight_moves[64];
    uint64_t king_moves[64];
    uint64_t pawn_attacks[2][64];

    // Moveable squares bitboard for bishop and rook without taking into account the edge squares
    // Used for computing blocker_bits given a position, used in BitPosition class
    uint64_t bishop_unfull_rays[64];
    uint64_t rook_unfull_rays[64];

    // Moveable squares bitboard for bishop and rook taking into account the edge squares
    // Used for computing pin_bits in BitPosition class (setPinsBits and setChecksAndPinsBits)
    uint64_t bishop_full_rays[64];
    uint64_t rook_full_rays[64];

    // Bitboards of rays from square_1 to square_2, excluding square_1 and excluding square_2
    uint64_t precomputedBishopMovesTableOneBlocker[64][64];
    uint64_t precomputedRookMovesTableOneBlocker[64][64];
    uint64_t precomputedQueenMovesTableOneBlocker[64][64];

    // Bitboards of rays from square_1 to square_2, excluding square_1 and including square_2 (for direct checks)
    uint64_t precomputedBishopMovesTableOneBlocker2[64][64];
    uint64_t precomputedRookMovesTableOneBlocker2[64][64];
    uint64_t precomputedQueenMovesTableOneBlocker2[64][64];

    // Bitboards of full line (8 squares) containing squares, otherwise 0
    uint64_t OnLineBitboards[64][64];

    // Helper to check if a square is on the board
    inline bool is_valid_square(int file, int rank)
    {
        return file >= 0 && file < 8 && rank >= 0 && rank < 8;
    }

    // Calculate knight moves
    uint64_t calculate_knight_moves(int square)
    {
        uint64_t moves = 0;
        int file = square % 8;
        int rank = square / 8;

        const int knight_offsets[8][2] = {
            {2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};

        for (auto &offset : knight_offsets)
        {
            int new_file = file + offset[0];
            int new_rank = rank + offset[1];
            if (is_valid_square(new_file, new_rank))
            {
                moves |= (1ULL << (new_rank * 8 + new_file));
            }
        }

        return moves;
    }

    // Calculate king moves
    uint64_t calculate_king_moves(int square)
    {
        uint64_t moves = 0;
        int file = square % 8;
        int rank = square / 8;

        const int king_offsets[8][2] = {
            {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

        for (auto &offset : king_offsets)
        {
            int new_file = file + offset[0];
            int new_rank = rank + offset[1];
            if (is_valid_square(new_file, new_rank))
            {
                moves |= (1ULL << (new_rank * 8 + new_file));
            }
        }

        return moves;
    }

    // Calculate pawn attack moves
    uint64_t calculate_pawn_attacks(int square, bool is_white)
    {
        uint64_t attacks = 0;
        int file = square % 8;
        int rank = square / 8;

        if (is_white && rank == 7)
            return 0;
        if (not is_white && rank == 0)
            return 0;

        const int offsets[2] = {
            -1, 1 // Left and right attacks
        };

        for (auto &offset : offsets)
        {
            int new_file = file + offset;
            int new_rank = rank + (is_white ? 1 : -1);
            if (is_valid_square(new_file, new_rank))
            {
                attacks |= (1ULL << (new_rank * 8 + new_file));
            }
        }

        return attacks;
    }

    // Determine the straight ray between two squares
    uint64_t calculate_straight_between(int square1, int square2, bool include_square2)
    {
        int file1 = square1 % 8;
        int rank1 = square1 / 8;
        int file2 = square2 % 8;
        int rank2 = square2 / 8;

        int file_step = (file2 - file1) ? (file2 - file1) / abs(file2 - file1) : 0;
        int rank_step = (rank2 - rank1) ? (rank2 - rank1) / abs(rank2 - rank1) : 0;

        if (abs(file2 - file1) != 0 && abs(rank2 - rank1) != 0)
            return 0;

        uint64_t between = 0;
        int current_file = file1 + file_step;
        int current_rank = rank1 + rank_step;

        while (current_file != file2 || current_rank != rank2)
        {
            between |= (1ULL << (current_rank * 8 + current_file));
            current_file += file_step;
            current_rank += rank_step;
        }

        if (include_square2)
            between |= (1ULL << square2);

        return between;
    }

    // Determine the diagonal ray between two squares
    uint64_t calculate_diagonal_between(int square1, int square2, bool include_square2)
    {
        int file1 = square1 % 8;
        int rank1 = square1 / 8;
        int file2 = square2 % 8;
        int rank2 = square2 / 8;

        int file_step = (file2 - file1) ? (file2 - file1) / abs(file2 - file1) : 0;
        int rank_step = (rank2 - rank1) ? (rank2 - rank1) / abs(rank2 - rank1) : 0;

        if (abs(file2 - file1) != abs(rank2 - rank1))
            return 0;

        uint64_t between = 0;
        int current_file = file1 + file_step;
        int current_rank = rank1 + rank_step;

        while (current_file != file2 || current_rank != rank2)
        {
            between |= (1ULL << (current_rank * 8 + current_file));
            current_file += file_step;
            current_rank += rank_step;
        }

        if (include_square2)
            between |= (1ULL << square2);

        return between;
    }

    // If the squares lie on the same diagonal or line it outputs the full line/diagonal.
    // else 0
    uint64_t calculate_full_between(int square1, int square2)
    {
        int file1 = square1 % 8;
        int rank1 = square1 / 8;
        int file2 = square2 % 8;
        int rank2 = square2 / 8;

        int file_diff = file2 - file1;
        int rank_diff = rank2 - rank1;

        // Determine if the squares lie on the same line or diagonal
        if (!(file_diff == 0 || rank_diff == 0 || abs(file_diff) == abs(rank_diff)))
        {
            return 0; // Not on the same line or diagonal
        }

        int file_step = (file_diff != 0) ? file_diff / abs(file_diff) : 0;
        int rank_step = (rank_diff != 0) ? rank_diff / abs(rank_diff) : 0;

        if (file_step == 0 && rank_step == 0)
            return 0;

        uint64_t between = 0;

        // Traverse the entire line or diagonal
        int current_file = file1;
        int current_rank = rank1;
        while (is_valid_square(current_file, current_rank))
        {
            between |= (1ULL << (current_rank * 8 + current_file));
            current_file += file_step;
            current_rank += rank_step;
        }

        current_file = file1 - file_step;
        current_rank = rank1 - rank_step;
        while (is_valid_square(current_file, current_rank))
        {
            between |= (1ULL << (current_rank * 8 + current_file));
            current_file -= file_step;
            current_rank -= rank_step;
        }

        return between;
    }

    void init_precomputed_moves()
    {
        // Populate the arrays
        for (int square = 0; square < 64; ++square)
        {
            // These bellow are all OK
            knight_moves[square] = calculate_knight_moves(square);
            king_moves[square] = calculate_king_moves(square);
            pawn_attacks[0][square] = calculate_pawn_attacks(square, true);
            pawn_attacks[1][square] = calculate_pawn_attacks(square, false);

            bishop_unfull_rays[square] = 0;
            rook_unfull_rays[square] = 0;
            bishop_full_rays[square] = 0;
            rook_full_rays[square] = 0;

            for (int target = 0; target < 64; ++target)
            {
                if (square == target)
                    continue;
                bishop_unfull_rays[square] |= calculate_diagonal_between(square, target, false);
                bishop_full_rays[square] |= calculate_diagonal_between(square, target, true);
            }

            for (int target = 0; target < 64; ++target)
            {
                if (square == target)
                    continue;
                rook_unfull_rays[square] |= calculate_straight_between(square, target, false);
                rook_full_rays[square] |= calculate_straight_between(square, target, true);
            }

            for (int target = 0; target < 64; ++target)
            {
                precomputedBishopMovesTableOneBlocker[square][target] = calculate_diagonal_between(square, target, false);
                precomputedRookMovesTableOneBlocker[square][target] = calculate_straight_between(square, target, false);
                precomputedQueenMovesTableOneBlocker[square][target] = precomputedBishopMovesTableOneBlocker[square][target] | precomputedRookMovesTableOneBlocker[square][target];

                precomputedBishopMovesTableOneBlocker2[square][target] = calculate_diagonal_between(square, target, true);
                precomputedRookMovesTableOneBlocker2[square][target] = calculate_straight_between(square, target, true);
                precomputedQueenMovesTableOneBlocker2[square][target] = precomputedBishopMovesTableOneBlocker2[square][target] | precomputedRookMovesTableOneBlocker2[square][target];

                OnLineBitboards[square][target] = calculate_full_between(square, target);
            }
        }
    }

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
        std::cout << "Knight Moves:\n";
        for (int square = 0; square < 64; ++square)
        {
            std::cout << "Square " << square << ":\n";
            pretty_print_bitboard(knight_moves[square]);
        }

        std::cout << "King Moves:\n";
        for (int square = 0; square < 64; ++square)
        {
            std::cout << "Square " << square << ":\n";
            pretty_print_bitboard(king_moves[square]);
        }

        std::cout << "White Pawn Attacks:\n";
        for (int square = 0; square < 64; ++square)
        {
            std::cout << "Square " << square << ":\n";
            pretty_print_bitboard(pawn_attacks[0][square]);
        }

        std::cout << "Black Pawn Attacks:\n";
        for (int square = 0; square < 64; ++square)
        {
            std::cout << "Square " << square << ":\n";
            pretty_print_bitboard(pawn_attacks[1][square]);
        }

        std::cout << "Bishop Unfull Rays:\n";
        for (int square = 0; square < 64; ++square)
        {
            std::cout << "Square " << square << ":\n";
            pretty_print_bitboard(bishop_unfull_rays[square]);
        }

        std::cout << "Rook Unfull Rays:\n";
        for (int square = 0; square < 64; ++square)
        {
            std::cout << "Square " << square << ":\n";
            pretty_print_bitboard(rook_unfull_rays[square]);
        }

        std::cout << "Bishop Full Rays:\n";
        for (int square = 0; square < 64; ++square)
        {
            std::cout << "Square " << square << ":\n";
            pretty_print_bitboard(bishop_full_rays[square]);
        }

        std::cout << "Rook Full Rays:\n";
        for (int square = 0; square < 64; ++square)
        {
            std::cout << "Square " << square << ":\n";
            pretty_print_bitboard(rook_full_rays[square]);
        }

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