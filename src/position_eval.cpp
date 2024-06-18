#include "position_eval.h"
#include <fstream>
#include <sstream>
#include <armadillo>
#include "bitposition.h"
#include "bit_utils.h" // Bit utility functions

///////////////////////////
// NNUE Evaluation
///////////////////////////

std::vector<std::vector<float>> load_weights(const std::string &file_path)
{
    std::vector<std::vector<float>> weights;
    std::ifstream file(file_path);
    std::string line;

    while (std::getline(file, line))
    {
        std::vector<float> row;
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

arma::vec convertToArmaVec(const std::vector<float> &vec)
{
    arma::vec result(vec.size());
    for (size_t i = 0; i < vec.size(); ++i)
    {
        result(i) = vec[i];
    }
    return result;
}

arma::mat convertToArmaMatrix(const std::vector<std::vector<float>> &vec)
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
arma::vec nnueInput;
arma::mat firstLayerWeights;
arma::mat secondLayerWeights;
arma::mat thirdLayerWeights;
arma::mat finalLayerWeights;

arma::vec firstLayerBiases;
arma::vec secondLayerBiases;
arma::vec thirdLayerBiases;
arma::vec finalLayerBiases;

void initNNUEParameters()
{
    // Load weights
    std::vector<std::vector<float>> firstLayerWeightsData = load_weights("models/quantized_model_sample_same_first/white_linear_weights.csv");
    std::vector<std::vector<float>> secondLayerWeightsData = load_weights("models/quantized_model_sample_same_first/second_layer_weights.csv");
    std::vector<std::vector<float>> thirdLayerWeightsData = load_weights("models/quantized_model_sample_same_first/third_layer_weights.csv");
    std::vector<std::vector<float>> finalLayerWeightsData = load_weights("models/quantized_model_sample_same_first/final_layer_weights.csv");

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
    std::vector<float> firstLayerBiasesData = load_biases("models/quantized_model_sample_same_first/white_linear_biases.csv");
    std::vector<float> secondLayerBiasesData = load_biases("models/quantized_model_sample_same_first/second_layer_biases.csv");
    std::vector<float> thirdLayerBiasesData = load_biases("models/quantized_model_sample_same_first/third_layer_biases.csv");
    std::vector<float> finalLayerBiasesData = load_biases("models/quantized_model_sample_same_first/final_layer_biases.csv");

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
}

int16_t evaluationFunction(const BitPosition &position)
{
    // Process input through the NNUE
    arma::vec secondLayerOutput = secondLayerWeights * nnueInput + secondLayerBiases;
    secondLayerOutput = arma::clamp(secondLayerOutput, 0.0, 1.0);

    arma::vec thirdLayerOutput = thirdLayerWeights * secondLayerOutput + thirdLayerBiases;
    thirdLayerOutput = arma::clamp(thirdLayerOutput, 0.0, 1.0);

    arma::vec finalOutput = finalLayerWeights * thirdLayerOutput + finalLayerBiases;
    return static_cast<int>(finalOutput[0]);
}

void initializeNNUEInput(const BitPosition position)
{
    std::vector<float> whiteInput(64 * 64 * 5, 0);
    std::vector<float> blackInput(64 * 64 * 5, 0);

    auto setBits = [](uint64_t pieces, int offset, std::vector<float> &input)
    {
        while (pieces)
        {
            int index = __builtin_ctzll(pieces);
            input[offset + index] = 1;
            pieces &= pieces - 1;
        }
    };

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

    setBits(whitePawnsBit, whiteOffset, whiteInput);
    setBits(whiteKnightsBit, whiteOffset + 64, whiteInput);
    setBits(whiteBishopsBit, whiteOffset + 128, whiteInput);
    setBits(whiteRooksBit, whiteOffset + 192, whiteInput);
    setBits(whiteQueensBit, whiteOffset + 256, whiteInput);

    setBits(blackPawnsBit, blackOffset, blackInput);
    setBits(blackKnightsBit, blackOffset + 64, blackInput);
    setBits(blackBishopsBit, blackOffset + 128, blackInput);
    setBits(blackRooksBit, blackOffset + 192, blackInput);
    setBits(blackQueensBit, blackOffset + 256, blackInput);

    arma::vec whiteVec = firstLayerWeights * convertToArmaVec(whiteInput) + firstLayerBiases;
    arma::vec blackVec = firstLayerWeights * convertToArmaVec(blackInput) + firstLayerBiases;

    // Join columns (nnueInput is a external variable)
    nnueInput = arma::join_cols(whiteVec, blackVec);
}

// Helper functions to update the input vector
// They are used in bitposition.cpp inside makeNormalMove, makeCapture, setPiece and removePiece.
void addOnWhiteInput(int square, int pieceIndex, int whiteKingPosition)
{
    // Determine the index within nnueInput corresponding to white pieces
    int index = whiteKingPosition * 64 * 5 + square + pieceIndex * 64;

    // Create a vector of length 32 with initial zeros
    arma::vec adjustedVector(32, arma::fill::zeros);

    // Copy the relevant part from firstLayerWeights.col(index) to the adjusted vector
    if (index < firstLayerWeights.n_cols)
    {
        adjustedVector.head(firstLayerWeights.n_rows) = firstLayerWeights.col(index);
    }

    // Add adjustedVector to nnueInput
    nnueInput += adjustedVector;
}

void removeOnWhiteInput(int square, int pieceIndex, int whiteKingPosition)
{
    // Determine the index within nnueInput corresponding to white pieces
    int index = whiteKingPosition * 64 * 5 + square + pieceIndex * 64;

    // Create a vector of length 32 with initial zeros
    arma::vec adjustedVector(32, arma::fill::zeros);

    // Copy the relevant part from firstLayerWeights.col(index) to the adjusted vector
    if (index < firstLayerWeights.n_cols)
    {
        adjustedVector.head(firstLayerWeights.n_rows) = firstLayerWeights.col(index);
    }

    // Subtract adjustedVector from nnueInput
    nnueInput -= adjustedVector;
}

void addOnBlackInput(int square, int pieceIndex, int blackKingPosition)
{
    // Determine the index within nnueInput corresponding to black pieces
    int index = 16 + blackKingPosition * 64 * 5 + square + pieceIndex * 64;

    // Create a vector of length 32 with initial zeros
    arma::vec adjustedVector(32, arma::fill::zeros);

    // Copy the relevant part from firstLayerWeights.col(index) to the adjusted vector
    if (index < firstLayerWeights.n_cols)
    {
        adjustedVector.tail(firstLayerWeights.n_rows) = firstLayerWeights.col(index);
    }

    // Add adjustedVector to nnueInput
    nnueInput += adjustedVector;
}

void removeOnBlackInput(int square, int pieceIndex, int blackKingPosition)
{
    // Determine the index within nnueInput corresponding to black pieces
    int index = 16 + blackKingPosition * 64 * 5 + square + pieceIndex * 64;

    // Create a vector of length 32 with initial zeros
    arma::vec adjustedVector(32, arma::fill::zeros);

    // Copy the relevant part from firstLayerWeights.col(index) to the adjusted vector
    if (index < firstLayerWeights.n_cols)
    {
        adjustedVector.tail(firstLayerWeights.n_rows) = firstLayerWeights.col(index);
    }

    // Subtract adjustedVector from nnueInput
    nnueInput -= adjustedVector;
}

// Move King functions to update nnueInput vector
void moveWhiteKingNNUEInput(int newWhiteKingPosition, uint64_t whitePawnsBit, uint64_t whiteKnightsBit, uint64_t whiteBishopsBit, uint64_t whiteRooksBit, uint64_t whiteQueensBit)
{
    // Clear the white part of nnueInput
    nnueInput.subvec(0, nnueInput.n_elem / 2 - 1).zeros();

    auto setBits = [](uint64_t pieces, int offset, arma::vec &input)
    {
        while (pieces)
        {
            int index = __builtin_ctzll(pieces);
            input[offset + index] = 1;
            pieces &= pieces - 1;
        }
    };

    int whiteOffset = newWhiteKingPosition * 64 * 5;

    // Update nnueInput for all pieces relative to the new white king's position
    setBits(whitePawnsBit, whiteOffset, nnueInput);
    setBits(whiteKnightsBit, whiteOffset + 64, nnueInput);
    setBits(whiteBishopsBit, whiteOffset + 128, nnueInput);
    setBits(whiteRooksBit, whiteOffset + 192, nnueInput);
    setBits(whiteQueensBit, whiteOffset + 256, nnueInput);
}

void moveBlackKingNNUEInput(int newBlackKingPosition, uint64_t blackPawnsBit, uint64_t blackKnightsBit, uint64_t blackBishopsBit, uint64_t blackRooksBit, uint64_t blackQueensBit)
{
    // Clear the black part of nnueInput
    nnueInput.subvec(nnueInput.n_elem / 2, nnueInput.n_elem - 1).zeros();

    auto setBits = [](uint64_t pieces, int offset, arma::vec &input)
    {
        while (pieces)
        {
            int index = __builtin_ctzll(pieces);
            input[offset + index] = 1;
            pieces &= pieces - 1;
        }
    };

    int blackOffset = newBlackKingPosition * 64 * 5 + 64 * 64 * 5;

    // Update nnueInput for all pieces relative to the new black king's position
    setBits(blackPawnsBit, blackOffset, nnueInput);
    setBits(blackKnightsBit, blackOffset + 64, nnueInput);
    setBits(blackBishopsBit, blackOffset + 128, nnueInput);
    setBits(blackRooksBit, blackOffset + 192, nnueInput);
    setBits(blackQueensBit, blackOffset + 256, nnueInput);
}