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
#include <cstring>
#include <algorithm>

extern bool ENGINEISWHITE;

extern int16_t firstLayerWeights2Indices[640][640][8];
extern int16_t firstLayerInvertedWeights2Indices[640][640][8];

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
    uint64_t blockersForKing;
    int16_t inputWhiteTurn[8]; // NUUEU Input
    int16_t inputBlackTurn[8]; // NNUEU Input
    int reversibleMovesMade; // Used for three-fold checks
    int pSquare; // Used to update zobrist key
    uint64_t zobristKey;

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
    // Board of color pieces 7s where no pieces
    int m_white_board[64];
    int m_black_board[64];

    // 64-bit to represent pieces on board
    uint64_t m_pieces[2][6];

    // Bits to represent all pieces of each player
    uint64_t m_pieces_bit[2];

    uint64_t m_all_pieces_bit{};

    // True white's turn, False black's
    bool m_turn{};

    // For updating stuff in makeMove, makeCapture and makeTTMove
    uint64_t m_moved_piece{7};
    uint64_t m_promoted_piece{7};
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
    
    // Ply info arrays
    std::array<uint64_t, 128> m_zobrist_keys_array{};

    StateInfo *state_info;

    uint64_t m_last_nnue_bits[2][6];

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

    void setCheckInfo();

    bool newKingSquareIsSafe(int new_position) const;

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

    bool isDiscoverCheckAfterPassant(int origin_square, int destination_square) const;

    bool isDiscoverCheck(int origin_square, int destination_square) const;

    bool isPawnCheckOrDiscover(int origin_square, int destination_square) const;
    bool isKnightCheckOrDiscover(int origin_square, int destination_square) const;
    bool isBishopCheckOrDiscover(int origin_square, int destination_square) const;
    bool isRookCheckOrDiscover(int origin_square, int destination_square) const;
    bool isQueenCheckOrDiscover(int origin_square, int destination_square) const;

    bool isPieceCheckOrDiscover(int moved_piece, int origin_square, int destination_square) const;

    Move *inCheckOrderedCapturesAndKingMoves(Move *&move_list) const;
    Move *inCheckOrderedCaptures(Move *&move_list) const;

    std::vector<Move> orderAllMovesOnFirstIterationFirstTime(std::vector<Move> &moves, Move ttMove) const;
    std::pair<std::vector<Move>, std::vector<int16_t>> orderAllMovesOnFirstIteration(std::vector<Move> &moves, std::vector<int16_t> &scores) const;
    std::vector<Move> inCheckMoves() const;
    std::vector<Move> nonCaptureMoves() const;

    bool isMate() const;

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

    bool isDraw() const;

    int aBMoveValue(Move move) const
    {
        // Promotions and castling
        if (move.getData() & 0b0100000000000000)
        {
            // Promotions
            if (m_turn && m_white_board[move.getOriginSquare()] == 0)
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
                score += piece_at + 2;

            piece_at = m_white_board[move.getOriginSquare()];
            if (piece_at != 7 && piece_at != 5)
                score -= piece_at;
        }
        else
        {
            int piece_at = m_white_board[move.getDestinationSquare()];
            if (piece_at != 7)
                score += piece_at + 2;

            piece_at = m_black_board[move.getOriginSquare()];
            if (piece_at != 7 && piece_at != 5)
                score -= piece_at;
        }
        return score;
    }
    int qSMoveValue(Move move) const
    // Captures and queen promotions
    {
        // Promotions
        if (move.getData() & 0b0100000000000000)
        {
            return 30;
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

    // NNUEU updates
    // Helper functions to update the input vector
    // They are used in bitposition.cpp inside makeCapture.
    void addAndRemoveOnInput(int subIndexAdd, int subIndexRemove)
    {
        // White turn (use normal NNUE)
        add_8_int16(state_info->inputWhiteTurn, firstLayerWeights2Indices[subIndexAdd][subIndexRemove]);
        // Black turn (use inverted NNUE)
        add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights2Indices[subIndexAdd][subIndexRemove]);
    }
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
        std::memcpy(secondLayer1WeightsBlockWhiteTurn, secondLayer1Weights[m_king_position[0]], 32);
        std::memcpy(secondLayer2WeightsBlockBlackTurn, secondLayer2Weights[invertIndex(m_king_position[0])], 32);
    }
    void moveBlackKingNNUEInput()
    {
        std::memcpy(secondLayer2WeightsBlockWhiteTurn, secondLayer2Weights[m_king_position[1]], 32);
        std::memcpy(secondLayer1WeightsBlockBlackTurn, secondLayer1Weights[invertIndex(m_king_position[1])], 32);
    }

    void initializeNNUEInput()
    // Initialize the NNUE accumulators.
    {
        std::memcpy(state_info->inputWhiteTurn, firstLayerBiases, sizeof(firstLayerBiases));
        std::memcpy(state_info->inputBlackTurn, firstLayerBiases, sizeof(firstLayerBiases));

        for (unsigned short index : getBitIndices(m_pieces[0][0]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[0][1]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 + index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[0][2]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 2 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 2 + index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[0][3]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 3 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 3 + index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[0][4]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 4 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 4 + index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[1][0]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 5 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 5 + index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[1][1]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 6 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 6 + index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[1][2]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 7 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 7 + index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[1][3]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 8 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 8 + index]);
        }

        for (unsigned short index : getBitIndices(m_pieces[1][4]))
        {
            add_8_int16(state_info->inputWhiteTurn, firstLayerWeights[64 * 9 + index]);
            add_8_int16(state_info->inputBlackTurn, firstLayerInvertedWeights[64 * 9 + index]);
        }

        std::memcpy(secondLayer1WeightsBlockWhiteTurn, secondLayer1Weights[m_king_position[0]], sizeof(secondLayer1Weights[m_king_position[0]]));
        std::memcpy(secondLayer1WeightsBlockBlackTurn, secondLayer1Weights[invertIndex(m_king_position[1])], sizeof(secondLayer1Weights[invertIndex(m_king_position[1])]));
        std::memcpy(secondLayer2WeightsBlockWhiteTurn, secondLayer2Weights[m_king_position[1]], sizeof(secondLayer2Weights[m_king_position[1]]));
        std::memcpy(secondLayer2WeightsBlockBlackTurn, secondLayer2Weights[invertIndex(m_king_position[0])], sizeof(secondLayer2Weights[invertIndex(m_king_position[0])]));

        setLastNNUEBits();
    }

    // This function is called ONLY when we enter quiscence search, NOT when we make a capture
    // We call it in initializeNNUEInput and updateAccumulator
    void setLastNNUEBits()
    {
        std::memcpy(m_last_nnue_bits, m_pieces, 8 * 6 * 2);
    }

    void updateAccumulator()
    {
        // ADD PIECES
        for (int i = 0; i < 5; ++i)
        {
            uint64_t piecesAdd = m_pieces[0][i] & ~m_last_nnue_bits[0][i];
            while (piecesAdd)
            {
                addOnInput(64 * i + popLeastSignificantBit(piecesAdd));
            }
            piecesAdd = m_pieces[1][i] & ~m_last_nnue_bits[1][i];
            while (piecesAdd)
            {
                addOnInput(64 * (i + 5) + popLeastSignificantBit(piecesAdd));
            }
        }

        // REMOVE PIECES
        for (int i = 0; i < 5; ++i)
        {
            uint64_t piecesAdd = ~m_pieces[0][i] & m_last_nnue_bits[0][i];
            while (piecesAdd)
            {
                removeOnInput(64 * i + popLeastSignificantBit(piecesAdd));
            }
            piecesAdd = ~m_pieces[1][i] & m_last_nnue_bits[1][i];
            while (piecesAdd)
            {
                removeOnInput(64 * (i + 5) + popLeastSignificantBit(piecesAdd));
            }
        }
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