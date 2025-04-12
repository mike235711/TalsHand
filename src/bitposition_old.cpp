#include <iostream>
#include <vector>
#include <cstdint>             // For fixed sized integers
#include "precomputed_moves.h" // Include the precomputed move constants
#include "bitposition.h"       // Where the BitPosition class is defined
#include "bit_utils.h"         // Bit utility functions
#include "move.h"
#include "magicmoves.h"
#include "zobrist_keys.h"
#include "position_eval.h" // Utility functions to update NNUE Input

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

void BitPosition::initializeZobristKey()
{
    m_zobrist_key = 0;
    // Piece keys
    for (int square : getBitIndices(m_white_pawns_bit))
        m_zobrist_key ^= zobrist_keys::whitePawnZobristNumbers[square];
    for (int square : getBitIndices(m_white_knights_bit))
        m_zobrist_key ^= zobrist_keys::whiteKnightZobristNumbers[square];
    for (int square : getBitIndices(m_white_bishops_bit))
        m_zobrist_key ^= zobrist_keys::whiteBishopZobristNumbers[square];
    for (int square : getBitIndices(m_white_rooks_bit))
        m_zobrist_key ^= zobrist_keys::whiteRookZobristNumbers[square];
    for (int square : getBitIndices(m_white_queens_bit))
        m_zobrist_key ^= zobrist_keys::whiteQueenZobristNumbers[square];
    m_zobrist_key ^= zobrist_keys::whiteKingZobristNumbers[m_white_king_position];

    for (int square : getBitIndices(m_black_pawns_bit))
        m_zobrist_key ^= zobrist_keys::blackPawnZobristNumbers[square];
    for (int square : getBitIndices(m_black_knights_bit))
        m_zobrist_key ^= zobrist_keys::blackKnightZobristNumbers[square];
    for (int square : getBitIndices(m_black_bishops_bit))
        m_zobrist_key ^= zobrist_keys::blackBishopZobristNumbers[square];
    for (int square : getBitIndices(m_black_rooks_bit))
        m_zobrist_key ^= zobrist_keys::blackRookZobristNumbers[square];
    for (int square : getBitIndices(m_black_queens_bit))
        m_zobrist_key ^= zobrist_keys::blackQueenZobristNumbers[square];
    m_zobrist_key ^= zobrist_keys::blackKingZobristNumbers[m_black_king_position];
    // Turn key
    if (not m_turn)
        m_zobrist_key ^= zobrist_keys::blackToMoveZobristNumber;
    // Castling rights key
    int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
    m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
    // Psquare key
    m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
    m_zobrist_keys_array[m_ply] = m_zobrist_key;
}
void BitPosition::updateZobristKeyPiecePartAfterMove(unsigned short origin_square, unsigned short destination_square)
// Since the zobrist hashes are done by XOR on each individual key. And XOR is its own inverse.
// We can efficiently update the hash by XORing the previous key with the key of the piece origin
// square and with the key of the piece destination square.
// Applied inside makeCapture and makeNormalMove.
{
    if (m_turn)
    {
        if (m_moved_piece == 0) // Pawn
            m_zobrist_key ^= zobrist_keys::whitePawnZobristNumbers[origin_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
        else if (m_moved_piece == 1) // Knight
            m_zobrist_key ^= zobrist_keys::whiteKnightZobristNumbers[origin_square] ^ zobrist_keys::whiteKnightZobristNumbers[destination_square];
        else if (m_moved_piece == 2) // Bishop
            m_zobrist_key ^= zobrist_keys::whiteBishopZobristNumbers[origin_square] ^ zobrist_keys::whiteBishopZobristNumbers[destination_square];
        else if (m_moved_piece == 3) // Rook
            m_zobrist_key ^= zobrist_keys::whiteRookZobristNumbers[origin_square] ^ zobrist_keys::whiteRookZobristNumbers[destination_square];
        else if (m_moved_piece == 5) // Queen
            m_zobrist_key ^= zobrist_keys::whiteQueenZobristNumbers[origin_square] ^ zobrist_keys::whiteQueenZobristNumbers[destination_square];
        else // King
            m_zobrist_key ^= zobrist_keys::whiteKingZobristNumbers[origin_square] ^ zobrist_keys::whiteKingZobristNumbers[destination_square];
        if (m_captured_piece != 7)
        {
            if (m_captured_piece == 0) // Pawn
                m_zobrist_key ^= zobrist_keys::blackPawnZobristNumbers[destination_square];
            else if (m_captured_piece == 1) // Knight
                m_zobrist_key ^= zobrist_keys::blackKnightZobristNumbers[destination_square];
            else if (m_captured_piece == 2) // Bishop
                m_zobrist_key ^= zobrist_keys::blackBishopZobristNumbers[destination_square];
            else if (m_captured_piece == 3) // Rook
                m_zobrist_key ^= zobrist_keys::blackRookZobristNumbers[destination_square];
            else // Queen
                m_zobrist_key ^= zobrist_keys::blackQueenZobristNumbers[destination_square];
        }
        if (m_promoted_piece != 7)
        {
            if (m_promoted_piece == 1) // Knight
                m_zobrist_key ^= zobrist_keys::whiteKnightZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else if (m_promoted_piece == 2) // Bishop
                m_zobrist_key ^= zobrist_keys::whiteBishopZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else if (m_promoted_piece == 3) // Rook
                m_zobrist_key ^= zobrist_keys::whiteRookZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else // Queen
                m_zobrist_key ^= zobrist_keys::whiteQueenZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
        }
    }
    else
    {
        if (m_moved_piece == 0) // Pawn
            m_zobrist_key ^= zobrist_keys::blackPawnZobristNumbers[origin_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
        else if (m_moved_piece == 1) // Knight
            m_zobrist_key ^= zobrist_keys::blackKnightZobristNumbers[origin_square] ^ zobrist_keys::blackKnightZobristNumbers[destination_square];
        else if (m_moved_piece == 2) // Bishop
            m_zobrist_key ^= zobrist_keys::blackBishopZobristNumbers[origin_square] ^ zobrist_keys::blackBishopZobristNumbers[destination_square];
        else if (m_moved_piece == 3) // Rook
            m_zobrist_key ^= zobrist_keys::blackRookZobristNumbers[origin_square] ^ zobrist_keys::blackRookZobristNumbers[destination_square];
        else if (m_moved_piece == 5) // Queen
            m_zobrist_key ^= zobrist_keys::blackQueenZobristNumbers[origin_square] ^ zobrist_keys::blackQueenZobristNumbers[destination_square];
        else // King
            m_zobrist_key ^= zobrist_keys::blackKingZobristNumbers[origin_square] ^ zobrist_keys::blackKingZobristNumbers[destination_square];
        if (m_captured_piece != 7)
        {
            if (m_captured_piece == 0) // Pawn
                m_zobrist_key ^= zobrist_keys::whitePawnZobristNumbers[destination_square];
            else if (m_captured_piece == 1) // Knight
                m_zobrist_key ^= zobrist_keys::whiteKnightZobristNumbers[destination_square];
            else if (m_captured_piece == 2) // Bishop
                m_zobrist_key ^= zobrist_keys::whiteBishopZobristNumbers[destination_square];
            else if (m_captured_piece == 3) // Rook
                m_zobrist_key ^= zobrist_keys::whiteRookZobristNumbers[destination_square];
            else // Queen
                m_zobrist_key ^= zobrist_keys::whiteQueenZobristNumbers[destination_square];
        }
        if (m_promoted_piece != 7)
        {
            if (m_promoted_piece == 1) // Knight
                m_zobrist_key ^= zobrist_keys::blackKnightZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else if (m_promoted_piece == 2) // Bishop
                m_zobrist_key ^= zobrist_keys::blackBishopZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else if (m_promoted_piece == 3) // Rook
                m_zobrist_key ^= zobrist_keys::blackRookZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else // Queen
                m_zobrist_key ^= zobrist_keys::blackQueenZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
        }
    }
}
void BitPosition::setWhiteBishopsAttackedSquares()
{
    m_squares_attacked_by_white_bishops = 0;
    for (short int origin_square : getBitIndices(m_white_bishops_bit))
    {
        m_squares_attacked_by_white_bishops |= BmagicNOMASK(origin_square, m_all_pieces_bit & precomputed_moves::bishop_unfull_rays[origin_square]);
    }
}
void BitPosition::setBlackBishopsAttackedSquares()
{
    m_squares_attacked_by_black_bishops = 0;
    for (short int origin_square : getBitIndices(m_black_bishops_bit))
    {
        m_squares_attacked_by_black_bishops |= BmagicNOMASK(origin_square, m_all_pieces_bit & precomputed_moves::bishop_unfull_rays[origin_square]);
    }
}
void BitPosition::setWhiteRooksAttackedSquares()
{
    m_squares_attacked_by_white_rooks = 0;
    for (short int origin_square : getBitIndices(m_white_rooks_bit))
    {
        m_squares_attacked_by_white_rooks |= RmagicNOMASK(origin_square, m_all_pieces_bit & precomputed_moves::rook_unfull_rays[origin_square]);
    }
}
void BitPosition::setBlackRooksAttackedSquares()
{
    m_squares_attacked_by_black_rooks = 0;
    for (short int origin_square : getBitIndices(m_black_rooks_bit))
    {
        m_squares_attacked_by_black_rooks |= RmagicNOMASK(origin_square, m_all_pieces_bit & precomputed_moves::rook_unfull_rays[origin_square]);
    }
}
void BitPosition::setWhiteQueensAttackedSquares()
{
    m_squares_attacked_by_white_queens = 0;
    for (short int origin_square : getBitIndices(m_white_queens_bit))
    {
        m_squares_attacked_by_white_queens |= BmagicNOMASK(origin_square, m_all_pieces_bit & precomputed_moves::bishop_unfull_rays[origin_square]);
        m_squares_attacked_by_white_queens |= RmagicNOMASK(origin_square, m_all_pieces_bit & precomputed_moves::rook_unfull_rays[origin_square]);
    }
}
void BitPosition::setBlackQueensAttackedSquares()
{
    m_squares_attacked_by_black_queens = 0;
    for (short int origin_square : getBitIndices(m_black_queens_bit))
    {
        m_squares_attacked_by_black_queens |= BmagicNOMASK(origin_square, m_all_pieces_bit & precomputed_moves::bishop_unfull_rays[origin_square]);
        m_squares_attacked_by_black_queens |= RmagicNOMASK(origin_square, m_all_pieces_bit & precomputed_moves::rook_unfull_rays[origin_square]);
    }
}
void BitPosition::setWhiteKnightsAttackedSquares()
{
    m_squares_attacked_by_white_knights = 0;
    for (short int origin_square : getBitIndices(m_white_knights_bit))
    {
        m_squares_attacked_by_white_knights |= precomputed_moves::knight_moves[origin_square];
    }
}
void BitPosition::setBlackKnightsAttackedSquares()
{
    m_squares_attacked_by_black_knights = 0;
    for (short int origin_square : getBitIndices(m_black_knights_bit))
    {
        m_squares_attacked_by_black_knights |= precomputed_moves::knight_moves[origin_square];
    }
}
void BitPosition::setWhitePawnsAttackedSquares()
{
    m_squares_attacked_by_white_pawns = (((m_white_pawns_bit & 0b0111111101111111011111110111111101111111011111110111111101111111) << 9) | (((m_white_pawns_bit & 0b1111111011111110111111101111111011111110111111101111111011111110) << 7)));
}
void BitPosition::setBlackPawnsAttackedSquares()
{
    m_squares_attacked_by_black_pawns = (((m_black_pawns_bit & 0b0111111101111111011111110111111101111111011111110111111101111111) >> 7) | (((m_black_pawns_bit & 0b1111111011111110111111101111111011111110111111101111111011111110) >> 9)));
}
void BitPosition::setWhiteKingAttackedSquares()
{
    m_squares_attacked_by_white_king = precomputed_moves::king_moves[m_white_king_position];
}
void BitPosition::setBlackKingAttackedSquares()
{
    m_squares_attacked_by_black_king = precomputed_moves::king_moves[m_black_king_position];
}
void BitPosition::setSliderAttackedSquares()
{

    if (not m_white_bishops_attacked_squares_set && (m_squares_attacked_by_white_bishops & m_move_interference_bit_on_slider & 0b0000000001111110011111100111111001111110011111100111111000000000) != 0)
        BitPosition::setWhiteBishopsAttackedSquares();

    if (not m_black_bishops_attacked_squares_set && (m_squares_attacked_by_black_bishops & m_move_interference_bit_on_slider & 0b0000000001111110011111100111111001111110011111100111111000000000) != 0)
        BitPosition::setBlackBishopsAttackedSquares();

    if (not m_white_rooks_attacked_squares_set && (m_squares_attacked_by_white_rooks & m_move_interference_bit_on_slider) != 0)
        BitPosition::setWhiteRooksAttackedSquares();

    if (not m_black_rooks_attacked_squares_set && (m_squares_attacked_by_black_rooks & m_move_interference_bit_on_slider) != 0)
        BitPosition::setBlackRooksAttackedSquares();

    if (not m_white_queens_attacked_squares_set && (m_squares_attacked_by_white_queens & m_move_interference_bit_on_slider) != 0)
        BitPosition::setWhiteQueensAttackedSquares();

    if (not m_black_queens_attacked_squares_set && (m_squares_attacked_by_black_queens & m_move_interference_bit_on_slider) != 0)
        BitPosition::setBlackQueensAttackedSquares();
}

void BitPosition::setAttackedSquaresAfterMove()
{
    // origin and destination bit, if the bits are not at edge and interfere with sliders
    m_move_interference_bit_on_slider = m_last_origin_bit | m_last_destination_bit;
    m_white_bishops_attacked_squares_set = false;
    m_black_bishops_attacked_squares_set = false;
    m_white_rooks_attacked_squares_set = false;
    m_black_rooks_attacked_squares_set = false;
    m_white_queens_attacked_squares_set = false;
    m_black_queens_attacked_squares_set = false;
    if (not m_turn) // Last move was white
    {
        if (m_moved_piece == 0 || m_promoted_piece != 7) // Moved white pawn (Dont need to update black pawns, white/black knights or kings)
        {
            BitPosition::setWhitePawnsAttackedSquares();
        }
        if (m_moved_piece == 1 || m_promoted_piece == 1) // Moved white knight (Dont need to update black knights, white/black pawns or kings)
        {
            BitPosition::setWhiteKnightsAttackedSquares();
        }
        if (m_moved_piece == 5) // Moved white king (Dont need to update black king, white/black pawns or knights)
        {
            m_squares_attacked_by_white_king = precomputed_moves::king_moves[m_white_king_position];
        }
        if (m_moved_piece == 2 || m_promoted_piece == 2) // Moved white bishop
        {
            BitPosition::setWhiteBishopsAttackedSquares();
            m_white_bishops_attacked_squares_set = true;
        }
        if (m_moved_piece == 3 || m_promoted_piece == 3) // Moved white rook
        {
            BitPosition::setWhiteRooksAttackedSquares();
            m_white_rooks_attacked_squares_set = true;
        }
        if (m_moved_piece == 4 || m_promoted_piece == 4)
        {
            BitPosition::setWhiteQueensAttackedSquares();
            m_white_queens_attacked_squares_set = true;
        }

        if (m_captured_piece == 0) // Captured black pawn
            BitPosition::setBlackPawnsAttackedSquares();
        else if (m_captured_piece == 1) // Captured black knight
            BitPosition::setBlackKnightsAttackedSquares();
        else if (m_captured_piece == 2) // Captured black bishop
        {
            BitPosition::setBlackBishopsAttackedSquares();
            m_black_bishops_attacked_squares_set = true;
        }
        else if (m_captured_piece == 3) // Captured black rook
        {
            BitPosition::setBlackRooksAttackedSquares();
            m_black_rooks_attacked_squares_set = true;
        }
        else if (m_captured_piece == 4) // Captured black queen
        {
            BitPosition::setBlackQueensAttackedSquares();
            m_black_queens_attacked_squares_set = true;
        }
        BitPosition::setSliderAttackedSquares();
    }
    else // Last move was white
    {
        if (m_moved_piece == 0 || m_promoted_piece != 7) // Moved black pawn (Dont need to update white pawns, white/black knights or kings)
        {
            BitPosition::setBlackPawnsAttackedSquares();
        }
        if (m_moved_piece == 1 || m_promoted_piece == 1) // Moved black knight (Dont need to update white knights, white/black pawns or kings)
        {
            BitPosition::setBlackKnightsAttackedSquares();
        }
        if (m_moved_piece == 5) // Moved black king (Dont need to update black king, white/black pawns or knights)
        {
            m_squares_attacked_by_black_king = precomputed_moves::king_moves[m_black_king_position];
        }
        if (m_moved_piece == 2 || m_promoted_piece == 2) // Moved black bishop
        {
            BitPosition::setBlackBishopsAttackedSquares();
            m_black_bishops_attacked_squares_set = true;
        }
        if (m_moved_piece == 3 || m_promoted_piece == 3) // Moved black rook
        {
            BitPosition::setBlackRooksAttackedSquares();
            m_black_rooks_attacked_squares_set = true;
        }
        if (m_moved_piece == 4 || m_promoted_piece == 4)
        {
            BitPosition::setBlackQueensAttackedSquares();
            m_black_queens_attacked_squares_set = true;
        }

        // Resetting piece type that is captutred
        if (m_captured_piece == 0) // Captured white pawn
            BitPosition::setWhitePawnsAttackedSquares();
        else if (m_captured_piece == 1) // Captured white knight
            BitPosition::setWhiteKnightsAttackedSquares();
        else if (m_captured_piece == 2) // Captured white bishop
        {
            BitPosition::setWhiteBishopsAttackedSquares();
            m_white_bishops_attacked_squares_set = true;
        }
        else if (m_captured_piece == 3) // Captured white rook
        {
            BitPosition::setWhiteRooksAttackedSquares();
            m_white_rooks_attacked_squares_set = true;
        }
        else if (m_captured_piece == 4) // Captured white queen
        {
            BitPosition::setWhiteQueensAttackedSquares();
            m_white_queens_attacked_squares_set = true;
        }
        BitPosition::setSliderAttackedSquares();
    }

    m_all_squares_attacked_by_white = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens | m_squares_attacked_by_white_king;
    m_all_squares_attacked_by_black = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens | m_squares_attacked_by_black_king;
}

bool BitPosition::isCheck() const
// Return if we are in check or not
// This member function is faster than setChecksAndPinsBits
// We are going to check for checks more efficiently from the kings position and assuming it can move as any piece.
// This member function is the first member function to be called at the search.
{
    if (m_turn == true) // If white's turn
    {
        if ((m_all_squares_attacked_by_black & m_white_king_bit) == 0)
        {
            return false; // No checks
        }
        else
        {
            return true; // Checks
        }
    }

    else // If black's turn
    {
        if ((m_all_squares_attacked_by_white & m_black_king_bit) == 0)
        {
            return false; // No checks
        }
        else
        {
            return true; // Checks
        }
    }
}

void BitPosition::setPinsBits()
// Set bitboard of the pinned squares, these are squares containg pinned piece and the ray. We set three bitboards, one for straight pins, one for diagonal pins and
// another for all pins. This makes generating legal moves easier, since the pinned sliders can move diagonally or straight depending if the pin is diagonal or straight.
// Assuming pins are set to 0 after making move.
// This will be called in case we are not in check. Else we call setChecksAndPinsBits.
{
    m_is_check = false;
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
    m_is_check = true;
    if (m_turn) // White's turn
    {
        // Black pawns (Note we can only give check with one pawn at a time)
        if ((m_squares_attacked_by_black_pawns & m_white_king_bit) != 0)
        {
            m_pawn_checks |= (precomputed_moves::white_pawn_attacks[m_white_king_position] & m_black_pawns_bit);
            ++m_num_checks;
        }
        // We use blocks so that attacking_squares variables dont conflict between each
        // Black knights (Note we can only give check with one knight at a time)
        if ((m_squares_attacked_by_black_knights & m_white_king_bit) != 0)
        {
            m_knight_checks |= (precomputed_moves::knight_moves[m_white_king_position] & m_black_knights_bit);
            ++m_num_checks;
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
        if ((m_squares_attacked_by_white_pawns & m_black_king_bit) != 0)
        {
            m_pawn_checks |= (precomputed_moves::black_pawn_attacks[m_black_king_position] & m_white_pawns_bit);
            ++m_num_checks;
        }
        // White knights (Note we can only give check with one knight at a time)
        if ((m_squares_attacked_by_white_knights & m_black_king_bit) != 0)
        {
            m_knight_checks |= (precomputed_moves::knight_moves[m_black_king_position] & m_white_knights_bit);
            ++m_num_checks;
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
{
    if (m_turn) // White's turn
    {
        // Black bishops and queens
        if ((BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & (m_all_pieces_bit & ~((1ULL << removed_square_1) | (1ULL << removed_square_2)))) & (m_black_bishops_bit | m_black_queens_bit)) != 0)
            return false;

        // Black rooks and queens
        if ((RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & (m_all_pieces_bit & ~((1ULL << removed_square_1) | (1ULL << removed_square_2)))) & (m_black_rooks_bit | m_black_queens_bit)) != 0)
            return false;
    }
    else // Black's turn
    {
        // White bishops and queens
        if ((BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & (m_all_pieces_bit & ~((1ULL << removed_square_1) | (1ULL << removed_square_2)))) & (m_white_bishops_bit | m_white_queens_bit)) != 0)
            return false;

        // Black rooks and queens
        if ((RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & (m_all_pieces_bit & ~((1ULL << removed_square_1) | (1ULL << removed_square_2)))) & (m_white_rooks_bit | m_white_queens_bit)) != 0)
            return false;
    }
    return true;
}

std::vector<Capture> BitPosition::inCheckOrderedCaptures() const
{
    std::vector<std::pair<Capture, int>> captures_scores;
    captures_scores.reserve(20);
    std::array<uint64_t, 5> checks_array{m_pawn_checks, m_knight_checks, m_bishop_checks, m_rook_checks, m_queen_checks};
    if (m_turn) // White's turn
    {
        std::array<uint64_t, 5> pieces_array{m_black_pawns_bit, m_black_knights_bit, m_black_bishops_bit, m_black_rooks_bit, m_black_queens_bit};
        // Capturing with king
        for (unsigned short i = 0; i <= 4; ++i)
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & pieces_array[i] & ~m_all_squares_attacked_by_black))
            {
                if (BitPosition::kingIsSafeFromSliders(destination_square))
                {
                    captures_scores.emplace_back(Capture(m_white_king_position, destination_square, 5), i);
                }
            }
        }
        if (m_num_checks == 1)
        // We can only capture (with a piece that is not the king) or block if there is only one check
        {
            // Capturing with pawns
            for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & ~m_all_pins))
            {
                // For each opponent piece type
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
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 1, 1), i + 1);
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 2, 1), i + 2);
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 3, 1), i + 3);
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 4, 1), i + 4);
                            }
                            else
                            {
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 0), i);
                            }
                        }
                    }
                }
                // Passant capture
                if ((precomputed_moves::white_pawn_attacks[origin_square] & passant_bitboards[m_psquare]) != 0 && kingIsSafeAfterPassant(origin_square, m_psquare - 8))
                {
                    captures_scores.emplace_back(Capture(origin_square, m_psquare, 0), 0);
                }
            }
            // Capturing with knights
            for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{precomputed_moves::knight_moves[origin_square] & checks_array[i]};
                    if (moveable_squares != 0)
                    {
                        captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(moveable_squares), 1), i - 1);
                    }
                }
            }
            // Capturing with bishops
            for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & checks_array[i]};
                    if (moveable_squares != 0)
                    {
                        captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(moveable_squares), 2), i - 2);
                    }
                }
            }
            // Capturing with rooks
            for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & checks_array[i]};
                    if (moveable_squares != 0)
                    {
                        captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(moveable_squares), 3), i - 3);
                    }
                }
            }
            // Capturing with queens
            for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & checks_array[i]};
                    if (moveable_squares != 0)
                    {
                        captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(moveable_squares), 4), i - 4);
                    }
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
            for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & pieces_array[i] & ~m_all_squares_attacked_by_white))
            {
                if (BitPosition::kingIsSafeFromSliders(destination_square))
                {
                    captures_scores.emplace_back(Capture(m_black_king_position, destination_square, 5), i + 1);
                }
            }
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
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 1, 1), i + 1);
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 2, 1), i + 2);
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 3, 1), i + 3);
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 4, 1), i + 4);
                            }
                            else
                            {
                                captures_scores.emplace_back(Capture(origin_square, destination_square, 0), i);
                            }
                        }
                    }
                }
                // Passant capture
                if ((precomputed_moves::black_pawn_attacks[origin_square] & passant_bitboards[m_psquare]) != 0 && kingIsSafeAfterPassant(origin_square, m_psquare + 8))
                {
                    captures_scores.emplace_back(Capture(origin_square, m_psquare, 0), 0);
                }
            }
            // Capturing with knights
            for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{precomputed_moves::knight_moves[origin_square] & checks_array[i]};
                    if (moveable_squares != 0)
                    {
                        captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(moveable_squares), 1), i - 1);
                    }
                }
            }
            // Capturing with bishops
            for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & checks_array[i]};
                    if (moveable_squares != 0)
                    {
                        captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(moveable_squares), 2), i - 2);
                    }
                }
            }
            // Capturing with rooks
            for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & checks_array[i]};
                    if (moveable_squares != 0)
                    {
                        captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(moveable_squares), 3), i - 3);
                    }
                }
            }
            // Capturing with queens
            for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
            {
                for (unsigned short i = 0; i <= 4; ++i)
                {
                    uint64_t moveable_squares{(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & checks_array[i]};
                    if (moveable_squares != 0)
                    {
                        captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(moveable_squares), 4), i - 4);
                    }
                }
            }
        }
    }
    // Sort the pair vector by scores, highest first
    std::sort(captures_scores.begin(), captures_scores.end(),
              [](const std::pair<Capture, int> &a, const std::pair<Capture, int> &b)
              {
                  return a.second > b.second; // Sort by score in descending order
              });

    std::vector<Capture> captures;
    captures.reserve(captures_scores.size()); // Reserve space to avoid reallocations

    // Unpack the sorted captures into the vector
    for (size_t i = 0; i < captures_scores.size(); ++i)
    {
        captures.push_back(captures_scores[i].first); // Note: first is the Move, second is the score
    }
    return captures;
}

std::vector<Capture> BitPosition::orderedCaptures() const
// We create a set of moves and order them in terms of the score, returning a tuple of moves. This set of moves will always be computed before
// non_capture_moves.
{
    std::vector<std::pair<Capture, int>> captures_scores;
    captures_scores.reserve(50);
    if (m_turn) // White's turn
    {
        std::array<uint64_t, 5> pieces_array{m_black_pawns_bit, m_black_knights_bit, m_black_bishops_bit, m_black_rooks_bit, m_black_queens_bit};
        // Capturing with knights that arent pinned
        for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                for (unsigned short destination_square : (getBitIndices(precomputed_moves::knight_moves[origin_square] & pieces_array[i])))
                {
                    captures_scores.emplace_back(Capture(origin_square, destination_square, 1), i - 1);
                }
            }
        }
        // Capturing with king
        for (unsigned short i = 0; i <= 4; ++i)
        {
            for (unsigned short destination_square : (getBitIndices(precomputed_moves::king_moves[m_white_king_position] & pieces_array[i] & ~m_all_squares_attacked_by_black)))
            {
                captures_scores.emplace_back(Capture(m_white_king_position, destination_square, 5), i);
            }
        }
        // Capturing with rooks that arent pinned
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                for (unsigned short destination_square : (getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i])))
                {
                    captures_scores.emplace_back(Capture(origin_square, destination_square, 3), i - 3);
                }
            }
        }
        // Capturing with bishops that arent pinned
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                for (unsigned short destination_square : (getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i])))
                {
                    captures_scores.emplace_back(Capture(origin_square, destination_square, 2), i - 1);
                }
            }
        }
        // Capturing with queens that arent pinned
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                for (unsigned short destination_square : (getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & pieces_array[i])))
                {
                    captures_scores.emplace_back(Capture(origin_square, destination_square, 4), i - 4);
                }
            }
        }
        // Pawn captures
        for (unsigned short i = 0; i <= 4; ++i)
        {
            for (unsigned short destination_square : getBitIndices((((m_white_pawns_bit & ~m_all_pins & 0b1111111011111110111111101111111011111110111111101111111011111110) << 7) & pieces_array[i])))
            {
                if (destination_square > 55)
                // Promotions
                {
                    captures_scores.emplace_back(Capture(destination_square - 7, destination_square, 1, 1), i + 1);
                    captures_scores.emplace_back(Capture(destination_square - 7, destination_square, 2, 1), i + 2);
                    captures_scores.emplace_back(Capture(destination_square - 7, destination_square, 3, 1), i + 3);
                    captures_scores.emplace_back(Capture(destination_square - 7, destination_square, 4, 1), i + 4);
                }
                else
                {
                    captures_scores.emplace_back(Capture(destination_square - 7, destination_square, 0), i);
                }
            }
            for (unsigned short destination_square : getBitIndices((((m_white_pawns_bit & ~m_all_pins & 0b0111111101111111011111110111111101111111011111110111111101111111) << 9) & pieces_array[i])))
            {
                if (destination_square > 55)
                // Promotions
                {
                    captures_scores.emplace_back(Capture(destination_square - 9, destination_square, 1, 1), i + 1);
                    captures_scores.emplace_back(Capture(destination_square - 9, destination_square, 2, 1), i + 2);
                    captures_scores.emplace_back(Capture(destination_square - 9, destination_square, 3, 1), i + 3);
                    captures_scores.emplace_back(Capture(destination_square - 9, destination_square, 4, 1), i + 4);
                }
                else
                {
                    captures_scores.emplace_back(Capture(destination_square - 9, destination_square, 0), i);
                }
            }
        }
        // Passant capture
        if (m_psquare != 0)
        {
            if ((((1ULL << (m_psquare - 7)) & m_white_pawns_bit & 0b1111111011111110111111101111111011111110111111101111111011111110 & ~m_all_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare - 7, m_psquare - 8))
            {
                captures_scores.emplace_back(Capture(m_psquare - 7, m_psquare, 0), 0);
            }

            if ((((1ULL << (m_psquare - 9)) & m_white_pawns_bit & 0b0111111101111111011111110111111101111111011111110111111101111111 & ~m_all_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare - 9, m_psquare - 8))
            {
                captures_scores.emplace_back(Capture(m_psquare - 9, m_psquare, 0), 0);
            }
        }
        // Capturing with pinned pieces
        // (Note, pinned rooks, bishops and queens will always be able to capture pinned piece if it is a diagonal/straight pin for bishops/rooks)

        // Capturing with pinned bishops
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & m_diagonal_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                uint64_t destination_bit{BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i] & m_diagonal_pins};
                if (destination_bit != 0)
                {
                    captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(destination_bit), 2), i - 2);
                }
            }
        }
        // Capturing with pinned rooks
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & m_straight_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                uint64_t destination_bit{RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i] & m_straight_pins};
                if (destination_bit != 0)
                {
                    captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(destination_bit), 3), i - 3);
                }
            }
        }
        // Capturing with pinned queens
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & m_diagonal_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            // Diagonal moves
            {
                uint64_t destination_bit{BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i] & m_diagonal_pins};
                if (destination_bit != 0)
                {
                    captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(destination_bit), 4), i - 4);
                }
            }
        }
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & m_straight_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            // Straight moves
            {
                uint64_t destination_bit{RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i] & m_straight_pins};
                if (destination_bit != 0)
                {
                    captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(destination_bit), 4), i - 4);
                }
            }
        }
        // Capturing with pinned pawns
        for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & m_diagonal_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                // Diagonal moves
                for (unsigned short destination_square : (getBitIndices(precomputed_moves::white_pawn_attacks[origin_square] & pieces_array[i] & m_diagonal_pins)))
                {
                    if (destination_square > 55)
                    {
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 1, 1), i + 1);
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 2, 1), i + 2);
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 3, 1), i + 3);
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 4, 1), i + 4);
                    }
                    else
                    {
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 0), i);
                    }
                }
            }
        }
    }
    else // Black's turn
    {
        std::array<uint64_t, 5> pieces_array{m_white_pawns_bit, m_white_knights_bit, m_white_bishops_bit, m_white_rooks_bit, m_white_queens_bit};
        // Capturing with knights that arent pinned
        for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                for (unsigned short destination_square : (getBitIndices(precomputed_moves::knight_moves[origin_square] & pieces_array[i])))
                {
                    captures_scores.emplace_back(Capture(origin_square, destination_square, 1), i - 1);
                }
            }
        }
        // Capturing with king
        for (unsigned short i = 0; i <= 4; ++i)
        {
            for (unsigned short destination_square : (getBitIndices(precomputed_moves::king_moves[m_black_king_position] & pieces_array[i] & ~m_all_squares_attacked_by_white)))
            {
                captures_scores.emplace_back(Capture(m_black_king_position, destination_square, 5), i);
            }
        }
        // Capturing with rooks that arent pinned
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                for (unsigned short destination_square : (getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i])))
                {
                    captures_scores.emplace_back(Capture(origin_square, destination_square, 3), i - 3);
                }
            }
        }
        // Capturing with bishops that arent pinned
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                for (unsigned short destination_square : (getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i])))
                {
                    captures_scores.emplace_back(Capture(origin_square, destination_square, 2), i - 2);
                }
            }
        }
        // Capturing with queens that arent pinned
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                for (unsigned short destination_square : (getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & pieces_array[i])))
                {
                    captures_scores.emplace_back(Capture(origin_square, destination_square, 4), i - 4);
                }
            }
        }
        // Pawn captures
        for (unsigned short i = 0; i <= 4; ++i)
        {
            for (unsigned short destination_square : getBitIndices(((m_black_pawns_bit & ~m_all_pins & 0b0111111101111111011111110111111101111111011111110111111101111111) >> 7) & pieces_array[i]))
            {
                if (destination_square < 8)
                // Promotions
                {
                    captures_scores.emplace_back(Capture(destination_square + 7, destination_square, 1, 1), i + 1);
                    captures_scores.emplace_back(Capture(destination_square + 7, destination_square, 2, 1), i + 2);
                    captures_scores.emplace_back(Capture(destination_square + 7, destination_square, 3, 1), i + 3);
                    captures_scores.emplace_back(Capture(destination_square + 7, destination_square, 4, 1), i + 4);
                }
                else
                {
                    captures_scores.emplace_back(Capture(destination_square + 7, destination_square, 0), i);
                }
            }
            for (unsigned short destination_square : getBitIndices(((m_black_pawns_bit & ~m_all_pins & 0b1111111011111110111111101111111011111110111111101111111011111110) >> 9) & pieces_array[i]))
            {
                if (destination_square < 8)
                // Promotions
                {
                    captures_scores.emplace_back(Capture(destination_square + 9, destination_square, 1, 1), i + 1);
                    captures_scores.emplace_back(Capture(destination_square + 9, destination_square, 2, 1), i + 2);
                    captures_scores.emplace_back(Capture(destination_square + 9, destination_square, 3, 1), i + 3);
                    captures_scores.emplace_back(Capture(destination_square + 9, destination_square, 4, 1), i + 4);
                }
                else
                {
                    captures_scores.emplace_back(Capture(destination_square + 9, destination_square, 0), i);
                }
            }
        }
        // Passant capture
        if (m_psquare != 0)
        {
            if ((((1ULL << (m_psquare + 9)) & m_black_pawns_bit & 0b1111111011111110111111101111111011111110111111101111111011111110 & ~m_all_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare + 8, m_psquare + 9))
            {
                captures_scores.emplace_back(Capture(m_psquare + 9, m_psquare, 0), 0);
            }

            if ((((1ULL << (m_psquare + 7)) & m_black_pawns_bit & 0b0111111101111111011111110111111101111111011111110111111101111111 & ~m_all_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare + 8, m_psquare + 7))
            {
                captures_scores.emplace_back(Capture(m_psquare + 7, m_psquare, 0), 0);
            }
        }
        // Capturing with pinned pieces
        // (Note, pinned rooks, bishops and queens will always be able to capture pinned piece if it is a diagonal/straight pin for bishops/rooks)

        // Capturing with pinned bishops
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & m_diagonal_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                uint64_t destination_bit{BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i] & m_diagonal_pins};
                if (destination_bit != 0)
                {
                    captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(destination_bit), 2), i - 2);
                }
            }
        }
        // Capturing with pinned rooks
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & m_straight_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                uint64_t destination_bit{RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i] & m_straight_pins};
                if (destination_bit != 0)
                {
                    captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(destination_bit), 3), i - 3);
                }
            }
        }
        // Capturing with pinned queens
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & m_diagonal_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            // Diagonal moves
            {
                uint64_t destination_bit{BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i] & m_diagonal_pins};
                if (destination_bit != 0)
                {
                    captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(destination_bit), 4), i - 4);
                }
            }
        }
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & m_straight_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            // Straight moves
            {
                uint64_t destination_bit{RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & pieces_array[i] & m_straight_pins};
                if (destination_bit != 0)
                {
                    captures_scores.emplace_back(Capture(origin_square, getLeastSignificantBitIndex(destination_bit), 4), i - 4);
                }
            }
        }
        // Capturing with pinned pawns
        for (unsigned short origin_square : getBitIndices(m_black_pawns_bit & m_diagonal_pins))
        {
            for (unsigned short i = 0; i <= 4; ++i)
            {
                // Diagonal moves
                for (unsigned short destination_square : (getBitIndices(precomputed_moves::black_pawn_attacks[origin_square] & pieces_array[i] & m_diagonal_pins)))
                {
                    if (destination_square < 8)
                    {
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 1, 1), i + 1);
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 2, 1), i + 2);
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 3, 1), i + 3);
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 4, 1), i + 4);
                    }
                    else
                    {
                        captures_scores.emplace_back(Capture(origin_square, destination_square, 0), i);
                    }
                }
            }
        }
    }
    // Sort the pair vector by scores, highest first
    std::sort(captures_scores.begin(), captures_scores.end(),
              [](const std::pair<Capture, int> &a, const std::pair<Capture, int> &b)
              {
                  return a.second > b.second; // Sort by score in descending order
              });

    std::vector<Capture> captures;
    captures.reserve(captures_scores.size()); // Reserve space to avoid reallocations

    // Unpack the sorted captures into the vector
    for (size_t i = 0; i < captures_scores.size(); ++i)
    {
        captures.push_back(captures_scores[i].first); // Note: first is the Move, second is the score
    }
    return captures;
}

void BitPosition::removePiece()
// Remove a piece (capture), given a destination bit
{
    if (m_turn)
    {
        // array of pointers to opponent pieces
        std::array<uint64_t *, 5> piece_arrays{&m_black_pawns_bit, &m_black_knights_bit, &m_black_bishops_bit, &m_black_rooks_bit, &m_black_queens_bit};

        int count{0};
        for (auto &piece_bit : piece_arrays)
        {
            if ((m_last_destination_bit & *piece_bit) != 0)
            {
                m_captured_piece = count;
                *piece_bit &= ~m_last_destination_bit;
                break;
            }
            count++;
        }
    }
    else
    {
        // array of pointers to opponent pieces
        std::array<uint64_t *, 5> piece_arrays{&m_white_pawns_bit, &m_white_knights_bit, &m_white_bishops_bit, &m_white_rooks_bit, &m_white_queens_bit};

        int count{0};
        for (auto &piece_bit : piece_arrays)
        {
            if ((m_last_destination_bit & *piece_bit) != 0)
            {
                m_captured_piece = count;
                *piece_bit &= ~m_last_destination_bit;
                break;
            }
            count++;
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
    m_is_check_array[m_ply] = m_is_check;

    m_squares_attacked_by_white_pawns_array[m_ply] = m_squares_attacked_by_white_pawns;
    m_squares_attacked_by_black_pawns_array[m_ply] = m_squares_attacked_by_black_pawns;
    m_squares_attacked_by_white_knights_array[m_ply] = m_squares_attacked_by_white_knights;
    m_squares_attacked_by_black_knights_array[m_ply] = m_squares_attacked_by_black_knights;
    m_squares_attacked_by_white_bishops_array[m_ply] = m_squares_attacked_by_white_bishops;
    m_squares_attacked_by_black_bishops_array[m_ply] = m_squares_attacked_by_black_bishops;
    m_squares_attacked_by_white_rooks_array[m_ply] = m_squares_attacked_by_white_rooks;
    m_squares_attacked_by_black_rooks_array[m_ply] = m_squares_attacked_by_black_rooks;
    m_squares_attacked_by_white_queens_array[m_ply] = m_squares_attacked_by_white_queens;
    m_squares_attacked_by_black_queens_array[m_ply] = m_squares_attacked_by_black_queens;
    m_squares_attacked_by_white_king_array[m_ply] = m_squares_attacked_by_white_king;
    m_squares_attacked_by_black_king_array[m_ply] = m_squares_attacked_by_black_king;

    m_all_squares_attacked_by_white_array[m_ply] = m_all_squares_attacked_by_white;
    m_all_squares_attacked_by_black_array[m_ply] = m_all_squares_attacked_by_black;
}

void BitPosition::makeCapture(Capture move)
// Captures and/or promotions
// Move piece and switch white and black roles, without rotating the board.
// After making move, set pins and checks to 0.
{
    BitPosition::storePlyInfo(); // store current state for unmake move

    unsigned short origin_square{move.getOriginSquare()};
    m_last_origin_bit = (1ULL << origin_square);
    unsigned short destination_square{move.getDestinationSquare()};
    m_last_destination_bit = (1ULL << destination_square);
    m_captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion
    m_moved_piece = move.getMovingOrPromotingPiece();
    unsigned short psquare{0};
    BitPosition::removePiece();
    if (m_turn) // White's move
    {
        // Update NNUE Input after m_captured_piece is set in removePiece(), unless passant capture
        if (m_captured_piece != 7)
        {
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, (m_captured_piece + 5) * 64 + destination_square);
        }

        if (destination_square == 63) // If we capture on h8
        {
            m_black_kingside_castling = false;
        }

        if (destination_square == 56) // If we capture on h1
        {
            m_black_queenside_castling = false;
        }

        if (move.isPromotion()) // Promotions
        {
            m_white_pawns_bit &= ~m_last_origin_bit;
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, origin_square);

            m_promoted_piece = m_moved_piece;
            if (m_promoted_piece == 1)
            {
                m_white_knights_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
            }
            else if (m_promoted_piece == 2)
            {
                m_white_bishops_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
            }
            else if (m_promoted_piece == 3)
            {
                m_white_rooks_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
            }
            else
            {
                m_white_queens_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
            }
            m_moved_piece = 0;
        }
        else if (m_moved_piece == 0) // Moving White Pawns (Non promotions)
        {
            m_white_pawns_bit &= ~m_last_origin_bit;
            m_white_pawns_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, destination_square);
            if (destination_square == m_psquare) // Passant capture
            {
                m_captured_piece = 0;
                m_black_pawns_bit &= ~(m_last_destination_bit >> 8);
                // Update NNUE Input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square - 8);
            }
            else if ((m_last_origin_bit << 16) == m_last_destination_bit) // Double move
                psquare = origin_square + 8;
        }
        else if (m_moved_piece == 1) // Moving White knights
        {
            m_white_knights_bit &= ~m_last_origin_bit;
            m_white_knights_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
        }
        else if (m_moved_piece == 2) // Moving White Bishops
        {
            m_white_bishops_bit &= ~m_last_origin_bit;
            m_white_bishops_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
        }
        else if (m_moved_piece == 3) // Moving White Rooks
        {
            // Castiling rights removed
            if (origin_square == 7)
                m_white_kingside_castling = false;
            if (origin_square == 0)
                m_white_queenside_castling = false;

            // Moving the rook
            m_white_rooks_bit &= ~m_last_origin_bit;
            m_white_rooks_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
        }
        else if (m_moved_piece == 4) // Moving White Queens
        {
            m_white_queens_bit &= ~m_last_origin_bit;
            m_white_queens_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
        }
        else // Moving king
        {
            m_white_kingside_castling = false;
            m_white_queenside_castling = false;
            m_white_king_position = destination_square;
            m_white_king_bit = m_last_destination_bit;

            // Update NNUE input
            NNUE::moveWhiteKingNNUEInput(*this);
        }
    }
    else // Black's move
    {
        // Update NNUE Input after m_captured_piece is set in removePiece(), unless passant capture
        if (m_captured_piece != 7)
        {
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_captured_piece * 64 + destination_square);
        }

        if (destination_square == 7) // If we capture on a8
            m_white_kingside_castling = false;

        if (destination_square == 0) // If we capture rook a1
            m_white_queenside_castling = false;

        if (move.isPromotion()) // Promotions
        {
            m_black_pawns_bit &= ~m_last_origin_bit;
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);

            m_promoted_piece = m_moved_piece;
            if (m_promoted_piece == 1)
            {
                m_black_knights_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
            }
            else if (m_promoted_piece == 2)
            {
                m_black_bishops_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
            }
            else if (m_promoted_piece == 3)
            {
                m_black_rooks_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
            }
            else
            {
                m_black_queens_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
            }
            m_moved_piece = 0;
        }

        else if (m_moved_piece == 0) // Moving Black Pawns (Non promotions)
        {
            m_black_pawns_bit &= ~m_last_origin_bit;
            m_black_pawns_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);

            if (destination_square == m_psquare) // Passant capture
            {
                m_captured_piece = 0;
                m_white_pawns_bit &= ~(m_last_destination_bit << 8);
                // Update NNUE Input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square + 8);
            }
            else if ((m_last_origin_bit >> 16) == m_last_destination_bit) // Double move
                psquare = destination_square + 8;
        }
        else if (m_moved_piece == 1) // Moving Black knights
        {
            m_black_knights_bit &= ~m_last_origin_bit;
            m_black_knights_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
        }
        else if (m_moved_piece == 2) // Moving Black bishops
        {
            m_black_bishops_bit &= ~m_last_origin_bit;
            m_black_bishops_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
        }
        else if (m_moved_piece == 3) // Moving black rooks
        {
            // Castling rights removed
            if (origin_square == 63)
                m_black_kingside_castling = false;

            if (origin_square == 56)
                m_black_queenside_castling = false;

            // Moving the rook
            m_black_rooks_bit &= ~m_last_origin_bit;
            m_black_rooks_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
        }
        else if (m_moved_piece == 4) // Moving black queens
        {
            m_black_queens_bit &= ~m_last_origin_bit;
            m_black_queens_bit |= m_last_destination_bit;

            // Update NNUE Input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
        }
        else // Moving king
        {
            m_black_kingside_castling = false;
            m_black_queenside_castling = false;
            m_black_king_position = destination_square;
            m_black_king_bit = m_last_destination_bit;

            // Update NNUE input
            NNUE::moveBlackKingNNUEInput(*this);
        }
    }

    BitPosition::updateZobristKeyPiecePartAfterMove(origin_square, destination_square);
    m_captured_piece_array[m_ply] = m_captured_piece;
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

    m_psquare = psquare;

    BitPosition::setAllPiecesBits();
}
void BitPosition::unmakeCapture(Capture move)
// Takes a move and undoes the move accordingly, updating all position attributes. When the engine transverses the tree of moves it will keep
// track of some irreversible aspects of the game at each ply.These are(white castling rights, black castling rights, passant square,
// capture index, pins, checks).
{
    m_ply--;

    // Update irreversible aspects
    m_white_kingside_castling = m_wkcastling_array[m_ply];
    m_white_queenside_castling = m_wqcastling_array[m_ply];
    m_black_kingside_castling = m_bkcastling_array[m_ply];
    m_black_queenside_castling = m_bqcastling_array[m_ply];
    m_psquare = m_psquare_array[m_ply];

    // Get irreversible info
    unsigned short previous_captured_piece{m_captured_piece_array[m_ply]};

    // Update aspects to not recompute
    m_diagonal_pins = m_diagonal_pins_array[m_ply];
    m_straight_pins = m_straight_pins_array[m_ply];
    m_is_check = m_is_check_array[m_ply];

    m_squares_attacked_by_white_pawns = m_squares_attacked_by_white_pawns_array[m_ply];
    m_squares_attacked_by_black_pawns = m_squares_attacked_by_black_pawns_array[m_ply];
    m_squares_attacked_by_white_knights = m_squares_attacked_by_white_knights_array[m_ply];
    m_squares_attacked_by_black_knights = m_squares_attacked_by_black_knights_array[m_ply];
    m_squares_attacked_by_white_bishops = m_squares_attacked_by_white_bishops_array[m_ply];
    m_squares_attacked_by_black_bishops = m_squares_attacked_by_black_bishops_array[m_ply];
    m_squares_attacked_by_white_rooks = m_squares_attacked_by_white_rooks_array[m_ply];
    m_squares_attacked_by_black_rooks = m_squares_attacked_by_black_rooks_array[m_ply];
    m_squares_attacked_by_white_queens = m_squares_attacked_by_white_queens_array[m_ply];
    m_squares_attacked_by_black_queens = m_squares_attacked_by_black_queens_array[m_ply];
    m_squares_attacked_by_white_king = m_squares_attacked_by_white_king_array[m_ply];
    m_squares_attacked_by_black_king = m_squares_attacked_by_black_king_array[m_ply];

    m_all_squares_attacked_by_white = m_all_squares_attacked_by_white_array[m_ply];
    m_all_squares_attacked_by_black = m_all_squares_attacked_by_black_array[m_ply];

    m_zobrist_key = m_zobrist_keys_array[m_ply];
    m_zobrist_keys_array[m_ply + 1] = 0;

    // Get move info
    unsigned short origin_square{move.getOriginSquare()};
    m_last_origin_bit = (1ULL << origin_square);
    unsigned short destination_square{move.getDestinationSquare()};
    m_last_destination_bit = (1ULL << destination_square);
    m_moved_piece = move.getMovingOrPromotingPiece(); // Moved piece / promoting piece

    if (m_turn) // Last move was black
    {
        // Unmaking promotions (own pawns part only)
        if (move.isPromotion())
        {
            m_black_pawns_bit |= m_last_origin_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);

            if (m_moved_piece == 4) // Unpromote black queen
            {
                m_black_queens_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
            }
            else if (m_moved_piece == 3) // Unpromote black rook
            {
                m_black_rooks_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
            }
            else if (m_moved_piece == 2) // Unpromote black bishop
            {
                m_black_bishops_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
            }
            else // Unpromote black knight
            {
                m_black_knights_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
            }
        }

        // Unmaking normal captures (own pieces part only)
        else
        {
            if (m_moved_piece == 0) // Unmove black pawn
            {
                m_black_pawns_bit |= m_last_origin_bit;
                m_black_pawns_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);
            }
            else if (m_moved_piece == 1) // Unmove black knight
            {
                m_black_knights_bit |= m_last_origin_bit;
                m_black_knights_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + origin_square);
            }
            else if (m_moved_piece == 2) // Unmove black bishop
            {
                m_black_bishops_bit |= m_last_origin_bit;
                m_black_bishops_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + origin_square);
            }
            else if (m_moved_piece == 3) // Unmove black rook
            {
                m_black_rooks_bit |= m_last_origin_bit;
                m_black_rooks_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + origin_square);
            }
            else if (m_moved_piece == 4) // Unmove black queen
            {
                m_black_queens_bit |= m_last_origin_bit;
                m_black_queens_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + origin_square);
            }
            else // Unmove king
            {
                m_black_king_bit = m_last_origin_bit;
                m_black_king_position = origin_square;
                NNUE::moveBlackKingNNUEInput(*this);
            }
        }

        // Unmaking captures (opponent's pieces part)
        if (previous_captured_piece == 0) // Uncapture pawn
        {
            // Unmake passant capture
            if (destination_square == m_psquare)
            {
                m_white_pawns_bit |= (1ULL << (m_psquare + 8));
                NNUE::addOnInput(m_white_king_position, m_black_king_position, m_psquare + 8);
            }
            // Unmake normal pawn capture
            else
            {
                m_white_pawns_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, destination_square);
            }
        }
        else if (previous_captured_piece == 1) // Uncapture knight
        {
            m_white_knights_bit |= m_last_destination_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
        }
        else if (previous_captured_piece == 2) // Uncapture bishop
        {
            m_white_bishops_bit |= m_last_destination_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
        }
        else if (previous_captured_piece == 3) // Uncapture rook
        {
            m_white_rooks_bit |= m_last_destination_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
        }
        else // Uncapture queen
        {
            m_white_queens_bit |= m_last_destination_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
        }
    }
    else // Last move was white
    {
        // Unmaking promotions (own pawns part only)
        if (move.isPromotion())
        {
            m_white_pawns_bit |= m_last_origin_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);

            if (m_moved_piece == 4) // Unpromote white queen
            {
                m_white_queens_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
            }
            else if (m_moved_piece == 3) // Unpromote white rook
            {
                m_white_rooks_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
            }
            else if (m_moved_piece == 2) // Unpromote white bishop
            {
                m_white_bishops_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
            }
            else // Unpromote white knight
            {
                m_white_knights_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
            }
        }

        // Unmaking normal captures (own pieces part only)
        else
        {
            if (m_moved_piece == 0) // Unmove pawn
            {
                m_white_pawns_bit |= m_last_origin_bit;
                m_white_pawns_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);
            }
            else if (m_moved_piece == 1) // Unmove knight
            {
                m_white_knights_bit |= m_last_origin_bit;
                m_white_knights_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + origin_square);
            }
            else if (m_moved_piece == 2) // Unmove bishop
            {
                m_white_bishops_bit |= m_last_origin_bit;
                m_white_bishops_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + origin_square);
            }
            else if (m_moved_piece == 3) // Unmove rook
            {
                m_white_rooks_bit |= m_last_origin_bit;
                m_white_rooks_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + origin_square);
            }
            else if (m_moved_piece == 4) // Unmove queen
            {
                m_white_queens_bit |= m_last_origin_bit;
                m_white_queens_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + origin_square);
            }
            else // Unmove king
            {
                m_white_king_bit = m_last_origin_bit;
                m_white_king_position = origin_square;
                NNUE::moveWhiteKingNNUEInput(*this);
            }
        }

        // Unmaking captures (opponent's pieces part)
        if (previous_captured_piece == 0) // Uncapture pawn
        {
            // Unmake passant capture
            if (destination_square == m_psquare)
            {
                m_black_pawns_bit |= (1ULL << (m_psquare - 8));
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_psquare - 8);
            }
            // Unmake normal pawn capture
            else
            {
                m_black_pawns_bit |= m_last_destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
            }
        }
        else if (previous_captured_piece == 1) // Uncapture knight
        {
            m_black_knights_bit |= m_last_destination_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
        }
        else if (previous_captured_piece == 2) // Uncapture bishop
        {
            m_black_bishops_bit |= m_last_destination_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
        }
        else if (previous_captured_piece == 3) // Uncapture rook
        {
            m_black_rooks_bit |= m_last_destination_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
        }
        else // Uncapture queen
        {
            m_black_queens_bit |= m_last_destination_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
        }
    }

    BitPosition::setAllPiecesBits();
    m_turn = not m_turn;
}

bool BitPosition::kingIsSafeFromSliders(unsigned short king_square) const
// See if the king is in check or not (from kings position). For when moving the king.
// Only used when in check in case the king moves away from slider check but still in ray.
{
    if (m_turn) // White's turn
    {
        // Black bishops and queens
        if ((BmagicNOMASK(king_square, precomputed_moves::bishop_unfull_rays[king_square] & (m_all_pieces_bit & ~m_white_king_bit)) & (m_black_bishops_bit | m_black_queens_bit)) != 0)
            return false;
        // Black rooks and queens
        if ((RmagicNOMASK(king_square, precomputed_moves::rook_unfull_rays[king_square] & (m_all_pieces_bit & ~m_white_king_bit)) & (m_black_rooks_bit | m_black_queens_bit)) != 0)
            return false;
    }
    else // Black's turn
    {
        // White bishops and queens
        if ((BmagicNOMASK(king_square, precomputed_moves::bishop_unfull_rays[king_square] & (m_all_pieces_bit & ~m_black_king_bit)) & (m_white_bishops_bit | m_white_queens_bit)) != 0)
            return false;
        // Black rooks and queens
        if ((RmagicNOMASK(king_square, precomputed_moves::rook_unfull_rays[king_square] & (m_all_pieces_bit & ~m_black_king_bit)) & (m_white_rooks_bit | m_white_queens_bit)) != 0)
            return false;
    }
    return true;
}

std::vector<Move> BitPosition::inCheckAllMoves() const
{
    std::vector<Move> moves;
    moves.reserve(30);
    uint64_t checks_bit{m_pawn_checks | m_knight_checks | m_bishop_checks | m_rook_checks | m_queen_checks};
    if (m_turn) // White's turn
    {
        // Moving king
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_squares_attacked_by_black & ~m_white_pieces_bit))
        {
            if (BitPosition::kingIsSafeFromSliders(destination_square))
                moves.emplace_back(m_white_king_position, destination_square);
        }

        if (m_num_checks == 1)
        // We can only capture (with a piece that is not the king) or block if there is only one check
        {
            // Captures from square being captured
            unsigned short destination_square{getLeastSignificantBitIndex(checks_bit)};
            // Pawn captures
            if (destination_square < 56)
            {
                for (unsigned short origin_square : getBitIndices(precomputed_moves::black_pawn_attacks[destination_square] & m_white_pawns_bit & ~m_all_pins))
                    moves.emplace_back(origin_square, destination_square);
            }
            else
            {
                for (unsigned short origin_square : getBitIndices(precomputed_moves::black_pawn_attacks[destination_square] & m_white_pawns_bit & ~m_all_pins))
                {
                    moves.push_back(Move(origin_square, destination_square, 1));
                    moves.push_back(Move(origin_square, destination_square, 2));
                    moves.push_back(Move(origin_square, destination_square, 3));
                    moves.push_back(Move(origin_square, destination_square, 4));
                }
            }

            // Knight captures
            for (unsigned short origin_square : getBitIndices(precomputed_moves::knight_moves[destination_square] & m_white_knights_bit & ~m_all_pins))
                moves.emplace_back(origin_square, destination_square);
            // Bishop captures
            for (unsigned short origin_square : getBitIndices(BmagicNOMASK(destination_square, precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit) & m_white_bishops_bit & ~m_all_pins))
                moves.emplace_back(origin_square, destination_square);
            // Rook captures
            for (unsigned short origin_square : getBitIndices(RmagicNOMASK(destination_square, precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit) & m_white_rooks_bit & ~m_all_pins))
                moves.emplace_back(origin_square, destination_square);
            // Queen captures
            for (unsigned short origin_square : getBitIndices((BmagicNOMASK(destination_square, precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit) | RmagicNOMASK(destination_square, precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit)) & m_white_queens_bit & ~m_all_pins))
                moves.emplace_back(origin_square, destination_square);

            // Passant capture (If pawn giving check can be en passant captured)
            if (m_psquare != 0 && (m_pawn_checks & (passant_bitboards[m_psquare] >> 8)) != 0)
            {
                for (unsigned short origin_square : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                    moves.emplace_back(origin_square, m_psquare);
            }

            // Blocks
            if (m_check_rays != 0)
                // Blocking with pawns (Can only block with non captures)
                for (unsigned short destination_square : getBitIndices(((m_white_pawns_bit & ~m_all_pins) << 8) & ~m_all_pieces_bit))
                {
                    if (((1ULL << destination_square) & m_check_rays) != 0)
                    // Single pawn moves
                    {
                        // Non promotions
                        if (destination_square < 56)
                            moves.emplace_back(destination_square - 8, destination_square);
                        // Promotions
                        else
                        {
                            moves.push_back(Move(destination_square - 8, destination_square, 1));
                            moves.push_back(Move(destination_square - 8, destination_square, 2));
                            moves.push_back(Move(destination_square - 8, destination_square, 3));
                            moves.push_back(Move(destination_square - 8, destination_square, 4));
                        }
                    }
                    else if (destination_square < 24 && ((1ULL << (destination_square + 8)) & m_check_rays) != 0)
                        // Double moves
                        moves.push_back(double_moves[destination_square - 8]);
                }
            // Blocking with knights
            for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & m_check_rays))
                    moves.emplace_back(origin_square, destination_square);
            }
            // Blocking with bishops
            for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & m_check_rays))
                    moves.emplace_back(origin_square, destination_square);
            }
            // Blocking with rooks
            for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & m_check_rays))
                    moves.emplace_back(origin_square, destination_square);
            }
            // Blocking with queens
            for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & m_check_rays))
                    moves.emplace_back(origin_square, destination_square);
            }
        }
    }
    else // Blacks turn
    {
        // Moving king
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_squares_attacked_by_white & ~m_black_pieces_bit))
        {
            if (BitPosition::kingIsSafeFromSliders(destination_square))
                moves.emplace_back(m_black_king_position, destination_square);
        }

        if (m_num_checks == 1)
        // We can only capture (with a piece that is not the king) or block if there is only one check
        {
            // Captures from square being captured
            unsigned short destination_square{getLeastSignificantBitIndex(checks_bit)};
            // Pawn captures
            if (destination_square > 7)
            {
                for (unsigned short origin_square : getBitIndices(precomputed_moves::white_pawn_attacks[destination_square] & m_black_pawns_bit & ~m_all_pins))
                    moves.emplace_back(origin_square, destination_square);
            }
            else
            {
                for (unsigned short origin_square : getBitIndices(precomputed_moves::white_pawn_attacks[destination_square] & m_black_pawns_bit & ~m_all_pins))
                {
                    moves.push_back(Move(origin_square, destination_square, 1));
                    moves.push_back(Move(origin_square, destination_square, 2));
                    moves.push_back(Move(origin_square, destination_square, 3));
                    moves.push_back(Move(origin_square, destination_square, 4));
                }
            }
            // Knight captures
            for (unsigned short origin_square : getBitIndices(precomputed_moves::knight_moves[destination_square] & m_black_knights_bit & ~m_all_pins))
                moves.emplace_back(origin_square, destination_square);
            // Bishop captures
            for (unsigned short origin_square : getBitIndices(BmagicNOMASK(destination_square, precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit) & m_black_bishops_bit & ~m_all_pins))
                moves.emplace_back(origin_square, destination_square);
            // Rook captures
            for (unsigned short origin_square : getBitIndices(RmagicNOMASK(destination_square, precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit) & m_black_rooks_bit & ~m_all_pins))
                moves.emplace_back(origin_square, destination_square);
            // Queen captures
            for (unsigned short origin_square : getBitIndices((BmagicNOMASK(destination_square, precomputed_moves::bishop_unfull_rays[destination_square] & m_all_pieces_bit) | RmagicNOMASK(destination_square, precomputed_moves::rook_unfull_rays[destination_square] & m_all_pieces_bit)) & m_black_queens_bit & ~m_all_pins))
                moves.emplace_back(origin_square, destination_square);

            // Passant capture (If pawn giving check can be en passant captured)
            if (m_psquare != 0 && (m_pawn_checks & (passant_bitboards[m_psquare] << 8)) != 0)
            {
                for (unsigned short origin_square : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                    moves.emplace_back(origin_square, m_psquare);
            }

            // Blocks
            if (m_check_rays != 0)
            {
                // Blocking with pawns (Can only block with non captures)
                // Pawn non captures
                for (unsigned short destination_square : getBitIndices(((m_black_pawns_bit & ~m_all_pins) >> 8) & ~m_all_pieces_bit))
                {
                    if (((1ULL << destination_square) & m_check_rays) != 0)
                    // Single pawn moves
                    {
                        // Non promotions
                        if (destination_square > 7)
                            moves.emplace_back(destination_square + 8, destination_square);
                        // Promotions
                        else
                        {
                            moves.push_back(Move(destination_square + 8, destination_square, 1));
                            moves.push_back(Move(destination_square + 8, destination_square, 2));
                            moves.push_back(Move(destination_square + 8, destination_square, 3));
                            moves.push_back(Move(destination_square + 8, destination_square, 4));
                        }
                    }
                    else if (destination_square > 39 && ((1ULL << (destination_square - 8)) & m_check_rays) != 0)
                        // Double moves
                        moves.push_back(double_moves[destination_square - 40]);
                }
                // Blocking with knights
                for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
                {
                    for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & m_check_rays))
                        moves.emplace_back(origin_square, destination_square);
                }
                // Blocking with bishops
                for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
                {
                    for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & m_check_rays))
                        moves.emplace_back(origin_square, destination_square);
                }
                // Blocking with rooks
                for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
                {
                    for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & m_check_rays))
                        moves.emplace_back(origin_square, destination_square);
                }
                // Blocking with queens
                for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
                {
                    for (unsigned short destination_square : getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & m_check_rays))
                        moves.emplace_back(origin_square, destination_square);
                }
            }
        }
    }
    return moves;
}
std::vector<Move> BitPosition::allMoves() const
// All possible moves, for normal alpha beta search
{
    std::vector<Move> moves;
    moves.reserve(128);
    if (m_turn) // White's turn
    {
        // Moving pieces that are not pinned

        // Knights
        for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & ~m_white_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Bishops
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_white_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Rooks
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_white_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Queens
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & ~m_white_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pawns non captures
        for (unsigned short destination_square : getBitIndices(((m_white_pawns_bit & ~m_all_pins) << 8) & ~m_all_pieces_bit))
        {
            // Non promotions
            if (destination_square < 56)
                moves.emplace_back(destination_square - 8, destination_square);
            // Promotions
            else
            {
                moves.push_back(Move(destination_square - 8, destination_square, 1));
                moves.push_back(Move(destination_square - 8, destination_square, 2));
                moves.push_back(Move(destination_square - 8, destination_square, 3));
                moves.push_back(Move(destination_square - 8, destination_square, 4));
            }
            // Double moves
            if (destination_square < 24 && ((1ULL << (destination_square + 8)) & m_all_pieces_bit) == 0)
                moves.push_back(double_moves[destination_square - 8]);
        }
        // Pawn captures
        for (unsigned short destination_square : getBitIndices((((m_white_pawns_bit & ~m_all_pins & 0b1111111011111110111111101111111011111110111111101111111011111110) << 7) & m_black_pieces_bit)))
        {
            if (destination_square > 55)
            // Promotions
            {
                moves.push_back(Move(destination_square - 7, destination_square, 1));
                moves.push_back(Move(destination_square - 7, destination_square, 2));
                moves.push_back(Move(destination_square - 7, destination_square, 3));
                moves.push_back(Move(destination_square - 7, destination_square, 4));
            }
            else
                moves.emplace_back(destination_square - 7, destination_square);
        }
        for (unsigned short destination_square : getBitIndices((((m_white_pawns_bit & ~m_all_pins & 0b0111111101111111011111110111111101111111011111110111111101111111) << 9) & m_black_pieces_bit)))
        {
            if (destination_square > 55)
            // Promotions
            {
                moves.push_back(Move(destination_square - 9, destination_square, 1));
                moves.push_back(Move(destination_square - 9, destination_square, 2));
                moves.push_back(Move(destination_square - 9, destination_square, 3));
                moves.push_back(Move(destination_square - 9, destination_square, 4));
            }
            else
                moves.emplace_back(destination_square - 9, destination_square);
        }
        // Passant capture
        if (m_psquare != 0)
        {
            if ((((1ULL << (m_psquare - 7)) & m_white_pawns_bit & 0b1111111011111110111111101111111011111110111111101111111011111110 & ~m_all_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare - 7, m_psquare - 8))
                moves.emplace_back(m_psquare - 7, m_psquare);

            if ((((1ULL << (m_psquare - 9)) & m_white_pawns_bit & 0b0111111101111111011111110111111101111111011111110111111101111111 & ~m_all_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare - 9, m_psquare - 8))
                moves.emplace_back(m_psquare - 9, m_psquare);
        }

        // King
        // Kingside castling
        if (m_white_kingside_castling && ((m_all_pieces_bit | m_all_squares_attacked_by_black) & 96) == 0)
            moves.push_back(castling_moves[0]);
        // Queenside castling
        if (m_white_queenside_castling && (m_all_pieces_bit & 14) == 0 && (m_all_squares_attacked_by_black & 12) == 0)
            moves.push_back(castling_moves[1]);

        // Normal king moves
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~(m_white_pieces_bit | m_all_squares_attacked_by_black)))
        {
            if (BitPosition::kingIsSafeFromSliders(destination_square))
                moves.emplace_back(m_white_king_position, destination_square);
        }

        // Moving pinned pieces (Note knights cannot be moved if pinned and kings cannot be pinned)
        // Pinned Bishops
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & m_diagonal_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Rooks
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & m_straight_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Queens
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & m_diagonal_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & m_straight_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Pawns
        for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & m_straight_pins))
        {
            uint64_t destination_bit{precomputed_moves::white_pawn_moves[origin_square] & m_straight_pins & ~m_all_pieces_bit};
            if (destination_bit != 0)
            {
                unsigned short destination_square{getLeastSignificantBitIndex(destination_bit)};
                {
                    moves.emplace_back(origin_square, destination_square);
                    if (origin_square < 16 && ((destination_bit << 8) & m_all_pieces_bit) == 0)
                        moves.push_back(double_moves[origin_square]);
                }
            }
        }
        // Capturing with pinned pawns
        for (unsigned short destination_square : getBitIndices((((m_white_pawns_bit & m_diagonal_pins & 0b1111111011111110111111101111111011111110111111101111111011111110) << 7) & m_black_pieces_bit & m_diagonal_pins)))
        {
            if (destination_square > 55)
            // Promotions
            {
                moves.push_back(Move(destination_square - 7, destination_square, 1));
                moves.push_back(Move(destination_square - 7, destination_square, 2));
                moves.push_back(Move(destination_square - 7, destination_square, 3));
                moves.push_back(Move(destination_square - 7, destination_square, 4));
            }
            else
                moves.emplace_back(destination_square - 7, destination_square);
        }
        for (unsigned short destination_square : getBitIndices((((m_white_pawns_bit & m_diagonal_pins & 0b0111111101111111011111110111111101111111011111110111111101111111) << 9) & m_black_pieces_bit & m_diagonal_pins)))
        {
            if (destination_square > 55)
            // Promotions
            {
                moves.push_back(Move(destination_square - 9, destination_square, 1));
                moves.push_back(Move(destination_square - 9, destination_square, 2));
                moves.push_back(Move(destination_square - 9, destination_square, 3));
                moves.push_back(Move(destination_square - 9, destination_square, 4));
            }
            else
                moves.emplace_back(destination_square - 9, destination_square);
        }
        // Passant capture
        if (m_psquare != 0)
        {
            if ((((1ULL << (m_psquare - 7)) & m_white_pawns_bit & 0b1111111011111110111111101111111011111110111111101111111011111110 & m_diagonal_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare - 8, m_psquare - 7))
                moves.emplace_back(m_psquare - 7, m_psquare);

            if ((((1ULL << (m_psquare - 9)) & m_white_pawns_bit & 0b0111111101111111011111110111111101111111011111110111111101111111 & m_diagonal_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare - 8, m_psquare - 9))
                moves.emplace_back(m_psquare - 9, m_psquare);
        }
    }
    else // Black's turn
    {
        // Moving pieces that are not pinned

        // Knights
        for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & ~m_black_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Bishops
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_black_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Rooks
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_black_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Queens
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & ~m_black_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pawn non captures
        for (unsigned short destination_square : getBitIndices(((m_black_pawns_bit & ~m_all_pins) >> 8) & ~m_all_pieces_bit))
        {
            // Non promotions
            if (destination_square > 7)
                moves.emplace_back(destination_square + 8, destination_square);
            // Promotions
            else
            {
                moves.push_back(Move(destination_square + 8, destination_square, 1));
                moves.push_back(Move(destination_square + 8, destination_square, 2));
                moves.push_back(Move(destination_square + 8, destination_square, 3));
                moves.push_back(Move(destination_square + 8, destination_square, 4));
            }
            // Double moves
            if (destination_square > 39 && ((1ULL << (destination_square - 8)) & m_all_pieces_bit) == 0)
                moves.push_back(double_moves[destination_square - 40]);
        }
        // Pawn captures
        for (unsigned short destination_square : getBitIndices(((m_black_pawns_bit & ~m_all_pins & 0b0111111101111111011111110111111101111111011111110111111101111111) >> 7) & m_white_pieces_bit))
        {
            if (destination_square < 8)
            // Promotions
            {
                moves.push_back(Move(destination_square + 7, destination_square, 1));
                moves.push_back(Move(destination_square + 7, destination_square, 2));
                moves.push_back(Move(destination_square + 7, destination_square, 3));
                moves.push_back(Move(destination_square + 7, destination_square, 4));
            }
            else
                moves.emplace_back(destination_square + 7, destination_square);
        }
        for (unsigned short destination_square : getBitIndices(((m_black_pawns_bit & ~m_all_pins & 0b1111111011111110111111101111111011111110111111101111111011111110) >> 9) & m_white_pieces_bit))
        {
            if (destination_square < 8)
            // Promotions
            {
                moves.push_back(Move(destination_square + 9, destination_square, 1));
                moves.push_back(Move(destination_square + 9, destination_square, 2));
                moves.push_back(Move(destination_square + 9, destination_square, 3));
                moves.push_back(Move(destination_square + 9, destination_square, 4));
            }
            else
                moves.emplace_back(destination_square + 9, destination_square);
        }
        // Passant capture
        if (m_psquare != 0)
        {
            if ((((1ULL << (m_psquare + 9)) & m_black_pawns_bit & 0b1111111011111110111111101111111011111110111111101111111011111110 & ~m_all_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare + 8, m_psquare + 9))
                moves.emplace_back(m_psquare + 9, m_psquare);

            if ((((1ULL << (m_psquare + 7)) & m_black_pawns_bit & 0b0111111101111111011111110111111101111111011111110111111101111111 & ~m_all_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare + 8, m_psquare + 7))
                moves.emplace_back(m_psquare + 7, m_psquare);
        }

        // King
        // Kingside castling
        if (m_black_kingside_castling && ((m_all_pieces_bit | m_all_squares_attacked_by_white) & 6917529027641081856) == 0)
            moves.push_back(castling_moves[2]);
        // Queenside castling
        if (m_black_queenside_castling && (m_all_pieces_bit & 1008806316530991104) == 0 && (m_all_squares_attacked_by_white & 864691128455135232) == 0)
            moves.push_back(castling_moves[3]);

        // Normal king moves
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~(m_black_pieces_bit | m_all_squares_attacked_by_white)))
        {
            moves.emplace_back(m_black_king_position, destination_square);
        }

        // Moving pinned pieces (Note knights cannot be moved if pinned and kings cannot be pinned)
        // Pinned Bishops
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & m_diagonal_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Rooks
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & m_straight_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Queens
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & m_diagonal_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & m_straight_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Pawns
        for (unsigned short origin_square : getBitIndices(m_black_pawns_bit & m_straight_pins))
        {
            uint64_t destination_bit{precomputed_moves::black_pawn_moves[origin_square] & m_straight_pins & ~m_all_pieces_bit};
            if (destination_bit != 0)
            {
                unsigned short destination_square{getLeastSignificantBitIndex(destination_bit)};
                {
                    moves.emplace_back(origin_square, destination_square);
                    if (origin_square > 47 && ((destination_bit >> 8) & m_all_pieces_bit) == 0)
                        moves.push_back(double_moves[origin_square - 48]);
                }
            }
        }

        // Pawn captures
        for (unsigned short destination_square : getBitIndices((((m_black_pawns_bit & m_diagonal_pins & 0b0111111101111111011111110111111101111111011111110111111101111111) >> 9) & m_white_pieces_bit & m_diagonal_pins)))
        {
            if (destination_square < 8)
            // Promotions
            {
                moves.push_back(Move(destination_square + 9, destination_square, 1));
                moves.push_back(Move(destination_square + 9, destination_square, 2));
                moves.push_back(Move(destination_square + 9, destination_square, 3));
                moves.push_back(Move(destination_square + 9, destination_square, 4));
            }
            else
                moves.emplace_back(destination_square + 9, destination_square);
        }
        for (unsigned short destination_square : getBitIndices((((m_black_pawns_bit & m_diagonal_pins & 0b1111111011111110111111101111111011111110111111101111111011111110) >> 7) & m_white_pieces_bit & m_diagonal_pins)))
        {
            if (destination_square < 8)
            // Promotions
            {
                moves.push_back(Move(destination_square + 7, destination_square, 1));
                moves.push_back(Move(destination_square + 7, destination_square, 2));
                moves.push_back(Move(destination_square + 7, destination_square, 3));
                moves.push_back(Move(destination_square + 7, destination_square, 4));
            }
            else
                moves.emplace_back(destination_square + 7, destination_square);
        }
        // Passant capture
        if (m_psquare != 0)
        {
            if ((((1ULL << (m_psquare + 9)) & m_black_pawns_bit & 0b1111111011111110111111101111111011111110111111101111111011111110 & m_diagonal_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare + 8, m_psquare + 9))
                moves.emplace_back(m_psquare + 9, m_psquare);

            if ((((1ULL << (m_psquare + 7)) & m_black_pawns_bit & 0b0111111101111111011111110111111101111111011111110111111101111111 & m_diagonal_pins) != 0) && BitPosition::kingIsSafeAfterPassant(m_psquare + 8, m_psquare + 7))
                moves.emplace_back(m_psquare + 7, m_psquare);
        }
    }
    return moves;
}
std::vector<Move> BitPosition::orderAllMoves(std::vector<Move> &moves, Move ttMove) const
{
    std::vector<std::pair<Move, int>> moves_and_scores;
    moves_and_scores.reserve(moves.size()); // Reserve space to avoid reallocations
    if (m_turn)                             // Whites turn
    {
        for (Move move : moves)
        {
            if (move.getData() == ttMove.getData() && move.getData() != 0)
            {
                moves_and_scores.emplace_back(move, 63);
            }
            else
            {
                unsigned short origin_square{move.getOriginSquare()};
                uint64_t origin_bit{(1ULL << origin_square)};
                unsigned short destination_square{move.getDestinationSquare()};
                uint64_t destination_bit{(1ULL << destination_square)};
                int score{0};
                // Capture score
                if ((destination_bit & m_black_pieces_bit) != 0)
                {
                    if ((destination_bit & m_black_pawns_bit) != 0)
                        score += 1;
                    else if ((destination_bit & m_black_knights_bit) != 0)
                        score += 2;
                    else if ((destination_bit & m_black_bishops_bit) != 0)
                        score += 3;
                    else if ((destination_bit & m_black_rooks_bit) != 0)
                        score += 4;
                    else
                        score += 5;
                }
                // Move safety
                if ((origin_bit & m_white_knights_bit) != 0) // Moving knight
                {
                    if ((destination_bit & ~m_squares_attacked_by_black_pawns) == 0) // Unsafe
                        score -= 2;
                }
                else if ((origin_bit & m_white_bishops_bit) != 0) // Moving bishop
                {
                    if ((destination_bit & ~m_squares_attacked_by_black_pawns) == 0) // Unsafe
                        score -= 2;
                }
                else if ((origin_bit & m_white_rooks_bit) != 0) // Moving rook
                {
                    if ((destination_bit & ~m_squares_attacked_by_black_pawns & ~m_squares_attacked_by_black_knights & ~m_squares_attacked_by_black_bishops) == 0) // Unsafe
                        score -= 3;
                }
                else if ((origin_bit & m_white_queens_bit) != 0) // Moving queen
                {
                    if ((destination_bit & ~m_squares_attacked_by_black_pawns & ~m_squares_attacked_by_black_knights & ~m_squares_attacked_by_black_bishops & ~m_squares_attacked_by_black_rooks) == 0) // Unsafe
                        score -= 3;
                }
                moves_and_scores.emplace_back(move, score);
            }
        }
    }
    else // Blacks turn
    {
        for (Move move : moves)
        {
            if (move.getData() == ttMove.getData() && move.getData() != 0)
            {
                moves_and_scores.emplace_back(move, 63);
            }
            else
            {
                unsigned short origin_square{move.getOriginSquare()};
                uint64_t origin_bit{(1ULL << origin_square)};
                unsigned short destination_square{move.getDestinationSquare()};
                uint64_t destination_bit{(1ULL << destination_square)};
                int score{0};
                // Capture score
                if ((destination_bit & m_white_pieces_bit) != 0)
                {
                    if ((destination_bit & m_white_pawns_bit) != 0)
                        score += 1;
                    else if ((destination_bit & m_white_knights_bit) != 0)
                        score += 2;
                    else if ((destination_bit & m_white_bishops_bit) != 0)
                        score += 3;
                    else if ((destination_bit & m_white_rooks_bit) != 0)
                        score += 4;
                    else
                        score += 5;
                }
                // Move safety
                if ((origin_bit & m_black_knights_bit) != 0) // Moving knight
                {
                    if ((destination_bit & ~m_squares_attacked_by_white_pawns) == 0) // Unsafe
                        score -= 2;
                }
                else if ((origin_bit & m_black_bishops_bit) != 0) // Moving bishop
                {
                    if ((destination_bit & ~m_squares_attacked_by_white_pawns) == 0) // Unsafe
                        score -= 2;
                }
                else if ((origin_bit & m_black_rooks_bit) != 0) // Moving rook
                {
                    if ((destination_bit & ~m_squares_attacked_by_white_pawns & ~m_squares_attacked_by_white_knights & ~m_squares_attacked_by_white_bishops) == 0) // Unsafe
                        score -= 3;
                }
                else if ((origin_bit & m_black_queens_bit) != 0) // Moving queen
                {
                    if ((destination_bit & ~m_squares_attacked_by_white_pawns & ~m_squares_attacked_by_white_knights & ~m_squares_attacked_by_white_bishops & ~m_squares_attacked_by_white_rooks) == 0) // Unsafe
                        score -= 3;
                }
                moves_and_scores.emplace_back(move, score);
            }
        }
    }
    // Order moves
    // Sort the pair vector by scores, highest first
    std::sort(moves_and_scores.begin(), moves_and_scores.end(),
              [](const std::pair<Move, int> &a, const std::pair<Move, int> &b)
              {
                  return a.second > b.second; // Sort by score in descending order
              });

    // Clear the original moves vector and repopulate it with sorted moves
    moves.clear();                          // Clear the input vector if modifying it directly
    moves.reserve(moves_and_scores.size()); // Reserve space to avoid reallocations

    // Unpack the sorted moves into the vector
    for (const auto &move_score : moves_and_scores)
    {
        moves.push_back(move_score.first); // Note: first is the Move
    }

    return moves; // Return the sorted moves
}

std::vector<Move> BitPosition::orderAllMovesOnFirstIterationFirstTime(std::vector<Move> &moves, Move ttMove) const
// This is the same as orderAllMoves but we have a ttMove which is the best move found at a better depth but incomplete search due to cutoff.
{
    std::vector<std::pair<Move, int>> moves_and_scores;
    moves_and_scores.reserve(moves.size()); // Reserve space to avoid reallocations

    if (m_turn) // Whites turn
    {
        for (Move move : moves)
        {
            // If move was found on ttable at a better depth to be the best move (although the search might have been cutoff, so we can't blindly use this move)
            if (move.getData() == ttMove.getData() && move.getData() != 0)
            {
                moves_and_scores.emplace_back(move, 62);
            }
            else
            {
                unsigned short origin_square{move.getOriginSquare()};
                uint64_t origin_bit{(1ULL << origin_square)};
                unsigned short destination_square{move.getDestinationSquare()};
                uint64_t destination_bit{(1ULL << destination_square)};
                int score{0};
                // Capture score
                if ((destination_bit & m_black_pieces_bit) != 0)
                {
                    if ((destination_bit & m_black_pawns_bit) != 0)
                        score += 1;
                    else if ((destination_bit & m_black_knights_bit) != 0)
                        score += 2;
                    else if ((destination_bit & m_black_bishops_bit) != 0)
                        score += 3;
                    else if ((destination_bit & m_black_rooks_bit) != 0)
                        score += 4;
                    else
                        score += 5;
                }
                // Move safety
                if ((origin_bit & m_white_knights_bit) != 0) // Moving knight
                {
                    if ((destination_bit & ~m_squares_attacked_by_black_pawns) == 0) // Unsafe
                        score -= 2;
                }
                else if ((origin_bit & m_white_bishops_bit) != 0) // Moving bishop
                {
                    if ((destination_bit & ~m_squares_attacked_by_black_pawns) == 0) // Unsafe
                        score -= 2;
                }
                else if ((origin_bit & m_white_rooks_bit) != 0) // Moving rook
                {
                    if ((destination_bit & ~m_squares_attacked_by_black_pawns & ~m_squares_attacked_by_black_knights & ~m_squares_attacked_by_black_bishops) == 0) // Unsafe
                        score -= 3;
                }
                else if ((origin_bit & m_white_queens_bit) != 0) // Moving queen
                {
                    if ((destination_bit & ~m_squares_attacked_by_black_pawns & ~m_squares_attacked_by_black_knights & ~m_squares_attacked_by_black_bishops & ~m_squares_attacked_by_black_rooks) == 0) // Unsafe
                        score -= 3;
                }
                moves_and_scores.emplace_back(move, score);
            }
        }
    }
    else // Blacks turn
    {
        for (Move move : moves)
        {
            if (move.getData() == ttMove.getData() && move.getData() != 0)
            {
                moves_and_scores.emplace_back(move, 62);
            }
            else
            {
                unsigned short origin_square{move.getOriginSquare()};
                uint64_t origin_bit{(1ULL << origin_square)};
                unsigned short destination_square{move.getDestinationSquare()};
                uint64_t destination_bit{(1ULL << destination_square)};
                int score{0};
                // Capture score
                if ((destination_bit & m_white_pieces_bit) != 0)
                {
                    if ((destination_bit & m_white_pawns_bit) != 0)
                        score += 1;
                    else if ((destination_bit & m_white_knights_bit) != 0)
                        score += 2;
                    else if ((destination_bit & m_white_bishops_bit) != 0)
                        score += 3;
                    else if ((destination_bit & m_white_rooks_bit) != 0)
                        score += 4;
                    else
                        score += 5;
                }
                // Move safety
                if ((origin_bit & m_black_knights_bit) != 0) // Moving knight
                {
                    if ((destination_bit & ~m_squares_attacked_by_white_pawns) == 0) // Unsafe
                        score -= 2;
                }
                else if ((origin_bit & m_black_bishops_bit) != 0) // Moving bishop
                {
                    if ((destination_bit & ~m_squares_attacked_by_white_pawns) == 0) // Unsafe
                        score -= 2;
                }
                else if ((origin_bit & m_black_rooks_bit) != 0) // Moving rook
                {
                    if ((destination_bit & ~m_squares_attacked_by_white_pawns & ~m_squares_attacked_by_white_knights & ~m_squares_attacked_by_white_bishops) == 0) // Unsafe
                        score -= 3;
                }
                else if ((origin_bit & m_black_queens_bit) != 0) // Moving queen
                {
                    if ((destination_bit & ~m_squares_attacked_by_white_pawns & ~m_squares_attacked_by_white_knights & ~m_squares_attacked_by_white_bishops & ~m_squares_attacked_by_white_rooks) == 0) // Unsafe
                        score -= 3;
                }
                moves_and_scores.emplace_back(move, score);
            }
        }
    }
    // Order moves
    // Sort the pair vector by scores, highest first
    std::sort(moves_and_scores.begin(), moves_and_scores.end(),
              [](const std::pair<Move, int> &a, const std::pair<Move, int> &b)
              {
                  return a.second > b.second; // Sort by score in descending order
              });

    // Clear the original moves vector and repopulate it with sorted moves
    moves.clear();                          // Clear the input vector if modifying it directly
    moves.reserve(moves_and_scores.size()); // Reserve space to avoid reallocations

    // Unpack the sorted moves into the vector
    for (const auto &move_score : moves_and_scores)
    {
        moves.push_back(move_score.first); // Note: first is the Move
    }

    return moves; // Return the sorted moves
}

std::pair<std::vector<Move>, std::vector<int16_t>> BitPosition::orderAllMovesOnFirstIteration(std::vector<Move> &moves, std::vector<int16_t> &scores) const
{
    // Create a vector of indices
    std::vector<int> indices(moves.size());
    for (int i = 0; i < indices.size(); ++i)
    {
        indices[i] = i;
    }

    // Sort the indices based on the scores in descending order
    std::sort(indices.begin(), indices.end(), [&scores](int a, int b)
              { return scores[a] > scores[b]; });

    // Create a new vector for the sorted moves
    std::vector<Move> sortedMoves;
    std::vector<int16_t> sortedScores;
    sortedMoves.reserve(moves.size());
    sortedScores.reserve(moves.size());
    for (int index : indices)
    {
        sortedMoves.push_back(moves[index]);
        sortedScores.push_back(scores[index]);
    }

    return std::pair<std::vector<Move>, std::vector<int16_t>>{sortedMoves, sortedScores}; // Return the sorted moves
}

void BitPosition::setPiece()
// Move a piece (except king), given an origin bit and destination bit
{
    if (m_turn)
    {
        // array of pointers to our pieces
        std::array<uint64_t *, 5> piece_arrays{&m_white_pawns_bit, &m_white_knights_bit, &m_white_bishops_bit, &m_white_rooks_bit, &m_white_queens_bit};
        int count{0};
        for (auto &piece_bit : piece_arrays)
        {
            if ((m_last_origin_bit & *piece_bit) != 0)
            {
                *piece_bit &= ~m_last_origin_bit;
                *piece_bit |= m_last_destination_bit;
                m_moved_piece = count;
                break;
            }
            count++;
        }
    }
    else
    {
        // array of pointers to our pieces
        std::array<uint64_t *, 5> piece_arrays{&m_black_pawns_bit, &m_black_knights_bit, &m_black_bishops_bit, &m_black_rooks_bit, &m_black_queens_bit};
        int count{0};
        for (auto &piece_bit : piece_arrays)
        {
            if ((m_last_origin_bit & *piece_bit) != 0)
            {
                *piece_bit &= ~m_last_origin_bit;
                *piece_bit |= m_last_destination_bit;
                m_moved_piece = count;
                break;
            }
            count++;
        }
    }
}

void BitPosition::makeNormalMove(Move move)
// Move piece and switch white and black roles, without rotating the board.
// After making move, set pins and checks to 0.
{
    BitPosition::storePlyInfo(); // store current state for unmake move

    unsigned short origin_square{move.getOriginSquare()};
    m_last_origin_bit = (1ULL << origin_square);
    unsigned short destination_square{move.getDestinationSquare()};
    m_last_destination_bit = (1ULL << destination_square);
    m_captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion

    if (m_turn) // White's move
    {
        if (origin_square == 0) // Moving from a1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (origin_square == 7) // Moving from h1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (destination_square == 63) // Capturing on a8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (destination_square == 56) // Capturing on h8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_origin_bit == m_white_king_bit) // Moving king
        {
            // Remove castling zobrist part
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            m_white_kingside_castling = false;
            m_white_queenside_castling = false;

            // Set new castling zobrist part
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            // Update king bit and king position
            m_white_king_bit = m_last_destination_bit;
            m_white_king_position = destination_square;
            m_moved_piece = 5;

            // Update NNUE input (after moving the king)
            NNUE::moveWhiteKingNNUEInput(*this);
        }
        // Moving any piece except king
        else
        {
            BitPosition::setPiece();
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * m_moved_piece + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * m_moved_piece + destination_square);
        }

        // Special Moves (4 types)
        unsigned short psquare{0};

        // 1) Captures (Non passant)
        if ((m_last_destination_bit & m_black_pieces_bit) != 0)
        {
            BitPosition::removePiece();
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * (m_captured_piece + 5) + destination_square);
        }
        // 2) Double pawn moves
        else if ((move.getData() & 49152) == 32768) // 1000000000000000
            psquare = origin_square + 8;

        // 3) Passant Captures
        else if (m_moved_piece == 0 && (m_last_destination_bit == passant_bitboards[m_psquare]))
        {
            // Remove captured piece
            m_black_pawns_bit &= ~(1ULL << (m_psquare - 8));
            m_captured_piece = 0;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_psquare - 8);
        }
        // 4) Promotions and Castling
        if ((move.getData() & 0b1100000000000000) == 0b0100000000000000) // 0100000000000000
        {
            if (move.getData() == 16772) // White kingside castling
            {
                m_white_rooks_bit &= ~128;
                m_white_rooks_bit |= 32;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 7);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 5);
            }
            else if (move.getData() == 16516) // White queenside castling
            {
                m_white_rooks_bit &= ~1;
                m_white_rooks_bit |= 8;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 3);
            }
            else // Promotions
            {
                m_white_pawns_bit &= ~m_last_destination_bit;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square);

                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};
                if (promoting_piece == 12288) // Queen promotion
                {
                    m_promoted_piece = 4;
                    m_white_queens_bit |= m_last_destination_bit;
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                }
                else if (promoting_piece == 8192) // Rook promotion
                {
                    m_promoted_piece = 3;
                    m_white_rooks_bit |= m_last_destination_bit;
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                }
                else if (promoting_piece == 4096) // Bishop promotion
                {
                    m_promoted_piece = 2;
                    m_white_bishops_bit |= m_last_destination_bit;
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                }
                else // Knight promotion
                {
                    m_promoted_piece = 1;
                    m_white_knights_bit |= m_last_destination_bit;
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                }
            }
        }
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
        m_psquare = psquare;
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
    }
    else // Black's move
    {
        if (origin_square == 56) // Moving from a8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (origin_square == 63) // Moving from h8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (destination_square == 0) // Capturing on a1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (destination_square == 7) // Capturing on h1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_origin_bit == m_black_king_bit) // Moving king
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            m_black_king_bit = m_last_destination_bit;
            m_black_king_position = destination_square;
            m_moved_piece = 5;

            // Update NNUE input (after moving the king)
            NNUE::moveBlackKingNNUEInput(*this);
        }
        // Moving any piece except the king
        else
        {
            BitPosition::setPiece();
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * (5 + m_moved_piece) + origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * (5 + m_moved_piece) + destination_square);
        }

        // Special Moves (4 types)
        unsigned short psquare{0};

        // 1) Captures (Non passant)
        if ((m_last_destination_bit & m_white_pieces_bit) != 0) // 1100000000000000
        {
            BitPosition::removePiece();
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * m_captured_piece + destination_square);
        }

        // 2) Double pawn moves
        else if ((move.getData() & 49152) == 32768) // 1000000000000000
            psquare = origin_square - 8;

        // 3) Passant Captures
        else if (m_moved_piece == 0 && (m_last_destination_bit == passant_bitboards[m_psquare])) // 1100000000000000
        {
            // Remove captured piece
            m_white_pawns_bit &= ~(1ULL << (m_psquare + 8));
            m_captured_piece = 0;

            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_psquare + 8);
        }
        // 4) Promotions and Castling
        if ((move.getData() & 0b1100000000000000) == 0b0100000000000000) // 0100000000000000
        {
            if (move.getData() == 20412) // Black kingside castling
            {
                m_black_rooks_bit &= ~9223372036854775808ULL;
                m_black_rooks_bit |= 2305843009213693952ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 63);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 61);
            }
            else if (move.getData() == 20156) // Black queenside castling
            {
                m_black_rooks_bit &= ~72057594037927936ULL;
                m_black_rooks_bit |= 576460752303423488ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 56);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 59);
            }
            else // Promotions
            {
                m_black_pawns_bit &= ~m_last_destination_bit;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);

                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};
                if (promoting_piece == 12288) // Queen promotion
                {
                    m_promoted_piece = 4;
                    m_black_queens_bit |= m_last_destination_bit;
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                }
                else if (promoting_piece == 8192) // Rook promotion
                {
                    m_promoted_piece = 3;
                    m_black_rooks_bit |= m_last_destination_bit;
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                }
                else if (promoting_piece == 4096) // Bishop promotion
                {
                    m_promoted_piece = 2;
                    m_black_bishops_bit |= m_last_destination_bit;
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                }
                else // Knight promotion
                {
                    m_promoted_piece = 1;
                    m_black_knights_bit |= m_last_destination_bit;
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                }
            }
        }
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
        m_psquare = psquare;
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
    }
    m_turn = not m_turn;
    BitPosition::updateZobristKeyPiecePartAfterMove(origin_square, destination_square);
    m_zobrist_key ^= zobrist_keys::blackToMoveZobristNumber;
    // Note, we store the zobrist key in it's array when making the move. However the rest of the ply info
    // is stored when making the next move to be able to go back.
    // So we store it in the m_ply+1 position because the initial position (or position after capture) is the m_ply 0.
    m_zobrist_keys_array[m_ply + 1] = m_zobrist_key;

    m_captured_piece_array[m_ply] = m_captured_piece;

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

void BitPosition::unmakeNormalMove(Move move)
// Takes a move and undoes the move accordingly, updating all position attributes. When the engine transverses the tree of moves it will keep
// track of some irreversible aspects of the game at each ply.These are(white castling rights, black castling rights, passant square,
// capture index, pins, checks).
{
    m_ply--;

    // Update irreversible aspects
    m_white_kingside_castling = m_wkcastling_array[m_ply];
    m_white_queenside_castling = m_wqcastling_array[m_ply];
    m_black_kingside_castling = m_bkcastling_array[m_ply];
    m_black_queenside_castling = m_bqcastling_array[m_ply];
    m_psquare = m_psquare_array[m_ply];

    // Get irreversible info
    unsigned short previous_captured_piece{m_captured_piece_array[m_ply]};

    // Update aspects to not recompute
    m_diagonal_pins = m_diagonal_pins_array[m_ply];
    m_straight_pins = m_straight_pins_array[m_ply];
    m_is_check = m_is_check_array[m_ply];

    m_squares_attacked_by_white_pawns = m_squares_attacked_by_white_pawns_array[m_ply];
    m_squares_attacked_by_black_pawns = m_squares_attacked_by_black_pawns_array[m_ply];
    m_squares_attacked_by_white_knights = m_squares_attacked_by_white_knights_array[m_ply];
    m_squares_attacked_by_black_knights = m_squares_attacked_by_black_knights_array[m_ply];
    m_squares_attacked_by_white_bishops = m_squares_attacked_by_white_bishops_array[m_ply];
    m_squares_attacked_by_black_bishops = m_squares_attacked_by_black_bishops_array[m_ply];
    m_squares_attacked_by_white_rooks = m_squares_attacked_by_white_rooks_array[m_ply];
    m_squares_attacked_by_black_rooks = m_squares_attacked_by_black_rooks_array[m_ply];
    m_squares_attacked_by_white_queens = m_squares_attacked_by_white_queens_array[m_ply];
    m_squares_attacked_by_black_queens = m_squares_attacked_by_black_queens_array[m_ply];
    m_squares_attacked_by_white_king = m_squares_attacked_by_white_king_array[m_ply];
    m_squares_attacked_by_black_king = m_squares_attacked_by_black_king_array[m_ply];

    m_all_squares_attacked_by_white = m_all_squares_attacked_by_white_array[m_ply];
    m_all_squares_attacked_by_black = m_all_squares_attacked_by_black_array[m_ply];

    m_zobrist_key = m_zobrist_keys_array[m_ply];
    m_zobrist_keys_array[m_ply + 1] = 0;

    // Get move info
    unsigned short origin_square{move.getOriginSquare()};
    m_last_origin_bit = (1ULL << origin_square);
    unsigned short destination_square{move.getDestinationSquare()};
    m_last_destination_bit = (1ULL << destination_square);

    if (m_turn) // Last move was black
    {
        // Special moves
        if ((move.getData() & 0b1100000000000000) == 0b0100000000000000)
        {
            // Unmake kingside castling
            if (move.getData() == 20412)
            {
                m_black_king_bit = (1ULL << 60);
                m_black_rooks_bit |= (1ULL << 63);
                m_black_rooks_bit &= ~(1ULL << 61);
                m_black_king_position = 60;

                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 63);
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 61);
                NNUE::moveBlackKingNNUEInput(*this);
            }

            // Unmake queenside castling
            else if (move.getData() == 20156)
            {
                m_black_king_bit = (1ULL << 60);
                m_black_rooks_bit |= (1ULL << 56);
                m_black_rooks_bit &= ~(1ULL << 59);
                m_black_king_position = 60;

                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 56);
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 59);
                NNUE::moveBlackKingNNUEInput(*this);
            }

            // Unmaking black promotions
            else
            {
                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};

                m_black_pawns_bit |= m_last_origin_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);

                if (promoting_piece == 12288) // Unpromote queen
                {
                    m_black_queens_bit &= ~m_last_destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                }
                else if (promoting_piece == 8192) // Unpromote rook
                {
                    m_black_rooks_bit &= ~m_last_destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                }
                else if (promoting_piece == 4096) // Unpromote bishop
                {
                    m_black_bishops_bit &= ~m_last_destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                }
                else // Unpromote knight
                {
                    m_black_knights_bit &= ~m_last_destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                }
                // Unmaking captures in promotions
                if (previous_captured_piece != 7)
                {
                    if (previous_captured_piece == 1) // Uncapture knight
                    {
                        m_white_knights_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                    }
                    else if (previous_captured_piece == 2) // Uncapture bishop
                    {
                        m_white_bishops_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                    }
                    else if (previous_captured_piece == 3) // Uncapture rook
                    {
                        m_white_rooks_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                    }
                    else // Uncapture queen
                    {
                        m_white_queens_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                    }
                }
            }
        }

        // Non special moves
        else
        {
            if ((m_last_destination_bit & m_black_pawns_bit) != 0) // Unmove pawn
            {
                m_black_pawns_bit |= m_last_origin_bit;
                m_black_pawns_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);
            }
            else if ((m_last_destination_bit & m_black_knights_bit) != 0) // Unmove knight
            {
                m_black_knights_bit |= m_last_origin_bit;
                m_black_knights_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + origin_square);
            }
            else if ((m_last_destination_bit & m_black_bishops_bit) != 0) // Unmove bishop
            {
                m_black_bishops_bit |= m_last_origin_bit;
                m_black_bishops_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + origin_square);
            }
            else if ((m_last_destination_bit & m_black_rooks_bit) != 0) // Unmove rook
            {
                m_black_rooks_bit |= m_last_origin_bit;
                m_black_rooks_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + origin_square);
            }
            else if ((m_last_destination_bit & m_black_queens_bit) != 0) // Unmove queen
            {
                m_black_queens_bit |= m_last_origin_bit;
                m_black_queens_bit &= ~m_last_destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + origin_square);
            }
            else // Unmove king
            {
                m_black_king_bit = m_last_origin_bit;
                m_black_king_position = origin_square;
                NNUE::moveBlackKingNNUEInput(*this);
            }

            // Unmaking captures
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 0) // Uncapture pawn
                {
                    // Unmake passant capture
                    if (destination_square == m_psquare)
                    {
                        m_white_pawns_bit |= (1ULL << (m_psquare + 8));
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, m_psquare + 8);
                    }
                    // Unmake normal pawn capture
                    else
                    {
                        m_white_pawns_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, destination_square);
                    }
                }
                else if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_white_knights_bit |= m_last_destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_white_bishops_bit |= m_last_destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_white_rooks_bit |= m_last_destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                }
                else // Uncapture queen
                {
                    m_white_queens_bit |= m_last_destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                }
            }
        }
    }
    else // Last move was white
    {
        // Special moves
        if ((move.getData() & 0b1100000000000000) == 0b0100000000000000)
        {
            // Unmake kingside castling
            if (move.getData() == 16772)
            {
                m_white_king_bit = (1ULL << 4);
                m_white_rooks_bit |= (1ULL << 7);
                m_white_rooks_bit &= ~(1ULL << 5);
                m_white_king_position = 4;

                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 7);
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 5);
                NNUE::moveWhiteKingNNUEInput(*this);
            }

            // Unmake queenside castling
            else if (move.getData() == 16516)
            {
                m_white_king_bit = (1ULL << 4);
                m_white_rooks_bit |= 1ULL;
                m_white_rooks_bit &= ~(1ULL << 3);
                m_white_king_position = 4;

                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3);
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 3);
                NNUE::moveWhiteKingNNUEInput(*this);
            }

            // Unmaking promotions
            else
            {
                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};

                m_white_pawns_bit |= m_last_origin_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);

                if (promoting_piece == 12288) // Unpromote queen
                {
                    m_white_queens_bit &= ~m_last_destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                }
                else if (promoting_piece == 8192) // Unpromote rook
                {
                    m_white_rooks_bit &= ~m_last_destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                }
                else if (promoting_piece == 4096) // Unpromote bishop
                {
                    m_white_bishops_bit &= ~m_last_destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                }
                else // Unpromote knight
                {
                    m_white_knights_bit &= ~m_last_destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                }
                // Unmaking captures in promotions
                if (previous_captured_piece != 7)
                {
                    if (previous_captured_piece == 1) // Uncapture knight
                    {
                        m_black_knights_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                    }
                    else if (previous_captured_piece == 2) // Uncapture bishop
                    {
                        m_black_bishops_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                    }
                    else if (previous_captured_piece == 3) // Uncapture rook
                    {
                        m_black_rooks_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                    }
                    else // Uncapture queen
                    {
                        m_black_queens_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                    }
                }
            }
        }

        // Non special moves
        else
        {
            if ((m_last_destination_bit & m_white_pawns_bit) != 0) // Unmove pawn
            {
                m_white_pawns_bit |= m_last_origin_bit;
                m_white_pawns_bit &= ~m_last_destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);
            }
            else if ((m_last_destination_bit & m_white_knights_bit) != 0) // Unmove knight
            {
                m_white_knights_bit |= m_last_origin_bit;
                m_white_knights_bit &= ~m_last_destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + origin_square);
            }
            else if ((m_last_destination_bit & m_white_bishops_bit) != 0) // Unmove bishop
            {
                m_white_bishops_bit |= m_last_origin_bit;
                m_white_bishops_bit &= ~m_last_destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + origin_square);
            }
            else if ((m_last_destination_bit & m_white_rooks_bit) != 0) // Unmove rook
            {
                m_white_rooks_bit |= m_last_origin_bit;
                m_white_rooks_bit &= ~m_last_destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + origin_square);
            }
            else if ((m_last_destination_bit & m_white_queens_bit) != 0) // Unmove queen
            {
                m_white_queens_bit |= m_last_origin_bit;
                m_white_queens_bit &= ~m_last_destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + origin_square);
            }
            else // Unmove king
            {
                m_white_king_bit = m_last_origin_bit;
                m_white_king_position = origin_square;
                NNUE::moveWhiteKingNNUEInput(*this);
            }

            // Unmaking captures
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 0) // Uncapture pawn
                {
                    // Unmake passant capture
                    if (destination_square == m_psquare)
                    {
                        m_black_pawns_bit |= (1ULL << (m_psquare - 8));
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_psquare - 8);
                    }
                    // Unmake normal pawn capture
                    else
                    {
                        m_black_pawns_bit |= m_last_destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                    }
                }
                else if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_black_knights_bit |= m_last_destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_black_bishops_bit |= m_last_destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_black_rooks_bit |= m_last_destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                }
                else // Uncapture queen
                {
                    m_black_queens_bit |= m_last_destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                }
            }
        }
    }
    BitPosition::setAllPiecesBits();
    m_turn = not m_turn;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The Folowing member functions will not be used in the engine. They are only used for debugging purposes.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Move> BitPosition::nonCaptureMoves() const
// This will be a generator which when checking if move is a check it yields a move with score 1, else 0.
{
    std::vector<Move> moves;
    moves.reserve(128);
    if (m_turn) // White's turn
    {
        // Moving pieces that are not pinned

        // Knights
        for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & ~m_all_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Bishops
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Rooks
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Queens
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & ~m_all_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pawns
        for (unsigned short destination_square : getBitIndices(((m_white_pawns_bit & ~m_all_pins) << 8) & ~m_all_pieces_bit))
        {
            // Non promotions
            if (destination_square < 56)
                moves.emplace_back(destination_square - 8, destination_square);
            // Promotions
            else
            {
                moves.push_back(Move(destination_square - 8, destination_square, 1));
                moves.push_back(Move(destination_square - 8, destination_square, 2));
                moves.push_back(Move(destination_square - 8, destination_square, 3));
                moves.push_back(Move(destination_square - 8, destination_square, 4));
            }
            // Double moves
            if (destination_square < 24 && ((1ULL << (destination_square + 8)) & m_all_pieces_bit) == 0)
                moves.push_back(double_moves[destination_square - 8]);
        }

        // King
        // Kingside castling
        if (m_white_kingside_castling && ((m_all_pieces_bit | m_all_squares_attacked_by_black) & 96) == 0)
            moves.push_back(castling_moves[0]);
        // Queenside castling
        if (m_white_queenside_castling && (m_all_pieces_bit & 14) == 0 && (m_all_squares_attacked_by_black & 12) == 0)
            moves.push_back(castling_moves[1]);
        // Normal king moves
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~(m_all_pieces_bit | m_all_squares_attacked_by_black)))
        {
            moves.emplace_back(m_white_king_position, destination_square);
        }

        // Moving pinned pieces (Note knights cannot be moved if pinned and kings cannot be pinned)
        // Pinned Bishops
        for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & m_diagonal_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Rooks
        for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit & m_straight_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Queens
        for (unsigned short origin_square : getBitIndices(m_white_queens_bit & m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square);
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit & m_straight_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Pawns
        for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::white_pawn_moves[origin_square] & ~m_all_pieces_bit & m_straight_pins))
            {
                moves.emplace_back(origin_square, destination_square);
                if (origin_square < 16 && (precomputed_moves::white_pawn_doubles[origin_square] & m_all_pieces_bit) == 0)
                    moves.push_back(double_moves[origin_square]);
            }
        }
    }
    else // Black's turn
    {
        // Moving pieces that are not pinned

        // Knights
        for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & ~m_all_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Bishops
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Rooks
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Queens
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & ~m_all_pieces_bit))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pawns
        for (unsigned short destination_square : getBitIndices(((m_black_pawns_bit & ~m_all_pins) >> 8) & ~m_all_pieces_bit))
        {
            // Non promotions
            if (destination_square > 7)
                moves.emplace_back(destination_square + 8, destination_square);
            // Promotions
            else
            {
                moves.push_back(Move(destination_square + 8, destination_square, 1));
                moves.push_back(Move(destination_square + 8, destination_square, 2));
                moves.push_back(Move(destination_square + 8, destination_square, 3));
                moves.push_back(Move(destination_square + 8, destination_square, 4));
            }
            // Double moves
            if (destination_square > 39 && ((1ULL << (destination_square - 8)) & m_all_pieces_bit) == 0)
                moves.push_back(double_moves[destination_square - 40]);
        }

        // King
        // Kingside castling
        if (m_black_kingside_castling && ((m_all_pieces_bit | m_all_squares_attacked_by_white) & 6917529027641081856) == 0)
            moves.push_back(castling_moves[2]);
        // Queenside castling
        if (m_black_queenside_castling && (m_all_pieces_bit & 1008806316530991104) == 0 && (m_all_squares_attacked_by_white & 864691128455135232) == 0)
            moves.push_back(castling_moves[3]);
        // Normal king moves
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~(m_all_pieces_bit | m_all_squares_attacked_by_white)))
        {
            moves.emplace_back(m_black_king_position, destination_square);
        }

        // Moving pinned pieces (Note knights cannot be moved if pinned and kings cannot be pinned)
        // Pinned Bishops
        for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & m_diagonal_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Rooks
        for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit & m_straight_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Queens
        for (unsigned short origin_square : getBitIndices(m_black_queens_bit & m_all_pins))
        {
            for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit & m_diagonal_pins))
                moves.emplace_back(origin_square, destination_square);
            for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & ~m_all_pieces_bit & m_straight_pins))
                moves.emplace_back(origin_square, destination_square);
        }
        // Pinned Pawns
        for (unsigned short origin_square : getBitIndices(m_black_pawns_bit & m_straight_pins))
        {
            for (unsigned short destination_square : getBitIndices(precomputed_moves::black_pawn_moves[origin_square] & ~m_all_pieces_bit & m_straight_pins))
            {
                moves.push_back(Move(origin_square, destination_square));
                if (origin_square > 47 && (precomputed_moves::black_pawn_doubles[origin_square] & m_all_pieces_bit) == 0)
                    moves.push_back(double_moves[origin_square - 48]);
            }
        }
    }
    return moves;
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
            // Blocking with pawns
            for (unsigned short origin_square : getBitIndices(m_white_pawns_bit & ~m_all_pins))
            {
                if ((precomputed_moves::white_pawn_moves[origin_square] & m_all_pieces_bit) == 0)
                // We can move the pawn up
                {
                    for (unsigned short destination_square : getBitIndices(precomputed_moves::white_pawn_moves[origin_square] & m_check_rays))
                    {
                        if (destination_square > 55)
                        // Promotions
                        {
                            moves.push_back(Move(origin_square, destination_square, 1));
                            moves.push_back(Move(origin_square, destination_square, 2));
                            moves.push_back(Move(origin_square, destination_square, 3));
                            moves.push_back(Move(origin_square, destination_square, 4));
                        }
                        else
                            // One up
                            moves.emplace_back(origin_square, destination_square);
                    }
                    if ((origin_square < 16) && (precomputed_moves::white_pawn_doubles[origin_square] & m_check_rays) != 0)
                        moves.push_back(double_moves[origin_square]);
                }
            }
            // Blocking with knights
            for (unsigned short origin_square : getBitIndices(m_white_knights_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square);
                }
            }
            // Blocking with rooks
            for (unsigned short origin_square : getBitIndices(m_white_rooks_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square);
                }
            }
            // Blocking with bishops
            for (unsigned short origin_square : getBitIndices(m_white_bishops_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square);
                }
            }
            // Blocking with queens
            for (unsigned short origin_square : getBitIndices(m_white_queens_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square);
                }
            }
        }
        // Move king
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~(m_all_pieces_bit | m_all_squares_attacked_by_black)))
            if (BitPosition::kingIsSafeFromSliders(destination_square))
                moves.emplace_back(m_white_king_position, destination_square);
    }
    else // Black's turn
    {
        if (m_num_checks == 1)
        // We can only capture (with a piece that is not the king) or block if there is only one check
        {
            // Blocking with pawns
            for (unsigned short origin_square : getBitIndices(m_black_pawns_bit & ~m_all_pins))
            {
                if ((precomputed_moves::black_pawn_moves[origin_square] & m_all_pieces_bit) == 0)
                // We can move the pawn up
                {
                    for (unsigned short destination_square : getBitIndices(precomputed_moves::black_pawn_moves[origin_square] & m_check_rays))
                    {
                        if (destination_square > 55)
                        // Promotions
                        {
                            moves.push_back(Move(origin_square, destination_square, 1));
                            moves.push_back(Move(origin_square, destination_square, 2));
                            moves.push_back(Move(origin_square, destination_square, 3));
                            moves.push_back(Move(origin_square, destination_square, 4));
                        }
                        else
                            // One up
                            moves.emplace_back(origin_square, destination_square);
                    }
                    if ((origin_square > 47) && (precomputed_moves::black_pawn_doubles[origin_square] & m_check_rays) != 0)
                        moves.push_back(double_moves[origin_square - 48]);
                }
            }
            // Blocking with knights
            for (unsigned short origin_square : getBitIndices(m_black_knights_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(precomputed_moves::knight_moves[origin_square] & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square);
                }
            }
            // Blocking with rooks
            for (unsigned short origin_square : getBitIndices(m_black_rooks_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit) & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square);
                }
            }
            // Blocking with bishops
            for (unsigned short origin_square : getBitIndices(m_black_bishops_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices(BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square);
                }
            }
            // Blocking with queens
            for (unsigned short origin_square : getBitIndices(m_black_queens_bit & ~m_all_pins))
            {
                for (unsigned short destination_square : getBitIndices((BmagicNOMASK(origin_square, precomputed_moves::bishop_unfull_rays[origin_square] & m_all_pieces_bit) | RmagicNOMASK(origin_square, precomputed_moves::rook_unfull_rays[origin_square] & m_all_pieces_bit)) & m_check_rays))
                {
                    moves.emplace_back(origin_square, destination_square);
                }
            }
        }
        // Move king
        for (unsigned short destination_square : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~(m_all_pieces_bit | m_all_squares_attacked_by_white)))
            if (BitPosition::kingIsSafeFromSliders(destination_square))
                moves.emplace_back(m_black_king_position, destination_square);
    }
    return moves;
}

void BitPosition::makeNormalMoveWithoutNNUE(Move move)
// Move piece and switch white and black roles, without rotating the board.
// After making move, set pins and checks to 0.
{
    unsigned short origin_square{move.getOriginSquare()};
    m_last_origin_bit = (1ULL << origin_square);
    unsigned short destination_square{move.getDestinationSquare()};
    m_last_destination_bit = (1ULL << destination_square);
    m_captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion

    if (m_turn) // White's move
    {
        if (origin_square == 0) // Moving from a1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (origin_square == 7) // Moving from h1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (destination_square == 63) // Capturing on a8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (destination_square == 56) // Capturing on h8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_origin_bit == m_white_king_bit) // Moving king
        {
            // Remove castling zobrist part
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            m_white_kingside_castling = false;
            m_white_queenside_castling = false;

            // Set new castling zobrist part
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            // Update king bit and king position
            m_white_king_bit = m_last_destination_bit;
            m_white_king_position = destination_square;
            m_moved_piece = 5;
        }
        // Moving any piece except king
        else
        {
            BitPosition::setPiece();
        }

        // Special Moves (4 types)
        unsigned short psquare{0};

        // 1) Captures (Non passant)
        if ((m_last_destination_bit & m_black_pieces_bit) != 0)
        {
            BitPosition::removePiece();
        }
        // 2) Double pawn moves
        else if ((move.getData() & 49152) == 32768) // 1000000000000000
            psquare = origin_square + 8;

        // 3) Passant Captures
        else if (m_moved_piece == 0 && (m_last_destination_bit == passant_bitboards[m_psquare]))
        {
            // Remove captured piece
            m_black_pawns_bit &= ~(1ULL << (m_psquare - 8));
            m_captured_piece = 0;
        }
        // 4) Promotions and Castling
        if ((move.getData() & 0b1100000000000000) == 0b0100000000000000) // 0100000000000000
        {
            if (move.getData() == 16772) // White kingside castling
            {
                m_white_rooks_bit &= ~128;
                m_white_rooks_bit |= 32;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
            }
            else if (move.getData() == 16516) // White queenside castling
            {
                m_white_rooks_bit &= ~1;
                m_white_rooks_bit |= 8;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
            }
            else // Promotions
            {
                m_white_pawns_bit &= ~m_last_destination_bit;

                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};
                if (promoting_piece == 12288) // Queen promotion
                {
                    m_promoted_piece = 4;
                    m_white_queens_bit |= m_last_destination_bit;
                }
                else if (promoting_piece == 8192) // Rook promotion
                {
                    m_promoted_piece = 3;
                    m_white_rooks_bit |= m_last_destination_bit;
                }
                else if (promoting_piece == 4096) // Bishop promotion
                {
                    m_promoted_piece = 2;
                    m_white_bishops_bit |= m_last_destination_bit;
                }
                else // Knight promotion
                {
                    m_promoted_piece = 1;
                    m_white_knights_bit |= m_last_destination_bit;
                }
            }
        }
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
        m_psquare = psquare;
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
    }
    else // Black's move
    {
        if (origin_square == 56) // Moving from a8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (origin_square == 63) // Moving from h8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (destination_square == 0) // Capturing on a1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (destination_square == 7) // Capturing on h1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_origin_bit == m_black_king_bit) // Moving king
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            m_black_king_bit = m_last_destination_bit;
            m_black_king_position = destination_square;
            m_moved_piece = 5;
        }
        // Moving any piece except the king
        else
        {
            BitPosition::setPiece();
        }

        // Special Moves (4 types)
        unsigned short psquare{0};

        // 1) Captures (Non passant)
        if ((m_last_destination_bit & m_white_pieces_bit) != 0) // 1100000000000000
        {
            BitPosition::removePiece();
        }

        // 2) Double pawn moves
        else if ((move.getData() & 49152) == 32768) // 1000000000000000
            psquare = origin_square - 8;

        // 3) Passant Captures
        else if (m_moved_piece == 0 && (m_last_destination_bit == passant_bitboards[m_psquare])) // 1100000000000000
        {
            // Remove captured piece
            m_white_pawns_bit &= ~(1ULL << (m_psquare + 8));
            m_captured_piece = 0;
        }
        // 4) Promotions and Castling
        if ((move.getData() & 0b1100000000000000) == 0b0100000000000000) // 0100000000000000
        {
            if (move.getData() == 20412) // Black kingside castling
            {
                m_black_rooks_bit &= ~9223372036854775808ULL;
                m_black_rooks_bit |= 2305843009213693952ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
            }
            else if (move.getData() == 20156) // Black queenside castling
            {
                m_black_rooks_bit &= ~72057594037927936ULL;
                m_black_rooks_bit |= 576460752303423488ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
            }
            else // Promotions
            {
                m_black_pawns_bit &= ~m_last_destination_bit;

                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};
                if (promoting_piece == 12288) // Queen promotion
                {
                    m_promoted_piece = 4;
                    m_black_queens_bit |= m_last_destination_bit;
                }
                else if (promoting_piece == 8192) // Rook promotion
                {
                    m_promoted_piece = 3;
                    m_black_rooks_bit |= m_last_destination_bit;
                }
                else if (promoting_piece == 4096) // Bishop promotion
                {
                    m_promoted_piece = 2;
                    m_black_bishops_bit |= m_last_destination_bit;
                }
                else // Knight promotion
                {
                    m_promoted_piece = 1;
                    m_black_knights_bit |= m_last_destination_bit;
                }
            }
        }
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
        m_psquare = psquare;
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
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
