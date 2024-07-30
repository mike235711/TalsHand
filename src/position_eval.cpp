#include "position_eval.h"
#include <fstream>
#include <sstream>
#include <armadillo>
#include "bitposition.h"
#include "bit_utils.h" // Bit utility functions
#include "precomputed_moves.h"

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

    // Iterate over each row of the input vector
    for (size_t i = 0; i < vec.size(); ++i)
    {
        // Iterate over each column of the current row
        for (size_t j = 0; j < vec[i].size(); ++j)
        {
            result(i, j) = vec[i][j];
        }
    }

    // Return the populated Armadillo matrix
    return result;
}

arma::mat convertToInvertedArmaMatrix(const std::vector<std::vector<int8_t>> &vec)
{
    arma::mat result(vec.size(), vec[0].size());

    // Iterate over each row of the input vector
    for (size_t i = 0; i < vec.size(); ++i)
    {
        // Iterate over each element of the current row
        for (size_t j = 0; j < vec[i].size(); ++j)
        {
            // Invert the element in the row accordingly
            int kingPos = std::floor(j / 640);
            int pieceType = std::floor((j - (kingPos * 640)) / 64);
            int square = j % 64;

            int newPieceType{(pieceType + 5) % 10};
            int newIndex{invertIndex(kingPos) * 640 + newPieceType * 64 + invertIndex(square)};
            result(i, newIndex) = vec[i][j];
        }
    }

    // Return the populated Armadillo matrix
    return result;
}

arma::vec blockInput {arma::zeros<arma::vec>(64 * 10)};

arma::vec whiteInputTurn;
arma::vec blackInputTurn;
arma::vec whiteInputNotTurn;
arma::vec blackInputNotTurn;

arma::mat firstLayer1Weights;
arma::mat firstLayer2Weights;
arma::mat firstLayer1InvertedWeights;
arma::mat firstLayer2InvertedWeights;

arma::mat secondLayerWeights;
arma::mat thirdLayerWeights;
arma::mat finalLayerWeights;

arma::vec firstLayer1Biases;
arma::vec firstLayer2Biases;
arma::vec secondLayerBiases;
arma::vec thirdLayerBiases;
arma::vec finalLayerBiases;

float firstLayer1Scale;
float firstLayer2Scale;
float secondLayerScale;
float thirdLayerScale;
float finalLayerScale;

extern bool ENGINEISWHITE;

void initNNUEParameters()
{
    const std::string modelDir = "models/quantized_model_v1_param_350_tomove_first_flip_full_epoch_5/";
    // Load weights
    std::vector<std::vector<int8_t>> firstLayer1WeightsData = load_weights(modelDir + "first_linear1_weights.csv");
    std::vector<std::vector<int8_t>> firstLayer2WeightsData = load_weights(modelDir + "first_linear2_weights.csv");

    std::vector<std::vector<int8_t>> secondLayerWeightsData = load_weights(modelDir + "second_layer_weights.csv");
    std::vector<std::vector<int8_t>> thirdLayerWeightsData = load_weights(modelDir + "third_layer_weights.csv");
    std::vector<std::vector<int8_t>> finalLayerWeightsData = load_weights(modelDir + "final_layer_weights.csv");

    // Convert weights to Armadillo matrices
    firstLayer1Weights = convertToArmaMatrix(firstLayer1WeightsData);
    firstLayer2Weights = convertToArmaMatrix(firstLayer2WeightsData);
    // Convert and invert the weights
    firstLayer1InvertedWeights = convertToInvertedArmaMatrix(firstLayer1WeightsData);
    firstLayer2InvertedWeights = convertToInvertedArmaMatrix(firstLayer2WeightsData);

    secondLayerWeights = convertToArmaMatrix(secondLayerWeightsData);
    thirdLayerWeights = convertToArmaMatrix(thirdLayerWeightsData);
    finalLayerWeights = convertToArmaMatrix(finalLayerWeightsData);

    // Print dimensions to verify
    std::cout << "Dimensions of firstLayer1Weights: " << firstLayer1Weights.n_rows << " x " << firstLayer1Weights.n_cols << std::endl;
    std::cout << "Dimensions of firstLayer2Weights: " << firstLayer2Weights.n_rows << " x " << firstLayer2Weights.n_cols << std::endl;
    std::cout << "Dimensions of firstLayer1InvertedWeights: " << firstLayer1InvertedWeights.n_rows << " x " << firstLayer1InvertedWeights.n_cols << std::endl;
    std::cout << "Dimensions of firstLayer2InvertedWeights: " << firstLayer2InvertedWeights.n_rows << " x " << firstLayer2InvertedWeights.n_cols << std::endl;
    std::cout << "Dimensions of secondLayerWeights: " << secondLayerWeights.n_rows << " x " << secondLayerWeights.n_cols << std::endl;
    std::cout << "Dimensions of thirdLayerWeights: " << thirdLayerWeights.n_rows << " x " << thirdLayerWeights.n_cols << std::endl;
    std::cout << "Dimensions of finalLayerWeights: " << finalLayerWeights.n_rows << " x " << finalLayerWeights.n_cols << std::endl;

    // Load biases
    std::vector<float> firstLayer1BiasesData = load_biases(modelDir + "first_linear1_biases.csv");
    std::vector<float> firstLayer2BiasesData = load_biases(modelDir + "first_linear2_biases.csv");
    std::vector<float> secondLayerBiasesData = load_biases(modelDir + "second_layer_biases.csv");
    std::vector<float> thirdLayerBiasesData = load_biases(modelDir + "third_layer_biases.csv");
    std::vector<float> finalLayerBiasesData = load_biases(modelDir + "final_layer_biases.csv");

    // Convert biases to Armadillo vectors
    firstLayer1Biases = convertToArmaVec(firstLayer1BiasesData);
    firstLayer2Biases = convertToArmaVec(firstLayer2BiasesData);
    secondLayerBiases = convertToArmaVec(secondLayerBiasesData);
    thirdLayerBiases = convertToArmaVec(thirdLayerBiasesData);
    finalLayerBiases = convertToArmaVec(finalLayerBiasesData);

    // Print dimensions to verify
    std::cout << "Dimensions of firstLayer1Biases: " << firstLayer1Biases.n_elem << std::endl;
    std::cout << "Dimensions of firstLayer2Biases: " << firstLayer2Biases.n_elem << std::endl;
    std::cout << "Dimensions of secondLayerBiases: " << secondLayerBiases.n_elem << std::endl;
    std::cout << "Dimensions of thirdLayerBiases: " << thirdLayerBiases.n_elem << std::endl;
    std::cout << "Dimensions of finalLayerBiases: " << finalLayerBiases.n_elem << std::endl;

    // Load scales
    firstLayer1Scale = load_scale(modelDir + "first_linear1_scales.csv");
    firstLayer2Scale = load_scale(modelDir + "first_linear2_scales.csv");
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
        secondLayerOutput = (secondLayerWeights * arma::join_cols(whiteInputTurn, blackInputNotTurn)) * secondLayerScale + secondLayerBiases;
    else
        secondLayerOutput = (secondLayerWeights * arma::join_cols(blackInputTurn, whiteInputNotTurn)) * secondLayerScale + secondLayerBiases;
    
    secondLayerOutput = arma::clamp(secondLayerOutput, 0.0, 1.0);

    arma::vec thirdLayerOutput = (thirdLayerWeights * secondLayerOutput) * thirdLayerScale + thirdLayerBiases;
    thirdLayerOutput = arma::clamp(thirdLayerOutput, 0.0, 1.0);

    arma::vec finalOutput = (finalLayerWeights * thirdLayerOutput) * finalLayerScale + finalLayerBiases;
    if (ourTurn)
        return static_cast<float>(finalOutput[0]);
    else
        return 1 - static_cast<float>(finalOutput[0]);
}


void initializeNNUEInput(const BitPosition position)
// Initialize the NNUE accumulators.
{
    uint64_t whitePawnsBit = position.getWhitePawnsBits();
    uint64_t whiteKnightsBit = position.getWhiteKnightsBits();
    uint64_t whiteBishopsBit = position.getWhiteBishopsBits();
    uint64_t whiteRooksBit = position.getWhiteRooksBits();
    uint64_t whiteQueensBit = position.getWhiteQueensBits();
    int whiteKingPosition = position.getWhiteKingPosition();

    uint64_t blackPawnsBit = position.getBlackPawnsBits();
    uint64_t blackKnightsBit = position.getBlackKnightsBits();
    uint64_t blackBishopsBit = position.getBlackBishopsBits();
    uint64_t blackRooksBit = position.getBlackRooksBits();
    uint64_t blackQueensBit = position.getBlackQueensBits();
    int blackKingPosition = position.getBlackKingPosition();

    // Offsets
    int whiteOffset = whiteKingPosition * 64 * 10;
    int blackOffset = blackKingPosition * 64 * 10;

    for (unsigned short index : getBitIndices(whitePawnsBit))
        blockInput[index] = 1;

    for (unsigned short index : getBitIndices(whiteKnightsBit))
        blockInput[64 + index] = 1;

    for (unsigned short index : getBitIndices(whiteBishopsBit))
        blockInput[64 * 2 + index] = 1;

    for (unsigned short index : getBitIndices(whiteRooksBit))
        blockInput[64 * 3 + index] = 1;

    for (unsigned short index : getBitIndices(whiteQueensBit))
        blockInput[64 * 4 + index] = 1;
    
    for (unsigned short index : getBitIndices(blackPawnsBit))
        blockInput[64 * 5 + index] = 1;
    
    for (unsigned short index : getBitIndices(blackKnightsBit))
        blockInput[64 * 6 + index] = 1;
    
    for (unsigned short index : getBitIndices(blackBishopsBit))
        blockInput[64 * 7 + index] = 1;

    for (unsigned short index : getBitIndices(blackRooksBit))
        blockInput[64 * 8 + index] = 1;

    for (unsigned short index : getBitIndices(blackQueensBit))
        blockInput[64 * 9 + index] = 1;
    

    // White turn (use normal NNUE)
    whiteInputTurn = (firstLayer1Weights.cols(whiteOffset, whiteOffset + 64 * 10 - 1) * blockInput) * firstLayer1Scale + firstLayer1Biases;
    blackInputNotTurn = (firstLayer2Weights.cols(blackOffset, blackOffset + 64 * 10 - 1) * blockInput) * firstLayer2Scale + firstLayer2Biases;

    // Black turn (use inverted NNUE)
    blackInputTurn = (firstLayer1InvertedWeights.cols(blackOffset, blackOffset + 64 * 10 - 1) * blockInput) * firstLayer1Scale + firstLayer1Biases;
    whiteInputNotTurn = (firstLayer2InvertedWeights.cols(whiteOffset, whiteOffset + 64 * 10 - 1) * blockInput) * firstLayer2Scale + firstLayer2Biases;
}

// Helper functions to update the input vector
// They are used in bitposition.cpp inside makeNormalMove, makeCapture, setPiece and removePiece.

void addOnInput(int whiteKingPosition, int blackKingPosition, int subIndex)
{
    // White turn (use normal NNUE)
    whiteInputTurn += firstLayer1Weights.col(whiteKingPosition * 64 * 10  + subIndex) * firstLayer1Scale;
    blackInputNotTurn += firstLayer2Weights.col(blackKingPosition * 64 * 10 + subIndex) * firstLayer2Scale;
    // Black turn (use inverted NNUE)
    whiteInputNotTurn += firstLayer2InvertedWeights.col(whiteKingPosition * 64 * 10 + subIndex) * firstLayer2Scale;
    blackInputTurn += firstLayer1InvertedWeights.col(blackKingPosition * 64 * 10 + subIndex) * firstLayer1Scale;

    blockInput[subIndex] = 1;
}

void removeOnInput(int whiteKingPosition, int blackKingPosition, int subIndex)
{
    // White turn (use normal NNUE)
    whiteInputTurn -= firstLayer1Weights.col(whiteKingPosition * 64 * 10 + subIndex) * firstLayer1Scale;
    blackInputNotTurn -= firstLayer2Weights.col(blackKingPosition * 64 * 10 + subIndex) * firstLayer2Scale;
    // Black turn (use inverted NNUE)
    whiteInputNotTurn -= firstLayer2InvertedWeights.col(whiteKingPosition * 64 * 10 + subIndex) * firstLayer2Scale;
    blackInputTurn -= firstLayer1InvertedWeights.col(blackKingPosition * 64 * 10 + subIndex) * firstLayer1Scale;

    blockInput[subIndex] = 0;
}
// Move King functions to update nnueInput vector
void moveWhiteKingNNUEInput(BitPosition &position)
{
    int whiteOffset = position.getWhiteKingPosition() * 64 * 10;

    whiteInputTurn = (firstLayer1Weights.cols(whiteOffset, whiteOffset + 64 * 10 - 1) * blockInput) * firstLayer1Scale + firstLayer1Biases;
    whiteInputNotTurn = (firstLayer2InvertedWeights.cols(whiteOffset, whiteOffset + 64 * 10 - 1) * blockInput) * firstLayer2Scale + firstLayer2Biases;
}
void moveBlackKingNNUEInput(BitPosition &position)
{
    int blackOffset = position.getBlackKingPosition() * 64 * 10;

    blackInputTurn = (firstLayer1InvertedWeights.cols(blackOffset, blackOffset + 64 * 10 - 1) * blockInput) * firstLayer1Scale + firstLayer1Biases;
    blackInputNotTurn = (firstLayer2Weights.cols(blackOffset, blackOffset + 64 * 10 - 1) * blockInput) * firstLayer2Scale + firstLayer2Biases;
}

