#ifndef BITPOSITION_H
#define BITPOSITION_H
#include <array>
#include <cstdint> // For fixed sized integers
#include "bit_utils.h" // Bit utility functions
#include "move.h"
#include <iostream>
class BitPosition
{
private:
    // 64-bit to represent pieces on board
    uint64_t m_white_pawns_bit;
    uint64_t m_white_knights_bit;
    uint64_t m_white_bishops_bit;
    uint64_t m_white_rooks_bit;
    uint64_t m_white_queens_bit;
    uint64_t m_white_king_bit;
    uint64_t m_black_pawns_bit;
    uint64_t m_black_knights_bit;
    uint64_t m_black_bishops_bit;
    uint64_t m_black_rooks_bit;
    uint64_t m_black_queens_bit;
    uint64_t m_black_king_bit;
    // True white's turn, False black's
    bool m_turn;                    
    // Booleans representing castling rights
    bool m_white_kingside_castling;
    bool m_white_queenside_castling;
    bool m_black_kingside_castling;
    bool m_black_queenside_castling;
    // index of passant square a1 = 0, h8 = 63
    uint64_t m_psquare{};
    // bits representing the straight pinned squares including pinning piece's square and excluding the king's square
    uint64_t m_diagonal_pins{};
    uint64_t m_straight_pins{};
    uint64_t m_all_pins{m_diagonal_pins | m_straight_pins};
    // bits representing where the pieces giving checks are
    uint64_t m_pawn_checks{};
    uint64_t m_knight_checks{};
    uint64_t m_bishop_checks{};
    uint64_t m_rook_checks{};
    uint64_t m_queen_checks{};
    uint64_t m_check_rays{};
    unsigned short m_num_checks{};
    // bits to represent all pieces of each player
    uint64_t m_white_pieces_bit{m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | m_white_queens_bit | m_white_king_bit};
    uint64_t m_black_pieces_bit{m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | m_black_queens_bit | m_black_king_bit};
    uint64_t m_all_pieces_bit{m_white_pieces_bit | m_black_pieces_bit};
    uint64_t m_all_pieces_bit_without_white_king{m_all_pieces_bit & ~m_white_king_bit};
    uint64_t m_all_pieces_bit_without_black_king{m_all_pieces_bit & ~m_black_king_bit};
    // unsigned short representing kings position
    unsigned short m_king_position{};
    // ply number
    // WE HAVE TO MAKE SURE WE RESET IT TO 0 AFTER THE ENGINE GIVES A MOVE.
    unsigned short m_ply{};
    // ply info arrays (these is the info used on non capture moves, 
    // since captured moves will all have already benn generated before unmaking move)
    std::array<bool, 64> m_wkcastling_array{};
    std::array<bool, 64> m_wqcastling_array{};
    std::array<bool, 64> m_bkcastling_array{};
    std::array<bool, 64> m_bqcastling_array{};
    std::array<unsigned short, 64> m_psquare_array{};
    std::array<uint64_t, 64> m_diagonal_pins_array{};
    std::array<uint64_t, 64> m_straight_pins_array{};

    std::array<uint64_t, 64> m_white_pawns_bits_array{};
    std::array<uint64_t, 64> m_white_knights_bits_array{};
    std::array<uint64_t, 64> m_white_bishops_bits_array{};
    std::array<uint64_t, 64> m_white_rooks_bits_array{};
    std::array<uint64_t, 64> m_white_queens_bits_array{};
    std::array<uint64_t, 64> m_white_king_bits_array{};

    std::array<uint64_t, 64> m_black_pawns_bits_array{};
    std::array<uint64_t, 64> m_black_knights_bits_array{};
    std::array<uint64_t, 64> m_black_bishops_bits_array{};
    std::array<uint64_t, 64> m_black_rooks_bits_array{};
    std::array<uint64_t, 64> m_black_queens_bits_array{};
    std::array<uint64_t, 64> m_black_king_bits_array{};
    // Double pawn moves

    // 10 00 011000 001000 - 0 0 24 8
    // 10 00 011001 001001 - 0 0 25 9
    // 10 00 011010 001010 - 0 0 26 10
    // 10 00 011011 001011 - 0 0 27 11
    // 10 00 011100 001100 - 0 0 28 12
    // 10 00 011101 001101 - 0 0 29 13
    // 10 00 011110 001110 - 0 0 30 14
    // 10 00 011111 001111 - 0 0 31 15

    // 10 00 100000 110000 - 0 0 32 48
    // 10 00 100001 110001 - 0 0 33 49
    // 10 00 100010 110010 - 0 0 34 50
    // 10 00 100011 110011 - 0 0 35 51
    // 10 00 100100 110100 - 0 0 36 52
    // 10 00 100101 110101 - 0 0 37 53
    // 10 00 100110 110110 - 0 0 38 54
    // 10 00 100111 110111 - 0 0 39 55

public: // I define the short member functions here, the rest are defined in bitposition.cpp
    // Member function declarations (defined in bitposition.cpp)
    BitPosition(uint64_t white_pawns_bit, uint64_t white_knights_bit, uint64_t white_bishops_bit,
                uint64_t white_rooks_bit, uint64_t white_queens_bit, uint64_t white_king_bit,
                uint64_t black_pawns_bit, uint64_t black_knights_bit, uint64_t black_bishops_bit,
                uint64_t black_rooks_bit, uint64_t black_queens_bit, uint64_t black_king_bit, 
                bool turn, bool white_kingside_castling, bool white_queenside_castling, 
                bool black_kingside_castling, bool black_queenside_castling);
    bool isCheck() const;
    void setPinsBits();
    void setChecksAndPinsBits();
    bool kingIsSafeAfterPassant(unsigned short removed_square_1, unsigned short removed_square_2) const;
    bool kingIsSafe(unsigned short king_square) const;
    std::vector<Move> inCheckCaptures() const;
    std::vector<Move> inCheckMoves() const;
    std::vector<Move> captureMoves() const;
    std::vector<Move> nonCaptureMoves() const;
    void setPiece(uint64_t origin_bit, uint64_t destination_bit);
    void removePiece(uint64_t destination_bit);
    void storePlyInfo();
    void makeMove(Move move);
    void unmakeMove();
    // Member function definitions

    void setKingPosition()
    // Set the index of the king in the board.
    {
        if (m_turn == true)
            m_king_position = getLeastSignificantBitIndex(m_white_king_bit);
        else
            m_king_position = getLeastSignificantBitIndex(m_black_king_bit);
    }

    void setAllPiecesBits()
    // Function that sets the own pieces, opponent pieces and all pieces bits.
    {
        m_white_pieces_bit = (m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | m_white_queens_bit | m_white_king_bit);
        m_black_pieces_bit = (m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | m_black_queens_bit | m_black_king_bit);
        m_all_pieces_bit = (m_white_pieces_bit | m_black_pieces_bit);
        m_all_pieces_bit_without_white_king = (m_all_pieces_bit & ~m_white_king_bit);
        m_all_pieces_bit_without_black_king = (m_all_pieces_bit & ~m_black_king_bit);
    }

    bool getTurn() const { return m_turn; }

    bool getIsCheck() const { return (m_num_checks != 0); }

    void printBitboards() const
    {
        std::cout << m_white_pawns_bit << "\n";
        std::cout << m_white_knights_bit << "\n";
        std::cout << m_white_bishops_bit << "\n";
        std::cout << m_white_rooks_bit << "\n";
        std::cout << m_white_queens_bit << "\n";
        std::cout << m_white_king_bit << "\n";

        std::cout << m_black_pawns_bit << "\n";
        std::cout << m_black_knights_bit << "\n";
        std::cout << m_black_bishops_bit << "\n";
        std::cout << m_black_rooks_bit << "\n";
        std::cout << m_black_queens_bit << "\n";
        std::cout << m_black_king_bit << "\n";

        std::cout << "psquare " << m_psquare;
    }
};
#endif