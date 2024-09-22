#ifndef POSITION_EVAL_H
#define POSITION_EVAL_H

#include <vector>
#include "bitposition.h"
#include <cstdint>


namespace NNUE
{
    // Global variables for NNUE parameters
    extern int16_t whiteInputTurn[8];
    extern int16_t blackInputTurn[8];
    extern int16_t whiteInputNotTurn[8];
    extern int16_t blackInputNotTurn[8];

    extern int16_t firstLayer1Weights[40960][8];
    extern int16_t firstLayer2Weights[40960][8];
    extern int16_t firstLayer1InvertedWeights[40960][8];
    extern int16_t firstLayer2InvertedWeights[40960][8];

    extern int8_t secondLayerWeights[16 * 8];
    extern int8_t thirdLayerWeights[8 * 4];
    extern int8_t finalLayerWeights[4];

    extern int16_t firstLayer1Biases[8];
    extern int16_t firstLayer2Biases[8];
    extern int16_t secondLayerBiases[8];
    extern int16_t thirdLayerBiases[4];
    extern int16_t finalLayerBias;

    // Declare the initialization function
    void initNNUEParameters();

    // Declare the neural network processing functions
    int16_t evaluationFunction(bool ourTurn);

    // Declare function to initialize the whiteInput and blackInput
    void initializeNNUEInput(const BitPosition position);

    // Declare functions to update efficiently NNUE input
    void addOnInput(int whiteKingPosition, int blackKingPosition, int subIndex);
    void removeOnInput(int whiteKingPosition, int blackKingPosition, int subIndex);
    void moveWhiteKingNNUEInput(BitPosition &position);
    void moveBlackKingNNUEInput(BitPosition &position);
}

namespace NNUEU
{
    // Global variables for NNUEU parameters
    extern int16_t inputWhiteTurn[8];
    extern int16_t inputBlackTurn[8];

    extern int16_t firstLayerWeights[640][8];
    extern int16_t firstLayerInvertedWeights[640][8];

    extern int8_t secondLayer1Weights[64][8 * 4];
    extern int8_t secondLayer2Weights[64][8 * 4];

    extern int8_t secondLayer1WeightsBlockWhiteTurn[8 * 4];
    extern int8_t secondLayer2WeightsBlockWhiteTurn[8 * 4];
    extern int8_t secondLayer1WeightsBlockBlackTurn[8 * 4];
    extern int8_t secondLayer2WeightsBlockBlackTurn[8 * 4];

    extern int8_t thirdLayerWeights[8 * 4];
    extern int8_t finalLayerWeights[4];

    extern int16_t firstLayerBiases[8];
    extern int16_t secondLayer1Biases[4];
    extern int16_t secondLayer2Biases[4];
    extern int16_t thirdLayerBiases[4];
    extern int16_t finalLayerBias;

    // Declare the initialization function
    void initNNUEParameters();

    // Declare the neural network processing functions
    int16_t evaluationFunction(bool ourTurn);

    // Declare function to initialize the inputWhiteTurn and inputBlackTurn
    void initializeNNUEInput(const BitPosition position);

    // Declare functions to update efficiently NNUE input
    void addOnInput(int whiteKingPosition, int blackKingPosition, int subIndex);
    void removeOnInput(int whiteKingPosition, int blackKingPosition, int subIndex);
    void moveWhiteKingNNUEInput(BitPosition &position);
    void moveBlackKingNNUEInput(BitPosition &position);
}

#endif // POSITION_EVAL_H
