#ifndef BITPOSITION_H
#define BITPOSITION_H
#include <array>
#include <cstdint> // For fixed sized integers
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

    // Bits to represent all pieces of each player
    uint64_t m_white_pieces_bit{};
    uint64_t m_black_pieces_bit{};
    uint64_t m_all_pieces_bit{};

    // True white's turn, False black's
    bool m_turn{};

    // Booleans representing castling rights
    bool m_white_kingside_castling{};
    bool m_white_queenside_castling{};
    bool m_black_kingside_castling{};
    bool m_black_queenside_castling{};

    // Index of passant square a1 = 0, h8 = 63
    unsigned short m_psquare{0};

    // Unsigned short representing kings' positions
    unsigned short m_white_king_position{};
    unsigned short m_black_king_position{};

    // For ilegal moves
    uint64_t m_straight_pins{};
    uint64_t m_diagonal_pins{};

    uint64_t m_unsafe_squares{};

    uint64_t m_king_unsafe_squares{};

    // For discovered checks
    uint64_t m_blockers{};
    bool m_blockers_set{false};

    // Bits representing check info
    bool m_is_check{false};
    unsigned short m_check_square{65};
    uint64_t m_check_rays{0};
    unsigned short m_num_checks{0};

    // For check info after move
    unsigned short m_last_origin_square{};
    unsigned short m_last_destination_square{};

    uint64_t m_last_destination_bit{};

    // Used for zobrist key updates
    uint64_t m_zobrist_key{};
    unsigned short m_moved_piece{}; // Used for setting check info after move
    unsigned short m_promoted_piece{7}; // Used for setting checks info after move
    unsigned short m_captured_piece{7}; // Used for unmakeMove too

    // Ply number
    unsigned short m_ply{};
    
    // Ply info arrays
    std::array<bool, 64> m_wkcastling_array{};
    std::array<bool, 64> m_wqcastling_array{};
    std::array<bool, 64> m_bkcastling_array{};
    std::array<bool, 64> m_bqcastling_array{};

    std::array<uint64_t, 64> m_straight_pins_array{};
    std::array<uint64_t, 64> m_diagonal_pins_array{};
    std::array<uint64_t, 64> m_blockers_array{};

    std::array<unsigned short, 64> m_last_origin_square_array{};
    std::array<unsigned short, 64> m_last_destination_square_array{};
    std::array<unsigned short, 64> m_moved_piece_array{};
    std::array<unsigned short, 64> m_promoted_piece_array{};
    std::array<unsigned short, 64> m_psquare_array{};

    std::array<uint64_t, 128> m_zobrist_keys_array{};

    std::array<unsigned short, 64> m_captured_piece_array{}; // For unmakeMove
    std::array<uint64_t, 64> m_unsafe_squares_array{};

    std::array<uint64_t, 64> m_last_destination_bit_array{};

    // std::array<std::string, 64> m_fen_array{}; // For debugging purposes

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

        setKingPosition();
        setIsCheckOnInitialization();
        setCheckInfoOnInitialization();
        setPins();
        setAttackedSquares();
        initializeZobristKey();
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

        setKingPosition();
        setIsCheckOnInitialization();
        setCheckInfoOnInitialization();
        setPins();
        setAttackedSquares();
        initializeZobristKey();
    }
    
    void setIsCheckOnInitialization();
    void setCheckInfoOnInitialization();

    void initializeZobristKey();
    void updateZobristKeyPiecePartAfterMove(unsigned short origin_square, unsigned short destination_square);

    bool ttMoveIsLegal(Move move);
    void storePlyInfoInTTMove();
    void makeTTMove(Move tt_move);
    void unmakeTTMove(Move move);

    void setPins();
    void setBlockers();
    void setAttackedSquares();

    template <typename T>
    bool isLegal(const T &move) const;
    bool isLegalForWhite(unsigned short origin_square, unsigned short destination_square) const;
    bool isLegalForBlack(unsigned short origin_square, unsigned short destination_square) const;

    // These set the checks bits, m_is_check and m_num_checks    
    void setDiscoverCheckForWhite();
    void setDiscoverCheckForBlack();

    void setCheckInfoAfterMove();

    bool newKingSquareIsSafe(unsigned short new_position) const;
    bool newWhiteKingSquareIsSafe(unsigned short new_position) const;
    bool newBlackKingSquareIsSafe(unsigned short new_position) const;

    bool whiteSquareIsSafe(unsigned short square) const;
    bool blackSquareIsSafe(unsigned short square) const;

    bool kingIsSafeAfterPassant(unsigned short removed_square_1, unsigned short removed_square_2) const;

    void pawnAllMoves(ScoredMove*& move_list) const;
    void knightAllMoves(ScoredMove*& move_list) const;
    void bishopAllMoves(ScoredMove*& move_list) const;
    void rookAllMoves(ScoredMove*& move_list) const;
    void queenAllMoves(ScoredMove*& move_list) const;
    void kingAllMoves(ScoredMove*& move_list) const;

    void kingAllMovesInCheck(Move *&move_list) const;

    Move getBestRefutation();

    void pawnCapturesAndQueenProms(ScoredMove*& move_list) const;
    void knightCaptures(ScoredMove*& move_list) const;
    void bishopCaptures(ScoredMove*& move_list) const;
    void rookCaptures(ScoredMove*& move_list) const;
    void queenCaptures(ScoredMove*& move_list) const;
    void kingCaptures(ScoredMove *&move_list) const;
    void kingCaptures(Move *&move_list) const;

    Move *setRefutationMovesOrdered(Move *&move_list);
    Move *setGoodCapturesOrdered(Move *&move_list);

    void pawnSafeMoves(ScoredMove *&move_list) const;
    void knightSafeMoves(ScoredMove *&move_list) const;
    void bishopSafeMoves(ScoredMove *&move_list) const;
    void rookSafeMoves(ScoredMove *&move_list) const;
    void queenSafeMoves(ScoredMove *&move_list) const;
    void kingNonCapturesAndPawnCaptures(ScoredMove *&move_list) const;

    void pawnBadCapturesOrUnsafeNonCaptures(Move *&move_list);
    void knightBadCapturesOrUnsafeNonCaptures(Move *&move_list);
    void bishopBadCapturesOrUnsafeNonCaptures(Move *&move_list);
    void rookBadCapturesOrUnsafeNonCaptures(Move *&move_list);
    void queenBadCapturesOrUnsafeNonCaptures(Move *&move_list);

    template <typename T>
    void kingCaptures(T *&move_list) const;

    void inCheckPawnBlocks(Move *&move_list) const;
    void inCheckKnightBlocks(Move *&move_list) const;
    void inCheckBishopBlocks(Move *&move_list) const;
    void inCheckRookBlocks(Move *&move_list) const;
    void inCheckQueenBlocks(Move *&move_list) const;

    bool isDiscoverCheckForWhiteAfterPassant(unsigned short origin_square, unsigned short destination_square) const;
    bool isDiscoverCheckForBlackAfterPassant(unsigned short origin_square, unsigned short destination_square) const;

    bool isDiscoverCheckForWhite(unsigned short origin_square, unsigned short destination_square) const;
    bool isDiscoverCheckForBlack(unsigned short origin_square, unsigned short destination_square) const;

    bool isPawnCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const;
    bool isKnightCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const;
    bool isBishopCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const;
    bool isRookCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const;
    bool isQueenCheckOrDiscoverForWhite(unsigned short origin_square, unsigned short destination_square) const;

    bool isPawnCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const;
    bool isKnightCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const;
    bool isBishopCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const;
    bool isRookCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const;
    bool isQueenCheckOrDiscoverForBlack(unsigned short origin_square, unsigned short destination_square) const;

    void inCheckOrderedCapturesAndKingMoves(Move *&move_list) const;
    void inCheckOrderedCaptures(Move *&move_list) const;

    ScoredMove *setSafeMovesAndScores(ScoredMove *&move_list_start);
    Move *setBadCapturesOrUnsafeMoves(Move *&currentMove);

    ScoredMove *setMovesAndScores(ScoredMove *&move_list_start);
    Move *setMovesInCheck(Move *&move_list_start);
    ScoredMove *setCapturesAndScores(ScoredMove *&move_list_start);
    Move *setOrderedCapturesInCheck(Move *&move_list_start);

    ScoredMove nextScoredMove(ScoredMove *&move_list, ScoredMove *endMoves);
    Move nextMove(Move *&move_list, Move *endMoves);
    ScoredMove nextScoredMove(ScoredMove *&move_list, ScoredMove *endMoves, Move ttMove);
    Move nextMove(Move *&move_list, Move *endMoves, Move ttMove);

    std::vector<Move> orderAllMovesOnFirstIterationFirstTime(std::vector<Move> &moves, Move ttMove) const;
    std::pair<std::vector<Move>, std::vector<int16_t>> orderAllMovesOnFirstIteration(std::vector<Move> &moves, std::vector<int16_t> &scores) const;
    std::vector<Move> inCheckMoves() const;
    std::vector<Move> nonCaptureMoves() const;

    bool isStalemate() const;
    bool isMate() const;
    bool isThreeFoldOr50MoveRule() const;
    int m_50_move_count{1};
    std::array<int, 64> m_50_move_count_array{};

    void setPiece(uint64_t origin_bit, uint64_t destination_bit);
    void storePlyInfo();
    void storePlyInfoInCaptures();
    bool moveIsReseter(Move move);
    void resetPlyInfo();

    template <typename T>
    void makeMove(T move);
    template <typename T>
    void unmakeMove(T move);

    template <typename T>
    void makeCapture(T move);
    template <typename T>
    void unmakeCapture(T move);

    template <typename T>
    void makeCaptureWithoutNNUE(T move);
    template <typename T>
    void unmakeCaptureWithoutNNUE(T move);

    std::vector<Move> inCheckAllMoves();
    std::vector<Move> allMoves();

    uint64_t updateZobristKeyPiecePartBeforeMove(unsigned short origin_square, unsigned short destination_square, unsigned short promoted_piece) const;
    template <typename T>
    bool positionIsDrawnAfterMove(T move) const;

    bool isEndgame() const
    {
        // Count the number of set bits (pieces) for each piece type
        int white_major_pieces = countBits(m_white_rooks_bit) + countBits(m_white_queens_bit);
        int white_minor_pieces = countBits(m_white_knights_bit) + countBits(m_white_bishops_bit);
        int black_major_pieces = countBits(m_black_rooks_bit) + countBits(m_black_queens_bit);
        int black_minor_pieces = countBits(m_black_knights_bit) + countBits(m_black_bishops_bit);

        // Total number of major and minor pieces
        int total_major_pieces = white_major_pieces + black_major_pieces;
        int total_minor_pieces = white_minor_pieces + black_minor_pieces;

        return total_major_pieces <= 2 && total_minor_pieces <= 3;
    }

    // Functions for tests
    void inCheckPawnBlocksNonQueenProms(Move *&move_list) const;
    void inCheckPawnCapturesNonQueenProms(Move *&move_list) const;
    void inCheckPassantCaptures(Move *&move_list) const;
    void pawnNonCapturesNonQueenProms(Move *&move_list) const;
    void knightNonCaptures(Move*& move_list) const;
    void bishopNonCaptures(Move*& move_list) const;
    void rookNonCaptures(Move*& move_list) const;
    void queenNonCaptures(Move*& move_list) const;
    void kingNonCaptures(Move*& move_list) const;
    void kingNonCapturesInCheck(Move *&move_list) const;

    Move *setNonCaptures(Move *&move_list_start);
    Move *setNonCapturesInCheck(Move *&move_list_start);

    Move *setMovesInCheckTest(Move *&move_list_start);
    Move *setCapturesInCheckTest(Move *&move_list_start);

    // Simple member function definitions

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
    }

    bool getTurn() const { return m_turn; }

    bool getIsCheck() const 
    {
        return m_is_check;
    }
    unsigned short getNumChecks() const
    {
        return m_num_checks;
    }

    uint64_t getZobristKey() const { return m_zobrist_key; }
    std::array<uint64_t, 128> getZobristKeysArray() const { return m_zobrist_keys_array; }
    void printZobristKeys() const
    {
        std::array<uint64_t, 128> keys = getZobristKeysArray();
        for (size_t i = 0; i < keys.size(); ++i)
        {
            if (keys[i] != 0)
                std::cout << "Key[" << i << "] = " << keys[i] << "\n";
        }
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

    uint64_t getAllWhitePiecesBits() const { return m_white_pieces_bit; }
    uint64_t getAllBlackPiecesBits() const { return m_black_pieces_bit; }

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
        std::cout << "Square of piece giving check " << m_check_square << "\n";
        std::cout << "Check rays " << m_check_rays << "\n";
        std::cout << "Number of checks " << m_num_checks << "\n";
    }
    void printPinsInfo() const
    {
        std::cout << "Straight pins bit " << m_straight_pins << "\n";
        std::cout << "Diagonal pins bit " << m_diagonal_pins << "\n";
        std::cout << "Blockers bit " << m_blockers << "\n";
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