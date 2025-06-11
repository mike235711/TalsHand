#include <fstream>
#include <sstream>

#include "network.h"
#include "simd.h"

/////////////////////////////////
// Parameter loading utilities
/////////////////////////////////

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

int16_t *load_int16_array(const std::string &file_path, size_t cols)
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


namespace NNUEU
{

    bool Network::load(const std::string &modelDir = "models/NNUEU_quantized_model_v4_param_350_epoch_10/")
    {
        try
        {
            auto tempThirdLayerWeights = load_int8_1D_array(modelDir + "third_layer_weights.csv", 8 * 4);
            std::memcpy(weights.thirdW, tempThirdLayerWeights, sizeof(int8_t) * 8 * 4);
            delete[] tempThirdLayerWeights;

            auto tempFinalLayerWeights = load_int8_1D_array(modelDir + "final_layer_weights.csv", 4);
            std::memcpy(weights.finalW, tempFinalLayerWeights, sizeof(int8_t) * 4);
            // std::memset(finalLayerWeights + 4, 0, sizeof(int8_t) * 4);
            delete[] tempFinalLayerWeights;

            // Load biases
            auto tempSecondLayer1Biases = load_int16_array(modelDir + "second_layer_turn_biases.csv", 4);
            auto tempSecondLayer2Biases = load_int16_array(modelDir + "second_layer_not_turn_biases.csv", 4);

            // Concatenate biases directly
            std::memcpy(weights.secondBias, tempSecondLayer1Biases, sizeof(int16_t) * 4);
            std::memcpy(weights.secondBias + 4, tempSecondLayer2Biases, sizeof(int16_t) * 4);

            delete[] tempSecondLayer1Biases;
            delete[] tempSecondLayer2Biases;

            auto tempThirdLayerBiases = load_int16_array(modelDir + "third_layer_biases.csv", 4);
            std::memcpy(weights.thirdBias, tempThirdLayerBiases, sizeof(int16_t) * 4);
            delete[] tempThirdLayerBiases;

            weights.finalBias = load_int16(modelDir + "final_layer_biases.csv");
        }
        catch (const std::exception &e)
        {
            std::cerr << "NNUEU load failed: " << e.what() << '\n';
            return false;
        }

        return transformer.load(modelDir);
    }

    // The engine is built to get an evaluation of the position with high values being good for the engine.
    // The NNUE is built to give an evaluation of the position with high values being good for whose turn it is.
    // This function gives an evaluation with high values being good for engine.
    int16_t Network::evaluate(const BitPosition &position, bool ourTurn, NNUEU::AccumulatorStack &accumulatorStack) const
    {
        // Update incrementally from the last computed node
        accumulatorStack.forward_update_incremental(accumulatorStack.findLastComputedNode(position.getTurn()), position.getTurn(), transformer);

#ifndef NDEBUG
        accumulatorStack.verifyTopAgainstFresh(position, not position.getTurn(), transformer);
#endif

        // Change the NNUEU king positions if needed
        if (accumulatorStack.getStackKingPosition(0) != position.getKingPosition(0))
            accumulatorStack.changeWhiteKingPosition(position.getKingPosition(0), transformer);

        if (accumulatorStack.getStackKingPosition(1) != position.getKingPosition(1))
            accumulatorStack.changeBlackKingPosition(position.getKingPosition(1), transformer);

        assert(position.getKingPosition(0) == accumulatorStack.getStackKingPosition(0));
        assert(position.getKingPosition(1) == accumulatorStack.getStackKingPosition(1));

        int16_t out;
        AccumulatorState &updatedAcc = accumulatorStack.top();

        if (position.getTurn())
        {
#ifndef NDEBUG
            out = fullNnueuPassDebug(updatedAcc.inputTurn[0], accumulatorStack.secondLayer1WeightsBlockWhiteTurn, accumulatorStack.secondLayer2WeightsBlockWhiteTurn);
#else
            out = fullNnueuPass(updatedAcc.inputTurn[0], accumulatorStack.secondLayer1WeightsBlockWhiteTurn, accumulatorStack.secondLayer2WeightsBlockWhiteTurn);
#endif
        }

        else
        {
#ifndef NDEBUG
            out = fullNnueuPassDebug(updatedAcc.inputTurn[1], accumulatorStack.secondLayer1WeightsBlockBlackTurn, accumulatorStack.secondLayer2WeightsBlockBlackTurn);
#else
            out = fullNnueuPass(updatedAcc.inputTurn[1], accumulatorStack.secondLayer1WeightsBlockBlackTurn, accumulatorStack.secondLayer2WeightsBlockBlackTurn);
#endif
        }

        // Change evaluation from player to move perspective to our perspective
        if (ourTurn)
            return out;

        return 4096 - out;
    }

} // namespace NNUEU
