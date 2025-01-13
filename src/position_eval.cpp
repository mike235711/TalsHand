#include "position_eval.h"
#include <fstream>
#include <sstream>
#include "bitposition.h"
#include "bit_utils.h" // Bit utility functions
#include "precomputed_moves.h"
#include "simd.h"

// NNUEU parameter loading

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
} // namespace NNUEU
