#ifndef POSITION_EVAL_H
#define POSITION_EVAL_H

#include <vector>
#include <armadillo>
#include "bitposition.h"

// Global variables for NNUE parameters
extern arma::vec whiteInput;
extern arma::vec blackInput;
extern arma::mat firstLayerWeights;
extern arma::mat secondLayerWeights;
extern arma::mat thirdLayerWeights;
extern arma::mat finalLayerWeights;

extern arma::vec firstLayerBiases;
extern arma::vec secondLayerBiases;
extern arma::vec thirdLayerBiases;
extern arma::vec finalLayerBiases;

extern float firstLayerScale;
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
void addOnWhiteInput(int index);
void removeOnWhiteInput(int index);
void addOnBlackInput(int index);
void removeOnBlackInput(int index);
void moveWhiteKingNNUEInput(const BitPosition &position);
void moveBlackKingNNUEInput(const BitPosition &position);

#endif // POSITION_EVAL_H
