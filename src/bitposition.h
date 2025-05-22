#ifndef BITPOSITION_H
#define BITPOSITION_H
#include <array>
#include <cstdint>     // For fixed sized integers
#include "bit_utils.h" // Bit utility functions
#include "move.h"
#include "simd.h" // NNUEU updates
#include "position_eval.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <cstring>
#include <algorithm>
#include <cassert>

extern bool ENGINEISWHITE;

enum CastlingRights : uint8_t
{
    WHITE_KS = 1 << 0, // 0x01 (bit 0)
    WHITE_QS = 1 << 1, // 0x02 (bit 1)
    BLACK_KS = 1 << 2, // 0x04 (bit 2)
    BLACK_QS = 1 << 3  // 0x08 (bit 3)
};

struct StateInfo
{
    // Copied when making a move or capture
    int8_t castlingRights;   // Bits: 0=WhiteKS, 1=WhiteQS, 2=BlackKS, 3=BlackQS
    int reversibleMovesMade; // Used for three-fold checks
    // Bellow this. Not copied when making a capture (will be recomputed anyhow)
    uint64_t zobristKey;

    // Bellow this. Not copied when making a move (will be recomputed anyhow)
    uint64_t straightPinnedPieces;
    uint64_t diagonalPinnedPieces;
    uint64_t pinnedPieces;
    uint64_t blockersForKing;
    int lastDestinationSquare; // For refutation moves after unmaking a Ttmove
    int capturedPiece;
    int lastOriginSquare; // For when calling setCheckInfo()
    bool isCheck;
    uint64_t checkBits[5];
    int pSquare; // Used to update zobrist key

    // Pointers to previous and next state
    StateInfo *previous;
    StateInfo *next;
};

class BitPosition
{
private:
    // Board of color pieces 7s where no pieces
    int m_white_board[64];
    int m_black_board[64];

    // 64-bit to represent pieces on board for each piece type and color
    uint64_t m_pieces[2][6];

    // Bits to represent all pieces of each player
    uint64_t m_pieces_bit[2];
    uint64_t m_all_pieces_bit{};

    // True white's turn, False black's
    bool m_turn{};

    // For updating stuff in makeMove, makeCapture and makeTTMove
    int m_moved_piece{7};
    int m_promoted_piece{7};
    int m_last_destination_square;

    // int representing kings' positions
    int m_king_position[2];

    // For discovered checks
    bool m_blockers_set{false};

    // Bits representing check info
    int m_check_square{65};
    uint64_t m_check_rays{0};
    int m_num_checks{0};

    // Ply number
    int m_ply{0};

    StateInfo *state_info;

    // std::array<std::string, 64> m_fen_array{}; // For debugging purposes

public:
    // Function to convert a FEN string to a BitPosition object
    BitPosition(const std::string &fen)
    {
        std::istringstream fenStream(fen);
        std::string board, turn, castling, enPassant;
        fenStream >> board >> turn >> castling >> enPassant;

        // Initialize all bitboards to 0
        m_pieces[0][0] = 0, m_pieces[0][1] = 0, m_pieces[0][2] = 0, m_pieces[0][3] = 0, m_pieces[0][4] = 0, m_pieces[0][5] = 0;
        m_pieces[1][0] = 0, m_pieces[1][1] = 0, m_pieces[1][2] = 0, m_pieces[1][3] = 0, m_pieces[1][4] = 0, m_pieces[1][5] = 0;

        std::fill(std::begin(m_white_board), std::end(m_white_board), 7);
        std::fill(std::begin(m_black_board), std::end(m_black_board), 7);
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
                    m_pieces[0][0] |= bit;
                    m_white_board[square] = 0;
                    break;
                case 'N':
                    m_pieces[0][1] |= bit;
                    m_white_board[square] = 1;
                    break;
                case 'B':
                    m_pieces[0][2] |= bit;
                    m_white_board[square] = 2;
                    break;
                case 'R':
                    m_pieces[0][3] |= bit;
                    m_white_board[square] = 3;
                    break;
                case 'Q':
                    m_pieces[0][4] |= bit;
                    m_white_board[square] = 4;
                    break;
                case 'K':
                    m_pieces[0][5] |= bit;
                    m_white_board[square] = 5;
                    break;
                case 'p':
                    m_pieces[1][0] |= bit;
                    m_black_board[square] = 0;
                    break;
                case 'n':
                    m_pieces[1][1] |= bit;
                    m_black_board[square] = 1;
                    break;
                case 'b':
                    m_pieces[1][2] |= bit;
                    m_black_board[square] = 2;
                    break;
                case 'r':
                    m_pieces[1][3] |= bit;
                    m_black_board[square] = 3;
                    break;
                case 'q':
                    m_pieces[1][4] |= bit;
                    m_black_board[square] = 4;
                    break;
                case 'k':
                    m_pieces[1][5] |= bit;
                    m_black_board[square] = 5;
                    break;
                }
                square++;
            }
        }

        // Determine which side is to move
        m_turn = (turn == "w");
        state_info = new StateInfo(); // Allocate memory for state_info
                                      // Initialize no castling rights
        uint8_t cr = 0;

        if (castling.find('K') != std::string::npos)
            cr |= WHITE_KS;

        if (castling.find('Q') != std::string::npos)
            cr |= WHITE_QS;

        if (castling.find('k') != std::string::npos)
            cr |= BLACK_KS;

        if (castling.find('q') != std::string::npos)
            cr |= BLACK_QS;

        state_info->castlingRights = cr;
        state_info->reversibleMovesMade = 0;
        state_info->pSquare = 0;

        setAllPiecesBits();
        setKingPosition();
        setIsCheckOnInitialization();
        if (getIsCheck())
            setCheckInfoOnInitialization();
        setBlockersAndPinsInAB();
        initializeZobristKey();
        NNUEU::globalAccumulatorStack.reset(*this);
    }

    void setIsCheckOnInitialization();
    bool getIsCheckOnInitialization();
    bool isKingInCheck(bool side) const;
    void setCheckInfoOnInitialization();

    uint64_t computeFullZobristKey() const;
    void initializeZobristKey();

    void setBlockersAndPinsInAB();
    void setBlockersPinsAndCheckBitsInQS();

    template <typename T>
    bool isLegal(const T *move) const;
    template <typename T>
    bool isCaptureLegal(const T *move) const;

    bool isRefutationLegal(Move move) const;
    bool isNormalMoveLegal(int origin_square, int destination_square) const;

    bool ttMoveIsOk(Move move) const;

    // These set the checks bits, m_is_check and m_num_checks

    void setCheckInfo();
    void setCheckBits();

    bool isQueenCheck(int destination_square);
    bool isPromotionCheck(int piece, int destination_square);

    bool newKingSquareIsSafe(int new_position) const;

    bool kingIsSafeAfterPassant(int removed_square_1, int removed_square_2) const;

    ScoredMove *pawnAllMoves(ScoredMove *&move_list) const;
    ScoredMove *knightAllMoves(ScoredMove *&move_list) const;
    ScoredMove *bishopAllMoves(ScoredMove *&move_list) const;
    ScoredMove *rookAllMoves(ScoredMove *&move_list) const;
    ScoredMove *queenAllMoves(ScoredMove *&move_list) const;
    ScoredMove *kingAllMoves(ScoredMove *&move_list) const;

    Move *kingAllMovesInCheck(Move *&move_list) const;

    ScoredMove *pawnCapturesAndQueenProms(ScoredMove *&move_list) const;
    ScoredMove *knightCaptures(ScoredMove *&move_list) const;
    ScoredMove *bishopCaptures(ScoredMove *&move_list) const;
    ScoredMove *rookCaptures(ScoredMove *&move_list) const;
    ScoredMove *queenCaptures(ScoredMove *&move_list) const;
    ScoredMove *kingCaptures(ScoredMove *&move_list) const;
    Move *kingCaptures(Move *&move_list) const;

    ScoredMove *kingNonCapturesAndPawnCaptures(ScoredMove *&move_list) const;

    Move *inCheckPawnBlocks(Move *&move_list) const;
    Move *inCheckKnightBlocks(Move *&move_list) const;
    Move *inCheckBishopBlocks(Move *&move_list) const;
    Move *inCheckRookBlocks(Move *&move_list) const;
    Move *inCheckQueenBlocks(Move *&move_list) const;

    bool isDiscoverCheckAfterPassant() const;

    bool isDiscoverCheck(int origin_square, int destination_square) const;

    Move *inCheckOrderedCapturesAndKingMoves(Move *&move_list) const;
    Move *inCheckOrderedCaptures(Move *&move_list) const;

    std::pair<std::vector<Move>, std::vector<int16_t>> orderAllMovesOnFirstIteration(std::vector<Move> &moves, std::vector<int16_t> &scores) const;
    std::vector<Move> inCheckMoves() const;
    std::vector<Move> nonCaptureMoves() const;

    bool isMate() const;

    bool moveIsReseter(Move move);

    template <typename T>
    void makeMove(T move, StateInfo &new_state_info);
    template <typename T>
    void unmakeMove(T move);

    template <typename T>
    void makeCapture(T move, StateInfo &new_state_info);
    template <typename T>
    void unmakeCapture(T move);

    template <typename T>
    void makeCaptureTest(T move, StateInfo &new_state_info);

    bool isDraw() const;

    inline int qsScore(int dst) const
    {
        return m_turn ? m_black_board[dst] 
                      : m_white_board[dst];
    }

    int qSMoveValue(Move move) const
    {
        // Promotions and castling
        if (move.getData() & 0b0100000000000000)
        {
            return 30;
        }
        // Non promotions
        if (m_turn)
        {
            return m_black_board[move.getDestinationSquare()];
        }
        else
        {
            return m_white_board[move.getDestinationSquare()];
        }
    }
    int aBMoveValue(Move move) const
    // Captures and queen promotions
    {
        // Promotions and castling
        if (move.getData() & 0b0100000000000000)
        {
            // Promotions
            if (m_turn && m_white_board[move.getOriginSquare()] == 0)
                return 30;
            else if (not m_turn && m_black_board[move.getOriginSquare()] == 0)
                return 30;
            // Castling
            return 2;
        }
        // Non promotions
        int score = 0;
        if (m_turn)
        {
            int piece_at = m_black_board[move.getDestinationSquare()];
            if (piece_at != 7)
                score += piece_at + 1;
        }
        else
        {
            int piece_at = m_white_board[move.getDestinationSquare()];
            if (piece_at != 7)
                score += piece_at + 1;
        }
        return score;
    }

    bool isEndgame() const
    {
        // Count the number of set bits (pieces) for each piece type
        int white_major_pieces = countBits(m_pieces[0][3]) + countBits(m_pieces[0][4]);
        int white_minor_pieces = countBits(m_pieces[0][1]) + countBits(m_pieces[0][2]);
        int black_major_pieces = countBits(m_pieces[1][3]) + countBits(m_pieces[1][4]);
        int black_minor_pieces = countBits(m_pieces[1][1]) + countBits(m_pieces[1][2]);

        // Total number of major and minor pieces
        int total_major_pieces = white_major_pieces + black_major_pieces;
        int total_minor_pieces = white_minor_pieces + black_minor_pieces;

        return total_major_pieces <= 2 && total_minor_pieces <= 3;
    }

    inline StateInfo *get_state_info() const { return state_info; }

    // Functions for tests
    Move *inCheckPawnBlocksNonQueenProms(Move *&move_list) const;
    Move *inCheckPawnCapturesNonQueenProms(Move *&move_list) const;
    Move *inCheckPassantCaptures(Move *&move_list) const;
    Move *pawnNonCapturesNonQueenProms(Move *&move_list) const;
    Move *knightNonCaptures(Move *&move_list) const;
    Move *bishopNonCaptures(Move *&move_list) const;
    Move *rookNonCaptures(Move *&move_list) const;
    Move *queenNonCaptures(Move *&move_list) const;
    Move *kingNonCaptures(Move *&move_list) const;
    Move *kingNonCapturesInCheck(Move *&move_list) const;

    // Simple member function definitions

    void setKingPosition()
    // Set the index of the king in the board.
    {
        m_king_position[0] = getLeastSignificantBitIndex(m_pieces[0][5]);
        m_king_position[1] = getLeastSignificantBitIndex(m_pieces[1][5]);
    }

    void setAllPiecesBits()
    // Function that sets the own pieces, opponent pieces and all pieces bits.
    {
        m_pieces_bit[0] = (m_pieces[0][0] | m_pieces[0][1] | m_pieces[0][2] | m_pieces[0][3] | m_pieces[0][4] | m_pieces[0][5]);
        m_pieces_bit[1] = (m_pieces[1][0] | m_pieces[1][1] | m_pieces[1][2] | m_pieces[1][3] | m_pieces[1][4] | m_pieces[1][5]);
        m_all_pieces_bit = (m_pieces_bit[0] | m_pieces_bit[1]);
    }

    inline bool getTurn() const { return m_turn; }

    inline int getKingPosition(bool turn) const
    {
        return m_king_position[turn];
    }
    uint64_t getPieces(int color, int pieceType) const
    {
        return m_pieces[color][pieceType];
    }

    inline bool hasBlockersUnset() const { return not m_blockers_set; }

    inline bool getIsCheck() const
    {
        return state_info->isCheck;
    }
    inline int getNumChecks() const
    {
        return m_num_checks;
    }

    inline bool sliderChecking() const { return m_check_rays != 0; }
    

    inline uint64_t getZobristKey() const
    {
        return state_info->zobristKey;
    }
    bool see_ge(Move m, int threshold) const;
    void printZobristKeys() const
    {
        const StateInfo *stp = state_info; // start at the current node
        std::size_t idx = 0;

        while (stp && idx < m_ply) // stop after 128 plies or at the root
        {
            std::cout << "Key[" << idx << "] = " << stp->zobristKey << '\n';
            stp = stp->previous; // walk back one ply
            ++idx;
        }
    }
    void print50MoveCount()
    {
        std::cout << state_info->reversibleMovesMade << "\n";
    }
    bool moreThanOneCheck() const { return m_num_checks > 1; }

    uint64_t getWhitePawnsBits() const { return m_pieces[0][0]; }
    uint64_t getWhiteKnightsBits() const { return m_pieces[0][1]; }
    uint64_t getWhiteBishopsBits() const { return m_pieces[0][2]; }
    uint64_t getWhiteRooksBits() const { return m_pieces[0][3]; }
    uint64_t getWhiteQueensBits() const { return m_pieces[0][4]; }
    uint64_t getWhiteKingBits() const { return m_pieces[0][5]; }

    uint64_t getBlackPawnsBits() const { return m_pieces[1][0]; }
    uint64_t getBlackKnightsBits() const { return m_pieces[1][1]; }
    uint64_t getBlackBishopsBits() const { return m_pieces[1][2]; }
    uint64_t getBlackRooksBits() const { return m_pieces[1][3]; }
    uint64_t getBlackQueensBits() const { return m_pieces[1][4]; }
    uint64_t getBlackKingBits() const { return m_pieces[1][5]; }

    int getMovedPiece() const { return m_moved_piece; }
    int getCapturedPiece() const { return state_info->capturedPiece; }
    int getPromotedPiece() const { return m_promoted_piece; }
    int getWhiteKingPosition() const { return m_king_position[0]; }
    int getBlackKingPosition() const { return m_king_position[1]; }

    void printBoard(const int board[64], const std::string &label)
    {
        std::cout << label << ":\n";
        for (int rank = 7; rank >= 0; --rank) // Start from the top rank (rank 8)
        {
            for (int file = 0; file < 8; ++file) // Left-to-right within the rank
            {
                int index = rank * 8 + file;
                std::cout << board[index] << " ";
            }
            std::cout << "\n"; // Newline after each rank
        }
    }

    void debugBoardState()
    {
        printBoard(m_white_board, "White Board");
        printBoard(m_black_board, "Black Board");

        std::cout << "\nBitboards:\n";
        for (int color = 0; color < 2; ++color)
        {
            std::cout << (color == 0 ? "White" : "Black") << " Pieces:\n";
            for (int piece = 0; piece < 6; ++piece)
            {
                std::cout << "Piece " << piece << ": " << m_pieces[color][piece] << "\n";
            }
        }
        std::cout << std::endl;
    }

    void printBitboards() const
    {
        std::cout << "White pawns " << m_pieces[0][0] << "\n";
        std::cout << "White knights " << m_pieces[0][1] << "\n";
        std::cout << "White bishops " << m_pieces[0][2] << "\n";
        std::cout << "White rooks " << m_pieces[0][3] << "\n";
        std::cout << "White queens " << m_pieces[0][4] << "\n";
        std::cout << "White king " << m_pieces[0][5] << "\n";

        std::cout << "Black pawns " << m_pieces[1][0] << "\n";
        std::cout << "Black knights " << m_pieces[1][1] << "\n";
        std::cout << "Black bishops " << m_pieces[1][2] << "\n";
        std::cout << "Black rooks " << m_pieces[1][3] << "\n";
        std::cout << "Black queens " << m_pieces[1][4] << "\n";
        std::cout << "Black king " << m_pieces[1][5] << "\n";

        std::cout << "All Pieces " << m_all_pieces_bit << "\n";

        std::cout << "psquare " << state_info->pSquare << "\n";

        std::cout << "White king position " << m_king_position[0] << "\n";
        std::cout << "Black king position " << m_king_position[1] << "\n";
    }
    void printChecksInfo() const
    {
        std::cout << "Square of piece giving check " << m_check_square << "\n";
        std::cout << "Check rays " << m_check_rays << "\n";
        std::cout << "Number of checks " << m_num_checks << "\n";
    }
    void printPinsInfo() const
    {
        std::cout << "Straight pins bit " << state_info->straightPinnedPieces << "\n";
        std::cout << "Diagonal pins bit " << state_info->diagonalPinnedPieces << "\n";
        std::cout << "Blockers bit " << state_info->blockersForKing << "\n";
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
                if (m_pieces[0][0] & bit)
                    pieceChar = 'P';
                else if (m_pieces[0][1] & bit)
                    pieceChar = 'N';
                else if (m_pieces[0][2] & bit)
                    pieceChar = 'B';
                else if (m_pieces[0][3] & bit)
                    pieceChar = 'R';
                else if (m_pieces[0][4] & bit)
                    pieceChar = 'Q';
                else if (m_pieces[0][5] & bit)
                    pieceChar = 'K';
                else if (m_pieces[1][0] & bit)
                    pieceChar = 'p';
                else if (m_pieces[1][1] & bit)
                    pieceChar = 'n';
                else if (m_pieces[1][2] & bit)
                    pieceChar = 'b';
                else if (m_pieces[1][3] & bit)
                    pieceChar = 'r';
                else if (m_pieces[1][4] & bit)
                    pieceChar = 'q';
                else if (m_pieces[1][5] & bit)
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
        if (!(state_info->castlingRights & WHITE_KS) && !(state_info->castlingRights & WHITE_QS) &&
            !(state_info->castlingRights & BLACK_KS) && !(state_info->castlingRights & BLACK_QS))
        {
            fen += '-';
        }
        else
        {
            if (state_info->castlingRights & WHITE_KS)
                fen += 'K';
            if (state_info->castlingRights & WHITE_QS)
                fen += 'Q';
            if (state_info->castlingRights & BLACK_KS)
                fen += 'k';
            if (state_info->castlingRights & BLACK_QS)
                fen += 'q';
        }

        // En passant target square
        fen += ' ';
        fen += '-';

        // Halfmove clock and fullmove number placeholders (not stored in BitPosition)
        fen += " 0 1"; // These values would need to be tracked elsewhere for accuracy

        return fen;
    }
    bool posIsFine() const
    {
        uint64_t recalculated_pieces[2][6] = {};
        uint64_t recalculated_pieces_bit[2] = {};
        uint64_t recalculated_all_pieces = 0;

        // Check white board
        for (int sq = 0; sq < 64; ++sq)
        {
            int piece = m_white_board[sq];
            if (piece < 6)
            { // valid piece
                recalculated_pieces[0][piece] |= (1ULL << sq);
                recalculated_pieces_bit[0] |= (1ULL << sq);
                recalculated_all_pieces |= (1ULL << sq);
            }
            else if (piece != 7)
            {
                std::cerr << "[posIsFine] Invalid white piece index at " << sq << ": " << piece << "\n";
                return false;
            }
        }

        // Check black board
        for (int sq = 0; sq < 64; ++sq)
        {
            int piece = m_black_board[sq];
            if (piece < 6)
            {
                recalculated_pieces[1][piece] |= (1ULL << sq);
                recalculated_pieces_bit[1] |= (1ULL << sq);
                recalculated_all_pieces |= (1ULL << sq);
            }
            else if (piece != 7)
            {
                std::cerr << "[posIsFine] Invalid black piece index at " << sq << ": " << piece << "\n";
                return false;
            }
        }

        // Compare bitboards
        for (int color = 0; color < 2; ++color)
        {
            for (int pt = 0; pt < 6; ++pt)
            {
                if (recalculated_pieces[color][pt] != m_pieces[color][pt])
                {
                    std::cerr << "[posIsFine] m_pieces mismatch at color " << color << ", pt " << pt << "\n";
                    return false;
                }
            }
            if (recalculated_pieces_bit[color] != m_pieces_bit[color])
            {
                std::cerr << "[posIsFine] m_pieces_bit mismatch at color " << color << "\n";
                return false;
            }
        }

        if (recalculated_all_pieces != m_all_pieces_bit)
        {
            std::cerr << "[posIsFine] m_all_pieces_bit mismatch\n";
            return false;
        }

        if (m_king_position[0] != getLeastSignificantBitIndex(m_pieces[0][5]))
        {
            std::cerr << "[posIsFine] m_king_position[0] mismatch\n";
            return false;
        }
        if (m_king_position[1] != getLeastSignificantBitIndex(m_pieces[1][5]))
        {
            std::cerr << "[posIsFine] m_king_position[1] mismatch\n";
            return false;
        }

        return true; // Everything checks out
    }
    template <typename T>
    bool moveIsFine(const T &move) const
    {
        int origin = move.getOriginSquare();
        int destination = move.getDestinationSquare();

        if (m_turn)
        {
            // Check that the origin square has a piece of the current player
            if (m_white_board[origin] == 7)
            {
                std::cerr << "[moveIsFine] No piece of current side at origin square " << origin << "\n";
                return false;
            }
            // Check that the destination square does NOT contain a piece of the current player
            if (m_white_board[destination] != 7)
            {
                std::cerr << "[moveIsFine] Destination square " << destination << " already occupied by same side\n";
                return false;
            }
        }
        else
        {
            // Check that the origin square has a piece of the current player
            if (m_black_board[origin] == 7)
            {
                std::cerr << "[moveIsFine] No piece of current side at origin square " << origin << "\n";
                return false;
            }
            // Check that the destination square does NOT contain a piece of the current player
            if (m_black_board[destination] != 7)
            {
                std::cerr << "[moveIsFine] Destination square " << destination << " already occupied by same side\n";
                return false;
            }
        }
        return true;
    }
};

#endif // BITPOSITION_H