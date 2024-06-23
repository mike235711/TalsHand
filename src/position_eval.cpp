#include "position_eval.h"
#include <fstream>
#include <sstream>
#include <armadillo>
#include "bitposition.h"
#include "bit_utils.h" // Bit utility functions

///////////////////////////
// NNUE Evaluation
///////////////////////////

std::vector<std::vector<int8_t>> load_weights(const std::string &file_path)
{
    std::vector<std::vector<int8_t>> weights;
    std::ifstream file(file_path);
    std::string line;

    while (std::getline(file, line))
    {
        std::vector<int8_t> row;
        std::stringstream ss(line);
        std::string value;

        while (std::getline(ss, value, ','))
        {
            row.push_back(std::stof(value));
        }

        weights.push_back(row);
    }

    return weights;
}

std::vector<float> load_biases(const std::string &file_path)
{
    std::vector<float> biases;
    std::ifstream file(file_path);
    std::string line;

    while (std::getline(file, line))
    {
        biases.push_back(std::stof(line));
    }

    return biases;
}

float load_scale(const std::string &file_path)
{
    std::ifstream file(file_path);
    std::string line;

    if (std::getline(file, line))
    {
        return std::stof(line);
    }

    throw std::runtime_error("Failed to read scale from file: " + file_path);
}

arma::vec convertToArmaVec(const std::vector<float> &vec)
{
    arma::vec result(vec.size());
    for (size_t i = 0; i < vec.size(); ++i)
    {
        result(i) = vec[i];
    }
    return result;
}

arma::vec convertToArmaVec(const std::vector<int8_t> &vec)
{
    arma::vec result(vec.size());
    for (size_t i = 0; i < vec.size(); ++i)
    {
        result(i) = vec[i];
    }
    return result;
}

arma::mat convertToArmaMatrix(const std::vector<std::vector<int8_t>> &vec)
{
    arma::mat result(vec.size(), vec[0].size());
    for (size_t i = 0; i < vec.size(); ++i)
    {
        for (size_t j = 0; j < vec[i].size(); ++j)
        {
            result(i, j) = vec[i][j];
        }
    }
    return result;
}

arma::vec whiteInput;
arma::vec blackInput;
arma::mat firstLayerWeights;
arma::mat secondLayerWeights;
arma::mat thirdLayerWeights;
arma::mat finalLayerWeights;

arma::vec firstLayerBiases;
arma::vec secondLayerBiases;
arma::vec thirdLayerBiases;
arma::vec finalLayerBiases;

float firstLayerScale;
float secondLayerScale;
float thirdLayerScale;
float finalLayerScale;

extern bool ENGINEISWHITE;

void initNNUEParameters()
{
    const std::string modelDir = "models/quantized_model_0plus_v1_param_50_same_first/";
    // Load weights
    std::vector<std::vector<int8_t>> firstLayerWeightsData = load_weights(modelDir + "white_linear_weights.csv");
    std::vector<std::vector<int8_t>> secondLayerWeightsData = load_weights(modelDir + "second_layer_weights.csv");
    std::vector<std::vector<int8_t>> thirdLayerWeightsData = load_weights(modelDir + "third_layer_weights.csv");
    std::vector<std::vector<int8_t>> finalLayerWeightsData = load_weights(modelDir + "final_layer_weights.csv");

    // Convert weights to Armadillo matrices
    firstLayerWeights = convertToArmaMatrix(firstLayerWeightsData);
    secondLayerWeights = convertToArmaMatrix(secondLayerWeightsData);
    thirdLayerWeights = convertToArmaMatrix(thirdLayerWeightsData);
    finalLayerWeights = convertToArmaMatrix(finalLayerWeightsData);

    // Print dimensions to verify
    std::cout << "Dimensions of firstLayerWeights: " << firstLayerWeights.n_rows << " x " << firstLayerWeights.n_cols << std::endl;
    std::cout << "Dimensions of secondLayerWeights: " << secondLayerWeights.n_rows << " x " << secondLayerWeights.n_cols << std::endl;
    std::cout << "Dimensions of thirdLayerWeights: " << thirdLayerWeights.n_rows << " x " << thirdLayerWeights.n_cols << std::endl;
    std::cout << "Dimensions of finalLayerWeights: " << finalLayerWeights.n_rows << " x " << finalLayerWeights.n_cols << std::endl;

    // Load biases
    std::vector<float> firstLayerBiasesData = load_biases(modelDir + "white_linear_biases.csv");
    std::vector<float> secondLayerBiasesData = load_biases(modelDir + "second_layer_biases.csv");
    std::vector<float> thirdLayerBiasesData = load_biases(modelDir + "third_layer_biases.csv");
    std::vector<float> finalLayerBiasesData = load_biases(modelDir + "final_layer_biases.csv");

    // Convert biases to Armadillo vectors
    firstLayerBiases = convertToArmaVec(firstLayerBiasesData);
    secondLayerBiases = convertToArmaVec(secondLayerBiasesData);
    thirdLayerBiases = convertToArmaVec(thirdLayerBiasesData);
    finalLayerBiases = convertToArmaVec(finalLayerBiasesData);

    // Print dimensions to verify
    std::cout << "Dimensions of firstLayerBiases: " << firstLayerBiases.n_elem << std::endl;
    std::cout << "Dimensions of secondLayerBiases: " << secondLayerBiases.n_elem << std::endl;
    std::cout << "Dimensions of thirdLayerBiases: " << thirdLayerBiases.n_elem << std::endl;
    std::cout << "Dimensions of finalLayerBiases: " << finalLayerBiases.n_elem << std::endl;

    // Load scales
    firstLayerScale = load_scale(modelDir + "white_linear_scales.csv");
    secondLayerScale = load_scale(modelDir + "second_layer_scales.csv");
    thirdLayerScale = load_scale(modelDir + "third_layer_scales.csv");
    finalLayerScale = load_scale(modelDir + "final_layer_scales.csv");
}

// The engine is built to get an evaluation of the position with high values being good for the engine. 
// The NNUE is built to give an evaluation of the position with high values being good for whose turn it is.
// This function gives an evaluation with high values being good for engine.
float evaluationFunction(const BitPosition &position, bool ourTurn)
{
    arma::vec secondLayerOutput;

    // If its our turn in white, or if its not our turn in black then it is white's turn
    // So the white vector goes first.
    if (ourTurn == ENGINEISWHITE)
        secondLayerOutput = (secondLayerWeights * arma::join_cols(whiteInput, blackInput)) * secondLayerScale + secondLayerBiases;
    else
        secondLayerOutput = (secondLayerWeights * arma::join_cols(blackInput, whiteInput)) * secondLayerScale + secondLayerBiases;
    
    secondLayerOutput = arma::clamp(secondLayerOutput, 0.0, 1.0);

    arma::vec thirdLayerOutput = (thirdLayerWeights * secondLayerOutput) * thirdLayerScale + thirdLayerBiases;
    thirdLayerOutput = arma::clamp(thirdLayerOutput, 0.0, 1.0);

    arma::vec finalOutput = (finalLayerWeights * thirdLayerOutput) * finalLayerScale + finalLayerBiases;
    if (ourTurn)
        return static_cast<float>(finalOutput[0]);
    else
        return 1-static_cast<float>(finalOutput[0]);
}

void initializeNNUEInput(const BitPosition position)
{
    std::vector<int8_t> whiteInputLong(64 * 64 * 5, 0);
    std::vector<int8_t> blackInputLong(64 * 64 * 5, 0);

    uint64_t whitePawnsBit = position.getWhitePawnsBits();
    uint64_t whiteKnightsBit = position.getWhiteKnightsBits();
    uint64_t whiteBishopsBit = position.getWhiteBishopsBits();
    uint64_t whiteRooksBit = position.getWhiteRooksBits();
    uint64_t whiteQueensBit = position.getWhiteQueensBits();
    int whiteKingPosition = getLeastSignificantBitIndex(position.getWhiteKingBits());

    uint64_t blackPawnsBit = position.getBlackPawnsBits();
    uint64_t blackKnightsBit = position.getBlackKnightsBits();
    uint64_t blackBishopsBit = position.getBlackBishopsBits();
    uint64_t blackRooksBit = position.getBlackRooksBits();
    uint64_t blackQueensBit = position.getBlackQueensBits();
    int blackKingPosition = getLeastSignificantBitIndex(position.getBlackKingBits());

    int whiteOffset = whiteKingPosition * 64 * 5;
    int blackOffset = blackKingPosition * 64 * 5;

    for (unsigned short index : getBitIndices(whitePawnsBit))
        whiteInputLong[whiteOffset + index] = 1;
    for (unsigned short index : getBitIndices(whiteKnightsBit))
        whiteInputLong[whiteOffset + 64 + index] = 1;
    for (unsigned short index : getBitIndices(whiteBishopsBit))
        whiteInputLong[whiteOffset + 128 + index] = 1;
    for (unsigned short index : getBitIndices(whiteRooksBit))
        whiteInputLong[whiteOffset + 192 + index] = 1;
    for (unsigned short index : getBitIndices(whiteQueensBit))
        whiteInputLong[whiteOffset + 256 + index] = 1;

    for (unsigned short index : getBitIndices(blackPawnsBit))
        blackInputLong[blackOffset + index] = 1;
    for (unsigned short index : getBitIndices(blackKnightsBit))
        blackInputLong[blackOffset + 64 + index] = 1;
    for (unsigned short index : getBitIndices(blackBishopsBit))
        blackInputLong[blackOffset + 128 + index] = 1;
    for (unsigned short index : getBitIndices(blackRooksBit))
        blackInputLong[blackOffset + 192 + index] = 1;
    for (unsigned short index : getBitIndices(blackQueensBit))
        blackInputLong[blackOffset + 256 + index] = 1;

    whiteInput = (firstLayerWeights * convertToArmaVec(whiteInputLong)) * firstLayerScale + firstLayerBiases;
    blackInput = (firstLayerWeights * convertToArmaVec(blackInputLong)) * firstLayerScale + firstLayerBiases;
}

// Helper functions to update the input vector
// They are used in bitposition.cpp inside makeNormalMove, makeCapture, setPiece and removePiece.

void addOnWhiteInput(int index)
{
    // Add vector (firstLayerWeights[j, index] for j in range(firstLayerWeights.n_columns)) to whiteInput
    whiteInput += firstLayerWeights.col(index) * firstLayerScale;
}

void removeOnWhiteInput(int index)
{
    // Remove vector (firstLayerWeights[j, index] for j in range(firstLayerWeights.n_columns)) to whiteInput
    whiteInput -= firstLayerWeights.col(index) * firstLayerScale;
}

void addOnBlackInput(int index)
{
    // Add vector (firstLayerWeights[j, index] for j in range(firstLayerWeights.n_columns)) to blackInput
    blackInput += firstLayerWeights.col(index) * firstLayerScale;
}

void removeOnBlackInput(int index)
{
    // Remove vector (firstLayerWeights[j, index] for j in range(firstLayerWeights.n_columns)) to blackInput
    blackInput -= firstLayerWeights.col(index) * firstLayerScale;
}

// Move King functions to update nnueInput vector

void moveWhiteKingNNUEInput(const BitPosition &position)
{
    arma::vec longWhiteInput(64 * 5);

    int whiteOffset = position.getWhiteKingPosition() * 64 * 5;

    // Update whiteInput for all pieces relative to the new white king's position
    for (unsigned short index : getBitIndices(position.getWhitePawnsBits()))
        longWhiteInput(index) = 1;
    for (unsigned short index : getBitIndices(position.getWhiteKnightsBits()))
        longWhiteInput(64 + index) = 1;
    for (unsigned short index : getBitIndices(position.getWhiteBishopsBits()))
        longWhiteInput(128 + index) = 1;
    for (unsigned short index : getBitIndices(position.getWhiteRooksBits()))
        longWhiteInput(192 + index) = 1;
    for (unsigned short index : getBitIndices(position.getWhiteQueensBits()))
        longWhiteInput(256 + index) = 1;

    whiteInput = (firstLayerWeights.cols(whiteOffset, whiteOffset + 64 * 5 -1) * longWhiteInput) * firstLayerScale + firstLayerBiases;
}

void moveBlackKingNNUEInput(const BitPosition &position)
{
    arma::vec longBlackInput(64 * 5);

    int blackOffset = position.getBlackKingPosition() * 64 * 5;

    // Update blackInput for all pieces relative to the new black king's position
    for (unsigned short index : getBitIndices(position.getBlackKingBits()))
        longBlackInput(index) = 1;
    for (unsigned short index : getBitIndices(position.getBlackKingBits()))
        longBlackInput(64 + index) = 1;
    for (unsigned short index : getBitIndices(position.getBlackBishopsBits()))
        longBlackInput(128 + index) = 1;
    for (unsigned short index : getBitIndices(position.getBlackRooksBits()))
        longBlackInput(192 + index) = 1;
    for (unsigned short index : getBitIndices(position.getBlackQueensBits()))
        longBlackInput(256 + index) = 1;

    blackInput = (firstLayerWeights.cols(blackOffset, blackOffset + 64 * 5 - 1) * longBlackInput) * firstLayerScale + firstLayerBiases;
}