#ifndef PRECOMPUTED_MOVES_H
#define PRECOMPUTED_MOVES_H

#include <cstdint> // For fixed sized integers
#include <cstdlib> // For abs function

// inline keyword is used to replace everytime there's a mention of the variable
// with the variable definition. However the compiler can choose to not replace it,
// since if the replacement is too big the performance might decrease at runtime.

namespace precomputed_moves
{
    // Each bit of the uint64_t represents a square of the chessboard
    // Initialize precomputed move tables (generated using my python move generator)
    extern uint64_t knight_moves[64];
    extern uint64_t king_moves[64];
    extern uint64_t white_pawn_attacks[64];
    extern uint64_t black_pawn_attacks[64];

    // Moveable squares bitboard for bishop and rook without taking into account the edge squares
    // Used for computing blocker_bits given a position, used in BitPosition class
    extern uint64_t bishop_unfull_rays[64];
    extern uint64_t rook_unfull_rays[64];

    // Moveable squares bitboard for bishop and rook taking into account the edge squares
    // Used for computing pin_bits in BitPosition class (setPinsBits and setChecksAndPinsBits)
    extern uint64_t bishop_full_rays[64];
    extern uint64_t rook_full_rays[64];

    // Bitboards of rays from square_1 to square_2, excluding square_1 and excluding square_2
    extern uint64_t precomputedBishopMovesTableOneBlocker[64][64];
    extern uint64_t precomputedRookMovesTableOneBlocker[64][64];
    extern uint64_t precomputedQueenMovesTableOneBlocker[64][64];

    // Bitboards of rays from square_1 to square_2, excluding square_1 and including square_2 (for direct checks)
    extern uint64_t precomputedBishopMovesTableOneBlocker2[64][64];
    extern uint64_t precomputedRookMovesTableOneBlocker2[64][64];
    extern uint64_t precomputedQueenMovesTableOneBlocker2[64][64];

    // Bitboards of full line (8 squares) containing squares, otherwise 0
    extern uint64_t OnLineBitboards[64][64];

    // Bitboards of full line (8 squares) containing squares, otherwise full bitboard (for discovered checks)
    extern uint64_t OnLineBitboards2[64][64];

    void init_precomputed_moves();
    void pretty_print_all();
}
#endif