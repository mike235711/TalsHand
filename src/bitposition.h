#ifndef BITPOSITION_H
#define BITPOSITION_H
#include <array>
#include <cstdint> // For fixed sized integers
#include "bit_utils.h" // Bit utility functions
#include "move.h"
#include "simd.h" // NNUEU updates
#include <iostream>
#include <sstream> 
#include <vector>
#include <unordered_set>


extern bool ENGINEISWHITE;
extern int16_t firstLayerWeights[640][8];
extern int16_t firstLayerInvertedWeights[640][8];

extern int8_t secondLayer1Weights[64][8 * 4];
extern int8_t secondLayer2Weights[64][8 * 4];

extern int8_t secondLayer1WeightsBlockWhiteTurn[8 * 4];
extern int8_t secondLayer2WeightsBlockWhiteTurn[8 * 4];
extern int8_t secondLayer1WeightsBlockBlackTurn[8 * 4];
extern int8_t secondLayer2WeightsBlockBlackTurn[8 * 4];

extern int8_t thirdLayerWeights[8 * 4];
extern int8_t finalLayerWeights[4];

extern int16_t firstLayerBiases[8];
extern int16_t secondLayerBiases[8];
extern int16_t thirdLayerBiases[4];
extern int16_t finalLayerBias;

struct StateInfo
{
    // Copied when making a move
    bool whiteKSCastling;
    bool whiteQSCastling;
    bool blackKSCastling;
    bool blackQSCastling;
    int fiftyMoveCount;
    int pSquare; // Used to update zobrist key
    uint64_t zobristKey;
    uint64_t blockersForKing;
    int16_t inputWhiteTurn[8];
    int16_t inputBlackTurn[8];

    // Not copied when making a move (will be recomputed anyhow), used for unmaking moves
    uint64_t straightPinnedPieces;
    uint64_t diagonalPinnedPieces;
    uint64_t pinnedPieces;
    uint64_t lastDestinationBit; // For refutation moves after unmaking a Ttmove
    int capturedPiece;
    int lastOriginSquare; // For when calling setCheckInfo()
    bool isCheck;

    // Pointers to previous and next state
    StateInfo *previous;
    StateInfo *next;
};

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

    // For updating stuff in makeMove, makeCapture and makeTTMove
    uint64_t m_moved_piece{7};
    uint64_t m_promoted_piece{7};
    int m_last_destination_square;

    // int representing kings' positions
    int m_white_king_position{};
    int m_black_king_position{};

    // For discovered checks
    bool m_blockers_set{false};

    // Bits representing check info
    int m_check_square{65};
    uint64_t m_check_rays{0};
    int m_num_checks{0};

    // Ply number
    int m_ply{0};
    
    // Ply info arrays
    std::array<uint64_t, 128> m_zobrist_keys_array{};

    StateInfo *state_info;

    uint64_t m_last_nnue_wp_bits;
    uint64_t m_last_nnue_wn_bits;
    uint64_t m_last_nnue_wb_bits;
    uint64_t m_last_nnue_wr_bits;
    uint64_t m_last_nnue_wq_bits;

    uint64_t m_last_nnue_bp_bits;
    uint64_t m_last_nnue_bn_bits;
    uint64_t m_last_nnue_bb_bits;
    uint64_t m_last_nnue_br_bits;
    uint64_t m_last_nnue_bq_bits;

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
          m_turn(turn)
    {
        state_info = new StateInfo(); // Allocate memory for state_info
        state_info->whiteKSCastling = white_kingside_castling;
        state_info->whiteQSCastling = white_queenside_castling;
        state_info->blackKSCastling = black_kingside_castling;
        state_info->blackQSCastling = black_queenside_castling;

        m_white_pieces_bit = m_white_pawns_bit | m_white_knights_bit | m_white_bishops_bit | m_white_rooks_bit | m_white_queens_bit | m_white_king_bit;
        m_black_pieces_bit = m_black_pawns_bit | m_black_knights_bit | m_black_bishops_bit | m_black_rooks_bit | m_black_queens_bit | m_black_king_bit;
        m_all_pieces_bit = m_white_pieces_bit | m_black_pieces_bit;

        setKingPosition();
        setIsCheckOnInitialization();
        setCheckInfoOnInitialization();
        setBlockersAndPinsInAB();
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
        state_info = new StateInfo(); // Allocate memory for state_info
        // Parse castling rights
        state_info->whiteKSCastling = (castling.find('K') != std::string::npos);
        state_info->whiteQSCastling = (castling.find('Q') != std::string::npos);
        state_info->blackKSCastling = (castling.find('k') != std::string::npos);
        state_info->blackQSCastling = (castling.find('q') != std::string::npos);

        state_info->pSquare = 0;
        
        setAllPiecesBits();
        setKingPosition();
        setIsCheckOnInitialization();
        if (getIsCheck())
            setCheckInfoOnInitialization();
        setBlockersAndPinsInAB();
        initializeZobristKey();
    }

    void setIsCheckOnInitialization();
    void setCheckInfoOnInitialization();

    void initializeZobristKey();
    void updateZobristKeyPiecePartAfterMove(int origin_square, int destination_square);
    void updateAccumulatorAfterMove(int origin_square, int destination_square);

    void setBlockersAndPinsInAB();
    void setBlockersAndPinsInQS();

    template <typename T>
    bool isLegal(const T *move) const;
    template <typename T>
    bool isCaptureLegal(const T *move) const;

    bool isRefutationLegal(Move move) const;
    bool isNormalMoveLegal(int origin_square, int destination_square) const;

    bool ttMoveIsOk(Move move) const;

    // These set the checks bits, m_is_check and m_num_checks    
    void setDiscoverCheckForWhite();
    void setDiscoverCheckForBlack();

    void setCheckInfo();

    bool newWhiteKingSquareIsSafe(int new_position) const;
    bool newBlackKingSquareIsSafe(int new_position) const;

    bool whiteKingSquareIsSafe() const;
    bool blackKingSquareIsSafe() const;

    bool kingIsSafeAfterPassant(int removed_square_1, int removed_square_2) const;

    ScoredMove *pawnAllMoves(ScoredMove *&move_list) const;
    ScoredMove *knightAllMoves(ScoredMove *&move_list) const;
    ScoredMove *bishopAllMoves(ScoredMove *&move_list) const;
    ScoredMove *rookAllMoves(ScoredMove *&move_list) const;
    ScoredMove *queenAllMoves(ScoredMove *&move_list) const;
    ScoredMove *kingAllMoves(ScoredMove *&move_list) const;

    Move *kingAllMovesInCheck(Move *&move_list) const;

    Move getBestRefutation();

    ScoredMove *pawnCapturesAndQueenProms(ScoredMove *&move_list) const;
    ScoredMove *knightCaptures(ScoredMove *&move_list) const;
    ScoredMove *bishopCaptures(ScoredMove *&move_list) const;
    ScoredMove *rookCaptures(ScoredMove *&move_list) const;
    ScoredMove *queenCaptures(ScoredMove *&move_list) const;
    ScoredMove *kingCaptures(ScoredMove *&move_list) const;
    Move *kingCaptures(Move *&move_list) const;

    ScoredMove *setRefutationMovesOrdered(ScoredMove *&move_list);
    ScoredMove *setGoodCapturesOrdered(ScoredMove *&move_list);

    ScoredMove *pawnRestMoves(ScoredMove *&move_list) const;
    ScoredMove *knightRestMoves(ScoredMove *&move_list) const;
    ScoredMove *bishopRestMoves(ScoredMove *&move_list) const;
    ScoredMove *rookRestMoves(ScoredMove *&move_list) const;
    ScoredMove *queenRestMoves(ScoredMove *&move_list) const;
    ScoredMove *kingNonCapturesAndPawnCaptures(ScoredMove *&move_list) const;

    Move *inCheckPawnBlocks(Move *&move_list) const;
    Move *inCheckKnightBlocks(Move *&move_list) const;
    Move *inCheckBishopBlocks(Move *&move_list) const;
    Move *inCheckRookBlocks(Move *&move_list) const;
    Move *inCheckQueenBlocks(Move *&move_list) const;

    bool isDiscoverCheckForWhiteAfterPassant(int origin_square, int destination_square) const;
    bool isDiscoverCheckForBlackAfterPassant(int origin_square, int destination_square) const;

    bool isDiscoverCheckForWhite(int origin_square, int destination_square) const;
    bool isDiscoverCheckForBlack(int origin_square, int destination_square) const;

    bool isPawnCheckOrDiscoverForWhite(int origin_square, int destination_square) const;
    bool isKnightCheckOrDiscoverForWhite(int origin_square, int destination_square) const;
    bool isBishopCheckOrDiscoverForWhite(int origin_square, int destination_square) const;
    bool isRookCheckOrDiscoverForWhite(int origin_square, int destination_square) const;
    bool isQueenCheckOrDiscoverForWhite(int origin_square, int destination_square) const;

    bool isPawnCheckOrDiscoverForBlack(int origin_square, int destination_square) const;
    bool isKnightCheckOrDiscoverForBlack(int origin_square, int destination_square) const;
    bool isBishopCheckOrDiscoverForBlack(int origin_square, int destination_square) const;
    bool isRookCheckOrDiscoverForBlack(int origin_square, int destination_square) const;
    bool isQueenCheckOrDiscoverForBlack(int origin_square, int destination_square) const;

    Move *inCheckOrderedCapturesAndKingMoves(Move *&move_list) const;
    Move *inCheckOrderedCaptures(Move *&move_list) const;

    std::vector<Move> orderAllMovesOnFirstIterationFirstTime(std::vector<Move> &moves, Move ttMove) const;
    std::pair<std::vector<Move>, std::vector<int16_t>> orderAllMovesOnFirstIteration(std::vector<Move> &moves, std::vector<int16_t> &scores) const;
    std::vector<Move> inCheckMoves() const;
    std::vector<Move> nonCaptureMoves() const;

    bool isMate() const;

    std::array<int, 64> m_50_move_count_array{};

    void setPiece(uint64_t origin_bit, uint64_t destination_bit);
    bool moveIsReseter(Move move);
    void resetPlyInfo();

    template <typename T>
    void makeMove(T move, StateInfo& new_state_info);
    template <typename T>
    void unmakeMove(T move);

    template <typename T>
    void makeCapture(T move, StateInfo& new_state_info);
    template <typename T>
    void unmakeCapture(T move);

    template <typename T>
    void makeCaptureTest(T move, StateInfo &new_state_info);

    std::vector<Move> inCheckAllMoves();
    std::vector<Move> allMoves();

    uint64_t updateZobristKeyPiecePartBeforeMove(int origin_square, int destination_square, int promoted_piece) const;
    template <typename T>
    bool positionIsDrawnAfterMove(T move) const;

    int pieceValueFrom(int square) const
    {
        if (m_turn)
        {
            if (square == m_white_king_position)
                return 0;
            uint64_t bit = 1ULL << square;
            if (bit & m_white_pawns_bit)
                return 4;
            else if (bit & (m_white_knights_bit| m_white_bishops_bit))
                return 10;
            else if (bit & m_white_rooks_bit)
                return 20;
            else
                return 36;
        }
        else
        {
            if (square == m_black_king_position)
                return 0;
            uint64_t bit = 1ULL << square;
            if (bit & m_black_pawns_bit)
                return 4;
            else if (bit & (m_black_knights_bit | m_black_bishops_bit))
                return 10;
            else if (bit & m_black_rooks_bit)
                return 20;
            else
                return 36;
        }
    }
    int pieceValueTo(int square) const
    {
        uint64_t bit = (1ULL << square);
        if (m_turn)
        {
            if (bit & m_black_pieces_bit)
            {
                if (bit & m_black_pawns_bit)
                    return 5;
                else if (bit & (m_black_knights_bit | m_black_bishops_bit))
                    return 12;
                else if (bit & m_black_rooks_bit)
                    return 23;
                else
                    return 40;
            }
            return 0;
        }
        else
        {
            if (bit & m_white_pieces_bit)
            {
                if (bit & m_white_pawns_bit)
                    return 5;
                else if (bit & (m_white_knights_bit | m_white_bishops_bit))
                    return 12;
                else if (bit & m_white_rooks_bit)
                    return 23;
                else
                    return 40;
            }
            return 0;
        }
    }

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

    inline StateInfo *get_state_info() const { return state_info; }

    // NNUEU updates
    // Helper functions to update the input vector
    // They are used in bitposition.cpp inside makeNormalMove, makeCapture, setPiece and removePiece.
    void addOnInput(int subIndex)
    {
        // White turn (use normal NNUE)
        add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[subIndex]);
        // Black turn (use inverted NNUE)
        add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[subIndex]);
    }
    void removeOnInput(int subIndex)
    {
        // White turn (use normal NNUE)
        substract_8_int16(state_info->inputWhiteTurn, firstLayerWeights[subIndex]);
        // Black turn (use inverted NNUE)
        substract_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[subIndex]);
    }
    // Move King functions to update nnueInput vector
    void moveWhiteKingNNUEInput()
    {
        std::memcpy(secondLayer1WeightsBlockWhiteTurn, secondLayer1Weights[m_white_king_position], 32);
        std::memcpy(secondLayer2WeightsBlockBlackTurn, secondLayer2Weights[invertIndex(m_white_king_position)], 32);
    }
    void moveBlackKingNNUEInput()
    {
        std::memcpy(secondLayer2WeightsBlockWhiteTurn, secondLayer2Weights[m_black_king_position], 32);
        std::memcpy(secondLayer1WeightsBlockBlackTurn, secondLayer1Weights[invertIndex(m_black_king_position)], 32);
    }

    void initializeNNUEInput()
    // Initialize the NNUE accumulators.
    {
        std::memcpy(state_info->inputWhiteTurn, firstLayerBiases, sizeof(firstLayerBiases));
        std::memcpy(state_info->inputBlackTurn, firstLayerBiases, sizeof(firstLayerBiases));

        for (unsigned short index : getBitIndices(m_white_pawns_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[index]);
        }

        for (unsigned short index : getBitIndices(m_white_knights_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 + index]);
        }

        for (unsigned short index : getBitIndices(m_white_bishops_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 2 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 2 + index]);
        }

        for (unsigned short index : getBitIndices(m_white_rooks_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 3 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 3 + index]);
        }

        for (unsigned short index : getBitIndices(m_white_queens_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 4 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 4 + index]);
        }

        for (unsigned short index : getBitIndices(m_black_pawns_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 5 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 5 + index]);
        }

        for (unsigned short index : getBitIndices(m_black_knights_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 6 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 6 + index]);
        }

        for (unsigned short index : getBitIndices(m_black_bishops_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 7 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 7 + index]);
        }

        for (unsigned short index : getBitIndices(m_black_rooks_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 8 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 8 + index]);
        }

        for (unsigned short index : getBitIndices(m_black_queens_bit))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 9 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 9 + index]);
        }

        std::memcpy(secondLayer1WeightsBlockWhiteTurn, secondLayer1Weights[m_white_king_position], sizeof(secondLayer1Weights[m_white_king_position]));
        std::memcpy(secondLayer1WeightsBlockBlackTurn, secondLayer1Weights[invertIndex(m_black_king_position)], sizeof(secondLayer1Weights[invertIndex(m_black_king_position)]));
        std::memcpy(secondLayer2WeightsBlockWhiteTurn, secondLayer2Weights[m_black_king_position], sizeof(secondLayer2Weights[m_black_king_position]));
        std::memcpy(secondLayer2WeightsBlockBlackTurn, secondLayer2Weights[invertIndex(m_white_king_position)], sizeof(secondLayer2Weights[invertIndex(m_white_king_position)]));

        setLastNNUEBits();
    }

    // This function is called ONLY when we enter quiscence search, NOT when we make a capture
    // We call it in initializeNNUEInput and updateAccumulator
    void setLastNNUEBits()
    {
        // White pieces
        m_last_nnue_wp_bits = m_white_pawns_bit;
        m_last_nnue_wn_bits = m_white_knights_bit;
        m_last_nnue_wb_bits = m_white_bishops_bit;
        m_last_nnue_wr_bits = m_white_rooks_bit;
        m_last_nnue_wq_bits = m_white_queens_bit;

        // Black pieces
        m_last_nnue_bp_bits = m_black_pawns_bit;
        m_last_nnue_bn_bits = m_black_knights_bit;
        m_last_nnue_bb_bits = m_black_bishops_bit;
        m_last_nnue_br_bits = m_black_rooks_bit;
        m_last_nnue_bq_bits = m_black_queens_bit;
    }

    void updateAccumulator1()
    {
        // Constants for piece indexing
        constexpr int pieceOffsets[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

        // Current and previous bitboards for each piece type
        const uint64_t currentBits[10] = {
            m_white_pawns_bit, m_white_knights_bit, m_white_bishops_bit,
            m_white_rooks_bit, m_white_queens_bit,
            m_black_pawns_bit, m_black_knights_bit, m_black_bishops_bit,
            m_black_rooks_bit, m_black_queens_bit};

        const uint64_t previousBits[10] = {
            m_last_nnue_wp_bits, m_last_nnue_wn_bits, m_last_nnue_wb_bits,
            m_last_nnue_wr_bits, m_last_nnue_wq_bits,
            m_last_nnue_bp_bits, m_last_nnue_bn_bits, m_last_nnue_bb_bits,
            m_last_nnue_br_bits, m_last_nnue_bq_bits};

        // ADD PIECES
        for (int i = 0; i < 10; ++i)
        {
            uint64_t piecesAdd = currentBits[i] & ~previousBits[i];
            while (piecesAdd)
            {
                addOnInput(64 * pieceOffsets[i] + popLeastSignificantBit(piecesAdd));
            }
        }

        // REMOVE PIECES
        for (int i = 0; i < 10; ++i)
        {
            uint64_t piecesRemove = ~currentBits[i] & previousBits[i];
            while (piecesRemove)
            {
                removeOnInput(64 * pieceOffsets[i] + popLeastSignificantBit(piecesRemove));
            }
        }
        setLastNNUEBits();
    }

    void updateAccumulator()
    {
        uint64_t piecesAdd, piecesRemove;

        // === ADD PIECES ===

        // White pawns
        piecesAdd = m_white_pawns_bit & ~m_last_nnue_wp_bits;
        while (piecesAdd)
            addOnInput(popLeastSignificantBit(piecesAdd));

        // White knights
        piecesAdd = m_white_knights_bit & ~m_last_nnue_wn_bits;
        while (piecesAdd)
            addOnInput(64 + popLeastSignificantBit(piecesAdd));

        // White bishops
        piecesAdd = m_white_bishops_bit & ~m_last_nnue_wb_bits;
        while (piecesAdd)
            addOnInput(64 * 2 + popLeastSignificantBit(piecesAdd));
        // White rooks
        piecesAdd = m_white_rooks_bit & ~m_last_nnue_wr_bits;
        while (piecesAdd)
            addOnInput(64 * 3 + popLeastSignificantBit(piecesAdd));

        // White queens
        piecesAdd = m_white_queens_bit & ~m_last_nnue_wq_bits;
        while (piecesAdd)
            addOnInput(64 * 4 + popLeastSignificantBit(piecesAdd));

        // Black pawns
        piecesAdd = m_black_pawns_bit & ~m_last_nnue_bp_bits;
        while (piecesAdd)
            addOnInput(64 * 5 + popLeastSignificantBit(piecesAdd));

        // Black knights
        piecesAdd = m_black_knights_bit & ~m_last_nnue_bn_bits;
        while (piecesAdd)
            addOnInput(64 * 6 + popLeastSignificantBit(piecesAdd));

        // Black bishops
        piecesAdd = m_black_bishops_bit & ~m_last_nnue_bb_bits;
        while (piecesAdd)
            addOnInput(64 * 7 + popLeastSignificantBit(piecesAdd));

        // Black rooks
        piecesAdd = m_black_rooks_bit & ~m_last_nnue_br_bits;
        while (piecesAdd)
            addOnInput(64 * 8 + popLeastSignificantBit(piecesAdd));

        // Black queens
        piecesAdd = m_black_queens_bit & ~m_last_nnue_bq_bits;
        while (piecesAdd)
            addOnInput(64 * 9 + popLeastSignificantBit(piecesAdd));

        // === REMOVE PIECES ===

        // White pawns
        piecesRemove = ~m_white_pawns_bit & m_last_nnue_wp_bits;
        while (piecesRemove)
            removeOnInput(popLeastSignificantBit(piecesRemove));

        // White knights
        piecesRemove = ~m_white_knights_bit & m_last_nnue_wn_bits;
        while (piecesRemove)
            removeOnInput(64 + popLeastSignificantBit(piecesRemove));

        // White bishops
        piecesRemove = ~m_white_bishops_bit & m_last_nnue_wb_bits;
        while (piecesRemove)
            removeOnInput(64 * 2 + popLeastSignificantBit(piecesRemove));

        // White rooks
        piecesRemove = ~m_white_rooks_bit & m_last_nnue_wr_bits;
        while (piecesRemove)
            removeOnInput(64 * 3 + popLeastSignificantBit(piecesRemove));

        // White queens
        piecesRemove = ~m_white_queens_bit & m_last_nnue_wq_bits;
        while (piecesRemove)
            removeOnInput(64 * 4 + popLeastSignificantBit(piecesRemove));

        // Black pawns
        piecesRemove = ~m_black_pawns_bit & m_last_nnue_bp_bits;
        while (piecesRemove)
            removeOnInput(64 * 5 + popLeastSignificantBit(piecesRemove));

        // Black knights
        piecesRemove = ~m_black_knights_bit & m_last_nnue_bn_bits;
        while (piecesRemove)
            removeOnInput(64 * 6 + popLeastSignificantBit(piecesRemove));

        // Black bishops
        piecesRemove = ~m_black_bishops_bit & m_last_nnue_bb_bits;
        while (piecesRemove)
            removeOnInput(64 * 7 + popLeastSignificantBit(piecesRemove));

        // Black rooks
        piecesRemove = ~m_black_rooks_bit & m_last_nnue_br_bits;
        while (piecesRemove)
            removeOnInput(64 * 8 + popLeastSignificantBit(piecesRemove));

        // Black queens
        piecesRemove = ~m_black_queens_bit & m_last_nnue_bq_bits;
        while (piecesRemove)
            removeOnInput(64 * 9 + popLeastSignificantBit(piecesRemove));

        setLastNNUEBits();
    }

    // The engine is built to get an evaluation of the position with high values being good for the engine.
    // The NNUE is built to give an evaluation of the position with high values being good for whose turn it is.
    // This function gives an evaluation with high values being good for engine.
    int16_t evaluationFunction(bool ourTurn)
    {
        int16_t out;

        // If its our turn in white, or if its not our turn in black then it is white's turn
        if (ourTurn == ENGINEISWHITE)
        {
            out = fullNnueuPass(state_info->inputWhiteTurn, secondLayer1WeightsBlockWhiteTurn, secondLayer2WeightsBlockWhiteTurn, secondLayerBiases,
                                thirdLayerWeights, thirdLayerBiases, finalLayerWeights, &finalLayerBias);
        }
        else
        {
            out = fullNnueuPass(state_info->inputBlackTurn, secondLayer1WeightsBlockBlackTurn, secondLayer2WeightsBlockBlackTurn, secondLayerBiases,
                                thirdLayerWeights, thirdLayerBiases, finalLayerWeights, &finalLayerBias);
        }
        // Change evaluation from player to move perspective to white perspective
        if (ourTurn)
            return out;

        return 64 * 64 - out;
    }

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

    Move *setNonCaptures(Move *&move_list_start);
    Move *setNonCapturesInCheck(Move *&move_list_start);

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

    bool hasBlockersUnset() const { return not m_blockers_set;  }

    inline bool getIsCheck() const 
    {
        return state_info->isCheck;
    }
    int getNumChecks() const
    {
        return m_num_checks;
    }
    bool sliderChecking() const { return m_check_rays != 0; }

    uint64_t getZobristKey() const
    {
        return state_info->zobristKey;
    }
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
    void print50MoveCount()
    {
        std::cout << state_info->fiftyMoveCount << "\n";
    }
    bool moreThanOneCheck() const { return m_num_checks > 1; }

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

    int getMovedPiece() const { return m_moved_piece; }
    int getCapturedPiece() const { return state_info->capturedPiece; }
    int getPromotedPiece() const { return m_promoted_piece; }
    int getWhiteKingPosition() const { return m_white_king_position; }
    int getBlackKingPosition() const { return m_black_king_position; }

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

        std::cout << "psquare " << state_info->pSquare << "\n";

        std::cout << "White king position " << m_white_king_position << "\n";
        std::cout << "Black king position " << m_black_king_position << "\n";
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
        if (!state_info->whiteKSCastling && !state_info->whiteQSCastling &&
            !state_info->blackKSCastling && !state_info->blackKSCastling)
        {
            fen += '-';
        }
        else
        {
            if (state_info->whiteKSCastling)
                fen += 'K';
            if (state_info->whiteQSCastling)
                fen += 'Q';
            if (state_info->blackKSCastling)
                fen += 'k';
            if (state_info->blackQSCastling)
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