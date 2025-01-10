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


int16_t firstLayerWeights[640][8] = {0};
int16_t firstLayerInvertedWeights[640][8] = {0};

int8_t secondLayer1Weights[64][8 * 4] = {0};
int8_t secondLayer2Weights[64][8 * 4] = {0};

int8_t secondLayer1WeightsBlockWhiteTurn[8 * 4] = {0};
int8_t secondLayer2WeightsBlockWhiteTurn[8 * 4] = {0};
int8_t secondLayer1WeightsBlockBlackTurn[8 * 4] = {0};
int8_t secondLayer2WeightsBlockBlackTurn[8 * 4] = {0};

int8_t thirdLayerWeights[8 * 4] = {0};
int8_t finalLayerWeights[4] = {0};

int16_t firstLayerBiases[8] = {0};
int16_t secondLayerBiases[8] = {0};
int16_t thirdLayerBiases[4] = {0};
int16_t finalLayerBias = 0;

template void BitPosition::makeMove<Move>(Move move, StateInfo& new_stae_info);
template void BitPosition::makeMove<ScoredMove>(ScoredMove move, StateInfo &new_stae_info);

template void BitPosition::makeCapture<Move>(Move move, StateInfo &new_stae_info);
template void BitPosition::makeCapture<ScoredMove>(ScoredMove move, StateInfo &new_stae_info);

template void BitPosition::makeCaptureTest<Move>(Move move, StateInfo &new_stae_info);
template void BitPosition::makeCaptureTest<ScoredMove>(ScoredMove move, StateInfo &new_stae_info);

template void BitPosition::unmakeMove<Move>(Move move);
template void BitPosition::unmakeMove<ScoredMove>(ScoredMove move);

template void BitPosition::unmakeCapture<Move>(Move move);
template void BitPosition::unmakeCapture<ScoredMove>(ScoredMove move);

template bool BitPosition::positionIsDrawnAfterMove<Move>(Move move) const;
template bool BitPosition::positionIsDrawnAfterMove<ScoredMove>(ScoredMove move) const;

template bool BitPosition::isLegal<Move>(const Move *move) const;
template bool BitPosition::isLegal<ScoredMove>(const ScoredMove *move) const;

template bool BitPosition::isCaptureLegal<Move>(const Move *move) const;
template bool BitPosition::isCaptureLegal<ScoredMove>(const ScoredMove *move) const;

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
inline uint64_t shift_down(uint64_t b)
{
    return b >> 8; 
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
    state_info->zobristKey = 0;
    // Piece keys
    for (int square : getBitIndices(m_white_pawns_bit))
        state_info->zobristKey ^= zobrist_keys::whitePawnZobristNumbers[square];
    for (int square : getBitIndices(m_white_knights_bit))
        state_info->zobristKey ^= zobrist_keys::whiteKnightZobristNumbers[square];
    for (int square : getBitIndices(m_white_bishops_bit))
        state_info->zobristKey ^= zobrist_keys::whiteBishopZobristNumbers[square];
    for (int square : getBitIndices(m_white_rooks_bit))
        state_info->zobristKey ^= zobrist_keys::whiteRookZobristNumbers[square];
    for (int square : getBitIndices(m_white_queens_bit))
        state_info->zobristKey ^= zobrist_keys::whiteQueenZobristNumbers[square];
    state_info->zobristKey ^= zobrist_keys::whiteKingZobristNumbers[m_white_king_position];

    for (int square : getBitIndices(m_black_pawns_bit))
        state_info->zobristKey ^= zobrist_keys::blackPawnZobristNumbers[square];
    for (int square : getBitIndices(m_black_knights_bit))
        state_info->zobristKey ^= zobrist_keys::blackKnightZobristNumbers[square];
    for (int square : getBitIndices(m_black_bishops_bit))
        state_info->zobristKey ^= zobrist_keys::blackBishopZobristNumbers[square];
    for (int square : getBitIndices(m_black_rooks_bit))
        state_info->zobristKey ^= zobrist_keys::blackRookZobristNumbers[square];
    for (int square : getBitIndices(m_black_queens_bit))
        state_info->zobristKey ^= zobrist_keys::blackQueenZobristNumbers[square];
    state_info->zobristKey ^= zobrist_keys::blackKingZobristNumbers[m_black_king_position];
    // Turn key
    if (not m_turn)
        state_info->zobristKey ^= zobrist_keys::blackToMoveZobristNumber;
    // Castling rights key
    int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
    state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
    // Psquare key
    state_info->zobristKey ^= zobrist_keys::passantSquaresZobristNumbers[state_info->pSquare];
    m_zobrist_keys_array[127 - m_ply] = state_info->zobristKey;
}
bool BitPosition::whiteKingSquareIsSafe() const
// For when moving the king
{
    // Knights
    if ((precomputed_moves::knight_moves[m_white_king_position] & m_black_knights_bit) != 0)
        return false;

    // Pawns
    if ((precomputed_moves::white_pawn_attacks[m_white_king_position] & m_black_pawns_bit) != 0)
        return false;

    // Queen
    if (((RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & m_all_pieces_bit) | BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & m_all_pieces_bit)) & m_black_queens_bit) != 0)
        return false;

    // Rook
    if ((RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & m_all_pieces_bit) & m_black_rooks_bit) != 0)
        return false;

    // Bishop
    if ((BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & m_all_pieces_bit) & m_black_bishops_bit) != 0)
        return false;

    // King
    if ((precomputed_moves::king_moves[m_white_king_position] & m_black_king_bit) != 0)
        return false;

    return true;
}
bool BitPosition::blackKingSquareIsSafe() const
// For when moving the king
{
    // Knights
    if ((precomputed_moves::knight_moves[m_black_king_position] & m_white_knights_bit) != 0)
        return false;

    // Pawns
    if ((precomputed_moves::black_pawn_attacks[m_black_king_position] & m_white_pawns_bit) != 0)
        return false;

    // Queen
    if (((RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & m_all_pieces_bit) | BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & m_all_pieces_bit)) & m_white_queens_bit) != 0)
        return false;

    // Rook
    if ((RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & m_all_pieces_bit) & m_white_rooks_bit) != 0)
        return false;

    // Bishop
    if ((BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & m_all_pieces_bit) & m_white_bishops_bit) != 0)
        return false;

    // King
    if ((precomputed_moves::king_moves[m_black_king_position] & m_white_king_bit) != 0)
        return false;

    return true;
}
void BitPosition::setIsCheckOnInitialization()
{
    if (m_turn && not whiteKingSquareIsSafe())
        (state_info->isCheck)  = true;
    else if (not m_turn && not blackKingSquareIsSafe())
        (state_info->isCheck)  = true;
    else
        (state_info->isCheck)  = false;
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
        int pawnCheck{getLeastSignificantBitIndex(precomputed_moves::white_pawn_attacks[m_white_king_position] & m_black_pawns_bit)};
        if (pawnCheck != 65)
        {
            m_num_checks++;
            m_check_square = pawnCheck;
        }
        // Knight check
        int knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_white_king_position] & m_black_knights_bit)};
        if (knightCheck != 65)
        {
            m_num_checks++;
            m_check_square = knightCheck;
        }
        // Bishop check
        for (int bishopSquare : getBitIndices(m_black_bishops_bit))
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
        for (int rookSquare : getBitIndices(m_black_rooks_bit))
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
        for (int queenSquare : getBitIndices(m_black_queens_bit))
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
        int pawnCheck{getLeastSignificantBitIndex(precomputed_moves::black_pawn_attacks[m_black_king_position] & m_white_pawns_bit)};
        if (pawnCheck != 65)
        {
            m_num_checks++;
            m_check_square = pawnCheck;
        }
        // Knight check
        int knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_black_king_position] & m_white_knights_bit)};
        if (knightCheck != 65)
        {
            m_num_checks++;
            m_check_square = knightCheck;
        }
        // Bishop check
        for (int bishopSquare : getBitIndices(m_white_bishops_bit))
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
        for (int rookSquare : getBitIndices(m_white_rooks_bit))
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
        for (int queenSquare : getBitIndices(m_white_queens_bit))
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
void BitPosition::updateZobristKeyPiecePartAfterMove(int origin_square, int destination_square)
// Since the zobrist hashes are done by XOR on each individual key. And XOR is its own inverse.
// We can efficiently update the hash by XORing the previous key with the key of the piece origin
// square and with the key of the piece destination square.
// Applied inside makeCapture and makeNormalMove.
{
    if (m_turn)
    {
        if (state_info->lastDestinationBit & m_white_pawns_bit) // Pawn
            state_info->zobristKey ^= zobrist_keys::whitePawnZobristNumbers[origin_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
        else if (state_info->lastDestinationBit & m_white_knights_bit) // Knight
            state_info->zobristKey ^= zobrist_keys::whiteKnightZobristNumbers[origin_square] ^ zobrist_keys::whiteKnightZobristNumbers[destination_square];
        else if (state_info->lastDestinationBit & m_white_bishops_bit) // Bishop
            state_info->zobristKey ^= zobrist_keys::whiteBishopZobristNumbers[origin_square] ^ zobrist_keys::whiteBishopZobristNumbers[destination_square];
        else if (state_info->lastDestinationBit & m_white_rooks_bit) // Rook
            state_info->zobristKey ^= zobrist_keys::whiteRookZobristNumbers[origin_square] ^ zobrist_keys::whiteRookZobristNumbers[destination_square];
        else if (state_info->lastDestinationBit & m_white_queens_bit) // Queen
            state_info->zobristKey ^= zobrist_keys::whiteQueenZobristNumbers[origin_square] ^ zobrist_keys::whiteQueenZobristNumbers[destination_square];
        else // King
            state_info->zobristKey ^= zobrist_keys::whiteKingZobristNumbers[origin_square] ^ zobrist_keys::whiteKingZobristNumbers[destination_square];
        if (state_info->capturedPiece != 7)
        {
            if (state_info->capturedPiece == 0) // Pawn
                state_info->zobristKey ^= zobrist_keys::blackPawnZobristNumbers[destination_square];
            else if (state_info->capturedPiece == 1) // Knight
                state_info->zobristKey ^= zobrist_keys::blackKnightZobristNumbers[destination_square];
            else if (state_info->capturedPiece == 2) // Bishop
                state_info->zobristKey ^= zobrist_keys::blackBishopZobristNumbers[destination_square];
            else if (state_info->capturedPiece == 3) // Rook
                state_info->zobristKey ^= zobrist_keys::blackRookZobristNumbers[destination_square];
            else // Queen
                state_info->zobristKey ^= zobrist_keys::blackQueenZobristNumbers[destination_square];
        }
        if (m_promoted_piece != 7)
        {
            if (m_promoted_piece == 1) // Knight
                state_info->zobristKey ^= zobrist_keys::whiteKnightZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else if (m_promoted_piece == 2) // Bishop
                state_info->zobristKey ^= zobrist_keys::whiteBishopZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else if (m_promoted_piece == 3) // Rook
                state_info->zobristKey ^= zobrist_keys::whiteRookZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else // Queen
                state_info->zobristKey ^= zobrist_keys::whiteQueenZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
        }
    }
    else
    {
        if (state_info->lastDestinationBit & m_black_pawns_bit) // Pawn
            state_info->zobristKey ^= zobrist_keys::blackPawnZobristNumbers[origin_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
        else if (state_info->lastDestinationBit & m_black_knights_bit) // Knight
            state_info->zobristKey ^= zobrist_keys::blackKnightZobristNumbers[origin_square] ^ zobrist_keys::blackKnightZobristNumbers[destination_square];
        else if (state_info->lastDestinationBit & m_black_bishops_bit) // Bishop
            state_info->zobristKey ^= zobrist_keys::blackBishopZobristNumbers[origin_square] ^ zobrist_keys::blackBishopZobristNumbers[destination_square];
        else if (state_info->lastDestinationBit & m_black_rooks_bit) // Rook
            state_info->zobristKey ^= zobrist_keys::blackRookZobristNumbers[origin_square] ^ zobrist_keys::blackRookZobristNumbers[destination_square];
        else if (state_info->lastDestinationBit & m_black_queens_bit) // Queen
            state_info->zobristKey ^= zobrist_keys::blackQueenZobristNumbers[origin_square] ^ zobrist_keys::blackQueenZobristNumbers[destination_square];
        else // King
            state_info->zobristKey ^= zobrist_keys::blackKingZobristNumbers[origin_square] ^ zobrist_keys::blackKingZobristNumbers[destination_square];
        if (state_info->capturedPiece != 7)
        {
            if (state_info->capturedPiece == 0) // Pawn
                state_info->zobristKey ^= zobrist_keys::whitePawnZobristNumbers[destination_square];
            else if (state_info->capturedPiece == 1) // Knight
                state_info->zobristKey ^= zobrist_keys::whiteKnightZobristNumbers[destination_square];
            else if (state_info->capturedPiece == 2) // Bishop
                state_info->zobristKey ^= zobrist_keys::whiteBishopZobristNumbers[destination_square];
            else if (state_info->capturedPiece == 3) // Rook
                state_info->zobristKey ^= zobrist_keys::whiteRookZobristNumbers[destination_square];
            else // Queen
                state_info->zobristKey ^= zobrist_keys::whiteQueenZobristNumbers[destination_square];
        }
        if (m_promoted_piece != 7)
        {
            if (m_promoted_piece == 1) // Knight
                state_info->zobristKey ^= zobrist_keys::blackKnightZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else if (m_promoted_piece == 2) // Bishop
                state_info->zobristKey ^= zobrist_keys::blackBishopZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else if (m_promoted_piece == 3) // Rook
                state_info->zobristKey ^= zobrist_keys::blackRookZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else // Queen
                state_info->zobristKey ^= zobrist_keys::blackQueenZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
        }
    }
}
uint64_t BitPosition::updateZobristKeyPiecePartBeforeMove(int origin_square, int destination_square, int promoted_piece) const
// Since the zobrist hashes are done by XOR on each individual key. And XOR is its own inverse.
// We can efficiently update the hash by XORing the previous key with the key of the piece origin
// square and with the key of the piece destination square.
// Applied inside positionIsDrawnAfterMove to check for draws before making a move
{
    uint64_t temp_zobrist_key{state_info->zobristKey};
    uint64_t origin_bit = (1ULL << origin_square);
    uint64_t destination_bit = (1ULL << destination_square);
    if (m_turn)
    {
        if (origin_bit & m_white_pawns_bit)
            temp_zobrist_key ^= zobrist_keys::whitePawnZobristNumbers[origin_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
        else if (origin_bit & m_white_knights_bit)
            temp_zobrist_key ^= zobrist_keys::whiteKnightZobristNumbers[origin_square] ^ zobrist_keys::whiteKnightZobristNumbers[destination_square];
        else if (origin_bit & m_white_bishops_bit)
            temp_zobrist_key ^= zobrist_keys::whiteBishopZobristNumbers[origin_square] ^ zobrist_keys::whiteBishopZobristNumbers[destination_square];
        else if (origin_bit & m_white_rooks_bit)
            temp_zobrist_key ^= zobrist_keys::whiteRookZobristNumbers[origin_square] ^ zobrist_keys::whiteRookZobristNumbers[destination_square];
        else if (origin_bit & m_white_queens_bit)
            temp_zobrist_key ^= zobrist_keys::whiteQueenZobristNumbers[origin_square] ^ zobrist_keys::whiteQueenZobristNumbers[destination_square];
        else // King
            temp_zobrist_key ^= zobrist_keys::whiteKingZobristNumbers[origin_square] ^ zobrist_keys::whiteKingZobristNumbers[destination_square];

        if (destination_bit & m_black_pawns_bit) // Pawn
            temp_zobrist_key ^= zobrist_keys::blackPawnZobristNumbers[destination_square];
        else if (destination_bit & m_black_knights_bit) // Knight
            temp_zobrist_key ^= zobrist_keys::blackKnightZobristNumbers[destination_square];
        else if (destination_bit & m_black_bishops_bit) // Bishop
            temp_zobrist_key ^= zobrist_keys::blackBishopZobristNumbers[destination_square];
        else if (destination_bit & m_black_rooks_bit) // Rook
            temp_zobrist_key ^= zobrist_keys::blackRookZobristNumbers[destination_square];
        else if (destination_bit & m_black_queens_bit) // Queen
            temp_zobrist_key ^= zobrist_keys::blackQueenZobristNumbers[destination_square];
            
        if (promoted_piece != 7)
        {
            if (promoted_piece == 1) // Knight
                temp_zobrist_key ^= zobrist_keys::whiteKnightZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else if (promoted_piece == 2) // Bishop
                temp_zobrist_key ^= zobrist_keys::whiteBishopZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else if (promoted_piece == 3) // Rook
                temp_zobrist_key ^= zobrist_keys::whiteRookZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
            else // Queen
                temp_zobrist_key ^= zobrist_keys::whiteQueenZobristNumbers[destination_square] ^ zobrist_keys::whitePawnZobristNumbers[destination_square];
        }
    }
    else
    {
        if (origin_bit & m_black_pawns_bit) // Pawn
            temp_zobrist_key ^= zobrist_keys::blackPawnZobristNumbers[origin_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
        else if (origin_bit & m_black_knights_bit) // Knight
            temp_zobrist_key ^= zobrist_keys::blackKnightZobristNumbers[origin_square] ^ zobrist_keys::blackKnightZobristNumbers[destination_square];
        else if (origin_bit & m_black_bishops_bit) // Bishop
            temp_zobrist_key ^= zobrist_keys::blackBishopZobristNumbers[origin_square] ^ zobrist_keys::blackBishopZobristNumbers[destination_square];
        else if (origin_bit & m_black_rooks_bit) // Rook
            temp_zobrist_key ^= zobrist_keys::blackRookZobristNumbers[origin_square] ^ zobrist_keys::blackRookZobristNumbers[destination_square];
        else if (origin_bit & m_black_queens_bit) // Queen
            temp_zobrist_key ^= zobrist_keys::blackQueenZobristNumbers[origin_square] ^ zobrist_keys::blackQueenZobristNumbers[destination_square];
        else // King
            temp_zobrist_key ^= zobrist_keys::blackKingZobristNumbers[origin_square] ^ zobrist_keys::blackKingZobristNumbers[destination_square];

        if (destination_bit & m_white_pawns_bit) // Pawn
            temp_zobrist_key ^= zobrist_keys::whitePawnZobristNumbers[destination_square];
        else if (destination_bit & m_white_knights_bit) // Knight
            temp_zobrist_key ^= zobrist_keys::whiteKnightZobristNumbers[destination_square];
        else if (destination_bit & m_white_bishops_bit) // Bishop
            temp_zobrist_key ^= zobrist_keys::whiteBishopZobristNumbers[destination_square];
        else if (destination_bit & m_white_rooks_bit) // Rook
            temp_zobrist_key ^= zobrist_keys::whiteRookZobristNumbers[destination_square];
        else if (destination_bit & m_white_queens_bit) // Queen
            temp_zobrist_key ^= zobrist_keys::whiteQueenZobristNumbers[destination_square];

        if (promoted_piece != 7)
        {
            if (promoted_piece == 1) // Knight
                temp_zobrist_key ^= zobrist_keys::blackKnightZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else if (promoted_piece == 2) // Bishop
                temp_zobrist_key ^= zobrist_keys::blackBishopZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else if (promoted_piece == 3) // Rook
                temp_zobrist_key ^= zobrist_keys::blackRookZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
            else // Queen
                temp_zobrist_key ^= zobrist_keys::blackQueenZobristNumbers[destination_square] ^ zobrist_keys::blackPawnZobristNumbers[destination_square];
        }
    }
    return temp_zobrist_key;
}

// Functions we call before move generation
void BitPosition::setDiscoverCheckForWhite()
// Return check info for discovered checks, for direct checks we know because of the move
// Called in setCheckInfoAfterMove
{
    // Black bishops and queens
    uint64_t checking_pieces_bit{BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (m_black_bishops_bit | m_black_queens_bit) & ~state_info->lastDestinationBit};
    while (checking_pieces_bit)
    {
        m_check_square = popLeastSignificantBit(checking_pieces_bit);
        m_check_rays |= precomputed_moves::precomputedBishopMovesTableOneBlocker[m_check_square][m_white_king_position];
        m_num_checks++;
    }
    // Black rooks and queens
    checking_pieces_bit = RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (m_black_rooks_bit | m_black_queens_bit) & ~state_info->lastDestinationBit;
    while (checking_pieces_bit)
    {
        m_check_square = popLeastSignificantBit(checking_pieces_bit);
        m_check_rays |= precomputed_moves::precomputedRookMovesTableOneBlocker[m_check_square][m_white_king_position];
        m_num_checks++;
    }
}
void BitPosition::setDiscoverCheckForBlack()
// Return check info for discovered checks, for direct checks we know because of the move
// Called in setCheckInfoAfterMove
{
    // white bishops and queens
    uint64_t checking_pieces_bit{BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & m_all_pieces_bit) & (m_white_bishops_bit | m_white_queens_bit) & ~state_info->lastDestinationBit};
    while (checking_pieces_bit)
    {
        m_check_square = popLeastSignificantBit(checking_pieces_bit);
        m_check_rays |= precomputed_moves::precomputedBishopMovesTableOneBlocker[m_check_square][m_black_king_position];
        m_num_checks++;
    }
    // white rooks and queens
    checking_pieces_bit = RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & m_all_pieces_bit) & (m_white_rooks_bit | m_white_queens_bit) & ~state_info->lastDestinationBit;
    while (checking_pieces_bit)
    {
        m_check_square = popLeastSignificantBit(checking_pieces_bit);
        m_check_rays |= precomputed_moves::precomputedRookMovesTableOneBlocker[m_check_square][m_black_king_position];
        m_num_checks++;
    }
}

void BitPosition::setCheckInfo()
// This is called before searching for moves in check in engine.cpp, and also in unmakeTTMove if the previous position was in check
{
    m_num_checks = 0; // Number of checks
    m_check_rays = 0; // Check ray if any
    m_check_square = 65; // Check square. If there is more than one, only one is stored

    if (m_turn) // Last move was black
    {
        int check_square{getLeastSignificantBitIndex(precomputed_moves::white_pawn_attacks[m_white_king_position] & m_black_pawns_bit)};
        if (check_square != 65)
        {
            m_num_checks++;
            m_check_square = check_square;
        }
        // Knight
        check_square = getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_white_king_position] & m_black_knights_bit);
        if (check_square != 65)
        {
            m_num_checks++;
            m_check_square = check_square;
        }
        // Bishop and queen
        uint64_t checks{BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (m_black_bishops_bit | m_black_queens_bit)};
        while (checks)
        {
            check_square = popLeastSignificantBit(checks);
            uint64_t ray = precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_white_king_position][check_square];
            m_num_checks++;
            m_check_rays |= ray & ~(1ULL << check_square);
            m_check_square = check_square;
        }
        // Rook and queen
        checks = RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (m_black_rooks_bit | m_black_queens_bit);
        while (checks)
        {
            check_square = popLeastSignificantBit(checks);
            uint64_t ray = precomputed_moves::precomputedRookMovesTableOneBlocker2[m_white_king_position][check_square];
            m_num_checks++;
            m_check_rays |= ray & ~(1ULL << check_square);
            m_check_square = check_square;
        }
    }
    else // Last move was white
    {
        int check_square{getLeastSignificantBitIndex(precomputed_moves::black_pawn_attacks[m_black_king_position] & m_white_pawns_bit)};
        if (check_square != 65)
        {
            m_num_checks++;
            m_check_square = check_square;
        }
        // Knight
        check_square = getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_black_king_position] & m_white_knights_bit);
        if (check_square != 65)
        {
            m_num_checks++;
            m_check_square = check_square;
        }
        // Bishop and queen
        uint64_t checks{BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & m_all_pieces_bit) & (m_white_bishops_bit | m_white_queens_bit)};
        while (checks)
        {
            check_square = popLeastSignificantBit(checks);
            uint64_t ray = precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_black_king_position][check_square];
            m_num_checks++;
            m_check_rays |= ray & ~(1ULL << check_square);
            m_check_square = check_square;
        }
        // Rook and queen
        checks = RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & m_all_pieces_bit) & (m_white_rooks_bit | m_white_queens_bit);
        while (checks)
        {
            check_square = popLeastSignificantBit(checks);
            uint64_t ray = precomputed_moves::precomputedRookMovesTableOneBlocker2[m_black_king_position][check_square];
            m_num_checks++;
            m_check_rays |= ray & ~(1ULL << check_square);
            m_check_square = check_square;
        }
    }
}

void BitPosition::setBlockersAndPinsInQS()
{
    state_info->blockersForKing = 0;
    state_info->pinnedPieces = 0;
    m_blockers_set = true;

    if (m_turn)
    {
        // Blockers of white bishops, rooks and queens
        uint64_t snipers_bits = ((m_white_bishops_bit | m_white_queens_bit) & precomputed_moves::bishop_full_rays[m_black_king_position]) | ((m_white_rooks_bit | m_white_queens_bit) & precomputed_moves::rook_full_rays[m_black_king_position]);
        while (snipers_bits)
        // For each square corresponding to black bishop raying black king
        {
            uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_black_king_position] & m_all_pieces_bit;
            if (ray && hasOneOne(ray))
                state_info->blockersForKing |= ray;
        }
        // Blockers and pins of black bishops and queens
        snipers_bits = ((m_black_bishops_bit | m_black_queens_bit) & precomputed_moves::bishop_full_rays[m_white_king_position]) | ((m_black_rooks_bit | m_black_queens_bit) & precomputed_moves::rook_full_rays[m_white_king_position]);
        while (snipers_bits)
        // For each square corresponding to black bishop raying black king
        {
            uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_white_king_position] & m_all_pieces_bit;
            if ((ray & m_white_pieces_bit) && hasOneOne(ray))
            {
                (state_info->pinnedPieces)  |= ray;
            }
        }
    }
    else
    {
        // Blockers of white bishops, rooks and queens
        uint64_t snipers_bits = ((m_black_bishops_bit | m_black_queens_bit) & precomputed_moves::bishop_full_rays[m_white_king_position]) | ((m_black_rooks_bit | m_black_queens_bit) & precomputed_moves::rook_full_rays[m_white_king_position]);
        while (snipers_bits)
        // For each square corresponding to black bishop raying black king
        {
            uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_white_king_position] & m_all_pieces_bit;
            if (ray && hasOneOne(ray))
                state_info->blockersForKing |= ray;
        }
        // Blockers and pins of black bishops and queens
        snipers_bits = ((m_white_bishops_bit | m_white_queens_bit) & precomputed_moves::bishop_full_rays[m_black_king_position]) | ((m_white_rooks_bit | m_white_queens_bit) & precomputed_moves::rook_full_rays[m_black_king_position]);
        while (snipers_bits)
        // For each square corresponding to black bishop raying black king
        {
            uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_black_king_position] & m_all_pieces_bit;
            if ((ray & m_black_pieces_bit) && hasOneOne(ray))
            {
                (state_info->pinnedPieces)  |= ray;
            }
        }
    }
}
void BitPosition::setBlockersAndPinsInAB()
// This is the only one called before ttable moves, since we need ot determine if move gives check or not.
// It is also called in nextMove, nextCapture, nextMoveInCheck and nextCaptureInCheck, when a legal move is found.
{
    state_info->blockersForKing = 0;
    state_info->straightPinnedPieces = 0;
    state_info->diagonalPinnedPieces = 0;
    m_blockers_set = true;

    if (m_turn)
    {
        // Blockers of white bishops, rooks and queens
        uint64_t snipers_bits = ((m_white_bishops_bit | m_white_queens_bit) & precomputed_moves::bishop_full_rays[m_black_king_position]) | ((m_white_rooks_bit | m_white_queens_bit) & precomputed_moves::rook_full_rays[m_black_king_position]);
        while (snipers_bits)
        // For each square corresponding to black bishop raying black king
        {
            uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_black_king_position] & m_all_pieces_bit;
            if (ray && hasOneOne(ray))
                state_info->blockersForKing |= ray;
        }
        // Blockers and pins of black bishops and queens
        snipers_bits = (m_black_bishops_bit | m_black_queens_bit) & precomputed_moves::bishop_full_rays[m_white_king_position];
        while (snipers_bits)
        // For each square corresponding to black bishop raying black king
        {
            uint64_t bishop_ray = precomputed_moves::precomputedBishopMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_white_king_position] & m_all_pieces_bit;
            if ((bishop_ray & m_white_pieces_bit) && hasOneOne(bishop_ray))
            {
                (state_info->diagonalPinnedPieces) |= bishop_ray;
            }
        }
        // Blockers and pins of black rooks and queens
        snipers_bits = (m_black_rooks_bit | m_black_queens_bit) & precomputed_moves::rook_full_rays[m_white_king_position];
        while (snipers_bits)
        // For each square corresponding to black rook raying black king
        {
            uint64_t rook_ray = precomputed_moves::precomputedRookMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_white_king_position] & m_all_pieces_bit;
            if ((rook_ray & m_white_pieces_bit) && hasOneOne(rook_ray))
            {
                (state_info->straightPinnedPieces) |= rook_ray;
            }
        }
    }
    else
    {
        // Blockers of black bishops, rooks and queens
        uint64_t snipers_bits = ((m_black_bishops_bit | m_black_queens_bit) & precomputed_moves::bishop_full_rays[m_white_king_position]) | ((m_black_rooks_bit | m_black_queens_bit) & precomputed_moves::rook_full_rays[m_white_king_position]);
        while (snipers_bits)
        // For each square corresponding to black bishop raying black king
        {
            uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_white_king_position] & m_all_pieces_bit;
            if (ray && hasOneOne(ray))
                state_info->blockersForKing |= ray;
        }
        // Blockers and pins of black bishops and queens
        snipers_bits = (m_white_bishops_bit | m_white_queens_bit) & precomputed_moves::bishop_full_rays[m_black_king_position];
        while (snipers_bits)
        // For each square corresponding to black bishop raying black king
        {
            uint64_t bishop_ray = precomputed_moves::precomputedBishopMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_black_king_position] & m_all_pieces_bit;
            if ((bishop_ray & m_black_pieces_bit) && hasOneOne(bishop_ray))
            {
                (state_info->diagonalPinnedPieces) |= bishop_ray;
            }
        }
        // Blockers and pins of black rooks and queens
        snipers_bits = (m_white_rooks_bit | m_white_queens_bit) & precomputed_moves::rook_full_rays[m_black_king_position];
        while (snipers_bits)
        // For each square corresponding to black rook raying black king
        {
            uint64_t rook_ray = precomputed_moves::precomputedRookMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_black_king_position] & m_all_pieces_bit;
            if ((rook_ray & m_black_pieces_bit) && hasOneOne(rook_ray))
            {
                (state_info->straightPinnedPieces) |= rook_ray;
            }
        }
    }
    state_info->pinnedPieces = (state_info->straightPinnedPieces | state_info->diagonalPinnedPieces);
}

// Functions we call during move generation
template <typename T>
bool BitPosition::isLegal(const T *move) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    // Move is legal if piece is not pinned, otherwise if origin, destination and king position are aligned
    if (m_turn)
    {
        if (move->getData() == 16772) // White kingside castling
        {
            return newWhiteKingSquareIsSafe(5) && newWhiteKingSquareIsSafe(6);
        }
        else if (move->getData() == 16516) // White queenside castling
        {
            return newWhiteKingSquareIsSafe(2) && newWhiteKingSquareIsSafe(3);
        }
        else
        {
            int origin_square = move->getOriginSquare();
            int destination_square = move->getDestinationSquare();
            // Knight moves are always legal
            if ((1ULL << origin_square) & m_white_knights_bit) 
                return true;
            // King moves
            else if (origin_square == m_white_king_position)
                return newWhiteKingSquareIsSafe(destination_square);
            // Rest of pieces
            else
                return ((1ULL << origin_square) & (state_info->pinnedPieces))  == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_white_king_bit;
        }
    }
    else
    {
        if (move->getData() == 20412) // Black kingside castling
        {
            return newBlackKingSquareIsSafe(61) && newBlackKingSquareIsSafe(62);
        }
        else if (move->getData() == 20156) // Black queenside castling
        {
            return newBlackKingSquareIsSafe(58) && newBlackKingSquareIsSafe(59);
        }
        else
        {
            int origin_square = move->getOriginSquare();
            int destination_square = move->getDestinationSquare();
            // Knight moves are always legal
            if ((1ULL << origin_square) & m_black_knights_bit)
                return true;
            // King moves
            else if (origin_square == m_black_king_position)
                return newBlackKingSquareIsSafe(destination_square);
            // Rest of pieces
            else
                return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_black_king_bit;
        }   
    }
}

bool BitPosition::isNormalMoveLegal(int origin_square, int destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    // Move is legal if piece is not pinned, otherwise if origin, destination and king position are aligned
    if (m_turn)
    {
        // Knight moves are always legal
        if ((1ULL << origin_square) & m_white_knights_bit)
            return true;
        // King moves
        else if (origin_square == m_white_king_position)
            return newWhiteKingSquareIsSafe(destination_square);
        // Rest of pieces
        else
            return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_white_king_bit;
    }
    else
    {
        // Knight moves are always legal
        if ((1ULL << origin_square) & m_black_knights_bit)
            return true;
        // King moves
        else if (origin_square == m_black_king_position)
            return newBlackKingSquareIsSafe(destination_square);
        // Rest of pieces
        else
            return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_black_king_bit;
    }
}

bool BitPosition::isRefutationLegal(Move move) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    int origin_square = move.getOriginSquare();
    int destination_square = move.getDestinationSquare();
    // Move is legal if piece is not pinned, otherwise if origin, destination and king position are aligned
    if (m_turn)
    {
        // King moves
        if (origin_square == m_white_king_position)
            return newWhiteKingSquareIsSafe(destination_square);
        // Rest of pieces
        else
            return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_white_king_bit;
    }
    else
    {
        // King moves
        if (origin_square == m_black_king_position)
            return newBlackKingSquareIsSafe(destination_square);
        // Rest of pieces
        else
            return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_black_king_bit;
    }
}

bool BitPosition::ttMoveIsOk(Move move) const
{
    if (m_turn)
    {
        if (((1ULL << move.getOriginSquare()) & m_white_pieces_bit) == 0 || ((1ULL << move.getDestinationSquare()) & m_white_pieces_bit))
            return false;
    }
    else
    {
        if (((1ULL << move.getOriginSquare()) & m_black_pieces_bit) == 0 || ((1ULL << move.getDestinationSquare()) & m_black_pieces_bit))
            return false;
    }
    return true;
}

template <typename T>
bool BitPosition::isCaptureLegal(const T *move) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    int origin_square = move->getOriginSquare();
    int destination_square = move->getDestinationSquare();
    if (m_turn)
    {
        // King moves
        if (origin_square == m_white_king_position)
            return newWhiteKingSquareIsSafe(destination_square);
        // Rest of pieces
        else
            return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_white_king_bit;
    }
    else
    {
        // King moves
        if (origin_square == m_black_king_position)
            return newBlackKingSquareIsSafe(destination_square);
        // Rest of pieces
        else
            return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_black_king_bit;
    }
}

bool BitPosition::newWhiteKingSquareIsSafe(int new_position) const
// For when moving the king in captures
{
    // Knights
    if ((precomputed_moves::knight_moves[new_position] & m_black_knights_bit) != 0)
        return false;

    // Pawns
    if ((precomputed_moves::white_pawn_attacks[new_position] & m_black_pawns_bit) != 0)
        return false;

    // Rook and queen
    if ((RmagicNOMASK(new_position, precomputed_moves::rook_unfull_rays[new_position] & m_all_pieces_bit & ~m_white_king_bit) & (m_black_rooks_bit | m_black_queens_bit)) != 0)
        return false;

    // Bishop and queen
    if ((BmagicNOMASK(new_position, precomputed_moves::bishop_unfull_rays[new_position] & m_all_pieces_bit & ~m_white_king_bit) & (m_black_bishops_bit | m_black_queens_bit)) != 0)
        return false;

    // King
    if ((precomputed_moves::king_moves[new_position] & m_black_king_bit) != 0)
        return false;

    return true;
}
bool BitPosition::newBlackKingSquareIsSafe(int new_position) const
// For when moving the king in captures
{
    // Knights
    if ((precomputed_moves::knight_moves[new_position] & m_white_knights_bit) != 0)
        return false;

    // Pawns
    if ((precomputed_moves::black_pawn_attacks[new_position] & m_white_pawns_bit) != 0)
        return false;

    // Rook and queen
    if ((RmagicNOMASK(new_position, precomputed_moves::rook_unfull_rays[new_position] & m_all_pieces_bit & ~m_black_king_bit) & (m_white_rooks_bit | m_white_queens_bit)) != 0)
        return false;

    // Bishop and queen
    if ((BmagicNOMASK(new_position, precomputed_moves::bishop_unfull_rays[new_position] & m_all_pieces_bit & ~m_black_king_bit) & (m_white_bishops_bit | m_white_queens_bit)) != 0)
        return false;

    // King
    if ((precomputed_moves::king_moves[new_position] & m_white_king_bit) != 0)
        return false;

    return true;
}

bool BitPosition::kingIsSafeAfterPassant(int removed_square_1, int removed_square_2) const // See if the king is in check or not (from kings position). For when moving the king.
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
bool BitPosition::isDiscoverCheckForWhiteAfterPassant(int origin_square, int destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks. 
// We just check if removing the pawn captured leads to check, since check after removing and placing our pawn is already checked using isPawnCheckOrDiscoverForWhite
{
    // If captured pawn is not blocking or after removing captured pawn we are not in check
    if (((1ULL << (destination_square + 8)) & (state_info->blockersForKing)) == 0 || ((BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & (m_all_pieces_bit & ~(1ULL << (destination_square + 8)))) & (m_black_bishops_bit | m_black_queens_bit)) == 0 && (RmagicNOMASK(m_white_king_position, precomputed_moves::rook_unfull_rays[m_white_king_position] & (m_all_pieces_bit & ~(1ULL << (destination_square + 8)))) & (m_black_rooks_bit | m_black_queens_bit)) == 0))
        return false;

    return true;
}
bool BitPosition::isDiscoverCheckForBlackAfterPassant(int origin_square, int destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks.
// We just check if removing the pawn captured leads to check, since check after removing and placing our pawn is already checked using isPawnCheckOrDiscoverForBlack
{
    // If captured pawn is not blocking or after removing captured pawn we are not in check
    if (((1ULL << (destination_square - 8)) & (state_info->blockersForKing)) == 0 || ((BmagicNOMASK(m_black_king_position, precomputed_moves::bishop_unfull_rays[m_black_king_position] & (m_all_pieces_bit & ~(1ULL << (destination_square - 8)))) & (m_white_bishops_bit | m_white_queens_bit)) == 0 && (RmagicNOMASK(m_black_king_position, precomputed_moves::rook_unfull_rays[m_black_king_position] & (m_all_pieces_bit & ~(1ULL << (destination_square - 8)))) & (m_white_rooks_bit | m_white_queens_bit)) == 0))
        return false;

    return true;
}

bool BitPosition::isDiscoverCheckForWhite(int origin_square, int destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
{
    // If piece is not blocking or moving in blocking ray
    if (((1ULL << origin_square) & (state_info->blockersForKing)) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_white_king_bit) != 0)
        return false;

    return true;
}
bool BitPosition::isDiscoverCheckForBlack(int origin_square, int destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
{
    // If piece is not blocking or moving in blocking ray
    if (((1ULL << origin_square) & (state_info->blockersForKing)) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_black_king_bit) != 0)
        return false;

    return true;
}

bool BitPosition::isPawnCheckOrDiscoverForWhite(int origin_square, int destination_square) const
{
    // Direct check
    if (precomputed_moves::black_pawn_attacks[destination_square] & m_white_king_bit)
        return true;
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isKnightCheckOrDiscoverForWhite(int origin_square, int destination_square) const
{
    // Direct check
    if (precomputed_moves::knight_moves[destination_square] & m_white_king_bit)
        return true;
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isBishopCheckOrDiscoverForWhite(int origin_square, int destination_square) const
{
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
    {
        return true;
    }
    // Direct check
    if ((precomputed_moves::precomputedBishopMovesTableOneBlocker2[destination_square][m_white_king_position] & m_all_pieces_bit) == m_white_king_bit)
        return true;

    // if ((precomputed_moves::bishop_full_rays[m_white_king_position] & (1ULL << destination_square)) != 0 && (BmagicNOMASK(m_white_king_position, precomputed_moves::bishop_unfull_rays[m_white_king_position] & m_all_pieces_bit) & (1ULL << destination_square)) != 0)
    //     return true;
    return false;
}
bool BitPosition::isRookCheckOrDiscoverForWhite(int origin_square, int destination_square) const
{
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    // Direct check
    if ((precomputed_moves::precomputedRookMovesTableOneBlocker2[destination_square][m_white_king_position] & m_all_pieces_bit) == m_white_king_bit)
        return true;
    return false;
}
bool BitPosition::isQueenCheckOrDiscoverForWhite(int origin_square, int destination_square) const
{
    // Discovered check
    if (isDiscoverCheckForWhite(origin_square, destination_square))
        return true;
    // Direct check
    if ((precomputed_moves::precomputedQueenMovesTableOneBlocker2[destination_square][m_white_king_position] & m_all_pieces_bit) == m_white_king_bit)
        return true;
    return false;
}

bool BitPosition::isPawnCheckOrDiscoverForBlack(int origin_square, int destination_square) const
{
    // Direct check
    if (precomputed_moves::white_pawn_attacks[destination_square] & m_black_king_bit)
        return true;
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isKnightCheckOrDiscoverForBlack(int origin_square, int destination_square) const
{
    // Direct check
    if (precomputed_moves::knight_moves[destination_square] & m_black_king_bit)
        return true;
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    return false;
}
bool BitPosition::isBishopCheckOrDiscoverForBlack(int origin_square, int destination_square) const
{
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    // Direct check
    if ((precomputed_moves::precomputedBishopMovesTableOneBlocker2[destination_square][m_black_king_position] & m_all_pieces_bit) == m_black_king_bit)
        return true;
    return false;
}
bool BitPosition::isRookCheckOrDiscoverForBlack(int origin_square, int destination_square) const
{
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    // Direct check
    if ((precomputed_moves::precomputedRookMovesTableOneBlocker2[destination_square][m_black_king_position] & m_all_pieces_bit) == m_black_king_bit)
        return true;
    return false;
}
bool BitPosition::isQueenCheckOrDiscoverForBlack(int origin_square, int destination_square) const
{
    // Discovered check
    if (isDiscoverCheckForBlack(origin_square, destination_square))
        return true;
    // Direct check
    if ((precomputed_moves::precomputedQueenMovesTableOneBlocker2[destination_square][m_black_king_position] & m_all_pieces_bit) == m_black_king_bit)
        return true;
    return false;
}

// First move generations
std::vector<Move> BitPosition::inCheckAllMoves()
// For first move search, we have to initialize check info
{
    setCheckInfoOnInitialization();
    setBlockersAndPinsInAB();
    std::vector<Move> moves;
    moves.reserve(64);
    if (m_turn) // White's turn
    {
        if (m_num_checks == 1)
        {
            // Pawn Blocks and Captures
            // Single moves
            uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
            uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
            for (int destination : getBitIndices(single_pawn_moves_blocking_bit))
            {
                    if (destination < 56) // Non promotions
                    {
                        if (isNormalMoveLegal(destination - 8, destination))
                            moves.emplace_back(Move(destination - 8, destination));
                    }
                    else // Promotions
                    {
                        if (isNormalMoveLegal(destination - 8, destination))
                        {
                            moves.emplace_back(Move(destination - 8, destination, 0));
                            moves.emplace_back(Move(destination - 8, destination, 1));
                            moves.emplace_back(Move(destination - 8, destination, 2));
                            moves.emplace_back(Move(destination - 8, destination, 3));
                        }
                    }
            }
            // Double moves
            for (int destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
            {
                if (isNormalMoveLegal(destination - 16, destination))
                    moves.emplace_back(Move(destination - 16, destination));
            }
            // Passant block
            if (((state_info->pSquare) & m_check_rays) != 0)
            {
                for (int origin : getBitIndices(precomputed_moves::black_pawn_attacks[(state_info->pSquare)] & m_white_pawns_bit))
                    if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                    {
                        moves.emplace_back(Move(origin, (state_info->pSquare)));
                    }
            }
            // Pawn captures from checking position
            for (int origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_check_square] & m_white_pawns_bit))
            {
                if (m_check_square >= 56) // Promotions
                {
                    if (isNormalMoveLegal(origin, m_check_square))
                    {
                        moves.emplace_back(Move(origin, m_check_square, 0));
                        moves.emplace_back(Move(origin, m_check_square, 1));
                        moves.emplace_back(Move(origin, m_check_square, 2));
                        moves.emplace_back(Move(origin, m_check_square, 3));
                    }
                }
                else // Non promotion
                {
                    if (isNormalMoveLegal(origin, m_check_square))
                        moves.emplace_back(Move(origin, m_check_square));
                }
            }
            // Knight Blocks and Captures
            for (int origin : getBitIndices(m_white_knights_bit & ~((state_info->pinnedPieces) )))
            {
                for (int destination : getBitIndices(precomputed_moves::knight_moves[origin] & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isNormalMoveLegal(origin, m_check_square))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Bishop blocks and Captures
            for (int origin : getBitIndices(m_white_bishops_bit & ~(state_info->straightPinnedPieces) ))
            {
                for (int destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isNormalMoveLegal(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Rook blocks and Captures
            for (int origin : getBitIndices(m_white_rooks_bit & ~(state_info->diagonalPinnedPieces) ))
            {
                for (int destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isNormalMoveLegal(origin, m_check_square))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Queen blocks and captures
            for (int origin : getBitIndices(m_white_queens_bit))
            {
                for (int destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_white_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isNormalMoveLegal(origin, m_check_square))
                        moves.emplace_back(Move(origin, destination));
                }
            }
        }
        // King moves
        for (int destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_white_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                moves.emplace_back(Move(m_white_king_position, destination));
        }
    }
    else // Blacks turn
    {
        if (m_num_checks == 1)
        {
            // Pawn Blocks and Captures
            // Single moves
            uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
            uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
            for (int destination : getBitIndices(single_pawn_moves_blocking_bit))
            {
                    if (destination > 7) // Non promotions
                    {
                        if (isNormalMoveLegal(destination + 8, destination))
                            moves.emplace_back(Move(destination + 8, destination));
                    }
                    else // Promotions
                    {
                        if (isNormalMoveLegal(destination + 8, destination))
                        {
                            moves.emplace_back(Move(destination + 8, destination, 0));
                            moves.emplace_back(Move(destination + 8, destination, 1));
                            moves.emplace_back(Move(destination + 8, destination, 2));
                            moves.emplace_back(Move(destination + 8, destination, 3));
                        }
                    }
            }
            // Double moves
            for (int destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
            {
                if (isNormalMoveLegal(destination + 16, destination))
                    moves.emplace_back(Move(destination + 16, destination));
            }
            // Passant block
            if (((state_info->pSquare) & m_check_rays) != 0)
            {
                for (int origin : getBitIndices(precomputed_moves::white_pawn_attacks[(state_info->pSquare)] & m_black_pawns_bit))
                    if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8)) // Legal
                    {
                        moves.emplace_back(Move(origin, (state_info->pSquare)));
                    }
            }
            // Pawn captures from checking position
            for (int origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_check_square] & m_black_pawns_bit))
            {
                        if (m_check_square <= 7) // Promotions
                        {
                            if (isNormalMoveLegal(origin, m_check_square))
                            {
                                moves.emplace_back(Move(origin, m_check_square, 0));
                                moves.emplace_back(Move(origin, m_check_square, 1));
                                moves.emplace_back(Move(origin, m_check_square, 2));
                                moves.emplace_back(Move(origin, m_check_square, 3));
                            }
                        }
                        else // Non promotion
                        {
                            if (isNormalMoveLegal(origin, m_check_square))
                                moves.emplace_back(Move(origin, m_check_square));
                        }
            }
            // Knight blocks and Captures
            for (int origin : getBitIndices(m_black_knights_bit & ~((state_info->pinnedPieces) )))
            {
                for (int destination : getBitIndices(precomputed_moves::knight_moves[origin] & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isNormalMoveLegal(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Bishop blocks and Captures
            for (int origin : getBitIndices(m_black_bishops_bit & ~(state_info->straightPinnedPieces) ))
            {
                for (int destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isNormalMoveLegal(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Rook blocks and Captures
            for (int origin : getBitIndices(m_black_rooks_bit & ~(state_info->diagonalPinnedPieces) ))
            {
                for (int destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isNormalMoveLegal(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
            // Queen blocks and captures
            for (int origin : getBitIndices(m_black_queens_bit))
            {
                for (int destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_black_pieces_bit & (m_check_rays | 1ULL << m_check_square)))
                {
                    if (isNormalMoveLegal(origin, destination))
                        moves.emplace_back(Move(origin, destination));
                }
            }
        }
        // King moves
        for (int destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_black_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                moves.emplace_back(Move(m_black_king_position, destination));
        }
    }
    return moves;
}
std::vector<Move> BitPosition::allMoves()
// For first move search
{
    setBlockersAndPinsInAB();
    std::vector<Move> moves;
    moves.reserve(128);
    if (m_turn) // White's turn
    {
        // Knights
        for (int origin : getBitIndices(m_white_knights_bit & ~((state_info->pinnedPieces))))
        {
            for (int destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_white_pieces_bit))
            {
                moves.emplace_back(Move(origin, destination));
            }
        }
        // Bishops
        for (int origin : getBitIndices(m_white_bishops_bit & ~(state_info->straightPinnedPieces)))
        {
            for (int destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit))
            {
                if (isNormalMoveLegal(origin, destination))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Rooks
        for (int origin : getBitIndices(m_white_rooks_bit & ~(state_info->diagonalPinnedPieces)))
        {
            for (int destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit))
            {
                if (isNormalMoveLegal(origin, destination))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Queens
        for (int origin : getBitIndices(m_white_queens_bit))
        {
            for (int destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_white_pieces_bit))
            {
                if (isNormalMoveLegal(origin, destination))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces)) & ~m_all_pieces_bit};
        for (int destination : getBitIndices(single_pawn_moves_bit))
        {
            if (isNormalMoveLegal(destination - 8, destination))
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
        for (int destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            if (isNormalMoveLegal(destination - 16, destination))
                moves.emplace_back(Move(destination - 16, destination));
        }
        // Right shift captures
        for (int destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_pieces_bit))
        {
            if (isNormalMoveLegal(destination - 9, destination))
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
        for (int destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_pieces_bit))
        {
            if (isNormalMoveLegal(destination - 7, destination))
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
        if ((state_info->pSquare) != 0)
        {
            for (int origin : getBitIndices(precomputed_moves::black_pawn_attacks[(state_info->pSquare)] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    moves.emplace_back(Move(origin, (state_info->pSquare)));
                }
        }

        // King
        for (int destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_white_pieces_bit))
        {
            if (newWhiteKingSquareIsSafe(destination))
                moves.emplace_back(Move(m_white_king_position, destination));
        }
        // Kingside castling
        if ((state_info->whiteKSCastling) && (m_all_pieces_bit & 96) == 0 && newWhiteKingSquareIsSafe(5) && newWhiteKingSquareIsSafe(6))
            moves.emplace_back(castling_moves[0]);
        // Queenside castling
        if ((state_info->whiteQSCastling) && (m_all_pieces_bit & 14) == 0 && newWhiteKingSquareIsSafe(2) && newWhiteKingSquareIsSafe(3))
            moves.emplace_back(castling_moves[1]);
    }
    else // Black's turn
    {
        // Knights
        for (int origin : getBitIndices(m_black_knights_bit & ~((state_info->pinnedPieces) )))
        {
            for (int destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_black_pieces_bit))
            {
                moves.emplace_back(Move(origin, destination));
            }
        }
        // Bishops
        for (int origin : getBitIndices(m_black_bishops_bit & ~(state_info->straightPinnedPieces) ))
        {
            for (int destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit))
            {
                if (isNormalMoveLegal(origin, destination))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Rooks
        for (int origin : getBitIndices(m_black_rooks_bit & ~(state_info->diagonalPinnedPieces) ))
        {
            for (int destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit))
            {
                if (isNormalMoveLegal(origin, destination))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Queens
        for (int origin : getBitIndices(m_black_queens_bit))
        {
            for (int destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_black_pieces_bit))
            {
                if (isNormalMoveLegal(origin, destination))
                    moves.emplace_back(Move(origin, destination));
            }
        }
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        for (int destination : getBitIndices(single_pawn_moves_bit))
        {
            if (isNormalMoveLegal(destination + 8, destination))
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
        for (int destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            if (isNormalMoveLegal(destination + 16, destination))
                moves.emplace_back(Move(destination + 16, destination));
        }
        // Right shift captures
        for (int destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_white_pieces_bit))
        {
            if (isNormalMoveLegal(destination + 7, destination))
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
        for (int destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_white_pieces_bit))
        {
            if (isNormalMoveLegal(destination + 9, destination))
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
        if ((state_info->pSquare) != 0)
        {
            for (int origin : getBitIndices(precomputed_moves::white_pawn_attacks[(state_info->pSquare)] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8))
                {
                        moves.emplace_back(Move(origin, (state_info->pSquare)));
                }
        }
        // King moves
        for (int destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_black_pieces_bit))
        {
            if (newBlackKingSquareIsSafe(destination))
                moves.emplace_back(Move(m_black_king_position, destination));
        }
        // Kingside castling
        if ((state_info->blackKSCastling) && (m_all_pieces_bit & 6917529027641081856) == 0 && newBlackKingSquareIsSafe(61) && newBlackKingSquareIsSafe(62))
            moves.emplace_back(castling_moves[2]);
        // Queenside castling
        if ((state_info->blackQSCastling) && (m_all_pieces_bit & 1008806316530991104) == 0 && newBlackKingSquareIsSafe(58) && newBlackKingSquareIsSafe(59))
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
                int origin_square{move.getOriginSquare()};
                uint64_t origin_bit{(1ULL << origin_square)};
                int destination_square{move.getDestinationSquare()};
                uint64_t destination_bit{(1ULL << destination_square)};
                int score{0};
                // Moving piece 
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
                    else if ((destination_bit & m_black_queens_bit) != 0)
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
                int origin_square{move.getOriginSquare()};
                uint64_t origin_bit{(1ULL << origin_square)};
                int destination_square{move.getDestinationSquare()};
                uint64_t destination_bit{(1ULL << destination_square)};
                int score{0};
                // Moving piece unsafely
                if ((origin_bit & m_black_pawns_bit) != 0)
                    score = -1;

                else if ((origin_bit & m_black_knights_bit) != 0)
                    score = -2;

                else if ((origin_bit & m_black_bishops_bit) != 0)
                    score = -3;

                else if ((origin_bit & m_black_rooks_bit) != 0)
                    score = -4;

                else
                // Capture score
                if ((destination_bit & m_white_pieces_bit) != 0)
                {
                    if ((destination_bit & m_white_pawns_bit) != 0)
                        score += 10;
                    else if ((destination_bit & m_white_knights_bit) != 0)
                        score += 20;
                    else if ((destination_bit & m_white_bishops_bit) != 0)
                        score += 30;
                    else if ((destination_bit & m_white_rooks_bit) != 0)
                        score += 40;
                    else if ((destination_bit & m_white_queens_bit) != 0)
                        score += 50;
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
    if (m_turn)
    {
        // Right shift captures
        if ((shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD) & ~EIGHT_ROW_BITBOARD & (state_info->lastDestinationBit) ) != 0)
        {
            return Move(m_last_destination_square - 9, m_last_destination_square);
        }
        // Left shift captures
        if ((shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD) & ~EIGHT_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            return Move(m_last_destination_square - 7, m_last_destination_square);
        }
        // Right shift promotions
        if ((shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD) & EIGHT_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            return Move(m_last_destination_square - 9, m_last_destination_square, 3);
        }
        // Left shift promotions
        if ((shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD) & EIGHT_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            return Move(m_last_destination_square - 7, m_last_destination_square, 3);
        }
        // Knight refutation
        uint64_t moveable_pieces{precomputed_moves::knight_moves[m_last_destination_square] & m_white_knights_bit};
        while (moveable_pieces)
        {
            return Move(popLeastSignificantBit(moveable_pieces), m_last_destination_square);
        }
        // Bishop refutation
        moveable_pieces = BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_white_bishops_bit;
        while (moveable_pieces)
        {
            return Move(popLeastSignificantBit(moveable_pieces), m_last_destination_square);
        }
        // Rook refutation
        moveable_pieces = RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_white_rooks_bit;
        while (moveable_pieces)
        {
            return Move(popLeastSignificantBit(moveable_pieces), m_last_destination_square);
        }
        // Queen refutation
        moveable_pieces = (BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) | RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit)) & m_white_queens_bit;
        while (moveable_pieces)
        {
            return Move(popLeastSignificantBit(moveable_pieces), m_last_destination_square);
        }
        // King refutation
        if ((precomputed_moves::king_moves[m_white_king_position] & (state_info->lastDestinationBit)) != 0)
        {
            if (newWhiteKingSquareIsSafe(m_last_destination_square))
                return Move(m_white_king_position, m_last_destination_square);
        }
    }
    else // Black captures
    {
        // Right shift captures
        if ((shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD) & ~FIRST_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            return Move(m_last_destination_square + 7, m_last_destination_square);
        }
        // Left shift captures
        if ((shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD) & ~FIRST_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            return Move(m_last_destination_square + 9, m_last_destination_square);
        }
        // Right shift promotions
        if ((shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD) & FIRST_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            return Move(m_last_destination_square + 7, m_last_destination_square, 3);
        }
        // Left shift promotions
        if ((shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD) & FIRST_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            return Move(m_last_destination_square + 9, m_last_destination_square, 3);
        }
        // Knight refutation
        uint64_t moveable_pieces{precomputed_moves::knight_moves[m_last_destination_square] & m_black_knights_bit};
        while (moveable_pieces)
        {
            return Move(popLeastSignificantBit(moveable_pieces), m_last_destination_square);
        }
        // Bishop refutation
        moveable_pieces = BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_black_bishops_bit;
        while (moveable_pieces)
        {
            return Move(popLeastSignificantBit(moveable_pieces), m_last_destination_square);
        }
        // Rook refutation
        moveable_pieces = RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit) & m_black_rooks_bit;
        while (moveable_pieces)
        {
            return Move(popLeastSignificantBit(moveable_pieces), m_last_destination_square);
        }
        // Queen refutation
        moveable_pieces = (BmagicNOMASK(m_last_destination_square, precomputed_moves::bishop_unfull_rays[m_last_destination_square] & m_all_pieces_bit) | RmagicNOMASK(m_last_destination_square, precomputed_moves::rook_unfull_rays[m_last_destination_square] & m_all_pieces_bit)) & m_black_queens_bit;
        while (moveable_pieces)
        {
            return Move(popLeastSignificantBit(moveable_pieces), m_last_destination_square);
        }
        // King refutation
        if ((precomputed_moves::king_moves[m_black_king_position] & (state_info->lastDestinationBit)) != 0)
        {
            if (newWhiteKingSquareIsSafe(m_last_destination_square))
                return Move(m_black_king_position, m_last_destination_square);
        }
    }
    return Move(0);
}

// Capture move generations (for Quiesence) assuming pins are not set (we set them if we find a pseudo legal move)
ScoredMove *BitPosition::pawnCapturesAndQueenProms(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        // Right shift captures
        uint64_t destination_bitboard{shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD) & m_black_pieces_bit & ~EIGHT_ROW_BITBOARD};
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination - 9, destination);
        }
        // Left shift captures
        destination_bitboard = shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD) & m_black_pieces_bit & ~EIGHT_ROW_BITBOARD;
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination - 7, destination);
        }
        // Queen promotions
        destination_bitboard = shift_up(m_white_pawns_bit) & ~m_all_pieces_bit & EIGHT_ROW_BITBOARD;
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination - 8, destination, 3);
        }
        // Right shift captures and queen promotions
        destination_bitboard = shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD) & m_black_pieces_bit & EIGHT_ROW_BITBOARD;
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination - 9, destination, 3);
        }
        // Left shift captures and queen promotions
        destination_bitboard = shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD) & m_black_pieces_bit & EIGHT_ROW_BITBOARD;
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination - 7, destination, 3);
        }
    }
    else // Black captures
    {
        // Right shift captures
        uint64_t destination_bitboard{shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD) & m_white_pieces_bit & ~FIRST_ROW_BITBOARD};
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination + 7, destination);
        }
        // Left shift captures
        destination_bitboard = shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD) & m_white_pieces_bit & ~FIRST_ROW_BITBOARD;
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination + 9, destination);
        }
        // Queen promotions
        destination_bitboard = shift_down(m_black_pawns_bit) & ~m_all_pieces_bit & FIRST_ROW_BITBOARD;
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination + 8, destination, 3);
        }
        // Right shift captures and queen promotions
        destination_bitboard = shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD) & m_white_pieces_bit & FIRST_ROW_BITBOARD;
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination + 7, destination, 3);
        }
        // Left shift captures and queen promotions
        destination_bitboard = shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD) & m_white_pieces_bit & FIRST_ROW_BITBOARD;
        while (destination_bitboard)
        {
            int destination{popLeastSignificantBit(destination_bitboard)};
            *move_list++ = Move(destination + 9, destination, 3);
        }
    }
    return move_list;
}
ScoredMove *BitPosition::knightCaptures(ScoredMove *&move_list) const
// All knight captures
{
    if (m_turn)
    {
        uint64_t moveable_knights{m_white_knights_bit};
        while (moveable_knights)
        {
            int origin{popLeastSignificantBit(moveable_knights)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & m_black_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_knights{m_black_knights_bit};
        while (moveable_knights)
        {
            int origin{popLeastSignificantBit(moveable_knights)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & m_white_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove* BitPosition::bishopCaptures(ScoredMove*& move_list) const
// All bishop captures
{
    if (m_turn)
    {
        uint64_t moveable_bishops{m_white_bishops_bit};
        while (moveable_bishops)
        {
            int origin{popLeastSignificantBit(moveable_bishops)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_black_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_bishops{m_black_bishops_bit};
        while (moveable_bishops)
        {
            int origin{popLeastSignificantBit(moveable_bishops)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_white_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove* BitPosition::rookCaptures(ScoredMove*& move_list) const
// All rook captures except capturing unsafe pawns, knights or rooks
{
    if (m_turn)
    {
        uint64_t moveable_rooks{m_white_rooks_bit};
        while (moveable_rooks)
        {
            int origin{popLeastSignificantBit(moveable_rooks)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_black_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_rooks{m_black_rooks_bit};
        while (moveable_rooks)
        {
            int origin{popLeastSignificantBit(moveable_rooks)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_white_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove* BitPosition::queenCaptures(ScoredMove*& move_list) const
// All queen captures except capturing unsafe pieces
{
    if (m_turn)
    {
        uint64_t moveable_queens{m_white_queens_bit};
        while (moveable_queens)
        {
            int origin{popLeastSignificantBit(moveable_queens)};
            uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_black_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_queens{m_black_queens_bit};
        while (moveable_queens)
        {
            int origin{popLeastSignificantBit(moveable_queens)};
            uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_white_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove* BitPosition::kingCaptures(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        uint64_t destinations{precomputed_moves::king_moves[m_white_king_position] & m_black_pieces_bit};
        while (destinations)
                *move_list++ = Move(m_white_king_position, popLeastSignificantBit(destinations));
    }
    else
    {
        uint64_t destinations{precomputed_moves::king_moves[m_black_king_position] & m_white_pieces_bit};
        while (destinations)
                *move_list++ = Move(m_black_king_position, popLeastSignificantBit(destinations));
    }
    return move_list;
}
Move* BitPosition::kingCaptures(Move *&move_list) const
{
    if (m_turn)
    {
        uint64_t destinations{precomputed_moves::king_moves[m_white_king_position] & m_black_pieces_bit};
        while (destinations)
            *move_list++ = Move(m_white_king_position, popLeastSignificantBit(destinations));
    }
    else
    {
        uint64_t destinations{precomputed_moves::king_moves[m_black_king_position] & m_white_pieces_bit};
        while (destinations)
            *move_list++ = Move(m_black_king_position, popLeastSignificantBit(destinations));
    }
    return move_list;
}

// All move generations (for PV Nodes in Alpha-Beta)
ScoredMove* BitPosition::pawnAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        // Single moves
        uint64_t piece_moves{shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
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
        piece_moves = shift_up((shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit) & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination - 16, destination);
        }
        // Right shift captures
        piece_moves = shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_pieces_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
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
        piece_moves = shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_pieces_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
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
        if ((state_info->pSquare) != 0)
        {
            piece_moves = precomputed_moves::black_pawn_attacks[(state_info->pSquare)] & m_white_pawns_bit;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
            }
        }
    }
    else // Black moves
    {
        // Single moves
        uint64_t piece_moves{shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
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
        piece_moves = shift_down((shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit) & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination + 16, destination);
        }
        // Right shift captures
        piece_moves = shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_white_pieces_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
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
        piece_moves = shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_white_pieces_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
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
        if ((state_info->pSquare) != 0)
        {
            piece_moves = precomputed_moves::white_pawn_attacks[(state_info->pSquare)] & m_black_pawns_bit;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
            }
        }
    }
    return move_list;
}
ScoredMove* BitPosition::knightAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        uint64_t moveable_knights{m_white_knights_bit & ~(state_info->pinnedPieces) };
        while (moveable_knights)
        {
            int origin{popLeastSignificantBit(moveable_knights)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & ~m_white_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_knights{m_black_knights_bit & ~(state_info->pinnedPieces) };
        while (moveable_knights)
        {
            int origin{popLeastSignificantBit(moveable_knights)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & ~m_black_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove* BitPosition::bishopAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        uint64_t moveable_bishops{m_white_bishops_bit & ~(state_info->straightPinnedPieces) };
        while (moveable_bishops)
        {
            int origin{popLeastSignificantBit(moveable_bishops)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_bishops{m_black_bishops_bit & ~(state_info->straightPinnedPieces) };
        while (moveable_bishops)
        {
            int origin{popLeastSignificantBit(moveable_bishops)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove* BitPosition::rookAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        uint64_t moveable_rooks{m_white_rooks_bit & ~(state_info->diagonalPinnedPieces) };
        while (moveable_rooks)
        {
            int origin{popLeastSignificantBit(moveable_rooks)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_white_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_rooks{m_black_rooks_bit & ~(state_info->diagonalPinnedPieces) };
        while (moveable_rooks)
        {
            int origin{popLeastSignificantBit(moveable_rooks)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_black_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove* BitPosition::queenAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        uint64_t moveable_queens{m_white_queens_bit};
        while (moveable_queens)
        {
            int origin{popLeastSignificantBit(moveable_queens)};
            uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_white_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_queens{m_black_queens_bit};
        while (moveable_queens)
        {
            int origin{popLeastSignificantBit(moveable_queens)};
            uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_black_pieces_bit};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove* BitPosition::kingAllMoves(ScoredMove *&move_list) const
{
    if (m_turn)
    {
        uint64_t destinations{precomputed_moves::king_moves[m_white_king_position] & ~m_white_pieces_bit};
        while (destinations)
            *move_list++ = Move(m_white_king_position, popLeastSignificantBit(destinations));
        // Kingside castling
        if ((state_info->whiteKSCastling) && (m_all_pieces_bit & 96) == 0)
            *move_list++ = castling_moves[0];
        // Queenside castling
        if ((state_info->whiteQSCastling) && (m_all_pieces_bit & 14) == 0)
            *move_list++ = castling_moves[1];
    }
    else
    {
        uint64_t destinations{precomputed_moves::king_moves[m_black_king_position] & ~m_black_pieces_bit};
        while (destinations)
            *move_list++ = Move(m_black_king_position, popLeastSignificantBit(destinations));
        // Kingside castling
        if ((state_info->blackKSCastling) && (m_all_pieces_bit & 6917529027641081856) == 0)
            *move_list++ = castling_moves[2];
        // Queenside castling
        if ((state_info->blackQSCastling) && (m_all_pieces_bit & 1008806316530991104) == 0)
            *move_list++ = castling_moves[3];
    }
    return move_list;
}

// Rest Move generations (for Non PV Nodes in Alpha-Beta)
ScoredMove *BitPosition::pawnRestMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (refutations are capturing (state_info->lastDestinationBit)) and non unsafe moves
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        uint64_t piece_moves = single_pawn_moves_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            if (destination < 56)
                *move_list++ = Move(destination - 8, destination);
            else
            {
                *move_list++ = Move(destination - 8, destination, 0);
                *move_list++ = Move(destination - 8, destination, 1);
                *move_list++ = Move(destination - 8, destination, 2);
            }
        }
        // Double moves
        piece_moves = shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination - 16, destination);
        }
        // Right shift captures non queen promotions
        piece_moves = shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & EIGHT_ROW_BITBOARD & m_black_pieces_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination - 9, destination, 0);
            *move_list++ = Move(destination - 9, destination, 1);
            *move_list++ = Move(destination - 9, destination, 2);
        }
        // Left shift captures non queen promotions
        piece_moves = shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & EIGHT_ROW_BITBOARD & m_black_pieces_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination - 7, destination, 0);
            *move_list++ = Move(destination - 7, destination, 1);
            *move_list++ = Move(destination - 7, destination, 2);
        }
        // Right shift pawn captures non refutations
        piece_moves = shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_black_pawns_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination - 9, destination);
        }
        // Left shift pawn captures non refutations
        piece_moves = shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_black_pawns_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination - 7, destination);
        }
        // Passant
        if (state_info->pSquare)
        {
            piece_moves = precomputed_moves::black_pawn_attacks[(state_info->pSquare)] & m_white_pawns_bit;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
            }
        }
    }
    else // Black moves
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces)) & ~m_all_pieces_bit};
        uint64_t piece_moves = single_pawn_moves_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            if (destination > 7)
                *move_list++ = Move(destination + 8, destination);
            else
            {
                *move_list++ = Move(destination + 8, destination, 0);
                *move_list++ = Move(destination + 8, destination, 1);
                *move_list++ = Move(destination + 8, destination, 2);
            }
        }
        // Double moves
        piece_moves = shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination + 16, destination);
        }
        // Right shift captures non queen promotions
        piece_moves = shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & FIRST_ROW_BITBOARD & m_white_pieces_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination + 7, destination, 0);
            *move_list++ = Move(destination + 7, destination, 1);
            *move_list++ = Move(destination + 7, destination, 2);
        }
        // Left shift captures non queen promotions
        piece_moves = shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & FIRST_ROW_BITBOARD & m_black_pieces_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination + 9, destination, 0);
            *move_list++ = Move(destination + 9, destination, 1);
            *move_list++ = Move(destination + 9, destination, 2);
        }
        // Right shift pawn captures non refutations
        piece_moves = shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_white_pawns_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination + 7, destination);
        }
        // Left shift pawn captures non refutations
        piece_moves = shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_white_pawns_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list++ = Move(destination + 9, destination);
        }
        // Passant
        if (state_info->pSquare)
        {
            piece_moves = precomputed_moves::white_pawn_attacks[(state_info->pSquare)] & m_black_pawns_bit;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
            }
        }
    }
    return move_list;
}
ScoredMove *BitPosition::knightRestMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (capturing (state_info->lastDestinationBit)) and non unsafe moves
{
    if (m_turn)
    {
        uint64_t moveable_knights{m_white_knights_bit & ~(state_info->pinnedPieces) };
        while (moveable_knights)
        {
            int origin{popLeastSignificantBit(moveable_knights)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & ~(m_white_pieces_bit | m_black_queens_bit | m_black_rooks_bit | (state_info->lastDestinationBit))};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_knights{m_black_knights_bit & ~(state_info->pinnedPieces) };
        while (moveable_knights)
        {
            int origin{popLeastSignificantBit(moveable_knights)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & ~(m_black_pieces_bit | m_white_queens_bit | m_white_rooks_bit | (state_info->lastDestinationBit))};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove *BitPosition::bishopRestMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (capturing (state_info->lastDestinationBit))
{
    if (m_turn)
    {
        uint64_t moveable_bishops{m_white_bishops_bit & ~(state_info->straightPinnedPieces) };
        while (moveable_bishops)
        {
            int origin{popLeastSignificantBit(moveable_bishops)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~(m_white_pieces_bit | m_black_queens_bit | m_black_rooks_bit | (state_info->lastDestinationBit))};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_bishops{m_black_bishops_bit & ~(state_info->straightPinnedPieces) };
        while (moveable_bishops)
        {
            int origin{popLeastSignificantBit(moveable_bishops)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~(m_black_pieces_bit | m_white_queens_bit | m_white_rooks_bit | (state_info->lastDestinationBit))};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove *BitPosition::rookRestMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (capturing (state_info->lastDestinationBit))
{
    if (m_turn)
    {
        uint64_t moveable_rooks{m_white_rooks_bit & ~(state_info->diagonalPinnedPieces) };
        while (moveable_rooks)
        {
            int origin{popLeastSignificantBit(moveable_rooks)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~(m_white_pieces_bit | m_black_queens_bit | (state_info->lastDestinationBit))};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_rooks{m_black_rooks_bit & ~(state_info->diagonalPinnedPieces) };
        while (moveable_rooks)
        {
            int origin{popLeastSignificantBit(moveable_rooks)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~(m_black_pieces_bit | m_white_queens_bit | (state_info->lastDestinationBit))};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove *BitPosition::queenRestMoves(ScoredMove *&move_list) const
// Non good captures and non refutations (capturing (state_info->lastDestinationBit))
{
    if (m_turn)
    {
        uint64_t moveable_queens{m_white_queens_bit};
        while (moveable_queens)
        {
            int origin{popLeastSignificantBit(moveable_queens)};
            uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~(m_white_pieces_bit | (state_info->lastDestinationBit))};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    else
    {
        uint64_t moveable_queens{m_black_queens_bit};
        while (moveable_queens)
        {
            int origin{popLeastSignificantBit(moveable_queens)};
            uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~(m_black_pieces_bit | (state_info->lastDestinationBit))};
            while (destinations)
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
ScoredMove *BitPosition::kingNonCapturesAndPawnCaptures(ScoredMove *&move_list) const
// King non captures
{
    if (m_turn)
    {
        uint64_t destinations{precomputed_moves::king_moves[m_white_king_position] & (~m_all_pieces_bit | (m_black_pawns_bit & ~(state_info->lastDestinationBit)))};
        while (destinations)
            *move_list++ = Move(m_white_king_position, popLeastSignificantBit(destinations));
        // Kingside castling
        if ((state_info->whiteKSCastling) && (m_all_pieces_bit & 96) == 0)
            *move_list++ = castling_moves[0];
        // Queenside castling
        if ((state_info->whiteQSCastling) && (m_all_pieces_bit & 14) == 0)
            *move_list++ = castling_moves[1];
    }
    else
    {
        uint64_t destinations{precomputed_moves::king_moves[m_black_king_position] & (~m_all_pieces_bit | (m_white_pawns_bit & ~(state_info->lastDestinationBit)))};
        while (destinations)
            *move_list++ = Move(m_black_king_position, popLeastSignificantBit(destinations));
        // Kingside castling
        if ((state_info->blackKSCastling) && (m_all_pieces_bit & 6917529027641081856) == 0)
            *move_list++ = castling_moves[2];
        // Queenside castling
        if ((state_info->blackQSCastling) && (m_all_pieces_bit & 1008806316530991104) == 0)
            *move_list++ = castling_moves[3];
    }
    return move_list;
}

// Check blocks (for Alpha-Beta)
Move *BitPosition::inCheckPawnBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        uint64_t pawn_blocks{single_pawn_moves_bit & m_check_rays};
        while (pawn_blocks)
        {
            int destination{popLeastSignificantBit(pawn_blocks)};
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
        pawn_blocks = shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays;
        while (pawn_blocks)
        {
            int destination{popLeastSignificantBit(pawn_blocks)};
            *move_list++ = Move(destination - 16, destination);
        }
    }
    else
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        uint64_t pawn_blocks{single_pawn_moves_bit & m_check_rays};
        while (pawn_blocks)
        {
            int destination{popLeastSignificantBit(pawn_blocks)};
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
        pawn_blocks = shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays;
        while (pawn_blocks)
        {
            int destination{popLeastSignificantBit(pawn_blocks)};
            *move_list++ = Move(destination + 16, destination);
        }
    }
    return move_list;
}
Move *BitPosition::inCheckKnightBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        uint64_t pieces{m_white_knights_bit & ~(state_info->pinnedPieces) };
        while (pieces)
        {
            int origin{popLeastSignificantBit(pieces)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & m_check_rays};
            while (destinations)
            {
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    else
    {
        uint64_t pieces{m_black_knights_bit & ~(state_info->pinnedPieces) };
        while (pieces)
        {
            int origin{popLeastSignificantBit(pieces)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & m_check_rays};
            while (destinations)
            {
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    return move_list;
}
Move *BitPosition::inCheckBishopBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        uint64_t pieces{m_white_bishops_bit & ~(state_info->straightPinnedPieces) };
        while (pieces)
        {
            int origin{popLeastSignificantBit(pieces)};
            uint64_t destiantions{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
            while (destiantions)
            {
                *move_list++ = Move(origin, popLeastSignificantBit(destiantions));
            }
        }
    }
    else
    {
        uint64_t pieces{m_black_bishops_bit & ~(state_info->straightPinnedPieces) };
        while (pieces)
        {
            int origin{popLeastSignificantBit(pieces)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
            while (destinations)
            {
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    return move_list;
}
Move *BitPosition::inCheckRookBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        uint64_t pieces{m_white_rooks_bit & ~(state_info->diagonalPinnedPieces) };
        while (pieces)
        {
            int origin{popLeastSignificantBit(pieces)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
            while (destinations)
            {
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    else
    {
        uint64_t pieces{m_black_rooks_bit & ~(state_info->diagonalPinnedPieces) };
        while (pieces)
        {
            int origin{popLeastSignificantBit(pieces)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
            while (destinations)
            {
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    return move_list;
}
Move *BitPosition::inCheckQueenBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        uint64_t pieces{m_white_queens_bit};
        while (pieces)
        {
            int origin{popLeastSignificantBit(pieces)};
            uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays};
            while (destinations)
            {
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    else
    {
        uint64_t pieces{m_black_queens_bit};
        while (pieces)
        {
            int origin{popLeastSignificantBit(pieces)};
            uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays};
            while (destinations)
            {
                *move_list++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    return move_list;
}
Move *BitPosition::inCheckOrderedCapturesAndKingMoves(Move *&move_list) const
// Only called if m_num_checks = 1
{
    if (m_turn)
    {
        // King captures
        uint64_t piece_moves = precomputed_moves::king_moves[m_white_king_position] & m_black_pieces_bit;
        while (piece_moves)
            *move_list++ = Move(m_white_king_position, popLeastSignificantBit(piece_moves));
        // Pawn captures from checking position
        piece_moves = precomputed_moves::black_pawn_attacks[m_check_square] & m_white_pawns_bit;
        while (piece_moves)
        {
            int origin{popLeastSignificantBit(piece_moves)};
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
        if ((state_info->pSquare) != 0)
        {
            piece_moves = precomputed_moves::black_pawn_attacks[(state_info->pSquare)] & m_white_pawns_bit;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                        *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
            }
        }
        // Knight captures from checking position
        piece_moves = precomputed_moves::knight_moves[m_check_square] & m_white_knights_bit & ~((state_info->pinnedPieces) );
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Bishop captures from checking position
        piece_moves = BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_white_bishops_bit & ~(state_info->straightPinnedPieces) ;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Rook captures from checking position
        piece_moves = RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_white_rooks_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Queen captures from checking position
        piece_moves = (BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_white_queens_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // King non captures
        piece_moves = precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit;
        while (piece_moves)
            *move_list++ = Move(m_white_king_position, popLeastSignificantBit(piece_moves));
    }
    else
    {
        // King captures
        uint64_t piece_moves = precomputed_moves::king_moves[m_black_king_position] & m_white_pieces_bit;
        while (piece_moves)
            *move_list++ = Move(m_black_king_position, popLeastSignificantBit(piece_moves));

        // Pawn captures from checking position
        piece_moves = precomputed_moves::white_pawn_attacks[m_check_square] & m_black_pawns_bit;
        while (piece_moves)
        {
            int origin{popLeastSignificantBit(piece_moves)};
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
        if ((state_info->pSquare) != 0)
        {
            piece_moves = precomputed_moves::white_pawn_attacks[(state_info->pSquare)] & m_black_pawns_bit;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
            }
        }
        // Knight captures from checking position
        piece_moves = precomputed_moves::knight_moves[m_check_square] & m_black_knights_bit & ~((state_info->pinnedPieces) );
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Bishop captures from checking position
        piece_moves = BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_black_bishops_bit & ~(state_info->straightPinnedPieces) ;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Rook captures from checking position
        piece_moves = RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_black_rooks_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Queen captures from checking position
        piece_moves = (BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_black_queens_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // King non captures
        piece_moves = precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit;
        while (piece_moves)
            *move_list++ = Move(m_black_king_position, popLeastSignificantBit(piece_moves));
    }
    return move_list;
}
Move *BitPosition::kingAllMovesInCheck(Move *&move_list) const
{
    if (m_turn)
    {
        uint64_t king_moves{precomputed_moves::king_moves[m_white_king_position] & ~m_white_pieces_bit};
        while(king_moves)
        {
            *move_list++ = Move(m_white_king_position, popLeastSignificantBit(king_moves));
        }
    }
    else
    {
        uint64_t king_moves{precomputed_moves::king_moves[m_black_king_position] & ~m_black_pieces_bit};
        while (king_moves)
        {
                *move_list++ = Move(m_black_king_position, popLeastSignificantBit(king_moves));
        }
    }
    return move_list;
}

// Captures in check (for Quiesence)
Move *BitPosition::inCheckOrderedCaptures(Move *&move_list) const
// Only called if m_num_checks = 1
{
    if (m_turn)
    {
        // King captures
        uint64_t piece_moves = precomputed_moves::king_moves[m_white_king_position] & m_black_pieces_bit;
        while (piece_moves)
        {
            *move_list++ = Move(m_white_king_position, popLeastSignificantBit(piece_moves));
        }
        // Pawn captures from checking position
        piece_moves = precomputed_moves::black_pawn_attacks[m_check_square] & m_white_pawns_bit;
        while (piece_moves)
        {
            if (m_check_square < 56) // Non promotions
            {
                *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
            }
            else // Promotions
            {
                *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square, 3);
            }
        }
        // Knight captures from checking position
        piece_moves = precomputed_moves::knight_moves[m_check_square] & m_white_knights_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Bishop captures from checking position
        piece_moves = BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_white_bishops_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Rook captures from checking position
        piece_moves = RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_white_rooks_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Queen captures from checking position
        piece_moves = (BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_white_queens_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
    }
    else
    {
        // King captures
        uint64_t piece_moves = precomputed_moves::king_moves[m_black_king_position] & m_white_pieces_bit;
        while (piece_moves)
                *move_list++ = Move(m_black_king_position, popLeastSignificantBit(piece_moves));

        // Pawn captures from checking position
        piece_moves = precomputed_moves::white_pawn_attacks[m_check_square] & m_black_pawns_bit;
        while (piece_moves)
        {
            if (m_check_square > 7) // Non promotions
            {
                *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
            }
            else // Promotions
            {
                *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square, 3);
            }
        }
        // Knight captures from checking position
        piece_moves = precomputed_moves::knight_moves[m_check_square] & m_black_knights_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Bishop captures from checking position
        piece_moves = BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_black_bishops_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Rook captures from checking position
        piece_moves = RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_black_rooks_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        // Queen captures from checking position
        piece_moves = (BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_black_queens_bit;
        while (piece_moves)
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
    }
    return move_list;
}

// For Alpha-Beta (For non PV nodes)
ScoredMove *BitPosition::setRefutationMovesOrdered(ScoredMove *&move_list_end)
// All captures of last destination square ordered by worst moving piece first
{
    int last_destination_square = getLeastSignificantBitIndex(state_info->lastDestinationBit);
    if (m_turn)
    {
        // Capturing with king
        if ((precomputed_moves::king_moves[last_destination_square] & m_white_king_bit) != 0)
        {
            *move_list_end++ = Move(m_white_king_position, last_destination_square);
        }
        // Capturing with pawn
        // Right shift captures
        if ((shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & ~EIGHT_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            *move_list_end++ = Move(last_destination_square - 9, last_destination_square);
        }
        // Left shift captures
        if ((shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & ~EIGHT_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            *move_list_end++ = Move(last_destination_square - 7, last_destination_square);
        }
        // Right shift promotions
        if ((shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & EIGHT_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            *move_list_end++ = Move(last_destination_square - 9, last_destination_square, 3);
        }
        // Left shift promotions
        if ((shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & EIGHT_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            *move_list_end++ = Move(last_destination_square - 7, last_destination_square, 3);
        }
        // Capturing with knight
        uint64_t piece_moves{precomputed_moves::knight_moves[last_destination_square] & m_white_knights_bit & ~((state_info->pinnedPieces) )};
        while (piece_moves)
        {
            *move_list_end++ = Move(popLeastSignificantBit(piece_moves), last_destination_square);
        }
        // Capturing with bishop
        piece_moves = BmagicNOMASK(last_destination_square, precomputed_moves::bishop_unfull_rays[last_destination_square] & m_all_pieces_bit) & m_white_bishops_bit & ~(state_info->straightPinnedPieces) ;
        while (piece_moves)
        {
            *move_list_end++ = Move(popLeastSignificantBit(piece_moves), last_destination_square);
        }
        // Capturing with rook
        piece_moves = RmagicNOMASK(last_destination_square, precomputed_moves::rook_unfull_rays[last_destination_square] & m_all_pieces_bit) & m_white_rooks_bit & ~(state_info->diagonalPinnedPieces) ;
        while (piece_moves)
        {
            *move_list_end++ = Move(popLeastSignificantBit(piece_moves), last_destination_square);
        }
        // Capturing with queen
        piece_moves = (BmagicNOMASK(last_destination_square, precomputed_moves::bishop_unfull_rays[last_destination_square] & m_all_pieces_bit) | RmagicNOMASK(last_destination_square, precomputed_moves::rook_unfull_rays[last_destination_square] & m_all_pieces_bit)) & m_white_queens_bit;
        while (piece_moves)
        {
            *move_list_end++ = Move(popLeastSignificantBit(piece_moves), last_destination_square);
        }
    }
    else
    {
        // Capturing with king
        if ((precomputed_moves::king_moves[last_destination_square] & m_black_king_bit) != 0)
        {
            *move_list_end++ = Move(m_black_king_position, last_destination_square);
        }
        // Capturing with pawn
        // Right shift captures
        if ((shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & ~FIRST_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            *move_list_end++ = Move(last_destination_square + 7, last_destination_square);
        }
        // Left shift captures
        if ((shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & ~FIRST_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            *move_list_end++ = Move(last_destination_square + 9, last_destination_square);
        }
        // Right shift promotions
        if ((shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & FIRST_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            *move_list_end++ = Move(last_destination_square + 7, last_destination_square, 3);
        }
        // Left shift promotions
        if ((shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & FIRST_ROW_BITBOARD & (state_info->lastDestinationBit)) != 0)
        {
            *move_list_end++ = Move(last_destination_square + 9, last_destination_square, 3);
        }
        // Capturing with knight
        uint64_t piece_moves{precomputed_moves::knight_moves[last_destination_square] & m_black_knights_bit & ~((state_info->pinnedPieces) )};
        while (piece_moves)
        {
            *move_list_end++ = Move(popLeastSignificantBit(piece_moves), last_destination_square);
        }
        // Capturing with bishop
        piece_moves = BmagicNOMASK(last_destination_square, precomputed_moves::bishop_unfull_rays[last_destination_square] & m_all_pieces_bit) & m_black_bishops_bit & ~(state_info->straightPinnedPieces) ;
        while (piece_moves)
        {
            *move_list_end++ = Move(popLeastSignificantBit(piece_moves), last_destination_square);
        }
        // Capturing with rook
        piece_moves = RmagicNOMASK(last_destination_square, precomputed_moves::rook_unfull_rays[last_destination_square] & m_all_pieces_bit) & m_black_rooks_bit & ~(state_info->diagonalPinnedPieces) ;
        while (piece_moves)
        {
            *move_list_end++ = Move(popLeastSignificantBit(piece_moves), last_destination_square);
        }
        // Capturing with queen
        piece_moves = (BmagicNOMASK(last_destination_square, precomputed_moves::bishop_unfull_rays[last_destination_square] & m_all_pieces_bit) | RmagicNOMASK(last_destination_square, precomputed_moves::rook_unfull_rays[last_destination_square] & m_all_pieces_bit)) & m_black_queens_bit;
        while (piece_moves)
        {
            *move_list_end++ = Move(popLeastSignificantBit(piece_moves), last_destination_square);
        }
    }
    return move_list_end;
}
ScoredMove *BitPosition::setGoodCapturesOrdered(ScoredMove *&move_list_end)
// All good captures ordered except refutations (capturing (state_info->lastDestinationBit))
{
    // setAttackedSquares();
    // Move *move_list_end = move_list_start; // Initially, end points to start
    if (m_turn)
    {
        // Pawn-Queen and Queen promotions
        // Right shift captures
        uint64_t piece_moves{shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_queens_bit & ~(state_info->lastDestinationBit) & ~EIGHT_ROW_BITBOARD};
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination - 9, destination);
        }
        // Left shift captures
        piece_moves = shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_queens_bit & ~(state_info->lastDestinationBit) & ~EIGHT_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination - 7, destination);
        }
        // Right shift queen promotions
        piece_moves = shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_pieces_bit & ~(state_info->lastDestinationBit)& EIGHT_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination - 9, destination, 3);
        }
        // Left shift queen promotions
        piece_moves = shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_pieces_bit & ~(state_info->lastDestinationBit) & EIGHT_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination - 7, destination, 3);
        }
        // King-Queen
        piece_moves = precomputed_moves::king_moves[m_white_king_position] & m_black_queens_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            *move_list_end++ = Move(m_white_king_position, popLeastSignificantBit(piece_moves));
        }
        // Queen promotions
        piece_moves = shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit & EIGHT_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination - 8, destination, 3);
        }
        // Pawn-Rook/Bishop/Knight
        // Right shift captures
        piece_moves = shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & (m_black_rooks_bit | m_black_bishops_bit | m_black_knights_bit) & ~(state_info->lastDestinationBit) & ~EIGHT_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination - 9, destination);
        }
        // Left shift captures
        piece_moves = shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & (m_black_rooks_bit | m_black_bishops_bit | m_black_knights_bit) & ~(state_info->lastDestinationBit) & ~EIGHT_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination - 7, destination);
        }
        // King-Rook/Bishop/Knight
        piece_moves = precomputed_moves::king_moves[m_white_king_position] & (m_black_rooks_bit | m_black_bishops_bit | m_black_knights_bit) & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            *move_list_end++ = Move(m_white_king_position, popLeastSignificantBit(piece_moves));
        }
        // Knight-Queen/Rook
        piece_moves = m_white_knights_bit & ~((state_info->pinnedPieces) );
        while (piece_moves)
        {
            int origin{popLeastSignificantBit(piece_moves)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & (m_black_rooks_bit | m_black_queens_bit) & ~(state_info->lastDestinationBit)};
            while (destinations)
            {
                *move_list_end++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
        // Bishop-Queen/Rook
        piece_moves = m_white_bishops_bit & ~(state_info->straightPinnedPieces) ;
        while (piece_moves)
        {
            int origin{popLeastSignificantBit(piece_moves)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & (m_black_rooks_bit | m_black_queens_bit) & ~(state_info->lastDestinationBit)};
            while (destinations)
            {
                *move_list_end++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
        // Rook-Queen
        piece_moves = m_white_rooks_bit & ~(state_info->diagonalPinnedPieces) ;
        while (piece_moves)
        {
            int origin{popLeastSignificantBit(piece_moves)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_black_queens_bit & ~(state_info->lastDestinationBit)};
            while (destinations)
            {
                *move_list_end++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    else
    {
        // Pawn-Queen and Queen promotions
        // Right shift captures
        uint64_t piece_moves{shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_white_queens_bit & ~(state_info->lastDestinationBit) & ~FIRST_ROW_BITBOARD};
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination +7, destination);
        }
        // Left shift captures
        piece_moves = shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_white_queens_bit & ~(state_info->lastDestinationBit) & ~FIRST_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination + 9, destination);
        }
        // Right shift queen promotions
        piece_moves = shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_white_pieces_bit & ~(state_info->lastDestinationBit) & FIRST_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination + 7, destination, 3);
        }
        // Left shift queen promotions
        piece_moves = shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_white_pieces_bit & ~(state_info->lastDestinationBit) & FIRST_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination + 9, destination, 3);
        }
        // King-Queen
        piece_moves = precomputed_moves::king_moves[m_black_king_position] & m_white_queens_bit & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            *move_list_end++ = Move(m_black_king_position, popLeastSignificantBit(piece_moves));
        }
        // Queen promotions
        piece_moves = shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces)) & ~m_all_pieces_bit & FIRST_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination + 8, destination, 3);
        }
        // Pawn-Rook/Bishop/Knight
        // Right shift captures
        piece_moves = shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & (m_white_rooks_bit | m_white_bishops_bit | m_white_knights_bit) & ~(state_info->lastDestinationBit) & ~FIRST_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination + 7, destination);
        }
        // Left shift captures
        piece_moves = shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & (m_white_rooks_bit | m_white_bishops_bit | m_white_knights_bit) & ~(state_info->lastDestinationBit) & ~FIRST_ROW_BITBOARD;
        while (piece_moves)
        {
            int destination{popLeastSignificantBit(piece_moves)};
            *move_list_end++ = Move(destination + 9, destination);
        }
        // King-Rook/Bishop/Knight
        piece_moves = precomputed_moves::king_moves[m_black_king_position] & (m_white_rooks_bit | m_white_bishops_bit | m_white_knights_bit) & ~(state_info->lastDestinationBit);
        while (piece_moves)
        {
            *move_list_end++ = Move(m_black_king_position, popLeastSignificantBit(piece_moves));
        }
        // Knight-Queen/Rook
        piece_moves = m_black_knights_bit & ~(state_info->pinnedPieces);
        while (piece_moves)
        {
            int origin{popLeastSignificantBit(piece_moves)};
            uint64_t destinations{precomputed_moves::knight_moves[origin] & (m_white_rooks_bit | m_white_queens_bit) & ~(state_info->lastDestinationBit)};
            while (destinations)
            {
                *move_list_end++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
        // Bishop-Queen/Rook
        piece_moves = m_black_bishops_bit & ~(state_info->straightPinnedPieces);
        while (piece_moves)
        {
            int origin{popLeastSignificantBit(piece_moves)};
            uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & (m_white_rooks_bit | m_white_queens_bit) & ~(state_info->lastDestinationBit)};
            while (destinations)
            {
                *move_list_end++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
        // Rook-Queen
        piece_moves = m_black_rooks_bit & ~(state_info->diagonalPinnedPieces);
        while (piece_moves)
        {
            int origin{popLeastSignificantBit(piece_moves)};
            uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_white_queens_bit & ~(state_info->lastDestinationBit)};
            while (destinations)
            {
                *move_list_end++ = Move(origin, popLeastSignificantBit(destinations));
            }
        }
    }
    return move_list_end;
}

// Moving pieces functions
void BitPosition::setPiece(uint64_t origin_bit, uint64_t destination_bit)
// Move a piece (except king), given an origin bit and destination bit
{
    if (m_turn)
    {
        if (origin_bit & m_white_pawns_bit)
        {
            m_white_pawns_bit &= ~origin_bit;
            m_white_pawns_bit |= destination_bit;
            m_moved_piece = 0;
            state_info->isCheck = isPawnCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
        }
        else if (origin_bit & m_white_knights_bit)
        {
            m_white_knights_bit &= ~origin_bit;
            m_white_knights_bit |= destination_bit;
            m_moved_piece = 1;
            state_info->isCheck = isKnightCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
        }
        else if (origin_bit & m_white_bishops_bit)
        {
            m_white_bishops_bit &= ~origin_bit;
            m_white_bishops_bit |= destination_bit;
            m_moved_piece = 2;
            state_info->isCheck = isBishopCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
        }
        else if (origin_bit & m_white_rooks_bit)
        {
            m_white_rooks_bit &= ~origin_bit;
            m_white_rooks_bit |= destination_bit;
            m_moved_piece = 3;
            state_info->isCheck = isRookCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
        }
        else
        {
            m_white_queens_bit &= ~origin_bit;
            m_white_queens_bit |= destination_bit;
            m_moved_piece = 4;
            state_info->isCheck = isQueenCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
        }
    }
    else
    {
        if (origin_bit & m_black_pawns_bit)
        {
            m_black_pawns_bit &= ~origin_bit;
            m_black_pawns_bit |= destination_bit;
            m_moved_piece = 0;
            state_info->isCheck = isPawnCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
        }
        else if (origin_bit & m_black_knights_bit)
        {
            m_black_knights_bit &= ~origin_bit;
            m_black_knights_bit |= destination_bit;
            m_moved_piece = 1;
            state_info->isCheck = isKnightCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
        }
        else if (origin_bit & m_black_bishops_bit)
        {
            m_black_bishops_bit &= ~origin_bit;
            m_black_bishops_bit |= destination_bit;
            m_moved_piece = 2;
            state_info->isCheck = isBishopCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
        }
        else if (origin_bit & m_black_rooks_bit)
        {
            m_black_rooks_bit &= ~origin_bit;
            m_black_rooks_bit |= destination_bit;
            m_moved_piece = 3;
            state_info->isCheck = isRookCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
        }
        else
        {
            m_black_queens_bit &= ~origin_bit;
            m_black_queens_bit |= destination_bit;
            m_moved_piece = 4;
            state_info->isCheck = isQueenCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
        }
    }
}

void BitPosition::resetPlyInfo()
// This member function is used when either opponent or engine makes a capture.
// To make the ply info smaller. For a faster 3 fold repetition check.
// It isn't used when making a move in search!
{
    m_ply = 0;
    m_zobrist_keys_array.fill(0);
    m_zobrist_keys_array[127] = state_info->zobristKey;
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
        if ((m_white_pawns_bit & origin_bit) != 0 && (state_info->pSquare) == destination_bit)
            return true;
    }
    else
    {
        if ((m_white_pieces_bit & destination_bit) != 0)
            return true;
        if ((m_black_pawns_bit & origin_bit) != 0 && (state_info->pSquare) == destination_bit)
            return true;
    }
    return false;
}

template <typename T>
bool BitPosition::positionIsDrawnAfterMove(T move) const
{
    // 50 move rule
    if (state_info->fiftyMoveCount > 99)
        return true;

    // Threefold repetitions
    int origin_square = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << origin_square);
    int destination_square = move.getDestinationSquare();
    uint64_t destination_bit = (1ULL << destination_square);
    int promoted_piece{7};
    // Promotions
    if (m_turn && (move.getData() & 0b0100000000000000) == 0b0100000000000000 && (destination_bit & EIGHT_ROW_BITBOARD) != 0)
        promoted_piece = move.getPromotingPiece() + 1;
    if (not m_turn && (move.getData() & 0b0100000000000000) == 0b0100000000000000 && (destination_bit & FIRST_ROW_BITBOARD) != 0)
        promoted_piece = move.getPromotingPiece() + 1;
    // Update the moved piece, captured piece and promoted piece part of the zobrist key
    uint64_t temp_zobrist_key{updateZobristKeyPiecePartBeforeMove(origin_square, destination_square, promoted_piece)};
    // Update the turn part in the zobrist key
    temp_zobrist_key ^= zobrist_keys::blackToMoveZobristNumber;

    // Update special parts of the zobrist key
    if (m_turn)
    {   
        if (origin_square == 0) // Moving from a1
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = ((state_info->whiteKSCastling) | (false << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (origin_square == 7) // Moving from h1
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = (false | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (destination_square == 63) // Capturing on a8
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | (false << 2) | ((state_info->blackQSCastling) << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (destination_square == 56) // Capturing on h8
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | (false << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_bit == m_white_king_bit) // Moving king
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = (false | (false << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }
        // Updating passant square
        temp_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[(state_info->pSquare)];
        int psquare{0};
        if ((origin_bit & m_white_pawns_bit) != 0  && (destination_square - origin_square) == 16)
            psquare = origin_square + 8;
        temp_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[psquare];
    }
    else
    {
        if (destination_square == 0) // Capturing on a1
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = ((state_info->whiteKSCastling) | (false << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (destination_square == 7) // Capturing on h1
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = (false | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_square == 63) // Moving from a8
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | (false << 2) | ((state_info->blackQSCastling) << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (origin_square == 56) // Moving from h8
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | (false << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_bit == m_black_king_bit) // Moving king
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | (false << 2) | (false << 3));
            temp_zobrist_key ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }
        // Updating passant square
        temp_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[(state_info->pSquare)];
        int psquare{0};
        if ((origin_bit & m_black_pawns_bit) != 0 && (origin_square - destination_square) == 16)
            psquare = destination_square + 8;
        temp_zobrist_key ^= zobrist_keys::passantSquaresZobristNumbers[psquare];
    }
    // Find how many keys are equal to this zobrist key
    // We are currently at m_ply,
    // Each time we make a move we do m_zobrist_keys_array[127 - m_ply] = zobrist_key
    // We must only check positions spaced by 2 moves since repetitions are from player's perspective
    int count = 0;
    for (int i = m_ply - 1; i >= 0; i -= 2)
    {
        if (m_zobrist_keys_array[127 - i] == temp_zobrist_key)
        {
            if (++count == 2)
                return true;
        }
    }
    
    return false;
}

template <typename T>
void BitPosition::makeMove(T move, StateInfo &new_state_info)
// Move piece and switch white and black roles, without rotating the board.
// The main difference with makeCapture is that we set blockers and pins here when making a move
{
    // Save irreversible aspects of position and create a new state
    // Irreversible aspects include: castlingRights, fiftyMoveCount, zobristKey and pSquare
    std::memcpy(&new_state_info, state_info, offsetof(StateInfo, straightPinnedPieces));
    new_state_info.previous = state_info;
    state_info->next = &new_state_info;
    state_info = &new_state_info;

    // std::string fen_before{(*this).toFenString()}; // Debugging purpose
    m_blockers_set = false;
    state_info->fiftyMoveCount++;

    state_info->lastOriginSquare = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << (state_info->lastOriginSquare));
    m_last_destination_square = move.getDestinationSquare();
    state_info->lastDestinationBit = (1ULL << m_last_destination_square);
    int captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion (Used for updating check info)
    state_info->isCheck  = false;
    if (m_turn) // White's move
    {
        if ((state_info->lastOriginSquare) == 0) // Moving from a1
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->whiteQSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if ((state_info->lastOriginSquare) == 7) // Moving from h1
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->whiteKSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_destination_square == 63) // Capturing on a8
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->blackKSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_destination_square == 56) // Capturing on h8
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->blackQSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_bit == m_white_king_bit) // Moving king
        {
            m_moved_piece = 5;
            // Remove castling zobrist part
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            state_info->whiteKSCastling = false;
            state_info->whiteQSCastling = false;

            // Set new castling zobrist part
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            // Update king bit and king position
            m_white_king_bit = (state_info->lastDestinationBit);
            m_white_king_position = m_last_destination_square;

            state_info->isCheck = isDiscoverCheckForBlack((state_info->lastOriginSquare), m_last_destination_square);
            moveWhiteKingNNUEInput();
        }
        // Moving any piece except king
        else
        {
            BitPosition::setPiece(origin_bit, (state_info->lastDestinationBit));
        }
        // Captures (Non passant)
        if (((state_info->lastDestinationBit) & m_black_pawns_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_black_pawns_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 0;
        }
        else if (((state_info->lastDestinationBit) & m_black_knights_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_black_knights_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 1;
        }
        else if (((state_info->lastDestinationBit) & m_black_bishops_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_black_bishops_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 2;
        }
        else if (((state_info->lastDestinationBit) & m_black_rooks_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_black_rooks_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 3;
        }
        else if (((state_info->lastDestinationBit) & m_black_queens_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_black_queens_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 4;
        }

        // Promotions, castling and passant
        if (move.getData() & 0b0100000000000000)
        {
            state_info->fiftyMoveCount = 1;
            if (move.getData() == 16772) // White kingside castling
            {
                m_white_rooks_bit &= ~128;
                m_white_rooks_bit |= 32;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares

                (state_info->isCheck)  = isRookCheckOrDiscoverForBlack(7, 5);
            }
            else if (move.getData() == 16516) // White queenside castling
            {
                m_white_rooks_bit &= ~1;
                m_white_rooks_bit |= 8;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares

                (state_info->isCheck)  = isRookCheckOrDiscoverForBlack(0, 3);
            }
            else if (((state_info->lastDestinationBit) & EIGHT_ROW_BITBOARD) != 0) // Promotions
            {
                m_all_pieces_bit &= ~origin_bit;
                m_white_pawns_bit &= ~(state_info->lastDestinationBit);

                m_promoted_piece = move.getPromotingPiece() + 1;
                if (m_promoted_piece == 4) // Queen promotion
                {
                    m_white_queens_bit |= (state_info->lastDestinationBit);
                    (state_info->isCheck)  = isQueenCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
                }
                else if (m_promoted_piece == 3) // Rook promotion
                {
                    m_white_rooks_bit |= (state_info->lastDestinationBit);
                    (state_info->isCheck)  = isRookCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
                }
                else if (m_promoted_piece == 2) // Bishop promotion
                {
                    m_white_bishops_bit |= (state_info->lastDestinationBit);
                    (state_info->isCheck)  = isBishopCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
                }
                else // Knight promotion
                {
                    m_white_knights_bit |= (state_info->lastDestinationBit);
                    (state_info->isCheck) = isKnightCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
                }
            }
            else // Passant
            {
                m_black_pawns_bit &= ~shift_down((state_info->lastDestinationBit));
                captured_piece = 0;
                if (not (state_info->isCheck) )
                    (state_info->isCheck)  = isDiscoverCheckForBlackAfterPassant((state_info->lastOriginSquare), m_last_destination_square);
            }
        }
        state_info->zobristKey ^= zobrist_keys::passantSquaresZobristNumbers[state_info->pSquare];

        // Updating passant square
        if (m_moved_piece == 0 && (m_last_destination_square - (state_info->lastOriginSquare)) == 16)
            state_info->pSquare = (state_info->lastOriginSquare) + 8;
        else
            state_info->pSquare = 0;

        state_info->zobristKey ^= zobrist_keys::passantSquaresZobristNumbers[state_info->pSquare];
    }
    else // Black's move
    {
        if ((state_info->lastOriginSquare) == 56) // Moving from a8
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->blackQSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if ((state_info->lastOriginSquare) == 63) // Moving from h8
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->blackKSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (m_last_destination_square == 0) // Capturing on a1
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->whiteQSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        else if (m_last_destination_square == 7) // Capturing on h1
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->whiteKSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
        }

        if (origin_bit == m_black_king_bit) // Moving king
        {
            int caslting_key_index{(state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3)};
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];
            state_info->blackKSCastling = false;
            state_info->blackQSCastling = false;
            caslting_key_index = ((state_info->whiteKSCastling) | ((state_info->whiteQSCastling) << 1) | ((state_info->blackKSCastling) << 2) | ((state_info->blackQSCastling) << 3));
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[caslting_key_index];

            m_black_king_bit = (state_info->lastDestinationBit);
            m_black_king_position = m_last_destination_square;
            m_moved_piece = 5;

            state_info->isCheck  = isDiscoverCheckForWhite((state_info->lastOriginSquare), m_last_destination_square);
            moveBlackKingNNUEInput();
        }
        // Moving any piece except the king
        else
        {
            BitPosition::setPiece(origin_bit, (state_info->lastDestinationBit));
        }

        // Captures (Non passant)
        if (((state_info->lastDestinationBit) & m_white_pawns_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_white_pawns_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 0;
        }
        else if (((state_info->lastDestinationBit) & m_white_knights_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_white_knights_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 1;
        }
        else if (((state_info->lastDestinationBit) & m_white_bishops_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_white_bishops_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 2;
        }
        else if (((state_info->lastDestinationBit) & m_white_rooks_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_white_rooks_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 3;
        }
        else if (((state_info->lastDestinationBit) & m_white_queens_bit) != 0)
        {
            state_info->fiftyMoveCount = 1;
            m_white_queens_bit &= ~(state_info->lastDestinationBit);
            captured_piece = 4;
        }

        // Promotions, passant and Castling
        if (move.getData() & 0b0100000000000000)
        {
            state_info->fiftyMoveCount = 1;
            if (move.getData() == 20412) // Black kingside castling
            {
                (state_info->isCheck)  = isRookCheckOrDiscoverForWhite(63, 61);

                m_black_rooks_bit &= ~9223372036854775808ULL;
                m_black_rooks_bit |= 2305843009213693952ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
            }
            else if (move.getData() == 20156) // Black queenside castling
            {
                (state_info->isCheck)  = isRookCheckOrDiscoverForWhite(56, 59);
                
                m_black_rooks_bit &= ~72057594037927936ULL;
                m_black_rooks_bit |= 576460752303423488ULL;

                m_moved_piece = 3; // Moved rook so we need to recompute the rook attacked squares
            }
            else if (((state_info->lastDestinationBit) & FIRST_ROW_BITBOARD) != 0) // Promotions
            {
                m_all_pieces_bit &= ~origin_bit;
                m_black_pawns_bit &= ~(state_info->lastDestinationBit);

                m_promoted_piece = move.getPromotingPiece() + 1;
                if (m_promoted_piece == 4) // Queen promotion
                {
                    m_black_queens_bit |= (state_info->lastDestinationBit);
                    (state_info->isCheck)  = isQueenCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
                }
                else if (m_promoted_piece == 3) // Rook promotion
                {
                    m_black_rooks_bit |= (state_info->lastDestinationBit);
                    (state_info->isCheck)  = isRookCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
                }
                else if (m_promoted_piece == 2) // Bishop promotion
                {
                    m_black_bishops_bit |= (state_info->lastDestinationBit);
                    (state_info->isCheck)  = isBishopCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
                }
                else // Knight promotion
                {
                    m_black_knights_bit |= (state_info->lastDestinationBit);
                    (state_info->isCheck)  = isKnightCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
                }
            }
            else // Passant
            {
                // Remove captured piece
                m_white_pawns_bit &= ~shift_up((state_info->lastDestinationBit));
                captured_piece = 0;
                if (not (state_info->isCheck) )
                    (state_info->isCheck)  = isDiscoverCheckForWhiteAfterPassant((state_info->lastOriginSquare), m_last_destination_square);
            }
        }
        state_info->zobristKey ^= zobrist_keys::passantSquaresZobristNumbers[state_info->pSquare];

        // Updating passant square
        if (m_moved_piece == 0 && ((state_info->lastOriginSquare) - m_last_destination_square) == 16)
            state_info->pSquare = (state_info->lastOriginSquare) - 8;
        else
            state_info->pSquare = 0;

        state_info->zobristKey ^= zobrist_keys::passantSquaresZobristNumbers[state_info->pSquare];
    }

    BitPosition::setAllPiecesBits();
    state_info->capturedPiece = captured_piece;

    BitPosition::updateZobristKeyPiecePartAfterMove((state_info->lastOriginSquare), m_last_destination_square);
    state_info->zobristKey ^= zobrist_keys::blackToMoveZobristNumber;
    
    m_turn = not m_turn;
    // Note, we store the zobrist key in it's array when making the move. However the rest of the ply info
    // is stored when making the next move to be able to go back.
    // So we store it in the m_ply+1 position because the initial position (or position after capture) is the m_ply 0.

    m_ply++;

    m_zobrist_keys_array[127 - m_ply] = state_info->zobristKey;

    // Debugging purposes

    // bool isCheck{(state_info->isCheck) };
    // setCheckInfoOnInitialization();
    // if ((state_info->isCheck)  != isCheck)
    // {
    //     std::cout << "Fen: " << fen_before << "\n";
    //     std::cout << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }

    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // int16_t eval_1{evaluationFunction(true)};
    // initializeNNUEInput();
    // int16_t eval_2{evaluationFunction(true)};
    // if (eval_1 != eval_2)
    // {
    //     std::cout << "makeMove \n";
    //     std::cout << "Eval 1: " << eval_1 << " Eval 2: " << eval_2 << "\n";
    //     std::cout << "fen before: " << fen_before << "\n";
    //     std::cout << "fen after: " << toFenString() << "\n";
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "Moved piece : " << m_moved_piece << "\n";
    //     std::cout << "Captured piece : " << state_info->capturedPiece << "\n";
    //     std::cout << "Promoted piece : " << m_promoted_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     printBitboards();
    //     // std::exit(EXIT_FAILURE);
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

    m_zobrist_keys_array[127 - m_ply] = 0;

    m_ply--;
    int previous_captured_piece{state_info->capturedPiece};

    // Update irreversible aspects
    std::memcpy(state_info->previous->inputWhiteTurn, state_info->inputWhiteTurn, 16);
    std::memcpy(state_info->previous->inputBlackTurn, state_info->inputBlackTurn, 16);
    state_info = state_info->previous;

    // Get move info
    int origin_square{move.getOriginSquare()};
    uint64_t origin_bit{1ULL << origin_square};
    int destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{1ULL << destination_square};

    if (m_turn) // Last move was black
    {
        // Castling, Passant and promotions
        if (move.getData() & 0b0100000000000000)
        {
            // Unmake kingside castling
            if (move.getData() == 20412)
            {
                m_black_king_bit = (1ULL << 60);
                m_black_rooks_bit |= (1ULL << 63);
                m_black_rooks_bit &= ~(1ULL << 61);
                m_black_king_position = 60;
            }

            // Unmake queenside castling
            else if (move.getData() == 20156)
            {
                m_black_king_bit = (1ULL << 60);
                m_black_rooks_bit |= (1ULL << 56);
                m_black_rooks_bit &= ~(1ULL << 59);
                m_black_king_position = 60;

            }

            // Unmaking black promotions
            else if ((destination_bit & FIRST_ROW_BITBOARD) != 0)
            {
                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};

                m_black_pawns_bit |= origin_bit;

                if (promoting_piece == 12288) // Unpromote queen
                {
                    m_black_queens_bit &= ~destination_bit;
                }
                else if (promoting_piece == 8192) // Unpromote rook
                {
                    m_black_rooks_bit &= ~destination_bit;
                }
                else if (promoting_piece == 4096) // Unpromote bishop
                {
                    m_black_bishops_bit &= ~destination_bit;
                }
                else // Unpromote knight
                {
                    m_black_knights_bit &= ~destination_bit;
                }
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
            else // Passant
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
                m_white_pawns_bit |= shift_up(destination_bit);
            }
        }

        // Non special moves
        else
        {
            if (destination_bit & m_black_pawns_bit) // Unmove pawn
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
            }
            else if (destination_bit & m_black_knights_bit) // Unmove knight
            {
                m_black_knights_bit |= origin_bit;
                m_black_knights_bit &= ~destination_bit;
            }
            else if (destination_bit & m_black_bishops_bit) // Unmove bishop
            {
                m_black_bishops_bit |= origin_bit;
                m_black_bishops_bit &= ~destination_bit;
            }
            else if (destination_bit & m_black_rooks_bit) // Unmove rook
            {
                m_black_rooks_bit |= origin_bit;
                m_black_rooks_bit &= ~destination_bit;
            }
            else if (destination_bit & m_black_queens_bit) // Unmove queen
            {
                m_black_queens_bit |= origin_bit;
                m_black_queens_bit &= ~destination_bit;
            }
            else // Unmove king
            {
                m_black_king_bit = origin_bit;
                m_black_king_position = origin_square;
                moveBlackKingNNUEInput();
            }
            // Unmaking captures
            if (previous_captured_piece != 7)
            {
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
    }
    else // Last move was white
    {
        // Special moves
        if (move.getData() & 0b0100000000000000)
        {
            // Unmake kingside castling
            if (move.getData() == 16772)
            {
                m_white_king_bit = (1ULL << 4);
                m_white_rooks_bit |= (1ULL << 7);
                m_white_rooks_bit &= ~(1ULL << 5);
                m_white_king_position = 4;
            }

            // Unmake queenside castling
            else if (move.getData() == 16516)
            {
                m_white_king_bit = (1ULL << 4);
                m_white_rooks_bit |= 1ULL;
                m_white_rooks_bit &= ~(1ULL << 3);
                m_white_king_position = 4;
            }

            // Unmaking promotions
            else if ((destination_bit & EIGHT_ROW_BITBOARD) != 0)
            {
                uint16_t promoting_piece{static_cast<uint16_t>(move.getData() & 12288)};

                m_white_pawns_bit |= origin_bit;

                if (promoting_piece == 12288) // Unpromote queen
                {
                    m_white_queens_bit &= ~destination_bit;
                }
                else if (promoting_piece == 8192) // Unpromote rook
                {
                    m_white_rooks_bit &= ~destination_bit;
                }
                else if (promoting_piece == 4096) // Unpromote bishop
                {
                    m_white_bishops_bit &= ~destination_bit;
                }
                else // Unpromote knight
                {
                    m_white_knights_bit &= ~destination_bit;
                }
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
            else // Passant
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;

                m_black_pawns_bit |= shift_down(destination_bit);
            }
        }

        // Non special moves
        else
        {
            if (destination_bit & m_white_pawns_bit) // Unmove pawn
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;
            }
            else if (destination_bit & m_white_knights_bit) // Unmove knight
            {
                m_white_knights_bit |= origin_bit;
                m_white_knights_bit &= ~destination_bit;
            }
            else if (destination_bit & m_white_bishops_bit) // Unmove bishop
            {
                m_white_bishops_bit |= origin_bit;
                m_white_bishops_bit &= ~destination_bit;
            }
            else if (destination_bit & m_white_rooks_bit) // Unmove rook
            {
                m_white_rooks_bit |= origin_bit;
                m_white_rooks_bit &= ~destination_bit;
            }
            else if (destination_bit & m_white_queens_bit) // Unmove queen
            {
                m_white_queens_bit |= origin_bit;
                m_white_queens_bit &= ~destination_bit;
            }
            else // Unmove king
            {
                m_white_king_bit = origin_bit;
                m_white_king_position = origin_square;
                moveWhiteKingNNUEInput();
            }

            // Unmaking captures
            if (previous_captured_piece != 7)
            {
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
    }
    BitPosition::setAllPiecesBits();
    m_turn = not m_turn;

    // Debugging purposes
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
void BitPosition::makeCapture(T move, StateInfo& new_state_info)
// Move piece and switch white and black roles, without rotating the board.
// The main difference with makeCapture is that we dont set blockers and pins here, we set them in the moveSelector class if a pseudo_legal move is found
{
    // Save irreversible aspects of position and create a new state
    // Irreversible aspects include: castlingRights, fiftyMoveCount, zobristKey and pSquare
    std::memcpy(&new_state_info, state_info, offsetof(StateInfo, straightPinnedPieces));
    new_state_info.previous = state_info;
    state_info->next = &new_state_info;
    state_info = &new_state_info;

    // std::string fen_before{(*this).toFenString()}; // Debugging purposes
    m_blockers_set = false;

    (state_info->lastOriginSquare) = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << (state_info->lastOriginSquare));
    m_last_destination_square = move.getDestinationSquare();
    (state_info->lastDestinationBit) = (1ULL << m_last_destination_square);
    int captured_piece = 7; // Representing no capture
    m_promoted_piece = 7; // Representing no promotion (Used for updating check info)
    state_info->pSquare = 0;
    state_info->isCheck  = false;

    if (m_turn) // White's move
    {
        // Promotions
        if (move.getData() & 0b0100000000000000)
        {
            m_moved_piece = 0;
            m_promoted_piece = 4;

            m_white_pawns_bit &= ~origin_bit;
            m_all_pieces_bit &= ~origin_bit;
            m_white_queens_bit |= (state_info->lastDestinationBit);
            state_info->isCheck = isQueenCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
            // Set NNUE input
            addOnInput(64 * 4 + m_last_destination_square);
            removeOnInput(state_info->lastOriginSquare);

            // Captures (Non passant)
            if (((state_info->lastDestinationBit) & m_black_pawns_bit) != 0)
            {
                m_black_pawns_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 0;
                // Set NNUE input
                removeOnInput(64 * 5 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_knights_bit) != 0)
            {
                m_black_knights_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 1;
                // Set NNUE input
                removeOnInput(64 * 6 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_bishops_bit) != 0)
            {
                m_black_bishops_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 2;
                // Set NNUE input
                removeOnInput(64 * 7 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_rooks_bit) != 0)
            {
                m_black_rooks_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 3;
                // Set NNUE input
                removeOnInput(64 * 8 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_queens_bit) != 0)
            {
                m_black_queens_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 4;
                // Set NNUE input
                removeOnInput(64 * 9 + m_last_destination_square);
            }
        }
        else
        {
            if (origin_bit == m_white_king_bit) // Moving king
            {
                // Update king bit and king position
                m_white_king_bit = (state_info->lastDestinationBit);
                m_white_king_position = m_last_destination_square;
                m_moved_piece = 5;

                // Update NNUE input (after moving the king)
                moveWhiteKingNNUEInput();
                state_info->isCheck = isDiscoverCheckForBlack((state_info->lastOriginSquare), m_last_destination_square);
            }
            // Moving any piece except king
            else
            {
                BitPosition::setPiece(origin_bit, (state_info->lastDestinationBit));
                // Set NNUE input
                addOnInput(64 * m_moved_piece + m_last_destination_square);
                removeOnInput(64 * m_moved_piece + state_info->lastOriginSquare);
            }
            // Captures (Non passant)
            if (((state_info->lastDestinationBit) & m_black_pawns_bit) != 0)
            {
                m_black_pawns_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 0;
                // Set NNUE input
                removeOnInput(64 * 5 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_knights_bit) != 0)
            {
                m_black_knights_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 1;
                // Set NNUE input
                removeOnInput(64 * 6 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_bishops_bit) != 0)
            {
                m_black_bishops_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 2;
                // Set NNUE input
                removeOnInput(64 * 7 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_rooks_bit) != 0)
            {
                m_black_rooks_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 3;
                // Set NNUE input
                removeOnInput(64 * 8 + m_last_destination_square);
            }
            else
            {
                m_black_queens_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 4;
                // Set NNUE input
                removeOnInput(64 * 9 + m_last_destination_square);
            }
        }
    }
    else // Black's move
    {
        // Promotions
        if (move.getData() & 0b0100000000000000)
        {
            m_moved_piece = 0;
            m_promoted_piece = 4;
            
            m_black_pawns_bit &= ~origin_bit;
            m_black_queens_bit |= (state_info->lastDestinationBit);
            m_all_pieces_bit &= ~origin_bit;
            (state_info->isCheck)  = isQueenCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
            // Set NNUE input
            addOnInput(64 * 9 + m_last_destination_square);
            removeOnInput(64 * 5 + state_info->lastOriginSquare);

            // Captures (Non passant)
            if (((state_info->lastDestinationBit) & m_white_pawns_bit) != 0)
            {
                m_white_pawns_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 0;
                // Set NNUE input
                removeOnInput(m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_knights_bit) != 0)
            {
                m_white_knights_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 1;
                // Set NNUE input
                removeOnInput(64 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_bishops_bit) != 0)
            {
                m_white_bishops_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 2;
                // Set NNUE input
                removeOnInput(64 * 2 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_rooks_bit) != 0)
            {
                m_white_rooks_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 3;
                // Set NNUE input
                removeOnInput(64 * 3 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_queens_bit) != 0)
            {
                m_white_queens_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 4;
                // Set NNUE input
                removeOnInput(64 * 4 + m_last_destination_square);
            }
        }
        // Non promotions
        else
        {
            if (origin_bit == m_black_king_bit) // Moving king
            {
                m_black_king_bit = (state_info->lastDestinationBit);
                m_black_king_position = m_last_destination_square;
                m_moved_piece = 5;
                (state_info->isCheck) = isDiscoverCheckForWhite((state_info->lastOriginSquare), m_last_destination_square);

                // Update NNUE input (after moving the king)
                moveBlackKingNNUEInput();
            }
            // Moving any piece except the king
            else
            {
                BitPosition::setPiece(origin_bit, (state_info->lastDestinationBit));
                // Set NNUE input
                addOnInput(64 * (5 + m_moved_piece) + m_last_destination_square);
                removeOnInput(64 *(5+ m_moved_piece) + state_info->lastOriginSquare);
            }

            // Captures (Non passant)
            if (((state_info->lastDestinationBit) & m_white_pawns_bit) != 0)
            {
                m_white_pawns_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 0;
                // Set NNUE input
                removeOnInput(m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_knights_bit) != 0)
            {
                m_white_knights_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 1;
                // Set NNUE input
                removeOnInput(64 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_bishops_bit) != 0)
            {
                m_white_bishops_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 2;
                // Set NNUE input
                removeOnInput(64 * 2 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_rooks_bit) != 0)
            {
                m_white_rooks_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 3;
                // Set NNUE input
                removeOnInput(64 * 3 + m_last_destination_square);
            }
            else
            {
                m_white_queens_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 4;
                // Set NNUE input
                removeOnInput(64 * 4 + m_last_destination_square);
            }
        }
    }
    BitPosition::setAllPiecesBits();

    m_turn = not m_turn;
    state_info->capturedPiece = captured_piece;
    m_ply++;

    // Debugging purposes

    // bool isCheck{(state_info->isCheck) };
    // setCheckInfoOnInitialization();
    // if ((state_info->isCheck)  != isCheck)
    // {
    //     std::cout << "Fen: " << fen_before << "\n";
    //     std::cout << move.toString() << "\n";
    //     std::exit(EXIT_FAILURE);
    // }

    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // int16_t eval_1{evaluationFunction(true)};
    // initializeNNUEInput();
    // int16_t eval_2{evaluationFunction(true)};
    // if (eval_1 != eval_2)
    // {
    //     std::cout << "makeCapture \n";
    //     std::cout << "Eval 1: " << eval_1 << " Eval 2: " << eval_2 << "\n";
    //     std::cout << "fen before: " << fen_before << "\n";
    //     std::cout << "fen after: " << toFenString() << "\n";
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "Moved piece : " << m_moved_piece << "\n";
    //     std::cout << "Captured piece : " << state_info->capturedPiece << "\n";
    //     std::cout << "Promoted piece : " << m_promoted_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     printBitboards();
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

    int previous_captured_piece{state_info->capturedPiece};

    // Update irreversible aspects
    state_info = state_info->previous;

    // Get irreversible info
    // Get move info
    int origin_square{move.getOriginSquare()};
    uint64_t origin_bit{1ULL << origin_square};
    int destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{1ULL << destination_square};

    if (m_turn) // Last move was black
    {
        // Promotions
        if (move.getData() & 0b0100000000000000)
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
            if (destination_bit & m_black_pawns_bit) // Unmove pawn
            {
                m_black_pawns_bit |= origin_bit;
                m_black_pawns_bit &= ~destination_bit;
            }
            else if (destination_bit & m_black_knights_bit) // Unmove knight
            {
                m_black_knights_bit |= origin_bit;
                m_black_knights_bit &= ~destination_bit;
            }
            else if (destination_bit & m_black_bishops_bit) // Unmove bishop
            {
                m_black_bishops_bit |= origin_bit;
                m_black_bishops_bit &= ~destination_bit;
            }
            else if (destination_bit & m_black_rooks_bit) // Unmove rook
            {
                m_black_rooks_bit |= origin_bit;
                m_black_rooks_bit &= ~destination_bit;
            }
            else if (destination_bit & m_black_queens_bit) // Unmove queen
            {
                m_black_queens_bit |= origin_bit;
                m_black_queens_bit &= ~destination_bit;
            }
            else // Unmove king
            {
                m_black_king_bit = origin_bit;
                m_black_king_position = origin_square;
                moveBlackKingNNUEInput();
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
        if (move.getData() & 0b0100000000000000)
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
            if (destination_bit & m_white_pawns_bit) // Unmove pawn
            {
                m_white_pawns_bit |= origin_bit;
                m_white_pawns_bit &= ~destination_bit;
            }
            else if (destination_bit & m_white_knights_bit) // Unmove knight
            {
                m_white_knights_bit |= origin_bit;
                m_white_knights_bit &= ~destination_bit;
            }
            else if (destination_bit & m_white_bishops_bit) // Unmove bishop
            {
                m_white_bishops_bit |= origin_bit;
                m_white_bishops_bit &= ~destination_bit;
            }
            else if (destination_bit & m_white_rooks_bit) // Unmove rook
            {
                m_white_rooks_bit |= origin_bit;
                m_white_rooks_bit &= ~destination_bit;
            }
            else if (destination_bit & m_white_queens_bit) // Unmove queen
            {
                m_white_queens_bit |= origin_bit;
                m_white_queens_bit &= ~destination_bit;
            }
            else // Unmove king
            {
                m_white_king_bit = origin_bit;
                m_white_king_position = origin_square;
                moveWhiteKingNNUEInput();
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

    // Debugging purposes
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
bool BitPosition::isMate() const
// This is called only in quiesence search, after having no available captures
{
    if (m_turn) // White's turn
    {
        // Only consider empty squares because we assume there are no captures
        if (m_num_checks == 1) // Can we block with pieces?
        {
            // Knight block
            uint64_t piece_moves = m_white_knights_bit & ~((state_info->pinnedPieces) );
            while (piece_moves)
            {
                if ((precomputed_moves::knight_moves[popLeastSignificantBit(piece_moves)] & m_check_rays) != 0)
                    return false;
            }
            // Single move pawn block
            uint64_t pawnAdvances{shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
            piece_moves = pawnAdvances & m_check_rays;
            while (piece_moves)
            {
                int destination{popLeastSignificantBit(piece_moves)};
                if (isNormalMoveLegal(destination - 8, destination))
                    return false;
            }
            // Double move pawn block
            piece_moves = shift_up(pawnAdvances) & m_check_rays;
            while (piece_moves)
            {
                int destination{popLeastSignificantBit(piece_moves)};
                if (isNormalMoveLegal(destination - 16, destination))
                    return false;
            }
            // Rook block
            piece_moves = m_white_rooks_bit & ~(state_info->diagonalPinnedPieces) ;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                uint64_t piece_destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
                while (piece_destinations)
                {
                    if (isNormalMoveLegal(origin, popLeastSignificantBit(piece_destinations)))
                        return false;
                }
            }
            // Bishop block
            piece_moves = m_white_bishops_bit & ~(state_info->straightPinnedPieces) ;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                uint64_t piece_destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
                while (piece_destinations)
                {
                    if (isNormalMoveLegal(origin, popLeastSignificantBit(piece_destinations)))
                        return false;
                }
            }
            // Queen block
            piece_moves = m_white_queens_bit;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                uint64_t piece_destinations{(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays};
                while (piece_destinations)
                {
                    if (isNormalMoveLegal(origin, popLeastSignificantBit(piece_destinations)))
                        return false;
                }
            }
        }
        //  Can we move king?
        uint64_t piece_moves{precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit};
        while (piece_moves)
        {
            if (newWhiteKingSquareIsSafe(popLeastSignificantBit(piece_moves)))
                return false;
        }
    }
    else // Black's turn
    {
        // Only consider empty squares because we assume there are no captures
        if (m_num_checks == 1) // Can we block with pieces?
        {
            // Knight block
            uint64_t piece_moves = m_black_knights_bit & ~((state_info->pinnedPieces) );
            while (piece_moves)
            {
                if ((precomputed_moves::knight_moves[popLeastSignificantBit(piece_moves)] & m_check_rays) != 0)
                    return false;
            }
            // Single move pawn block
            uint64_t pawnAdvances{shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
            piece_moves = pawnAdvances & m_check_rays;
            while (piece_moves)
            {
                int destination{popLeastSignificantBit(piece_moves)};
                if (isNormalMoveLegal(destination + 8, destination))
                    return false;
            }
            // Double move pawn block
            piece_moves = shift_down(pawnAdvances) & m_check_rays;
            while (piece_moves)
            {
                int destination{popLeastSignificantBit(piece_moves)};
                if (isNormalMoveLegal(destination + 16, destination))
                    return false;
            }
            // Rook block
            piece_moves = m_black_rooks_bit & ~(state_info->diagonalPinnedPieces) ;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                uint64_t piece_destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
                while (piece_destinations)
                {
                    if (isNormalMoveLegal(origin, popLeastSignificantBit(piece_destinations)))
                        return false;
                }
            }
            // Bishop block
            piece_moves = m_black_bishops_bit & ~(state_info->straightPinnedPieces) ;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                uint64_t piece_destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
                while (piece_destinations)
                {
                    if (isNormalMoveLegal(origin, popLeastSignificantBit(piece_destinations)))
                        return false;
                }
            }
            // Queen block
            piece_moves = m_black_queens_bit;
            while (piece_moves)
            {
                int origin{popLeastSignificantBit(piece_moves)};
                uint64_t piece_destinations{(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) | BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays};
                while (piece_destinations)
                {
                    if (isNormalMoveLegal(origin, popLeastSignificantBit(piece_destinations)))
                        return false;
                }
            }
        }
        //  Can we move king?
        uint64_t piece_moves{precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit};
        while (piece_moves)
        {
            if (newBlackKingSquareIsSafe(popLeastSignificantBit(piece_moves)))
                return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The Folowing member functions will not be used in the engine. They are only used for debugging purposes.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Move *BitPosition::inCheckPawnBlocksNonQueenProms(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
        for (int destination : getBitIndices(single_pawn_moves_blocking_bit))
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
        for (int destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
        {
            *move_list++ = Move(destination - 16, destination);
        }
        // Passant block or capture
        if ((state_info->pSquare) != 0)
        {
            for (int origin : getBitIndices(precomputed_moves::black_pawn_attacks[(state_info->pSquare)] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    else
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
        for (int destination : getBitIndices(single_pawn_moves_blocking_bit))
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
        for (int destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit & m_check_rays))
        {
            *move_list++ = Move(destination + 16, destination);
        }
        // Passant block or capture
        if ((state_info->pSquare) != 0)
        {
            for (int origin : getBitIndices(precomputed_moves::white_pawn_attacks[(state_info->pSquare)] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    return move_list;
}
Move *BitPosition::inCheckPawnCapturesNonQueenProms(Move *&move_list) const
{
    if (m_turn)
    {
        // Pawn captures from checking position
        for (int origin : getBitIndices(precomputed_moves::black_pawn_attacks[m_check_square] & m_white_pawns_bit))
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
        for (int origin : getBitIndices(precomputed_moves::white_pawn_attacks[m_check_square] & m_black_pawns_bit))
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
    return move_list;
}
Move *BitPosition::inCheckPassantCaptures(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays == 0
{
    if (m_turn)
    {
        // Passant block or capture
        if ((state_info->pSquare) != 0)
        {
            for (int origin : getBitIndices(precomputed_moves::black_pawn_attacks[(state_info->pSquare)] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    else
    {
        // Passant block or capture
        if ((state_info->pSquare) != 0)
        {
            for (int origin : getBitIndices(precomputed_moves::white_pawn_attacks[(state_info->pSquare)] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    return move_list;
}

Move *BitPosition::pawnNonCapturesNonQueenProms(Move *&move_list) const
{
    if (m_turn)
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_up(m_white_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        for (int destination : getBitIndices(single_pawn_moves_bit))
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
        for (int destination : getBitIndices(shift_up(single_pawn_moves_bit & THIRD_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            *move_list++ = Move(destination - 16, destination);
        }
        // Right shift captures and non-queen promotions
        for (int destination : getBitIndices(shift_up_right(m_white_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 9, destination, 0);
            *move_list++ = Move(destination - 9, destination, 1);
            *move_list++ = Move(destination - 9, destination, 2);
        }
        // Left shift captures and non-queen promotions
        for (int destination : getBitIndices(shift_up_left(m_white_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_black_pieces_bit & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 7, destination, 0);
            *move_list++ = Move(destination - 7, destination, 1);
            *move_list++ = Move(destination - 7, destination, 2);
        }
        // Passant
        if ((state_info->pSquare) != 0)
        {
            for (int origin : getBitIndices(precomputed_moves::black_pawn_attacks[(state_info->pSquare)] & m_white_pawns_bit))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    else // Black moves
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_black_pawns_bit & ~(state_info->diagonalPinnedPieces) ) & ~m_all_pieces_bit};
        for (int destination : getBitIndices(single_pawn_moves_bit))
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
        for (int destination : getBitIndices(shift_down(single_pawn_moves_bit & SIXTH_ROW_BITBOARD) & ~m_all_pieces_bit))
        {
            *move_list++ = Move(destination + 16, destination);
        }
        // Right shift captures and non-queen promotions
        for (int destination : getBitIndices(shift_down_right(m_black_pawns_bit & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_white_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 7, destination, 0);
            *move_list++ = Move(destination + 7, destination, 1);
            *move_list++ = Move(destination + 7, destination, 2);
        }
        // Left shift captures and non-queen promotions
        for (int destination : getBitIndices(shift_down_left(m_black_pawns_bit & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces) ) & m_white_pieces_bit & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 9, destination, 0);
            *move_list++ = Move(destination + 9, destination, 1);
            *move_list++ = Move(destination + 9, destination, 2);
        }
        // Passant
        if ((state_info->pSquare) != 0)
        {
            for (int origin : getBitIndices(precomputed_moves::white_pawn_attacks[(state_info->pSquare)] & m_black_pawns_bit))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8))
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    return move_list;
}
Move *BitPosition::knightNonCaptures(Move*& move_list) const
{
    if (m_turn)
    {
        for (int origin : getBitIndices(m_white_knights_bit & ~((state_info->pinnedPieces) )))
        {
            for (int destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_all_pieces_bit))
            {
                    *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (int origin : getBitIndices(m_black_knights_bit & ~((state_info->pinnedPieces) )))
        {
            for (int destination : getBitIndices(precomputed_moves::knight_moves[origin] & ~m_all_pieces_bit))
            {
                    *move_list++ = Move(origin, destination);
            }
        }
    }
    return move_list;
}
Move *BitPosition::bishopNonCaptures(Move *&move_list) const
{
    if (m_turn)
    {
        for (int origin : getBitIndices(m_white_bishops_bit & ~(state_info->straightPinnedPieces) ))
        {
            for (int destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (int origin : getBitIndices(m_black_bishops_bit & ~(state_info->straightPinnedPieces) ))
        {
            for (int destination : getBitIndices(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    return move_list;
}
Move *BitPosition::rookNonCaptures(Move*& move_list) const
{
    if (m_turn)
    {
        for (int origin : getBitIndices(m_white_rooks_bit & ~(state_info->diagonalPinnedPieces) ))
        {
            for (int destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (int origin : getBitIndices(m_black_rooks_bit & ~(state_info->diagonalPinnedPieces) ))
        {
            for (int destination : getBitIndices(RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    return move_list;
}
Move *BitPosition::queenNonCaptures(Move *&move_list) const
{
    if (m_turn)
    {
        for (int origin : getBitIndices(m_white_queens_bit))
        {
            for (int destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_all_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    else
    {
        for (int origin : getBitIndices(m_black_queens_bit))
        {
            for (int destination : getBitIndices((BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_all_pieces_bit))
            {
                *move_list++ = Move(origin, destination);
            }
        }
    }
    return move_list;
}
Move *BitPosition::kingNonCaptures(Move *&move_list) const
{
    if (m_turn)
    {
        for (int destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit))
        {
            *move_list++ = Move(m_white_king_position, destination);
        }
        // Kingside castling
        if ((state_info->whiteKSCastling) && (m_all_pieces_bit & 96) == 0)
            *move_list++ = castling_moves[0];
        // Queenside castling
        if ((state_info->whiteQSCastling) && (m_all_pieces_bit & 14) == 0)
            *move_list++ = castling_moves[1];
    }
    else
    {
        for (int destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit))
        {
            *move_list++ = Move(m_black_king_position, destination);
        }
        // Kingside castling
        if ((state_info->blackKSCastling) && (m_all_pieces_bit & 6917529027641081856) == 0)
            *move_list++ = castling_moves[2];
        // Queenside castling
        if ((state_info->blackQSCastling) && (m_all_pieces_bit & 1008806316530991104) == 0)
            *move_list++ = castling_moves[3];
    }
    return move_list;
}

Move *BitPosition::kingNonCapturesInCheck(Move *&move_list) const
{
    if (m_turn)
    {
        for (int destination : getBitIndices(precomputed_moves::king_moves[m_white_king_position] & ~m_all_pieces_bit))
        {
            *move_list++ = Move(m_white_king_position, destination);
        }
    }
    else
    {
        for (int destination : getBitIndices(precomputed_moves::king_moves[m_black_king_position] & ~m_all_pieces_bit))
        {
            *move_list++ = Move(m_black_king_position, destination);
        }
    }
    return move_list;
}

template <typename T>
void BitPosition::makeCaptureTest(T move, StateInfo &new_state_info)
// Compared to moveCapture this function updates castling rights too, for perft tests.
{
    // Save irreversible aspects of position and create a new state
    // Irreversible aspects include: castlingRights, fiftyMoveCount, zobristKey and pSquare
    std::memcpy(&new_state_info, state_info, offsetof(StateInfo, straightPinnedPieces));
    new_state_info.previous = state_info;
    state_info->next = &new_state_info;
    state_info = &new_state_info;

    // std::string fen_before{(*this).toFenString()}; // Debugging purposes
    m_blockers_set = false;

    (state_info->lastOriginSquare) = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << (state_info->lastOriginSquare));
    m_last_destination_square = move.getDestinationSquare();
    (state_info->lastDestinationBit) = (1ULL << m_last_destination_square);
    int captured_piece = 7; // Representing no capture
    m_promoted_piece = 7;   // Representing no promotion (Used for updating check info)
    state_info->pSquare = 0;
    state_info->isCheck = false;

    if (m_turn) // White's move
    {
        if ((state_info->lastOriginSquare) == 0) // Moving from a1
        {
            state_info->whiteQSCastling = false;
        }

        else if ((state_info->lastOriginSquare) == 7) // Moving from h1
        {
            state_info->whiteKSCastling = false;
        }

        if (m_last_destination_square == 63) // Capturing on a8
        {
            state_info->blackKSCastling = false;
        }

        else if (m_last_destination_square == 56) // Capturing on h8
        {
            state_info->blackQSCastling = false;
        }
        if (origin_bit == m_white_king_bit) // Moving king
        {
            state_info->whiteKSCastling = false;
            state_info->whiteQSCastling = false;
        }
        // Promotions
        if (move.getData() & 0b0100000000000000)
        {
            m_moved_piece = 0;
            m_promoted_piece = 4;

            m_white_pawns_bit &= ~origin_bit;
            m_all_pieces_bit &= ~origin_bit;
            m_white_queens_bit |= (state_info->lastDestinationBit);
            state_info->isCheck = isQueenCheckOrDiscoverForBlack((state_info->lastOriginSquare), m_last_destination_square);
            // Set NNUE input
            removeOnInput((state_info->lastOriginSquare));
            addOnInput(64 * 4 + m_last_destination_square);

            // Captures (Non passant)
            if (((state_info->lastDestinationBit) & m_black_pawns_bit) != 0)
            {
                m_black_pawns_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 0;
                // Set NNUE input
                removeOnInput(64 * 5 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_knights_bit) != 0)
            {
                m_black_knights_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 1;
                // Set NNUE input
                removeOnInput(64 * 6 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_bishops_bit) != 0)
            {
                m_black_bishops_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 2;
                // Set NNUE input
                removeOnInput(64 * 7 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_rooks_bit) != 0)
            {
                m_black_rooks_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 3;
                // Set NNUE input
                removeOnInput(64 * 8 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_queens_bit) != 0)
            {
                m_black_queens_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 4;
                // Set NNUE input
                removeOnInput(64 * 9 + m_last_destination_square);
            }
        }
        else
        {
            if (origin_bit == m_white_king_bit) // Moving king
            {
                // Update king bit and king position
                m_white_king_bit = (state_info->lastDestinationBit);
                m_white_king_position = m_last_destination_square;
                m_moved_piece = 5;

                // Update NNUE input (after moving the king)
                moveWhiteKingNNUEInput();
                state_info->isCheck = isDiscoverCheckForBlack((state_info->lastOriginSquare), m_last_destination_square);
            }
            // Moving any piece except king
            else
            {
                BitPosition::setPiece(origin_bit, (state_info->lastDestinationBit));
                // Set NNUE input
                removeOnInput(64 * m_moved_piece + (state_info->lastOriginSquare));
                addOnInput(64 * m_moved_piece + m_last_destination_square);
            }
            // Captures (Non passant)
            if (((state_info->lastDestinationBit) & m_black_pawns_bit) != 0)
            {
                m_black_pawns_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 0;
                // Set NNUE input
                removeOnInput(64 * 5 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_knights_bit) != 0)
            {
                m_black_knights_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 1;
                // Set NNUE input
                removeOnInput(64 * 6 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_bishops_bit) != 0)
            {
                m_black_bishops_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 2;
                // Set NNUE input
                removeOnInput(64 * 7 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_black_rooks_bit) != 0)
            {
                m_black_rooks_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 3;
                // Set NNUE input
                removeOnInput(64 * 8 + m_last_destination_square);
            }
            else
            {
                m_black_queens_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 4;
                // Set NNUE input
                removeOnInput(64 * 9 + m_last_destination_square);
            }
        }
    }
    else // Black's move
    {
        if ((state_info->lastOriginSquare) == 56) // Moving from a8
        {
            state_info->blackQSCastling = false;
        }

        else if ((state_info->lastOriginSquare) == 63) // Moving from h8
        {
            state_info->blackKSCastling = false;
        }

        if (m_last_destination_square == 0) // Capturing on a1
        {
            state_info->whiteQSCastling = false;
        }

        else if (m_last_destination_square == 7) // Capturing on h1
        {
            state_info->whiteKSCastling = false;
        }
        if (origin_bit == m_black_king_bit) // Moving king
        {
            state_info->blackKSCastling = false;
            state_info->blackQSCastling = false;
        }
        // Promotions
        if (move.getData() & 0b0100000000000000)
        {
            m_moved_piece = 0;
            m_promoted_piece = 4;

            m_black_pawns_bit &= ~origin_bit;
            m_black_queens_bit |= (state_info->lastDestinationBit);
            m_all_pieces_bit &= ~origin_bit;
            (state_info->isCheck) = isQueenCheckOrDiscoverForWhite((state_info->lastOriginSquare), m_last_destination_square);
            // Set NNUE input
            removeOnInput(64 * 5 + (state_info->lastOriginSquare));
            addOnInput(64 * 9 + m_last_destination_square);

            // Captures (Non passant)
            if (((state_info->lastDestinationBit) & m_white_pawns_bit) != 0)
            {
                m_white_pawns_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 0;
                // Set NNUE input
                removeOnInput(m_last_destination_square);
            }
            
            else if (((state_info->lastDestinationBit) & m_white_knights_bit) != 0)
            {
                m_white_knights_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 1;
                // Set NNUE input
                removeOnInput(64 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_bishops_bit) != 0)
            {
                m_white_bishops_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 2;
                // Set NNUE input
                removeOnInput(64 * 2 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_rooks_bit) != 0)
            {
                m_white_rooks_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 3;
                // Set NNUE input
                removeOnInput(64 * 3 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_queens_bit) != 0)
            {
                m_white_queens_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 4;
                // Set NNUE input
                removeOnInput(64 * 4 + m_last_destination_square);
            }
        }
        // Non promotions
        else
        {
            if (origin_bit == m_black_king_bit) // Moving king
            {
                m_black_king_bit = (state_info->lastDestinationBit);
                m_black_king_position = m_last_destination_square;
                m_moved_piece = 5;
                (state_info->isCheck) = isDiscoverCheckForWhite((state_info->lastOriginSquare), m_last_destination_square);

                // Update NNUE input (after moving the king)
                moveBlackKingNNUEInput();
            }
            // Moving any piece except the king
            else
            {
                BitPosition::setPiece(origin_bit, (state_info->lastDestinationBit));
                // Set NNUE input
                removeOnInput(64 * (5 + m_moved_piece) + (state_info->lastOriginSquare));
                addOnInput(64 * (5 + m_moved_piece) + m_last_destination_square);
            }

            // Captures (Non passant)
            if (((state_info->lastDestinationBit) & m_white_pawns_bit) != 0)
            {
                m_white_pawns_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 0;
                // Set NNUE input
                removeOnInput(m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_knights_bit) != 0)
            {
                m_white_knights_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 1;
                // Set NNUE input
                removeOnInput(64 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_bishops_bit) != 0)
            {
                m_white_bishops_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 2;
                // Set NNUE input
                removeOnInput(64 * 2 + m_last_destination_square);
            }
            else if (((state_info->lastDestinationBit) & m_white_rooks_bit) != 0)
            {
                m_white_rooks_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 3;
                // Set NNUE input
                removeOnInput(64 * 3 + m_last_destination_square);
            }
            else
            {
                m_white_queens_bit &= ~(state_info->lastDestinationBit);
                captured_piece = 4;
                // Set NNUE input
                removeOnInput(64 * 4 + m_last_destination_square);
            }
        }
    }
    BitPosition::setAllPiecesBits();

    m_turn = not m_turn;
    state_info->capturedPiece = captured_piece;
    m_ply++;

    // Debugging purposes
    // bool special{(move.getData() & 0b0100000000000000) == 0b0100000000000000};
    // if (true)
    // {
    //     std::cout << "makeMove \n";
    //     // std::cout << "fen before: " << fen_before << "\n";
    //     // std::cout << "fen after: " << (*this).toFenString() << "\n";
    //     std::cout << "Special move : " << special << "\n";
    //     std::cout << "Moved piece : " << m_moved_piece << "\n";
    //     std::cout << "Captured piece : " << captured_piece << "\n";
    //     std::cout << "Promoted piece : " << m_promoted_piece << "\n";
    //     std::cout << "Move : " << move.toString() << "\n";
    //     (*this).printBitboards();
    //     // std::exit(EXIT_FAILURE);
    // }
}