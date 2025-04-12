#ifndef BITPOSITION_H
#define BITPOSITION_H
#include <array>
#include <cstdint>     // For fixed sized integers
#include "bit_utils.h" // Bit utility functions
#include "move.h"
#include <iostream>
#include <sstream>
#include <vector>
class BitPosition
{
private:
    // 64-bit to represent pieces on board
    uint64_t m_white_pawns_bit{};
    uint64_t m_white_knights_bit{};
    uint64_t m_white_bishops_bit{};
    uint64_t m_white_rooks_bit{};
    uint64_t m_white_queens_bit{};
    uint64_t m_white_king_bit{};
    uint64_t m_black_pawns_bit{};
    uint64_t m_black_knights_bit{};
    uint64_t m_black_bishops_bit{};
    uint64_t m_black_rooks_bit{};
    uint64_t m_black_queens_bit{};
    uint64_t m_black_king_bit{};
    // True white's turn, False black's
    bool m_turn{};
    // Booleans representing castling rights
    bool m_white_kingside_castling{};
    bool m_white_queenside_castling{};
    bool m_black_kingside_castling{};
    bool m_black_queenside_castling{};
    // Index of passant square a1 = 0, h8 = 63
    uint64_t m_psquare{};
    // Bits representing the pinned squares
    uint64_t m_diagonal_pins{};
    uint64_t m_straight_pins{};
    uint64_t m_all_pins{};
    // Bits representing checks
    uint64_t m_pawn_checks{};
    uint64_t m_knight_checks{};
    uint64_t m_bishop_checks{};
    uint64_t m_rook_checks{};
    uint64_t m_queen_checks{};
    uint64_t m_check_rays{};
    unsigned short m_num_checks{};
    bool m_is_check{};
    // Bits to represent all pieces of each player
    uint64_t m_white_pieces_bit{};
    uint64_t m_black_pieces_bit{};
    uint64_t m_all_pieces_bit{};
    uint64_t m_all_pieces_bit_without_white_king{};
    uint64_t m_all_pieces_bit_without_black_king{};
    // Unsigned short representing kings' positions
    unsigned short m_white_king_position{};
    unsigned short m_black_king_position{};

    // Used for computing attacked squares efficiently
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

    uint64_t m_zobrist_key{};
    // Ply number
    unsigned short m_ply{};
    // Ply info arrays
    std::array<bool, 128> m_wkcastling_array{};
    std::array<bool, 128> m_wqcastling_array{};
    std::array<bool, 128> m_bkcastling_array{};
    std::array<bool, 128> m_bqcastling_array{};
    std::array<unsigned short, 128> m_psquare_array{};
    std::array<uint64_t, 128> m_diagonal_pins_array{};
    std::array<uint64_t, 128> m_straight_pins_array{};

    std::array<bool, 128> m_is_check_array{};

    std::array<uint64_t, 128> m_squares_attacked_by_white_pawns_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_black_pawns_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_white_knights_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_black_knights_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_white_bishops_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_black_bishops_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_white_rooks_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_black_rooks_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_white_queens_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_black_queens_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_white_king_array{};
    std::array<uint64_t, 128> m_squares_attacked_by_black_king_array{};

    std::array<uint64_t, 128> m_zobrist_keys_array{};

    std::array<unsigned short, 128> m_captured_piece_array{};

    std::array<int8_t, 128> m_number_of_captures_tried_array{};
    int8_t m_number_of_captures_tried{0};

    std::array<int8_t, 128> m_current_type_capture_array{};
    int8_t m_current_type_capture{0};

    // These are for generating captures type after type
    std::vector<Capture> m_captures{};
    std::array<std::vector<Capture>, 128> m_captures_array{};

    std::array<uint64_t, 128> m_pawn_checks_array{};
    std::array<uint64_t, 128> m_knight_checks_array{};
    std::array<uint64_t, 128> m_bishop_checks_array{};
    std::array<uint64_t, 128> m_rook_checks_array{};
    std::array<uint64_t, 128> m_queen_checks_array{};
    std::array<uint64_t, 128> m_check_rays_array{};
    std::array<unsigned short, 128> m_num_checks_array{};

public:
    // I define the short member functions here, the rest are defined in bitposition.cpp
    // Member function declarations (defined in bitposition.cpp)
    BitPosition(uint64_t white_pawns_bit, uint64_t white_knights_bit, uint64_t white_bishops_bit,
                uint64_t white_rooks_bit, uint64_t white_queens_bit, uint64_t white_king_bit,
                uint64_t black_pawns_bit, uint64_t black_knights_bit, uint64_t black_bishops_bit,
                uint64_t black_rooks_bit, uint64_t black_queens_bit, uint64_t black_king_bit,
                bool turn, bool white_kingside_castling, bool white_queenside_castling,
                bool black_kingside_castling, bool black_queenside_castling)
        : m_white_pawns_bit(white_pawns_bit),
          m_white_knights_bit(white_knights_bit),
          m_white_bishops_bit(white_bishops_bit),
          m_white_rooks_bit(white_rooks_bit),
          m_white_queens_bit(white_queens_bit),
          m_white_king_bit(white_king_bit),
          m_black_pawns_bit(black_pawns_bit),
          m_black_knights_bit(black_knights_bit),
          m_black_bishops_bit(black_bishops_bit),
          m_black_rooks_bit(black_rooks_bit),
          m_black_queens_bit(black_queens_bit),
          m_black_king_bit(black_king_bit),
          m_turn(turn),
          m_white_kingside_castling(white_kingside_castling),
          m_white_queenside_castling(white_queenside_castling),
          m_black_kingside_castling(black_kingside_castling),
          m_black_queenside_castling(black_queenside_castling)
    {
        m_white_pieces_bit = m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | m_white_queens_bit | m_white_king_bit;
        m_black_pieces_bit = m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | m_black_queens_bit | m_black_king_bit;
        m_all_pieces_bit = m_white_pieces_bit | m_black_pieces_bit;
        m_all_pieces_bit_without_white_king = m_all_pieces_bit & ~m_white_king_bit;
        m_all_pieces_bit_without_black_king = m_all_pieces_bit & ~m_black_king_bit;

        setKingPosition();
        setBlackBishopsAttackedSquares();
        setBlackRooksAttackedSquares();
        setBlackQueensAttackedSquares();
        setWhiteBishopsAttackedSquares();
        setWhiteRooksAttackedSquares();
        setWhiteQueensAttackedSquares();
        setBlackKnightsAttackedSquares();
        setBlackKingAttackedSquares();
        setBlackPawnsAttackedSquares();
        setWhiteKnightsAttackedSquares();
        setWhiteKingAttackedSquares();
        setWhitePawnsAttackedSquares();

        m_all_squares_attacked_by_white = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens | m_squares_attacked_by_white_king;
        m_all_squares_attacked_by_black = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens | m_squares_attacked_by_black_king;

        initializeZobristKey();

        m_captures.reserve(16);
    }
    // Function to convert a FEN string to a BitPosition object
    BitPosition(const std::string &fen)
    {
        std::istringstream fenStream(fen);
        std::string board, turn, castling, enPassant;
        fenStream >> board >> turn >> castling >> enPassant;

        // Initialize all bitboards to 0
        m_white_pawns_bit = 0, m_white_knights_bit = 0, m_white_bishops_bit = 0, m_white_rooks_bit = 0, m_white_queens_bit = 0, m_white_king_bit = 0;
        m_black_pawns_bit = 0, m_black_knights_bit = 0, m_black_bishops_bit = 0, m_black_rooks_bit = 0, m_black_queens_bit = 0, m_black_king_bit = 0;

        int square = 56; // Start from the top-left corner of the chess board
        for (char c : board)
        {
            if (c == '/')
            {
                square -= 16; // Move to the next row
            }
            else if (isdigit(c))
            {
                square += c - '0'; // Skip empty squares
            }
            else
            {
                uint64_t bit = 1ULL << square;
                switch (c)
                {
                case 'P':
                    m_white_pawns_bit |= bit;
                    break;
                case 'N':
                    m_white_knights_bit |= bit;
                    break;
                case 'B':
                    m_white_bishops_bit |= bit;
                    break;
                case 'R':
                    m_white_rooks_bit |= bit;
                    break;
                case 'Q':
                    m_white_queens_bit |= bit;
                    break;
                case 'K':
                    m_white_king_bit |= bit;
                    break;
                case 'p':
                    m_black_pawns_bit |= bit;
                    break;
                case 'n':
                    m_black_knights_bit |= bit;
                    break;
                case 'b':
                    m_black_bishops_bit |= bit;
                    break;
                case 'r':
                    m_black_rooks_bit |= bit;
                    break;
                case 'q':
                    m_black_queens_bit |= bit;
                    break;
                case 'k':
                    m_black_king_bit |= bit;
                    break;
                }
                square++;
            }
        }

        // Determine which side is to move
        m_turn = (turn == "w");

        // Parse castling rights
        m_white_kingside_castling = castling.find('K') != std::string::npos;
        m_white_queenside_castling = castling.find('Q') != std::string::npos;
        m_black_kingside_castling = castling.find('k') != std::string::npos;
        m_black_queenside_castling = castling.find('q') != std::string::npos;

        setAllPiecesBits();
        setKingPosition();

        m_white_pieces_bit = m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | m_white_queens_bit | m_white_king_bit;
        m_black_pieces_bit = m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | m_black_queens_bit | m_black_king_bit;
        m_all_pieces_bit = m_white_pieces_bit | m_black_pieces_bit;
        m_all_pieces_bit_without_white_king = m_all_pieces_bit & ~m_white_king_bit;
        m_all_pieces_bit_without_black_king = m_all_pieces_bit & ~m_black_king_bit;

        setKingPosition();
        setBlackBishopsAttackedSquares();
        setBlackRooksAttackedSquares();
        setBlackQueensAttackedSquares();
        setWhiteBishopsAttackedSquares();
        setWhiteRooksAttackedSquares();
        setWhiteQueensAttackedSquares();
        setBlackKnightsAttackedSquares();
        setBlackKingAttackedSquares();
        setBlackPawnsAttackedSquares();
        setWhiteKnightsAttackedSquares();
        setWhiteKingAttackedSquares();
        setWhitePawnsAttackedSquares();

        m_all_squares_attacked_by_white = m_squares_attacked_by_white_pawns | m_squares_attacked_by_white_knights | m_squares_attacked_by_white_bishops | m_squares_attacked_by_white_rooks | m_squares_attacked_by_white_queens | m_squares_attacked_by_white_king;
        m_all_squares_attacked_by_black = m_squares_attacked_by_black_pawns | m_squares_attacked_by_black_knights | m_squares_attacked_by_black_bishops | m_squares_attacked_by_black_rooks | m_squares_attacked_by_black_queens | m_squares_attacked_by_black_king;

        initializeZobristKey();

        m_captures.reserve(16);
    }

    void initializeZobristKey();
    void updateZobristKeyPiecePartAfterMove(unsigned short origin_square, unsigned short destination_square);
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
    Capture nextCapture();
    Capture nextCaptureInCheck();

    void whitePawnQueenCaptures();
    void whiteKingQueenCaptures();
    void whiteKnightQueenCaptures();
    void whiteBishopQueenCaptures();
    void whiteRookQueenCaptures();
    void whiteKingAllExceptQueenCaptures();
    void whitePawnRookBishopKnightCaptures();
    void whiteKnightRookCaptures();
    void whiteBishopRookCaptures();
    void whiteKnightBishopKnightCaptures();
    void whiteBishopBishopKnightCaptures();
    void whitePawnPawnCaptures();
    void whiteKnightPawnCaptures();
    void whiteBishopPawnCaptures();
    void whiteRookAllExceptQueenCaptures();
    void whiteQueenAllCaptures();

    void blackPawnQueenCaptures();
    void blackKingQueenCaptures();
    void blackKnightQueenCaptures();
    void blackBishopQueenCaptures();
    void blackRookQueenCaptures();
    void blackKingAllExceptQueenCaptures();
    void blackPawnRookBishopKnightCaptures();
    void blackKnightRookCaptures();
    void blackBishopRookCaptures();
    void blackKnightBishopKnightCaptures();
    void blackBishopBishopKnightCaptures();
    void blackPawnPawnCaptures();
    void blackKnightPawnCaptures();
    void blackBishopPawnCaptures();
    void blackRookAllExceptQueenCaptures();
    void blackQueenAllCaptures();

    void whiteInCheckCaptures();
    void blackInCheckCaptures();

    std::vector<Capture> inCheckOrderedGoodCaptures() const;
    std::vector<Capture> inCheckOrderedBadCaptures() const;
    std::vector<Capture> orderedGoodCaptures() const;
    std::vector<Capture> orderedBadCaptures() const;
    std::vector<Move> inCheckAllMoves() const;
    std::vector<Move> allMoves() const;
    std::vector<Move> orderAllMoves(std::vector<Move> &moves, Move ttMove) const;
    std::vector<Move> orderAllMovesOnFirstIterationFirstTime(std::vector<Move> &moves, Move ttMove) const;
    std::pair<std::vector<Move>, std::vector<int16_t>> orderAllMovesOnFirstIteration(std::vector<Move> &moves, std::vector<int16_t> &scores) const;
    std::vector<Move> inCheckMoves() const;
    std::vector<Move> nonCaptureMoves() const;
    bool isStalemate() const;
    bool isMate() const;

    void setPiece();
    void removePiece();
    void storePlyInfo();
    void makeNormalMove(Move move);
    void makeNormalMoveForTest(Move move);
    void makeCapture(Capture move);
    void unmakeNormalMove(Move move);
    void unmakeNormalMoveForTest(Move move);
    void unmakeCapture(Capture move);
    void makeNormalMoveWithoutNNUE(Move move);
    void setSliderAttackedSquares();
    void setAttackedSquaresAfterMove();

    // Member function definitions

    void restorePlyInfo()
    // This member function is used when either opponent or engine makes a capture.
    // To make the ply info smaller. For a faster 3 fold repetition check.
    // It isnt used when making a move in search!
    {
        m_ply = 0;
        std::array<bool, 128> m_wkcastling_array{};
        std::array<bool, 128> m_wqcastling_array{};
        std::array<bool, 128> m_bkcastling_array{};
        std::array<bool, 128> m_bqcastling_array{};
        std::array<unsigned short, 128> m_psquare_array{};
        std::array<uint64_t, 128> m_diagonal_pins_array{};
        std::array<uint64_t, 128> m_straight_pins_array{};

        std::array<bool, 128> m_is_check_array{};

        std::array<unsigned short, 128> m_white_king_positions_array{};
        std::array<unsigned short, 128> m_black_king_positions_array{};

        std::array<uint64_t, 128> m_squares_attacked_by_white_pawns_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_black_pawns_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_white_knights_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_black_knights_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_white_bishops_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_black_bishops_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_white_rooks_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_black_rooks_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_white_queens_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_black_queens_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_white_king_array{};
        std::array<uint64_t, 128> m_squares_attacked_by_black_king_array{};

        std::array<uint64_t, 128> m_all_squares_attacked_by_white_array{};
        std::array<uint64_t, 128> m_all_squares_attacked_by_black_array{};

        std::array<uint64_t, 128> m_zobrist_keys_array{m_zobrist_key};

        std::array<unsigned short, 128> m_captured_piece_array{};
        std::array<std::vector<Capture>, 128> m_captures_array{};
    }
    bool isThreeFold()
    {
        if (m_zobrist_keys_array.empty())
        {
            return false; // Ensure there is at least one element
        }

        // Find the last non-zero key in the array
        uint64_t lastKey = 0;
        for (auto it = m_zobrist_keys_array.rbegin(); it != m_zobrist_keys_array.rend(); ++it)
        {
            if (*it != 0)
            {
                lastKey = *it;
                break; // Break once the last non-zero key is found
            }
        }

        // If lastKey remains 0, it means all elements were zero, or the last element is zero which should not count.
        if (lastKey == 0)
        {
            return false;
        }

        int count = 0;
        for (uint64_t key : m_zobrist_keys_array)
        {
            if (key == lastKey)
            {
                count++;
            }
        }
        return count >= 3; // Return true if last non-zero position occurred at least three times
    }

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

    bool getIsCheck() const { return m_is_check; }

    uint64_t getZobristKey() const { return m_zobrist_key; }

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

    uint64_t getAllWhitePiecesBits() const { return m_white_pieces_bit; }
    uint64_t getAllBlackPiecesBits() const { return m_black_pieces_bit; }

    uint64_t getWhiteAttackedSquaresBits() const { return m_all_squares_attacked_by_white; }
    uint64_t getBlackAttackedSquaresBits() const { return m_all_squares_attacked_by_black; }

    unsigned short getMovedPiece() const { return m_moved_piece; }
    unsigned short getCapturedPiece() const { return m_captured_piece; }
    unsigned short getPromotedPiece() const { return m_promoted_piece; }
    unsigned short getWhiteKingPosition() const { return m_white_king_position; }
    unsigned short getBlackKingPosition() const { return m_black_king_position; }

    void printBitboards() const
    {
        std::cout << "White pawns " << m_white_pawns_bit << "\n";
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

        std::cout << "All Whites " << m_white_pieces_bit << "\n";
        std::cout << "All Blacks " << m_black_pieces_bit << "\n";
        std::cout << "All Pieces " << m_all_pieces_bit << "\n";

        std::cout << "psquare " << m_psquare << "\n";
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

    std::string toFenString() const
    {
        std::string fen;
        // Pieces part
        for (int row = 7; row >= 0; --row)
        {
            int emptyCount = 0;
            for (int col = 0; col < 8; ++col)
            {
                int square = row * 8 + col;
                uint64_t bit = 1ULL << square;
                char pieceChar = ' ';
                if (m_white_pawns_bit & bit)
                    pieceChar = 'P';
                else if (m_white_knights_bit & bit)
                    pieceChar = 'N';
                else if (m_white_bishops_bit & bit)
                    pieceChar = 'B';
                else if (m_white_rooks_bit & bit)
                    pieceChar = 'R';
                else if (m_white_queens_bit & bit)
                    pieceChar = 'Q';
                else if (m_white_king_bit & bit)
                    pieceChar = 'K';
                else if (m_black_pawns_bit & bit)
                    pieceChar = 'p';
                else if (m_black_knights_bit & bit)
                    pieceChar = 'n';
                else if (m_black_bishops_bit & bit)
                    pieceChar = 'b';
                else if (m_black_rooks_bit & bit)
                    pieceChar = 'r';
                else if (m_black_queens_bit & bit)
                    pieceChar = 'q';
                else if (m_black_king_bit & bit)
                    pieceChar = 'k';

                if (pieceChar != ' ')
                {
                    if (emptyCount > 0)
                    {
                        fen += std::to_string(emptyCount);
                        emptyCount = 0;
                    }
                    fen += pieceChar;
                }
                else
                {
                    ++emptyCount;
                }
            }
            if (emptyCount > 0)
                fen += std::to_string(emptyCount);
            if (row > 0)
                fen += '/';
        }

        // Active color
        fen += ' ';
        fen += (m_turn ? 'w' : 'b');

        // Castling availability
        fen += ' ';
        if (!m_white_kingside_castling && !m_white_queenside_castling &&
            !m_black_kingside_castling && !m_black_queenside_castling)
        {
            fen += '-';
        }
        else
        {
            if (m_white_kingside_castling)
                fen += 'K';
            if (m_white_queenside_castling)
                fen += 'Q';
            if (m_black_kingside_castling)
                fen += 'k';
            if (m_black_queenside_castling)
                fen += 'q';
        }

        // En passant target square
        fen += ' ';
        fen += '-';

        // Halfmove clock and fullmove number placeholders (not stored in BitPosition)
        fen += " 0 1"; // These values would need to be tracked elsewhere for accuracy

        return fen;
    }
};

#endif // BITPOSITION_H