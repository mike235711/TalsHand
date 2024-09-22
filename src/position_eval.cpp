#include "position_eval.h"
#include <fstream>
#include <sstream>
#include "bitposition.h"
#include "bit_utils.h" // Bit utility functions
#include "precomputed_moves.h"
#include "simd.h"

// Function to load a 2D int8_t array from a file
void load_int16_2D_array(const std::string &file_path, int16_t weights[40960][8])
{
    std::ifstream file(file_path);
    std::string line;
    size_t row = 0;

    while (std::getline(file, line) && row < 8)
    {
        std::stringstream ss(line);
        std::string value;
        size_t col = 0;

        while (std::getline(ss, value, ',') && col < 40960)
        {
            weights[col][row] = static_cast<int16_t>(std::stoi(value));
            col++;
        }
        row++;
    }
}

void load_inverted_int16_2D_array(const std::string &file_path, int16_t weights[40960][8])
{
    std::ifstream file(file_path);
    std::string line;
    size_t row = 0;

    while (std::getline(file, line) && row < 8)
    {
        std::stringstream ss(line);
        std::string value;
        size_t col = 0;

        while (std::getline(ss, value, ',') && col < 40960)
        {
            int kingPos = col / 640;
            int pieceType = (col % 640) / 64;
            int square = col % 64;

            // Compute the new index after inverting
            int newPieceType = (pieceType + 5) % 10;
            int newCol = invertIndex(kingPos) * 640 + newPieceType * 64 + invertIndex(square);
            weights[newCol][row] = static_cast<int16_t>(std::stoi(value));
            col++;
        }
        row++;
    }
}

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
template <size_t N>
void print_array(const int8_t (&arr)[N])
{
    for (size_t i = 0; i < N; ++i)
    {
        std::cout << static_cast<int>(arr[i]) << " ";
    }
    std::cout << std::endl;
}

template <size_t N>
void print_array(const int16_t (&arr)[N])
{
    for (size_t i = 0; i < N; ++i)
    {
        std::cout << arr[i] << " ";
    }
    std::cout << std::endl;
}
void print_2D_array(const int8_t arr[40960][8], size_t rows, size_t cols)
{
    for (size_t i = 0; i < rows; ++i)
    {
        for (size_t j = 0; j < cols; ++j)
        {
            std::cout << static_cast<int>(arr[i][j]) << " ";
        }
        std::cout << std::endl;
    }
}
extern bool ENGINEISWHITE;

namespace NNUE
{
    ///////////////////////////
    // NNUE Evaluation
    ///////////////////////////

    int16_t whiteInputTurn[8] = {0};
    int16_t blackInputTurn[8] = {0};
    int16_t whiteInputNotTurn[8] = {0};
    int16_t blackInputNotTurn[8] = {0};

    int16_t firstLayer1Weights[40960][8] = {0};
    int16_t firstLayer2Weights[40960][8] = {0};
    int16_t firstLayer1InvertedWeights[40960][8] = {0};
    int16_t firstLayer2InvertedWeights[40960][8] = {0};

    int8_t secondLayerWeights[16 * 8] = {0};
    int8_t thirdLayerWeights[8 * 4] = {0};
    int8_t finalLayerWeights[4] = {0};

    int16_t firstLayer1Biases[8] = {0};
    int16_t firstLayer2Biases[8] = {0};
    int16_t secondLayerBiases[8] = {0};
    int16_t thirdLayerBiases[4] = {0};
    int16_t finalLayerBias = 0;

    void initNNUEParameters()
    {
        const std::string modelDir = "models/small_quantized_model_v1_param_350_epoch_8/";

        // Load weights into fixed-size arrays
        load_int16_2D_array(modelDir + "first_linear1_weights.csv", firstLayer1Weights);
        load_int16_2D_array(modelDir + "first_linear2_weights.csv", firstLayer2Weights);

        // Load inverted weights into fixed-size arrays
        load_inverted_int16_2D_array(modelDir + "first_linear1_weights.csv", firstLayer1InvertedWeights);
        load_inverted_int16_2D_array(modelDir + "first_linear2_weights.csv", firstLayer2InvertedWeights);

        auto tempSecondLayerWeights = load_int8_1D_array(modelDir + "second_layer_weights.csv", 16 * 8);
        std::memcpy(secondLayerWeights, tempSecondLayerWeights, sizeof(int8_t) * 16 * 8);
        delete[] tempSecondLayerWeights;

        auto tempThirdLayerWeights = load_int8_1D_array(modelDir + "third_layer_weights.csv", 8 * 4);
        std::memcpy(thirdLayerWeights, tempThirdLayerWeights, sizeof(int8_t) * 8 * 4);
        delete[] tempThirdLayerWeights;

        auto tempFinalLayerWeights = load_int8_1D_array(modelDir + "final_layer_weights.csv", 4);
        std::memcpy(finalLayerWeights, tempFinalLayerWeights, sizeof(int8_t) * 4);
        delete[] tempFinalLayerWeights;

        // Load biases
        auto tempFirstLayer1Biases = load_int16_array(modelDir + "first_linear1_biases.csv", 8);
        std::memcpy(firstLayer1Biases, tempFirstLayer1Biases, sizeof(int16_t) * 8);
        delete[] tempFirstLayer1Biases;

        auto tempFirstLayer2Biases = load_int16_array(modelDir + "first_linear2_biases.csv", 8);
        std::memcpy(firstLayer2Biases, tempFirstLayer2Biases, sizeof(int16_t) * 8);
        delete[] tempFirstLayer2Biases;

        auto tempSecondLayerBiases = load_int16_array(modelDir + "second_layer_biases.csv", 8);
        std::memcpy(secondLayerBiases, tempSecondLayerBiases, sizeof(int16_t) * 8);
        delete[] tempSecondLayerBiases;

        auto tempThirdLayerBiases = load_int16_array(modelDir + "third_layer_biases.csv", 4);
        std::memcpy(thirdLayerBiases, tempThirdLayerBiases, sizeof(int16_t) * 4);
        delete[] tempThirdLayerBiases;

        finalLayerBias = load_int16(modelDir + "final_layer_biases.csv");

        // Print loaded weights and biases
        // std::cout << "First Layer 1 Weights:" << std::endl;
        // print_2D_array(firstLayer1Weights, 40960, 8);

        // std::cout << "First Layer 2 Weights:" << std::endl;
        // print_2D_array(firstLayer2Weights, 40960, 8);

        // std::cout << "First Layer 1 Inverted Weights:" << std::endl;
        // print_2D_array(firstLayer1InvertedWeights, 40960, 8);

        // std::cout << "First Layer 2 Inverted Weights:" << std::endl;
        // print_2D_array(firstLayer2InvertedWeights, 40960, 8);

        // std::cout << "Second Layer Weights:" << std::endl;
        // print_array(secondLayerWeights);

        // std::cout << "Third Layer Weights:" << std::endl;
        // print_array(thirdLayerWeights);

        // std::cout << "Final Layer Weights:" << std::endl;
        // print_array(finalLayerWeights);

        // std::cout << "First Layer 1 Biases:" << std::endl;
        // print_array(firstLayer1Biases);

        // std::cout << "First Layer 2 Biases:" << std::endl;
        // print_array(firstLayer2Biases);

        // std::cout << "Second Layer Biases:" << std::endl;
        // print_array(secondLayerBiases);

        // std::cout << "Third Layer Biases:" << std::endl;
        // print_array(thirdLayerBiases);

        // std::cout << "Final Layer Bias:" << std::endl;
        // std::cout << finalLayerBias << std::endl;
    }

    int16_t evaluationFunction(bool ourTurn)
    // The engine is built to get an evaluation of the position with high values being good for the engine.
    // The NNUE is built to give an evaluation of the position with high values being good for whose turn it is.
    // This function gives an evaluation with high values being good for engine.
    {
        int16_t out;

        // If its our turn in white, or if its not our turn in black then it is white's turn
        // So the white vector goes first.
        if (ourTurn == ENGINEISWHITE)
        {
            out = fullNnuePass(whiteInputTurn, blackInputNotTurn, secondLayerWeights, secondLayerBiases, 
                            thirdLayerWeights, thirdLayerBiases, finalLayerWeights, &finalLayerBias);
        }
        else
        {
            out = fullNnuePass(blackInputTurn, whiteInputNotTurn, secondLayerWeights, secondLayerBiases,
                                 thirdLayerWeights, thirdLayerBiases, finalLayerWeights, &finalLayerBias);
        }
        // Change evaluation from player to move perspective to white perspective
        if (ourTurn)
            return out;

        return 64 * 64 - out;
    }

    void initializeNNUEInput(const BitPosition position)
    // Initialize the NNUE accumulators.
    {
        int whiteOffset = position.getWhiteKingPosition() * 64 * 10;
        int blackOffset = position.getBlackKingPosition() * 64 * 10;

        std::memcpy(whiteInputTurn, firstLayer1Biases, sizeof(firstLayer1Biases));
        std::memcpy(whiteInputNotTurn, firstLayer2Biases, sizeof(firstLayer2Biases));
        std::memcpy(blackInputTurn, firstLayer1Biases, sizeof(firstLayer1Biases));
        std::memcpy(blackInputNotTurn, firstLayer2Biases, sizeof(firstLayer2Biases));

        for (unsigned short index : getBitIndices(position.getWhitePawnsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + index]);
        }

        for (unsigned short index : getBitIndices(position.getWhiteKnightsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 + index]);
        }

        for (unsigned short index : getBitIndices(position.getWhiteBishopsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 2 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 2 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 2 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 2 + index]);
        }

        for (unsigned short index : getBitIndices(position.getWhiteRooksBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 3 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 3 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 3 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 3 + index]);
        }

        for (unsigned short index : getBitIndices(position.getWhiteQueensBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 4 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 4 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 4 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 4 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackPawnsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 5 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 5 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 5 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 5 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackKnightsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 6 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 6 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 6 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 6 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackBishopsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 7 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 7 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 7 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 7 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackRooksBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 8 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 8 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 8 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 8 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackQueensBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 9 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 9 + index]);
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 9 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 9 + index]);
        }
    }

    // Helper functions to update the input vector
    // They are used in bitposition.cpp inside makeNormalMove, makeCapture, setPiece and removePiece.

    void addOnInput(int whiteKingPosition, int blackKingPosition, int subIndex)
    {
        // White turn (use normal NNUE)
        add_8_int16(whiteInputTurn, firstLayer1Weights[whiteKingPosition * 640 + subIndex]);
        add_8_int16(blackInputNotTurn, firstLayer2Weights[blackKingPosition * 640 + subIndex]);
        // Black turn (use inverted NNUE)
        add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteKingPosition * 640 + subIndex]);
        add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackKingPosition * 640 + subIndex]);
    }

    void removeOnInput(int whiteKingPosition, int blackKingPosition, int subIndex)
    {
        // White turn (use normal NNUE)
        substract_8_int16(whiteInputTurn, firstLayer1Weights[whiteKingPosition * 640 + subIndex]);
        substract_8_int16(blackInputNotTurn, firstLayer2Weights[blackKingPosition * 640 + subIndex]);
        // Black turn (use inverted NNUE)
        substract_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteKingPosition * 640 + subIndex]);
        substract_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackKingPosition * 640 + subIndex]);
    }
    // Move King functions to update nnueInput vector
    void moveWhiteKingNNUEInput(BitPosition &position)
    {
        int whiteOffset = position.getWhiteKingPosition() * 640;

        std::memcpy(whiteInputTurn, firstLayer1Biases, 16);
        std::memcpy(whiteInputNotTurn, firstLayer2Biases, 16);

        for (unsigned short index : getBitIndices(position.getWhitePawnsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + index]);
        }
        for (unsigned short index : getBitIndices(position.getWhiteKnightsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 + index]);
        }
        for (unsigned short index : getBitIndices(position.getWhiteBishopsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 2 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 2 + index]);
        }
        for (unsigned short index : getBitIndices(position.getWhiteRooksBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 3 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 3 + index]);
        }
        for (unsigned short index : getBitIndices(position.getWhiteQueensBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 4 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 4 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackPawnsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 5 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 5 + index]);
        }
        for (unsigned short index : getBitIndices(position.getBlackKnightsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 6 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 6 + index]);
        }
        for (unsigned short index : getBitIndices(position.getBlackBishopsBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 7 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 7 + index]);
        }
        for (unsigned short index : getBitIndices(position.getBlackRooksBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 8 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 8 + index]);
        }
        for (unsigned short index : getBitIndices(position.getBlackQueensBits()))
        {
            add_8_int16(whiteInputTurn, firstLayer1Weights[whiteOffset + 64 * 9 + index]);
            add_8_int16(whiteInputNotTurn, firstLayer2InvertedWeights[whiteOffset + 64 * 9 + index]);
        }
    }
    void moveBlackKingNNUEInput(BitPosition &position)
    {
        int blackOffset = position.getBlackKingPosition() * 640;

        std::memcpy(blackInputTurn, firstLayer1Biases, 16);
        std::memcpy(blackInputNotTurn, firstLayer2Biases, 16);

        for (unsigned short index : getBitIndices(position.getWhitePawnsBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + index]);
        }
        for (unsigned short index : getBitIndices(position.getWhiteKnightsBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 + index]);
        }
        for (unsigned short index : getBitIndices(position.getWhiteBishopsBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 2 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 2 + index]);
        }
        for (unsigned short index : getBitIndices(position.getWhiteRooksBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 3 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 3 + index]);
        }
        for (unsigned short index : getBitIndices(position.getWhiteQueensBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 4 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 4 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackPawnsBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 5 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 5 + index]);
        }
        for (unsigned short index : getBitIndices(position.getBlackKnightsBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 6 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 6 + index]);
        }
        for (unsigned short index : getBitIndices(position.getBlackBishopsBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 7 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 7 + index]);
        }
        for (unsigned short index : getBitIndices(position.getBlackRooksBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 8 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 8 + index]);
        }
        for (unsigned short index : getBitIndices(position.getBlackQueensBits()))
        {
            add_8_int16(blackInputTurn, firstLayer1InvertedWeights[blackOffset + 64 * 9 + index]);
            add_8_int16(blackInputNotTurn, firstLayer2Weights[blackOffset + 64 * 9 + index]);
        }
    }
} // namespace NNUE

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

namespace NNUEU
{
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

    int16_t inputWhiteTurn[8] = {0};
    int16_t inputBlackTurn[8] = {0};

    int16_t firstLayerWeights[640][8] = {0};
    int16_t firstLayerInvertedWeights[640][8] = {0};

    int8_t secondLayer1Weights[64][8 * 4] = {0};
    int8_t secondLayer2Weights[64][8 * 4] = {0};

    int8_t secondLayer1WeightsBlockWhiteTurn[8 * 4] = {0};
    int8_t secondLayer2WeightsBlockWhiteTurn[8 * 4] = {0};
    int8_t secondLayer1WeightsBlockBlackTurn[8 * 4] = {0};
    int8_t secondLayer2WeightsBlockBlackTurn[8 * 4] = {0};

    int8_t thirdLayerWeights[8 * 4] = {0};
    int8_t finalLayerWeights[4] = {0};

    int16_t firstLayerBiases[8] = {0};
    int16_t secondLayerBiases[8] = {0};
    int16_t thirdLayerBiases[4] = {0};
    int16_t finalLayerBias = 0;

    void initNNUEParameters()
    {
        const std::string modelDir = "models/NNUEU_quantized_model_v1_param_350_epoch_5/";

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

    // The engine is built to get an evaluation of the position with high values being good for the engine.
    // The NNUE is built to give an evaluation of the position with high values being good for whose turn it is.
    // This function gives an evaluation with high values being good for engine.

    int16_t evaluationFunction(bool ourTurn)
    {
        int16_t out;

        // If its our turn in white, or if its not our turn in black then it is white's turn
        if (ourTurn == ENGINEISWHITE)
        {
            out = fullNnueuPass(inputWhiteTurn, secondLayer1WeightsBlockWhiteTurn, secondLayer2WeightsBlockWhiteTurn, secondLayerBiases,
                               thirdLayerWeights, thirdLayerBiases, finalLayerWeights, &finalLayerBias);
        }
        else
        {
            out = fullNnueuPass(inputBlackTurn, secondLayer1WeightsBlockBlackTurn, secondLayer2WeightsBlockBlackTurn, secondLayerBiases, 
                                thirdLayerWeights, thirdLayerBiases, finalLayerWeights, &finalLayerBias);
        }
        // Change evaluation from player to move perspective to white perspective
        if (ourTurn)
            return out;

        return 64 * 64 - out;
    }

    void initializeNNUEInput(const BitPosition position)
    // Initialize the NNUE accumulators.
    {
        std::memcpy(inputWhiteTurn, firstLayerBiases, sizeof(firstLayerBiases));
        std::memcpy(inputBlackTurn, firstLayerBiases, sizeof(firstLayerBiases));

        for (unsigned short index : getBitIndices(position.getWhitePawnsBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[index]);
        }

        for (unsigned short index : getBitIndices(position.getWhiteKnightsBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 + index]);
        }

        for (unsigned short index : getBitIndices(position.getWhiteBishopsBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 * 2 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 * 2 + index]);
        }

        for (unsigned short index : getBitIndices(position.getWhiteRooksBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 * 3 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 * 3 + index]);
        }

        for (unsigned short index : getBitIndices(position.getWhiteQueensBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 * 4 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 * 4 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackPawnsBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 * 5 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 * 5 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackKnightsBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 * 6 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 * 6 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackBishopsBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 * 7 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 * 7 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackRooksBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 * 8 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 * 8 + index]);
        }

        for (unsigned short index : getBitIndices(position.getBlackQueensBits()))
        {
            add_8_int16(inputWhiteTurn, firstLayerWeights[64 * 9 + index]);
            add_8_int16(inputBlackTurn, firstLayerInvertedWeights[64 * 9 + index]);
        }
        int whiteKingPos{position.getWhiteKingPosition()};
        int blackKingPos{position.getBlackKingPosition()};

        std::memcpy(secondLayer1WeightsBlockWhiteTurn, secondLayer1Weights[whiteKingPos], sizeof(secondLayer1Weights[whiteKingPos]));
        std::memcpy(secondLayer1WeightsBlockBlackTurn, secondLayer1Weights[invertIndex(blackKingPos)], sizeof(secondLayer1Weights[invertIndex(blackKingPos)]));
        std::memcpy(secondLayer2WeightsBlockWhiteTurn, secondLayer2Weights[blackKingPos], sizeof(secondLayer2Weights[blackKingPos]));
        std::memcpy(secondLayer2WeightsBlockBlackTurn, secondLayer2Weights[invertIndex(whiteKingPos)], sizeof(secondLayer2Weights[invertIndex(whiteKingPos)]));
    }

    // Helper functions to update the input vector
    // They are used in bitposition.cpp inside makeNormalMove, makeCapture, setPiece and removePiece.
    void addOnInput(int whiteKingPosition, int blackKingPosition, int subIndex)
    {
        // White turn (use normal NNUE)
        add_8_int16(inputWhiteTurn, firstLayerWeights[subIndex]);
        // Black turn (use inverted NNUE)
        add_8_int16(inputBlackTurn, firstLayerInvertedWeights[subIndex]);
    }
    void removeOnInput(int whiteKingPosition, int blackKingPosition, int subIndex)
    {
        // White turn (use normal NNUE)
        substract_8_int16(inputWhiteTurn, firstLayerWeights[subIndex]);
        // Black turn (use inverted NNUE)
        substract_8_int16(inputBlackTurn, firstLayerInvertedWeights[subIndex]);
    }
    // Move King functions to update nnueInput vector
    void moveWhiteKingNNUEInput(BitPosition &position)
    {
        int whiteKingPos{position.getWhiteKingPosition()};
        std::memcpy(secondLayer1WeightsBlockWhiteTurn, secondLayer1Weights[whiteKingPos], 32);
        std::memcpy(secondLayer2WeightsBlockBlackTurn, secondLayer2Weights[invertIndex(whiteKingPos)], 32);
    }
    void moveBlackKingNNUEInput(BitPosition &position)
    {
        int blackKingPos{position.getBlackKingPosition()};
        std::memcpy(secondLayer2WeightsBlockWhiteTurn, secondLayer2Weights[blackKingPos], 32);
        std::memcpy(secondLayer1WeightsBlockBlackTurn, secondLayer1Weights[invertIndex(blackKingPos)], 32);
    }
} // namespace NNUEU
