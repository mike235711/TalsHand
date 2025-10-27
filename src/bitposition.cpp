#include <iostream>
#include <vector>
#include <cstdint>             // For fixed sized integers
#include <cassert>

#include "precomputed_moves.h" // Include the precomputed move constants
#include "bitposition.h"       // Where the BitPosition class is defined
#include "bit_utils.h"         // Bit utility functions
#include "move.h"
#include "magicmoves.h"
#include "zobrist_keys.h"
#include "accumulation.h" // Utility functions to update NNUE Input

//  SEE support: piece values and fast attack generation
bool BitPosition::see_ge(Move m, int threshold) const
{
    // Early exit for non‑standard moves (promotion, castling, en‑passant):
    if ((m.getData() & 0xC000) != 0)
        return 100 /*Pawn value*/ >= threshold;

    const int from = m.getOriginSquare();
    const int to = m.getDestinationSquare();

    // Some local helpers
    constexpr int VAL[6] = {100, 320, 330, 500, 900, 20000}; // P N B R Q K

    // All direct attackers on OCC up‑to‑date occupancy
    auto attackers_to = [&](int sq, uint64_t occ) -> uint64_t
    {
        return (precomputed_moves::pawn_attacks[1][sq] & m_pieces[0][0]) | // white pawns
               (precomputed_moves::pawn_attacks[0][sq] & m_pieces[1][0]) | // black pawns
               (precomputed_moves::knight_moves[sq] & (m_pieces[0][1] | m_pieces[1][1])) |
               (BmagicNOMASK(sq, precomputed_moves::bishop_unfull_rays[sq] & occ) &
                (m_pieces[0][2] | m_pieces[1][2] | m_pieces[0][4] | m_pieces[1][4])) |
               (RmagicNOMASK(sq, precomputed_moves::rook_unfull_rays[sq] & occ) &
                (m_pieces[0][3] | m_pieces[1][3] | m_pieces[0][4] | m_pieces[1][4])) |
               (precomputed_moves::king_moves[sq] & (m_pieces[0][5] | m_pieces[1][5]));
    };

    // Pop least‑valuable attacker of colour c (0 = white, 1 = black)
    auto pop_lva = [&](uint64_t bb, int c) -> std::pair<int, int>
    {
        for (int pt = 0; pt < 6; ++pt)
        { // P → K order
            uint64_t subset = bb & m_pieces[c][pt];
            if (subset)
            {
                int sq = popLeastSignificantBit(subset);
                return {sq, pt};
            }
        }
        return {64, 5}; // dummy (no square, king type)
    };

    auto piece_on = [&](int sq) -> int { // returns pt 0..5
        int pt = m_board[0][sq];      // 7 = empty in your code base
        return (pt != 7) ? pt : m_board[1][sq];
    };

    // Initial capture
    int swap = VAL[piece_on(to)] - threshold; // gain after first capture
    if (swap < 0)
        return false; // already below target

    uint64_t occ = m_all_pieces_bit ^ (1ULL << from); // remove moving piece
    uint64_t atk = attackers_to(to, occ);

    int stm = m_turn ? 0 : 1; // side‑to‑move in SEE (0 = white, 1 = black)
    const int us = stm;

    while (true)
    {
        stm ^= 1;   // opponent replies
        atk &= occ; // only remaining pieces
        uint64_t stmAtk = atk & (m_pieces[stm][0] | m_pieces[stm][1] | m_pieces[stm][2] |
                                 m_pieces[stm][3] | m_pieces[stm][4] | m_pieces[stm][5]);
        if (!stmAtk)
            break; // no more recaptures → previous side wins

        auto [sq, pt] = pop_lva(stmAtk, stm);
        swap = VAL[pt] - swap; // new material balance

        if (swap < 0 && stm == us)
            return false; // we drop below 0
        if (swap >= 0 && stm != us)
            return true; // opponent can’t refute

        occ ^= 1ULL << sq;            // remove that piece
        atk |= attackers_to(to, occ); // add x‑rays
    }
    return stm != us; // true if we made the last capture
}

int BitPosition::countStartPieces() const
{
    int count = 0;

    // White pieces
    const uint64_t white_pawn_start = 0x000000000000FF00;
    const uint64_t white_rook_start = 0x0000000000000081;
    const uint64_t white_knight_start = 0x0000000000000042;
    const uint64_t white_bishop_start = 0x0000000000000024;
    const uint64_t white_queen_start = 0x0000000000000008;
    const uint64_t white_king_start = 0x0000000000000010;

    count += countBits(m_pieces[0][0] & white_pawn_start);
    count += countBits(m_pieces[0][1] & white_knight_start);
    count += countBits(m_pieces[0][2] & white_bishop_start);
    count += countBits(m_pieces[0][3] & white_rook_start);
    count += countBits(m_pieces[0][4] & white_queen_start);
    count += countBits(m_pieces[0][5] & white_king_start);

    // Black pieces
    const uint64_t black_pawn_start = 0x00FF000000000000;
    const uint64_t black_rook_start = 0x8100000000000000;
    const uint64_t black_knight_start = 0x4200000000000000;
    const uint64_t black_bishop_start = 0x2400000000000000;
    const uint64_t black_queen_start = 0x0800000000000000;
    const uint64_t black_king_start = 0x1000000000000000;

    count += countBits(m_pieces[1][0] & black_pawn_start);
    count += countBits(m_pieces[1][1] & black_knight_start);
    count += countBits(m_pieces[1][2] & black_bishop_start);
    count += countBits(m_pieces[1][3] & black_rook_start);
    count += countBits(m_pieces[1][4] & black_queen_start);
    count += countBits(m_pieces[1][5] & black_king_start);

    return count;
}

int BitPosition::countAllPieces() const
{
    return countBits(m_all_pieces_bit);
}

static const uint8_t castlingMask[64] = {
    0x02, 0, 0, 0, 0x03, 0, 0, 0x01,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0x08, 0, 0, 0, 0xC, 0, 0, 0x04};

template NNUEU::NNUEUChange BitPosition::makeMove<Move>(Move move, StateInfo &new_stae_info);
template NNUEU::NNUEUChange BitPosition::makeMove<ScoredMove>(ScoredMove move, StateInfo &new_stae_info);

template NNUEU::NNUEUChange BitPosition::makeCapture<Move>(Move move, StateInfo &new_stae_info);
template NNUEU::NNUEUChange BitPosition::makeCapture<ScoredMove>(ScoredMove move, StateInfo &new_stae_info);

template void BitPosition::unmakeMove<Move>(Move move);
template void BitPosition::unmakeMove<ScoredMove>(ScoredMove move);

template void BitPosition::unmakeCapture<Move>(Move move);
template void BitPosition::unmakeCapture<ScoredMove>(ScoredMove move);

template bool BitPosition::isLegal<Move>(const Move *move) const;
template bool BitPosition::isLegal<ScoredMove>(const ScoredMove *move) const;

template bool BitPosition::isCaptureLegal<Move>(const Move *move) const;
template bool BitPosition::isCaptureLegal<ScoredMove>(const ScoredMove *move) const;

// Precomputed NNUE base indices: side in {0,1}, piece in {0..4}
static constexpr int NNUE_BASE[2][5] = {
    { 64 * 0,  64 * 1,  64 * 2,  64 * 3,  64 * 4}, // side = 0
    { 64 * 5,  64 * 6,  64 * 7,  64 * 8,  64 * 9}  // side = 1
};

Move castling_moves[2][2]{{Move(16772), Move(16516)}, {Move(20412), Move(20156)}}; // [[WKS, WQS], [BKS, BQS]] (origin = origin of king, destination = destination of king)

static constexpr int kingside_castling_check_squares[2][2] = {{5, 6}, {61, 62}};   // [white][sq1, sq2], [black][sq1, sq2]
static constexpr int queenside_castling_check_squares[2][2] = {{2, 3}, {58, 59}}; // [white][sq1, sq2], [black][sq1, sq2]

static constexpr uint8_t castling_rights_masks[2][2] = {{WHITE_KS, WHITE_QS}, {BLACK_KS, BLACK_QS}};
static constexpr uint64_t castling_empty_squares_masks[2][2] = {
    {96, 14},                                           // White: KS, QS
    {6917529027641081856ULL, 1008806316530991104ULL}    // Black: KS, QS
};

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

// Function pointers for pawn shifts based on color
using ShiftFunc = uint64_t (*)(uint64_t);
static constexpr ShiftFunc shift_forward[2] = {shift_up, shift_down};
static constexpr ShiftFunc shift_forward_right[2] = {shift_up_right, shift_down_right};
static constexpr ShiftFunc shift_forward_left[2] = {shift_up_left, shift_down_left};

static constexpr uint64_t promotion_ranks[2] = {EIGHT_ROW_BITBOARD, FIRST_ROW_BITBOARD};
static constexpr uint64_t double_move_boards[2] = {THIRD_ROW_BITBOARD, SIXTH_ROW_BITBOARD};
static constexpr int pawn_move_offsets[2] = {-8, 8}; // destination - origin
static constexpr int double_pawn_move_offsets[2] = {-16, 16}; // destination - origin
static constexpr int pawn_capture_offsets_right[2] = {-9, 7}; // destination - origin
static constexpr int pawn_capture_offsets_left[2] = {-7, 9}; // destination - origin

uint64_t BitPosition::computeFullZobristKey() const
{
    uint64_t key = 0ULL;

    auto xor_piece_list = [&](int colour, int piece, uint64_t bb)
    {
        while (bb)
        {
            int sq = popLeastSignificantBit(bb);
            key ^= zobrist_keys::pieceZobristNumbers[colour][piece][sq];
        }
    };

    // white non-king pieces (piece types 0..4)
    for (int pt = 0; pt < 5; ++pt)
        xor_piece_list(0, pt, m_pieces[0][pt]);
    // white king
    key ^= zobrist_keys::pieceZobristNumbers[0][5][m_king_position[0]];

    // black non-king pieces
    for (int pt = 0; pt < 5; ++pt)
        xor_piece_list(1, pt, m_pieces[1][pt]);
    // black king
    key ^= zobrist_keys::pieceZobristNumbers[1][5][m_king_position[1]];

    // side to move
    if (!m_turn)
        key ^= zobrist_keys::blackToMoveZobristNumber;

    // castling rights
    key ^= zobrist_keys::castlingRightsZobristNumbers[state_info->castlingRights];

    // en-passant file (0-7) or 8 if none
    key ^= zobrist_keys::passantSquaresZobristNumbers[state_info->pSquare];

    return key;
}

// Functions we call on initialization
void BitPosition::initializeZobristKey()
{
    state_info->zobristKey = computeFullZobristKey();
}

void BitPosition::setIsCheckOnInitialization()
// For when moving the king
{
    state_info->isCheck = false;
    // Knights
    if ((precomputed_moves::knight_moves[m_king_position[not m_turn]] & m_pieces[m_turn][1]))
        state_info->isCheck = true;
    // Pawns
    if ((precomputed_moves::pawn_attacks[not m_turn][m_king_position[not m_turn]] & m_pieces[m_turn][0]))
        state_info->isCheck = true;

    // Queen
    if (((RmagicNOMASK(m_king_position[not m_turn], precomputed_moves::rook_unfull_rays[m_king_position[not m_turn]] & m_all_pieces_bit) | BmagicNOMASK(m_king_position[not m_turn], precomputed_moves::bishop_unfull_rays[m_king_position[not m_turn]] & m_all_pieces_bit)) & m_pieces[m_turn][4]) != 0)
        state_info->isCheck = true;

    // Rook
    if ((RmagicNOMASK(m_king_position[not m_turn], precomputed_moves::rook_unfull_rays[m_king_position[not m_turn]] & m_all_pieces_bit) & m_pieces[m_turn][3]))
        state_info->isCheck = true;

    // Bishop
    if ((BmagicNOMASK(m_king_position[not m_turn], precomputed_moves::bishop_unfull_rays[m_king_position[not m_turn]] & m_all_pieces_bit) & m_pieces[m_turn][2]))
        state_info->isCheck = true;

    // King
    if ((precomputed_moves::king_moves[m_king_position[not m_turn]] & m_pieces[m_turn][5]) != 0)
        state_info->isCheck = true;
}

bool BitPosition::getIsCheckOnInitialization(bool turn)
// For when moving the king
{
    // Knights
    if ((precomputed_moves::knight_moves[m_king_position[not turn]] & m_pieces[turn][1]))
        return true;
    // Pawns
    if ((precomputed_moves::pawn_attacks[not turn][m_king_position[not turn]] & m_pieces[turn][0]))
        return true;

    // Queen
    if (((RmagicNOMASK(m_king_position[not turn], precomputed_moves::rook_unfull_rays[m_king_position[not turn]] & m_all_pieces_bit) | BmagicNOMASK(m_king_position[not turn], precomputed_moves::bishop_unfull_rays[m_king_position[not turn]] & m_all_pieces_bit)) & m_pieces[turn][4]) != 0)
        return true;

    // Rook
    if ((RmagicNOMASK(m_king_position[not turn], precomputed_moves::rook_unfull_rays[m_king_position[not turn]] & m_all_pieces_bit) & m_pieces[turn][3]))
        return true;

    // Bishop
    if ((BmagicNOMASK(m_king_position[not turn], precomputed_moves::bishop_unfull_rays[m_king_position[not turn]] & m_all_pieces_bit) & m_pieces[turn][2]))
        return true;

    // King
    if ((precomputed_moves::king_moves[m_king_position[not turn]] & m_pieces[turn][5]) != 0)
        return true;
    return false;
}
bool BitPosition::isKingInCheck(bool side) const
{
    int kingSquare = m_king_position[side];

    // Pawn check
    if (precomputed_moves::pawn_attacks[side][kingSquare] & m_pieces[not side][0])
        return true;

    // Knight check
    if (precomputed_moves::knight_moves[kingSquare] & m_pieces[not side][1])
        return true;

    // Bishop/Queen check
    uint64_t bishopsQueens = (m_pieces[not side][2] | m_pieces[not side][4]);
    if (BmagicNOMASK(kingSquare,
                     precomputed_moves::bishop_unfull_rays[kingSquare] & m_all_pieces_bit) &
        bishopsQueens)
        return true;

    // Rook/Queen check
    uint64_t rooksQueens = (m_pieces[not side][3] | m_pieces[not side][4]);
    if (RmagicNOMASK(kingSquare,
                     precomputed_moves::rook_unfull_rays[kingSquare] & m_all_pieces_bit) &
        rooksQueens)
        return true;

    // King check (only relevant if kings are adjacent, but good to be consistent)
    if (precomputed_moves::king_moves[kingSquare] & m_pieces[not side][5])
        return true;

    return false;
}

// Functions we call before first move generation
void BitPosition::setCheckInfoOnInitialization()
{
    m_num_checks = 0;
    m_check_rays = 0;
    m_check_square = 65;

    // Pawn check
    int pawnCheck{getLeastSignificantBitIndex(precomputed_moves::pawn_attacks[not m_turn][m_king_position[not m_turn]] & m_pieces[m_turn][0])};
    if (pawnCheck != 65)
    {
        m_num_checks++;
        m_check_square = pawnCheck;
    }
    // Knight check
    int knightCheck{getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_king_position[not m_turn]] & m_pieces[m_turn][1])};
    if (knightCheck != 65)
    {
        m_num_checks++;
        m_check_square = knightCheck;
    }
    // Bishop check
    uint64_t piece_bits = m_pieces[m_turn][2];
    while (piece_bits)
    {
        int bishopSquare = popLeastSignificantBit(piece_bits);
        uint64_t bishopRay{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_king_position[not m_turn]][bishopSquare]};
        if ((bishopRay & m_all_pieces_bit) == (1ULL << bishopSquare))
        {
            m_num_checks++;
            m_check_rays |= bishopRay & ~(1ULL << bishopSquare);
            m_check_square = bishopSquare;
        }
    }
    // Rook check
    piece_bits = m_pieces[m_turn][3];
    while (piece_bits)
    {
        int rookSquare = popLeastSignificantBit(piece_bits);
        uint64_t rookRay{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_king_position[not m_turn]][rookSquare]};
        if ((rookRay & m_all_pieces_bit) == (1ULL << rookSquare))
        {
            m_num_checks++;
            m_check_rays |= rookRay & ~(1ULL << rookSquare);
            m_check_square = rookSquare;
        }
    }
    // Queen check
    piece_bits = m_pieces[m_turn][4];
    while (piece_bits)
    {
        int queenSquare = popLeastSignificantBit(piece_bits);
        uint64_t queenRayDiag{precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_king_position[not m_turn]][queenSquare]};
        if ((queenRayDiag & m_all_pieces_bit) == (1ULL << queenSquare))
        {
            m_num_checks++;
            m_check_rays |= queenRayDiag & ~(1ULL << queenSquare);
            m_check_square = queenSquare;
        }
        uint64_t queenRayStra{precomputed_moves::precomputedRookMovesTableOneBlocker2[m_king_position[not m_turn]][queenSquare]};
        if ((queenRayStra & m_all_pieces_bit) == (1ULL << queenSquare))
        {
            m_num_checks++;
            m_check_rays |= queenRayStra & ~(1ULL << queenSquare);
            m_check_square = queenSquare;
        }
    }
}

// Functions we call before move generation
void BitPosition::setCheckInfo()
// This is called before searching for moves in check in engine.cpp, and also in unmakeTTMove if the previous position was in check
{
    m_num_checks = 0;    // Number of checks
    m_check_rays = 0;    // Check ray if any
    m_check_square = 65; // Check square. If there is more than one, only one is stored

    int check_square{getLeastSignificantBitIndex(precomputed_moves::pawn_attacks[not m_turn][m_king_position[not m_turn]] & m_pieces[m_turn][0])};
    if (check_square != 65)
    {
        m_num_checks++;
        m_check_square = check_square;
    }
    // Knight
    check_square = getLeastSignificantBitIndex(precomputed_moves::knight_moves[m_king_position[not m_turn]] & m_pieces[m_turn][1]);
    if (check_square != 65)
    {
        m_num_checks++;
        m_check_square = check_square;
    }
    // Bishop and queen
    uint64_t checks{BmagicNOMASK(m_king_position[not m_turn], precomputed_moves::bishop_unfull_rays[m_king_position[not m_turn]] & m_all_pieces_bit) & (m_pieces[m_turn][2] | m_pieces[m_turn][4])};
    while (checks)
    {
        check_square = popLeastSignificantBit(checks);
        uint64_t ray = precomputed_moves::precomputedBishopMovesTableOneBlocker2[m_king_position[not m_turn]][check_square];
        m_num_checks++;
        m_check_rays |= ray & ~(1ULL << check_square);
        m_check_square = check_square;
    }
    // Rook and queen
    checks = RmagicNOMASK(m_king_position[not m_turn], precomputed_moves::rook_unfull_rays[m_king_position[not m_turn]] & m_all_pieces_bit) & (m_pieces[m_turn][3] | m_pieces[m_turn][4]);
    while (checks)
    {
        check_square = popLeastSignificantBit(checks);
        uint64_t ray = precomputed_moves::precomputedRookMovesTableOneBlocker2[m_king_position[not m_turn]][check_square];
        m_num_checks++;
        m_check_rays |= ray & ~(1ULL << check_square);
        m_check_square = check_square;
    }
}

void BitPosition::setCheckBits()
{
    // Pawn checking bits
    state_info->checkBits[0] = precomputed_moves::pawn_attacks[m_turn][m_king_position[m_turn]];
    // Knight checking bits
    state_info->checkBits[1] = precomputed_moves::knight_moves[m_king_position[m_turn]];
    // Bishop and queen checking bits
    uint64_t diagonalChecks = BmagicNOMASK(m_king_position[m_turn], precomputed_moves::bishop_unfull_rays[m_king_position[m_turn]] & m_all_pieces_bit);
    state_info->checkBits[2] = diagonalChecks;
    state_info->checkBits[4] = diagonalChecks;

    // Rook and queen checking bits
    uint64_t straightChecks = RmagicNOMASK(m_king_position[m_turn], precomputed_moves::rook_unfull_rays[m_king_position[m_turn]] & m_all_pieces_bit);
    state_info->checkBits[3] = straightChecks;
    state_info->checkBits[4] |= straightChecks;
}
void BitPosition::setBlockersPinsAndCheckBitsInQS()
{
    state_info->blockersForKing = 0;
    state_info->pinnedPieces = 0;
    m_blockers_set = true;

    // Blockers of own bishops, rooks and queens
    uint64_t snipers_bits = ((m_pieces[not m_turn][2] | m_pieces[not m_turn][4]) & precomputed_moves::bishop_full_rays[m_king_position[m_turn]]) | ((m_pieces[not m_turn][3] | m_pieces[not m_turn][4]) & precomputed_moves::rook_full_rays[m_king_position[m_turn]]);
    while (snipers_bits)
    // For each square corresponding to black bishop raying black king
    {
        uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_king_position[m_turn]] & m_all_pieces_bit;
        if (ray && hasOneOne(ray))
            state_info->blockersForKing |= ray;
    }
    // Pins of opponent bishops, rooks and queens
    snipers_bits = ((m_pieces[m_turn][2] | m_pieces[m_turn][4]) & precomputed_moves::bishop_full_rays[m_king_position[not m_turn]]) | ((m_pieces[m_turn][3] | m_pieces[m_turn][4]) & precomputed_moves::rook_full_rays[m_king_position[not m_turn]]);
    while (snipers_bits)
    // For each square corresponding to black bishop raying black king
    {
        uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_king_position[not m_turn]] & m_all_pieces_bit;
        if ((ray & m_pieces_bit[not m_turn]) && hasOneOne(ray))
        {
            state_info->pinnedPieces |= ray;
        }
    }
    setCheckBits();
}
void BitPosition::setBlockersAndPinsInAB()
// This is the only one called before ttable moves, since we need ot determine if move gives check or not.
// It is also called in nextMove, nextCapture, nextMoveInCheck and nextCaptureInCheck, when a legal move is found.
{
    state_info->blockersForKing = 0;
    state_info->straightPinnedPieces = 0;
    state_info->diagonalPinnedPieces = 0;
    m_blockers_set = true;

    // Blockers of own bishops, rooks and queens
    uint64_t snipers_bits = ((m_pieces[not m_turn][2] | m_pieces[not m_turn][4]) & precomputed_moves::bishop_full_rays[m_king_position[m_turn]]) | ((m_pieces[not m_turn][3] | m_pieces[not m_turn][4]) & precomputed_moves::rook_full_rays[m_king_position[m_turn]]);
    while (snipers_bits)
    // For each square corresponding to our sniper raying opponent king
    {
        uint64_t ray = precomputed_moves::precomputedQueenMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_king_position[m_turn]] & m_all_pieces_bit;
        if (ray && hasOneOne(ray))
            state_info->blockersForKing |= ray;
    }
    // Pins of opponent bishops and queens
    snipers_bits = (m_pieces[m_turn][2] | m_pieces[m_turn][4]) & precomputed_moves::bishop_full_rays[m_king_position[not m_turn]];
    while (snipers_bits)
    // For each square corresponding to black bishop raying black king
    {
        uint64_t bishop_ray = precomputed_moves::precomputedBishopMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_king_position[not m_turn]] & m_all_pieces_bit;
        if ((bishop_ray & m_pieces_bit[not m_turn]) && hasOneOne(bishop_ray))
        {
            state_info->diagonalPinnedPieces |= bishop_ray;
        }
    }
    // Pins of opponent rooks and queens
    snipers_bits = (m_pieces[m_turn][3] | m_pieces[m_turn][4]) & precomputed_moves::rook_full_rays[m_king_position[not m_turn]];
    while (snipers_bits)
    // For each square corresponding to black rook raying black king
    {
        uint64_t rook_ray = precomputed_moves::precomputedRookMovesTableOneBlocker[popLeastSignificantBit(snipers_bits)][m_king_position[not m_turn]] & m_all_pieces_bit;
        if ((rook_ray & m_pieces_bit[not m_turn]) && hasOneOne(rook_ray))
        {
            state_info->straightPinnedPieces |= rook_ray;
        }
    }
    state_info->pinnedPieces = state_info->straightPinnedPieces | state_info->diagonalPinnedPieces;
}

template <typename T>
bool BitPosition::isLegal(const T *move) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    // Move is legal if piece is not pinned, otherwise if origin, destination and king position are aligned
    if (move->getData() == castling_moves[not m_turn][0].getData()) // Kingside castling
    {
        return newKingSquareIsSafe(kingside_castling_check_squares[not m_turn][0]) && newKingSquareIsSafe(kingside_castling_check_squares[not m_turn][1]);
    }
    else if (move->getData() == castling_moves[not m_turn][1].getData()) // Queenside castling
    {
        return newKingSquareIsSafe(queenside_castling_check_squares[not m_turn][0]) && newKingSquareIsSafe(queenside_castling_check_squares[not m_turn][1]);
    }
    else
    {
        int origin_square = move->getOriginSquare();
        // Knight moves are always legal
        if ((1ULL << origin_square) & m_pieces[not m_turn][1])
            return true;
        // King moves
        else if (origin_square == m_king_position[not m_turn])
            return newKingSquareIsSafe(move->getDestinationSquare());
        // Rest of pieces
        else
            return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][move->getDestinationSquare()] & m_pieces[not m_turn][5];
    }
}

bool BitPosition::isNormalMoveLegal(int origin_square, int destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called in isMate within QuisenceSearch
{
    // Move is legal if piece is not pinned, otherwise if origin, destination and king position are aligned
    // Knight moves are always legal
    if ((1ULL << origin_square) & m_pieces[not m_turn][1])
        return true;
    // King moves
    else if (origin_square == m_king_position[not m_turn])
        return newKingSquareIsSafe(destination_square); // FIX THIS FOR BOTH COLORS
    // Rest of pieces
    else
        return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_pieces[not m_turn][5];
}


template <typename T>
bool BitPosition::isCaptureLegal(const T *move) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
// This is only called when origin is in line with king position and there are no pieces in between
{
    int origin_square = move->getOriginSquare();

    // King moves
    if (origin_square == m_king_position[not m_turn])
        return newKingSquareIsSafe(move->getDestinationSquare()); // FIX THIS FOR BOTH COLORS
    // Rest of pieces
    else
        return ((1ULL << origin_square) & (state_info->pinnedPieces)) == 0 || precomputed_moves::OnLineBitboards[origin_square][move->getDestinationSquare()] & m_pieces[not m_turn][5];
}

bool BitPosition::ttMoveIsOk(Move move) const
{
    if (((1ULL << move.getOriginSquare()) & m_pieces_bit[not m_turn]) == 0 || ((1ULL << move.getDestinationSquare()) & m_pieces_bit[not m_turn]))
        return false;
    else
        return true;
}

bool BitPosition::newKingSquareIsSafe(int new_position) const
// For when moving the king in captures
{
    // Knights
    if (precomputed_moves::knight_moves[new_position] & m_pieces[m_turn][1])
        return false;

    // Pawns
    if (precomputed_moves::pawn_attacks[not m_turn][new_position] & m_pieces[m_turn][0])
        return false;

    // Rook and queen
    if (RmagicNOMASK(new_position, precomputed_moves::rook_unfull_rays[new_position] & m_all_pieces_bit & ~m_pieces[not m_turn][5]) & (m_pieces[m_turn][3] | m_pieces[m_turn][4]))
        return false;

    // Bishop and queen
    if (BmagicNOMASK(new_position, precomputed_moves::bishop_unfull_rays[new_position] & m_all_pieces_bit & ~m_pieces[not m_turn][5]) & (m_pieces[m_turn][2] | m_pieces[m_turn][4]))
        return false;

    // King
    if (precomputed_moves::king_moves[new_position] & m_pieces[m_turn][5])
        return false;

    return true;
}

bool BitPosition::kingIsSafeAfterPassant(int removed_square_1, int removed_square_2) const // See if the king is in check or not (from kings position). For when moving the king.
{
    // Black bishops and queens
    if ((BmagicNOMASK(m_king_position[not m_turn], precomputed_moves::bishop_unfull_rays[m_king_position[not m_turn]] & (m_all_pieces_bit & ~((1ULL << removed_square_1) | (1ULL << removed_square_2)))) & (m_pieces[m_turn][2] | m_pieces[m_turn][4])) != 0)
        return false;

    // Black rooks and queens
    if ((RmagicNOMASK(m_king_position[not m_turn], precomputed_moves::rook_unfull_rays[m_king_position[not m_turn]] & (m_all_pieces_bit & ~((1ULL << removed_square_1) | (1ULL << removed_square_2)))) & (m_pieces[m_turn][3] | m_pieces[m_turn][4])) != 0)
        return false;
    return true;
}

// Functions we call inside move makers
bool BitPosition::isDiscoverCheckAfterPassant() const
// Return if we are in check or not by sliders, for the case of discovered checks.
// We just check if removing the pawn captured leads to check, since check after removing and placing our pawn is already checked using isDiscoverCheck
{
    // If captured pawn is not blocking or after removing captured pawn we are not in check
    if ((BmagicNOMASK(m_king_position[m_turn], precomputed_moves::bishop_unfull_rays[m_king_position[m_turn]] & m_all_pieces_bit) & (m_pieces[not m_turn][2] | m_pieces[not m_turn][4])) || (RmagicNOMASK(m_king_position[m_turn], precomputed_moves::rook_unfull_rays[m_king_position[m_turn]] & m_all_pieces_bit) & (m_pieces[not m_turn][3] | m_pieces[not m_turn][4])))
        return true;
    return false;
}

bool BitPosition::isDiscoverCheck(int origin_square, int destination_square) const
// Return if we are in check or not by sliders, for the case of discovered checks
// For direct checks we know because of the move
{
    // If piece is not blocking or moving in blocking ray
    if (((1ULL << origin_square) & (state_info->previous->blockersForKing)) == 0 || (precomputed_moves::OnLineBitboards[origin_square][destination_square] & m_pieces[m_turn][5]) != 0)
        return false;

    return true;
}

bool BitPosition::isQueenCheck(int destination_square)
{
    if ((precomputed_moves::precomputedQueenMovesTableOneBlocker2[destination_square][m_king_position[m_turn]] & m_all_pieces_bit) == m_pieces[m_turn][5])
        return true;
    return false;
}

bool BitPosition::isPromotionCheck(int piece, int destination_square)
{
    if (piece == 2 && ((precomputed_moves::precomputedBishopMovesTableOneBlocker2[destination_square][m_king_position[m_turn]] & m_all_pieces_bit) == m_pieces[m_turn][5]))
        return true;
    else if (piece == 3 && ((precomputed_moves::precomputedRookMovesTableOneBlocker2[destination_square][m_king_position[m_turn]] & m_all_pieces_bit) == m_pieces[m_turn][5]))
        return true;
    else if (piece == 4 && ((precomputed_moves::precomputedQueenMovesTableOneBlocker2[destination_square][m_king_position[m_turn]] & m_all_pieces_bit) == m_pieces[m_turn][5]))
        return true;
    return false;
}
// First move generations
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

// Capture move generations (for Quiesence) assuming pins are not set (we set them if we find a pseudo legal move)
ScoredMove *BitPosition::pawnCapturesAndQueenProms(ScoredMove *&move_list) const
{
    uint64_t pawns = m_pieces[not m_turn][0];

    // Right shift captures (no promotion)
    uint64_t destination_bitboard = shift_forward_right[not m_turn](pawns & NON_RIGHT_BITBOARD) & m_pieces_bit[m_turn] & ~promotion_ranks[not m_turn];
    while (destination_bitboard)
    {
        int destination = popLeastSignificantBit(destination_bitboard);
        *move_list = Move(destination + pawn_capture_offsets_right[not m_turn], destination);
        move_list->score = m_board[not m_turn][destination];
        move_list++;
    }

    // Left shift captures (no promotion)
    destination_bitboard = shift_forward_left[not m_turn](pawns & NON_LEFT_BITBOARD) & m_pieces_bit[m_turn] & ~promotion_ranks[not m_turn];
    while (destination_bitboard)
    {
        int destination = popLeastSignificantBit(destination_bitboard);
        *move_list = Move(destination + pawn_capture_offsets_left[not m_turn], destination);
        move_list->score = m_board[not m_turn][destination];
        move_list++;
    }

    // Queen promotions (no capture)
    destination_bitboard = shift_forward[not m_turn](pawns) & ~m_all_pieces_bit & promotion_ranks[not m_turn];
    while (destination_bitboard)
    {
        int destination = popLeastSignificantBit(destination_bitboard);
        *move_list = Move(destination + pawn_move_offsets[not m_turn], destination, 3);
        move_list->score = 30;
        move_list++;
    }

    // Right shift captures with queen promotion
    destination_bitboard = shift_forward_right[not m_turn](pawns & NON_RIGHT_BITBOARD) & m_pieces_bit[m_turn] & promotion_ranks[not m_turn];
    while (destination_bitboard)
    {
        int destination = popLeastSignificantBit(destination_bitboard);
        *move_list = Move(destination + pawn_capture_offsets_right[not m_turn], destination, 3);
        move_list->score = 30;
        move_list++;
    }

    // Left shift captures with queen promotion
    destination_bitboard = shift_forward_left[not m_turn](pawns & NON_LEFT_BITBOARD) & m_pieces_bit[m_turn] & promotion_ranks[not m_turn];
    while (destination_bitboard)
    {
        int destination = popLeastSignificantBit(destination_bitboard);
        *move_list = Move(destination + pawn_capture_offsets_left[not m_turn], destination, 3);
        move_list->score = 30;
        move_list++;
    }

    return move_list;
}
ScoredMove *BitPosition::knightCaptures(ScoredMove *&move_list) const
// All knight captures
{
    uint64_t moveable_knights{m_pieces[not m_turn][1] & ~state_info->pinnedPieces};
    uint64_t enemy = m_pieces_bit[m_turn];
    while (moveable_knights)
    {
        int origin{popLeastSignificantBit(moveable_knights)};
        uint64_t destinations{precomputed_moves::knight_moves[origin] & enemy};
        while (destinations)
        {
            int dest = popLeastSignificantBit(destinations);
            *move_list = Move(origin, dest);
            move_list->score = qsScore(dest);
            move_list++;
        }
    }
    return move_list;
}
ScoredMove *BitPosition::bishopCaptures(ScoredMove *&move_list) const
// All bishop captures
{
    uint64_t moveable_bishops{m_pieces[not m_turn][2]};
    uint64_t occ = m_all_pieces_bit;
    uint64_t enemy = m_pieces_bit[m_turn];

    while (moveable_bishops)
    {
        int origin{popLeastSignificantBit(moveable_bishops)};
        uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & occ) & enemy};
        while (destinations)
        {
            int dest = popLeastSignificantBit(destinations);
            *move_list = Move(origin, dest);
            move_list->score = qsScore(dest);
            move_list++;
        }
    }

    return move_list;
}
ScoredMove *BitPosition::rookCaptures(ScoredMove *&move_list) const
// All rook captures except capturing unsafe pawns, knights or rooks
{
    uint64_t moveable_rooks{m_pieces[not m_turn][3]};
    uint64_t occ = m_all_pieces_bit;
    uint64_t enemy = m_pieces_bit[m_turn];
    while (moveable_rooks)
    {
        int origin{popLeastSignificantBit(moveable_rooks)};
        uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & occ) & enemy};
        while (destinations)
        {
            int dest = popLeastSignificantBit(destinations);
            *move_list = Move(origin, dest);
            move_list->score = qsScore(dest);
            move_list++;
        }
    }
    return move_list;
}
ScoredMove *BitPosition::queenCaptures(ScoredMove *&move_list) const
// All queen captures except capturing unsafe pieces
{
    uint64_t moveable_queens{m_pieces[not m_turn][4]};
    uint64_t occ = m_all_pieces_bit;
    uint64_t enemy = m_pieces_bit[m_turn];
    while (moveable_queens)
    {
        int origin{popLeastSignificantBit(moveable_queens)};
        uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & occ) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & occ)) & enemy};
        while (destinations)
        {
            int dest = popLeastSignificantBit(destinations);
            *move_list = Move(origin, dest);
            move_list->score = qsScore(dest);
            move_list++;
        }
    }
    return move_list;
}
ScoredMove *BitPosition::kingCaptures(ScoredMove *&move_list) const
{
    uint64_t destinations{precomputed_moves::king_moves[m_king_position[not m_turn]] & m_pieces_bit[m_turn]};
    while (destinations)
    {
        int dest = popLeastSignificantBit(destinations);
        *move_list = Move(m_king_position[not m_turn], dest);
        move_list->score = qsScore(dest);
        move_list++;
    }
    return move_list;
}
Move *BitPosition::kingCaptures(Move *&move_list) const
{
    uint64_t destinations{precomputed_moves::king_moves[m_king_position[not m_turn]] & m_pieces_bit[m_turn]};
    while (destinations)
        *move_list++ = Move(m_king_position[not m_turn], popLeastSignificantBit(destinations));
    return move_list;
}

// All move generations (for PV Nodes in Alpha-Beta)
ScoredMove *BitPosition::pawnAllMoves(ScoredMove *&move_list) const
{
    const uint64_t pawns = m_pieces[not m_turn][0];

    // Single moves
    uint64_t single_moves = shift_forward[not m_turn](pawns & ~state_info->diagonalPinnedPieces) & ~m_all_pieces_bit;
    uint64_t single_non_promotions = single_moves & ~promotion_ranks[not m_turn];
    while (single_non_promotions)
    {
        int destination = popLeastSignificantBit(single_non_promotions);
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination);
    }
    uint64_t single_promotions = single_moves & promotion_ranks[not m_turn];
    while (single_promotions)
    {
        int destination = popLeastSignificantBit(single_promotions);
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination, 3); // Queen
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination, 2); // Rook
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination, 1); // Bishop
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination, 0); // Knight
    }

    // Double moves
    uint64_t double_move_board = double_move_boards[not m_turn];
    uint64_t double_moves = shift_forward[not m_turn](single_moves & double_move_board) & ~m_all_pieces_bit;
    while (double_moves)
    {
        int destination = popLeastSignificantBit(double_moves);
        *move_list++ = Move(destination + double_pawn_move_offsets[not m_turn], destination);
    }

    // Right shift captures
    uint64_t right_captures = shift_forward_right[not m_turn](pawns & NON_RIGHT_BITBOARD & ~state_info->straightPinnedPieces) & m_pieces_bit[m_turn];
    uint64_t right_captures_non_promotions = right_captures & ~promotion_ranks[not m_turn];
    while (right_captures_non_promotions)
    {
        int destination = popLeastSignificantBit(right_captures_non_promotions);
        *move_list++ = Move(destination + pawn_capture_offsets_right[not m_turn], destination);
    }
    uint64_t right_captures_promotions = right_captures & promotion_ranks[not m_turn];
    while (right_captures_promotions)
    {
        int destination = popLeastSignificantBit(right_captures_promotions);
        *move_list++ = Move(destination + pawn_capture_offsets_right[not m_turn], destination, 3);
        *move_list++ = Move(destination + pawn_capture_offsets_right[not m_turn], destination, 2);
        *move_list++ = Move(destination + pawn_capture_offsets_right[not m_turn], destination, 1);
        *move_list++ = Move(destination + pawn_capture_offsets_right[not m_turn], destination, 0);
    }

    // Left shift captures
    uint64_t left_captures = shift_forward_left[not m_turn](pawns & NON_LEFT_BITBOARD & ~state_info->straightPinnedPieces) & m_pieces_bit[m_turn];
    uint64_t left_captures_non_promotions = left_captures & ~promotion_ranks[not m_turn];
    while (left_captures_non_promotions)
    {
        int destination = popLeastSignificantBit(left_captures_non_promotions);
        *move_list++ = Move(destination + pawn_capture_offsets_left[not m_turn], destination);
    }
    uint64_t left_captures_promotions = left_captures & promotion_ranks[not m_turn];
    while (left_captures_promotions)
    {
        int destination = popLeastSignificantBit(left_captures_promotions);
        *move_list++ = Move(destination + pawn_capture_offsets_left[not m_turn], destination, 3);
        *move_list++ = Move(destination + pawn_capture_offsets_left[not m_turn], destination, 2);
        *move_list++ = Move(destination + pawn_capture_offsets_left[not m_turn], destination, 1);
        *move_list++ = Move(destination + pawn_capture_offsets_left[not m_turn], destination, 0);
    }

    // En passant
    if (state_info->pSquare)
    {
        uint64_t passant_attackers = precomputed_moves::pawn_attacks[m_turn][state_info->pSquare] & pawns;
        while (passant_attackers)
        {
            int origin = popLeastSignificantBit(passant_attackers);
            if (kingIsSafeAfterPassant(origin, state_info->pSquare + pawn_move_offsets[not m_turn]))
            {
                *move_list++ = Move(origin, state_info->pSquare, 0);
            }
        }
    }

    return move_list;
}
ScoredMove *BitPosition::knightAllMoves(ScoredMove *&move_list) const
{
    uint64_t moveable_knights{m_pieces[not m_turn][1] & ~(state_info->pinnedPieces)};
    while (moveable_knights)
    {
        int origin{popLeastSignificantBit(moveable_knights)};
        uint64_t destinations{precomputed_moves::knight_moves[origin] & ~m_pieces_bit[not m_turn]};
        while (destinations)
            *move_list++ = Move(origin, popLeastSignificantBit(destinations));
    }
    return move_list;
}
ScoredMove *BitPosition::bishopAllMoves(ScoredMove *&move_list) const
{
    uint64_t moveable_bishops{m_pieces[not m_turn][2] & ~(state_info->straightPinnedPieces)};
    while (moveable_bishops)
    {
        int origin{popLeastSignificantBit(moveable_bishops)};
        uint64_t destinations{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_pieces_bit[not m_turn]};
        while (destinations)
            *move_list++ = Move(origin, popLeastSignificantBit(destinations));
    }
    return move_list;
}
ScoredMove *BitPosition::rookAllMoves(ScoredMove *&move_list) const
{
    uint64_t moveable_rooks{m_pieces[not m_turn][3] & ~(state_info->diagonalPinnedPieces)};
    while (moveable_rooks)
    {
        int origin{popLeastSignificantBit(moveable_rooks)};
        uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_pieces_bit[not m_turn]};
        while (destinations)
            *move_list++ = Move(origin, popLeastSignificantBit(destinations));
    }
    return move_list;
}
ScoredMove *BitPosition::queenAllMoves(ScoredMove *&move_list) const
{
    uint64_t moveable_queens{m_pieces[not m_turn][4]};
    while (moveable_queens)
    {
        int origin{popLeastSignificantBit(moveable_queens)};
        uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_pieces_bit[not m_turn]};
        while (destinations)
            *move_list++ = Move(origin, popLeastSignificantBit(destinations));
    }
    return move_list;
}
ScoredMove *BitPosition::kingAllMoves(ScoredMove *&move_list) const
{
    uint64_t destinations{precomputed_moves::king_moves[m_king_position[not m_turn]] & ~m_pieces_bit[not m_turn]};
    while (destinations)
        *move_list++ = Move(m_king_position[not m_turn], popLeastSignificantBit(destinations));

    const int side = not m_turn;

    // Kingside castling
    if ((state_info->castlingRights & castling_rights_masks[side][0]) && ((m_all_pieces_bit & castling_empty_squares_masks[side][0]) == 0))
        *move_list++ = castling_moves[side][0];

    // Queenside castling
    if ((state_info->castlingRights & castling_rights_masks[side][1]) && ((m_all_pieces_bit & castling_empty_squares_masks[side][1]) == 0))
        *move_list++ = castling_moves[side][1];

    return move_list;
}

// Check blocks (for Alpha-Beta)
Move *BitPosition::inCheckPawnBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    const uint64_t pawns = m_pieces[not m_turn][0];
    const uint64_t promotion_rank = promotion_ranks[not m_turn];

    // Single moves
    uint64_t single_moves = shift_forward[not m_turn](pawns & ~state_info->diagonalPinnedPieces) & ~m_all_pieces_bit;
    uint64_t pawn_blocks = single_moves & m_check_rays;

    uint64_t non_promotions = pawn_blocks & ~promotion_rank;
    while (non_promotions)
    {
        int destination = popLeastSignificantBit(non_promotions);
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination);
    }

    uint64_t promotions = pawn_blocks & promotion_rank;
    while (promotions)
    {
        int destination = popLeastSignificantBit(promotions);
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination, 0);
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination, 1);
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination, 2);
        *move_list++ = Move(destination + pawn_move_offsets[not m_turn], destination, 3);
    }

    // Double moves
    pawn_blocks = shift_forward[not m_turn](single_moves & double_move_boards[not m_turn]) & ~m_all_pieces_bit & m_check_rays;
    while (pawn_blocks)
    {
        int destination = popLeastSignificantBit(pawn_blocks);
        *move_list++ = Move(destination + double_pawn_move_offsets[not m_turn], destination);
    }

    return move_list;
}
Move *BitPosition::inCheckKnightBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    uint64_t pieces{m_pieces[not m_turn][1] & ~(state_info->pinnedPieces)};
    while (pieces)
    {
        int origin{popLeastSignificantBit(pieces)};
        uint64_t destinations{precomputed_moves::knight_moves[origin] & m_check_rays};
        while (destinations)
        {
            *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
Move *BitPosition::inCheckBishopBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    uint64_t pieces{m_pieces[not m_turn][2] & ~(state_info->straightPinnedPieces)};
    while (pieces)
    {
        int origin{popLeastSignificantBit(pieces)};
        uint64_t destiantions{BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
        while (destiantions)
        {
            *move_list++ = Move(origin, popLeastSignificantBit(destiantions));
        }
    }
    return move_list;
}
Move *BitPosition::inCheckRookBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    uint64_t pieces{m_pieces[not m_turn][3] & ~(state_info->diagonalPinnedPieces)};
    while (pieces)
    {
        int origin{popLeastSignificantBit(pieces)};
        uint64_t destinations{RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & m_check_rays};
        while (destinations)
        {
            *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
Move *BitPosition::inCheckQueenBlocks(Move *&move_list) const
// Only called if m_num_checks = 1 and m_check_rays != 0
// Non captures
{
    uint64_t pieces{m_pieces[not m_turn][4]};
    while (pieces)
    {
        int origin{popLeastSignificantBit(pieces)};
        uint64_t destinations{(BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & m_check_rays};
        while (destinations)
        {
            *move_list++ = Move(origin, popLeastSignificantBit(destinations));
        }
    }
    return move_list;
}
Move *BitPosition::inCheckOrderedCapturesAndKingMoves(Move *&move_list) const
// Only called if m_num_checks = 1
{
    // King captures
    uint64_t piece_moves = precomputed_moves::king_moves[m_king_position[not m_turn]] & m_pieces_bit[m_turn];
    while (piece_moves)
        *move_list++ = Move(m_king_position[not m_turn], popLeastSignificantBit(piece_moves));

    const uint64_t pawns = m_pieces[not m_turn][0];
    const uint64_t promotion_rank = promotion_ranks[not m_turn];

    // Pawn captures of the checking piece
    piece_moves = precomputed_moves::pawn_attacks[m_turn][m_check_square] & pawns;
    while (piece_moves)
    {
        int origin = popLeastSignificantBit(piece_moves);
        if (!((1ULL << m_check_square) & promotion_rank)) // Non-promotion capture
        {
            *move_list++ = Move(origin, m_check_square);
        }
        else // Promotion capture
        {
            *move_list++ = Move(origin, m_check_square, 0);
            *move_list++ = Move(origin, m_check_square, 1);
            *move_list++ = Move(origin, m_check_square, 2);
            *move_list++ = Move(origin, m_check_square, 3);
        }
    }

    // En passant capture
    if (state_info->pSquare != 0)
    {
        piece_moves = precomputed_moves::pawn_attacks[m_turn][state_info->pSquare] & pawns;
        while (piece_moves)
        {
            int origin = popLeastSignificantBit(piece_moves);
            if (kingIsSafeAfterPassant(origin, state_info->pSquare + pawn_move_offsets[not m_turn]))
            {
                *move_list++ = Move(origin, state_info->pSquare, 0);
            }
        }
    }

    // Knight captures from checking position
    piece_moves = precomputed_moves::knight_moves[m_check_square] & m_pieces[not m_turn][1] & ~state_info->pinnedPieces;
    while (piece_moves)
    {
        *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
    }
    // Bishop captures from checking position
    piece_moves = BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_pieces[not m_turn][2] & ~(state_info->straightPinnedPieces);
    while (piece_moves)
    {
        *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
    }
    // Rook captures from checking position
    piece_moves = RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_pieces[not m_turn][3] & ~(state_info->diagonalPinnedPieces);
    while (piece_moves)
    {
        *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
    }
    // Queen captures from checking position
    piece_moves = (BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_pieces[not m_turn][4];
    while (piece_moves)
    {
        *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
    }
    // King non captures
    piece_moves = precomputed_moves::king_moves[m_king_position[not m_turn]] & ~m_all_pieces_bit;
    while (piece_moves)
        *move_list++ = Move(m_king_position[not m_turn], popLeastSignificantBit(piece_moves));
    return move_list;
}
Move *BitPosition::kingAllMovesInCheck(Move *&move_list) const
{
    uint64_t king_moves{precomputed_moves::king_moves[m_king_position[not m_turn]] & ~m_pieces_bit[not m_turn]};
    while (king_moves)
        *move_list++ = Move(m_king_position[not m_turn], popLeastSignificantBit(king_moves));

    return move_list;
}

// Captures in check (for Quiesence)
Move *BitPosition::inCheckOrderedCaptures(Move *&move_list) const
// Only called if m_num_checks = 1
{
    // King captures
    uint64_t piece_moves = precomputed_moves::king_moves[m_king_position[not m_turn]] & m_pieces_bit[m_turn];
    while (piece_moves)
    {
        *move_list++ = Move(m_king_position[not m_turn], popLeastSignificantBit(piece_moves));
    }
    // Pawn captures from checking position
    piece_moves = precomputed_moves::pawn_attacks[m_turn][m_check_square] & m_pieces[not m_turn][0];
    while (piece_moves)
    {
        if (!((1ULL << m_check_square) & promotion_ranks[not m_turn])) // Non promotions
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
        }
        else // Promotions
        {
            *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square, 3);
        }
    }
    // Knight captures from checking position
    piece_moves = precomputed_moves::knight_moves[m_check_square] & (m_pieces[not m_turn][1] & ~state_info->pinnedPieces);
    while (piece_moves)
    {
        *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
    }
    // Bishop captures from checking position
    piece_moves = BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) & m_pieces[not m_turn][2];
    while (piece_moves)
    {
        *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
    }
    // Rook captures from checking position
    piece_moves = RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit) & m_pieces[not m_turn][3];
    while (piece_moves)
    {
        *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
    }
    // Queen captures from checking position
    piece_moves = (BmagicNOMASK(m_check_square, precomputed_moves::bishop_unfull_rays[m_check_square] & m_all_pieces_bit) | RmagicNOMASK(m_check_square, precomputed_moves::rook_unfull_rays[m_check_square] & m_all_pieces_bit)) & m_pieces[not m_turn][4];
    while (piece_moves)
    {
        *move_list++ = Move(popLeastSignificantBit(piece_moves), m_check_square);
    }
    return move_list;
}

// Moving pieces functions
bool BitPosition::moveIsReseter(Move move)
// Return if move is a capture
{
    uint64_t destination_bit{1ULL << move.getDestinationSquare()};
    uint64_t origin_bit{1ULL << move.getDestinationSquare()};
    if ((m_pieces_bit[m_turn] & destination_bit)) // Capture
        return true;
    if ((m_pieces[not m_turn][0] & origin_bit) || (state_info->pSquare) == destination_bit) // Passant Capture
        return true;
    return false;
}

bool BitPosition::isDraw() const
{
    assert(state_info);                           // current node must exist
    assert(state_info->reversibleMovesMade >= 0); // no negative counters

    // Threefold repetitions can only happen if there where at least 8 reversible moves made
    if (state_info->reversibleMovesMade < 8)
        return false;
    // 50 move rule
    if (state_info->reversibleMovesMade > 99)
        return true;

    // We start from 4 plies back since making two moves cant repeat the position (at least 4 are needed)
    // This is safe since we know that the position is at least 8 moves old
    const StateInfo *stp = state_info->previous->previous;
    int pliesBack = 4;

    int repetitions = 0;
    while (stp && pliesBack <= state_info->reversibleMovesMade)
    {
        /* We’re about to jump back two more plies – make sure they exist   */
        assert(stp->previous && stp->previous->previous &&
               "StateInfo chain broken during three-fold scan");

        stp = stp->previous->previous;

        if (stp->zobristKey == state_info->zobristKey)
            if (++repetitions == 2)
                return true;

        pliesBack += 2;
    }

    return false;
}

template <typename T>
NNUEU::NNUEUChange BitPosition::makeMove(T move, StateInfo &new_state_info)
// Move piece and switch white and black roles, without rotating the board.
// The main difference with makeCapture is that we set blockers and pins here when making a move
{
#ifndef NDEBUG // DEBUG
    #ifdef VERBOSE_DEBUG
        // std::cout << "Making move " << move.toString() << "\n";
    #endif
    if (!moveIsFine(move)) // DEBUG
    {
        std::cerr << "Assertion failed in makeMove: moveIsFine(move)\n";
        std::cerr << "FEN: " << toFenString() << "\n";
        std::cerr << "Move: " << move.toString() << "\n";
        assert(false);
    }
    assert(move.getData() != 0); // DEBUG
    assert(not getIsCheckOnInitialization(not m_turn)); // DEBUG
#endif
    NNUEU::NNUEUChange nnueuChanges;

    // Save irreversible aspects of position and create a new state
    // Irreversible aspects include: castlingRights, reversibleMovesMade and zobristKey
    std::memcpy(&new_state_info, state_info, offsetof(StateInfo, straightPinnedPieces));
    new_state_info.previous = state_info;
    state_info->next = &new_state_info;
    state_info = &new_state_info;

    m_blockers_set = false;

    int origin_square = move.getOriginSquare();
    uint64_t origin_bit = (1ULL << origin_square);
    int destination_square = move.getDestinationSquare();
    uint64_t destination_bit = 1ULL << destination_square;
    int captured_piece;
    m_promoted_piece = 7; // Representing no promotion (Used for updating check info)
    state_info->reversibleMovesMade++;
    bool isPassant = false;

    m_all_pieces_bit &= ~origin_bit;
    m_all_pieces_bit |= destination_bit;
    m_pieces_bit[not m_turn] ^= (origin_bit | destination_bit);

    m_moved_piece = m_board[not m_turn][origin_square];
    captured_piece = m_board[m_turn][destination_square];

    assert(m_moved_piece != 7);

    m_board[not m_turn][origin_square] = 7;
    m_board[not m_turn][destination_square] = m_moved_piece;
    m_board[m_turn][destination_square] = 7;

    if (m_moved_piece == 5) // Moving king
    {
        // Update king bit and king position
        m_pieces[not m_turn][5] = destination_bit;
        m_king_position[not m_turn] = destination_square;

        state_info->isCheck = isDiscoverCheck(origin_square, destination_square);

        // Castling
        if (move.getData() == 16772) // White kingside castling
        {
            state_info->reversibleMovesMade = 0; // Move is irreversible
            m_pieces[0][3] &= ~128;
            m_all_pieces_bit &= ~128;
            m_pieces_bit[0] &= ~128;
            m_pieces[0][3] |= 32;
            m_all_pieces_bit |= 32;
            m_pieces_bit[0] |= 32;

            m_board[0][7] = 7;
            m_board[0][5] = 3;

            state_info->zobristKey ^= zobrist_keys::pieceZobristNumbers[0][3][7] ^ zobrist_keys::pieceZobristNumbers[0][3][5];

            // Direct check
            state_info->isCheck = state_info->previous->checkBits[3] & 32;
            // Set NNUEU input
            nnueuChanges.add(64 * 3 + 5, 64 * 3 + 7);
        }
        else if (move.getData() == 16516) // White queenside castling
        {
            state_info->reversibleMovesMade = 0; // Move is irreversible
            m_pieces[0][3] &= ~1;
            m_all_pieces_bit &= ~1;
            m_pieces_bit[0] &= ~1;
            m_pieces[0][3] |= 8;
            m_all_pieces_bit |= 8;
            m_pieces_bit[0] |= 8;

            m_board[0][0] = 7;
            m_board[0][3] = 3;

            state_info->zobristKey ^= zobrist_keys::pieceZobristNumbers[0][3][0] ^ zobrist_keys::pieceZobristNumbers[0][3][3];

            // Direct check
            state_info->isCheck = state_info->previous->checkBits[3] & 8;
            // Set NNUEU input
            nnueuChanges.add(64 * 3 + 3, 64 * 3);
        }
        else if (move.getData() == 20412) // Black kingside castling
        {
            state_info->reversibleMovesMade = 0; // Move is irreversible
            // Direct check
            state_info->isCheck = state_info->previous->checkBits[3] & 2305843009213693952ULL;

            m_pieces[1][3] &= ~9223372036854775808ULL;
            m_all_pieces_bit &= ~9223372036854775808ULL;
            m_pieces_bit[1] &= ~9223372036854775808ULL;
            m_pieces[1][3] |= 2305843009213693952ULL;
            m_all_pieces_bit |= 2305843009213693952ULL;
            m_pieces_bit[1] |= 2305843009213693952ULL;

            m_board[1][63] = 7;
            m_board[1][61] = 3;

            state_info->zobristKey ^= zobrist_keys::pieceZobristNumbers[1][3][63] ^ zobrist_keys::pieceZobristNumbers[1][3][61];
            // Set NNUEU input
            nnueuChanges.add(64 * 8 + 61, 64 * 8 + 63);
        }
        else if (move.getData() == 20156) // Black queenside castling
        {
            state_info->reversibleMovesMade = 0; // Move is irreversible
            // Direct check
            state_info->isCheck = state_info->previous->checkBits[3] & 576460752303423488ULL;

            m_pieces[1][3] &= ~72057594037927936ULL;
            m_all_pieces_bit &= ~72057594037927936ULL;
            m_pieces_bit[1] &= ~72057594037927936ULL;
            m_pieces[1][3] |= 576460752303423488ULL;
            m_all_pieces_bit |= 576460752303423488ULL;
            m_pieces_bit[1] |= 576460752303423488ULL;

            m_board[1][56] = 7;
            m_board[1][59] = 3;

            state_info->zobristKey ^= zobrist_keys::pieceZobristNumbers[1][3][56] ^ zobrist_keys::pieceZobristNumbers[1][3][59];
            // Set NNUEU input
            nnueuChanges.add(64 * 8 + 59, 64 * 8 + 56);
        }
    }
    else if (m_moved_piece == 0) // Moving Pawn
    {
        m_pieces[not m_turn][0] ^= (origin_bit | destination_bit);

        // Checks
        state_info->isCheck = state_info->previous->checkBits[0] & destination_bit;
        // Discover checks
        if (not state_info->isCheck)
            state_info->isCheck = isDiscoverCheck(origin_square, destination_square);

        state_info->reversibleMovesMade = 0; // Move is irreversible

        // Set NNUEU input
        nnueuChanges.add(NNUE_BASE[not m_turn][0] + destination_square,
                         NNUE_BASE[not m_turn][0] + origin_square);

        if (destination_bit & promotion_ranks[not m_turn]) // Promotions
        {
            m_pieces[not m_turn][0] &= ~destination_bit;

            m_promoted_piece = move.getPromotingPiece() + 1;
            m_pieces[not m_turn][m_promoted_piece] |= destination_bit;

            m_board[not m_turn][destination_square] = m_promoted_piece;
            // Direct check and discover check
            if (not state_info->isCheck)
                state_info->isCheck = state_info->previous->checkBits[m_promoted_piece] & destination_bit || isPromotionCheck(m_promoted_piece, destination_square);
            // Set NNUEU input
            nnueuChanges.add(NNUE_BASE[not m_turn][m_promoted_piece] + destination_square,
                             NNUE_BASE[not m_turn][0] + origin_square);

            state_info->zobristKey ^= zobrist_keys::pieceZobristNumbers[not m_turn][m_moved_piece][destination_square] ^ zobrist_keys::pieceZobristNumbers[not m_turn][m_promoted_piece][destination_square];
        }
        else if (destination_square == state_info->previous->pSquare) // Passant
        {
            m_pieces[m_turn][0] &= ~shift_forward[m_turn](destination_bit);
            m_all_pieces_bit &= ~shift_forward[m_turn](destination_bit);
            m_pieces_bit[m_turn] &= ~shift_forward[m_turn](destination_bit);
            m_board[m_turn][destination_square + pawn_move_offsets[not m_turn]] = 7;

            captured_piece = 0;

            if (not(state_info->isCheck))
                state_info->isCheck = isDiscoverCheckAfterPassant();

            // Set NNUEU input
            nnueuChanges.addlast(NNUE_BASE[m_turn][0] + destination_square + pawn_move_offsets[not m_turn]);
            isPassant = true;

            state_info->zobristKey ^= zobrist_keys::pieceZobristNumbers[m_turn][0][destination_square + pawn_move_offsets[not m_turn]];
        }
    }
    // Moving any piece except king or pawn
    else
    {
        m_pieces[not m_turn][m_moved_piece] ^= (origin_bit | destination_bit);

        // Checks
        state_info->isCheck = state_info->previous->checkBits[m_moved_piece] & destination_bit;
        // Discover checks
        if (not state_info->isCheck)
            state_info->isCheck = isDiscoverCheck(origin_square, destination_square);

        // Set NNUEU input
        nnueuChanges.add(NNUE_BASE[not m_turn][m_moved_piece] + destination_square,
                         NNUE_BASE[not m_turn][m_moved_piece] + origin_square);
    }
    // Captures (Non passant)
    if (captured_piece != 7 && not isPassant)
    {
        m_pieces[m_turn][captured_piece] &= ~destination_bit;
        m_pieces_bit[m_turn] &= ~destination_bit;
        state_info->reversibleMovesMade = 0; // Move is irreversible
        // Set NNUEU input
        nnueuChanges.addlast(NNUE_BASE[m_turn][captured_piece] + destination_square);

        state_info->zobristKey ^= zobrist_keys::pieceZobristNumbers[m_turn][captured_piece][destination_square];

        uint8_t mask = castlingMask[destination_square];
        if (mask != 0)
        {
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[state_info->castlingRights];
            state_info->castlingRights &= ~mask;
            state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[state_info->castlingRights];
        }
    }

    state_info->zobristKey ^= zobrist_keys::passantSquaresZobristNumbers[state_info->previous->pSquare];

    // Updating passant square
    if (m_moved_piece == 0 && (destination_square - origin_square) == double_pawn_move_offsets[m_turn])
        state_info->pSquare = origin_square + pawn_move_offsets[m_turn];
    else
        state_info->pSquare = 0;

    state_info->zobristKey ^= zobrist_keys::passantSquaresZobristNumbers[state_info->pSquare];
    
    state_info->capturedPiece = captured_piece;

    state_info->zobristKey ^= zobrist_keys::pieceZobristNumbers[not m_turn][m_moved_piece][origin_square] ^ zobrist_keys::pieceZobristNumbers[not m_turn][m_moved_piece][destination_square];
    state_info->zobristKey ^= zobrist_keys::blackToMoveZobristNumber;
    // Update castling rights
    uint8_t mask = castlingMask[origin_square];
    if (mask != 0)
    {
        state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[state_info->castlingRights];
        state_info->castlingRights &= ~mask;
        state_info->zobristKey ^= zobrist_keys::castlingRightsZobristNumbers[state_info->castlingRights];
    }
    m_turn = not m_turn;

    // Note, we store the zobrist key in it's array when making the move. However the rest of the ply info
    // is stored when making the next move to be able to go back.
    // So we store it in the m_ply+1 position because the initial position (or position after capture) is the m_ply 0.
    m_ply++;
#ifndef NDEBUG // DEBUG
        if (!posIsFine()) // DEBUG
        {
            std::cerr << "Assertion failed in makeMove: posIsFine()\n";
            std::cerr << "FEN: " << toFenString() << "\n";
            std::cerr << "Move: " << move.toString() << "\n";
            assert(false);
        }
        if (isKingInCheck(m_turn)) // DEBUG
        {
            std::cerr << "Assertion failed in makeMove: !isKingInCheck(m_turn)\n";
            std::cerr << "FEN: " << toFenString() << "\n";
            std::cerr << "Move: " << move.toString() << "\n";
            assert(false);
        }
        if (getIsCheckOnInitialization(m_turn) != state_info->isCheck) // DEBUG
        {
            std::cerr << "Assertion failed in makeMove: getIsCheckOnInitialization(m_turn) == state_info->isCheck\n";
            std::cerr << "FEN: " << toFenString() << "\n";
            std::cerr << "Move: " << move.toString() << "\n";
            assert(false);
        }
        // If the castling rights are on, the king must be on initial square
        if (not m_turn && (state_info->castlingRights & 0b0011) != 0) // Move we did was white's
        {
            int king_square = m_king_position[m_turn];
            if (king_square != 4)
            {
                std::cerr << "Assertion failed in makeMove: White castling rights are active but king is not on initial square\n";
                std::cerr << "FEN: " << toFenString() << "\n";
                std::cerr << "Move: " << move.toString() << "\n";
                assert(false);
            }
        }
        else if (m_turn && (state_info->castlingRights & 0b1100) != 0) // Move we did was black's
        {
            int king_square = m_king_position[m_turn];
            if (king_square != 60)
            {
                std::cerr << "Assertion failed in makeMove: Black castling rights are active but king is not on initial square\n";
                std::cerr << "FEN: " << toFenString() << "\n";
                std::cerr << "Move: " << move.toString() << "\n";
                assert(false);
            }
        }
        
        assert(computeFullZobristKey() == state_info->zobristKey);
#endif
    return nnueuChanges;
}
template <typename T>
void BitPosition::unmakeMove(T move)
// Takes a move and undoes the move accordingly, updating all position attributes. When the engine transverses the tree of moves it will keep
// track of some irreversible aspects of the game at each ply.These are(white castling rights, black castling rights, passant square,
// capture index, pins, checks).
{
#ifndef NDEBUG
    #ifdef VERBOSE_DEBUG
        // std::cout << "Unmaking move " << move.toString() << "\n";
    #endif
#endif
    // If a move was made before, that means previous position had blockers set (which we restore from ply info)
    m_blockers_set = true;

    m_ply--;
    int previous_captured_piece{state_info->capturedPiece};

    state_info = state_info->previous;

    // Get move info
    int origin_square{move.getOriginSquare()};
    uint64_t origin_bit{1ULL << origin_square};
    int destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{1ULL << destination_square};

    m_all_pieces_bit &= ~destination_bit;
    m_all_pieces_bit |= origin_bit;
    m_pieces_bit[m_turn] ^= (origin_bit | destination_bit);
    int moved_piece = m_board[m_turn][destination_square];
    // Castling, Passant and promotions
    if (move.isSpecial())
    {
        // Unmake kingside castling
        if (move.getData() == 20412)
        {
            // Rook
            m_pieces[m_turn][3] |= (1ULL << 63);
            m_all_pieces_bit |= (1ULL << 63);
            m_pieces_bit[m_turn] |= (1ULL << 63);
            m_pieces[m_turn][3] &= ~(1ULL << 61);
            m_all_pieces_bit &= ~(1ULL << 61);
            m_pieces_bit[m_turn] &= ~(1ULL << 61);

            // King
            m_pieces[m_turn][5] = (1ULL << 60);
            m_king_position[m_turn] = 60;

            m_board[m_turn][63] = 3;
            m_board[m_turn][61] = 7;
        }

        // Unmake queenside castling
        else if (move.getData() == 20156)
        {
            // Rook
            m_pieces[m_turn][3] |= (1ULL << 56);
            m_all_pieces_bit |= (1ULL << 56);
            m_pieces_bit[m_turn] |= (1ULL << 56);
            m_pieces[m_turn][3] &= ~(1ULL << 59);
            m_all_pieces_bit &= ~(1ULL << 59);
            m_pieces_bit[m_turn] &= ~(1ULL << 59);

            // King
            m_pieces[m_turn][5] = (1ULL << 60);
            m_king_position[m_turn] = 60;

            m_board[m_turn][56] = 3;
            m_board[m_turn][59] = 7;
        }
        // Unmake kingside castling
        else if (move.getData() == 16772)
        {
            // Rook
            m_pieces[m_turn][3] |= (1ULL << 7);
            m_pieces_bit[m_turn] |= (1ULL << 7);
            m_all_pieces_bit |= (1ULL << 7);
            m_pieces[m_turn][3] &= ~(1ULL << 5);
            m_pieces_bit[m_turn] &= ~(1ULL << 5);
            m_all_pieces_bit &= ~(1ULL << 5);

            // King
            m_pieces[m_turn][5] = (1ULL << 4);
            m_king_position[m_turn] = 4;

            m_board[m_turn][7] = 3;
            m_board[m_turn][5] = 7;
        }
        // Unmake queenside castling
        else if (move.getData() == 16516)
        {
            // Rook
            m_pieces[m_turn][3] |= 1ULL;
            m_pieces_bit[m_turn] |= 1ULL;
            m_all_pieces_bit |= 1ULL;
            m_pieces[m_turn][3] &= ~(1ULL << 3);
            m_pieces_bit[m_turn] &= ~(1ULL << 3);
            m_all_pieces_bit &= ~(1ULL << 3);

            // King
            m_pieces[m_turn][5] = (1ULL << 4);
            m_king_position[m_turn] = 4;

            m_board[m_turn][0] = 3;
            m_board[m_turn][3] = 7;
        }

        // Unmaking promotions
        else if (destination_bit & promotion_ranks[m_turn])
        {
            moved_piece = 0;

            m_pieces[m_turn][0] |= origin_bit;
            m_pieces[m_turn][move.getPromotingPiece() + 1] &= ~destination_bit;

            // Unmaking captures in promotions
            if (previous_captured_piece != 7)
            {
                m_pieces[not m_turn][previous_captured_piece] |= destination_bit;
                m_pieces_bit[not m_turn] |= destination_bit;
                m_all_pieces_bit |= destination_bit;
            }
            m_board[not m_turn][destination_square] = previous_captured_piece;
        }
        else // Passant
        {
            m_pieces[m_turn][0] |= origin_bit;
            m_pieces[m_turn][0] &= ~destination_bit;

            // Restore captured pawn
            m_pieces[not m_turn][0] |= shift_forward[not m_turn](destination_bit);
            m_pieces_bit[not m_turn] |= shift_forward[not m_turn](destination_bit);
            m_all_pieces_bit |= shift_forward[not m_turn](destination_bit);
            m_board[not m_turn][destination_square + pawn_move_offsets[m_turn]] = 0;
        }
    }
    // Non special moves
    else
    {
        if (moved_piece == 5) // Unmove king
        {
            m_pieces[m_turn][5] = origin_bit;
            m_king_position[m_turn] = origin_square;
            // moveBlackKingNNUEInput();
        }
        else // Unmove any other piece
        {
            m_pieces[m_turn][moved_piece] |= origin_bit;
            m_pieces[m_turn][moved_piece] &= ~destination_bit;
        }
        // Unmaking captures
        if (previous_captured_piece != 7)
        {
            m_pieces[not m_turn][previous_captured_piece] |= destination_bit;
            m_pieces_bit[not m_turn] |= destination_bit;
            m_all_pieces_bit |= destination_bit;
        }
        m_board[not m_turn][destination_square] = previous_captured_piece;
    }
    m_board[m_turn][destination_square] = 7;
    m_board[m_turn][origin_square] = moved_piece;

    m_turn = not m_turn;

#ifndef NDEBUG // DEBUG
    if (!posIsFine()) // DEBUG
    {
        std::cerr << "Assertion failed in unmakeMove: posIsFine()\n";
        std::cerr << "FEN: " << toFenString() << "\n";
        std::cerr << "Move: " << move.toString() << "\n";
        assert(false);
    }
    if (isKingInCheck(m_turn)) // DEBUG
    {
        std::cerr << "Assertion failed in unmakeMove: !isKingInCheck(m_turn)\n";
        std::cerr << "FEN: " << toFenString() << "\n";
        std::cerr << "Move: " << move.toString() << "\n";
        assert(false);
    }
    if (getIsCheckOnInitialization(m_turn) != state_info->isCheck) // DEBUG
    {
        std::cerr << "Assertion failed in unmakeMove: getIsCheckOnInitialization(m_turn) == state_info->isCheck\n";
        std::cerr << "FEN: " << toFenString() << "\n";
        std::cerr << "Move: " << move.toString() << "\n";
        assert(false);
    }
#endif
}

template <typename T>
NNUEU::NNUEUChange BitPosition::makeCapture(T move, StateInfo &new_state_info)
// Captures and queen promotions
{
#ifndef NDEBUG // DEBUG
    #ifdef VERBOSE_DEBUG
        // std::cout << "Making capture " << move.toString() << "\n";
    #endif
    if (!moveIsFine(move)) // DEBUG
    {
        std::cerr << "Assertion failed in makeCapture: moveIsFine(move)\n";
        std::cerr << "FEN: " << toFenString() << "\n";
        std::cerr << "Move: " << move.toString() << "\n";
        assert(false);
    }
    assert(move.getData() != 0); // DEBUG
    assert(not getIsCheckOnInitialization(not m_turn)); // DEBUG

    std::memcpy(&new_state_info, state_info, offsetof(StateInfo, straightPinnedPieces));
#endif
    NNUEU::NNUEUChange nnueuChanges;
    new_state_info.previous = state_info;
    state_info->next = &new_state_info;
    state_info = &new_state_info;

    state_info->pSquare = 0;

    m_blockers_set = false;

    int origin_square = move.getOriginSquare();
    uint64_t origin_bit = 1ULL << origin_square;
    int destination_square = move.getDestinationSquare();
    uint64_t destination_bit = 1ULL << destination_square;
    int captured_piece;

    m_all_pieces_bit &= ~origin_bit;
    m_all_pieces_bit |= destination_bit;
    m_pieces_bit[not m_turn] ^= (origin_bit | destination_bit);
    m_pieces_bit[m_turn] &= ~destination_bit;


    m_moved_piece = m_board[not m_turn][origin_square];
    captured_piece = m_board[m_turn][destination_square];
    assert(m_moved_piece != 7); // DEBUG: Moved piece must be a piece (not empty square or own piece)

    // Promotions (always to queen in captures)
    if (move.isSpecial())
    {
        m_pieces[not m_turn][0] &= ~origin_bit;
        m_pieces[not m_turn][4] |= destination_bit;

        // Set NNUE input
        nnueuChanges.add(NNUE_BASE[not m_turn][4] + destination_square,
                         NNUE_BASE[not m_turn][0] + origin_square);

        // Captures (Non passant)
        if (captured_piece != 7)
        {
            m_pieces[m_turn][captured_piece] &= ~destination_bit;
            // Set NNUE input
            nnueuChanges.addlast(NNUE_BASE[m_turn][captured_piece] + destination_square);
            m_board[m_turn][destination_square] = 7;
        }
        m_board[not m_turn][origin_square] = 7;
        m_board[not m_turn][destination_square] = 4;

        // Direct or discover checks
        state_info->isCheck = isQueenCheck(destination_square) or isDiscoverCheck(origin_square, destination_square);
    }
    else // Normal captures
    {
        assert(captured_piece != 7); // Captured piece must be a piece (not empty square or own piece)
        if (m_moved_piece == 5) // Moving king
        {
            // Update king bit and king position
            m_pieces[not m_turn][5] = destination_bit;
            m_king_position[not m_turn] = destination_square;

            // Discover checks
            state_info->isCheck = isDiscoverCheck(origin_square, destination_square);
        }
        // Moving any piece except king
        else
        {
            m_pieces[not m_turn][m_moved_piece] ^= (origin_bit | destination_bit);

            // Checks
            state_info->isCheck = state_info->previous->checkBits[m_moved_piece] & destination_bit;
            if (not state_info->isCheck)
                state_info->isCheck = isDiscoverCheck(origin_square, destination_square);

            // Set NNUE input
            nnueuChanges.add(NNUE_BASE[not m_turn][m_moved_piece] + destination_square,
                             NNUE_BASE[not m_turn][m_moved_piece] + origin_square);
        }
        // Captures (Non passant)
        m_pieces[m_turn][captured_piece] &= ~destination_bit;
        // Set NNUE input
        nnueuChanges.addlast(NNUE_BASE[m_turn][captured_piece] + destination_square);

        m_board[not m_turn][origin_square] = 7;
        m_board[not m_turn][destination_square] = m_moved_piece;
        m_board[m_turn][destination_square] = 7;
    }
    
#ifndef NDEBUG // DEBUG
    // Clear bits if moving from or moving to a rook corner square (we dont need to do this in release mode since in quiescence search
    // we dont care about castling rights)
    {
        uint8_t mask = castlingMask[destination_square] | castlingMask[origin_square];
        if (mask)
            state_info->castlingRights &= ~mask;
    }
#endif
    m_turn = not m_turn;
    state_info->capturedPiece = captured_piece;
    m_ply++;
    state_info->zobristKey = computeFullZobristKey();
    assert(posIsFine());
    assert(!isKingInCheck(m_turn));
    assert(getIsCheckOnInitialization(m_turn) == state_info->isCheck);
    return nnueuChanges;
}
template <typename T>
void BitPosition::unmakeCapture(T move)
{
#ifndef NDEBUG // DEBUG
    #ifdef VERBOSE_DEBUG
        // std::cout << "Unmaking capture: " << move.toString() << "\n";
    #endif
#endif
    // If a move was made before, that means previous position had blockers set (which we restore from ply info)
    m_blockers_set = true;

    m_ply--;

    int previous_captured_piece{state_info->capturedPiece};

    // Update irreversible aspects
    state_info = state_info->previous;

    // Get move info
    int origin_square{move.getOriginSquare()};
    uint64_t origin_bit{1ULL << origin_square};
    int destination_square{move.getDestinationSquare()};
    uint64_t destination_bit{1ULL << destination_square};

    m_all_pieces_bit |= origin_bit;
    m_pieces_bit[m_turn] ^= (origin_bit | destination_bit);
    m_pieces_bit[not m_turn] |= destination_bit;


    int moved_piece = m_board[m_turn][destination_square];
    // Promotions
    if (move.isSpecial())
    {
        moved_piece = 0;
        m_pieces[m_turn][0] |= origin_bit;
        m_pieces[m_turn][4] &= ~destination_bit;

        // Unmaking captures in promotions
        if (previous_captured_piece != 7)
        {
            m_pieces[not m_turn][previous_captured_piece] |= destination_bit;
        }
        else
        {
            m_pieces_bit[not m_turn] &= ~destination_bit;
            m_all_pieces_bit &= ~destination_bit;
        }
    }
    // Non promotions
    else
    {
        if (moved_piece == 5) // Unmove king
        {
            m_pieces[m_turn][5] = origin_bit;
            m_king_position[m_turn] = origin_square;
        }
        else
        {
            m_pieces[m_turn][moved_piece] |= origin_bit;
            m_pieces[m_turn][moved_piece] &= ~destination_bit;
        }
        // Unmaking captures
        m_pieces[not m_turn][previous_captured_piece] |= destination_bit;
    }
    m_board[m_turn][destination_square] = 7;
    m_board[m_turn][origin_square] = moved_piece;
    m_board[not m_turn][destination_square] = previous_captured_piece;
    
    m_turn = not m_turn;
#ifndef NDEBUG // DEBUG
    if (!posIsFine()) // DEBUG
    {
        std::cerr << "Assertion failed in unmakeCapture: posIsFine()\n";
        std::cerr << "FEN: " << toFenString() << "\n";
        std::cerr << "Move: " << move.toString() << "\n";
        assert(false);
    }
    if (isKingInCheck(m_turn)) // DEBUG
    {
        std::cerr << "Assertion failed in unmakeCapture: !isKingInCheck(m_turn)\n";
        std::cerr << "FEN: " << toFenString() << "\n";
        std::cerr << "Move: " << move.toString() << "\n";
        assert(false);
    }
    if (getIsCheckOnInitialization(m_turn) != state_info->isCheck) // DEBUG
    {
        std::cerr << "Assertion failed in unmakeCapture: getIsCheckOnInitialization(m_turn) == state_info->isCheck\n";
        std::cerr << "FEN: " << toFenString() << "\n";
        std::cerr << "Move: " << move.toString() << "\n";
        assert(false);
    }
#endif
}

// Game ending functions
bool BitPosition::isMate() const
// This is called only in quiesence search, after having no available captures
{
    // Only consider empty squares because we only call this function if there are no captures
    if (m_num_checks == 1) // Can we block with pieces?
    {
        // Knight block
        uint64_t piece_moves = m_pieces[not m_turn][1] & ~state_info->pinnedPieces;
        while (piece_moves)
        {
            if (precomputed_moves::knight_moves[popLeastSignificantBit(piece_moves)] & m_check_rays)
                return false;
        }

        // Single move pawn block
        uint64_t single_pawn_advances = shift_forward[not m_turn](m_pieces[not m_turn][0]) & ~m_all_pieces_bit;
        piece_moves = single_pawn_advances & m_check_rays;
        while (piece_moves)
        {
            int destination = popLeastSignificantBit(piece_moves);
            if (isNormalMoveLegal(destination + pawn_move_offsets[not m_turn], destination))
                return false;
        }
        // Double move pawn block
        piece_moves = shift_forward[not m_turn](single_pawn_advances) & m_check_rays;
        while (piece_moves)
        {
            int destination = popLeastSignificantBit(piece_moves);
            if (isNormalMoveLegal(destination + double_pawn_move_offsets[not m_turn], destination))
                return false;
        }

        // Rook/Queen block
        piece_moves = m_pieces[not m_turn][3] | m_pieces[not m_turn][4];
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
        // Bishop/Queen block
        piece_moves = m_pieces[not m_turn][2] | m_pieces[not m_turn][4];
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
    }
    //  Can we move king?
    uint64_t piece_moves{precomputed_moves::king_moves[m_king_position[not m_turn]] & ~m_all_pieces_bit};
    while (piece_moves)
    {
        if (newKingSquareIsSafe(popLeastSignificantBit(piece_moves)))
            return false;
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
        uint64_t single_pawn_moves_bit{shift_up(m_pieces[0][0] & ~(state_info->diagonalPinnedPieces)) & ~m_all_pieces_bit};
        uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
        while (single_pawn_moves_blocking_bit)
        {
            int destination = popLeastSignificantBit(single_pawn_moves_blocking_bit);
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
            for (int origin : getBitIndices(precomputed_moves::pawn_attacks[1][(state_info->pSquare)] & m_pieces[0][0]))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    else
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_pieces[1][0] & ~(state_info->diagonalPinnedPieces)) & ~m_all_pieces_bit};
        uint64_t single_pawn_moves_blocking_bit{single_pawn_moves_bit & m_check_rays};
        while(single_pawn_moves_blocking_bit)
        {
            int destination = popLeastSignificantBit(single_pawn_moves_blocking_bit);
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
            for (int origin : getBitIndices(precomputed_moves::pawn_attacks[0][(state_info->pSquare)] & m_pieces[1][0]))
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
        uint64_t piece_bits = precomputed_moves::pawn_attacks[1][m_check_square] & m_pieces[0][0];
        // Pawn captures from checking position
        while (piece_bits)
        {
            int origin = popLeastSignificantBit(piece_bits);
            if (m_check_square < 56) // Non promotions
            {
                continue;
            }
            else // Promotions (non queen)
            {
                *move_list++ = Move(origin, m_check_square, 0);
                *move_list++ = Move(origin, m_check_square, 1);
                *move_list++ = Move(origin, m_check_square, 2);
            }
        }
    }
    else
    {
        uint64_t piece_bits = precomputed_moves::pawn_attacks[1][m_check_square] & m_pieces[1][0];
        // Pawn captures from checking position
        while (piece_bits)
        {
            int origin = popLeastSignificantBit(piece_bits);
            if (m_check_square > 7) // Non promotions
            {
                continue;
            }
            else // Promotions (non queen)
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
        if (state_info->pSquare)
        {
            uint64_t piece_bits = precomputed_moves::pawn_attacks[1][state_info->pSquare] & m_pieces[0][0];
            while (piece_bits)
            {
                int origin = popLeastSignificantBit(piece_bits);
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    *move_list++ = Move(origin, state_info->pSquare, 0);
                }
            }
        }
    }
    else
    {
        // Passant block or capture
        if (state_info->pSquare)
        {
            uint64_t piece_bits = precomputed_moves::pawn_attacks[0][state_info->pSquare] & m_pieces[1][0];
            while (piece_bits)
            {
                int origin = popLeastSignificantBit(piece_bits);
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8)) // Legal
                {
                    *move_list++ = Move(origin, state_info->pSquare, 0);
                }
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
        uint64_t single_pawn_moves_bit{shift_up(m_pieces[0][0] & ~(state_info->diagonalPinnedPieces)) & ~m_all_pieces_bit};
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
        for (int destination : getBitIndices(shift_up_right(m_pieces[0][0] & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_pieces_bit[m_turn] & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 9, destination, 0);
            *move_list++ = Move(destination - 9, destination, 1);
            *move_list++ = Move(destination - 9, destination, 2);
        }
        // Left shift captures and non-queen promotions
        for (int destination : getBitIndices(shift_up_left(m_pieces[0][0] & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_pieces_bit[m_turn] & EIGHT_ROW_BITBOARD))
        {
            *move_list++ = Move(destination - 7, destination, 0);
            *move_list++ = Move(destination - 7, destination, 1);
            *move_list++ = Move(destination - 7, destination, 2);
        }
        // Passant
        if (state_info->pSquare)
        {
            for (int origin : getBitIndices(precomputed_moves::pawn_attacks[m_turn][(state_info->pSquare)] & m_pieces[0][0]))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) - 8)) // Legal
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    else // Black moves
    {
        // Single moves
        uint64_t single_pawn_moves_bit{shift_down(m_pieces[1][0] & ~(state_info->diagonalPinnedPieces)) & ~m_all_pieces_bit};
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
        for (int destination : getBitIndices(shift_down_right(m_pieces[1][0] & NON_RIGHT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_pieces_bit[m_turn] & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 7, destination, 0);
            *move_list++ = Move(destination + 7, destination, 1);
            *move_list++ = Move(destination + 7, destination, 2);
        }
        // Left shift captures and non-queen promotions
        for (int destination : getBitIndices(shift_down_left(m_pieces[1][0] & NON_LEFT_BITBOARD & ~(state_info->straightPinnedPieces)) & m_pieces_bit[m_turn] & FIRST_ROW_BITBOARD))
        {
            *move_list++ = Move(destination + 9, destination, 0);
            *move_list++ = Move(destination + 9, destination, 1);
            *move_list++ = Move(destination + 9, destination, 2);
        }
        // Passant
        if (state_info->pSquare)
        {
            for (int origin : getBitIndices(precomputed_moves::pawn_attacks[m_turn][(state_info->pSquare)] & m_pieces[1][0]))
                if (kingIsSafeAfterPassant(origin, (state_info->pSquare) + 8))
                {
                    *move_list++ = Move(origin, (state_info->pSquare), 0);
                }
        }
    }
    return move_list;
}
Move *BitPosition::knightNonCaptures(Move *&move_list) const
{
    uint64_t piece_bits = m_pieces[not m_turn][1] & ~(state_info->pinnedPieces);
    while (piece_bits)
    {
        int origin = popLeastSignificantBit(piece_bits);
        uint64_t destinations = precomputed_moves::knight_moves[origin] & ~m_all_pieces_bit;
        while (destinations)
        {
            int destination = popLeastSignificantBit(destinations);
            *move_list++ = Move(origin, destination);
        }
    }
    return move_list;
}
Move *BitPosition::bishopNonCaptures(Move *&move_list) const
{
    uint64_t piece_bits = m_pieces[not m_turn][2] & ~state_info->straightPinnedPieces;
    while (piece_bits)
    {
        int origin = popLeastSignificantBit(piece_bits);
        uint64_t destinations = BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit;
        while (destinations)
        {
            int destination = popLeastSignificantBit(destinations);
            *move_list++ = Move(origin, destination);
        }
    }
    return move_list;
}
Move *BitPosition::rookNonCaptures(Move *&move_list) const
{
    uint64_t piece_bits = m_pieces[not m_turn][3] & ~state_info->diagonalPinnedPieces;
    while (piece_bits)
    {
        int origin = popLeastSignificantBit(piece_bits);
        uint64_t destinations = RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit) & ~m_all_pieces_bit;
        while (destinations)
        {
            int destination = popLeastSignificantBit(destinations);
            *move_list++ = Move(origin, destination);
        }
    }
    return move_list;
}
Move *BitPosition::queenNonCaptures(Move *&move_list) const
{
    uint64_t piece_bits = m_pieces[not m_turn][4];
    while (piece_bits)
    {
        int origin = popLeastSignificantBit(piece_bits);
        uint64_t destinations = (BmagicNOMASK(origin, precomputed_moves::bishop_unfull_rays[origin] & m_all_pieces_bit) | RmagicNOMASK(origin, precomputed_moves::rook_unfull_rays[origin] & m_all_pieces_bit)) & ~m_all_pieces_bit;
        while (destinations)
        {
            int destination = popLeastSignificantBit(destinations);
            *move_list++ = Move(origin, destination);
        }
    }
    return move_list;
}
Move *BitPosition::kingNonCaptures(Move *&move_list) const
{
    for (int destination : getBitIndices(precomputed_moves::king_moves[m_king_position[not m_turn]] & ~m_all_pieces_bit))
    {
        *move_list++ = Move(m_king_position[not m_turn], destination);
    }
    if (m_turn)
    {
        // Kingside castling
        if ((state_info->castlingRights & WHITE_KS) && (m_king_position[0] == 4) && (m_all_pieces_bit & 96) == 0)
            *move_list++ = castling_moves[0][0];
        // Queenside castling
        if ((state_info->castlingRights & WHITE_QS) && (m_king_position[0] == 4) && (m_all_pieces_bit & 14) == 0)
            *move_list++ = castling_moves[0][1];
    }
    else
    {
        // Kingside castling
        if ((state_info->castlingRights & BLACK_KS) && (m_king_position[1] == 60) && (m_all_pieces_bit & 6917529027641081856) == 0)
            *move_list++ = castling_moves[1][0];
        // Queenside castling
        if ((state_info->castlingRights & BLACK_QS) && (m_king_position[1] == 60) && (m_all_pieces_bit & 1008806316530991104) == 0)
            *move_list++ = castling_moves[1][1];
    }
    return move_list;
}

Move *BitPosition::kingNonCapturesInCheck(Move *&move_list) const
{
    for (int destination : getBitIndices(precomputed_moves::king_moves[m_king_position[not m_turn]] & ~m_all_pieces_bit))
        *move_list++ = Move(m_king_position[not m_turn], destination);

    return move_list;
}