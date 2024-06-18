#ifndef POSITION_EVAL_H
#define POSITION_EVAL_H

#include <vector>
#include <armadillo>
#include "bitposition.h"

// Global variables for NNUE parameters
extern arma::vec nnueInput;
extern arma::mat firstLayerWeights;
extern arma::mat secondLayerWeights;
extern arma::mat thirdLayerWeights;
extern arma::mat finalLayerWeights;

extern arma::vec firstLayerBiases;
extern arma::vec secondLayerBiases;
extern arma::vec thirdLayerBiases;
extern arma::vec finalLayerBiases;

// Declare functions to load weights and biases
std::vector<std::vector<float>> load_weights(const std::string &file_path);
std::vector<float> load_biases(const std::string &file_path);

// Declare utility conversion functions
arma::vec convertToArmaVec(const std::vector<float> &vec);
arma::mat convertToArmaMatrix(const std::vector<std::vector<float>> &vec);

// Declare the initialization function
void initNNUEParameters();

// Declare the neural network processing functions
int16_t evaluationFunction(const BitPosition &position);

void initializeNNUEInput(const BitPosition position);

// Declare functions to update efficiently NNUE input
void addOnWhiteInput(int square, int pieceIndex, int whiteKingPosition);
void removeOnWhiteInput(int square, int pieceIndex, int whiteKingPosition);
void addOnBlackInput(int square, int pieceIndex, int blackKingPosition);
void removeOnBlackInput(int square, int pieceIndex, int blackKingPosition);
void moveWhiteKingNNUEInput(int newWhiteKingPosition, uint64_t whitePawnsBit, uint64_t whiteKnightsBit, uint64_t whiteBishopsBit, uint64_t whiteRooksBit, uint64_t whiteQueensBit);
void moveBlackKingNNUEInput(int newBlackKingPosition, uint64_t blackPawnsBit, uint64_t blackKnightsBit, uint64_t blackBishopsBit, uint64_t blackRooksBit, uint64_t blackQueensBit);

#endif // POSITION_EVAL_H
