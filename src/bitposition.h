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
    bool m_is_check{};
    // bits to represent all pieces of each player
    uint64_t m_white_pieces_bit{m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | m_white_queens_bit | m_white_king_bit};
    uint64_t m_black_pieces_bit{m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | m_black_queens_bit | m_black_king_bit};
    uint64_t m_all_pieces_bit{m_white_pieces_bit | m_black_pieces_bit};
    uint64_t m_all_pieces_bit_without_white_king{m_all_pieces_bit & ~m_white_king_bit};
    uint64_t m_all_pieces_bit_without_black_king{m_all_pieces_bit & ~m_black_king_bit};
    // unsigned short representing kings position
    unsigned short m_white_king_position{};
    unsigned short m_black_king_position{};
    // ply number
    // WE HAVE TO MAKE SURE WE RESET IT TO 0 AFTER THE ENGINE GIVES A MOVE.
    unsigned short m_ply{};
    // ply info arrays (these is the info used on non capture moves, 
    // since captured moves will all have already benn generated before unmaking move)
    std::array<bool, 150> m_wkcastling_array{};
    std::array<bool, 150> m_wqcastling_array{};
    std::array<bool, 150> m_bkcastling_array{};
    std::array<bool, 150> m_bqcastling_array{};
    std::array<unsigned short, 150> m_psquare_array{};
    std::array<uint64_t, 150> m_diagonal_pins_array{};
    std::array<uint64_t, 150> m_straight_pins_array{};

    std::array<uint64_t, 150> m_white_pawns_bits_array{};
    std::array<uint64_t, 150> m_white_knights_bits_array{};
    std::array<uint64_t, 150> m_white_bishops_bits_array{};
    std::array<uint64_t, 150> m_white_rooks_bits_array{};
    std::array<uint64_t, 150> m_white_queens_bits_array{};
    std::array<uint64_t, 150> m_white_king_bits_array{};

    std::array<uint64_t, 150> m_black_pawns_bits_array{};
    std::array<uint64_t, 150> m_black_knights_bits_array{};
    std::array<uint64_t, 150> m_black_bishops_bits_array{};
    std::array<uint64_t, 150> m_black_rooks_bits_array{};
    std::array<uint64_t, 150> m_black_queens_bits_array{};
    std::array<uint64_t, 150> m_black_king_bits_array{};

    std::array<bool, 150> m_is_check_array{};

    std::array<unsigned short, 150> m_white_king_positions_array{};
    std::array<unsigned short, 150> m_black_king_positions_array{};

public:
    // I define the short member functions here, the rest are defined in bitposition.cpp
    // Member function declarations (defined in bitposition.cpp)
    BitPosition(uint64_t white_pawns_bit, uint64_t white_knights_bit, uint64_t white_bishops_bit,
                uint64_t white_rooks_bit, uint64_t white_queens_bit, uint64_t white_king_bit,
                uint64_t black_pawns_bit, uint64_t black_knights_bit, uint64_t black_bishops_bit,
                uint64_t black_rooks_bit, uint64_t black_queens_bit, uint64_t black_king_bit, 
                bool turn, bool white_kingside_castling, bool white_queenside_castling, 
                bool black_kingside_castling, bool black_queenside_castling);
    bool isCheck();
    bool isMate();
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
        m_white_king_position = getLeastSignificantBitIndex(m_white_king_bit);
        m_black_king_position = getLeastSignificantBitIndex(m_black_king_bit);
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

    bool getIsCheck() const { return (m_is_check); }

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
    uint64_t getWhitePawnsBits() const { return m_white_pawns_bit; }
    uint64_t getWhiteKnightsBits() const { return m_white_knights_bit; }
    uint64_t getWhiteBishopsBits() const { return m_white_bishops_bit; }
    uint64_t getWhiteRooksBits() const { return m_white_rooks_bit; }
    uint64_t getWhiteQueensBits() const { return m_white_queens_bit; }
    uint64_t getWhiteKingBits() const { return m_white_king_bit; }

    uint64_t getBlackPawnsBits() const { return m_black_pawns_bit; }
    uint64_t getBlackKnightsBits() const { return m_black_knights_bit; }
    uint64_t getBlackBishopsBits() const { return m_black_bishops_bit; }
    uint64_t getBlackRooksBits() const { return m_black_rooks_bit; }
    uint64_t getBlackQueensBits() const { return m_black_queens_bit; }
    uint64_t getBlackKingBits() const { return m_black_king_bit; }
};
#endif