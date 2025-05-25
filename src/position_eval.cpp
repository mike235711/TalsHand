#include "position_eval.h"
#include <fstream>
#include <sstream>
#include "bitposition.h"
#include "bit_utils.h" // Bit utility functions
#include "precomputed_moves.h"
#include "simd.h"
#include <cassert>

// NNUEU parameter loading
int16_t firstLayerWeights2Indices[640][640][8] = {0};
int16_t firstLayerInvertedWeights2Indices[640][640][8] = {0};

int16_t firstLayerWeights[640][8] = {0};
int16_t firstLayerInvertedWeights[640][8] = {0};

int8_t secondLayer1Weights[64][8 * 4] = {0};
int8_t secondLayer2Weights[64][8 * 4] = {0};

static const int8_t *secondLayer1WeightsBlockWhiteTurn = nullptr;
static const int8_t *secondLayer2WeightsBlockWhiteTurn = nullptr;
static const int8_t *secondLayer1WeightsBlockBlackTurn = nullptr;
static const int8_t *secondLayer2WeightsBlockBlackTurn = nullptr;

int8_t thirdLayerWeights[8 * 4] = {0};
int8_t finalLayerWeights[8] = {0};

int16_t firstLayerBiases[8] = {0};
int16_t secondLayerBiases[8] = {0};
int16_t thirdLayerBiases[4] = {0};
int16_t finalLayerBias = 0;

int8_t *load_int8_1D_array(const std::string &file_path, size_t cols)
{
    int8_t *arr = new int8_t[cols](); // Zero-initialize the array
    std::vector<int8_t> values;
    std::ifstream file(file_path);
    std::string line;

    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return nullptr;
    }
    std::size_t index = 0;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ','))
        {
            arr[index++] = static_cast<int8_t>(std::stoi(item));
        }
    }
    return arr;
}

int16_t* load_int16_array(const std::string &file_path, size_t cols)
{
    int16_t *arr = new int16_t[cols](); // Zero-initialize the array
    std::ifstream file(file_path);
    std::string line;
    std::size_t index = 0;

    while (std::getline(file, line) && index < cols)
    {
        arr[index++] = static_cast<int16_t>(std::stoi(line));
    }
    return arr;
}

int16_t load_int16(const std::string &file_path)
{
    int16_t result;
    std::ifstream file(file_path);
    std::string line;
    std::size_t index = 0;

    while (std::getline(file, line) && index < 1)
    {
        result = static_cast<int16_t>(std::stoi(line));
    }
    return result;
}

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
            int newCol =  newPieceType * 64 + invertIndex(square);
            weights[newCol][row] = static_cast<int16_t>(std::stoi(value));
            col++;
        }
        row++;
    }
}

void initializeDoubleWeights()
{
    for (int i = 0; i < 640; i++)
    {
        for (int j = 0; j < 640; j++)
        {
            for (int k = 0; k < 8; k++)
            {
                // Sum with overflow handling for firstLayerWeights2Indices
                int32_t sum1 = (int32_t)firstLayerWeights[i][k] - (int32_t)firstLayerWeights[j][k];
                if (sum1 > INT16_MAX)
                {
                    firstLayerWeights2Indices[i][j][k] = INT16_MAX;
                }
                else if (sum1 < INT16_MIN)
                {
                    firstLayerWeights2Indices[i][j][k] = INT16_MIN;
                }
                else
                {
                    firstLayerWeights2Indices[i][j][k] = (int16_t)sum1;
                }

                // Sum with overflow handling for firstLayerInvertedWeights2Indices
                int32_t sum2 = (int32_t)firstLayerInvertedWeights[i][k] - (int32_t)firstLayerInvertedWeights[j][k];
                if (sum2 > INT16_MAX)
                {
                    firstLayerInvertedWeights2Indices[i][j][k] = INT16_MAX;
                }
                else if (sum2 < INT16_MIN)
                {
                    firstLayerInvertedWeights2Indices[i][j][k] = INT16_MIN;
                }
                else
                {
                    firstLayerInvertedWeights2Indices[i][j][k] = (int16_t)sum2;
                }
            }
        }
    }
}

namespace NNUEU
{
    AccumulatorStack globalAccumulatorStack;
    ///////////////////////////
    // NNUEU Evaluation
    ///////////////////////////

    void printArray(const char *name, const int16_t *array, size_t size)
    {
        std::cout << name << ": ";
        for (size_t i = 0; i < size; ++i)
        {
            std::cout << array[i] << " ";
        }
        std::cout << std::endl;
    }

    void print2DArray(const char *name, const int16_t array[][8], size_t rows)
    {
        std::cout << name << ":" << std::endl;
        for (size_t i = 0; i < rows; ++i)
        {
            for (size_t j = 0; j < 8; ++j)
            {
                std::cout << array[i][j] << " ";
            }
            std::cout << std::endl;
        }
    }

    void print2DArray(const char *name, const int8_t array[][8 * 4], size_t rows)
    {
        std::cout << name << ":" << std::endl;
        for (size_t i = 0; i < rows; ++i)
        {
            for (size_t j = 0; j < 8 * 4; ++j)
            {
                std::cout << static_cast<int>(array[i][j]) << " ";
            }
            std::cout << std::endl;
        }
    }

    void printArray(const char *name, const int8_t *array, size_t size)
    {
        std::cout << name << ": ";
        for (size_t i = 0; i < size; ++i)
        {
            std::cout << static_cast<int>(array[i]) << " ";
        }
        std::cout << std::endl;
    }
    void initNNUEParameters()
    {
        const std::string modelDir = "models/NNUEU_quantized_model_v4_param_350_epoch_10/";

        // Load weights into fixed-size arrays
        load_int16_2D_array1(modelDir + "first_linear_weights.csv", firstLayerWeights);
        load_inverted_int16_2D_array1(modelDir + "first_linear_weights.csv", firstLayerInvertedWeights);

        load_int8_2D_array1(modelDir + "second_layer_turn_weights.csv", secondLayer1Weights);
        load_int8_2D_array1(modelDir + "second_layer_not_turn_weights.csv", secondLayer2Weights);

        auto tempThirdLayerWeights = load_int8_1D_array(modelDir + "third_layer_weights.csv", 8 * 4);
        std::memcpy(thirdLayerWeights, tempThirdLayerWeights, sizeof(int8_t) * 8 * 4);
        delete[] tempThirdLayerWeights;

        auto tempFinalLayerWeights = load_int8_1D_array(modelDir + "final_layer_weights.csv", 4);
        std::memcpy(finalLayerWeights, tempFinalLayerWeights, sizeof(int8_t) * 4);
        // std::memset(finalLayerWeights + 4, 0, sizeof(int8_t) * 4);
        delete[] tempFinalLayerWeights;

        // Load biases
        auto tempFirstLayerBiases = load_int16_array(modelDir + "first_linear_biases.csv", 8);
        std::memcpy(firstLayerBiases, tempFirstLayerBiases, sizeof(int16_t) * 8);
        delete[] tempFirstLayerBiases;

        auto tempSecondLayer1Biases = load_int16_array(modelDir + "second_layer_turn_biases.csv", 4);
        auto tempSecondLayer2Biases = load_int16_array(modelDir + "second_layer_not_turn_biases.csv", 4);

        // Concatenate biases directly
        std::memcpy(secondLayerBiases, tempSecondLayer1Biases, sizeof(int16_t) * 4);
        std::memcpy(secondLayerBiases + 4, tempSecondLayer2Biases, sizeof(int16_t) * 4);

        delete[] tempSecondLayer1Biases;
        delete[] tempSecondLayer2Biases;

        auto tempThirdLayerBiases = load_int16_array(modelDir + "third_layer_biases.csv", 4);
        std::memcpy(thirdLayerBiases, tempThirdLayerBiases, sizeof(int16_t) * 4);
        delete[] tempThirdLayerBiases;

        finalLayerBias = load_int16(modelDir + "final_layer_biases.csv");

        initializeDoubleWeights();

        // Print arrays
        // print2DArray("First Layer Weights", firstLayerWeights, 640);
        // print2DArray("First Layer Inverted Weights", firstLayerInvertedWeights, 640);
        // print2DArray("Second Layer 1 Weights", secondLayer1Weights, 64);
        // print2DArray("Second Layer 2 Weights", secondLayer2Weights, 64);

        // printArray("Third Layer Weights", thirdLayerWeights, 8 * 4);
        // printArray("Final Layer Weights", finalLayerWeights, 4);

        // printArray("First Layer Biases", firstLayerBiases, 8);
        // printArray("Second Layer Biases", secondLayerBiases, 8);
        // printArray("Third Layer Biases", thirdLayerBiases, 4);
        // std::cout << "Final Layer Bias: " << finalLayerBias << std::endl;
    }

    void initializeNNUEInput(const BitPosition &position, AccumulatorState &accumulatorState)
    {
        // Start accumulators with the firstLayerBiases
        std::memcpy(accumulatorState.inputTurn[0], firstLayerBiases, sizeof(firstLayerBiases));
        std::memcpy(accumulatorState.inputTurn[1], firstLayerBiases, sizeof(firstLayerBiases));

        // White pawns
        for (unsigned short index : getBitIndices(position.getPieces(0, 0)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[index]);
        }
        // White knights
        for (unsigned short index : getBitIndices(position.getPieces(0, 1)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 + index]);
        }
        // White bishops
        for (unsigned short index : getBitIndices(position.getPieces(0, 2)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 * 2 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 * 2 + index]);
        }
        // White rooks
        for (unsigned short index : getBitIndices(position.getPieces(0, 3)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 * 3 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 * 3 + index]);
        }
        // White queens
        for (unsigned short index : getBitIndices(position.getPieces(0, 4)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 * 4 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 * 4 + index]);
        }

        // Black pawns
        for (unsigned short index : getBitIndices(position.getPieces(1, 0)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 * 5 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 * 5 + index]);
        }
        // Black knights
        for (unsigned short index : getBitIndices(position.getPieces(1, 1)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 * 6 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 * 6 + index]);
        }
        // Black bishops
        for (unsigned short index : getBitIndices(position.getPieces(1, 2)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 * 7 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 * 7 + index]);
        }
        // Black rooks
        for (unsigned short index : getBitIndices(position.getPieces(1, 3)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 * 8 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 * 8 + index]);
        }
        // Black queens
        for (unsigned short index : getBitIndices(position.getPieces(1, 4)))
        {
            add_8_int16(accumulatorState.inputTurn[0], firstLayerWeights[64 * 9 + index]);
            add_8_int16(accumulatorState.inputTurn[1], firstLayerInvertedWeights[64 * 9 + index]);
        }

        // Now handle the second layer for the kings
        const int whiteKing = position.getKingPosition(0);
        const int blackKing = position.getKingPosition(1);

        assert(whiteKing >= 0 && whiteKing < 64);
        assert(blackKing >= 0 && blackKing < 64);

        secondLayer1WeightsBlockWhiteTurn = secondLayer1Weights[whiteKing];
        secondLayer2WeightsBlockBlackTurn = secondLayer2Weights[invertIndex(whiteKing)];
        secondLayer2WeightsBlockWhiteTurn = secondLayer2Weights[blackKing];
        secondLayer1WeightsBlockBlackTurn = secondLayer1Weights[invertIndex(blackKing)];

        // std::memcpy(secondLayer1WeightsBlockWhiteTurn, secondLayer1Weights[whiteKing], sizeof(secondLayer1Weights[whiteKing]));
        // std::memcpy(secondLayer1WeightsBlockBlackTurn, secondLayer1Weights[invertIndex(blackKing)], sizeof(secondLayer1Weights[invertIndex(blackKing)]));
        // std::memcpy(secondLayer2WeightsBlockWhiteTurn, secondLayer2Weights[blackKing], sizeof(secondLayer2Weights[blackKing]));
        // std::memcpy(secondLayer2WeightsBlockBlackTurn, secondLayer2Weights[invertIndex(whiteKing)], sizeof(secondLayer2Weights[invertIndex(whiteKing)]));
    }

    // Functions to add/remove features from the accumulators
    void addAndRemoveOnInput(AccumulatorState &st, int subIndexAdd, int subIndexRemove, bool turn)
    {
        assert(subIndexAdd >= 0 && subIndexAdd < 640 && subIndexRemove >= 0 && subIndexRemove < 640);
        if (not turn)
            add_8_int16(st.inputTurn[0], firstLayerWeights2Indices[subIndexAdd][subIndexRemove]);
        else
            add_8_int16(st.inputTurn[1], firstLayerInvertedWeights2Indices[subIndexAdd][subIndexRemove]);
    }

    void addOnInput(AccumulatorState &st, int subIndex, bool turn)
    {
        assert(subIndex >= 0 && subIndex < 640);
        if (not turn)
            add_8_int16(st.inputTurn[0], firstLayerWeights[subIndex]);
        else
            add_8_int16(st.inputTurn[1], firstLayerInvertedWeights[subIndex]);
    }

    void removeOnInput(AccumulatorState &st, int subIndex, bool turn)
    {
        assert(subIndex >= 0 && subIndex < 640);
        if (not turn)
            substract_8_int16(st.inputTurn[0], firstLayerWeights[subIndex]);
        else
            substract_8_int16(st.inputTurn[1], firstLayerInvertedWeights[subIndex]);
    }
    void moveWhiteKingNNUEInput(int kingPos)
    {
        assert(kingPos >= 0 && kingPos < 64);
        secondLayer1WeightsBlockWhiteTurn = secondLayer1Weights[kingPos];
        secondLayer2WeightsBlockBlackTurn = secondLayer2Weights[invertIndex(kingPos)];
        globalAccumulatorStack.changeWhiteKingPosition(kingPos);
    }

    void moveBlackKingNNUEInput(int kingPos)
    {
        assert(kingPos >= 0 && kingPos < 64);

        secondLayer2WeightsBlockWhiteTurn = secondLayer2Weights[kingPos];
        secondLayer1WeightsBlockBlackTurn = secondLayer1Weights[invertIndex(kingPos)];
        globalAccumulatorStack.changeBlackKingPosition(kingPos);
    }

    // Overload for a normal (non-capture) move: two indices
    void NNUEUChange::add(int idx0, int idx1)
    {
        assert(idx0 >= 0 && idx0 < 640);
        assert(idx1 >= 0 && idx1 < 640);
        is_capture = false; // No capture
        indices[0] = idx0;
        indices[1] = idx1;
    }

    // Overload for a capture move: three indices
    void NNUEUChange::add(int idx0, int idx1, int idx2)
    {
        assert(idx0 >= 0 && idx0 < 640);
        assert(idx1 >= 0 && idx1 < 640);
        assert(idx2 >= 0 && idx2 < 640);
        is_capture = true; // It's a capture
        indices[0] = idx0;
        indices[1] = idx1;
        indices[2] = idx2;
    }

    void NNUEUChange::addlast(int idx2)
    {
        assert(idx2 >= 0 && idx2 < 640);
        is_capture = true; // It's a capture
        indices[2] = idx2;
    }
    inline bool NNUEUChange::isKingMove() const
    {
        return indices[0] == indices[1];
    }
    inline bool NNUEUChange::isCapture() const
    {
        // King captured something, but didn't affect NNUE input for the king
        return is_capture;
    }
#ifndef NDEBUG
    void AccumulatorStack::verifyTopAgainstFresh(const BitPosition &pos, bool turn)
    {
        // 1. Build a *fresh* accumulator for reference
        AccumulatorState fresh;
        initializeNNUEInput(pos, fresh);
    
        // 2. Compare with the incrementally-updated top of the stack
        const AccumulatorState &inc = top();

        for (int i = 0; i < 8; ++i)
            assert(fresh.inputTurn[turn][i] == inc.inputTurn[turn][i] &&
                    "NNUEU incremental accumulation mismatch");
    }
#endif // NDEBUG

    // Reset to a new root position
    void AccumulatorStack::reset(const BitPosition &rootPos)
    {
        m_current_idx = 1;
        AccumulatorState &rootState = stack[0];

        // Build a fresh accumulator for the root
        initializeNNUEInput(rootPos, rootState);
        // Set the king positions
        nnueu_king_positions[0] = rootPos.getKingPosition(0);
        nnueu_king_positions[1] = rootPos.getKingPosition(1);
        rootState.computed[0] = true;
        rootState.computed[1] = true;
    }
    void AccumulatorStack::changeWhiteKingPosition(int kingPos)
    {
        nnueu_king_positions[0] = kingPos;
    }
    void AccumulatorStack::changeBlackKingPosition(int kingPos)
    {
        nnueu_king_positions[1] = kingPos;
    }
    int AccumulatorStack::getNNUEUKingPosition(int color) const
    {
        return nnueu_king_positions[color];
    }

    void AccumulatorStack::push(const NNUEUChange &chngs)
    {
        assert(m_current_idx < stack.size()); // Ensure space exists
        stack[m_current_idx].newAcc(chngs);
        m_current_idx++;
    }

    // Pop the top state when unmaking a move
    void AccumulatorStack::pop()
    {
        assert(m_current_idx > 1); // Never pop below 1, since root accumulator should be computed
        m_current_idx--;
    }

    AccumulatorState &AccumulatorStack::top()
    {
        assert(m_current_idx - 1 < stack.size());
        assert(stack[m_current_idx - 1].computed[0] || stack[m_current_idx - 1].computed[1]);
        return stack[m_current_idx - 1];
    }
    // From top down to 0, find the first node that has both sides computed
    int AccumulatorStack::findLastComputedNode(bool turn) const
    {
        for (std::size_t curr_idx = m_current_idx - 2; curr_idx > 0; curr_idx--)
        {
            if (stack[curr_idx].computed[not turn])
                return curr_idx;
        }
        return 0;
    }
    void AccumulatorStack::forward_update_incremental(const int begin, bool turn)
    {
        for (int next = begin + 1; next < m_current_idx; next++)
            applyIncrementalChanges(stack[next], stack[next - 1], not turn);
    }

    // This applies the “NNUEUChange” to the current node
    void AccumulatorStack::applyIncrementalChanges(AccumulatorState &curr, const AccumulatorState &prev, bool turn)
    {
        assert(prev.computed[turn]);
        // Copy previous accumulators
        std::memcpy(curr.inputTurn[turn], prev.inputTurn[turn], 16);

        const NNUEUChange &c = curr.changes;

        if (c.isCapture())
            removeOnInput(curr, c.indices[2], turn);
        if (!c.isKingMove())
            addAndRemoveOnInput(curr, c.indices[0], c.indices[1], turn);
        curr.computed[turn] = true;
    }

    // The engine is built to get an evaluation of the position with high values being good for the engine.
    // The NNUE is built to give an evaluation of the position with high values being good for whose turn it is.
    // This function gives an evaluation with high values being good for engine.
    int16_t evaluationFunction(BitPosition &position, bool ourTurn)
    {
        // Update incrementally from the last computed node
        globalAccumulatorStack.forward_update_incremental(globalAccumulatorStack.findLastComputedNode(position.getTurn()), position.getTurn());

#ifndef NDEBUG
        globalAccumulatorStack.verifyTopAgainstFresh(position, not position.getTurn());
#endif

        // Change the NNUEU king positions if needed
        if (globalAccumulatorStack.getNNUEUKingPosition(0) != position.getKingPosition(0))
            moveWhiteKingNNUEInput(position.getKingPosition(0));

        if (globalAccumulatorStack.getNNUEUKingPosition(1) != position.getKingPosition(1))
            moveBlackKingNNUEInput(position.getKingPosition(1));

        assert(position.getKingPosition(0) == globalAccumulatorStack.getNNUEUKingPosition(0));
        assert(position.getKingPosition(1) == globalAccumulatorStack.getNNUEUKingPosition(1));

        int16_t out;
        AccumulatorState &updatedAcc = globalAccumulatorStack.top();

        if (position.getTurn())
        {
            #ifndef NDEBUG
            out = fullNnueuPassDebug(updatedAcc.inputTurn[0], secondLayer1WeightsBlockWhiteTurn, secondLayer2WeightsBlockWhiteTurn);
            #else
            out = fullNnueuPass(updatedAcc.inputTurn[0], secondLayer1WeightsBlockWhiteTurn, secondLayer2WeightsBlockWhiteTurn);
            #endif
        }

        else
        {
            #ifndef NDEBUG
            out = fullNnueuPassDebug(updatedAcc.inputTurn[1], secondLayer1WeightsBlockBlackTurn, secondLayer2WeightsBlockBlackTurn);
            #else
            out = fullNnueuPass(updatedAcc.inputTurn[1], secondLayer1WeightsBlockBlackTurn, secondLayer2WeightsBlockBlackTurn);
            #endif
        }

        // Change evaluation from player to move perspective to our perspective
        if (ourTurn)
            return out;

        return 4096 - out;
    }

    } // namespace NNUEU
