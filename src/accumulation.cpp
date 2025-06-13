#include <fstream>
#include <sstream>
#include <cassert>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifdef __SSE4_1__
#include <smmintrin.h> // SSE4.1
#endif

#include "bitposition.h"
#include "bit_utils.h" // Bit utility functions
#include "precomputed_moves.h"
#include "accumulation.h"
#include "network.h"

// Function to load a 2D int8_t array from a file
void load_int8_2D_array1(const std::string &file_path, int8_t weights[64][8 * 4])
{
    std::ifstream file(file_path);
    std::string line;
    size_t row = 0;

    while (std::getline(file, line) && row < 4)
    {
        std::stringstream ss(line);
        std::string value;
        size_t col = 0;

        while (std::getline(ss, value, ',') && col < 64 * 8)
        {
            weights[col / 8][(col % 8) + row * 8] = static_cast<int8_t>(std::stoi(value));
            col++;
        }
        row++;
    }
}
void load_int16_2D_array1(const std::string &file_path, int16_t weights[640][8])
{
    std::ifstream file(file_path);
    std::string line;
    size_t row = 0;

    while (std::getline(file, line) && row < 8)
    {
        std::stringstream ss(line);
        std::string value;
        size_t col = 0;

        while (std::getline(ss, value, ',') && col < 640)
        {
            weights[col][row] = static_cast<int16_t>(std::stoi(value));
            col++;
        }
        row++;
    }
}

void load_inverted_int16_2D_array1(const std::string &file_path, int16_t weights[640][8])
{
    std::ifstream file(file_path);
    std::string line;
    size_t row = 0;

    while (std::getline(file, line) && row < 8)
    {
        std::stringstream ss(line);
        std::string value;
        size_t col = 0;

        while (std::getline(ss, value, ',') && col < 640)
        {
            int pieceType = col / 64;
            int square = col % 64;

            // Compute the new column after inverting color and square
            int newPieceType = (pieceType + 5) % 10;
            int newCol = newPieceType * 64 + invertIndex(square);
            weights[newCol][row] = static_cast<int16_t>(std::stoi(value));
            col++;
        }
        row++;
    }
}
    // Initialize Accumulators
void NNUEU::AccumulatorState::initialize(const BitPosition &position, const Transformer &transformer)
{
    // Start accumulators with the firstLayerBiases
    std::memcpy(inputTurn[0], transformer.weights.firstBias, sizeof(transformer.weights.firstBias));
    std::memcpy(inputTurn[1], transformer.weights.firstBias, sizeof(transformer.weights.firstBias));

    // White pawns
    for (unsigned short index : getBitIndices(position.getPieces(0, 0)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[index]);
    }
    // White knights
    for (unsigned short index : getBitIndices(position.getPieces(0, 1)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 + index]);
    }
    // White bishops
    for (unsigned short index : getBitIndices(position.getPieces(0, 2)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 * 2 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 * 2 + index]);
    }
    // White rooks
    for (unsigned short index : getBitIndices(position.getPieces(0, 3)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 * 3 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 * 3 + index]);
    }
    // White queens
    for (unsigned short index : getBitIndices(position.getPieces(0, 4)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 * 4 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 * 4 + index]);
    }

    // Black pawns
    for (unsigned short index : getBitIndices(position.getPieces(1, 0)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 * 5 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 * 5 + index]);
    }
    // Black knights
    for (unsigned short index : getBitIndices(position.getPieces(1, 1)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 * 6 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 * 6 + index]);
    }
    // Black bishops
    for (unsigned short index : getBitIndices(position.getPieces(1, 2)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 * 7 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 * 7 + index]);
    }
    // Black rooks
    for (unsigned short index : getBitIndices(position.getPieces(1, 3)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 * 8 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 * 8 + index]);
    }
    // Black queens
    for (unsigned short index : getBitIndices(position.getPieces(1, 4)))
    {
        add_8_int16(inputTurn[0], transformer.weights.firstW[64 * 9 + index]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[64 * 9 + index]);
    }     
    }

    // Functions to add/remove features from the accumulators
    inline void NNUEU::AccumulatorState::addAndRemoveOnInput(int subIndexAdd, int subIndexRemove, bool turn, const Transformer &transformer)
    {
        assert(subIndexAdd >= 0 && subIndexAdd < 640 && subIndexRemove >= 0 && subIndexRemove < 640);
        if (not turn)
            add_8_int16(inputTurn[0], transformer.weights.firstW2Indices[subIndexAdd][subIndexRemove]);
        else
            add_8_int16(inputTurn[1], transformer.weights.firstW2IndicesInv[subIndexAdd][subIndexRemove]);
    }

    inline void NNUEU::AccumulatorState::add_8_int16(int16_t *a, const int16_t *b)
    {
#if defined(__ARM_NEON)
        int16x8_t v1 = vld1q_s16(a); // Load 8 int16_t values from array a
        int16x8_t v2 = vld1q_s16(b); // Load 8 int16_t values from array b

        vst1q_s16(a, vaddq_s16(v1, v2)); // Store the result back to array a
#elif defined(__AVX2__) || defined(__SSE2__) || defined(__SSE4_1__)
        __m128i v1 = _mm_loadu_si128((__m128i *)a);
        __m128i v2 = _mm_loadu_si128((__m128i *)b);
        __m128i sum = _mm_add_epi16(v1, v2);
        _mm_storeu_si128((__m128i *)a, sum);
#else
        // Fallback scalar code
        for (int i = 0; i < 8; i++)
            a[i] += b[i];
#endif
    }

    inline void NNUEU::AccumulatorState::substract_8_int16(int16_t *a, const int16_t *b) // For NNUE accumulation
    {
#if defined(__ARM_NEON)
        int16x8_t v1 = vld1q_s16(a); // Load 8 int16_t values from array a
        int16x8_t v2 = vld1q_s16(b); // Load 8 int16_t values from array b

        vst1q_s16(a, vsubq_s16(v1, v2)); // Store the result back to array a

#elif defined(__AVX2__) || defined(__SSE2__) || defined(__SSE4_1__)
        __m128i v1 = _mm_loadu_si128((const __m128i *)a);
        __m128i v2 = _mm_loadu_si128((const __m128i *)b);
        __m128i v_sub = _mm_sub_epi16(v1, v2);
        _mm_storeu_si128((__m128i *)a, v_sub);
#else
        // Fallback scalar code
        for (int i = 0; i < 8; i++)
            a[i] -= b[i];

#endif
    }

    inline void NNUEU::AccumulatorState::addOnInput(int subIndex, bool turn, const Transformer &transformer)
    {
        assert(subIndex >= 0 && subIndex < 640);
        if (not turn)
            add_8_int16(inputTurn[0], transformer.weights.firstW[subIndex]);
        else
            add_8_int16(inputTurn[1], transformer.weights.firstWInv[subIndex]);
    }

    inline void NNUEU::AccumulatorState::removeOnInput(int subIndex, bool turn, const Transformer &transformer)
    {
        assert(subIndex >= 0 && subIndex < 640);
        if (not turn)
            substract_8_int16(inputTurn[0], transformer.weights.firstW[subIndex]);
        else
            substract_8_int16(inputTurn[1], transformer.weights.firstWInv[subIndex]);
    }

    // Overload for a normal (non-capture) move: two indices
    void NNUEU::NNUEUChange::add(int idx0, int idx1)
    {
        assert(idx0 >= 0 && idx0 < 640);
        assert(idx1 >= 0 && idx1 < 640);
        is_capture = false; // No capture
        indices[0] = idx0;
        indices[1] = idx1;
    }

    // Overload for a capture move: three indices
    void NNUEU::NNUEUChange::add(int idx0, int idx1, int idx2)
    {
        assert(idx0 >= 0 && idx0 < 640);
        assert(idx1 >= 0 && idx1 < 640);
        assert(idx2 >= 0 && idx2 < 640);
        is_capture = true; // It's a capture
        indices[0] = idx0;
        indices[1] = idx1;
        indices[2] = idx2;
    }

    void NNUEU::NNUEUChange::addlast(int idx2)
    {
        assert(idx2 >= 0 && idx2 < 640);
        is_capture = true; // It's a capture
        indices[2] = idx2;
    }
    inline bool NNUEU::NNUEUChange::isKingMove() const
    {
        return indices[0] == indices[1];
    }
    inline bool NNUEU::NNUEUChange::isCapture() const
    {
        // King captured something, but didn't affect NNUE input for the king
        return is_capture;
    }

#ifndef NDEBUG
    void NNUEU::AccumulatorStack::verifyTopAgainstFresh(const BitPosition &pos, bool turn, const Transformer &transformer)
    {
        // 1. Build a *fresh* accumulator for reference
        AccumulatorState fresh;
        fresh.initialize(pos, transformer);
    
        // 2. Compare with the incrementally-updated top of the stack
        const AccumulatorState &inc = top();

        for (int i = 0; i < 8; ++i)
            assert(fresh.inputTurn[turn][i] == inc.inputTurn[turn][i] && "NNUEU incremental accumulation mismatch");
    }
#endif // NDEBUG

    // Reset to a new root position
    void NNUEU::AccumulatorStack::reset(const BitPosition &rootPos, const Transformer &transformer)
    {
        m_current_idx = 1;
        AccumulatorState &rootState = stack[0];

        // Build a fresh accumulator for the root
        rootState.initialize(rootPos, transformer);

        const int whiteKing = rootPos.getKingPosition(0);
        const int blackKing = rootPos.getKingPosition(1);

        assert(whiteKing >= 0 && whiteKing < 64);
        assert(blackKing >= 0 && blackKing < 64);

        // Set the king positions
        nnueu_king_positions[0] = whiteKing;
        nnueu_king_positions[1] = blackKing;
        rootState.computed[0] = true;
        rootState.computed[1] = true;

        secondLayer1WeightsBlockWhiteTurn = transformer.weights.second1[whiteKing];
        secondLayer2WeightsBlockBlackTurn = transformer.weights.second2[invertIndex(whiteKing)];
        secondLayer2WeightsBlockWhiteTurn = transformer.weights.second2[blackKing];
        secondLayer1WeightsBlockBlackTurn = transformer.weights.second1[invertIndex(blackKing)];
    }
    void NNUEU::AccumulatorStack::changeWhiteKingPosition(int kingPos, const Transformer &transformer)
    {
        assert(kingPos >= 0 && kingPos < 64);
        secondLayer1WeightsBlockWhiteTurn = transformer.weights.second1[kingPos];
        secondLayer2WeightsBlockBlackTurn = transformer.weights.second2[invertIndex(kingPos)];
        nnueu_king_positions[0] = kingPos;
    }
    void NNUEU::AccumulatorStack::changeBlackKingPosition(int kingPos, const Transformer &transformer)
    {
        assert(kingPos >= 0 && kingPos < 64);
        secondLayer2WeightsBlockWhiteTurn = transformer.weights.second2[kingPos];
        secondLayer1WeightsBlockBlackTurn = transformer.weights.second1[invertIndex(kingPos)];
        nnueu_king_positions[1] = kingPos;
    }

    void NNUEU::AccumulatorStack::push(const NNUEUChange &chngs)
    {
        assert(m_current_idx < stack.size()); // Ensure space exists
        stack[m_current_idx].newAcc(chngs);
        m_current_idx++;
    }

    // Pop the top state when unmaking a move
    void NNUEU::AccumulatorStack::pop()
    {
        assert(m_current_idx > 1); // Never pop below 1, since root accumulator should be computed
        m_current_idx--;
    }

    NNUEU::AccumulatorState &NNUEU::AccumulatorStack::top()
    {
        assert(m_current_idx - 1 < stack.size());
        assert(stack[m_current_idx - 1].computed[0] || stack[m_current_idx - 1].computed[1]);
        return stack[m_current_idx - 1];
    }
    // From top down to 0, find the first node that has both sides computed
    int NNUEU::AccumulatorStack::findLastComputedNode(bool turn) const
    {
        for (std::size_t curr_idx = m_current_idx - 2; curr_idx > 0; curr_idx--)
        {
            if (stack[curr_idx].computed[not turn])
                return curr_idx;
        }
        return 0;
    }
    void NNUEU::AccumulatorStack::forward_update_incremental(const int begin, bool turn, const Transformer &transformer)
    {
        for (int next = begin + 1; next < m_current_idx; next++)
            applyIncrementalChanges(stack[next], stack[next - 1], not turn, transformer);
    }

    // This applies the “NNUEUChange” to the current node
    void NNUEU::AccumulatorStack::applyIncrementalChanges(AccumulatorState &curr, const AccumulatorState &prev, bool turn, const Transformer &transformer)
    {
        assert(prev.computed[turn]);
        // Copy previous accumulators
        std::memcpy(curr.inputTurn[turn], prev.inputTurn[turn], 16);

        const NNUEUChange &c = curr.changes;

        if (c.isCapture())
            curr.removeOnInput(c.indices[2], turn, transformer);
        if (!c.isKingMove())
            curr.addAndRemoveOnInput(c.indices[0], c.indices[1], turn, transformer);
        curr.computed[turn] = true;
    }

    bool NNUEU::Transformer::load(const std::string &modelDir = "models/NNUEU_quantized_model_v4_param_350_epoch_10/")
    {
        try
        {
            // Load weights into fixed-size arrays
            load_int16_2D_array1(modelDir + "first_linear_weights.csv", weights.firstW);
            load_inverted_int16_2D_array1(modelDir + "first_linear_weights.csv", weights.firstWInv);

            load_int8_2D_array1(modelDir + "second_layer_turn_weights.csv", weights.second1);
            load_int8_2D_array1(modelDir + "second_layer_not_turn_weights.csv", weights.second2);

            // Load biases
            auto tempFirstLayerBiases = load_int16_array(modelDir + "first_linear_biases.csv", 8);
            std::memcpy(weights.firstBias, tempFirstLayerBiases, sizeof(int16_t) * 8);
            delete[] tempFirstLayerBiases;

        }
        catch (const std::exception &e)
        {
            std::cerr << "NNUEU load failed: " << e.what() << '\n';
            return false;
        }

        // Initialze double weights for addAndRemoveFromInput
        for (int i = 0; i < 640; i++)
        {
            for (int j = 0; j < 640; j++)
            {
                for (int k = 0; k < 8; k++)
                {
                    // Sum with overflow handling for weights.firstW2
                    int32_t sum1 = (int32_t)weights.firstW[i][k] - (int32_t)weights.firstW[j][k];
                    if (sum1 > INT16_MAX)
                    {
                        weights.firstW2Indices[i][j][k] = INT16_MAX;
                    }
                    else if (sum1 < INT16_MIN)
                    {
                        weights.firstW2Indices[i][j][k] = INT16_MIN;
                    }
                    else
                    {
                        weights.firstW2Indices[i][j][k] = (int16_t)sum1;
                    }

                    // Sum with overflow handling for firstLayerInvertedWeights2Indices
                    int32_t sum2 = (int32_t)weights.firstWInv[i][k] - (int32_t)weights.firstWInv[j][k];
                    if (sum2 > INT16_MAX)
                    {
                        weights.firstW2IndicesInv[i][j][k] = INT16_MAX;
                    }
                    else if (sum2 < INT16_MIN)
                    {
                        weights.firstW2IndicesInv[i][j][k] = INT16_MIN;
                    }
                    else
                    {
                        weights.firstW2IndicesInv[i][j][k] = (int16_t)sum2;
                    }
                }
            }
        }

        return true;
    }
