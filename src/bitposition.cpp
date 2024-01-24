#include <iostream>
#include <vector>
#include <cstdint>             // For fixed sized integers
#include "precomputed_moves.h" // Include the precomputed move constants
#include "bitposition.h"       // Where the BitPosition class is defined
#include "bit_utils.h"         // Bit utility functions
#include "move.h"

std::array<Move, 4> castling_moves{Move(16772), Move(16516), Move(20412), Move(20156)}; // WKS, WQS, BKS, BQS
std::array<Move, 16> double_moves{Move(34864), Move(34929), Move(34994), Move(35059),   // Black double pawn moves (a-h)
                                  Move(35124), Move(35189), Move(35254), Move(35319),
                                  Move(34312), Move(34377), Move(34442), Move(34507), // White double pawn moves (a-h)
                                  Move(34572), Move(34637), Move(34702), Move(34767)};
std::array<uint64_t, 64> passant_bitboards{0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0,
                                           65536, 131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608,
                                           0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0,
                                           1099511627776, 2199023255552, 4398046511104, 8796093022208, 17592186044416, 35184372088832, 70368744177664, 140737488355328,
                                           0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0};

// Some more complex BitPosition member functions
// Constructor
BitPosition::BitPosition(uint64_t white_pawns_bit, uint64_t white_knights_bit, uint64_t white_bishops_bit,
                         uint64_t white_rooks_bit, uint64_t white_queens_bit, uint64_t white_king_bit,
                         uint64_t black_pawns_bit, uint64_t black_knights_bit, uint64_t black_bishops_bit,
                         uint64_t black_rooks_bit, uint64_t black_queens_bit, uint64_t black_king_bit,
                         bool turn,
                         bool white_kingside_castling, bool white_queenside_castling, bool black_kingside_castling,
                         bool black_queenside_castling)
    : m_white_pawns_bit{white_pawns_bit}, m_white_knights_bit{white_knights_bit}, m_white_bishops_bit{white_bishops_bit}, m_white_rooks_bit{white_rooks_bit}, m_white_queens_bit{white_queens_bit}, m_white_king_bit{white_king_bit},
      m_black_pawns_bit{black_pawns_bit}, m_black_knights_bit{black_knights_bit}, m_black_bishops_bit{black_bishops_bit}, m_black_rooks_bit{black_rooks_bit}, m_black_queens_bit{black_queens_bit}, m_black_king_bit{black_king_bit},
      m_turn{turn}, m_white_kingside_castling{white_kingside_castling}, m_white_queenside_castling{white_queenside_castling}, m_black_kingside_castling{black_kingside_castling}, m_black_queenside_castling{black_queenside_castling},
      m_white_pieces_bit{m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | m_white_queens_bit | white_king_bit}, m_black_pieces_bit{m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | m_black_queens_bit | black_king_bit},
      m_all_pieces_bit{m_white_pieces_bit | m_black_pieces_bit}, m_all_pieces_bit_without_white_king{m_all_pieces_bit & ~m_white_king_bit}, m_all_pieces_bit_without_black_king{m_all_pieces_bit & ~m_black_king_bit},
      m_white_king_position{getLeastSignificantBitIndex(m_white_king_bit)}, m_black_king_position{getLeastSignificantBitIndex(m_black_king_bit)}
{
    BitPosition::setBlackBishopsAttackedSquares();
    BitPosition::setBlackRooksAttackedSquares();
    BitPosition::setBlackQueensAttackedSquares();
    BitPosition::setWhiteBishopsAttackedSquares();
    BitPosition::setWhiteRooksAttackedSquares();
    BitPosition::setWhiteQueensAttackedSquares();
    BitPosition::setBlackKnightsAttackedSquares();
    BitPosition::setBlackKingAttackedSquares();
    BitPosition::setBlackPawnsAttackedSquares();
    BitPosition::setWhiteKnightsAttackedSquares();
    BitPosition::setWhiteKingAttackedSquares();
    BitPosition::setWhitePawnsAttackedSquares();
}

bool BitPosition::isCheck() const
// Return if we are in check or not
// This member function is faster than setChecksAndPinsBits
// We are going to check for checks more efficiently from the kings position and assuming it can move as any piece.
// This member function is the first member function to be called at the search.
{
    if (m_turn == true) // If white's turn
    {
        if ((m_squares_attacked_by_black_pieces & m_white_king_bit) == 0)
            return false;
        return true;
    }

    else // If black's turn
    {
        if ((m_squares_attacked_by_white_pieces & m_black_king_bit) == 0)
            return false;
        return true;
    }
    return false; // No checks
}
void BitPosition::setWhiteBishopsAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_white_bishops_bit))
    {
        m_squares_attacked_by_white_bishops |= precomputed_moves::precomputedBishopMovesTable[origin_square][m_all_pieces_bit & precomputed_moves::bishop_unfull_rays[origin_square]];
    }
}
void BitPosition::setBlackBishopsAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_black_bishops_bit))
    {
        m_squares_attacked_by_black_bishops |= precomputed_moves::precomputedBishopMovesTable[origin_square][m_all_pieces_bit & precomputed_moves::bishop_unfull_rays[origin_square]];
    }
}
void BitPosition::setWhiteRooksAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_white_rooks_bit))
    {
        m_squares_attacked_by_white_rooks |= precomputed_moves::precomputedRookMovesTable[origin_square][m_all_pieces_bit & precomputed_moves::rook_unfull_rays[origin_square]];
    }
}
void BitPosition::setBlackRooksAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_black_rooks_bit))
    {
        m_squares_attacked_by_black_rooks |= precomputed_moves::precomputedRookMovesTable[origin_square][m_all_pieces_bit & precomputed_moves::rook_unfull_rays[origin_square]];
    }
}
void BitPosition::setWhiteQueensAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_white_queens_bit))
    {
        m_squares_attacked_by_white_queens |= precomputed_moves::precomputedBishopMovesTable[origin_square][m_all_pieces_bit & precomputed_moves::bishop_unfull_rays[origin_square]];
        m_squares_attacked_by_white_queens |= precomputed_moves::precomputedRookMovesTable[origin_square][m_all_pieces_bit & precomputed_moves::rook_unfull_rays[origin_square]];
    }
}
void BitPosition::setBlackQueensAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_black_queens_bit))
    {
        m_squares_attacked_by_black_queens |= precomputed_moves::precomputedBishopMovesTable[origin_square][m_all_pieces_bit & precomputed_moves::bishop_unfull_rays[origin_square]];
        m_squares_attacked_by_black_queens |= precomputed_moves::precomputedRookMovesTable[origin_square][m_all_pieces_bit & precomputed_moves::rook_unfull_rays[origin_square]];
    }
}
void BitPosition::setWhiteKnightsAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_white_knights_bit))
    {
        m_squares_attacked_by_white_knights |= precomputed_moves::knight_moves[origin_square];
    }
}
void BitPosition::setBlackKnightsAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_black_knights_bit))
    {
        m_squares_attacked_by_black_knights |= precomputed_moves::knight_moves[origin_square];
    }
}
void BitPosition::setWhitePawnsAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_white_pawns_bit))
    {
        m_squares_attacked_by_white_pawns |= precomputed_moves::white_pawn_attacks[origin_square];
    }
}
void BitPosition::setBlackPawnsAttackedSquares()
{
    for (short int origin_square : getBitIndices(m_black_pawns_bit))
    {
        m_squares_attacked_by_black_pawns |= precomputed_moves::black_pawn_attacks[origin_square];
    }
}
void BitPosition::setWhiteKingAttackedSquares()
{
    m_squares_attacked_by_white_king = precomputed_moves::king_moves[m_white_king_position];
}
void BitPosition::setBlackKingAttackedSquares()
{
    m_squares_attacked_by_black_king = precomputed_moves::king_moves[m_black_king_position];
}
void BitPosition::setPinsBits()
// Set bitboard of the pinned squares, these are squares containg pinned piece and the ray. We set three bitboards, one for straight pins, one for diagonal pins and
// another for all pins. This makes generating legal moves easier, since the pinned sliders can move diagonally or straight depending if the pin is diagonal or straight.
// Assuming pins are set to 0 after making move.
// This will be called in case we are not in check. Else we call setChecksAndPinsBits.
{
    if (m_turn) // White's turn
    {
        // Pins by black bishops
        for (short int square : getBitIndices(m_black_bishops_bit & precomputed_moves::bishop_full_rays[m_white_king_position]))
        // For each square corresponding to black bishop raying white king
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(bishop_ray & m_white_pieces_bit) && hasOneOne(bishop_ray & m_black_pieces_bit))
            {
                m_diagonal_pins |= bishop_ray;
            }
        }
        // Pins by black rooks
        for (short int square : getBitIndices(m_black_rooks_bit & precomputed_moves::rook_full_rays[m_white_king_position]))
        // For each square corresponding to black rook raying white king
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(rook_ray & m_white_pieces_bit) && hasOneOne(rook_ray & m_black_pieces_bit))
            {
                m_straight_pins |= rook_ray;
            }
        }
        // Pins by black queens
        for (short int square : getBitIndices(m_black_queens_bit & precomputed_moves::bishop_full_rays[m_white_king_position]))
        // For each square corresponding to black queen raying white king diagonally
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(bishop_ray & m_white_pieces_bit) && hasOneOne(bishop_ray & m_black_pieces_bit))
            {
                m_diagonal_pins |= bishop_ray;
            }
        }
        for (short int square : getBitIndices(m_black_queens_bit & precomputed_moves::rook_full_rays[m_white_king_position]))
        // For each square corresponding to black queen raying white king straight
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(rook_ray & m_white_pieces_bit) && hasOneOne(rook_ray & m_black_pieces_bit))
            {
                m_straight_pins |= rook_ray;
            }
        }
    }
    else // Black's turn
    {
        // Pins by white bishops
        for (short int square : getBitIndices(m_white_bishops_bit & precomputed_moves::bishop_full_rays[m_black_king_position]))
        // For each square corresponding to white bishop raying white king
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(bishop_ray & m_white_pieces_bit) && hasOneOne(bishop_ray & m_black_pieces_bit))
            {
                m_diagonal_pins |= bishop_ray;
            }
        }
        // Pins by white rooks
        for (short int square : getBitIndices(m_white_rooks_bit & precomputed_moves::rook_full_rays[m_black_king_position]))
        // For each square corresponding to white rook raying white king
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(rook_ray & m_white_pieces_bit) && hasOneOne(rook_ray & m_black_pieces_bit))
            {
                m_straight_pins |= rook_ray;
            }
        }
        // Pins by white queens
        for (short int square : getBitIndices(m_white_queens_bit & precomputed_moves::bishop_full_rays[m_black_king_position]))
        // For each square corresponding to white queen raying white king diagonally
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(bishop_ray & m_white_pieces_bit) && hasOneOne(bishop_ray & m_black_pieces_bit))
            {
                m_diagonal_pins |= bishop_ray;
            }
        }
        for (short int square : getBitIndices(m_white_queens_bit & precomputed_moves::rook_full_rays[m_black_king_position]))
        // For each square corresponding to white queen raying white king straight
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(rook_ray & m_white_pieces_bit) && hasOneOne(rook_ray & m_black_pieces_bit))
            {
                m_straight_pins |= rook_ray;
            }
        }
    }
    m_all_pins = m_straight_pins | m_diagonal_pins;
}
void BitPosition::setChecksAndPinsBits()
// Set a bitboard of the pinned squares, these are squares containg pinned piece and the ray. We set three bitboards, one for straight pins and one for
// diagonal pins and another for all pins. This makes generating legal moves easier, since the pinned sliders can move diagonally or straight depending if the pin is diagonal or straight.
// Assuming pins, checks, check_rays and num_checks are all set to 0 after making move.
// This will be called in case we are in check. Else we call setPinsBits.
{
    if (m_turn) // White's turn
    {
        // Black pawns (Note we can only give check with one pawn at a time)
        {
            uint64_t attacking_squares{precomputed_moves::white_pawn_attacks[m_white_king_position]};
            // Maybe use reference or pointer instead of creating new variable attacking_squares??
            if ((attacking_squares & m_black_pawns_bit) != 0)
            {
                m_pawn_checks |= (attacking_squares & m_black_pawns_bit);
                ++m_num_checks;
            }
        }
        // We use blocks so that attacking_squares variables dont conflict between each
        {
            // Black knights (Note we can only give check with one knight at a time)
            uint64_t attacking_squares{precomputed_moves::knight_moves[m_white_king_position]};
            if ((attacking_squares & m_black_knights_bit) != 0)
            {
                m_knight_checks |= (attacking_squares & m_black_knights_bit);
                ++m_num_checks;
            }
        }
        // Black bishops
        for (short int square : getBitIndices(m_black_bishops_bit & precomputed_moves::bishop_full_rays[m_white_king_position]))
        {
            // Ray from square where bishop is to king, including opponents bishop square, excluding king square
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(bishop_ray & m_all_pieces_bit)) // Check
            {
                m_bishop_checks |= (bishop_ray & m_black_bishops_bit);
                m_check_rays = (bishop_ray & ~m_black_bishops_bit);
                ++m_num_checks;
            }
            else if (hasOneOne(bishop_ray & m_white_pieces_bit) && hasOneOne(bishop_ray & m_black_pieces_bit)) // Pin
                m_diagonal_pins |= bishop_ray;
        }
        // Black rooks
        for (short int square : getBitIndices(m_black_rooks_bit & precomputed_moves::rook_full_rays[m_white_king_position]))
        {
            // Ray from square where rook is to king, including opponents rook square, excluding king square
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(rook_ray & m_all_pieces_bit)) // Check
            {
                m_rook_checks |= (rook_ray & m_black_rooks_bit);
                m_check_rays = (rook_ray & ~m_black_rooks_bit);
                ++m_num_checks;
            }
            else if (hasOneOne(rook_ray & m_white_pieces_bit) && hasOneOne(rook_ray & m_black_pieces_bit)) // Pin
                m_straight_pins |= rook_ray;
        }
        // Black queens
        for (short int square : getBitIndices(m_black_queens_bit & precomputed_moves::bishop_full_rays[m_white_king_position]))
        {
            // Ray from square where queen is to king, including opponents queen square, excluding king square
            uint64_t diagonal_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(diagonal_ray & m_all_pieces_bit)) // Check
            {
                m_queen_checks |= (diagonal_ray & m_black_queens_bit);
                m_check_rays = (diagonal_ray & ~m_black_queens_bit);
                ++m_num_checks;
            }
            else if (hasOneOne(diagonal_ray & m_white_pieces_bit) && hasOneOne(diagonal_ray & m_black_pieces_bit)) // Pin
                m_diagonal_pins |= diagonal_ray;
        }
        for (short int square : getBitIndices(m_black_queens_bit & precomputed_moves::rook_full_rays[m_white_king_position]))
        {
            // Ray from square where queen is to king, including opponents queen square, excluding king square
            uint64_t straight_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(straight_ray & m_all_pieces_bit)) // Check
            {
                m_queen_checks |= (straight_ray & m_black_queens_bit);
                m_check_rays = (straight_ray & ~m_black_queens_bit);
                ++m_num_checks;
            }
            else if (hasOneOne(straight_ray & m_white_pieces_bit) && hasOneOne(straight_ray & m_black_pieces_bit)) // Pin
                m_straight_pins |= straight_ray;
        }
    }
    else // Black's turn
    {
        // White pawns (Note we can only give check with one pawn at a time)
        {
            uint64_t attacking_squares{precomputed_moves::black_pawn_attacks[m_black_king_position]};
            // Maybe use reference or pointer instead of creating new variable attacking_squares??
            if ((attacking_squares & m_white_pawns_bit) != 0)
            {
                m_pawn_checks |= (attacking_squares & m_white_pawns_bit);
                ++m_num_checks;
            }
        }
        // We use blocks so that attacking_squares variables dont conflict between each
        {
            // Black knights (Note we can only give check with one knight at a time)
            uint64_t attacking_squares{precomputed_moves::knight_moves[m_black_king_position]};
            if ((attacking_squares & m_white_knights_bit) != 0)
            {
                m_knight_checks |= (attacking_squares & m_white_knights_bit);
                ++m_num_checks;
            }
        }
        // White bishops
        for (short int square : getBitIndices(m_white_bishops_bit & precomputed_moves::bishop_full_rays[m_black_king_position]))
        {
            // Ray from square where bishop is to king, including opponents bishop square, excluding king square
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(bishop_ray & m_all_pieces_bit)) // Check
            {
                m_bishop_checks |= (bishop_ray & m_white_bishops_bit);
                m_check_rays = (bishop_ray & ~m_white_bishops_bit);
                ++m_num_checks;
            }
            else if (hasOneOne(bishop_ray & m_white_pieces_bit) && hasOneOne(bishop_ray & m_black_pieces_bit)) // Pin
                m_diagonal_pins |= bishop_ray;
        }
        // White rooks
        for (short int square : getBitIndices(m_white_rooks_bit & precomputed_moves::rook_full_rays[m_black_king_position]))
        {
            // Ray from square where rook is to king, including opponents rook square, excluding king square
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(rook_ray & m_all_pieces_bit)) // Check
            {
                m_rook_checks |= (rook_ray & m_white_rooks_bit);
                m_check_rays = (rook_ray & ~m_white_rooks_bit);
                ++m_num_checks;
            }
            else if (hasOneOne(rook_ray & m_white_pieces_bit) && hasOneOne(rook_ray & m_black_pieces_bit)) // Pin
                m_straight_pins |= rook_ray;
        }
        // White queens
        for (short int square : getBitIndices(m_white_queens_bit & precomputed_moves::bishop_full_rays[m_black_king_position]))
        {
            // Ray from square where queen is to king, including opponents queen square, excluding king square
            uint64_t diagonal_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(diagonal_ray & m_all_pieces_bit)) // Check
            {
                m_queen_checks |= (diagonal_ray & m_white_queens_bit);
                m_check_rays = (diagonal_ray & ~m_white_queens_bit);
                ++m_num_checks;
            }
            else if (hasOneOne(diagonal_ray & m_white_pieces_bit) && hasOneOne(diagonal_ray & m_black_pieces_bit)) // Pin
                m_diagonal_pins |= diagonal_ray;
        }
        for (short int square : getBitIndices(m_white_queens_bit & precomputed_moves::rook_full_rays[m_black_king_position]))
        {
            // Ray from square where queen is to king, including opponents queen square, excluding king square
            uint64_t straight_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(straight_ray & m_all_pieces_bit)) // Check
            {
                m_queen_checks |= (straight_ray & m_white_queens_bit);
                m_check_rays = (straight_ray & ~m_white_queens_bit);
                ++m_num_checks;
            }
            else if (hasOneOne(straight_ray & m_white_pieces_bit) && hasOneOne(straight_ray & m_black_pieces_bit)) // Pin
                m_straight_pins |= straight_ray;
        }
    }
    m_all_pins = m_straight_pins | m_diagonal_pins;
}

bool BitPosition::kingIsSafeAfterPassant(unsigned short removed_square_1, unsigned short removed_square_2) const // we can make it const because it doesn't change the object
// See if the king is in check or not (from kings position). For when moving the king.
{
    if (m_turn) // White's turn
    {
        // Black bishops and queens
        if ((precomputed_moves::precomputedBishopMovesTable[m_white_king_position][precomputed_moves::bishop_unfull_rays[m_white_king_position] & (m_all_pieces_bit & ~(precomputed_moves::one_one_bits[removed_square_1] | precomputed_moves::one_one_bits[removed_square_2]))] & (m_black_bishops_bit | m_black_queens_bit)) != 0)
            return false;

        // Black rooks and queens
        if ((precomputed_moves::precomputedRookMovesTable[m_white_king_position][precomputed_moves::rook_unfull_rays[m_white_king_position] & (m_all_pieces_bit & ~(precomputed_moves::one_one_bits[removed_square_1] | precomputed_moves::one_one_bits[removed_square_2]))] & (m_black_rooks_bit | m_black_queens_bit)) != 0)
            return false;
    }
    else // Black's turn
    {
        // White bishops and queens
        if ((precomputed_moves::precomputedBishopMovesTable[m_black_king_position][precomputed_moves::bishop_unfull_rays[m_black_king_position] & (m_all_pieces_bit & ~(precomputed_moves::one_one_bits[removed_square_1] | precomputed_moves::one_one_bits[removed_square_2]))] & (m_white_bishops_bit | m_white_queens_bit)) != 0)
            return false;

        // White rooks and queens
        if ((precomputed_moves::precomputedRookMovesTable[m_black_king_position][precomputed_moves::rook_unfull_rays[m_black_king_position] & (m_all_pieces_bit & ~(precomputed_moves::one_one_bits[removed_square_1] | precomputed_moves::one_one_bits[removed_square_2]))] & (m_white_rooks_bit | m_white_queens_bit)) != 0)
            return false;
    }
    return true;
}

std::vector<Move> BitPosition::inCheckMoves() const
// We are going to check for checks more efficiently from the kings position and assuming it can move as any piece.
// This will yield the moves to reduce computational time.
{
    std::vector<Move> moves;
    moves.reserve(50);
    if (m_turn) // White's turn
    {
        if (m_num_checks == 1)
        // We can only capture (with a piece that is not the king) or block if there is only one check
        {
            // Blocking with pawns (non promotions)
            for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & ~m_all_pins & 0x0000FFFFFFFFFFFF))
            {
                if ((precomputed_moves::white_pawn_moves[origin_square] & m_all_pieces_bit) == 0)
                // We can move the pawn up
                {
                    for (unsigned short destination_square : getBitIndices(precomputed_moves::white_pawn_moves[origin_square] & m_check_rays))
                        moves.emplace_back(origin_square, destination_square, 0);
                    if ((origin_square < 16) && (precomputed_moves::white_pawn_doubles[origin_square] & m_check_rays) != 0)
                        moves.push_back(double_moves[origin_square]);
                }
            }
            // Blocking with knights
            for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square, 1);
                }
            }
            // Blocking with rooks
            for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square, 3);
                }
            }
            // Blocking with bishops
            for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square, 2);
                }
            }
            // Blocking with queens
            for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices((precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] | precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit]) & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square, 4);
                }
            }
        }
        // Move king
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~(m_all_pieces_bit | m_check_rays) & ~m_squares_attacked_by_black_pieces))
            moves.emplace_back(m_white_king_position, destination_square, 5);
    }
    else // Black's turn
    {
        if (m_num_checks == 1)
        // We can only capture (with a piece that is not the king) or block if there is only one check
        {
            // Blocking with pawns (non promotions)
            for (unsigned short origin_square : getBitIndices(m_black_pawns_bit & ~m_all_pins & 0xFFFFFFFFFFFF0000))
            {
                if ((precomputed_moves::black_pawn_moves[origin_square] & m_all_pieces_bit) == 0)
                // We can move the pawn up
                {
                    for (unsigned short destination_square : getBitIndices(precomputed_moves::black_pawn_moves[origin_square] & m_check_rays))
                        // One up
                        moves.emplace_back(origin_square, destination_square, 0);
                    if ((origin_square > 47) && (precomputed_moves::black_pawn_doubles[origin_square] & m_check_rays) != 0)
                        moves.push_back(double_moves[origin_square - 48]);
                }
            }
            // Blocking with knights
            for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & m_check_rays))
                    moves.emplace_back(origin_square, destination_square, 1);
            }
            // Blocking with rooks
            for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & m_check_rays))
                    moves.emplace_back(origin_square, destination_square, 3);
            }
            // Blocking with bishops
            for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & m_check_rays))
                    moves.emplace_back(origin_square, destination_square, 2);
            }
            // Blocking with queens
            for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices((precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] | precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit]) & m_check_rays))
                    moves.emplace_back(origin_square, destination_square, 4);
            }
        }
        // Move king
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~(m_all_pieces_bit | m_check_rays) & ~m_squares_attacked_by_white_pieces))
            moves.emplace_back(m_black_king_position, destination_square, 5);
    }
    return moves;
}
std::vector<Move> BitPosition::nonCaptureMoves() const
// This will be a generator which when checking if move is a check it yields a move with score 1, else 0.
{
    std::vector<Move> moves;
    moves.reserve(128);
    if (m_turn) // White's turn
    {
        // Moving pieces that are not pinned safely (pawns can be moved unsafely)

        // Knights
        for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & ~m_all_pieces_bit & ~m_squares_attacked_by_black_pieces))
                moves.emplace_back(origin_square, destination_square, 1, true);
        }
        // Bishops
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & ~m_squares_attacked_by_black_pieces))
                moves.emplace_back(origin_square, destination_square, 2, true);
        }
        // Rooks
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & ~m_squares_attacked_by_black_pieces))
                moves.emplace_back(origin_square, destination_square, 3, true);
        }
        // Queens
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices((precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] | precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit]) & ~m_all_pieces_bit & ~m_squares_attacked_by_black_pieces))
                moves.emplace_back(origin_square, destination_square, 4, true);
        }
        // Pawns
        for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::white_pawn_moves[origin_square] & ~m_all_pieces_bit))
            {
                if (origin_square < 16 && (precomputed_moves::white_pawn_doubles[origin_square] & m_all_pieces_bit) == 0)
                // Can do double pawn moves
                {
                    moves.push_back(double_moves[origin_square]);
                    moves.emplace_back(origin_square, destination_square, 0, true);
                }
                else if (destination_square <= 55)
                    // One up moves only
                    moves.emplace_back(origin_square, destination_square, 0, true);
            }
        }
        // King
        // Kingside castling
        if (m_white_kingside_castling && (((m_all_pieces_bit | m_squares_attacked_by_black_pieces) & 96) == 0))
            moves.push_back(castling_moves[0]);
        // Queenside castling
        if (m_white_queenside_castling && (((m_all_pieces_bit | m_squares_attacked_by_black_pieces) & 14) == 0))
            moves.push_back(castling_moves[1]);
        // Normal king moves
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit & ~m_squares_attacked_by_black_pieces))
            moves.emplace_back(m_white_king_position, destination_square, 5, true);

        // Moving pinned pieces (Note knights cannot be moved if pinned and kings cannot be pinned)
        // These are considered unsafe moves
        // Pinned Bishops
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & m_diagonal_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square, 2, false);
        }
        // Pinned Rooks
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_straight_pins))
                moves.emplace_back(origin_square, destination_square, 3, false);
        }
        // Pinned Queens
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square, 4, false);
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_straight_pins))
                moves.emplace_back(origin_square, destination_square, 4, false);
        }
        // Pinned Pawns
        for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::white_pawn_moves[origin_square] & ~m_all_pieces_bit & m_straight_pins))
            {
                moves.emplace_back(origin_square, destination_square, 0, false);
                if (origin_square < 16 && (precomputed_moves::white_pawn_doubles[origin_square] & m_all_pieces_bit) == 0)
                    moves.push_back(double_moves[origin_square]);
            }
        }

        // Other unsafe moves
        // Knights
        for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & ~m_all_pieces_bit & m_squares_attacked_by_black_pieces))
                moves.emplace_back(origin_square, destination_square, 1, false);
        }
        // Bishops
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_squares_attacked_by_black_pieces))
                moves.emplace_back(origin_square, destination_square, 2, false);
        }
        // Rooks
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_squares_attacked_by_black_pieces))
                moves.emplace_back(origin_square, destination_square, 3, false);
        }
        // Queens
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices((precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] | precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit]) & ~m_all_pieces_bit & m_squares_attacked_by_black_pieces))
                moves.emplace_back(origin_square, destination_square, 4, false);
        }
    }
    else // Black's turn
    {
        // Moving pieces that are not pinned safely (pawns can be moved unsafely)

        // Knights
        for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & ~m_all_pieces_bit & ~m_squares_attacked_by_white_pieces))
                moves.emplace_back(origin_square, destination_square, 1, true);
        }
        // Bishops
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & ~m_squares_attacked_by_white_pieces))
                moves.emplace_back(origin_square, destination_square, 2, true);
        }
        // Rooks
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & ~m_squares_attacked_by_white_pieces))
                moves.emplace_back(origin_square, destination_square, 3, true);
        }
        // Queens
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices((precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] | precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit]) & ~m_all_pieces_bit & ~m_squares_attacked_by_white_pieces))
                moves.emplace_back(origin_square, destination_square, 4, true);
        }
        // Pawns
        for (unsigned short origin_square : getBitIndices(m_black_pawns_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::black_pawn_moves[origin_square] & ~m_all_pieces_bit))
            {
                if (origin_square > 47 && (precomputed_moves::black_pawn_doubles[origin_square] & m_all_pieces_bit) == 0)
                // Can do double pawn moves
                {
                    moves.push_back(double_moves[origin_square]);
                    moves.emplace_back(origin_square, destination_square, 0, true);
                }
                else if (destination_square > 7) // Non promotions
                    // One up moves only
                    moves.emplace_back(origin_square, destination_square, 0, true);
            }
        }
        // King
        // Kingside castling
        if (m_black_kingside_castling && (((m_all_pieces_bit | m_squares_attacked_by_black_pieces) & 6917529027641081856) == 0))
            moves.push_back(castling_moves[2]);
        // Queenside castling
        if (m_black_queenside_castling && (((m_all_pieces_bit | m_squares_attacked_by_black_pieces) & 1008806316530991104) == 0))
            moves.push_back(castling_moves[3]);
        // Normal king moves
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit & ~m_squares_attacked_by_white_pieces))
            moves.emplace_back(m_black_king_position, destination_square, 5, true);

        // Moving pinned pieces (Note knights cannot be moved if pinned and kings cannot be pinned)
        // These are considered unsafe moves
        // Pinned Bishops
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & m_diagonal_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square, 2, false);
        }
        // Pinned Rooks
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_straight_pins))
                moves.emplace_back(origin_square, destination_square, 3, false);
        }
        // Pinned Queens
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square, 4, false);
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_straight_pins))
                moves.emplace_back(origin_square, destination_square, 4, false);
        }
        // Pinned Pawns
        for (unsigned short origin_square : getBitIndices(m_black_pawns_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::black_pawn_moves[origin_square] & ~m_all_pieces_bit & m_straight_pins))
            {
                moves.emplace_back(origin_square, destination_square, 0, false);
                if (origin_square > 47 && (precomputed_moves::black_pawn_doubles[origin_square] & m_all_pieces_bit) == 0)
                    moves.push_back(double_moves[origin_square]);
            }
        }

        // Other unsafe moves
        // Knights
        for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & ~m_all_pieces_bit & m_squares_attacked_by_white_pieces))
                moves.emplace_back(origin_square, destination_square, 1, false);
        }
        // Bishops
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_squares_attacked_by_white_pieces))
                moves.emplace_back(origin_square, destination_square, 2, false);
        }
        // Rooks
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & ~m_all_pieces_bit & m_squares_attacked_by_white_pieces))
                moves.emplace_back(origin_square, destination_square, 3, false);
        }
        // Queens
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices((precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] | precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit]) & ~m_all_pieces_bit & m_squares_attacked_by_white_pieces))
                moves.emplace_back(origin_square, destination_square, 4, false);
        }
    }
    return moves;
}

std::vector<Capture> BitPosition::inCheckCaptures() const
// All captures made in check are set to good captures.
{
    std::vector<Capture> captures;
    captures.reserve(20);
    std::array<uint64_t, 5> checks_array{m_pawn_checks, m_knight_checks, m_bishop_checks, m_rook_checks, m_queen_checks};
    if (m_turn) // White's turn
    {
        std::array<uint64_t, 5> pieces_array{m_black_pawns_bit, m_black_knights_bit, m_black_bishops_bit, m_black_rooks_bit, m_black_queens_bit};
        // Capturing with king
        for (unsigned short i = 0; i <= 4; ++i)
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & pieces_array[i] & ~m_squares_attacked_by_black_pieces))
                captures.emplace_back(m_white_king_position, destination_square, 2);
        }
        if (m_num_checks == 1)
        // We can only capture (with a piece that is not the king) or block if there is only one check
        {
            // Capturing/blocking with pawns
            for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    if ((precomputed_moves::white_pawn_attacks[origin_square] & checks_array[i]) != 0)
                    {
                        uint64_t moveable_squares{precomputed_moves::white_pawn_attacks[origin_square] & checks_array[i]};
                        if (moveable_squares != 0)
                        {
                            unsigned short destination_square{getLeastSignificantBitIndex(moveable_squares)};
                            if (destination_square > 55)
                            // Promotions
                            {
                                captures.emplace_back(origin_square, destination_square, 1, 0);
                                captures.emplace_back(origin_square, destination_square, 1, 1);
                                captures.emplace_back(origin_square, destination_square, 1, 2);
                                captures.emplace_back(origin_square, destination_square, 1, 3);
                            }
                            else
                                captures.emplace_back(origin_square, destination_square, 2);
                        }
                    }
                }
                // Passant capture
                if ((precomputed_moves::white_pawn_attacks[origin_square] & passant_bitboards[m_psquare]) != 0 && kingIsSafeAfterPassant(origin_square, m_psquare - 8))
                    captures.emplace_back(origin_square, m_psquare, 2);
            }
            // Capturing/blocking with knights
            for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{precomputed_moves::knight_moves[origin_square] & checks_array[i]};
                    if (moveable_squares != 0)
                        captures.emplace_back(origin_square, getLeastSignificantBitIndex(moveable_squares), 2);
                }
            }
            // Capturing/blocking with bishops
            for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & checks_array[i]};
                    if (moveable_squares != 0)
                        captures.emplace_back(origin_square, getLeastSignificantBitIndex(moveable_squares), 2);
                }
            }
            // Capturing/blocking with rooks
            for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & checks_array[i]};
                    if (moveable_squares != 0)
                        captures.emplace_back(origin_square, getLeastSignificantBitIndex(moveable_squares), 2);
                }
            }
            // Capturing/blocking with queens
            for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] | precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit]) & checks_array[i]};
                    if (moveable_squares != 0)
                        captures.emplace_back(origin_square, getLeastSignificantBitIndex(moveable_squares), 2);
                }
            }
        }
    }
    else // Blacks turn
    {
        std::array<uint64_t, 5> pieces_array{m_white_pawns_bit, m_white_knights_bit, m_white_bishops_bit, m_white_rooks_bit, m_white_queens_bit};
        // Capturing with king
        for (unsigned short i = 0; i <= 4; ++i)
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & pieces_array[i] & ~m_squares_attacked_by_white_pieces))
                captures.emplace_back(m_black_king_position, destination_square, 2);
        }
        if (m_num_checks == 1)
        // We can only capture (with a piece that is not the king) or block if there is only one check
        {
            // Capturing/blocking with pawns
            for (unsigned short origin_square : getBitIndices(m_black_pawns_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    if ((precomputed_moves::black_pawn_attacks[origin_square] & checks_array[i]) != 0)
                    {
                        uint64_t moveable_squares{precomputed_moves::black_pawn_attacks[origin_square] & checks_array[i]};
                        if (moveable_squares != 0)
                        {
                            unsigned short destination_square{getLeastSignificantBitIndex(moveable_squares)};
                            if (destination_square < 8)
                            // Promotions
                            {
                                captures.emplace_back(origin_square, destination_square, 1, 0);
                                captures.emplace_back(origin_square, destination_square, 1, 1);
                                captures.emplace_back(origin_square, destination_square, 1, 2);
                                captures.emplace_back(origin_square, destination_square, 1, 3);
                            }
                            else
                                captures.emplace_back(origin_square, destination_square, 2);
                        }
                    }
                }
                // Passant capture
                if ((precomputed_moves::black_pawn_attacks[origin_square] & passant_bitboards[m_psquare]) != 0 && kingIsSafeAfterPassant(origin_square, m_psquare + 8))
                    captures.emplace_back(origin_square, m_psquare, 2);
            }
            // Capturing/blocking with knights
            for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{precomputed_moves::knight_moves[origin_square] & checks_array[i]};
                    if (moveable_squares != 0)
                        captures.emplace_back(origin_square, getLeastSignificantBitIndex(moveable_squares), 2);
                }
            }
            // Capturing/blocking with bishops
            for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & checks_array[i]};
                    if (moveable_squares != 0)
                        captures.emplace_back(origin_square, getLeastSignificantBitIndex(moveable_squares), 2);
                }
            }
            // Capturing/blocking with rooks
            for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & checks_array[i]};
                    if (moveable_squares != 0)
                        captures.emplace_back(origin_square, getLeastSignificantBitIndex(moveable_squares), 2);
                }
            }
            // Capturing/blocking with queens
            for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] | precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit]) & checks_array[i]};
                    if (moveable_squares != 0)
                        captures.emplace_back(origin_square, getLeastSignificantBitIndex(moveable_squares), 2);
                }
            }
        }
    }
    return captures;
}

std::vector<Capture> BitPosition::captureMoves() const
// This set of moves will always be computed before non_capture_moves.
// They are generated in order so that they dont have to be ordered later (CHECK ORDER IS NOT REVERSED).
{
    std::vector<Capture> captures;
    captures.reserve(50);
    if (m_turn) // White's turn
    {
        // Good captures are captures of a better or equal piece.
        // Good Pawn captures (non promotions) and passant captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_pawns & ((m_black_pieces_bit & 72057594037927935) | passant_bitboards[m_psquare])))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::black_pawn_attacks[destination_square] & m_white_pawns_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good Knight captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_knights & m_black_pieces_bit & ~m_black_pawns_bit))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::knight_moves[destination_square] & m_white_knights_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good King captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_king & m_black_pieces_bit & ~m_squares_attacked_by_black_pieces))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::king_moves[destination_square]))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good Unpinned Bishop captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_bishops & m_black_pieces_bit & ~m_black_pawns_bit))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[destination_square][precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit] & m_white_bishops_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good Unpinned Rook captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_rooks & (m_black_rooks_bit | m_black_queens_bit)))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[destination_square][precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit] & m_white_rooks_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good Unpinned Queen captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_queens & m_black_queens_bit))
        {
            for (unsigned short origin_square : getBitIndices((precomputed_moves::precomputedRookMovesTable[destination_square][precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit] | precomputed_moves::precomputedBishopMovesTable[destination_square][precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit]) & m_white_queens_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Unpinned Pawn promotions and captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_pawns & m_black_pieces_bit & 18374686479671623680))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::black_pawn_attacks[destination_square] & m_white_pawns_bit & ~m_all_pins))
            {
                captures.emplace_back(origin_square, destination_square, 1, 0);
                captures.emplace_back(origin_square, destination_square, 1, 1);
                captures.emplace_back(origin_square, destination_square, 1, 2);
                captures.emplace_back(origin_square, destination_square, 1, 3);
            }
        }
        // Unpinned Pawn promotions non-captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_pawns & ~m_black_pieces_bit & 18374686479671623680))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::black_pawn_attacks[destination_square] & m_white_pawns_bit & ~m_all_pins))
            {
                captures.emplace_back(origin_square, destination_square, 3, 0);
                captures.emplace_back(origin_square, destination_square, 3, 1);
                captures.emplace_back(origin_square, destination_square, 3, 2);
                captures.emplace_back(origin_square, destination_square, 3, 3);
            }
        }

        // Pinned captures (set as good captures)
        // Pinned bishop captures
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & m_diagonal_pins))
        {
            unsigned short destination_square{getLeastSignificantBitIndex(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & m_diagonal_pins)};
            if (destination_square != 0)
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Pinned rook captures
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & m_straight_pins))
        {
            unsigned short destination_square{getLeastSignificantBitIndex(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & m_straight_pins)};
            if (destination_square != 0)
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Pinned queen captures
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & m_diagonal_pins))
        {
            unsigned short destination_square{getLeastSignificantBitIndex(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & m_diagonal_pins)};
            if (destination_square != 0)
                captures.emplace_back(origin_square, destination_square, 2);
        }
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & m_straight_pins))
        {
            unsigned short destination_square{getLeastSignificantBitIndex(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & m_straight_pins)};
            if (destination_square != 0)
                captures.emplace_back(origin_square, destination_square, 2);
        }

        // Bad Knight captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_knights & m_black_pawns_bit))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::knight_moves[destination_square] & m_white_knights_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 0);
        }
        // Bad Unpinned Bishop captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_bishops & m_black_pawns_bit))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[destination_square][precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit] & m_white_bishops_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 0);
        }
        // Bad Unpinned Rook captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_rooks & (m_black_pawns_bit | m_black_bishops_bit | m_black_knights_bit)))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[destination_square][precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit] & m_white_rooks_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 0);
        }
        // Bad Unpinned Queen captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_white_queens & m_black_pieces_bit & ~m_black_queens_bit))
        {
            for (unsigned short origin_square : getBitIndices((precomputed_moves::precomputedRookMovesTable[destination_square][precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit] | precomputed_moves::precomputedBishopMovesTable[destination_square][precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit]) & m_white_queens_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 0);
        }
    }
    else // Black's turn
    {
        // Good captures are captures of a better or equal piece.
        // Good Pawn captures (non promotions), including passant captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_pawns & ((m_white_pieces_bit & 18446744073709551360) | passant_bitboards[m_psquare])))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::white_pawn_attacks[destination_square] & m_black_pawns_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good Knight captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_knights & m_white_pieces_bit & ~m_white_pawns_bit))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::knight_moves[destination_square] & m_black_knights_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good King captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_king & m_white_pieces_bit & ~m_squares_attacked_by_white_pieces))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::king_moves[destination_square]))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good Bishop captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_bishops & m_white_pieces_bit & ~m_white_pawns_bit))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[destination_square][precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit] & m_black_bishops_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good Rook captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_rooks & (m_white_rooks_bit | m_white_queens_bit)))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[destination_square][precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit] & m_black_rooks_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Good Queen captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_queens & m_white_queens_bit))
        {
            for (unsigned short origin_square : getBitIndices((precomputed_moves::precomputedRookMovesTable[destination_square][precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit] | precomputed_moves::precomputedBishopMovesTable[destination_square][precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit]) & m_black_queens_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Pawn promotions and captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_pawns & m_white_pieces_bit & 255))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::white_pawn_attacks[destination_square] & m_black_pawns_bit & ~m_all_pins))
            {
                captures.emplace_back(origin_square, destination_square, 1, 0);
                captures.emplace_back(origin_square, destination_square, 1, 1);
                captures.emplace_back(origin_square, destination_square, 1, 2);
                captures.emplace_back(origin_square, destination_square, 1, 3);
            }
        }
        // Pawn promotions non-captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_pawns & ~m_white_pieces_bit & 255))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::white_pawn_attacks[destination_square] & m_black_pawns_bit & ~m_all_pins))
            {
                captures.emplace_back(origin_square, destination_square, 3, 0);
                captures.emplace_back(origin_square, destination_square, 3, 1);
                captures.emplace_back(origin_square, destination_square, 3, 2);
                captures.emplace_back(origin_square, destination_square, 3, 3);
            }
        }
        // Pinned captures (set as good captures)
        // Pinned bishop captures
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & m_diagonal_pins))
        {
            unsigned short destination_square{getLeastSignificantBitIndex(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & m_diagonal_pins)};
            if (destination_square != 0)
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Pinned rook captures
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & m_straight_pins))
        {
            unsigned short destination_square{getLeastSignificantBitIndex(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & m_straight_pins)};
            if (destination_square != 0)
                captures.emplace_back(origin_square, destination_square, 2);
        }
        // Pinned queen captures
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & m_diagonal_pins))
        {
            unsigned short destination_square{getLeastSignificantBitIndex(precomputed_moves::precomputedBishopMovesTable[origin_square][precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit] & m_diagonal_pins)};
            if (destination_square != 0)
                captures.emplace_back(origin_square, destination_square, 2);
        }
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & m_straight_pins))
        {
            unsigned short destination_square{getLeastSignificantBitIndex(precomputed_moves::precomputedRookMovesTable[origin_square][precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit] & m_straight_pins)};
            if (destination_square != 0)
                captures.emplace_back(origin_square, destination_square, 2);
        }

        // Bad Knight captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_knights & m_white_pawns_bit))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::knight_moves[destination_square] & m_black_knights_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 0);
        }
        // Bad Bishop captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_bishops & m_white_pawns_bit))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::precomputedBishopMovesTable[destination_square][precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit] & m_black_bishops_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 0);
        }
        // Bad Rook captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_rooks & (m_white_pawns_bit | m_white_bishops_bit | m_white_knights_bit)))
        {
            for (unsigned short origin_square : getBitIndices(precomputed_moves::precomputedRookMovesTable[destination_square][precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit] & m_black_rooks_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 0);
        }
        // Bad Queen captures
        for (unsigned short destination_square : getBitIndices(m_squares_attacked_by_black_queens & m_white_pieces_bit & ~m_white_queens_bit))
        {
            for (unsigned short origin_square : getBitIndices((precomputed_moves::precomputedRookMovesTable[destination_square][precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit] | precomputed_moves::precomputedBishopMovesTable[destination_square][precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit]) & m_black_queens_bit & ~m_all_pins))
                captures.emplace_back(origin_square, destination_square, 0);
        }
    }
    return captures;
}

void BitPosition::setPiece(uint64_t origin_bit, uint64_t destination_bit)
// Move a piece, given an origin bit and destination bit
{
    if (m_turn)
    {
        // array of pointers to our pieces
        std::array<uint64_t *, 6> piece_arrays{&m_white_pawns_bit, &m_white_knights_bit, &m_white_bishops_bit, &m_white_rooks_bit, &m_white_queens_bit, &m_white_king_bit};

        for (auto &piece_bit : piece_arrays)
        {
            if ((origin_bit & *piece_bit) != 0)
            {
                *piece_bit &= ~origin_bit;
                *piece_bit |= destination_bit;
                break;
            }
        }
    }
    else
    {
        // array of pointers to our pieces
        std::array<uint64_t *, 6> piece_arrays{&m_black_pawns_bit, &m_black_knights_bit, &m_black_bishops_bit, &m_black_rooks_bit, &m_black_queens_bit, &m_black_king_bit};

        for (auto &piece_bit : piece_arrays)
        {
            if ((origin_bit & *piece_bit) != 0)
            {
                *piece_bit &= ~origin_bit;
                *piece_bit |= destination_bit;
                break;
            }
        }
    }
}

void BitPosition::removePiece(uint64_t destination_bit)
// Remove a piece, given a destination bit
{
    if (m_turn)
    {
        // array of pointers to opponent pieces
        std::array<uint64_t *, 5> piece_arrays{&m_black_pawns_bit, &m_black_knights_bit, &m_black_bishops_bit, &m_black_rooks_bit, &m_black_queens_bit};

        for (auto &piece_bit : piece_arrays)
        {
            if ((destination_bit & *piece_bit) != 0)
            {
                *piece_bit &= ~destination_bit;
                break;
            }
        }
    }
    else
    {
        // array of pointers to opponent pieces
        std::array<uint64_t *, 5> piece_arrays{&m_white_pawns_bit, &m_white_knights_bit, &m_white_bishops_bit, &m_white_rooks_bit, &m_white_queens_bit};

        for (auto &piece_bit : piece_arrays)
        {
            if ((destination_bit & *piece_bit) != 0)
            {
                *piece_bit &= ~destination_bit;
                break;
            }
        }
    }
}

void BitPosition::storePlyInfo()
// Add info on the following arrays:
// wkcastling
// wqcastling
// bkcastling
// bqcastling
// psquare
// pin
// pieces bits
{
    m_wkcastling_array[m_ply] = m_white_kingside_castling;
    m_wqcastling_array[m_ply] = m_white_queenside_castling;
    m_bkcastling_array[m_ply] = m_black_kingside_castling;
    m_bqcastling_array[m_ply] = m_black_queenside_castling;
    m_psquare_array[m_ply] = m_psquare;
    m_diagonal_pins_array[m_ply] = m_diagonal_pins;
    m_straight_pins_array[m_ply] = m_straight_pins;

    m_white_pawns_bits_array[m_ply] = m_white_pawns_bit;
    m_white_knights_bits_array[m_ply] = m_white_knights_bit;
    m_white_bishops_bits_array[m_ply] = m_white_bishops_bit;
    m_white_rooks_bits_array[m_ply] = m_white_rooks_bit;
    m_white_queens_bits_array[m_ply] = m_white_queens_bit;
    m_white_king_bits_array[m_ply] = m_white_king_bit;
    m_black_pawns_bits_array[m_ply] = m_black_pawns_bit;
    m_black_knights_bits_array[m_ply] = m_black_knights_bit;
    m_black_bishops_bits_array[m_ply] = m_black_bishops_bit;
    m_black_rooks_bits_array[m_ply] = m_black_rooks_bit;
    m_black_queens_bits_array[m_ply] = m_black_queens_bit;
    m_black_king_bits_array[m_ply] = m_black_king_bit;
}
void BitPosition::makeCapture(Capture move)
// Move piece and switch white and black roles, without rotating the board.
// After making move, set pins and checks to 0.
{
    BitPosition::storePlyInfo(); // store current state for unmake move

    unsigned short origin_square{move.getOriginSquare()};
    uint64_t origin_bit{precomputed_moves::one_one_bits[origin_square]};
    unsigned short destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{precomputed_moves::one_one_bits[destination_square]};

    if (m_turn) // White's move
    {
        if (origin_bit == m_white_king_bit) // If we move king
        {
            m_white_kingside_castling = false;
            m_white_queenside_castling = false;
        }
        if (origin_square == 7) // If we move rook
            m_white_kingside_castling = false;

        if (origin_square == 0) // If we move rook
            m_white_queenside_castling = false;

        if (destination_square == 63) // If we capture rook
            m_black_kingside_castling = false;

        if (destination_square == 56) // If we capture rook
            m_black_queenside_castling = false;

        BitPosition::removePiece(destination_bit);
        // 1) Normal captures
        if (move.isntPromotion()) // not 0100 0000 0000 0000
        {
            BitPosition::setPiece(origin_bit, destination_bit);
            m_psquare = 0;
        }
        // 2) Promotions
        else // 0100 0000 0000 0000
        {
            m_white_pawns_bit &= ~origin_bit;
            if (move.getPromotionType() == 3) // Queen promotion
                m_white_queens_bit |= destination_bit;
            else if (move.getPromotionType() == 2) // Rook promotion
                m_white_rooks_bit |= destination_bit;
            else if (move.getPromotionType() == 1) // Bishop promotion
                m_white_bishops_bit |= destination_bit;
            else
                m_white_knights_bit |= destination_bit;
        }
    }
    else // Black's move
    {
        if (origin_bit == m_black_king_bit) // If we move king
        {
            m_black_kingside_castling = false;
            m_black_queenside_castling = false;
        }
        if (origin_square == 63) // If we move rook
            m_black_kingside_castling = false;

        if (origin_square == 56) // If we move rook
            m_black_queenside_castling = false;

        if (destination_square == 7) // If we capture rook
            m_white_kingside_castling = false;

        if (destination_square == 0) // If we capture rook
            m_white_queenside_castling = false;

        BitPosition::removePiece(destination_bit);

        // 1) Normal captures
        if (move.isntPromotion()) // not 0100 0000 0000 0000
        {
            BitPosition::setPiece(origin_bit, destination_bit);
            m_psquare = 0;
        }
        // 2) Promotions
        else // 0100 0000 0000 0000
        {
            m_black_pawns_bit &= ~origin_bit;
            if (move.getPromotionType() == 3) // Queen promotion
                m_black_queens_bit |= destination_bit;
            else if (move.getPromotionType() == 2) // Rook promotion
                m_black_rooks_bit |= destination_bit;
            else if (move.getPromotionType() == 1) // Bishop promotion
                m_black_bishops_bit |= destination_bit;
            else // Knight promotion
                m_black_knights_bit |= destination_bit;
        }
    }
    m_turn = not m_turn;
    m_diagonal_pins = 0;
    m_straight_pins = 0;
    m_all_pins = 0;
    m_pawn_checks = 0;
    m_knight_checks = 0;
    m_bishop_checks = 0;
    m_rook_checks = 0;
    m_queen_checks = 0;
    m_check_rays = 0;
    m_num_checks = 0;
    m_ply++;

    BitPosition::setAllPiecesBits();
}
void BitPosition::makeMove(Move move)
// Move piece and switch white and black roles, without rotating the board.
// After making move, set pins and checks to 0.
{
    BitPosition::storePlyInfo(); // store current state for unmake move

    unsigned short origin_square{move.getOriginSquare()};
    uint64_t origin_bit{precomputed_moves::one_one_bits[origin_square]};
    unsigned short destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{precomputed_moves::one_one_bits[destination_square]};

    if (m_turn) // White's move
    {
        if (origin_square == 7) // If we move rook
            m_white_kingside_castling = false;

        if (origin_square == 0) // If we move rook
            m_white_queenside_castling = false;

        if (destination_square == 63) // If we capture rook
            m_black_kingside_castling = false;

        if (destination_square == 56) // If we capture rook
            m_black_queenside_castling = false;

        unsigned short moving_piece{move.movingPiece()};
        // 1) Knight moves
        if (moving_piece == 1)
        {
            m_white_knights_bit &= ~origin_bit;
            m_white_knights_bit |= destination_bit;
            m_psquare = 0;
            BitPosition::setWhiteKnightsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 2) Bishop moves
        else if (moving_piece == 2)
        {
            m_white_bishops_bit &= ~origin_bit;
            m_white_bishops_bit |= destination_bit;
            m_psquare = 0;
            BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 3) Rook moves
        else if (moving_piece == 3)
        {
            m_white_rooks_bit &= ~origin_bit;
            m_white_rooks_bit |= destination_bit;
            m_psquare = 0;
            BitPosition::setWhiteRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 4) Queen moves
        else if (moving_piece == 4)
        {
            m_white_queens_bit &= ~origin_bit;
            m_white_queens_bit |= destination_bit;
            m_psquare = 0;
            BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 5) Pawn moves
        else if (moving_piece == 0)
        {
            m_white_pawns_bit &= ~origin_bit;
            m_white_pawns_bit |= destination_bit;
            if ((destination_square - origin_square) == 16) // Double pawn moves
                m_psquare = origin_square + 8;
            BitPosition::setWhitePawnsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 6) King Moves
        else
        {
            m_white_kingside_castling = false;
            m_white_queenside_castling = false;
            m_white_king_bit = destination_bit;

            m_psquare = 0;
            if (move.getData() == 16772) // White kingside castling
            {
                m_white_king_bit = destination_bit;
                m_white_rooks_bit &= ~128;
                m_white_rooks_bit |= 32;
            }
            else if (move.getData() == 16516) // White queenside castling
            {
                m_white_king_bit = destination_bit;
                m_white_rooks_bit &= ~1;
                m_white_rooks_bit |= 8;
            }
            BitPosition::setWhiteKingAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
    }
    else // Black's move
    {
        if (origin_square == 63) // If we move rook
            m_black_kingside_castling = false;

        if (origin_square == 56) // If we move rook
            m_black_queenside_castling = false;

        if (destination_square == 7) // If we capture rook
            m_white_kingside_castling = false;

        if (destination_square == 0) // If we capture rook
            m_white_queenside_castling = false;

        unsigned short moving_piece{move.movingPiece()};
        // 1) Knight moves
        if (moving_piece == 1)
        {
            m_black_knights_bit &= ~origin_bit;
            m_black_knights_bit |= destination_bit;
            m_psquare = 0;
            BitPosition::setBlackKnightsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 2) Bishop moves
        else if (moving_piece == 2)
        {
            m_black_bishops_bit &= ~origin_bit;
            m_black_bishops_bit |= destination_bit;
            m_psquare = 0;
            BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 3) Rook moves
        else if (moving_piece == 3)
        {
            m_black_rooks_bit &= ~origin_bit;
            m_black_rooks_bit |= destination_bit;
            m_psquare = 0;
            BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 4) Queen moves
        else if (moving_piece == 4)
        {
            m_black_queens_bit &= ~origin_bit;
            m_black_queens_bit |= destination_bit;
            m_psquare = 0;
            BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 5) Pawn moves
        else if (moving_piece == 0)
        {
            m_black_pawns_bit &= ~origin_bit;
            m_black_pawns_bit |= destination_bit;
            if ((origin_square - destination_square) == 16) // Double pawn moves
                m_psquare = destination_square + 8;
            BitPosition::setBlackPawnsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
        // 6) King Moves
        else
        {
            m_black_kingside_castling = false;
            m_black_queenside_castling = false;
            m_black_king_bit = destination_bit;

            m_psquare = 0;
            if (move.getData() == 20412) // Black kingside castling
            {
                m_black_king_bit = destination_bit;
                m_black_rooks_bit &= ~9223372036854775808ULL;
                m_black_rooks_bit |= 2305843009213693952ULL;
            }
            else if (move.getData() == 20156) // Black queenside castling
            {
                m_black_king_bit = destination_bit;
                m_black_rooks_bit &= ~72057594037927936ULL;
                m_black_rooks_bit |= 576460752303423488ULL;
            }
            BitPosition::setBlackKingAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_queens) != 0)
                BitPosition::setBlackQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_queens) != 0)
                BitPosition::setWhiteQueensAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_bishops) != 0)
                BitPosition::setBlackBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_bishops) != 0)
                BitPosition::setWhiteBishopsAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_black_rooks) != 0)
                BitPosition::setBlackRooksAttackedSquares();
            if ((destination_bit & m_squares_attacked_by_white_rooks) != 0)
                BitPosition::setWhiteRooksAttackedSquares();
            m_squares_attacked_by_white_pieces = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_king | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens;
            m_squares_attacked_by_black_pieces = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_king | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens;
        }
    }
    m_turn = not m_turn;
    m_diagonal_pins = 0;
    m_straight_pins = 0;
    m_all_pins = 0;
    m_pawn_checks = 0;
    m_knight_checks = 0;
    m_bishop_checks = 0;
    m_rook_checks = 0;
    m_queen_checks = 0;
    m_check_rays = 0;
    m_num_checks = 0;
    m_ply++;

    BitPosition::setAllPiecesBits();
}
void BitPosition::unmakeMove()
// Takes a move and undoes the move accordingly, updating all position attributes. When the engine transverses the tree of moves it will keep
// track of some irreversible aspects of the game at each ply.These are(white castling rights, black castling rights, passant square,
// moving piece, capture index, pins, checks).
{
    m_ply--;

    m_white_kingside_castling = m_wkcastling_array[m_ply];
    m_white_queenside_castling = m_wqcastling_array[m_ply];
    m_black_kingside_castling = m_bkcastling_array[m_ply];
    m_black_queenside_castling = m_bqcastling_array[m_ply];
    m_psquare = m_psquare_array[m_ply];
    m_diagonal_pins = m_diagonal_pins_array[m_ply];
    m_straight_pins = m_straight_pins_array[m_ply];

    m_white_pawns_bit = m_white_pawns_bits_array[m_ply];
    m_white_knights_bit = m_white_knights_bits_array[m_ply];
    m_white_bishops_bit = m_white_bishops_bits_array[m_ply];
    m_white_rooks_bit = m_white_rooks_bits_array[m_ply];
    m_white_queens_bit = m_white_queens_bits_array[m_ply];
    m_white_king_bit = m_white_king_bits_array[m_ply];
    m_black_pawns_bit = m_black_pawns_bits_array[m_ply];
    m_black_knights_bit = m_black_knights_bits_array[m_ply];
    m_black_bishops_bit = m_black_bishops_bits_array[m_ply];
    m_black_rooks_bit = m_black_rooks_bits_array[m_ply];
    m_black_queens_bit = m_black_queens_bits_array[m_ply];
    m_black_king_bit = m_black_king_bits_array[m_ply];

    m_white_king_position = getLeastSignificantBitIndex(m_white_king_bit);
    m_black_king_position = getLeastSignificantBitIndex(m_black_king_bit);

    BitPosition::setAllPiecesBits();

    m_turn = not m_turn;
}