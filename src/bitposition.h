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
    bool m_is_check{false};
    // bits to represent all pieces of each player
    uint64_t m_white_pieces_bit{m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | m_white_queens_bit | m_white_king_bit};
    uint64_t m_black_pieces_bit{m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | m_black_queens_bit | m_black_king_bit};
    uint64_t m_all_pieces_bit{m_white_pieces_bit | m_black_pieces_bit};
    uint64_t m_all_pieces_bit_without_white_king{m_all_pieces_bit & ~m_white_king_bit};
    uint64_t m_all_pieces_bit_without_black_king{m_all_pieces_bit & ~m_black_king_bit};
    // unsigned short representing kings position
    unsigned short m_white_king_position{};
    unsigned short m_black_king_position{};

    // Used for computing attacked squares efficently
    unsigned short m_moved_piece{};
    unsigned short m_captured_piece{7};
    unsigned short m_promoted_piece{7};
    uint64_t m_last_destination_bit{};
    uint64_t m_last_origin_bit{};
    uint64_t m_move_interference_bit_on_slider{};

    uint64_t m_squares_attacked_by_white_king{};
    uint64_t m_squares_attacked_by_black_king{};
    uint64_t m_squares_attacked_by_white_pawns{};
    uint64_t m_squares_attacked_by_black_pawns{};
    uint64_t m_squares_attacked_by_white_bishops{};
    uint64_t m_squares_attacked_by_black_bishops{};
    uint64_t m_squares_attacked_by_white_knights{};
    uint64_t m_squares_attacked_by_black_knights{};
    uint64_t m_squares_attacked_by_white_rooks{};
    uint64_t m_squares_attacked_by_black_rooks{};
    uint64_t m_squares_attacked_by_white_queens{};
    uint64_t m_squares_attacked_by_black_queens{};

    uint64_t m_all_squares_attacked_by_white{};
    uint64_t m_all_squares_attacked_by_black{};

    bool m_white_bishops_attacked_squares_set{false};
    bool m_black_bishops_attacked_squares_set{false};
    bool m_white_rooks_attacked_squares_set{false};
    bool m_black_rooks_attacked_squares_set{false};
    bool m_white_queens_attacked_squares_set{false};
    bool m_black_queens_attacked_squares_set{false};
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

    std::array<uint64_t, 150> m_squares_attacked_by_white_pawns_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_black_pawns_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_white_knights_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_black_knights_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_white_bishops_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_black_bishops_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_white_rooks_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_black_rooks_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_white_queens_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_black_queens_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_white_king_array{};
    std::array<uint64_t, 150> m_squares_attacked_by_black_king_array{};

    std::array<uint64_t, 150> m_all_squares_attacked_by_white_array{};
    std::array<uint64_t, 150> m_all_squares_attacked_by_black_array{};

public:
    // I define the short member functions here, the rest are defined in bitposition.cpp
    // Member function declarations (defined in bitposition.cpp)
    BitPosition(uint64_t white_pawns_bit, uint64_t white_knights_bit, uint64_t white_bishops_bit,
                uint64_t white_rooks_bit, uint64_t white_queens_bit, uint64_t white_king_bit,
                uint64_t black_pawns_bit, uint64_t black_knights_bit, uint64_t black_bishops_bit,
                uint64_t black_rooks_bit, uint64_t black_queens_bit, uint64_t black_king_bit, 
                bool turn, bool white_kingside_castling, bool white_queenside_castling, 
                bool black_kingside_castling, bool black_queenside_castling);

    void setBlackBishopsAttackedSquares();
    void setBlackRooksAttackedSquares();
    void setBlackQueensAttackedSquares();
    void setWhiteBishopsAttackedSquares();
    void setWhiteRooksAttackedSquares();
    void setWhiteQueensAttackedSquares();
    void setBlackKnightsAttackedSquares();
    void setBlackKingAttackedSquares();
    void setBlackPawnsAttackedSquares();
    void setWhiteKnightsAttackedSquares();
    void setWhiteKingAttackedSquares();
    void setWhitePawnsAttackedSquares();
    bool isCheck() const;
    void setPinsBits();
    void setChecksAndPinsBits();
    bool kingIsSafeFromSliders(unsigned short destination_square) const;
    bool kingIsSafeAfterPassant(unsigned short removed_square_1, unsigned short removed_square_2) const;
    std::vector<Move> inCheckCaptures() const;
    std::vector<Move> inCheckAllMoves() const;
    std::vector<Move> captureMoves() const;
    std::vector<Move> allMoves() const;
    std::vector<Move> inCheckMoves() const;
    std::vector<Move> nonCaptureMoves() const;

    void setPiece(uint64_t origin_bit, uint64_t destination_bit);
    void removePiece(uint64_t destination_bit);
    void storePlyInfo();
    void makeNormalMove(Move move);
    void makeMove(Move move);
    void unmakeMove();
    void setSliderAttackedSquares();
    void setAttackedSquaresAfterMove();
    std::vector<Move> setPinsBitsReturningPinnedMoves();
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
        std::cout << "White pawns "<< m_white_pawns_bit << "\n";
        std::cout << "White knights " << m_white_knights_bit << "\n";
        std::cout << "White bishops " << m_white_bishops_bit << "\n";
        std::cout << "White rooks " << m_white_rooks_bit << "\n";
        std::cout << "White queens " << m_white_queens_bit << "\n";
        std::cout << "White king " << m_white_king_bit << "\n";

        std::cout << "Black pawns " << m_black_pawns_bit << "\n";
        std::cout << "Black knights " << m_black_knights_bit << "\n";
        std::cout << "Black bishops " << m_black_bishops_bit << "\n";
        std::cout << "Black rooks " << m_black_rooks_bit << "\n";
        std::cout << "Black queens " << m_black_queens_bit << "\n";
        std::cout << "Black king " << m_black_king_bit << "\n";

        std::cout << "psquare " << m_psquare;
    }
    void printChecksInfo() const
    {
        std::cout << "Diagonal pins " << m_diagonal_pins << "\n";
        std::cout << "Staright pins " << m_straight_pins << "\n";
        std::cout << "Pawns giving checks " << m_pawn_checks << "\n";
        std::cout << "Knights giving checks " << m_knight_checks << "\n";
        std::cout << "Bishops giving checks " << m_bishop_checks << "\n";
        std::cout << "Rooks giving checks " << m_rook_checks << "\n";
        std::cout << "Queens giving checks " << m_queen_checks << "\n";
        std::cout << "Check rays " << m_check_rays << "\n";
        std::cout << "Number of checks " << m_num_checks << "\n";
        std::cout << "Is check " << m_is_check << "\n";
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