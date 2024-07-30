#ifndef POSITION_EVAL_H
#define POSITION_EVAL_H

#include <vector>
#include <armadillo>
#include "bitposition.h"

// Global variables for NNUE parameters
// extern arma::vec whiteTurnBlock;
// extern arma::vec blackTurnBlock;

extern arma::vec whiteInputTurn;
extern arma::vec blackInputTurn;
extern arma::vec whiteInputNotTurn;
extern arma::vec blackInputNotTurn;

extern arma::mat firstLayer1Weights;
extern arma::mat firstLayer2Weights;
extern arma::mat firstLayer1InvertedWeights;
extern arma::mat firstLayer2InvertedWeights;

extern arma::mat secondLayerWeights;
extern arma::mat thirdLayerWeights;
extern arma::mat finalLayerWeights;

extern arma::vec firstLayer1Biases;
extern arma::vec firstLayer2Biases;
extern arma::vec secondLayerBiases;
extern arma::vec thirdLayerBiases;
extern arma::vec finalLayerBiases;

extern float firstLayer1Scale;
extern float firstLayer2Scale;
extern float secondLayerScale;
extern float thirdLayerScale;
extern float finalLayerScale;

// Declare functions to load weights and biases
std::vector<std::vector<int8_t>> load_weights(const std::string &file_path);
std::vector<float> load_biases(const std::string &file_path);

// Declare utility conversion functions
arma::vec convertToArmaVec(const std::vector<float> &vec);
arma::vec convertToArmaVec(const std::vector<int8_t> &vec);
arma::mat convertToArmaMatrix(const std::vector<std::vector<int8_t>> &vec);

// Declare the initialization function
void initNNUEParameters();

// Declare the neural network processing functions
float evaluationFunction(const BitPosition &position, bool ourTurn);

// Declare function to initialize the whiteInput and blackInput
void initializeNNUEInput(const BitPosition position);

// Declare functions to update efficiently NNUE input
void addOnInput(int whiteKingPosition, int blackKingPosition, int subIndex);
void removeOnInput(int whiteKingPosition, int blackKingPosition, int subIndex);
void moveWhiteKingNNUEInput(BitPosition &position);
void moveBlackKingNNUEInput(BitPosition &position);

#endif // POSITION_EVAL_H
