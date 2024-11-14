#include <iostream>
#include <vector>
#include <cstdint> // For fixed sized integers
#include "precomputed_moves.h" // Include the precomputed move constants
#include "bitposition.h" // Where the BitPosition class is defined
#include "bit_utils.h" // Bit utility functions
#include "move.h"
#include "magicmoves.h"
#include "zobrist_keys.h"
#include "position_eval.h" // Utility functions to update NNUE Input


template void BitPosition::makeMove<Move>(Move move);
template void BitPosition::makeMove<ScoredMove>(ScoredMove move);

template void BitPosition::makeCapture<Move>(Move move);
template void BitPosition::makeCapture<ScoredMove>(ScoredMove move);

template void BitPosition::unmakeMove<Move>(Move move);
template void BitPosition::unmakeMove<ScoredMove>(ScoredMove move);

template void BitPosition::unmakeCapture<Move>(Move move);
template void BitPosition::unmakeCapture<ScoredMove>(ScoredMove move);

template void BitPosition::makeCaptureWithoutNNUE<Move>(Move move);
template void BitPosition::makeCaptureWithoutNNUE<ScoredMove>(ScoredMove move);

template void BitPosition::unmakeCaptureWithoutNNUE<Move>(Move move);
template void BitPosition::unmakeCaptureWithoutNNUE<ScoredMove>(ScoredMove move);

std::array<Move, 4> castling_moves{Move(16772), Move(16516), Move(20412), Move(20156)}; // WKS, WQS, BKS, BQS

constexpr uint64_t NON_LEFT_BITBOARD = 0b1111111011111110111111101111111011111110111111101111111011111110;
constexpr uint64_t NON_RIGHT_BITBOARD = 0b0111111101111111011111110111111101111111011111110111111101111111;
constexpr uint64_t FIRST_ROW_BITBOARD = 0b0000000000000000000000000000000000000000000000000000000011111111;
constexpr uint64_t THIRD_ROW_BITBOARD = 0b0000000000000000000000000000000000000000111111110000000000000000;
constexpr uint64_t SIXTH_ROW_BITBOARD = 0b0000000000000000111111110000000000000000000000000000000000000000;
constexpr uint64_t EIGHT_ROW_BITBOARD = 0b1111111100000000000000000000000000000000000000000000000000000000;

inline uint64_t shift_up(uint64_t b)
{
    return b << 8;
}
inline uint64_t shift_double_up(uint64_t b)
{
    return b << 16;
}
inline uint64_t shift_down(uint64_t b)
{
    return b >> 8; 
}
inline uint64_t shift_double_down(uint64_t b)
{
    return b >> 16;
}
inline uint64_t shift_up_left(uint64_t b)
{
    return (b << 7);
}
inline uint64_t shift_up_right(uint64_t b)
{
    return (b << 9); 
}
inline uint64_t shift_down_left(uint64_t b)
{
    return (b >> 9);
}
inline uint64_t shift_down_right(uint64_t b)
{
    return (b >> 7);
}

// Functions we call on initialization
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
    m_zobrist_keys_array[63 - m_ply] = m_zobrist_key;
}
bool BitPosition::whiteSquareIsSafe(unsigned short square) const
// For when moving the king
{
    // Knights
    if ((precomputed_moves::knight_moves[square] & m_black_knights_bit) != 0)
        return false;

    // Pawns
    if ((precomputed_moves::white_pawn_attacks[square] & m_black_pawns_bit) != 0)
        return false;

    // Queen
    if (((RmagicNOMASK(square, precomputed_moves::rook_unfull_rays[square] & m_all_pieces_bit) | BmagicNOMASK(square, precomputed_moves::bishop_unfull_rays[square] & m_all_pieces_bit)) & m_black_queens_bit) != 0)
        return false;

    // Rook
    if ((RmagicNOMASK(square, precomputed_moves::rook_unfull_rays[square] & m_all_pieces_bit) & m_black_rooks_bit) != 0)
        return false;

    // Bishop
    if ((BmagicNOMASK(square, precomputed_moves::bishop_unfull_rays[square] & m_all_pieces_bit) & m_black_bishops_bit) != 0)
        return false;

    // King
    if ((precomputed_moves::king_moves[square] & m_black_king_bit) != 0)
        return false;

    return true;
}
bool BitPosition::blackSquareIsSafe(unsigned short square) const
// For when moving the king
{
    // Knights
    if ((precomputed_moves::knight_moves[square] & m_white_knights_bit) != 0)
        return false;

    // Pawns
    if ((precomputed_moves::black_pawn_attacks[square] & m_white_pawns_bit) != 0)
        return false;

    // Queen
    if (((RmagicNOMASK(square, precomputed_moves::rook_unfull_rays[square] & m_all_pieces_bit) | BmagicNOMASK(square, precomputed_moves::bishop_unfull_rays[square] & m_all_pieces_bit)) & m_white_queens_bit) != 0)
        return false;

    // Rook
    if ((RmagicNOMASK(square, precomputed_moves::rook_unfull_rays[square] & m_all_pieces_bit) & m_white_rooks_bit) != 0)
        return false;

    // Bishop
    if ((BmagicNOMASK(square, precomputed_moves::bishop_unfull_rays[square] & m_all_pieces_bit) & m_white_bishops_bit) != 0)
        return false;

    // King
    if ((precomputed_moves::king_moves[square] & m_white_king_bit) != 0)
        return false;

    return true;
}
void BitPosition::setIsCheckOnInitialization()
{
    if (m_turn && not whiteSquareIsSafe(m_white_king_position))
        m_is_check = true;
    else if (not m_turn && not blackSquareIsSafe(m_black_king_position))
        m_is_check = true;
    else
        m_is_check = false;
}

// Functions we call before first move generation
void BitPosition::setCheckInfoOnInitialization()
{
    m_num_checks = 0;
    m_check_rays = 0;
    m_check_square = 65;
    if (m_turn)
    {
        // Pawn check
        unsigned short pawnCheck{getLeastSignificantBitIndex(precomputed_moves::white_pawn_attacks[m_white_king_position] & m_black_pawns_bit)};
        if (pawnCheck != 65)
        {
            m_num_checks++;
            m_check_square = pawnCheck;
        }
        // Knight check
        unsigned short knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_white_king_position] & m_black_knights_bit)};
        if (knightCheck != 65)
        {
            m_num_checks++;
            m_check_square = knightCheck;
        }
        // Bishop check
        for (unsigned short bishopSquare : getBitIndices(m_black_bishops_bit))
        {
            uint64_t bishopRay{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_white_king_position][bishopSquare]};
            if ((bishopRay & m_all_pieces_bit) == (1ULL << bishopSquare))
            {
                m_num_checks++;
                m_check_rays |= bishopRay & ~(1ULL << bishopSquare);
                m_check_square = bishopSquare;
            }
        }
        // Rook check
        for (unsigned short rookSquare : getBitIndices(m_black_rooks_bit))
        {
            uint64_t rookRay{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_white_king_position][rookSquare]};
            if ((rookRay & m_all_pieces_bit) == (1ULL << rookSquare))
            {
                m_num_checks++;
                m_check_rays |= rookRay & ~(1ULL << rookSquare);
                m_check_square = rookSquare;
            }
        }
        // Queen check
        for (unsigned short queenSquare : getBitIndices(m_black_queens_bit))
        {
            uint64_t queenRayDiag{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_white_king_position][queenSquare]};
            if ((queenRayDiag & m_all_pieces_bit) == (1ULL << queenSquare))
            {
                m_num_checks++;
                m_check_rays |= queenRayDiag & ~(1ULL << queenSquare);
                m_check_square = queenSquare;
            }
            uint64_t queenRayStra{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_white_king_position][queenSquare]};
            if ((queenRayStra & m_all_pieces_bit) == (1ULL << queenSquare))
            {
                m_num_checks++;
                m_check_rays |= queenRayStra & ~(1ULL << queenSquare);
                m_check_square = queenSquare;
            }
        }
    }
    else
    {
        // Pawn check
        unsigned short pawnCheck{getLeastSignificantBitIndex(precomputed_moves::black_pawn_attacks[m_black_king_position] & m_white_pawns_bit)};
        if (pawnCheck != 65)
        {
            m_num_checks++;
            m_check_square = pawnCheck;
        }
        // Knight check
        unsigned short knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_black_king_position] & m_white_knights_bit)};
        if (knightCheck != 65)
        {
            m_num_checks++;
            m_check_square = knightCheck;
        }
        // Bishop check
        for (unsigned short bishopSquare : getBitIndices(m_white_bishops_bit))
        {
            uint64_t bishopRay{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_black_king_position][bishopSquare]};
            if ((bishopRay & m_all_pieces_bit) == (1ULL << bishopSquare))
            {
                m_num_checks++;
                m_check_rays |= bishopRay & ~(1ULL << bishopSquare);
                m_check_square = bishopSquare;
            }
        }
        // Rook check
        for (unsigned short rookSquare : getBitIndices(m_white_rooks_bit))
        {
            uint64_t rookRay{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_black_king_position][rookSquare]};
            if ((rookRay & m_all_pieces_bit) == (1ULL << rookSquare))
            {
                m_num_checks++;
                m_check_rays |= rookRay & ~(1ULL << rookSquare);
                m_check_square = rookSquare;
            }
        }
        // Queen check
        for (unsigned short queenSquare : getBitIndices(m_white_queens_bit))
        {
            uint64_t queenRayDiag{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_black_king_position][queenSquare]};
            if ((queenRayDiag & m_all_pieces_bit) == (1ULL << queenSquare))
            {
                m_num_checks++;
                m_check_rays |= queenRayDiag & ~(1ULL << queenSquare);
                m_check_square = queenSquare;
            }
            uint64_t queenRayStra{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_black_king_position][queenSquare]};
            if ((queenRayStra & m_all_pieces_bit) == (1ULL << queenSquare))
            {
                m_num_checks++;
                m_check_rays |= queenRayStra & ~(1ULL << queenSquare);
                m_check_square = queenSquare;
            }
        }
    }
}

// Functions we call after making move
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

// Functions for making transposition table moves
bool BitPosition::ttMoveIsLegal(Move move)
{
    uint64_t origin_bit{1ULL << move.getOriginSquare()};
    uint64_t destination_bit{1ULL << move.getDestinationSquare()};
    if (m_turn)
    {
        // If there isn't a piece lying on origin, or there is a white piece lying on destination, or we are capturing king
        if ((m_white_pieces_bit & origin_bit) == 0 || (m_white_pieces_bit & destination_bit) != 0 || m_black_king_bit == destination_bit)
            return false;
        // Pawn moves
        if ((m_white_pawns_bit & origin_bit) != 0)
            {
                if (destination_bit == shift_up(origin_bit))
                {
                    if ((destination_bit & m_all_pieces_bit) != 0) // Normal pawn move into piece
                        return false;
                }
                else if (destination_bit == shift_double_up(origin_bit))
                {
                    if (((destination_bit | shift_up(origin_bit)) & m_all_pieces_bit) != 0) // Double pawn move into piece
                        return false;
                }
                else if ((destination_bit & m_black_pieces_bit) == 0) // Pawn capture not into piece
                    return false;
            }
        // Moving king into check
        if (origin_bit == m_white_king_bit && not newWhiteKingSquareIsSafe(move.getDestinationSquare()))
            return false;
        // Bishop / Queen pins
        for (short int square : getBitIndices((m_black_bishops_bit | m_black_queens_bit) & precomputed_moves::bishop_full_rays[m_white_king_position]))
        // For each square corresponding to black bishop raying white king
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_white_king_position]};
            if ((bishop_ray & m_black_pieces_bit) == 0 && (bishop_ray & m_white_pieces_bit) == origin_bit && (bishop_ray & destination_bit) == 0)
                return false;
        }
        // Rook / Queen pins
        for (short int square : getBitIndices((m_black_rooks_bit | m_black_queens_bit) & precomputed_moves::rook_full_rays[m_white_king_position]))
        // For each square corresponding to black bishop raying white king
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_white_king_position]};
            if ((rook_ray & m_black_pieces_bit) == 0 && (rook_ray & m_white_pieces_bit) == origin_bit && (rook_ray & destination_bit) == 0)
                return false;
        }
    }
    else
    {
        // If there isn't a black piece lying on origin, or there is a black piece lying on destination, or we are capturing king
        if ((m_black_pieces_bit & origin_bit) == 0 || (m_black_pieces_bit & destination_bit) != 0 || m_white_king_bit == destination_bit)
            return false;
        // Pawn moves
        if ((m_black_pawns_bit & origin_bit) != 0)
        {
            if (destination_bit == shift_down(origin_bit))
            {
                if ((destination_bit & m_all_pieces_bit) != 0) // Normal pawn move into piece
                    return false;
            }
            else if (destination_bit == shift_double_down(origin_bit))
            {
                if (((destination_bit | shift_down(origin_bit)) & m_all_pieces_bit) != 0) // Double pawn move into piece
                    return false;
            }
            else if ((destination_bit & m_white_pieces_bit) == 0) // Pawn capture not into piece
                return false;
        }
        // Moving king into check
        if (origin_bit == m_black_king_bit && not newBlackKingSquareIsSafe(move.getDestinationSquare()))
            return false;
        // Bishop / Queen pins
        for (short int square : getBitIndices((m_white_bishops_bit | m_white_queens_bit) & precomputed_moves::bishop_full_rays[m_black_king_position]))
        // For each square corresponding to black bishop raying white king
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_black_king_position]};
            if ((bishop_ray & m_white_pieces_bit) == 0 && (bishop_ray & m_black_pieces_bit) == origin_bit && (bishop_ray & destination_bit) == 0)
                return false;
        }
        // Rook / Queen pins
        for (short int square : getBitIndices((m_white_rooks_bit | m_white_queens_bit) & precomputed_moves::rook_full_rays[m_black_king_position]))
        // For each square corresponding to black bishop raying white king
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_black_king_position]};
            if ((rook_ray & m_white_pieces_bit) == 0 && (rook_ray & m_black_pieces_bit) == origin_bit && (rook_ray & destination_bit) == 0)
                return false;
        }
    }
    return true;
}
void BitPosition::storePlyInfoInTTMove()
{
    m_wkcastling_array[m_ply] = m_white_kingside_castling;
    m_wqcastling_array[m_ply] = m_white_queenside_castling;
    m_bkcastling_array[m_ply] = m_black_kingside_castling;
    m_bqcastling_array[m_ply] = m_black_queenside_castling;

    m_blockers_array[m_ply] = m_blockers;

    // When making and unmaking a tt move we lose these variables which are essential for setCheckInfoAfterMove()
    m_last_origin_square_array[m_ply] = m_last_origin_square;
    m_last_destination_square_array[m_ply] = m_last_destination_square;
    m_moved_piece_array[m_ply] = m_moved_piece;
    m_promoted_piece_array[m_ply] = m_promoted_piece;
    m_psquare_array[m_ply] = m_psquare;

    m_last_destination_bit_array[m_ply] = m_last_destination_bit;

    // m_fen_array[m_ply] = (*this).toFenString(); // For debugging purposes
}
void BitPosition::makeTTMove(Move move)
// Same as makeMove but we store also m_diagonal_pins, m_straight_pins, m_last_origin_square, m_last_destination_square,
// m_moved_piece, m_promoted_piece_array.
{
    // std::string fen_before{(*this).toFenString()}; // Debugging purpose

    m_blockers_set = false;
    BitPosition::storePlyInfoInTTMove(); // store current state for unmake move

    m_last_origin_square = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << m_last_origin_square);
    m_last_destination_square = move.getDestinationSquare();
    m_last_destination_bit = (1ULL << m_last_destination_square);
    m_captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion (Used for updating check info)
    m_is_check = false;

    if (m_turn) // White's move
    {
        if (m_last_origin_square == 0) // Moving from a1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_origin_square == 7) // Moving from h1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_destination_square == 63) // Capturing on a8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_destination_square == 56) // Capturing on h8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_bit == m_white_king_bit) // Moving king
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
            m_white_king_position = m_last_destination_square;
            m_moved_piece = 5;

            // Update NNUE input (after moving the king)
            NNUE::moveWhiteKingNNUEInput(*this);
            m_is_check = isDiscoverCheckForBlack(m_last_origin_square, m_last_destination_square);
        }
        // Moving any piece except king
        else
        {
            BitPosition::setPiece(origin_bit, m_last_destination_bit);
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * m_moved_piece + m_last_origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * m_moved_piece + m_last_destination_square);
        }
        // Captures (Non passant)
        if ((m_last_destination_bit & m_black_pawns_bit) != 0)
        {
            m_black_pawns_bit &= ~m_last_destination_bit;
            m_captured_piece = 0;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_black_knights_bit) != 0)
        {
            m_black_knights_bit &= ~m_last_destination_bit;
            m_captured_piece = 1;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_black_bishops_bit) != 0)
        {
            m_black_bishops_bit &= ~m_last_destination_bit;
            m_captured_piece = 2;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_black_rooks_bit) != 0)
        {
            m_black_rooks_bit &= ~m_last_destination_bit;
            m_captured_piece = 3;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_black_queens_bit) != 0)
        {
            m_black_queens_bit &= ~m_last_destination_bit;
            m_captured_piece = 4;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + m_last_destination_square);
        }

        // Promotions, castling and passant
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            if (move.getData() == 16772) // White kingside castling
            {
                m_white_rooks_bit &= ~128;
                m_white_rooks_bit |= 32;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares

                m_is_check = isRookCheckOrDiscoverForBlack(7, 5);

                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 7);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 5);
            }
            else if (move.getData() == 16516) // White queenside castling
            {
                m_white_rooks_bit &= ~1;
                m_white_rooks_bit |= 8;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares

                m_is_check = isRookCheckOrDiscoverForBlack(0, 3);

                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 3);
            }
            else if ((m_last_destination_bit & EIGHT_ROW_BITBOARD) != 0) // Promotions
            {
                m_all_pieces_bit &= ~origin_bit;
                m_white_pawns_bit &= ~m_last_destination_bit;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_destination_square);

                m_promoted_piece = move.getPromotingPiece() + 1;
                if (m_promoted_piece == 4) // Queen promotion
                {
                    m_white_queens_bit |= m_last_destination_bit;
                    m_is_check = isQueenCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + m_last_destination_square);
                }
                else if (m_promoted_piece == 3) // Rook promotion
                {
                    m_white_rooks_bit |= m_last_destination_bit;
                    m_is_check = isRookCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + m_last_destination_square);
                }
                else if (m_promoted_piece == 2) // Bishop promotion
                {
                    m_white_bishops_bit |= m_last_destination_bit;
                    m_is_check = isBishopCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + m_last_destination_square);
                }
                else // Knight promotion
                {
                    m_white_knights_bit |= m_last_destination_bit;
                    m_is_check = isKnightCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + m_last_destination_square);
                }
            }
            else // Passant
            {
                m_black_pawns_bit &= ~shift_down(m_last_destination_bit);
                m_captured_piece = 0;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_destination_square - 8);
            }
        }
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];

        // Updating passant square
        if (m_moved_piece == 0 && (m_last_destination_square - m_last_origin_square) == 16)
            m_psquare = m_last_origin_square + 8;
        else
            m_psquare = 0;

        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
    }
    else // Black's move
    {
        if (m_last_origin_square == 56) // Moving from a8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_origin_square == 63) // Moving from h8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_destination_square == 0) // Capturing on a1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_destination_square == 7) // Capturing on h1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_bit == m_black_king_bit) // Moving king
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            m_black_king_bit = m_last_destination_bit;
            m_black_king_position = m_last_destination_square;
            m_moved_piece = 5;

            m_is_check = isDiscoverCheckForWhite(m_last_origin_square, m_last_destination_square);

            // Update NNUE input (after moving the king)
            NNUE::moveBlackKingNNUEInput(*this);
        }
        // Moving any piece except the king
        else
        {
            BitPosition::setPiece(origin_bit, m_last_destination_bit);
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * (5 + m_moved_piece) + m_last_origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * (5 + m_moved_piece) + m_last_destination_square);
        }

        // Captures (Non passant)
        if ((m_last_destination_bit & m_white_pawns_bit) != 0)
        {
            m_white_pawns_bit &= ~m_last_destination_bit;
            m_captured_piece = 0;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_white_knights_bit) != 0)
        {
            m_white_knights_bit &= ~m_last_destination_bit;
            m_captured_piece = 1;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_white_bishops_bit) != 0)
        {
            m_white_bishops_bit &= ~m_last_destination_bit;
            m_captured_piece = 2;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_white_rooks_bit) != 0)
        {
            m_white_rooks_bit &= ~m_last_destination_bit;
            m_captured_piece = 3;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_white_queens_bit) != 0)
        {
            m_white_queens_bit &= ~m_last_destination_bit;
            m_captured_piece = 4;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + m_last_destination_square);
        }

        // Promotions, passant and Castling
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            if (move.getData() == 20412) // Black kingside castling
            {
                m_is_check = isRookCheckOrDiscoverForWhite(63, 61);

                m_black_rooks_bit &= ~9223372036854775808ULL;
                m_black_rooks_bit |= 2305843009213693952ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 63);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 61);
            }
            else if (move.getData() == 20156) // Black queenside castling
            {
                m_is_check = isRookCheckOrDiscoverForWhite(56, 59);

                m_black_rooks_bit &= ~72057594037927936ULL;
                m_black_rooks_bit |= 576460752303423488ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 56);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 59);
            }
            else if ((m_last_destination_bit & FIRST_ROW_BITBOARD) != 0) // Promotions
            {
                m_all_pieces_bit &= ~origin_bit;
                m_black_pawns_bit &= ~m_last_destination_bit;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_destination_square);

                m_promoted_piece = move.getPromotingPiece() + 1;
                if (m_promoted_piece == 4) // Queen promotion
                {
                    m_black_queens_bit |= m_last_destination_bit;
                    m_is_check = isQueenCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + m_last_destination_square);
                }
                else if (m_promoted_piece == 3) // Rook promotion
                {
                    m_black_rooks_bit |= m_last_destination_bit;
                    m_is_check = isRookCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + m_last_destination_square);
                }
                else if (m_promoted_piece == 2) // Bishop promotion
                {
                    m_black_bishops_bit |= m_last_destination_bit;
                    m_is_check = isBishopCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + m_last_destination_square);
                }
                else // Knight promotion
                {
                    m_black_knights_bit |= m_last_destination_bit;
                    m_is_check = isKnightCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + m_last_destination_square);
                }
            }
            else // Passant
            {
                // Remove captured piece
                m_white_pawns_bit &= ~shift_up(m_last_destination_bit);
                m_captured_piece = 0;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_destination_square + 8);
            }
        }
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];

        // Updating passant square
        if (m_moved_piece == 0 && (m_last_origin_square - m_last_destination_square) == 16)
            m_psquare = m_last_origin_square - 8;
        else
            m_psquare = 0;

        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
    }

    BitPosition::setAllPiecesBits();

    m_turn = not m_turn;
    BitPosition::updateZobristKeyPiecePartAfterMove(m_last_origin_square, m_last_destination_square);
    m_zobrist_key ^= zobrist_keys::blackToMoveZobristNumber;
    // Note, we store the zobrist key in it's array when making the move. However the rest of the ply info
    // is stored when making the next move to be able to go back.
    // So we store it in the m_ply+1 position because the initial position (or position after capture) is the m_ply 0.

    m_captured_piece_array[m_ply] = m_captured_piece;

    m_ply++;

    m_zobrist_keys_array[63 - m_ply] = m_zobrist_key;

    // bool isCheck{m_is_check};
    // setCheckInfoOnInitialization();
    // if (m_is_check != isCheck)
    // {
    //     std::cout << "Fen: " << fen_before << "\n";
    //     std::cout << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }
    // Debugging purposes
    // int16_t eval_1{NNUE::evaluationFunction(true)};
    // NNUE::initializeNNUEInput(*this);
    // int16_t eval_2{NNUE::evaluationFunction(true)};
    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // if (eval_1 != eval_2)
    // {
    //     std::cout << "makeTTMove \n";
    //     std::cout << "fen before: " << fen_before << "\n";
    //     std::cout << "fen after: " << (*this).toFenString() << "\n";
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "Moved piece : " << m_moved_piece << "\n";
    //     std::cout << "Captured piece : " << m_captured_piece << "\n";
    //     std::cout << "Promoted piece : " << m_promoted_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }
}
void BitPosition::unmakeTTMove(Move move)
// Same as unmakeMove but we restore also m_diagonal_pins, m_straight_pins, m_last_origin_square, m_last_destination_square,
// m_moved_piece, m_promoted_piece_array.
{
    // If a move was made before, that means previous position had blockers set (which we restore from ply info)
    m_blockers_set = true;

    m_zobrist_keys_array[63 - m_ply] = 0;

    m_ply--;

    // Update irreversible aspects
    m_white_kingside_castling = m_wkcastling_array[m_ply];
    m_white_queenside_castling = m_wqcastling_array[m_ply];
    m_black_kingside_castling = m_bkcastling_array[m_ply];
    m_black_queenside_castling = m_bqcastling_array[m_ply];

    m_diagonal_pins = m_diagonal_pins_array[m_ply];
    m_straight_pins = m_straight_pins_array[m_ply];
    m_blockers = m_blockers_array[m_ply];

    m_last_origin_square = m_last_origin_square_array[m_ply];
    m_last_destination_square = m_last_destination_square_array[m_ply];
    m_moved_piece = m_moved_piece_array[m_ply];
    m_promoted_piece = m_promoted_piece_array[m_ply];
    m_psquare = m_psquare_array[m_ply];

    m_last_destination_bit = m_last_destination_bit_array[m_ply];

    // Get irreversible info
    unsigned short previous_captured_piece{m_captured_piece_array[m_ply]};

    // Update aspects to not recompute
    m_zobrist_key = m_zobrist_keys_array[63 - m_ply];

    // Get move info
    unsigned short origin_square{move.getOriginSquare()};
    uint64_t origin_bit{1ULL << origin_square};
    unsigned short destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{1ULL << destination_square};

    if (m_turn) // Last move was black
    {
        // Castling, Passant and promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
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
            else if ((destination_bit & FIRST_ROW_BITBOARD) != 0)
            {
                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};

                m_black_pawns_bit |= origin_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);

                if (promoting_piece == 12288) // Unpromote queen
                {
                    m_black_queens_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                }
                else if (promoting_piece == 8192) // Unpromote rook
                {
                    m_black_rooks_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                }
                else if (promoting_piece == 4096) // Unpromote bishop
                {
                    m_black_bishops_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                }
                else // Unpromote knight
                {
                    m_black_knights_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                }
                // Unmaking captures in promotions
                if (previous_captured_piece != 7)
                {
                    if (previous_captured_piece == 1) // Uncapture knight
                    {
                        m_white_knights_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                    }
                    else if (previous_captured_piece == 2) // Uncapture bishop
                    {
                        m_white_bishops_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                    }
                    else if (previous_captured_piece == 3) // Uncapture rook
                    {
                        m_white_rooks_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                    }
                    else // Uncapture queen
                    {
                        m_white_queens_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                    }
                }
            }
            else // Passant
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                m_white_pawns_bit |= shift_up(destination_bit);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, destination_square + 8);
            }
        }

        // Non special moves
        else
        {
            if ((destination_bit & m_black_pawns_bit) != 0) // Unmove pawn
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);
            }
            else if ((destination_bit & m_black_knights_bit) != 0) // Unmove knight
            {
                m_black_knights_bit |= origin_bit;
                m_black_knights_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + origin_square);
            }
            else if ((destination_bit & m_black_bishops_bit) != 0) // Unmove bishop
            {
                m_black_bishops_bit |= origin_bit;
                m_black_bishops_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + origin_square);
            }
            else if ((destination_bit & m_black_rooks_bit) != 0) // Unmove rook
            {
                m_black_rooks_bit |= origin_bit;
                m_black_rooks_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + origin_square);
            }
            else if ((destination_bit & m_black_queens_bit) != 0) // Unmove queen
            {
                m_black_queens_bit |= origin_bit;
                m_black_queens_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + origin_square);
            }
            else // Unmove king
            {
                m_black_king_bit = origin_bit;
                m_black_king_position = origin_square;
                NNUE::moveBlackKingNNUEInput(*this);
            }
            // Unmaking captures
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 0) // Uncapture pawn
                {
                    m_white_pawns_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, destination_square);
                }
                else if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_white_knights_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_white_bishops_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_white_rooks_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                }
                else // Uncapture queen
                {
                    m_white_queens_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                }
            }
        }
    }
    else // Last move was white
    {
        // Special moves
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
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
            else if ((destination_bit & EIGHT_ROW_BITBOARD) != 0)
            {
                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};

                m_white_pawns_bit |= origin_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);

                if (promoting_piece == 12288) // Unpromote queen
                {
                    m_white_queens_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                }
                else if (promoting_piece == 8192) // Unpromote rook
                {
                    m_white_rooks_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                }
                else if (promoting_piece == 4096) // Unpromote bishop
                {
                    m_white_bishops_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                }
                else // Unpromote knight
                {
                    m_white_knights_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                }
                // Unmaking captures in promotions
                if (previous_captured_piece != 7)
                {
                    if (previous_captured_piece == 1) // Uncapture knight
                    {
                        m_black_knights_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                    }
                    else if (previous_captured_piece == 2) // Uncapture bishop
                    {
                        m_black_bishops_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                    }
                    else if (previous_captured_piece == 3) // Uncapture rook
                    {
                        m_black_rooks_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                    }
                    else // Uncapture queen
                    {
                        m_black_queens_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                    }
                }
            }
            else // Passant
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;

                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square);

                m_black_pawns_bit |= shift_down(destination_bit);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square - 8);
            }
        }

        // Non special moves
        else
        {
            if ((destination_bit & m_white_pawns_bit) != 0) // Unmove pawn
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);
            }
            else if ((destination_bit & m_white_knights_bit) != 0) // Unmove knight
            {
                m_white_knights_bit |= origin_bit;
                m_white_knights_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + origin_square);
            }
            else if ((destination_bit & m_white_bishops_bit) != 0) // Unmove bishop
            {
                m_white_bishops_bit |= origin_bit;
                m_white_bishops_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + origin_square);
            }
            else if ((destination_bit & m_white_rooks_bit) != 0) // Unmove rook
            {
                m_white_rooks_bit |= origin_bit;
                m_white_rooks_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + origin_square);
            }
            else if ((destination_bit & m_white_queens_bit) != 0) // Unmove queen
            {
                m_white_queens_bit |= origin_bit;
                m_white_queens_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + origin_square);
            }
            else // Unmove king
            {
                m_white_king_bit = origin_bit;
                m_white_king_position = origin_square;
                NNUE::moveWhiteKingNNUEInput(*this);
            }

            // Unmaking captures
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 0) // Uncapture pawn
                {
                    m_black_pawns_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                }
                else if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_black_knights_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_black_bishops_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_black_rooks_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                }
                else // Uncapture queen
                {
                    m_black_queens_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                }
            }
        }
    }
    BitPosition::setAllPiecesBits();
    m_turn = not m_turn;

    // Debugging purposes
    // int16_t eval_1{NNUE::evaluationFunction(true)};
    // NNUE::initializeNNUEInput(*this);
    // int16_t eval_2{NNUE::evaluationFunction(true)};
    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // if (eval_1 != eval_2)
    // {
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "unmakeTTMove \n";
    //     std::cout << "fen after: " << (*this).toFenString() << "\n";
    //     std::cout << "Turn : " << m_turn << "\n";
    //     std::cout << "Origin square : " << origin_square << "\n";
    //     std::cout << "Destination square : " << destination_square << "\n";
    //     std::cout << "Captured piece : " << previous_captured_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }
}

// Functions we call before move generation
void BitPosition::setDiscoverCheckForWhite()
// Return check info for discovered checks, for direct checks we know because of the move
// Called in setCheckInfoAfterMove
{
    // Black bishops and queens
    for (unsigned short checking_piece_square : getBitIndices(BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (m_black_bishops_bit | m_black_queens_bit) & ~(1ULL << m_last_destination_square)))
    {
        m_check_rays |= precomputed_moves::precomputedBishopMovesTableOneBlocker[checking_piece_square][m_white_king_position];
        m_check_square = checking_piece_square;
        m_num_checks++;
    }
    // Black rooks and queens
    for (unsigned short checking_piece_square : getBitIndices(RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (m_black_rooks_bit | m_black_queens_bit) & ~(1ULL << m_last_destination_square)))
    {
        m_check_rays |= precomputed_moves::precomputedRookMovesTableOneBlocker[checking_piece_square][m_white_king_position];
        m_check_square = checking_piece_square;
        m_num_checks++;
    }
}
void BitPosition::setDiscoverCheckForBlack()
// Return check info for discovered checks, for direct checks we know because of the move
// Called in setCheckInfoAfterMove
{
    // White bishops and queens
    for (unsigned short checking_piece_square : getBitIndices(BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & m_all_pieces_bit) & (m_white_bishops_bit | m_white_queens_bit) & ~(1ULL << m_last_destination_square)))
    {
        m_check_rays |= precomputed_moves::precomputedBishopMovesTableOneBlocker[checking_piece_square][m_black_king_position];
        m_check_square = checking_piece_square;
        m_num_checks++;
    }
    // White rooks and queens
    for (unsigned short checking_piece_square : getBitIndices(RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & m_all_pieces_bit) & (m_white_rooks_bit | m_white_queens_bit) & ~(1ULL << m_last_destination_square)))
    {
        m_check_rays |= precomputed_moves::precomputedRookMovesTableOneBlocker[checking_piece_square][m_black_king_position];
        m_check_square = checking_piece_square;
        m_num_checks++;
    }
}

void BitPosition::setCheckInfoAfterMove()
// This is called in setMovesInCheck and setCapturesInCheck
{
    m_num_checks = 0;
    m_check_rays = 0;
    m_check_square = 65;

    if (m_turn) // Last move was black
    {
        if (m_moved_piece == 0) // If the last move was a pawn
        {
            if (m_promoted_piece == 7) // Direct  pawn check
            {
                unsigned short pawnCheck{getLeastSignificantBitIndex(precomputed_moves::white_pawn_attacks[m_white_king_position] & m_black_pawns_bit)};
                if (pawnCheck != 65)
                {
                    m_num_checks++;
                    m_check_square = pawnCheck;
                }
            }
            else if (m_promoted_piece == 1) // Knight direct check
            {
                unsigned short knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_white_king_position] & m_black_knights_bit)};
                if (knightCheck != 65)
                {
                    m_num_checks++;
                    m_check_square = knightCheck;
                }
            }
            else if (m_promoted_piece == 2) // Bishop direct check
            {
                uint64_t ray{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_white_king_position][m_last_destination_square]};
                if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
                {
                    m_num_checks++;
                    m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                    m_check_square = m_last_destination_square;
                }
            }
            else if (m_promoted_piece == 3) // Rook direct check
            {
                uint64_t ray{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_white_king_position][m_last_destination_square]};
                if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
                {
                    m_num_checks++;
                    m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                    m_check_square = m_last_destination_square;
                }
            }
            else // Queen direct check
            {
                uint64_t ray{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_white_king_position][m_last_destination_square]};
                if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
                {
                    m_num_checks++;
                    m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                    m_check_square = m_last_destination_square;
                }
                ray = precomputed_moves::precomputedRookMovesTableOneBlocker2[m_white_king_position][m_last_destination_square];
                if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
                {
                    m_num_checks++;
                    m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                    m_check_square = m_last_destination_square;
                }
            }
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_white_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForWhite();
        }
        else if (m_moved_piece == 1) // If last move was a knight
        {
            // Direct check
            unsigned short knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_white_king_position] & m_black_knights_bit)};
            if (knightCheck != 65)
            {
                m_num_checks++;
                m_check_square = knightCheck;
            }
            // Discovered check if origin and king position don't lie on same line (kngihts origin and destination never lie on the same line)
            if (precomputed_moves::OnLineBitboards[m_last_origin_square][m_white_king_position] != 0)
                setDiscoverCheckForWhite();
        }
        else if (m_moved_piece == 2) // If last move was a bishop
        {
            uint64_t ray{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_white_king_position][m_last_destination_square]};
            // Direct check
            if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
            {
                m_num_checks++;
                m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                m_check_square = m_last_destination_square;
            }
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_white_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForWhite();
        }
        else if (m_moved_piece == 3) // If last move was a rook
        {
            uint64_t ray{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_white_king_position][m_last_destination_square]};
            // Direct check
            if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
            {
                m_num_checks++;
                m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                m_check_square = m_last_destination_square;
            }
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_white_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForWhite();
        }
        else if (m_moved_piece == 4) // If last move was a queen
        {
            uint64_t ray{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_white_king_position][m_last_destination_square]};
            // Direct check
            if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
            {
                m_num_checks++;
                m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                m_check_square = m_last_destination_square;
            }
            ray = precomputed_moves::precomputedRookMovesTableOneBlocker2[m_white_king_position][m_last_destination_square];
            // Direct check
            if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
            {
                m_num_checks++;
                m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                m_check_square = m_last_destination_square;
            }
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_white_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForWhite();
        }
        else // Discovered check after moving king
        {
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_white_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForWhite();
        }
    }
    else // Last move was white
    {
        if (m_moved_piece == 0) // If the last move was a pawn
        {
            if (m_promoted_piece == 7) // Direct  pawn check
            {
                unsigned short pawnCheck{getLeastSignificantBitIndex(precomputed_moves::black_pawn_attacks[m_black_king_position] & m_white_pawns_bit)};
                if (pawnCheck != 65)
                {
                    m_num_checks++;
                    m_check_square = pawnCheck;
                }
            }
            else if (m_promoted_piece == 1) // Knight direct check
            {
                unsigned short knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_black_king_position] & m_white_knights_bit)};
                if (knightCheck != 65)
                {
                    m_num_checks++;
                    m_check_square = knightCheck;
                }
            }
            else if (m_promoted_piece == 2) // Bishop direct check
            {
                uint64_t ray{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_black_king_position][m_last_destination_square]};
                if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
                {
                    m_num_checks++;
                    m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                    m_check_square = m_last_destination_square;
                }
            }
            else if (m_promoted_piece == 3) // Rook direct check
            {
                uint64_t ray{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_black_king_position][m_last_destination_square]};
                if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
                {
                    m_num_checks++;
                    m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                    m_check_square = m_last_destination_square;
                }
            }
            else // Queen direct check
            {
                uint64_t ray{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_black_king_position][m_last_destination_square]};
                if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
                {
                    m_num_checks++;
                    m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                    m_check_square = m_last_destination_square;
                }
                ray = precomputed_moves::precomputedRookMovesTableOneBlocker2[m_black_king_position][m_last_destination_square];
                if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
                {
                    m_num_checks++;
                    m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                    m_check_square = m_last_destination_square;
                }
            }
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_black_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForBlack();
        }
        else if (m_moved_piece == 1) // If last move was a knight
        {
            // Direct check
            unsigned short knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_black_king_position] & m_white_knights_bit)};
            if (knightCheck != 65)
            {
                m_num_checks++;
                m_check_square = knightCheck;
            }
            // Discovered check if origin and king position don't lie on same line (kngihts origin and destination never lie on the same line)
            if (precomputed_moves::OnLineBitboards[m_last_origin_square][m_black_king_position] != 0)
                setDiscoverCheckForBlack();
        }
        else if (m_moved_piece == 2) // If last move was a bishop
        {
            uint64_t ray{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_black_king_position][m_last_destination_square]};
            // Direct check
            if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
            {
                m_num_checks++;
                m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                m_check_square = m_last_destination_square;
            }
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_black_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForBlack();
        }
        else if (m_moved_piece == 3) // If last move was a rook
        {
            uint64_t ray{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_black_king_position][m_last_destination_square]};
            // Direct check
            if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
            {
                m_num_checks++;
                m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                m_check_square = m_last_destination_square;
            }
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_black_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForBlack();
        }
        else if (m_moved_piece == 4) // If last move was a queen
        {
            uint64_t ray{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_black_king_position][m_last_destination_square]};
            // Direct check
            if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
            {
                m_num_checks++;
                m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                m_check_square = m_last_destination_square;
            }
            ray = precomputed_moves::precomputedRookMovesTableOneBlocker2[m_black_king_position][m_last_destination_square];
            // Direct check
            if ((ray & m_all_pieces_bit) == (1ULL << m_last_destination_square))
            {
                m_num_checks++;
                m_check_rays |= ray & ~(1ULL << m_last_destination_square);
                m_check_square = m_last_destination_square;
            }
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_black_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForBlack();
        }
        else // Discovered check after moving king
        {
            // Discovered check if origin, destination and king position dont lie on same line
            if ((precomputed_moves::OnLineBitboards2[m_last_origin_square][m_black_king_position] & (1ULL << m_last_destination_square)) == 0)
                setDiscoverCheckForBlack();
        }
    }
}
void BitPosition::setPins()
// Set m_straight_pins, m_diagonal_pins. Called in all move generators: setMovesAndScores, setMovesInCheck, setCapturesAndScores, setCapturesInCheck.
{
    m_diagonal_pins = 0;
    m_straight_pins = 0;

    if (m_turn) // White's turn
    {
        // Pins by black bishops and queens
        for (short int square : getBitIndices((m_black_bishops_bit | m_black_queens_bit) & precomputed_moves::bishop_full_rays[m_white_king_position]))
        // For each square corresponding to black bishop raying white king
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_white_king_position]};
            if ((bishop_ray & m_black_pieces_bit) == 0 && hasOneOne(bishop_ray & m_white_pieces_bit))
            {
                m_diagonal_pins |= bishop_ray;
            }
        }
        // Pins by black rooks and queens
        for (short int square : getBitIndices((m_black_rooks_bit | m_black_queens_bit) & precomputed_moves::rook_full_rays[m_white_king_position]))
        // For each square corresponding to black rook raying white king
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_white_king_position]};
            if ((rook_ray & m_black_pieces_bit) == 0 && hasOneOne(rook_ray & m_white_pieces_bit))
            {
                m_straight_pins |= rook_ray;
            }
        }
    }
    else // Black's turn
    {
        // Pins by white bishops and queens
        for (short int square : getBitIndices((m_white_bishops_bit | m_white_queens_bit) & precomputed_moves::bishop_full_rays[m_black_king_position]))
        // For each square corresponding to white bishop raying white king
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_black_king_position]};
            if ((bishop_ray & m_white_pieces_bit) == 0 && hasOneOne(bishop_ray & m_black_pieces_bit))
            {
                m_diagonal_pins |= bishop_ray;
            }
        }
        // Pins by white rooks and queens
        for (short int square : getBitIndices((m_white_rooks_bit | m_white_queens_bit) & precomputed_moves::rook_full_rays[m_black_king_position]))
        // For each square corresponding to white rook raying white king
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_black_king_position]};
            if ((rook_ray & m_white_pieces_bit) == 0 && hasOneOne(rook_ray & m_black_pieces_bit))
            {
                m_straight_pins |= rook_ray;
            }
        }
    }
}
void BitPosition::setAttackedSquares()
// These are set for move ordering. Called only in setMovesAndScores, to penalize unsafe moves.
// They are also for moving king safely in normal moves.
{
    m_unsafe_squares = 0;
    m_king_unsafe_squares = 0;

    if (m_turn)
    {
        // Knights
        for (unsigned short origin : getBitIndices(m_black_knights_bit))
            m_unsafe_squares |= precomputed_moves::knight_moves[origin];

        // Pawns
        m_unsafe_squares |= (shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD) | shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD));
        // King
        m_unsafe_squares |= precomputed_moves::king_moves[m_black_king_position];

        m_king_unsafe_squares = m_unsafe_squares;

        // Queen
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            m_unsafe_squares |= (((RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit))));
            m_king_unsafe_squares |= (((RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit & ~m_white_king_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit & ~m_white_king_bit))));
        }
        // Rook
        for (unsigned short origin : getBitIndices(m_black_rooks_bit))
        {
            m_unsafe_squares |= (RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit));
            m_king_unsafe_squares |= (RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit & ~m_white_king_bit));
        }
        // Bishop
        for (unsigned short origin : getBitIndices(m_black_bishops_bit))
        {
            m_unsafe_squares |= (BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit));
            m_king_unsafe_squares |= (BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit & ~m_white_king_bit));
        }
    }
    else
    {
        // Knights
        for (unsigned short origin : getBitIndices(m_white_knights_bit))
            m_unsafe_squares |= precomputed_moves::knight_moves[origin];

        // King
        m_unsafe_squares |= precomputed_moves::king_moves[m_white_king_position];
        // Pawns
        m_unsafe_squares |= (shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD) | shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD));

        m_king_unsafe_squares = m_unsafe_squares;

        // Queen
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            m_unsafe_squares |= (((RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit))));
            m_king_unsafe_squares |= (((RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit & ~m_black_king_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit & ~m_black_king_bit))));
        }
        // Rook
        for (unsigned short origin : getBitIndices(m_white_rooks_bit))
        {
            m_unsafe_squares |= (RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit));
            m_king_unsafe_squares |= (RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit & ~m_black_king_bit));
        }
        // Bishop
        for (unsigned short origin : getBitIndices(m_white_bishops_bit))
        {
            m_unsafe_squares |= (BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit));
            m_king_unsafe_squares |= (BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit & ~m_black_king_bit));
        }
    }
}
void BitPosition::setBlockers()
// This is the only one called before ttable moves, since we need ot determine if move gives check or not.
// It is also called in nextMove, nextCapture, nextMoveInCheck and nextCaptureInCheck, when a legal move is found.
{
    m_blockers = 0;
    if (m_turn)
    {
        // Blockers of white bishops and queens
        for (short int square : getBitIndices((m_white_bishops_bit | m_white_queens_bit) & precomputed_moves::bishop_full_rays[m_black_king_position]))
        // For each square corresponding to black bishop raying black king
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(bishop_ray & m_all_pieces_bit))
            {
                m_blockers |= bishop_ray;
            }
        }
        // Blockers of white rooks and queens
        for (short int square : getBitIndices((m_white_rooks_bit | m_white_queens_bit) & precomputed_moves::rook_full_rays[m_black_king_position]))
        // For each square corresponding to black rook raying black king
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_black_king_position]};
            if (hasOneOne(rook_ray & m_all_pieces_bit))
            {
                m_blockers |= rook_ray;
            }
        }
    }
    else
    {
        // Blockers of black bishops and queens
        for (short int square : getBitIndices((m_black_bishops_bit | m_black_queens_bit) & precomputed_moves::bishop_full_rays[m_white_king_position]))
        // For each square corresponding to black bishop raying white king
        {
            uint64_t bishop_ray{precomputed_moves::precomputedBishopMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(bishop_ray & m_all_pieces_bit))
            {
                m_blockers |= bishop_ray;
            }
        }
        // Blockers of black rooks and queens
        for (short int square : getBitIndices((m_black_rooks_bit | m_black_queens_bit) & precomputed_moves::rook_full_rays[m_white_king_position]))
        // For each square corresponding to black rook raying white king
        {
            uint64_t rook_ray{precomputed_moves::precomputedRookMovesTableOneBlocker[square][m_white_king_position]};
            if (hasOneOne(rook_ray & m_all_pieces_bit))
            {
                m_blockers |= rook_ray;
            }
        }
    }
}

// Functions we call during move generation
template <typename T>
bool BitPosition::isLegal(const T &move) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    unsigned short origin_square{move.getOriginSquare()};
    unsigned short destination_square{move.getDestinationSquare()};
    // Move is legal if piece is not pinned, otherwise if origin, destination and king position are aligned
    if (m_turn)
    {
        // Knight and king moves are always legal
        if (((1ULL << origin_square) & (m_white_knights_bit | m_white_king_bit)) != 0)
            return true;
        if (((1ULL << origin_square) & (m_diagonal_pins | m_straight_pins)) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_white_king_bit) != 0)
        {
            return true;
        }
    }
    else
    {
        // Knight and king moves are always legal
        if (((1ULL << origin_square) & (m_black_knights_bit | m_black_king_bit)) != 0)
            return true;
        if (((1ULL << origin_square) & (m_diagonal_pins | m_straight_pins)) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_black_king_bit) != 0)
            return true;
    }
    return false;
}
bool BitPosition::isLegalForWhite(unsigned short origin_square, unsigned short destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    // Pawn and king moves are always legal
    if (((1ULL << origin_square) & (m_white_knights_bit | m_white_king_bit)) != 0)
        return true;
    // Move is legal if piece is not pinned, otherwise if origin, destination and king position are aligned
    if (((1ULL << origin_square) & (m_diagonal_pins | m_straight_pins)) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_white_king_bit) != 0)
        return true;
    return false;
}
bool BitPosition::isLegalForBlack(unsigned short origin_square, unsigned short destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    // Pawn and king moves are always legal
    if (((1ULL << origin_square) & (m_black_knights_bit | m_black_king_bit)) != 0)
        return true;

    // Move is legal if piece is not pinned, otherwise if origin, destination and king position are aligned
    if (((1ULL << origin_square) & (m_diagonal_pins|m_straight_pins)) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_black_king_bit) != 0)
        return true;
    return false;
}

bool BitPosition::newKingSquareIsSafe(unsigned short new_position) const
// For when moving the king in normal moves
{
    if (((1ULL << new_position) & m_king_unsafe_squares) != 0)
        return false;
    return true;
}
bool BitPosition::newWhiteKingSquareIsSafe(unsigned short new_position) const
// For when moving the king in captures
{
    // Knights
    if ((precomputed_moves::knight_moves[new_position] & m_black_knights_bit) != 0)
        return false;

    // Pawns
    if ((precomputed_moves::white_pawn_attacks[new_position] & m_black_pawns_bit) != 0)
        return false;

    // Queen
    if (((RmagicNOMASK(new_position, precomputed_moves::rook_unfull_rays[new_position] & m_all_pieces_bit & ~m_white_king_bit) | BmagicNOMASK(new_position, precomputed_moves::bishop_unfull_rays[new_position] & m_all_pieces_bit & ~m_white_king_bit)) & m_black_queens_bit) != 0)
        return false;

    // Rook
    if ((RmagicNOMASK(new_position, precomputed_moves::rook_unfull_rays[new_position] & m_all_pieces_bit & ~m_white_king_bit) & m_black_rooks_bit) != 0)
        return false;

    // Bishop
    if ((BmagicNOMASK(new_position, precomputed_moves::bishop_unfull_rays[new_position] & m_all_pieces_bit & ~m_white_king_bit) & m_black_bishops_bit) != 0)
        return false;

    // King
    if ((precomputed_moves::king_moves[new_position] & m_black_king_bit) != 0)
        return false;

    return true;
}
bool BitPosition::newBlackKingSquareIsSafe(unsigned short new_position) const
// For when moving the king in captures
{
    // Knights
    if ((precomputed_moves::knight_moves[new_position] & m_white_knights_bit) != 0)
        return false;

    // Pawns
    if ((precomputed_moves::black_pawn_attacks[new_position] & m_white_pawns_bit) != 0)
        return false;

    // Queen
    if (((RmagicNOMASK(new_position, precomputed_moves::rook_unfull_rays[new_position] & m_all_pieces_bit & ~m_black_king_bit) | BmagicNOMASK(new_position, precomputed_moves::bishop_unfull_rays[new_position] & m_all_pieces_bit & ~m_black_king_bit)) & m_white_queens_bit) != 0)
        return false;

    // Rook
    if ((RmagicNOMASK(new_position, precomputed_moves::rook_unfull_rays[new_position] & m_all_pieces_bit & ~m_black_king_bit) & m_white_rooks_bit) != 0)
        return false;

    // Bishop
    if ((BmagicNOMASK(new_position, precomputed_moves::bishop_unfull_rays[new_position] & m_all_pieces_bit & ~m_black_king_bit) & m_white_bishops_bit) != 0)
        return false;

    // King
    if ((precomputed_moves::king_moves[new_position] & m_white_king_bit) != 0)
        return false;

    return true;
}

bool BitPosition::kingIsSafeAfterPassant(unsigned short removed_square_1, unsigned short removed_square_2) const // See if the king is in check or not (from kings position). For when moving the king.
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

// Functions we call inside move makers
bool BitPosition::isDiscoverCheckForWhiteAfterPassant(unsigned short origin_square, unsigned short destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks. 
// We just check if removing the pawn captured leads to check, since check after removing and placing our pawn is already checked using isPawnCheckOrDiscoverForWhite
{
    // If captured pawn is not blocking or after removing captured pawn we are not in check
    if (((1ULL << (destination_square + 8)) & m_blockers) == 0 || ((BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & (m_all_pieces_bit & ~(1ULL << (destination_square + 8)))) & (m_black_bishops_bit | m_black_queens_bit)) == 0 && (RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & (m_all_pieces_bit & ~(1ULL << (destination_square + 8)))) & (m_black_rooks_bit | m_black_queens_bit)) == 0))
        return false;

    return true;
}
bool BitPosition::isDiscoverCheckForBlackAfterPassant(unsigned short origin_square, unsigned short destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks.
// We just check if removing the pawn captured leads to check, since check after removing and placing our pawn is already checked using isPawnCheckOrDiscoverForBlack
{
    // If captured pawn is not blocking or after removing captured pawn we are not in check
    if (((1ULL << (destination_square - 8)) & m_blockers) == 0 || ((BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & (m_all_pieces_bit & ~(1ULL << (destination_square - 8)))) & (m_white_bishops_bit | m_white_queens_bit)) == 0 && (RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & (m_all_pieces_bit & ~(1ULL << (destination_square - 8)))) & (m_white_rooks_bit | m_white_queens_bit)) == 0))
        return false;

    return true;
}

bool BitPosition::isDiscoverCheckForWhite(unsigned short origin_square, unsigned short destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
{
    // If piece is not blocking or moving in blocking ray
    if (((1ULL << origin_square) & m_blockers) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_white_king_bit) != 0)
        return false;

    return true;
}
bool BitPosition::isDiscoverCheckForBlack(unsigned short origin_square, unsigned short destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
{
    // If piece is not blocking or moving in blocking ray
    if (((1ULL << origin_square) & m_blockers) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_black_king_bit) != 0)
        return false;

    return true;
}

bool BitPosition::isPawnCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::black_pawn_attacks[destination_square] & m_white_king_bit) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isKnightCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::knight_moves[destination_square] & m_white_king_bit) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isBishopCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::bishop_full_rays[m_white_king_position] & (1ULL << destination_square)) != 0 && (BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (1ULL << destination_square)) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isRookCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::rook_full_rays[m_white_king_position] & (1ULL << destination_square)) != 0 && (RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (1ULL << destination_square)) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isQueenCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::queen_full_rays[m_white_king_position] & (1ULL << destination_square)) != 0 && ((BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & m_all_pieces_bit) | RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & m_all_pieces_bit)) & (1ULL << destination_square)) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    return false;
}

bool BitPosition::isPawnCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::white_pawn_attacks[destination_square] & m_black_king_bit) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isKnightCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::knight_moves[destination_square] & m_black_king_bit) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isBishopCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::bishop_full_rays[m_black_king_position] & (1ULL << destination_square)) != 0 && (BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & m_all_pieces_bit) & (1ULL << destination_square)) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isRookCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::rook_full_rays[m_black_king_position] & (1ULL << destination_square)) != 0 && (RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & m_all_pieces_bit) & (1ULL << destination_square)) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isQueenCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const
{
    // Direct check
    if ((precomputed_moves::queen_full_rays[m_black_king_position] & (1ULL << destination_square)) != 0 && ((BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & m_all_pieces_bit) | RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & m_all_pieces_bit)) & (1ULL << destination_square)) != 0)
        return true;
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    return false;
}

// First move generations
std::vector<Move> BitPosition::inCheckAllMoves()
// For first move search, we have to initialize check info
{
    setCheckInfoOnInitialization();
    setPins();
    setBlockers();
    setAttackedSquares();
    std::vector<Move> moves;
    moves.reserve(64);
    if (m_turn) // White's turn
    {
        if (m_num_checks == 1)
        {
            // Pawn Blocks and Captures
            // Single moves
            uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
            uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
            for (unsigned short destination : getBitIndices(single_pawn_moves_blocking_bit))
            {
                    if (destination < 56) // Non promotions
                    {
                        if (isLegalForWhite(destination - 8, destination))
                            moves.emplace_back(Move(destination - 8, destination));
                    }
                    else // Promotions
                    {
                        if (isLegalForWhite(destination - 8, destination))
                        {
                            moves.emplace_back(Move(destination - 8, destination, 0));
                            moves.emplace_back(Move(destination - 8, destination, 1));
                            moves.emplace_back(Move(destination - 8, destination, 2));
                            moves.emplace_back(Move(destination - 8, destination, 3));
                        }
                    }
            }
            // Double moves
            for (unsigned short destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
            {
                if (isLegalForWhite(destination - 16, destination))
                    moves.emplace_back(Move(destination - 16, destination));
            }
            // Passant block
            if ((m_psquare & m_check_rays) != 0)
            {
                for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                    if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                    {
                        moves.emplace_back(Move(origin, m_psquare));
                    }
            }
            // Pawn captures from checking position
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_check_square] & m_white_pawns_bit))
            {
                if (m_check_square >= 56) // Promotions
                {
                    if (isLegalForWhite(origin, m_check_square))
                    {
                        moves.emplace_back(Move(origin, m_check_square, 0));
                        moves.emplace_back(Move(origin, m_check_square, 1));
                        moves.emplace_back(Move(origin, m_check_square, 2));
                        moves.emplace_back(Move(origin, m_check_square, 3));
                    }
                }
                else // Non promotion
                {
                    if (isLegalForWhite(origin, m_check_square))
                        moves.emplace_back(Move(origin, m_check_square));
                }
            }
            // Knight Blocks and Captures
            for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
            {
                for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isLegalForWhite(origin, m_check_square))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Bishop blocks and Captures
            for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
            {
                for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isLegalForWhite(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Rook blocks and Captures
            for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
            {
                for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isLegalForWhite(origin, m_check_square))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Queen blocks and captures
            for (unsigned short origin : getBitIndices(m_white_queens_bit))
            {
                for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_white_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isLegalForWhite(origin, m_check_square))
                        moves.emplace_back(Move(origin, destination));
                }
            }
        }
        // King moves
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_white_pieces_bit))
        {
            if (newKingSquareIsSafe(destination))
                moves.emplace_back(Move(m_white_king_position, destination));
        }
    }
    else // Blacks turn
    {
        if (m_num_checks == 1)
        {
            // Pawn Blocks and Captures
            // Single moves
            uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
            uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
            for (unsigned short destination : getBitIndices(single_pawn_moves_blocking_bit))
            {
                    if (destination > 7) // Non promotions
                    {
                        if (isLegalForBlack(destination + 8, destination))
                            moves.emplace_back(Move(destination + 8, destination));
                    }
                    else // Promotions
                    {
                        if (isLegalForBlack(destination + 8, destination))
                        {
                            moves.emplace_back(Move(destination + 8, destination, 0));
                            moves.emplace_back(Move(destination + 8, destination, 1));
                            moves.emplace_back(Move(destination + 8, destination, 2));
                            moves.emplace_back(Move(destination + 8, destination, 3));
                        }
                    }
            }
            // Double moves
            for (unsigned short destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
            {
                if (isLegalForBlack(destination + 16, destination))
                    moves.emplace_back(Move(destination + 16, destination));
            }
            // Passant block
            if ((m_psquare & m_check_rays) != 0)
            {
                for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                    if (kingIsSafeAfterPassant(origin, m_psquare + 8)) // Legal
                    {
                        moves.emplace_back(Move(origin, m_psquare));
                    }
            }
            // Pawn captures from checking position
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_check_square] & m_black_pawns_bit))
            {
                        if (m_check_square <= 7) // Promotions
                        {
                            if (isLegalForBlack(origin, m_check_square))
                            {
                                moves.emplace_back(Move(origin, m_check_square, 0));
                                moves.emplace_back(Move(origin, m_check_square, 1));
                                moves.emplace_back(Move(origin, m_check_square, 2));
                                moves.emplace_back(Move(origin, m_check_square, 3));
                            }
                        }
                        else // Non promotion
                        {
                            if (isLegalForBlack(origin, m_check_square))
                                moves.emplace_back(Move(origin, m_check_square));
                        }
            }
            // Knight blocks and Captures
            for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
            {
                for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isLegalForBlack(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Bishop blocks and Captures
            for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
            {
                for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isLegalForBlack(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Rook blocks and Captures
            for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
            {
                for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isLegalForBlack(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Queen blocks and captures
            for (unsigned short origin : getBitIndices(m_black_queens_bit))
            {
                for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_black_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isLegalForBlack(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
        }
        // King moves
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_black_pieces_bit))
        {
            if (newKingSquareIsSafe(destination))
                moves.emplace_back(Move(m_black_king_position, destination));
        }
    }
    return moves;
}
std::vector<Move> BitPosition::allMoves()
// For first move search
{
    setPins();
    setBlockers();
    setAttackedSquares();
    std::vector<Move> moves;
    moves.reserve(128);
    if (m_turn) // White's turn
    {
        // Knights
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_white_pieces_bit))
            {
                moves.emplace_back(Move(origin, destination));
            }
        }
        // Bishops
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit))
            {
                if ((*this).isLegal(Move(origin, destination)))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Rooks
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit))
            {
                if ((*this).isLegal(Move(origin, destination)))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Queens
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_white_pieces_bit))
            {
                if ((*this).isLegal(Move(origin, destination)))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit))
        {
            if ((*this).isLegal(Move(destination - 8, destination)))
            {
                if (destination < 56) // Non promotions
                {
                    moves.emplace_back(Move(destination - 8, destination));
                }
                else // Promotions
                {
                    moves.emplace_back(Move(destination - 8, destination, 0));
                    moves.emplace_back(Move(destination - 8, destination, 1));
                    moves.emplace_back(Move(destination - 8, destination, 2));
                    moves.emplace_back(Move(destination - 8, destination, 3));
                }
            }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            if ((*this).isLegal(Move(destination - 16, destination)))
                moves.emplace_back(Move(destination - 16, destination));
        }
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit))
        {
            if ((*this).isLegal(Move(destination - 9, destination)))
            {
                if (destination < 56) // Non promotions
                {
                    moves.emplace_back(Move(destination - 9, destination));
                }
                else // Promotions
                {
                    moves.emplace_back(Move(destination - 9, destination, 0));
                    moves.emplace_back(Move(destination - 9, destination, 1));
                    moves.emplace_back(Move(destination - 9, destination, 2));
                    moves.emplace_back(Move(destination - 9, destination, 3));
                }
            }
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit))
        {
            if ((*this).isLegal(Move(destination - 7, destination)))
            {
                if (destination < 56) // Non promotions
                {
                    moves.emplace_back(Move(destination - 7, destination));
                }
                else // Promotions
                {
                    moves.emplace_back(Move(destination - 7, destination, 0));
                    moves.emplace_back(Move(destination - 7, destination, 1));
                    moves.emplace_back(Move(destination - 7, destination, 2));
                    moves.emplace_back(Move(destination - 7, destination, 3));
                }
            }
        }
        // Passant
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                {
                    moves.emplace_back(Move(origin, m_psquare));
                }
        }

        // King
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_white_pieces_bit & ~m_king_unsafe_squares))
        {
            moves.emplace_back(Move(m_white_king_position, destination));
        }
        // Kingside castling
        if (m_white_kingside_castling && (m_all_pieces_bit & 96) == 0 && newKingSquareIsSafe(5) && newKingSquareIsSafe(6))
            moves.emplace_back(castling_moves[0]);
        // Queenside castling
        if (m_white_queenside_castling && (m_all_pieces_bit & 14) == 0 && newKingSquareIsSafe(2) && newKingSquareIsSafe(3))
            moves.emplace_back(castling_moves[1]);
    }
    else // Black's turn
    {
        // Knights
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_black_pieces_bit))
            {
                moves.emplace_back(Move(origin, destination));
            }
        }
        // Bishops
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit))
            {
                if ((*this).isLegal(Move(origin, destination)))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Rooks
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit))
            {
                if ((*this).isLegal(Move(origin, destination)))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Queens
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_black_pieces_bit))
            {
                if ((*this).isLegal(Move(origin, destination)))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit))
        {
            if ((*this).isLegal(Move(destination + 8, destination)))
            {
                if (destination > 7) // Non promotions
                {
                        moves.emplace_back(Move(destination + 8, destination));
                }
                else // Promotions
                {
                        moves.emplace_back(Move(destination + 8, destination, 0));
                        moves.emplace_back(Move(destination + 8, destination, 1));
                        moves.emplace_back(Move(destination + 8, destination, 2));
                        moves.emplace_back(Move(destination + 8, destination, 3));
                }
            }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            if ((*this).isLegal(Move(destination + 16, destination)))
                moves.emplace_back(Move(destination + 16, destination));
        }
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit))
        {
            if ((*this).isLegal(Move(destination + 7, destination)))
            {
                if (destination > 7) // Non promotions
                {
                        moves.emplace_back(Move(destination + 7, destination));
                }
                else // Promotions
                {
                        moves.emplace_back(Move(destination + 7, destination, 0));
                        moves.emplace_back(Move(destination + 7, destination, 1));
                        moves.emplace_back(Move(destination + 7, destination, 2));
                        moves.emplace_back(Move(destination + 7, destination, 3));
                }
            }
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit))
        {
            if ((*this).isLegal(Move(destination + 9, destination)))
            {
                if (destination > 7) // Non promotions
                {
                        moves.emplace_back(Move(destination + 9, destination));
                }
                else // Promotions
                {
                        moves.emplace_back(Move(destination + 9, destination, 0));
                        moves.emplace_back(Move(destination + 9, destination, 1));
                        moves.emplace_back(Move(destination + 9, destination, 2));
                        moves.emplace_back(Move(destination + 9, destination, 3));
                }
            }
        }
        // Passant
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare + 8))
                {
                        moves.emplace_back(Move(origin, m_psquare));
                }
        }
        // King moves
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_black_pieces_bit & ~m_king_unsafe_squares))
        {
            moves.emplace_back(Move(m_black_king_position, destination));
        }
        // Kingside castling
        if (m_black_kingside_castling && (m_all_pieces_bit & 6917529027641081856) == 0 && newKingSquareIsSafe(61) && newKingSquareIsSafe(62))
            moves.emplace_back(castling_moves[2]);
        // Queenside castling
        if (m_black_queenside_castling && (m_all_pieces_bit & 1008806316530991104) == 0 && newKingSquareIsSafe(58) && newKingSquareIsSafe(59))
            moves.emplace_back(castling_moves[3]);
    }
    return moves;
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
                // Moving piece unsafely
                if (origin_bit != m_white_king_bit && ((destination_bit & m_unsafe_squares) != 0))
                {
                    if ((origin_bit & m_white_pawns_bit) != 0)
                        score = -1;

                    else if ((origin_bit & m_white_knights_bit) != 0)
                        score = -2;

                    else if ((origin_bit & m_white_bishops_bit) != 0)
                        score = -3;

                    else if ((origin_bit & m_white_rooks_bit) != 0)
                        score = -4;

                    else
                        score = -5;
                }
                // Capture score
                if ((destination_bit & m_black_pieces_bit) != 0)
                {
                    if ((destination_bit & m_black_pawns_bit) != 0)
                        score += 10;
                    else if ((destination_bit & m_black_knights_bit) != 0)
                        score += 20;
                    else if ((destination_bit & m_black_bishops_bit) != 0)
                        score += 30;
                    else if ((destination_bit & m_black_rooks_bit) != 0)
                        score += 40;
                    else
                        score += 50;
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
                // Moving piece unsafely
                if (origin_bit != m_black_king_bit && ((destination_bit & m_unsafe_squares) != 0))
                {
                    if ((origin_bit & m_black_pawns_bit) != 0)
                        score = -1;

                    else if ((origin_bit & m_black_knights_bit) != 0)
                        score = -2;

                    else if ((origin_bit & m_black_bishops_bit) != 0)
                        score = -3;

                    else if ((origin_bit & m_black_rooks_bit) != 0)
                        score = -4;

                    else
                        score = -5;
                }
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

// Refutation move generations (for Quiesence)
Move BitPosition::getBestRefutation()
{
    setPins();
    if (m_turn)
    {
        // Right shift captures
        if ((shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & ~EIGHT_ROW_BITBOARD & m_last_destination_bit ) != 0)
        {
            Move move{Move(m_last_destination_square - 9, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Left shift captures
        if ((shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & ~EIGHT_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            Move move{Move(m_last_destination_square - 7, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Right shift promotions
        if ((shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & EIGHT_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            Move move{Move(m_last_destination_square - 9, m_last_destination_square, 3)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Left shift promotions
        if ((shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & EIGHT_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            Move move{Move(m_last_destination_square - 7, m_last_destination_square, 3)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Knight refutation
        for (unsigned short origin : getBitIndices(precomputed_moves::knight_moves[m_last_destination_square] & m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            Move move{Move(origin, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Bishop refutation
        for (unsigned short origin : getBitIndices(BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_white_bishops_bit & ~m_straight_pins))
        {
            Move move{Move(origin, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Rook refutation
        for (unsigned short origin : getBitIndices(RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_white_rooks_bit & ~m_diagonal_pins))
        {
            Move move{Move(origin, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Queen refutation
        for (unsigned short origin : getBitIndices((BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) | RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit)) & m_white_queens_bit))
        {
            Move move{Move(origin, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // King refutation
        if ((precomputed_moves::king_moves[m_white_king_position] & m_last_destination_bit) != 0 && newWhiteKingSquareIsSafe(m_last_destination_square))
        {
            Move move{Move(m_white_king_position, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
    }
    else // Black captures
    {
        // Right shift captures
        if ((shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & ~FIRST_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            Move move{Move(m_last_destination_square + 7, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Left shift captures
        if ((shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & ~FIRST_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            Move move{Move(m_last_destination_square + 9, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Right shift promotions
        if ((shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & FIRST_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            Move move{Move(m_last_destination_square + 7, m_last_destination_square, 3)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Left shift promotions
        if ((shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & FIRST_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            Move move{Move(m_last_destination_square + 9, m_last_destination_square, 3)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Knight refutation
        for (unsigned short origin : getBitIndices(precomputed_moves::knight_moves[m_last_destination_square] & m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            Move move{Move(origin, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Bishop refutation
        for (unsigned short origin : getBitIndices(BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_black_bishops_bit & ~m_straight_pins))
        {
            Move move{Move(origin, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Rook refutation
        for (unsigned short origin : getBitIndices(RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_black_rooks_bit & ~m_diagonal_pins))
        {
            Move move{Move(origin, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // Queen refutation
        for (unsigned short origin : getBitIndices((BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) | RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit)) & m_black_queens_bit))
        {
            Move move{Move(origin, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
        // King refutation
        if ((precomputed_moves::king_moves[m_black_king_position] & m_last_destination_bit) != 0 && newBlackKingSquareIsSafe(m_last_destination_square))
        {
            Move move{Move(m_black_king_position, m_last_destination_square)};
            if (isLegal(move))
            {
                setBlockers();
                return move;
            }
        }
    }
    return Move(0);
}

// Capture move generations (for Quiesence)
void BitPosition::pawnCapturesAndQueenProms(ScoredMove*& move_list) const
{
    if (m_turn)
    {
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & ~EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 9, destination);
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & ~EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 7, destination);
        }
        // Queen promotions
        for (unsigned short destination : getBitIndices(shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 8, destination, 3);
        }
        // Right shift captures and queen promotions
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 9, destination, 3);
        }
        // Left shift captures and queen promotions
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 7, destination, 3);
        }
    }
    else // Black captures
    {
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & ~FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 7, destination);
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & ~FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 9, destination);
        }
        // Queen promotions
        for (unsigned short destination : getBitIndices(shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 8, destination, 3);
        }
        // Right shift captures and queen promotions
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 7, destination, 3);
        }
        // Left shift captures and queen promotions
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 9, destination, 3);
        }
    }
}
void BitPosition::knightCaptures(ScoredMove*& move_list) const
// All knight captures except capturing unsafe pawns
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & m_black_pieces_bit))
            {
                    *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & m_white_pieces_bit))
            {
                    *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::bishopCaptures(ScoredMove*& move_list) const
// All bishop captures except capturing unsafe pawns
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_black_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_white_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::rookCaptures(ScoredMove*& move_list) const
// All rook captures except capturing unsafe pawns, knights or rooks
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_black_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_white_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::queenCaptures(ScoredMove*& move_list) const
// All queen captures except capturing unsafe pieces
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_black_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_white_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::kingCaptures(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & m_black_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
    }
    else
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & m_white_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
    }
}
void BitPosition::kingCaptures(Move *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & m_black_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
    }
    else
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & m_white_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
    }
}

// All move generations (for PV Nodes in Alpha-Beta)
void BitPosition::pawnAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit))
        {
            if (destination < 56) // Non promotions
            {
                *move_list++ = Move(destination - 8, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination - 8, destination, 0);
                *move_list++ = Move(destination - 8, destination, 1);
                *move_list++ = Move(destination - 8, destination, 2);
                *move_list++ = Move(destination - 8, destination, 3);
            }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            *move_list++ = Move(destination - 16, destination);
        }
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit))
        {
            if (destination < 56) // Non promotions
            {
                *move_list++ = Move(destination - 9, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination - 9, destination, 0);
                *move_list++ = Move(destination - 9, destination, 1);
                *move_list++ = Move(destination - 9, destination, 2);
                *move_list++ = Move(destination - 9, destination, 3);
            }
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit))
        {
            if (destination < 56) // Non promotions
            {
                *move_list++ = Move(destination - 7, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination - 7, destination, 0);
                *move_list++ = Move(destination - 7, destination, 1);
                *move_list++ = Move(destination - 7, destination, 2);
                *move_list++ = Move(destination - 7, destination, 3);
            }
        }
        // Passant
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
    else // Black moves
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit))
        {
            if (destination > 7) // Non promotions
            {
                *move_list++ = Move(destination + 8, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination + 8, destination, 0);
                *move_list++ = Move(destination + 8, destination, 1);
                *move_list++ = Move(destination + 8, destination, 2);
                *move_list++ = Move(destination + 8, destination, 3);
            }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            *move_list++ = Move(destination + 16, destination);
        }
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit))
        {
            if (destination > 7) // Non promotions
            {
                *move_list++ = Move(destination + 7, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination + 7, destination, 0);
                *move_list++ = Move(destination + 7, destination, 1);
                *move_list++ = Move(destination + 7, destination, 2);
                *move_list++ = Move(destination + 7, destination, 3);
            }
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit))
        {
            if (destination > 7) // Non promotions
            {
                *move_list++ = Move(destination + 9, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination + 9, destination, 0);
                *move_list++ = Move(destination + 9, destination, 1);
                *move_list++ = Move(destination + 9, destination, 2);
                *move_list++ = Move(destination + 9, destination, 3);
            }
        }
        // Passant
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare + 8))
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
}
void BitPosition::knightAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_white_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_black_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::bishopAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::rookAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::queenAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_white_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_black_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::kingAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_white_pieces_bit & ~m_king_unsafe_squares))
        {
            *move_list++ = Move(m_white_king_position, destination);
        }
        // Kingside castling
        if (m_white_kingside_castling && (m_all_pieces_bit & 96) == 0 && newKingSquareIsSafe(5) && newKingSquareIsSafe(6))
            *move_list++ = castling_moves[0];
        // Queenside castling
        if (m_white_queenside_castling && (m_all_pieces_bit & 14) == 0 && newKingSquareIsSafe(2) && newKingSquareIsSafe(3))
            *move_list++ = castling_moves[1];
    }
    else
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_black_pieces_bit & ~m_king_unsafe_squares))
        {
            *move_list++ = Move(m_black_king_position, destination);
        }
        // Kingside castling
        if (m_black_kingside_castling && (m_all_pieces_bit & 6917529027641081856) == 0 && newKingSquareIsSafe(61) && newKingSquareIsSafe(62))
            *move_list++ = castling_moves[2];
        // Queenside castling
        if (m_black_queenside_castling && (m_all_pieces_bit & 1008806316530991104) == 0 && newKingSquareIsSafe(58) && newKingSquareIsSafe(59))
            *move_list++ = castling_moves[3];
    }
}

// Safe Move generations (for Non PV Nodes in Alpha-Beta)
void BitPosition::pawnSafeMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (refutations are capturing m_last_destination_bit) and non unsafe moves
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit & ~m_unsafe_squares & ~EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 8, destination);
        }
        // Single move non-queen promotions
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 8, destination, 0);
            *move_list++ = Move(destination - 8, destination, 1);
            *move_list++ = Move(destination - 8, destination, 2);
        }
        // Single move unsafe queen promotions
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit & EIGHT_ROW_BITBOARD & m_unsafe_squares))
        {
            *move_list++ = Move(destination - 8, destination, 3);
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~(m_all_pieces_bit | m_unsafe_squares)))
        {
            *move_list++ = Move(destination - 16, destination);
        }
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pawns_bit & ~(m_unsafe_squares | m_last_destination_bit | EIGHT_ROW_BITBOARD)))
        {
            *move_list++ = Move(destination - 9, destination);
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pawns_bit & ~(m_unsafe_squares | m_last_destination_bit | EIGHT_ROW_BITBOARD)))
        {
            *move_list++ = Move(destination - 7, destination);
        }
        // Right shift non queen promotions
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 9, destination, 0);
            *move_list++ = Move(destination - 9, destination, 1);
            *move_list++ = Move(destination - 9, destination, 2);
        }
        // Right shift unsafe non refutation queen proms
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & EIGHT_ROW_BITBOARD & m_unsafe_squares & ~m_last_destination_bit))
        {
            *move_list++ = Move(destination - 9, destination, 3);
        }
        // Left shift non queen promotions
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 7, destination, 0);
            *move_list++ = Move(destination - 7, destination, 1);
            *move_list++ = Move(destination - 7, destination, 2);
        }
        // Left shift unsafe non refutation queen proms
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & EIGHT_ROW_BITBOARD & m_unsafe_squares & ~m_last_destination_bit))
        {
            *move_list++ = Move(destination - 7, destination, 3);
        }
        // Passant
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
    else // Black moves
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit & ~m_unsafe_squares & ~FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 8, destination);
        }
        // Single move non-queen promotions
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 8, destination, 0);
            *move_list++ = Move(destination + 8, destination, 1);
            *move_list++ = Move(destination + 8, destination, 2);
        }
        // Single move unsafe queen promotions
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit & FIRST_ROW_BITBOARD & m_unsafe_squares))
        {
            *move_list++ = Move(destination + 8, destination, 3);
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~(m_all_pieces_bit | m_unsafe_squares)))
        {
            *move_list++ = Move(destination + 16, destination);
        }
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pawns_bit & ~(m_unsafe_squares | m_last_destination_bit | FIRST_ROW_BITBOARD)))
        {
            *move_list++ = Move(destination + 7, destination);
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pawns_bit & ~(m_unsafe_squares | m_last_destination_bit | FIRST_ROW_BITBOARD)))
        {
            *move_list++ = Move(destination + 9, destination);
        }
        // Right shift non queen promotions
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 7, destination, 0);
            *move_list++ = Move(destination + 7, destination, 1);
            *move_list++ = Move(destination + 7, destination, 2);
        }
        // Right shift unsafe and non refutation queen promotions
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD & m_unsafe_squares & ~m_last_destination_bit))
        {
            *move_list++ = Move(destination + 7, destination, 3);
        }
        // Left shift non queen promotions
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 9, destination, 0);
            *move_list++ = Move(destination + 9, destination, 1);
            *move_list++ = Move(destination + 9, destination, 2);
        }
        // Left shift unsafe and non refutation queen promotions
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD & m_unsafe_squares & ~m_last_destination_bit))
        {
            *move_list++ = Move(destination + 9, destination, 3);
        }
        // Passant
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare + 8))
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
}
void BitPosition::knightSafeMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (capturing m_last_destination_bit) and non unsafe moves
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~(m_white_pieces_bit | m_black_queens_bit | m_black_rooks_bit | m_unsafe_squares | m_last_destination_bit)))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~(m_black_pieces_bit | m_white_queens_bit | m_white_rooks_bit | m_unsafe_squares | m_last_destination_bit)))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::bishopSafeMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (capturing m_last_destination_bit)
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~(m_white_pieces_bit | m_black_queens_bit | m_black_rooks_bit | m_unsafe_squares | m_last_destination_bit)))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~(m_black_pieces_bit | m_white_queens_bit | m_white_rooks_bit | m_unsafe_squares | m_last_destination_bit)))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::rookSafeMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (capturing m_last_destination_bit)
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~(m_white_pieces_bit | m_black_queens_bit | m_unsafe_squares | m_last_destination_bit)))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~(m_black_pieces_bit | m_white_queens_bit | m_unsafe_squares | m_last_destination_bit)))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::queenSafeMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (capturing m_last_destination_bit)
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~(m_white_pieces_bit | m_unsafe_squares | m_last_destination_bit)))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~(m_black_pieces_bit | m_unsafe_squares | m_last_destination_bit)))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::kingNonCapturesAndPawnCaptures(ScoredMove *&move_list) const
// King non captures
{
    if (m_turn)
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & (~m_all_pieces_bit | (m_black_pawns_bit & ~m_last_destination_bit))))
        {
            if (newKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
        // Kingside castling
        if (m_white_kingside_castling && (m_all_pieces_bit & 96) == 0 && newWhiteKingSquareIsSafe(5) && newWhiteKingSquareIsSafe(6))
            *move_list++ = castling_moves[0];
        // Queenside castling
        if (m_white_queenside_castling && (m_all_pieces_bit & 14) == 0 && newWhiteKingSquareIsSafe(2) && newWhiteKingSquareIsSafe(3))
            *move_list++ = castling_moves[1];
    }
    else
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & (~m_all_pieces_bit  | (m_white_pawns_bit & ~m_last_destination_bit))))
        {
            if (newKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
        // Kingside castling
        if (m_black_kingside_castling && (m_all_pieces_bit & 6917529027641081856) == 0 && newBlackKingSquareIsSafe(61) && newBlackKingSquareIsSafe(62))
            *move_list++ = castling_moves[2];
        // Queenside castling
        if (m_black_queenside_castling && (m_all_pieces_bit & 1008806316530991104) == 0 && newBlackKingSquareIsSafe(58) && newBlackKingSquareIsSafe(59))
            *move_list++ = castling_moves[3];
    }
}

// Bad captures or unsafe generator (for Non PV Nodes in Alpha-Beta)
void BitPosition::pawnBadCapturesOrUnsafeNonCaptures(Move *&move_list)
// Unsafe bad captures (non refutations) or unsafe non captures
{
    if (m_turn)
    {
        // Right shift pawn captures
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & ~EIGHT_ROW_BITBOARD & m_black_pawns_bit & m_unsafe_squares & ~m_last_destination_bit))
        {
            *move_list++ = Move(destination - 9, destination);
        }
        // Left shift pawn captures
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & ~EIGHT_ROW_BITBOARD & m_black_pawns_bit & m_unsafe_squares & ~m_last_destination_bit))
        {
            *move_list++ = Move(destination - 7, destination);
        }
        // Unsafe single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit & m_unsafe_squares))
        {
            if (destination < 56) // Non promotions
            {
                *move_list++ = Move(destination - 8, destination);
            }
        }
        // Unsafe double moves
        for (unsigned short destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit & m_unsafe_squares))
        {
            *move_list++ = Move(destination - 16, destination);
        }
    }
    else // Black captures
    {
        // Right shift pawn captures
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & ~FIRST_ROW_BITBOARD & m_white_pawns_bit & m_unsafe_squares & ~m_last_destination_bit))
        {
            *move_list++ = Move(destination + 7, destination);
        }
        // Left shift pawn captures
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & ~FIRST_ROW_BITBOARD & m_white_pawns_bit & m_unsafe_squares & ~m_last_destination_bit))
        {
            *move_list++ = Move(destination + 9, destination);
        }
        // Unsafe single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit & m_unsafe_squares))
        {
            if (destination > 7) // Non promotions
            {
                *move_list++ = Move(destination + 8, destination);
            }
        }
        // Unsafe double moves
        for (unsigned short destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit & m_unsafe_squares))
        {
            *move_list++ = Move(destination + 16, destination);
        }
    }
}
void BitPosition::knightBadCapturesOrUnsafeNonCaptures(Move *&move_list)
// Unsafe bad captures (non refutations) and unsafe non captures
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ((m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | ~m_all_pieces_bit) & m_unsafe_squares) & ~m_last_destination_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ((m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | ~m_all_pieces_bit) & m_unsafe_squares) & ~m_last_destination_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::bishopBadCapturesOrUnsafeNonCaptures(Move *&move_list)
// Unsafe bad captures (non refutations) and unsafe non captures
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ((m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | ~m_all_pieces_bit) & m_unsafe_squares) & ~m_last_destination_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ((m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | ~m_all_pieces_bit) & m_unsafe_squares) & ~m_last_destination_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::rookBadCapturesOrUnsafeNonCaptures(Move *&move_list)
// Unsafe bad captures (non refutations) and unsafe non captures
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ((m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | ~m_all_pieces_bit) & m_unsafe_squares) & ~m_last_destination_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ((m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | ~m_all_pieces_bit) & m_unsafe_squares) & ~m_last_destination_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::queenBadCapturesOrUnsafeNonCaptures(Move *&move_list)
// Unsafe bad captures (non refutations) and unsafe non captures
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ((m_black_pieces_bit | ~m_all_pieces_bit) & m_unsafe_squares) & ~m_last_destination_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ((m_white_pieces_bit | ~m_all_pieces_bit) & m_unsafe_squares) & ~m_last_destination_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
}

// Check blocks (for Alpha-Beta)
void BitPosition::inCheckPawnBlocks(Move*& move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
        for (unsigned short destination : getBitIndices(single_pawn_moves_blocking_bit))
        {
                if (destination < 56) // Non promotions
                {
                        *move_list++ = Move(destination - 8, destination);
                }
                else // Promotions
                {
                        *move_list++ = Move(destination - 8, destination, 0);
                        *move_list++ = Move(destination - 8, destination, 1);
                        *move_list++ = Move(destination - 8, destination, 2);
                        *move_list++ = Move(destination - 8, destination, 3);
                }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
        {
                    *move_list++ = Move(destination - 16, destination);
        }
    }
    else
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
        for (unsigned short destination : getBitIndices(single_pawn_moves_blocking_bit))
        {
                if (destination > 7) // Non promotions
                {
                        *move_list++ = Move(destination + 8, destination);
                }
                else // Promotions
                {
                        *move_list++ = Move(destination + 8, destination, 0);
                        *move_list++ = Move(destination + 8, destination, 1);
                        *move_list++ = Move(destination + 8, destination, 2);
                        *move_list++ = Move(destination + 8, destination, 3);
                }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
        {
                    *move_list++ = Move(destination + 16, destination);
        }
    }
}
void BitPosition::inCheckKnightBlocks(Move*& move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & m_check_rays))
            {
                    *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & m_check_rays))
            {
                    *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::inCheckBishopBlocks(Move*& move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::inCheckRookBlocks(Move*& move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::inCheckQueenBlocks(Move*& move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::inCheckOrderedCapturesAndKingMoves(Move*& move_list) const
// Only called if m_num_checks = 1
{
    if (m_turn)
    {
        // King captures
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & m_black_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
        // Pawn captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_check_square] & m_white_pawns_bit))
        {
                if (m_check_square < 56) // Non promotions
                {
                        *move_list++ = Move(origin, m_check_square);
                }
                else // Promotions
                {
                        *move_list++ = Move(origin, m_check_square, 0);
                        *move_list++ = Move(origin, m_check_square, 1);
                        *move_list++ = Move(origin, m_check_square, 2);
                        *move_list++ = Move(origin, m_check_square, 3);
                }
        }
        // Passant block or capture
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                {
                        *move_list++ = Move(origin, m_psquare, 0);
                }
        }
        // Knight captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::knight_moves[m_check_square] & m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
                *move_list++ = Move(origin, m_check_square);
        }
        // Bishop captures from checking position
        for (unsigned short origin : getBitIndices(BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_white_bishops_bit & ~m_straight_pins))
        {
                    *move_list++ = Move(origin, m_check_square);
        }
        // Rook captures from checking position
        for (unsigned short origin : getBitIndices(RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_white_rooks_bit))
        {
                    *move_list++ = Move(origin, m_check_square);
        }
        // Queen captures from checking position
        for (unsigned short origin : getBitIndices((BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_white_queens_bit))
        {
                    *move_list++ = Move(origin, m_check_square);
        }
        // King non captures
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
    }
    else
    {
        // King captures
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & m_white_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
        // Pawn captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_check_square] & m_black_pawns_bit))
        {
                if (m_check_square > 7) // Non promotions
                {
                        *move_list++ = Move(origin, m_check_square);
                }
                else // Promotions
                {
                        *move_list++ = Move(origin, m_check_square, 0);
                        *move_list++ = Move(origin, m_check_square, 1);
                        *move_list++ = Move(origin, m_check_square, 2);
                        *move_list++ = Move(origin, m_check_square, 3);
                }
        }
        // Passant block or capture
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare + 8)) // Legal
                {
                        *move_list++ = Move(origin, m_psquare, 0);
                }
        }
        // Knight captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::knight_moves[m_check_square] & m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
                *move_list++ = Move(origin, m_check_square);
        }
        // Bishop captures from checking position
        for (unsigned short origin : getBitIndices(BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_black_bishops_bit & ~m_straight_pins))
        {
                    *move_list++ = Move(origin, m_check_square);
        }
        // Rook captures from checking position
        for (unsigned short origin : getBitIndices(RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_black_rooks_bit))
        {
                    *move_list++ = Move(origin, m_check_square);
        }
        // Queen captures from checking position
        for (unsigned short origin : getBitIndices((BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_black_queens_bit))
        {
                    *move_list++ = Move(origin, m_check_square);
        }
        // King non captures
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
    }
}
void BitPosition::kingAllMovesInCheck(Move *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_white_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
    }
    else
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_black_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
    }
}

// Captures in check (for Quiesence)
void BitPosition::inCheckOrderedCaptures(Move *&move_list) const
// Only called if m_num_checks = 1
{
    if (m_turn)
    {
        // King captures
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & m_black_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
        // Pawn captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_check_square] & m_white_pawns_bit))
        {
            if (m_check_square < 56) // Non promotions
            {
                *move_list++ = Move(origin, m_check_square);
            }
            else // Promotions
            {
                *move_list++ = Move(origin, m_check_square, 3);
            }
        }
        // Knight captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::knight_moves[m_check_square] & m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            *move_list++ = Move(origin, m_check_square);
        }
        // Bishop captures from checking position
        for (unsigned short origin : getBitIndices(BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_white_bishops_bit & ~m_straight_pins))
        {
            *move_list++ = Move(origin, m_check_square);
        }
        // Rook captures from checking position
        for (unsigned short origin : getBitIndices(RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_white_rooks_bit))
        {
            *move_list++ = Move(origin, m_check_square);
        }
        // Queen captures from checking position
        for (unsigned short origin : getBitIndices((BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_white_queens_bit))
        {
            *move_list++ = Move(origin, m_check_square);
        }
    }
    else
    {
        // King captures
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & m_white_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
        // Pawn captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_check_square] & m_black_pawns_bit))
        {
            if (m_check_square > 7) // Non promotions
            {
                *move_list++ = Move(origin, m_check_square);
            }
            else // Promotions
            {
                *move_list++ = Move(origin, m_check_square, 3);
            }
        }
        // Knight captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::knight_moves[m_check_square] & m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            *move_list++ = Move(origin, m_check_square);
        }
        // Bishop captures from checking position
        for (unsigned short origin : getBitIndices(BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_black_bishops_bit & ~m_straight_pins))
        {
            *move_list++ = Move(origin, m_check_square);
        }
        // Rook captures from checking position
        for (unsigned short origin : getBitIndices(RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_black_rooks_bit))
        {
            *move_list++ = Move(origin, m_check_square);
        }
        // Queen captures from checking position
        for (unsigned short origin : getBitIndices((BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_black_queens_bit))
        {
            *move_list++ = Move(origin, m_check_square);
        }
    }
}

// For Alpha-Beta (For PV nodes)
ScoredMove *BitPosition::setMovesAndScores(ScoredMove *&move_list_start)
{
    ScoredMove *move_list_end = move_list_start; // Initially, end points to start
    BitPosition::setPins();
    BitPosition::setAttackedSquares();
    // Generate moves for all pieces
    BitPosition::pawnAllMoves(move_list_end);
    BitPosition::knightAllMoves(move_list_end);
    BitPosition::bishopAllMoves(move_list_end);
    BitPosition::rookAllMoves(move_list_end);
    BitPosition::queenAllMoves(move_list_end);
    BitPosition::kingAllMoves(move_list_end);

    // Scoring the moves
    if (m_turn)
    {
        for (ScoredMove *move = move_list_start; move < move_list_end; ++move)
        {
            unsigned short origin_square = move->getOriginSquare();
            unsigned short destination_square = move->getDestinationSquare();
            uint64_t origin_bit = (1ULL << origin_square);
            uint64_t destination_bit = (1ULL << destination_square);

            // Penalizing moving piece unsafely
            if (origin_bit != m_white_king_bit && ((destination_bit & m_unsafe_squares) != 0))
            {
                if ((origin_bit & m_white_pawns_bit) != 0)
                    move->score -= 4;

                else if ((origin_bit & m_white_knights_bit) != 0)
                    move->score -= 10;

                else if ((origin_bit & m_white_bishops_bit) != 0)
                    move->score -= 12;

                else if ((origin_bit & m_white_rooks_bit) != 0)
                    move->score -= 20;

                else
                    move->score -= 36;
            }
            // Bonus for leaving unsafe square
            if ((origin_bit & m_unsafe_squares) != 0)
            {
                if ((origin_bit & m_white_pawns_bit) != 0)
                    move->score += 4;

                else if ((origin_bit & m_white_knights_bit) != 0)
                    move->score += 10;

                else if ((origin_bit & m_white_bishops_bit) != 0)
                    move->score += 12;

                else if ((origin_bit & m_white_rooks_bit) != 0)
                    move->score += 20;

                else
                    move->score += 36;
            }

            // Captured piece
            if ((destination_bit & m_black_pieces_bit) != 0)
            {
                if ((destination_bit & m_black_pawns_bit) != 0)
                    move->score += 5;
                else if ((destination_bit & m_black_knights_bit) != 0)
                    move->score += 11;
                else if ((destination_bit & m_black_bishops_bit) != 0)
                    move->score += 13;
                else if ((destination_bit & m_black_rooks_bit) != 0)
                    move->score += 21;
                else
                    move->score += 37;
            }
        }
    }
    else // Black turn similar
    {
        for (ScoredMove *move = move_list_start; move < move_list_end; ++move)
        {
            unsigned short origin_square = move->getOriginSquare();
            unsigned short destination_square = move->getDestinationSquare();
            uint64_t origin_bit = (1ULL << origin_square);
            uint64_t destination_bit = (1ULL << destination_square);

            // Penalizing moving piece unsafely
            if (origin_bit != m_black_king_bit && ((destination_bit & m_unsafe_squares) != 0))
            {
                if ((origin_bit & m_black_pawns_bit) != 0)
                    move->score -= 4;

                else if ((origin_bit & m_black_knights_bit) != 0)
                    move->score -= 10;

                else if ((origin_bit & m_black_bishops_bit) != 0)
                    move->score -= 12;

                else if ((origin_bit & m_black_rooks_bit) != 0)
                    move->score -= 20;

                else
                    move->score -= 36;
            }
            // Bonus for leaving unsafe square
            if ((origin_bit & m_unsafe_squares) != 0)
            {
                if ((origin_bit & m_black_pawns_bit) != 0)
                    move->score += 4;

                else if ((origin_bit & m_black_knights_bit) != 0)
                    move->score += 10;

                else if ((origin_bit & m_black_bishops_bit) != 0)
                    move->score += 12;

                else if ((origin_bit & m_black_rooks_bit) != 0)
                    move->score += 20;

                else
                    move->score += 36;
            }

            // Captured piece
            if ((destination_bit & m_white_pieces_bit) != 0)
            {
                if ((destination_bit & m_white_pawns_bit) != 0)
                    move->score += 5;
                else if ((destination_bit & m_white_knights_bit) != 0)
                    move->score += 11;
                else if ((destination_bit & m_white_bishops_bit) != 0)
                    move->score += 13;
                else if ((destination_bit & m_white_rooks_bit) != 0)
                    move->score += 21;
                else
                    move->score += 36;
            }
        }
    }
    return move_list_end;
}

// For Alpha-Beta (For non PV nodes)
Move *BitPosition::setRefutationMovesOrdered(Move *&move_list_start)
// All captures of last destination square ordered by worst moving piece first
{
    setPins();
    Move *move_list_end = move_list_start; // Initially, end points to start
    if (m_turn)
    {
        // Capturing with king
        if ((precomputed_moves::king_moves[m_last_destination_square] & m_white_king_bit) != 0)
        {
            if (newWhiteKingSquareIsSafe(m_last_destination_square))
                *move_list_end++ = Move(m_white_king_position, m_last_destination_square);
        }
        // Capturing with pawn
        // Right shift captures
        if ((shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & ~EIGHT_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            *move_list_end++ = Move(m_last_destination_square - 9, m_last_destination_square);
        }
        // Left shift captures
        if ((shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & ~EIGHT_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            *move_list_end++ = Move(m_last_destination_square - 7, m_last_destination_square);
        }
        // Right shift promotions
        if ((shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & EIGHT_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            *move_list_end++ = Move(m_last_destination_square - 9, m_last_destination_square, 3);
        }
        // Left shift promotions
        if ((shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & EIGHT_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            *move_list_end++ = Move(m_last_destination_square - 7, m_last_destination_square, 3);
        }
        // Capturing with knight
        for (unsigned short origin : getBitIndices(precomputed_moves::knight_moves[m_last_destination_square] & m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            *move_list_end++ = Move(origin, m_last_destination_square);
        }
        // Capturing with bishop
        for (unsigned short origin : getBitIndices(BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_white_bishops_bit & ~m_straight_pins))
        {
            *move_list_end++ = Move(origin, m_last_destination_square);
        }
        // Capturing with rook
        for (unsigned short origin : getBitIndices(RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_white_rooks_bit & ~m_diagonal_pins))
        {
            *move_list_end++ = Move(origin, m_last_destination_square);
        }
        // Capturing with queen
        for (unsigned short origin : getBitIndices((BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) | RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit)) & m_white_queens_bit))
        {
            *move_list_end++ = Move(origin, m_last_destination_square);
        }
    }
    else
    {
        // Capturing with king
        if ((precomputed_moves::king_moves[m_last_destination_square] & m_black_king_bit) != 0)
        {
            if (newBlackKingSquareIsSafe(m_last_destination_square))
                *move_list_end++ = Move(m_black_king_position, m_last_destination_square);
        }
        // Capturing with pawn
        // Right shift captures
        if ((shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & ~FIRST_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            *move_list_end++ = Move(m_last_destination_square + 7, m_last_destination_square);
        }
        // Left shift captures
        if ((shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & ~FIRST_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            *move_list_end++ = Move(m_last_destination_square + 9, m_last_destination_square);
        }
        // Right shift promotions
        if ((shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & FIRST_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            *move_list_end++ = Move(m_last_destination_square + 7, m_last_destination_square, 3);
        }
        // Left shift promotions
        if ((shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & FIRST_ROW_BITBOARD & m_last_destination_bit) != 0)
        {
            *move_list_end++ = Move(m_last_destination_square + 9, m_last_destination_square, 3);
        }
        // Capturing with knight
        for (unsigned short origin : getBitIndices(precomputed_moves::knight_moves[m_last_destination_square] & m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            *move_list_end++ = Move(origin, m_last_destination_square);
        }
        // Capturing with bishop
        for (unsigned short origin : getBitIndices(BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_black_bishops_bit & ~m_straight_pins))
        {
            *move_list_end++ = Move(origin, m_last_destination_square);
        }
        // Capturing with rook
        for (unsigned short origin : getBitIndices(RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_black_rooks_bit & ~m_diagonal_pins))
        {
            *move_list_end++ = Move(origin, m_last_destination_square);
        }
        // Capturing with queen
        for (unsigned short origin : getBitIndices((BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) | RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit)) & m_black_queens_bit))
        {
            *move_list_end++ = Move(origin, m_last_destination_square);
        }
    }
    return move_list_end;
}
Move *BitPosition::setGoodCapturesOrdered(Move *&move_list_start)
// All good captures ordered except refutations (capturing m_last_destination_bit)
{
    setAttackedSquares();
    Move *move_list_end = move_list_start; // Initially, end points to start
    if (m_turn)
    {
        // Pawn-Queen and Queen promotions
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_queens_bit & ~m_last_destination_bit & ~EIGHT_ROW_BITBOARD))
        {
            *move_list_end++ = Move(destination - 9, destination);
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_queens_bit & ~m_last_destination_bit & ~EIGHT_ROW_BITBOARD))
        {
            *move_list_end++ = Move(destination - 7, destination);
        }
        // Right shift safe queen promotions
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & ~(m_last_destination_bit | m_unsafe_squares) & EIGHT_ROW_BITBOARD))
        {
            *move_list_end++ = Move(destination - 9, destination, 3);
        }
        // Left shift safe queen promotions
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & ~(m_last_destination_bit | m_unsafe_squares) & EIGHT_ROW_BITBOARD))
        {
            *move_list_end++ = Move(destination - 7, destination, 3);
        }
        // King-Queen
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & m_black_queens_bit & ~m_last_destination_bit))
        {
            if (newKingSquareIsSafe(destination))
                *move_list_end++ = Move(m_white_king_position, destination);
        }
        // Safe queen promotions
        for (unsigned short destination : getBitIndices(shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit & ~m_unsafe_squares & EIGHT_ROW_BITBOARD))
        {
            *move_list_end++ = Move(destination - 8, destination, 3);
        }
        // Pawn-Rook/Bishop/Knight
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & (m_black_rooks_bit | m_black_bishops_bit | m_black_knights_bit) & ~m_last_destination_bit & ~EIGHT_ROW_BITBOARD))
        {
            *move_list_end++ = Move(destination - 9, destination);
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & (m_black_rooks_bit | m_black_bishops_bit | m_black_knights_bit) & ~m_last_destination_bit & ~EIGHT_ROW_BITBOARD))
        {
            *move_list_end++ = Move(destination - 7, destination);
        }
        // King-Rook/Bishop/Knight
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & (m_black_rooks_bit | m_black_bishops_bit | m_black_knights_bit) & ~m_last_destination_bit))
        {
            if (newKingSquareIsSafe(destination))
                *move_list_end++ = Move(m_white_king_position, destination);
        }
        // Knight-Queen/Rook
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & (m_black_rooks_bit | m_black_queens_bit) & ~m_last_destination_bit))
            {
                *move_list_end++ = Move(origin, destination);
            }
        }
        // Bishop-Queen/Rook
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & (m_black_rooks_bit | m_black_queens_bit) & ~m_last_destination_bit))
            {
                *move_list_end++ = Move(origin, destination);
            }
        }
        // Rook-Queen
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_black_queens_bit & ~m_last_destination_bit))
            {
                *move_list_end++ = Move(origin, destination);
            }
        }
    }
    else
    {
        // Pawn-Queen and Queen promotions
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_queens_bit & ~FIRST_ROW_BITBOARD & ~m_last_destination_bit))
        {
            *move_list_end++ = Move(destination + 7, destination);
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_queens_bit & ~FIRST_ROW_BITBOARD & ~m_last_destination_bit))
        {
            *move_list_end++ = Move(destination + 9, destination);
        }
        // Right shift safe queen promotions
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD & ~(m_last_destination_bit | m_unsafe_squares)))
        {
            *move_list_end++ = Move(destination + 7, destination, 3);
        }
        // Left shift safe queen promotions
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD & ~(m_last_destination_bit | m_unsafe_squares)))
        {
            *move_list_end++ = Move(destination + 9, destination, 3);
        }
        // King-Queen
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & m_white_queens_bit & ~m_last_destination_bit))
        {
            if (newKingSquareIsSafe(destination))
                *move_list_end++ = Move(m_black_king_position, destination);
        }
        // Safe queen promotions
        for (unsigned short destination : getBitIndices(shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit & FIRST_ROW_BITBOARD & ~m_unsafe_squares))
        {
            *move_list_end++ = Move(destination + 8, destination, 3);
        }
        // Pawn-Rook/Bishop/Knight
        // Right shift captures
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & (m_white_rooks_bit | m_white_bishops_bit | m_white_knights_bit) & ~FIRST_ROW_BITBOARD & ~m_last_destination_bit))
        {
            *move_list_end++ = Move(destination + 7, destination);
        }
        // Left shift captures
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & (m_white_rooks_bit | m_white_bishops_bit | m_white_knights_bit) & ~FIRST_ROW_BITBOARD & ~m_last_destination_bit))
        {
            *move_list_end++ = Move(destination + 9, destination);
        }
        // King-Rook/Bishop/Knight
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & (m_white_rooks_bit | m_white_bishops_bit | m_white_knights_bit) & ~m_last_destination_bit))
        {
            if (newKingSquareIsSafe(destination))
                *move_list_end++ = Move(m_black_king_position, destination);
        }
        // Knight-Queen/Rook
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & (m_white_rooks_bit | m_white_queens_bit) & ~m_last_destination_bit))
            {
                *move_list_end++ = Move(origin, destination);
            }
        }
        // Bishop-Queen/Rook
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & (m_white_rooks_bit | m_white_queens_bit) & ~m_last_destination_bit))
            {
                *move_list_end++ = Move(origin, destination);
            }
        }
        // Rook-Queen
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_white_queens_bit & ~m_last_destination_bit))
            {
                *move_list_end++ = Move(origin, destination);
            }
        }
    }
    return move_list_end;
}
ScoredMove *BitPosition::setSafeMovesAndScores(ScoredMove *&move_list_start)
{
    setAttackedSquares();
    ScoredMove *move_list_end = move_list_start; // Initially, end points to start
    // Generate moves for all pieces
    BitPosition::pawnSafeMoves(move_list_end);
    BitPosition::knightSafeMoves(move_list_end);
    BitPosition::bishopSafeMoves(move_list_end);
    BitPosition::rookSafeMoves(move_list_end);
    BitPosition::queenSafeMoves(move_list_end);
    BitPosition::kingNonCapturesAndPawnCaptures(move_list_end);

    // Scoring the moves
    if (m_turn)
    {
        for (ScoredMove *move = move_list_start; move < move_list_end; ++move)
        {
            unsigned short origin_square = move->getOriginSquare();
            unsigned short destination_square = move->getDestinationSquare();
            uint64_t origin_bit = (1ULL << origin_square);
            uint64_t destination_bit = (1ULL << destination_square);

            // Bonus for leaving unsafe square
            if ((origin_bit & m_unsafe_squares) != 0)
            {
                if ((origin_bit & m_white_pawns_bit) != 0)
                    move->score += 4;

                else if ((origin_bit & m_white_knights_bit) != 0)
                    move->score += 10;

                else if ((origin_bit & m_white_bishops_bit) != 0)
                    move->score += 12;

                else if ((origin_bit & m_white_rooks_bit) != 0)
                    move->score += 20;

                else
                    move->score += 36;
            }

            // Captured piece
            if ((destination_bit & m_black_pieces_bit) != 0)
            {
                if ((destination_bit & m_black_pawns_bit) != 0)
                    move->score += 5;
                else if ((destination_bit & m_black_knights_bit) != 0)
                    move->score += 11;
                else if ((destination_bit & m_black_bishops_bit) != 0)
                    move->score += 13;
                else if ((destination_bit & m_black_rooks_bit) != 0)
                    move->score += 21;
                else
                    move->score += 37;
            }
        }
    }
    else // Black turn similar
    {
        for (ScoredMove *move = move_list_start; move < move_list_end; ++move)
        {
            unsigned short origin_square = move->getOriginSquare();
            unsigned short destination_square = move->getDestinationSquare();
            uint64_t origin_bit = (1ULL << origin_square);
            uint64_t destination_bit = (1ULL << destination_square);

            // Bonus for leaving unsafe square
            if ((origin_bit & m_unsafe_squares) != 0)
            {
                if ((origin_bit & m_black_pawns_bit) != 0)
                    move->score += 4;

                else if ((origin_bit & m_black_knights_bit) != 0)
                    move->score += 10;

                else if ((origin_bit & m_black_bishops_bit) != 0)
                    move->score += 12;

                else if ((origin_bit & m_black_rooks_bit) != 0)
                    move->score += 20;

                else
                    move->score += 36;
            }

            // Captured piece
            if ((destination_bit & m_white_pieces_bit) != 0)
            {
                if ((destination_bit & m_white_pawns_bit) != 0)
                    move->score += 5;
                else if ((destination_bit & m_white_knights_bit) != 0)
                    move->score += 11;
                else if ((destination_bit & m_white_bishops_bit) != 0)
                    move->score += 13;
                else if ((destination_bit & m_white_rooks_bit) != 0)
                    move->score += 21;
                else
                    move->score += 36;
            }
        }
    }
    return move_list_end;
}
Move *BitPosition::setBadCapturesOrUnsafeMoves(Move *&move_list_start)
{
    Move *move_list_end = move_list_start; // Initially, end points to start
    // Generate moves for all pieces
    BitPosition::pawnBadCapturesOrUnsafeNonCaptures(move_list_end);
    BitPosition::knightBadCapturesOrUnsafeNonCaptures(move_list_end);
    BitPosition::bishopBadCapturesOrUnsafeNonCaptures(move_list_end);
    BitPosition::rookBadCapturesOrUnsafeNonCaptures(move_list_end);
    BitPosition::queenBadCapturesOrUnsafeNonCaptures(move_list_end);
    return move_list_end;
}

// For Alpha-Beta in check
Move *BitPosition::setMovesInCheck(Move *&move_list_start)
{
    Move *move_list_end = move_list_start; // Initially, end points to start
    BitPosition::setCheckInfoAfterMove();
    BitPosition::setPins();

    if (m_num_checks == 1)
    {
        if (m_check_rays != 0) // Captures in check, king moves and blocks
        {
            BitPosition::inCheckOrderedCapturesAndKingMoves(move_list_end);
            BitPosition::inCheckPawnBlocks(move_list_end);
            BitPosition::inCheckKnightBlocks(move_list_end);
            BitPosition::inCheckBishopBlocks(move_list_end);
            BitPosition::inCheckRookBlocks(move_list_end);
            BitPosition::inCheckQueenBlocks(move_list_end);
        }
        else // Captures in check and king moves
        {
            BitPosition::inCheckOrderedCapturesAndKingMoves(move_list_end);
        }
    }
    else // Only king moves
    {
        BitPosition::kingAllMovesInCheck(move_list_end);
    }
    return move_list_end;
}

// For Quiesence
ScoredMove *BitPosition::setCapturesAndScores(ScoredMove *&move_list_start)
// Scores and also update check info per moves arrays, so that when making move we don't need to recompute check stuff
{
    ScoredMove *move_list_end = move_list_start; // Initially, end points to start
    BitPosition::pawnCapturesAndQueenProms(move_list_end);
    BitPosition::knightCaptures(move_list_end);
    BitPosition::bishopCaptures(move_list_end);
    BitPosition::rookCaptures(move_list_end);
    BitPosition::queenCaptures(move_list_end);
    BitPosition::kingCaptures(move_list_end);

    if (m_turn)
    {
        for (ScoredMove *move = move_list_start; move < move_list_end; ++move)
        {
            unsigned short origin_square = move->getOriginSquare();
            unsigned short destination_square = move->getDestinationSquare();
            uint64_t origin_bit = (1ULL << origin_square);
            uint64_t destination_bit = (1ULL << destination_square);
            // Moving piece score
            // if ((origin_bit & m_white_pawns_bit) != 0)
            //     move->score -= 1;
            // else if ((origin_bit & m_white_knights_bit) != 0)
            //     move->score -= 2;
            // else if ((origin_bit & m_white_bishops_bit) != 0)
            //     move->score -= 3;
            // else if ((origin_bit & m_white_rooks_bit) != 0)
            //     move->score -= 4;
            // else if (((origin_bit & m_white_queens_bit) != 0))
            //     move->score -= 5;

            // Capture score
            if ((destination_bit & m_black_pawns_bit) != 0)
                move->score += 5;
            else if ((destination_bit & m_black_knights_bit) != 0)
                move->score += 11;
            else if ((destination_bit & m_black_bishops_bit) != 0)
                move->score += 13;
            else if ((destination_bit & m_black_rooks_bit) != 0)
                move->score += 21;
            else
                move->score += 37;
        }
    }
    else
    {
        for (ScoredMove *move = move_list_start; move < move_list_end; ++move)
        {
            unsigned short origin_square = move->getOriginSquare();
            unsigned short destination_square = move->getDestinationSquare();
            uint64_t origin_bit = (1ULL << origin_square);
            uint64_t destination_bit = (1ULL << destination_square);
            // Moving piece score
            // if ((origin_bit & m_black_pawns_bit) != 0)
            //     move->score -= 1;
            // else if ((origin_bit & m_black_knights_bit) != 0)
            //     move->score -= 2;
            // else if ((origin_bit & m_black_bishops_bit) != 0)
            //     move->score -= 3;
            // else if ((origin_bit & m_black_rooks_bit) != 0)
            //     move->score -= 4;
            // else if (((origin_bit & m_black_queens_bit) != 0))
            //     move->score -= 5;

            // Capture score
            if ((destination_bit & m_white_pawns_bit) != 0)
                move->score += 5;
            else if ((destination_bit & m_white_knights_bit) != 0)
                move->score += 11;
            else if ((destination_bit & m_white_bishops_bit) != 0)
                move->score += 13;
            else if ((destination_bit & m_white_rooks_bit) != 0)
                move->score += 21;
            else
                move->score += 36;
        }
    }
    return move_list_end;
}
Move *BitPosition::setOrderedCapturesInCheck(Move *&move_list_start)
{
    Move *move_list_end = move_list_start; // Initially, end points to start
    BitPosition::setCheckInfoAfterMove();
    BitPosition::setPins();

    if (m_num_checks == 1)
        BitPosition::inCheckOrderedCaptures(move_list_end);
    else
        BitPosition::kingCaptures(move_list_end);

    return move_list_end;
}

// Looping through moves
ScoredMove BitPosition::nextScoredMove(ScoredMove *&currentMove, ScoredMove *endMoves)
// Now return next best move (scores should already be set in search)
{
    while (currentMove < endMoves)
    {
        std::swap(*currentMove, *std::max_element(currentMove, endMoves));

        // Check if the current move is legal
        if (isLegal(*currentMove))
        {
            // We only set blockers once if there is a legal move
            if (not m_blockers_set)
                BitPosition::setBlockers();

           return *currentMove++;
        }

        // If not legal, continue to the next move
        ++currentMove;
    }
    return ScoredMove(); // Return a default move if no legal moves are found
}
Move BitPosition::nextMove(Move *&currentMove, Move *endMoves)
// For this case moves are generated in order
{
    while (currentMove < endMoves)
    {
        // Check if the current move is legal
        if (isLegal(*currentMove))
        {
            // We only set blockers once if there is a legal move
            if (not m_blockers_set)
                BitPosition::setBlockers();

            return *currentMove++;
        }

        // If not legal, continue to the next move
        ++currentMove;
    }
    return Move(0);
}
ScoredMove BitPosition::nextScoredMove(ScoredMove *&currentMove, ScoredMove *endMoves, Move ttMove)
// Now return next best move (scores should already be set in search)
{
    while (currentMove < endMoves) 
    {
        std::swap(*currentMove, *std::max_element(currentMove, endMoves));

        // Check if the current move is legal
        if (currentMove->getData() != ttMove.getData() && isLegal(*currentMove))
        {
            // We only set blockers once if there is a legal move
            if (not m_blockers_set)
                BitPosition::setBlockers();

            return *currentMove++;
        }

        // If not legal, continue to the next move
        ++currentMove;
    }
    return ScoredMove(); // Return a default move if no legal moves are found
}
Move BitPosition::nextMove(Move *&currentMove, Move *endMoves, Move ttMove)
// For this case moves are generated in order
{
    while (currentMove < endMoves)
    {
        // Check if the current move is legal
        if (currentMove->getData() != ttMove.getData() && isLegal(*currentMove))
        {
            // We only set blockers once if there is a legal move
            if (not m_blockers_set)
                BitPosition::setBlockers();

            return *currentMove++;
        }

        // If not legal, continue to the next move
        ++currentMove;
    }
    return Move(0);
}

// Moving pieces functions
void BitPosition::setPiece(uint64_t origin_bit, uint64_t destination_bit)
// Move a piece (except king), given an origin bit and destination bit
{
    if (m_turn)
    {
        if ((origin_bit & m_white_pawns_bit) != 0)
        {
            m_white_pawns_bit &= ~origin_bit;
            m_white_pawns_bit |= destination_bit;
            m_moved_piece = 0;
            m_is_check = isPawnCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
        }
        else if ((origin_bit & m_white_knights_bit) != 0)
        {
            m_white_knights_bit &= ~origin_bit;
            m_white_knights_bit |= destination_bit;
            m_moved_piece = 1;
            m_is_check = isKnightCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
        }
        else if ((origin_bit & m_white_bishops_bit) != 0)
        {
            m_white_bishops_bit &= ~origin_bit;
            m_white_bishops_bit |= destination_bit;
            m_moved_piece = 2;
            m_is_check = isBishopCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
        }
        else if ((origin_bit & m_white_rooks_bit) != 0)
        {
            m_white_rooks_bit &= ~origin_bit;
            m_white_rooks_bit |= destination_bit;
            m_moved_piece = 3;
            m_is_check = isRookCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
        }
        else
        {
            m_white_queens_bit &= ~origin_bit;
            m_white_queens_bit |= destination_bit;
            m_moved_piece = 4;
            m_is_check = isQueenCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
        }
    }
    else
    {
        if ((origin_bit & m_black_pawns_bit) != 0)
        {
            m_black_pawns_bit &= ~origin_bit;
            m_black_pawns_bit |= destination_bit;
            m_moved_piece = 0;
            m_is_check = isPawnCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
        }
        else if ((origin_bit & m_black_knights_bit) != 0)
        {
            m_black_knights_bit &= ~origin_bit;
            m_black_knights_bit |= destination_bit;
            m_moved_piece = 1;
            m_is_check = isKnightCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
        }
        else if ((origin_bit & m_black_bishops_bit) != 0)
        {
            m_black_bishops_bit &= ~origin_bit;
            m_black_bishops_bit |= destination_bit;
            m_moved_piece = 2;
            m_is_check = isBishopCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
        }
        else if ((origin_bit & m_black_rooks_bit) != 0)
        {
            m_black_rooks_bit &= ~origin_bit;
            m_black_rooks_bit |= destination_bit;
            m_moved_piece = 3;
            m_is_check = isRookCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
        }
        else
        {
            m_black_queens_bit &= ~origin_bit;
            m_black_queens_bit |= destination_bit;
            m_moved_piece = 4;
            m_is_check = isQueenCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
        }
    }
}

void BitPosition::storePlyInfo()
{
    m_wkcastling_array[m_ply] = m_white_kingside_castling;
    m_wqcastling_array[m_ply] = m_white_queenside_castling;
    m_bkcastling_array[m_ply] = m_black_kingside_castling;
    m_bqcastling_array[m_ply] = m_black_queenside_castling;

    m_diagonal_pins_array[m_ply] = m_diagonal_pins;
    m_straight_pins_array[m_ply] = m_straight_pins;

    m_blockers_array[m_ply] = m_blockers;
    m_unsafe_squares_array[m_ply] = m_unsafe_squares;

    m_50_move_count_array[m_ply] = m_50_move_count;

    m_last_destination_bit_array[m_ply] = m_last_destination_bit;
    m_psquare_array[m_ply] = m_psquare;

    // m_fen_array[m_ply] = (*this).toFenString(); // For debugging purposes
}
void BitPosition::resetPlyInfo()
// This member function is used when either opponent or engine makes a capture.
// To make the ply info smaller. For a faster 3 fold repetition check.
// It isn't used when making a move in search!
{
    m_ply = 0;
    m_wkcastling_array.fill(false);
    m_wqcastling_array.fill(false);
    m_bkcastling_array.fill(false);
    m_bqcastling_array.fill(false);

    m_diagonal_pins_array.fill(0);
    m_straight_pins_array.fill(0);
    m_blockers_array.fill(0);

    m_zobrist_keys_array.fill(0);
    m_zobrist_keys_array[63] = m_zobrist_key;

    m_captured_piece_array.fill(0);

    m_last_origin_square_array.fill(0);
    m_last_destination_square_array.fill(0);
    m_moved_piece_array.fill(0);
    m_promoted_piece_array.fill(0);
    m_psquare_array.fill(0);

    m_unsafe_squares_array.fill(0);
    m_50_move_count_array.fill(0);

    m_last_destination_bit_array.fill(0);
}
void BitPosition::storePlyInfoInCaptures()
{
    m_diagonal_pins_array[m_ply] = m_diagonal_pins;
    m_straight_pins_array[m_ply] = m_straight_pins;
    m_blockers_array[m_ply] = m_blockers;
    m_last_destination_bit_array[m_ply] = m_last_destination_bit;

    // m_fen_array[m_ply] = (*this).toFenString(); // For debugging purposes
}

bool BitPosition::moveIsReseter(Move move)
// Return if move is a capture
{
    uint64_t destination_bit{1ULL << move.getDestinationSquare()};
    uint64_t origin_bit{1ULL << move.getDestinationSquare()};
    if (m_turn)
    {
        if ((m_black_pieces_bit & destination_bit) != 0)
            return true;
        if ((m_white_pawns_bit & origin_bit) != 0 && m_psquare == destination_bit)
            return true;
    }
    else
    {
        if ((m_white_pieces_bit & destination_bit) != 0)
            return true;
        if ((m_black_pawns_bit & origin_bit) != 0 && m_psquare == destination_bit)
            return true;
    }
    return false;
}

template <typename T>
void BitPosition::makeMove(T move)
// Move piece and switch white and black roles, without rotating the board.
// After making move, set pins and checks to 0.
{
    // std::string fen_before{(*this).toFenString()}; // Debugging purpose
    m_blockers_set = false;
    BitPosition::storePlyInfo(); // store current state for unmake move
    m_50_move_count++;

    m_last_origin_square = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << m_last_origin_square);
    m_last_destination_square = move.getDestinationSquare();
    m_last_destination_bit = (1ULL << m_last_destination_square);
    m_captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion (Used for updating check info)
    m_is_check = false;

    if (m_turn) // White's move
    {
        if (m_last_origin_square == 0) // Moving from a1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_origin_square == 7) // Moving from h1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_destination_square == 63) // Capturing on a8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_destination_square == 56) // Capturing on h8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_bit == m_white_king_bit) // Moving king
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
            m_white_king_position = m_last_destination_square;
            m_moved_piece = 5;

            // Update NNUE input (after moving the king)
            NNUE::moveWhiteKingNNUEInput(*this);
            m_is_check = isDiscoverCheckForBlack(m_last_origin_square, m_last_destination_square);
        }
        // Moving any piece except king
        else
        {
            BitPosition::setPiece(origin_bit, m_last_destination_bit);
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * m_moved_piece + m_last_origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * m_moved_piece + m_last_destination_square);
        }
        // Captures (Non passant)
        if ((m_last_destination_bit & m_black_pawns_bit) != 0)
        {
            m_50_move_count = 1;
            m_black_pawns_bit &= ~m_last_destination_bit;
            m_captured_piece = 0;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_black_knights_bit) != 0)
        {
            m_50_move_count = 1;
            m_black_knights_bit &= ~m_last_destination_bit;
            m_captured_piece = 1;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_black_bishops_bit) != 0)
        {
            m_50_move_count = 1;
            m_black_bishops_bit &= ~m_last_destination_bit;
            m_captured_piece = 2;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_black_rooks_bit) != 0)
        {
            m_50_move_count = 1;
            m_black_rooks_bit &= ~m_last_destination_bit;
            m_captured_piece = 3;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_black_queens_bit) != 0)
        {
            m_50_move_count = 1;
            m_black_queens_bit &= ~m_last_destination_bit;
            m_captured_piece = 4;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + m_last_destination_square);
        }

        // Promotions, castling and passant
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            if (move.getData() == 16772) // White kingside castling
            {
                m_white_rooks_bit &= ~128;
                m_white_rooks_bit |= 32;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares

                m_is_check = isRookCheckOrDiscoverForBlack(7, 5);

                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 7);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 5);
            }
            else if (move.getData() == 16516) // White queenside castling
            {
                m_white_rooks_bit &= ~1;
                m_white_rooks_bit |= 8;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares

                m_is_check = isRookCheckOrDiscoverForBlack(0, 3);

                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + 3);
            }
            else if ((m_last_destination_bit & EIGHT_ROW_BITBOARD) != 0) // Promotions
            {
                m_all_pieces_bit &= ~origin_bit;
                m_white_pawns_bit &= ~m_last_destination_bit;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_destination_square);

                m_promoted_piece = move.getPromotingPiece() + 1;
                if (m_promoted_piece == 4) // Queen promotion
                {
                    m_white_queens_bit |= m_last_destination_bit;
                    m_is_check = isQueenCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + m_last_destination_square);
                }
                else if (m_promoted_piece == 3) // Rook promotion
                {
                    m_white_rooks_bit |= m_last_destination_bit;
                    m_is_check = isRookCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + m_last_destination_square);
                }
                else if (m_promoted_piece == 2) // Bishop promotion
                {
                    m_white_bishops_bit |= m_last_destination_bit;
                    m_is_check = isBishopCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + m_last_destination_square);
                }
                else // Knight promotion
                {
                    m_white_knights_bit |= m_last_destination_bit;
                    m_is_check = isKnightCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + m_last_destination_square);
                }
            }
            else // Passant
            {
                m_50_move_count = 1;
                m_black_pawns_bit &= ~shift_down(m_last_destination_bit);
                m_captured_piece = 0;
                if (not m_is_check)
                    m_is_check = isDiscoverCheckForBlackAfterPassant(m_last_origin_square, m_last_destination_square);
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_destination_square - 8);
            }
        }
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
        
        // Updating passant square
        if (m_moved_piece == 0 && (m_last_destination_square - m_last_origin_square) == 16)
            m_psquare = m_last_origin_square + 8;
        else
            m_psquare = 0;

        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
    }
    else // Black's move
    {
        if (m_last_origin_square == 56) // Moving from a8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_origin_square == 63) // Moving from h8
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_destination_square == 0) // Capturing on a1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_destination_square == 7) // Capturing on h1
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_white_kingside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_bit == m_black_king_bit) // Moving king
        {
            int caslting_key_index{m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3)};
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            m_black_kingside_castling = false;
            m_black_queenside_castling = false;
            caslting_key_index = (m_white_kingside_castling | (m_white_queenside_castling << 1) | (m_black_kingside_castling << 2) | (m_black_queenside_castling << 3));
            m_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            m_black_king_bit = m_last_destination_bit;
            m_black_king_position = m_last_destination_square;
            m_moved_piece = 5;

            m_is_check = isDiscoverCheckForWhite(m_last_origin_square, m_last_destination_square);

            // Update NNUE input (after moving the king)
            NNUE::moveBlackKingNNUEInput(*this);
        }
        // Moving any piece except the king
        else
        {
            BitPosition::setPiece(origin_bit, m_last_destination_bit);
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * (5 + m_moved_piece) + m_last_origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * (5 + m_moved_piece) + m_last_destination_square);
        }

        // Captures (Non passant)
        if ((m_last_destination_bit & m_white_pawns_bit) != 0)
        {
            m_50_move_count = 1;
            m_white_pawns_bit &= ~m_last_destination_bit;
            m_captured_piece = 0;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_white_knights_bit) != 0)
        {
            m_50_move_count = 1;
            m_white_knights_bit &= ~m_last_destination_bit;
            m_captured_piece = 1;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_white_bishops_bit) != 0)
        {
            m_50_move_count = 1;
            m_white_bishops_bit &= ~m_last_destination_bit;
            m_captured_piece = 2;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_white_rooks_bit) != 0)
        {
            m_50_move_count = 1;
            m_white_rooks_bit &= ~m_last_destination_bit;
            m_captured_piece = 3;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + m_last_destination_square);
        }
        else if ((m_last_destination_bit & m_white_queens_bit) != 0)
        {
            m_50_move_count = 1;
            m_white_queens_bit &= ~m_last_destination_bit;
            m_captured_piece = 4;
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + m_last_destination_square);
        }

        // Promotions, passant and Castling
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            if (move.getData() == 20412) // Black kingside castling
            {
                m_is_check = isRookCheckOrDiscoverForWhite(63, 61);

                m_black_rooks_bit &= ~9223372036854775808ULL;
                m_black_rooks_bit |= 2305843009213693952ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 63);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 61);
            }
            else if (move.getData() == 20156) // Black queenside castling
            {
                m_is_check = isRookCheckOrDiscoverForWhite(56, 59);
                
                m_black_rooks_bit &= ~72057594037927936ULL;
                m_black_rooks_bit |= 576460752303423488ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 56);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + 59);
            }
            else if ((m_last_destination_bit & FIRST_ROW_BITBOARD) != 0) // Promotions
            {
                m_all_pieces_bit &= ~origin_bit;
                m_black_pawns_bit &= ~m_last_destination_bit;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_destination_square);

                m_promoted_piece = move.getPromotingPiece() + 1;
                if (m_promoted_piece == 4) // Queen promotion
                {
                    m_black_queens_bit |= m_last_destination_bit;
                    m_is_check = isQueenCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + m_last_destination_square);
                }
                else if (m_promoted_piece == 3) // Rook promotion
                {
                    m_black_rooks_bit |= m_last_destination_bit;
                    m_is_check = isRookCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + m_last_destination_square);
                }
                else if (m_promoted_piece == 2) // Bishop promotion
                {
                    m_black_bishops_bit |= m_last_destination_bit;
                    m_is_check = isBishopCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + m_last_destination_square);
                }
                else // Knight promotion
                {
                    m_black_knights_bit |= m_last_destination_bit;
                    m_is_check = isKnightCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
                    // Set NNUE input
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + m_last_destination_square);
                }
            }
            else // Passant
            {
                m_50_move_count = 1;
                // Remove captured piece
                m_white_pawns_bit &= ~shift_up(m_last_destination_bit);
                m_captured_piece = 0;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_destination_square + 8);
                if (not m_is_check)
                    m_is_check = isDiscoverCheckForWhiteAfterPassant(m_last_origin_square, m_last_destination_square);
            }
        }
        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
        
        // Updating passant square
        if (m_moved_piece == 0 && (m_last_origin_square - m_last_destination_square) == 16)
            m_psquare = m_last_origin_square - 8;
        else
            m_psquare = 0;

        m_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[m_psquare];
    }

    BitPosition::setAllPiecesBits();

    m_turn = not m_turn;
    BitPosition::updateZobristKeyPiecePartAfterMove(m_last_origin_square, m_last_destination_square);
    m_zobrist_key ^= zobrist_keys::blackToMoveZobristNumber;
    // Note, we store the zobrist key in it's array when making the move. However the rest of the ply info
    // is stored when making the next move to be able to go back.
    // So we store it in the m_ply+1 position because the initial position (or position after capture) is the m_ply 0.

    m_captured_piece_array[m_ply] = m_captured_piece;

    m_ply++;

    m_zobrist_keys_array[63 - m_ply] = m_zobrist_key;

    // bool isCheck{m_is_check};
    // setCheckInfoOnInitialization();
    // if (m_is_check != isCheck)
    // {
    //     std::cout << "Fen: " << fen_before << "\n";
    //     std::cout << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }

    // Debugging purposes
    // int16_t eval_1{NNUE::evaluationFunction(true)};
    // NNUE::initializeNNUEInput(*this);
    // int16_t eval_2{NNUE::evaluationFunction(true)};
    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // if (eval_1 != eval_2)
    // {
    //     std::cout << "makeMove \n";
    //     std::cout << "fen before: " << fen_before << "\n";
    //     std::cout << "fen after: " << (*this).toFenString() << "\n";
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "Moved piece : " << m_moved_piece << "\n";
    //     std::cout << "Captured piece : " << m_captured_piece << "\n";
    //     std::cout << "Promoted piece : " << m_promoted_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     (*this).printBitboards();
    //     std::exit(EXIT_FAILURE);
    // }
}
template <typename T>
void BitPosition::unmakeMove(T move)
// Takes a move and undoes the move accordingly, updating all position attributes. When the engine transverses the tree of moves it will keep
// track of some irreversible aspects of the game at each ply.These are(white castling rights, black castling rights, passant square,
// capture index, pins, checks).
{
    // If a move was made before, that means previous position had blockers set (which we restore from ply info)
    m_blockers_set = true;

    m_zobrist_keys_array[63 - m_ply] = 0;

    m_ply--;

    // Update irreversible aspects
    m_white_kingside_castling = m_wkcastling_array[m_ply];
    m_white_queenside_castling = m_wqcastling_array[m_ply];
    m_black_kingside_castling = m_bkcastling_array[m_ply];
    m_black_queenside_castling = m_bqcastling_array[m_ply];

    m_diagonal_pins = m_diagonal_pins_array[m_ply];
    m_straight_pins = m_straight_pins_array[m_ply];
    m_blockers = m_blockers_array[m_ply];
    m_unsafe_squares = m_unsafe_squares_array[m_ply];
    m_psquare = m_psquare_array[m_ply];

    m_50_move_count = m_50_move_count_array[m_ply];
    m_last_destination_bit = m_last_destination_bit_array[m_ply];

    // Get irreversible info
    unsigned short previous_captured_piece{m_captured_piece_array[m_ply]};

    // Update aspects to not recompute
    m_zobrist_key = m_zobrist_keys_array[63 - m_ply];

    // Get move info
    unsigned short origin_square{move.getOriginSquare()};
    uint64_t origin_bit{1ULL << origin_square};
    unsigned short destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{1ULL << destination_square};

    if (m_turn) // Last move was black
    {
        // Castling, Passant and promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
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
            else if ((destination_bit & FIRST_ROW_BITBOARD) != 0)
            {
                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};

                m_black_pawns_bit |= origin_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);

                if (promoting_piece == 12288) // Unpromote queen
                {
                    m_black_queens_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                }
                else if (promoting_piece == 8192) // Unpromote rook
                {
                    m_black_rooks_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                }
                else if (promoting_piece == 4096) // Unpromote bishop
                {
                    m_black_bishops_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                }
                else // Unpromote knight
                {
                    m_black_knights_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                }
                // Unmaking captures in promotions
                if (previous_captured_piece != 7)
                {
                    if (previous_captured_piece == 1) // Uncapture knight
                    {
                        m_white_knights_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                    }
                    else if (previous_captured_piece == 2) // Uncapture bishop
                    {
                        m_white_bishops_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                    }
                    else if (previous_captured_piece == 3) // Uncapture rook
                    {
                        m_white_rooks_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                    }
                    else // Uncapture queen
                    {
                        m_white_queens_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                    }
                }
            }
            else // Passant
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                m_white_pawns_bit |= shift_up(destination_bit);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, destination_square + 8);
            }
        }

        // Non special moves
        else
        {
            if ((destination_bit & m_black_pawns_bit) != 0) // Unmove pawn
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);
            }
            else if ((destination_bit & m_black_knights_bit) != 0) // Unmove knight
            {
                m_black_knights_bit |= origin_bit;
                m_black_knights_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + origin_square);
            }
            else if ((destination_bit & m_black_bishops_bit) != 0) // Unmove bishop
            {
                m_black_bishops_bit |= origin_bit;
                m_black_bishops_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + origin_square);
            }
            else if ((destination_bit & m_black_rooks_bit) != 0) // Unmove rook
            {
                m_black_rooks_bit |= origin_bit;
                m_black_rooks_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + origin_square);
            }
            else if ((destination_bit & m_black_queens_bit) != 0) // Unmove queen
            {
                m_black_queens_bit |= origin_bit;
                m_black_queens_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + origin_square);
            }
            else // Unmove king
            {
                m_black_king_bit = origin_bit;
                m_black_king_position = origin_square;
                NNUE::moveBlackKingNNUEInput(*this);
            }
            // Unmaking captures
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 0) // Uncapture pawn
                {
                    m_white_pawns_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, destination_square);
                }
                else if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_white_knights_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_white_bishops_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_white_rooks_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                }
                else // Uncapture queen
                {
                    m_white_queens_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                }
            }
        }
    }
    else // Last move was white
    {
        // Special moves
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
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
            else if ((destination_bit & EIGHT_ROW_BITBOARD) != 0)
            {
                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};

                m_white_pawns_bit |= origin_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);

                if (promoting_piece == 12288) // Unpromote queen
                {
                    m_white_queens_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                }
                else if (promoting_piece == 8192) // Unpromote rook
                {
                    m_white_rooks_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                }
                else if (promoting_piece == 4096) // Unpromote bishop
                {
                    m_white_bishops_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                }
                else // Unpromote knight
                {
                    m_white_knights_bit &= ~destination_bit;
                    NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                }
                // Unmaking captures in promotions
                if (previous_captured_piece != 7)
                {
                    if (previous_captured_piece == 1) // Uncapture knight
                    {
                        m_black_knights_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                    }
                    else if (previous_captured_piece == 2) // Uncapture bishop
                    {
                        m_black_bishops_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                    }
                    else if (previous_captured_piece == 3) // Uncapture rook
                    {
                        m_black_rooks_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                    }
                    else // Uncapture queen
                    {
                        m_black_queens_bit |= destination_bit;
                        NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                    }
                }
            }
            else // Passant
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;

                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square);

                m_black_pawns_bit |= shift_down(destination_bit);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square - 8);
            }
        }

        // Non special moves
        else
        {
            if ((destination_bit & m_white_pawns_bit) != 0) // Unmove pawn
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);
            }
            else if ((destination_bit & m_white_knights_bit) != 0) // Unmove knight
            {
                m_white_knights_bit |= origin_bit;
                m_white_knights_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + origin_square);
            }
            else if ((destination_bit & m_white_bishops_bit) != 0) // Unmove bishop
            {
                m_white_bishops_bit |= origin_bit;
                m_white_bishops_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + origin_square);
            }
            else if ((destination_bit & m_white_rooks_bit) != 0) // Unmove rook
            {
                m_white_rooks_bit |= origin_bit;
                m_white_rooks_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + origin_square);
            }
            else if ((destination_bit & m_white_queens_bit) != 0) // Unmove queen
            {
                m_white_queens_bit |= origin_bit;
                m_white_queens_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + origin_square);
            }
            else // Unmove king
            {
                m_white_king_bit = origin_bit;
                m_white_king_position = origin_square;
                NNUE::moveWhiteKingNNUEInput(*this);
            }

            // Unmaking captures
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 0) // Uncapture pawn
                {
                    m_black_pawns_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                }
                else if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_black_knights_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_black_bishops_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_black_rooks_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                }
                else // Uncapture queen
                {
                    m_black_queens_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                }
            }
        }
    }
    BitPosition::setAllPiecesBits();
    m_turn = not m_turn;

    // Debugging purposes
    // int16_t eval_1{NNUE::evaluationFunction(true)};
    // NNUE::initializeNNUEInput(*this);
    // int16_t eval_2{NNUE::evaluationFunction(true)};
    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // if (eval_1 != eval_2)
    // {
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "unmakeMove \n";
    //     std::cout << "fen: " << (*this).toFenString() << "\n";
    //     std::cout << "Turn : " << m_turn << "\n";
    //     std::cout << "Origin square : " << origin_square << "\n";
    //     std::cout << "Destination square : " << destination_square << "\n";
    //     std::cout << "Captured piece : " << previous_captured_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }
}

template <typename T>
void BitPosition::makeCapture(T move)
// Move piece and switch white and black roles, without rotating the board.
// After making move, set pins and checks to 0.
{
    // std::string fen_before{(*this).toFenString()}; // Debugging purposes
    m_blockers_set = false;
    BitPosition::storePlyInfoInCaptures(); // store current state for unmake move
    m_last_origin_square = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << m_last_origin_square);
    m_last_destination_square = move.getDestinationSquare();
    m_last_destination_bit = (1ULL << m_last_destination_square);
    m_captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion (Used for updating check info)
    m_psquare = 0;
    m_is_check = false;

    if (m_turn) // White's move
    {
        // Promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            m_moved_piece = 0;
            m_promoted_piece = 4;

            m_white_pawns_bit &= ~origin_bit;
            m_all_pieces_bit &= ~origin_bit;
            m_white_queens_bit |= m_last_destination_bit;
            m_is_check = isQueenCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + m_last_destination_square);

            // Captures (Non passant)
            if ((m_last_destination_bit & m_black_pawns_bit) != 0)
            {
                m_black_pawns_bit &= ~m_last_destination_bit;
                m_captured_piece = 0;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_black_knights_bit) != 0)
            {
                m_black_knights_bit &= ~m_last_destination_bit;
                m_captured_piece = 1;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_black_bishops_bit) != 0)
            {
                m_black_bishops_bit &= ~m_last_destination_bit;
                m_captured_piece = 2;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_black_rooks_bit) != 0)
            {
                m_black_rooks_bit &= ~m_last_destination_bit;
                m_captured_piece = 3;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_black_queens_bit) != 0)
            {
                m_black_queens_bit &= ~m_last_destination_bit;
                m_captured_piece = 4;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + m_last_destination_square);
            }
        }
        else
        {
            if (origin_bit == m_white_king_bit) // Moving king
            {
                // Update king bit and king position
                m_white_king_bit = m_last_destination_bit;
                m_white_king_position = m_last_destination_square;
                m_moved_piece = 5;

                // Update NNUE input (after moving the king)
                NNUE::moveWhiteKingNNUEInput(*this);
                m_is_check = isDiscoverCheckForBlack(m_last_origin_square, m_last_destination_square);
            }
            // Moving any piece except king
            else
            {
                BitPosition::setPiece(origin_bit, m_last_destination_bit);
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * m_moved_piece + m_last_origin_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * m_moved_piece + m_last_destination_square);
            }
            // Captures (Non passant)
            if ((m_last_destination_bit & m_black_pawns_bit) != 0)
            {
                m_black_pawns_bit &= ~m_last_destination_bit;
                m_captured_piece = 0;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_black_knights_bit) != 0)
            {
                m_black_knights_bit &= ~m_last_destination_bit;
                m_captured_piece = 1;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_black_bishops_bit) != 0)
            {
                m_black_bishops_bit &= ~m_last_destination_bit;
                m_captured_piece = 2;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_black_rooks_bit) != 0)
            {
                m_black_rooks_bit &= ~m_last_destination_bit;
                m_captured_piece = 3;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + m_last_destination_square);
            }
            else
            {
                m_black_queens_bit &= ~m_last_destination_bit;
                m_captured_piece = 4;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + m_last_destination_square);
            }
        }
    }
    else // Black's move
    {
        // Promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            m_moved_piece = 0;
            m_promoted_piece = 4;
            
            m_black_pawns_bit &= ~origin_bit;
            m_black_queens_bit |= m_last_destination_bit;
            m_all_pieces_bit &= ~origin_bit;
            m_is_check = isQueenCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);
            // Set NNUE input
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + m_last_origin_square);
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + m_last_destination_square);

            // Captures (Non passant)
            if ((m_last_destination_bit & m_white_pawns_bit) != 0)
            {
                m_white_pawns_bit &= ~m_last_destination_bit;
                m_captured_piece = 0;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_white_knights_bit) != 0)
            {
                m_white_knights_bit &= ~m_last_destination_bit;
                m_captured_piece = 1;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_white_bishops_bit) != 0)
            {
                m_white_bishops_bit &= ~m_last_destination_bit;
                m_captured_piece = 2;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_white_rooks_bit) != 0)
            {
                m_white_rooks_bit &= ~m_last_destination_bit;
                m_captured_piece = 3;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_white_queens_bit) != 0)
            {
                m_white_queens_bit &= ~m_last_destination_bit;
                m_captured_piece = 4;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + m_last_destination_square);
            }
        }
        // Non promotions
        else
        {
            if (origin_bit == m_black_king_bit) // Moving king
            {
                m_black_king_bit = m_last_destination_bit;
                m_black_king_position = m_last_destination_square;
                m_moved_piece = 5;
                m_is_check = isDiscoverCheckForWhite(m_last_origin_square, m_last_destination_square);

                // Update NNUE input (after moving the king)
                NNUE::moveBlackKingNNUEInput(*this);
            }
            // Moving any piece except the king
            else
            {
                BitPosition::setPiece(origin_bit, m_last_destination_bit);
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * (5 + m_moved_piece) + m_last_origin_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * (5 + m_moved_piece) + m_last_destination_square);
            }

            // Captures (Non passant)
            if ((m_last_destination_bit & m_white_pawns_bit) != 0)
            {
                m_white_pawns_bit &= ~m_last_destination_bit;
                m_captured_piece = 0;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_white_knights_bit) != 0)
            {
                m_white_knights_bit &= ~m_last_destination_bit;
                m_captured_piece = 1;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_white_bishops_bit) != 0)
            {
                m_white_bishops_bit &= ~m_last_destination_bit;
                m_captured_piece = 2;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + m_last_destination_square);
            }
            else if ((m_last_destination_bit & m_white_rooks_bit) != 0)
            {
                m_white_rooks_bit &= ~m_last_destination_bit;
                m_captured_piece = 3;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + m_last_destination_square);
            }
            else
            {
                m_white_queens_bit &= ~m_last_destination_bit;
                m_captured_piece = 4;
                // Set NNUE input
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + m_last_destination_square);
            }
        }
    }
    BitPosition::setAllPiecesBits();

    m_blockers_set = false;

    m_turn = not m_turn;
    m_captured_piece_array[m_ply] = m_captured_piece;
    m_ply++;

    // bool isCheck{m_is_check};
    // setCheckInfoOnInitialization();
    // if (m_is_check != isCheck)
    // {
    //     std::cout << "Fen: " << fen_before << "\n";
    //     std::cout << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }

    // Debugging purposes
    // int16_t eval_1{NNUE::evaluationFunction(true)};
    // NNUE::initializeNNUEInput(*this);
    // int16_t eval_2{NNUE::evaluationFunction(true)};
    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // if (eval_1 != eval_2)
    // {
    //     std::cout << "makeCapture \n";
    //     std::cout << "fen before: " << fen_before << "\n";
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "Moved piece : " << m_moved_piece << "\n";
    //     std::cout << "Captured piece : " << m_captured_piece << "\n";
    //     std::cout << "Promoted piece : " << m_promoted_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     std::cout << "fen after: " << (*this).toFenString() << "\n";
    //     this->printBitboards();
    //     for (const auto &str : m_fen_array)
    //     {
    //         std::cout << str << "\n";
    //     }
    //     std::exit(EXIT_FAILURE);
    // }
}
template <typename T>
void BitPosition::unmakeCapture(T move)
// Takes a move and undoes the move accordingly, updating all position attributes. When the engine transverses the tree of moves it will keep
// track of some irreversible aspects of the game at each ply.These are(white castling rights, black castling rights, passant square,
// capture index, pins, checks).
{
    // If a move was made before, that means previous position had blockers set (which we restore from ply info)
    m_blockers_set = true;

    m_ply--;

    // Update irreversible aspects
    m_diagonal_pins = m_diagonal_pins_array[m_ply];
    m_straight_pins = m_straight_pins_array[m_ply];
    m_blockers = m_blockers_array[m_ply];
    m_last_destination_bit = m_last_destination_bit_array[m_ply];

    // Get irreversible info
    unsigned short previous_captured_piece{m_captured_piece_array[m_ply]};

    // Get move info
    unsigned short origin_square{move.getOriginSquare()};
    uint64_t origin_bit{1ULL << origin_square};
    unsigned short destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{1ULL << destination_square};

    if (m_turn) // Last move was black
    {
        // Promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            m_black_pawns_bit |= origin_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);

            m_black_queens_bit &= ~destination_bit;
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);

            // Unmaking captures in promotions
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_white_knights_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_white_bishops_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_white_rooks_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                }
                else // Uncapture queen
                {
                    m_white_queens_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                }
            }
        }
        // Non promotions
        else
        {
            if ((destination_bit & m_black_pawns_bit) != 0) // Unmove pawn
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + origin_square);
            }
            else if ((destination_bit & m_black_knights_bit) != 0) // Unmove knight
            {
                m_black_knights_bit |= origin_bit;
                m_black_knights_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + origin_square);
            }
            else if ((destination_bit & m_black_bishops_bit) != 0) // Unmove bishop
            {
                m_black_bishops_bit |= origin_bit;
                m_black_bishops_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + origin_square);
            }
            else if ((destination_bit & m_black_rooks_bit) != 0) // Unmove rook
            {
                m_black_rooks_bit |= origin_bit;
                m_black_rooks_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + origin_square);
            }
            else if ((destination_bit & m_black_queens_bit) != 0) // Unmove queen
            {
                m_black_queens_bit |= origin_bit;
                m_black_queens_bit &= ~destination_bit;
                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + origin_square);
            }
            else // Unmove king
            {
                m_black_king_bit = origin_bit;
                m_black_king_position = origin_square;
                NNUE::moveBlackKingNNUEInput(*this);
            }
            // Unmaking captures
            if (previous_captured_piece == 0) // Uncapture pawn
            {
                m_white_pawns_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, destination_square);
            }
            else if (previous_captured_piece == 1) // Uncapture knight
            {
                m_white_knights_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
            }
            else if (previous_captured_piece == 2) // Uncapture bishop
            {
                m_white_bishops_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
            }
            else if (previous_captured_piece == 3) // Uncapture rook
            {
                m_white_rooks_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
            }
            else // Uncapture queen
            {
                m_white_queens_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
            }
        }
    }
    else // Last move was white
    {
        // Passant and promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            m_white_pawns_bit |= origin_bit;
            NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);

            m_white_queens_bit &= ~destination_bit;
            NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
            // Unmaking captures in promotions
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_black_knights_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_black_bishops_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_black_rooks_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
                }
                else // Uncapture queen
                {
                    m_black_queens_bit |= destination_bit;
                    NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
                }
            }
        }

        // Non special moves
        else
        {
            if ((destination_bit & m_white_pawns_bit) != 0) // Unmove pawn
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, origin_square);
            }
            else if ((destination_bit & m_white_knights_bit) != 0) // Unmove knight
            {
                m_white_knights_bit |= origin_bit;
                m_white_knights_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 + origin_square);
            }
            else if ((destination_bit & m_white_bishops_bit) != 0) // Unmove bishop
            {
                m_white_bishops_bit |= origin_bit;
                m_white_bishops_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 2 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 2 + origin_square);
            }
            else if ((destination_bit & m_white_rooks_bit) != 0) // Unmove rook
            {
                m_white_rooks_bit |= origin_bit;
                m_white_rooks_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 3 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 3 + origin_square);
            }
            else if ((destination_bit & m_white_queens_bit) != 0) // Unmove queen
            {
                m_white_queens_bit |= origin_bit;
                m_white_queens_bit &= ~destination_bit;

                NNUE::removeOnInput(m_white_king_position, m_black_king_position, 64 * 4 + destination_square);
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 4 + origin_square);
            }
            else // Unmove king
            {
                m_white_king_bit = origin_bit;
                m_white_king_position = origin_square;
                NNUE::moveWhiteKingNNUEInput(*this);
            }

            // Unmaking captures
            if (previous_captured_piece == 0) // Uncapture pawn
            {
                m_black_pawns_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 5 + destination_square);
            }
            else if (previous_captured_piece == 1) // Uncapture knight
            {
                m_black_knights_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 6 + destination_square);
            }
            else if (previous_captured_piece == 2) // Uncapture bishop
            {
                m_black_bishops_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 7 + destination_square);
            }
            else if (previous_captured_piece == 3) // Uncapture rook
            {
                m_black_rooks_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 8 + destination_square);
            }
            else // Uncapture queen
            {
                m_black_queens_bit |= destination_bit;
                NNUE::addOnInput(m_white_king_position, m_black_king_position, 64 * 9 + destination_square);
            }
        }
    }
    BitPosition::setAllPiecesBits();
    m_turn = not m_turn;

    // Debugging purposes
    // int16_t eval_1{NNUE::evaluationFunction(true)};
    // NNUE::initializeNNUEInput(*this);
    // int16_t eval_2{NNUE::evaluationFunction(true)};
    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // if (eval_1 != eval_2)
    // {
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "unmakeCapture \n";
    //     std::cout << "Turn : " << m_turn << "\n";
    //     std::cout << "Origin square : " << origin_square << "\n";
    //     std::cout << "Destination square : " << destination_square << "\n";
    //     std::cout << "Captured piece : " << previous_captured_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }
}

// Game ending functions
bool BitPosition::isStalemate() const
// This is called only in quiesence search, after having no available captures. Thus we only consider empty squares.
{
    if (m_turn)
    {
        // Single move pawn move
        uint64_t pawnAdvances{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(pawnAdvances))
        {
            if (isLegalForWhite(destination - 8, destination))
                return false;
        }
        // King move
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                return false;
        }
        // Knight move
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_all_pieces_bit))
                return false;
        }
        // Rook move
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                if (isLegalForWhite(origin, destination))
                    return false;
            }
        }
        // Bishop move
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                if (isLegalForWhite(origin, destination))
                    return false;
            }
        }
        // Queen move
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            for (unsigned short destination : getBitIndices((RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit)) & ~m_all_pieces_bit))
            {
                if (isLegalForWhite(origin, destination))
                    return false;
            }
        }
        // Passant capture
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                {
                    return false;
                }
        }
    }
    else
    {
        // Single move pawn move
        uint64_t pawnAdvances{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(pawnAdvances))
        {
            if (isLegalForBlack(destination + 8, destination))
                return false;
        }
        // King move
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                return false;
        }
        // Knight move
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_all_pieces_bit))
                return false;
        }
        // Rook move
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                if (isLegalForBlack(origin, destination))
                    return false;
            }
        }
        // Bishop move
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                if (isLegalForBlack(origin, destination))
                    return false;
            }
        }
        // Queen move
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            for (unsigned short destination : getBitIndices((RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit)) & ~m_all_pieces_bit))
            {
                if (isLegalForBlack(origin, destination))
                    return false;
            }
        }
        // Passant block or capture
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare + 8)) // Legal
                {
                    return false;
                }
        }
    }
    return true;
}
bool BitPosition::isMate() const
// This is called only in quiesence search, after having no available captures
{
    if (m_turn) // White's turn
    {
        // Only consider empty squares because we assume there are no captures
        //  Can we move king?
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                return false;
        }
        if (m_num_checks == 1) // Can we block with pieces?
        {
            // Single move pawn block
            uint64_t pawnAdvances{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
            for (unsigned short destination : getBitIndices(pawnAdvances & m_check_rays))
            {
                if (isLegalForWhite(destination - 8, destination))
                    return false;
            }
            // Double move pawn block
            for (unsigned short destination : getBitIndices(shift_up(pawnAdvances) & m_check_rays))
            {
                if (isLegalForWhite(destination - 16, destination))
                    return false;
            }
            // Knight block
            for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
            {
                for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & m_check_rays))
                        return false;
            }
            // Rook block
            for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
            {
                for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays))
                {
                    if (isLegalForWhite(origin, destination))
                        return false;
                }
            }
            // Bishop block
            for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
            {
                for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays))
                {
                    if (isLegalForWhite(origin, destination))
                        return false;
                }
            }
            // Queen block
            for (unsigned short origin : getBitIndices(m_white_queens_bit))
            {
                for (unsigned short destination : getBitIndices((RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays))
                {
                    if (isLegalForWhite(origin, destination))
                        return false;
                }
            }
            // Passant block or capture
            if (m_psquare != 0)
            {
                for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                    if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                    {
                        return false;
                    }
            }
        }
    }
    else // Black's turn
    {
        // Only consider empty squares because we assume there are no captures
        //  Can we move king?
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                return false;
        }
        if (m_num_checks == 1) // Can we block with pieces?
        {
            // Single move pawn block
            uint64_t pawnAdvances{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
            for (unsigned short destination : getBitIndices(pawnAdvances & m_check_rays))
            {
                if (isLegalForBlack(destination + 8, destination))
                    return false;
            }
            // Double move pawn block
            for (unsigned short destination : getBitIndices(shift_down(pawnAdvances) & m_check_rays))
            {
                if (isLegalForBlack(destination + 16, destination))
                    return false;
            }
            // Knight block
            for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
            {
                for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & m_check_rays))
                    return false;
            }
            // Rook block
            for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
            {
                for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays))
                {
                    if (isLegalForBlack(origin, destination))
                        return false;
                }
            }
            // Bishop block
            for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
            {
                for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays))
                {
                    if (isLegalForBlack(origin, destination))
                        return false;
                }
            }
            // Queen block
            for (unsigned short origin : getBitIndices(m_black_queens_bit))
            {
                for (unsigned short destination : getBitIndices((RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays))
                {
                    if (isLegalForBlack(origin, destination))
                        return false;
                }
            }
            // Passant block or capture
            if (m_psquare != 0)
            {
                for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                    if (kingIsSafeAfterPassant(origin, m_psquare + 8)) // Legal
                    {
                        return false;
                    }
            }
        }
    }
    return true;
}
bool BitPosition::isThreeFoldOr50MoveRule() const
{
    if (m_50_move_count >= 50)
        return true;
    // Find the last non-zero key in the array
    uint64_t lastKey = 0;
    int countFifty = 0;
    int count = 0;
    for (uint64_t key : m_zobrist_keys_array)
    {
        if (key != 0)
        {
            if (lastKey == 0)
            {
                lastKey = key;
                count++;
            }
            // Threefold rep
            else if (key == lastKey)
            {
                if (++count == 3)
                    return true;
            }
        }
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The Folowing member functions will not be used in the engine. They are only used for debugging purposes.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BitPosition::inCheckPawnBlocksNonQueenProms(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
        for (unsigned short destination : getBitIndices(single_pawn_moves_blocking_bit))
        {
            if (destination < 56) // Non promotions
            {
                *move_list++ = Move(destination - 8, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination - 8, destination, 0);
                *move_list++ = Move(destination - 8, destination, 1);
                *move_list++ = Move(destination - 8, destination, 2);
            }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
        {
            *move_list++ = Move(destination - 16, destination);
        }
        // Passant block or capture
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
    else
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
        for (unsigned short destination : getBitIndices(single_pawn_moves_blocking_bit))
        {
            if (destination > 7) // Non promotions
            {
                *move_list++ = Move(destination + 8, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination + 8, destination, 0);
                *move_list++ = Move(destination + 8, destination, 1);
                *move_list++ = Move(destination + 8, destination, 2);
            }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
        {
            *move_list++ = Move(destination + 16, destination);
        }
        // Passant block or capture
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare + 8)) // Legal
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
}
void BitPosition::inCheckPawnCapturesNonQueenProms(Move *&move_list) const
{
    if (m_turn)
    {
        // Pawn captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_check_square] & m_white_pawns_bit))
        {
            if (m_check_square < 56) // Non promotions
            {
                continue;
            }
            else // Promotions
            {
                *move_list++ = Move(origin, m_check_square, 0);
                *move_list++ = Move(origin, m_check_square, 1);
                *move_list++ = Move(origin, m_check_square, 2);
            }
        }
    }
    else
    {
        // Pawn captures from checking position
        for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_check_square] & m_black_pawns_bit))
        {
            if (m_check_square > 7) // Non promotions
            {
                continue;
            }
            else // Promotions
            {
                *move_list++ = Move(origin, m_check_square, 0);
                *move_list++ = Move(origin, m_check_square, 1);
                *move_list++ = Move(origin, m_check_square, 2);
            }
        }
    }
}
void BitPosition::inCheckPassantCaptures(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays == 0
{
    if (m_turn)
    {
        // Passant block or capture
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
    else
    {
        // Passant block or capture
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare + 8)) // Legal
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
}

void BitPosition::pawnNonCapturesNonQueenProms(Move*& move_list) const
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit))
        {
            if (destination < 56) // Non promotions
            {
                *move_list++ = Move(destination - 8, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination - 8, destination, 0);
                *move_list++ = Move(destination - 8, destination, 1);
                *move_list++ = Move(destination - 8, destination, 2);
            }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            *move_list++ = Move(destination - 16, destination);
        }
        // Right shift captures and non-queen promotions
        for (unsigned short destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 9, destination, 0);
            *move_list++ = Move(destination - 9, destination, 1);
            *move_list++ = Move(destination - 9, destination, 2);
        }
        // Left shift captures and non-queen promotions
        for (unsigned short destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_black_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 7, destination, 0);
            *move_list++ = Move(destination - 7, destination, 1);
            *move_list++ = Move(destination - 7, destination, 2);
        }
        // Passant
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_psquare] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare - 8)) // Legal
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
    else // Black moves
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~m_diagonal_pins) & ~m_all_pieces_bit};
        for (unsigned short destination : getBitIndices(single_pawn_moves_bit))
        {
            if (destination > 7) // Non promotions
            {
                *move_list++ = Move(destination + 8, destination);
            }
            else // Promotions
            {
                *move_list++ = Move(destination + 8, destination, 0);
                *move_list++ = Move(destination + 8, destination, 1);
                *move_list++ = Move(destination + 8, destination, 2);
            }
        }
        // Double moves
        for (unsigned short destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            *move_list++ = Move(destination + 16, destination);
        }
        // Right shift captures and non-queen promotions
        for (unsigned short destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 7, destination, 0);
            *move_list++ = Move(destination + 7, destination, 1);
            *move_list++ = Move(destination + 7, destination, 2);
        }
        // Left shift captures and non-queen promotions
        for (unsigned short destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~m_straight_pins) & m_white_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 9, destination, 0);
            *move_list++ = Move(destination + 9, destination, 1);
            *move_list++ = Move(destination + 9, destination, 2);
        }
        // Passant
        if (m_psquare != 0)
        {
            for (unsigned short origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_psquare] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, m_psquare + 8))
                {
                    *move_list++ = Move(origin, m_psquare, 0);
                }
        }
    }
}
void BitPosition::knightNonCaptures(Move*& move_list) const
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_all_pieces_bit))
            {
                    *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_knights_bit & ~(m_straight_pins | m_diagonal_pins)))
        {
            for (unsigned short destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_all_pieces_bit))
            {
                    *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::bishopNonCaptures(Move*& move_list) const
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_bishops_bit & ~m_straight_pins))
        {
            for (unsigned short destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::rookNonCaptures(Move*& move_list) const
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_rooks_bit & ~m_diagonal_pins))
        {
            for (unsigned short destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::queenNonCaptures(Move*& move_list) const
{
    if (m_turn)
    {
        for (unsigned short origin : getBitIndices(m_white_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_all_pieces_bit))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (unsigned short origin : getBitIndices(m_black_queens_bit))
        {
            for (unsigned short destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_all_pieces_bit))
            {
                        *move_list++ = Move(origin, destination);
            }
        }
    }
}
void BitPosition::kingNonCaptures(Move *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
        // Kingside castling
        if (m_white_kingside_castling && (m_all_pieces_bit & 96) == 0 && newWhiteKingSquareIsSafe(5) && newWhiteKingSquareIsSafe(6))
            *move_list++ = castling_moves[0];
        // Queenside castling
        if (m_white_queenside_castling && (m_all_pieces_bit & 14) == 0 && newWhiteKingSquareIsSafe(2) && newWhiteKingSquareIsSafe(3))
            *move_list++ = castling_moves[1];
    }
    else
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
        // Kingside castling
        if (m_black_kingside_castling && (m_all_pieces_bit & 6917529027641081856) == 0 && newBlackKingSquareIsSafe(61) && newBlackKingSquareIsSafe(62))
            *move_list++ = castling_moves[2];
        // Queenside castling
        if (m_black_queenside_castling && (m_all_pieces_bit & 1008806316530991104) == 0 && newBlackKingSquareIsSafe(58) && newBlackKingSquareIsSafe(59))
            *move_list++ = castling_moves[3];
    }
}

void BitPosition::kingNonCapturesInCheck(Move *&move_list) const
{
    if (m_turn)
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                *move_list++ = Move(m_white_king_position, destination);
        }
    }
    else
    {
        for (unsigned short destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                *move_list++ = Move(m_black_king_position, destination);
        }
    }
}

Move *BitPosition::setNonCaptures(Move *&move_list_start)
{
    Move *move_list_end = move_list_start; // Initially, end points to start

    BitPosition::pawnNonCapturesNonQueenProms(move_list_end);
    BitPosition::knightNonCaptures(move_list_end);
    BitPosition::bishopNonCaptures(move_list_end);
    BitPosition::rookNonCaptures(move_list_end);
    BitPosition::queenNonCaptures(move_list_end);
    BitPosition::kingNonCaptures(move_list_end);

    return move_list_end;
}
Move *BitPosition::setNonCapturesInCheck(Move *&move_list_start)
{
    Move *move_list_end = move_list_start; // Initially, end points to start

    if (m_num_checks == 1)
    {
        if (m_check_rays != 0)
        {
            BitPosition::inCheckPawnBlocksNonQueenProms(move_list_end);
            BitPosition::inCheckPawnCapturesNonQueenProms(move_list_end);
            BitPosition::inCheckKnightBlocks(move_list_end);
            BitPosition::inCheckBishopBlocks(move_list_end);
            BitPosition::inCheckRookBlocks(move_list_end);
            BitPosition::inCheckQueenBlocks(move_list_end);
            BitPosition::kingNonCapturesInCheck(move_list_end);
        }
        else // Passant captures and king moves
        {
            BitPosition::inCheckPawnCapturesNonQueenProms(move_list_end);
            BitPosition::inCheckPassantCaptures(move_list_end);
            BitPosition::kingNonCapturesInCheck(move_list_end);
        }
    }
    else
        BitPosition::kingNonCapturesInCheck(move_list_end);

    return move_list_end;
}

Move *BitPosition::setMovesInCheckTest(Move *&move_list_start)
{
    Move *move_list_end = move_list_start; // Initially, end points to start
    BitPosition::setPins();
    BitPosition::setAttackedSquares();
    BitPosition::setCheckInfoOnInitialization();

    if (m_num_checks == 1)
    {
        if (m_check_rays != 0) // Captures in check, king moves and blocks
        {
            BitPosition::inCheckOrderedCapturesAndKingMoves(move_list_end);
            BitPosition::inCheckPawnBlocks(move_list_end);
            BitPosition::inCheckKnightBlocks(move_list_end);
            BitPosition::inCheckBishopBlocks(move_list_end);
            BitPosition::inCheckRookBlocks(move_list_end);
            BitPosition::inCheckQueenBlocks(move_list_end);
        }
        else // Captures in check and king moves
        {
            BitPosition::inCheckOrderedCapturesAndKingMoves(move_list_end);
        }
    }
    else // Only king moves
    {
        BitPosition::kingAllMovesInCheck(move_list_end);
    }
    return move_list_end;
}
Move *BitPosition::setCapturesInCheckTest(Move *&move_list_start)
{
    Move *move_list_end = move_list_start; // Initially, end points to start
    BitPosition::setPins();
    BitPosition::setCheckInfoOnInitialization();

    if (m_num_checks == 1)
        BitPosition::inCheckOrderedCaptures(move_list_end);
    else
        BitPosition::kingCaptures(move_list_end);

    return move_list_end;
}

template <typename T>
void BitPosition::makeCaptureWithoutNNUE(T move)
// Same as makeCapture but without NNUE updates and updating and storing castling rights too.
{
    m_blockers_set = false;
    BitPosition::storePlyInfo(); // store current state for unmake move
    m_last_origin_square = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << m_last_origin_square);
    m_last_destination_square = move.getDestinationSquare();
    m_last_destination_bit = (1ULL << m_last_destination_square);
    m_captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion (Used for updating check info)
    m_psquare = 0;
    m_is_check = false;

    if (m_turn) // White's move
    {
        if (m_last_origin_square == 0) // Moving from a1
        {
            m_white_queenside_castling = false;
        }

        else if (m_last_origin_square == 7) // Moving from h1
        {
            m_white_kingside_castling = false;
        }

        if (m_last_destination_square == 63) // Capturing on a8
        {
            m_black_kingside_castling = false;
        }

        else if (m_last_destination_square == 56) // Capturing on h8
        {
            m_black_queenside_castling = false;
        }
        // Promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            m_moved_piece = 0;
            m_promoted_piece = 4;

            m_white_pawns_bit &= ~origin_bit;
            m_white_queens_bit |= m_last_destination_bit;
            m_is_check = isQueenCheckOrDiscoverForBlack(m_last_origin_square, m_last_destination_square);

            // Captures (Non passant)
            if ((m_last_destination_bit & m_black_pawns_bit) != 0)
            {
                m_black_pawns_bit &= ~m_last_destination_bit;
                m_captured_piece = 0;
            }
            else if ((m_last_destination_bit & m_black_knights_bit) != 0)
            {
                m_black_knights_bit &= ~m_last_destination_bit;
                m_captured_piece = 1;
            }
            else if ((m_last_destination_bit & m_black_bishops_bit) != 0)
            {
                m_black_bishops_bit &= ~m_last_destination_bit;
                m_captured_piece = 2;
            }
            else if ((m_last_destination_bit & m_black_rooks_bit) != 0)
            {
                m_black_rooks_bit &= ~m_last_destination_bit;
                m_captured_piece = 3;
            }
            else if ((m_last_destination_bit & m_black_queens_bit) != 0)
            {
                m_black_queens_bit &= ~m_last_destination_bit;
                m_captured_piece = 4;
            }
        }
        else
        {
            if (origin_bit == m_white_king_bit) // Moving king
            {
                // Update king bit and king position
                m_white_king_bit = m_last_destination_bit;
                m_white_king_position = m_last_destination_square;
                m_moved_piece = 5;
                m_is_check = isDiscoverCheckForBlack(m_last_origin_square, m_last_destination_square);

                m_white_kingside_castling = false;
                m_white_queenside_castling = false;
            }
            // Moving any piece except king
            else
            {
                BitPosition::setPiece(origin_bit, m_last_destination_bit);
            }
            // Captures (Non passant)
            if ((m_last_destination_bit & m_black_pawns_bit) != 0)
            {
                m_black_pawns_bit &= ~m_last_destination_bit;
                m_captured_piece = 0;
            }
            else if ((m_last_destination_bit & m_black_knights_bit) != 0)
            {
                m_black_knights_bit &= ~m_last_destination_bit;
                m_captured_piece = 1;
            }
            else if ((m_last_destination_bit & m_black_bishops_bit) != 0)
            {
                m_black_bishops_bit &= ~m_last_destination_bit;
                m_captured_piece = 2;
            }
            else if ((m_last_destination_bit & m_black_rooks_bit) != 0)
            {
                m_black_rooks_bit &= ~m_last_destination_bit;
                m_captured_piece = 3;
            }
            else
            {
                m_black_queens_bit &= ~m_last_destination_bit;
                m_captured_piece = 4;
            }
        }
    }
    else // Black's move
    {
        if (m_last_origin_square == 56) // Moving from a8
        {
            m_black_queenside_castling = false;
        }

        else if (m_last_origin_square == 63) // Moving from h8
        {
            m_black_kingside_castling = false;
        }

        if (m_last_destination_square == 0) // Capturing on a1
        {
            m_white_queenside_castling = false;
        }

        else if (m_last_destination_square == 7) // Capturing on h1
        {
            m_white_kingside_castling = false;
        }
        // Promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            m_moved_piece = 0;
            m_promoted_piece = 4;

            m_black_pawns_bit &= ~origin_bit;
            m_black_queens_bit |= m_last_destination_bit;
            m_is_check = isQueenCheckOrDiscoverForWhite(m_last_origin_square, m_last_destination_square);

            // Captures (Non passant)
            if ((m_last_destination_bit & m_white_pawns_bit) != 0)
            {
                m_white_pawns_bit &= ~m_last_destination_bit;
                m_captured_piece = 0;
            }
            else if ((m_last_destination_bit & m_white_knights_bit) != 0)
            {
                m_white_knights_bit &= ~m_last_destination_bit;
                m_captured_piece = 1;
            }
            else if ((m_last_destination_bit & m_white_bishops_bit) != 0)
            {
                m_white_bishops_bit &= ~m_last_destination_bit;
                m_captured_piece = 2;
            }
            else if ((m_last_destination_bit & m_white_rooks_bit) != 0)
            {
                m_white_rooks_bit &= ~m_last_destination_bit;
                m_captured_piece = 3;
            }
            else if ((m_last_destination_bit & m_white_queens_bit) != 0)
            {
                m_white_queens_bit &= ~m_last_destination_bit;
                m_captured_piece = 4;
            }
        }
        // Non promotions
        else
        {
            if (origin_bit == m_black_king_bit) // Moving king
            {
                m_black_king_bit = m_last_destination_bit;
                m_black_king_position = m_last_destination_square;
                m_moved_piece = 5;
                m_is_check = isDiscoverCheckForWhite(m_last_origin_square, m_last_destination_square);

                m_black_kingside_castling = false;
                m_black_queenside_castling = false;
            }
            // Moving any piece except the king
            else
            {
                BitPosition::setPiece(origin_bit, m_last_destination_bit);
            }

            // Captures (Non passant)
            if ((m_last_destination_bit & m_white_pawns_bit) != 0)
            {
                m_white_pawns_bit &= ~m_last_destination_bit;
                m_captured_piece = 0;
            }
            else if ((m_last_destination_bit & m_white_knights_bit) != 0)
            {
                m_white_knights_bit &= ~m_last_destination_bit;
                m_captured_piece = 1;
            }
            else if ((m_last_destination_bit & m_white_bishops_bit) != 0)
            {
                m_white_bishops_bit &= ~m_last_destination_bit;
                m_captured_piece = 2;
            }
            else if ((m_last_destination_bit & m_white_rooks_bit) != 0)
            {
                m_white_rooks_bit &= ~m_last_destination_bit;
                m_captured_piece = 3;
            }
            else
            {
                m_white_queens_bit &= ~m_last_destination_bit;
                m_captured_piece = 4;
            }
        }
    }
    m_turn = not m_turn;
    m_captured_piece_array[m_ply] = m_captured_piece;
    m_ply++;

    BitPosition::setAllPiecesBits();
}
template <typename T>
void BitPosition::unmakeCaptureWithoutNNUE(T move)
// Same as unmakeCapture but without NNUE updates and restoring castling rights too.
{
    m_blockers_set = true;

    m_ply--;

    // Update irreversible aspects
    m_white_kingside_castling = m_wkcastling_array[m_ply];
    m_white_queenside_castling = m_wqcastling_array[m_ply];
    m_black_kingside_castling = m_bkcastling_array[m_ply];
    m_black_queenside_castling = m_bqcastling_array[m_ply];

    m_diagonal_pins = m_diagonal_pins_array[m_ply];
    m_straight_pins = m_straight_pins_array[m_ply];
    m_blockers = m_blockers_array[m_ply];

    m_psquare = m_psquare_array[m_ply];
    m_last_destination_bit = m_last_destination_bit_array[m_ply];

    // Get irreversible info
    unsigned short previous_captured_piece{m_captured_piece_array[m_ply]};

    // Get move info
    unsigned short origin_square{move.getOriginSquare()};
    uint64_t origin_bit{1ULL << origin_square};
    unsigned short destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{1ULL << destination_square};

    if (m_turn) // Last move was black
    {
        // Promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            m_black_pawns_bit |= origin_bit;
            m_black_queens_bit &= ~destination_bit;

            // Unmaking captures in promotions
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_white_knights_bit |= destination_bit;
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_white_bishops_bit |= destination_bit;
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_white_rooks_bit |= destination_bit;
                }
                else // Uncapture queen
                {
                    m_white_queens_bit |= destination_bit;
                }
            }
        }
        // Non promotions
        else
        {
            if ((destination_bit & m_black_pawns_bit) != 0) // Unmove pawn
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
            }
            else if ((destination_bit & m_black_knights_bit) != 0) // Unmove knight
            {
                m_black_knights_bit |= origin_bit;
                m_black_knights_bit &= ~destination_bit;
            }
            else if ((destination_bit & m_black_bishops_bit) != 0) // Unmove bishop
            {
                m_black_bishops_bit |= origin_bit;
                m_black_bishops_bit &= ~destination_bit;
            }
            else if ((destination_bit & m_black_rooks_bit) != 0) // Unmove rook
            {
                m_black_rooks_bit |= origin_bit;
                m_black_rooks_bit &= ~destination_bit;
            }
            else if ((destination_bit & m_black_queens_bit) != 0) // Unmove queen
            {
                m_black_queens_bit |= origin_bit;
                m_black_queens_bit &= ~destination_bit;
            }
            else // Unmove king
            {
                m_black_king_bit = origin_bit;
                m_black_king_position = origin_square;
            }
            // Unmaking captures
            if (previous_captured_piece == 0) // Uncapture pawn
            {
                m_white_pawns_bit |= destination_bit;
            }
            else if (previous_captured_piece == 1) // Uncapture knight
            {
                m_white_knights_bit |= destination_bit;
            }
            else if (previous_captured_piece == 2) // Uncapture bishop
            {
                m_white_bishops_bit |= destination_bit;
            }
            else if (previous_captured_piece == 3) // Uncapture rook
            {
                m_white_rooks_bit |= destination_bit;
            }
            else // Uncapture queen
            {
                m_white_queens_bit |= destination_bit;
            }
        }
    }
    else // Last move was white
    {
        // Passant and promotions
        if ((move.getData() & 0b0100000000000000) == 0b0100000000000000)
        {
            m_white_pawns_bit |= origin_bit;
            m_white_queens_bit &= ~destination_bit;
            // Unmaking captures in promotions
            if (previous_captured_piece != 7)
            {
                if (previous_captured_piece == 1) // Uncapture knight
                {
                    m_black_knights_bit |= destination_bit;
                }
                else if (previous_captured_piece == 2) // Uncapture bishop
                {
                    m_black_bishops_bit |= destination_bit;
                }
                else if (previous_captured_piece == 3) // Uncapture rook
                {
                    m_black_rooks_bit |= destination_bit;
                }
                else // Uncapture queen
                {
                    m_black_queens_bit |= destination_bit;
                }
            }
        }

        // Non special moves
        else
        {
            if ((destination_bit & m_white_pawns_bit) != 0) // Unmove pawn
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;
            }
            else if ((destination_bit & m_white_knights_bit) != 0) // Unmove knight
            {
                m_white_knights_bit |= origin_bit;
                m_white_knights_bit &= ~destination_bit;
            }
            else if ((destination_bit & m_white_bishops_bit) != 0) // Unmove bishop
            {
                m_white_bishops_bit |= origin_bit;
                m_white_bishops_bit &= ~destination_bit;
            }
            else if ((destination_bit & m_white_rooks_bit) != 0) // Unmove rook
            {
                m_white_rooks_bit |= origin_bit;
                m_white_rooks_bit &= ~destination_bit;
            }
            else if ((destination_bit & m_white_queens_bit) != 0) // Unmove queen
            {
                m_white_queens_bit |= origin_bit;
                m_white_queens_bit &= ~destination_bit;
            }
            else // Unmove king
            {
                m_white_king_bit = origin_bit;
                m_white_king_position = origin_square;
            }

            // Unmaking captures
            if (previous_captured_piece == 0) // Uncapture pawn
            {
                m_black_pawns_bit |= destination_bit;
            }
            else if (previous_captured_piece == 1) // Uncapture knight
            {
                m_black_knights_bit |= destination_bit;
            }
            else if (previous_captured_piece == 2) // Uncapture bishop
            {
                m_black_bishops_bit |= destination_bit;
            }
            else if (previous_captured_piece == 3) // Uncapture rook
            {
                m_black_rooks_bit |= destination_bit;
            }
            else // Uncapture queen
            {
                m_black_queens_bit |= destination_bit;
            }
        }
    }
    BitPosition::setAllPiecesBits();
    m_turn = not m_turn;
}