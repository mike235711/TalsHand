#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <iostream> // For std::cerr, std::endl
#include <cstdlib>  // For exit()
#include <cassert>
#include <algorithm> // for std::clamp if you like
#include <cstdint>
#include <limits>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifdef __SSE4_1__
#include <smmintrin.h> // SSE4.1
#endif

#include "network.h"

// Define static members declared in AccumulatorStack
namespace NNUEU
{
    const int8_t *AccumulatorStack::secondLayer1WeightsBlockWhiteTurn = nullptr;
    const int8_t *AccumulatorStack::secondLayer2WeightsBlockWhiteTurn = nullptr;
    const int8_t *AccumulatorStack::secondLayer1WeightsBlockBlackTurn = nullptr;
    const int8_t *AccumulatorStack::secondLayer2WeightsBlockBlackTurn = nullptr;
}

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

        return true;
    }

    // The engine is built to get an evaluation of the position with high values being good for the engine.
    // The NNUE is built to give an evaluation of the position with high values being good for whose turn it is.
    // This function gives an evaluation with high values being good for engine.
    int16_t Network::evaluate(const BitPosition &position, bool ourTurn, NNUEU::AccumulatorStack &accumulatorStack, const Transformer &transformer) const
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
            out = forwardPassDebug(updatedAcc.inputTurn[0], accumulatorStack.secondLayer1WeightsBlockWhiteTurn, accumulatorStack.secondLayer2WeightsBlockWhiteTurn);
#else
            out = forwardPass(updatedAcc.inputTurn[0], accumulatorStack.secondLayer1WeightsBlockWhiteTurn, accumulatorStack.secondLayer2WeightsBlockWhiteTurn);
#endif
        }

        else
        {
#ifndef NDEBUG
            out = forwardPassDebug(updatedAcc.inputTurn[1], accumulatorStack.secondLayer1WeightsBlockBlackTurn, accumulatorStack.secondLayer2WeightsBlockBlackTurn);
#else
            out = forwardPass(updatedAcc.inputTurn[1], accumulatorStack.secondLayer1WeightsBlockBlackTurn, accumulatorStack.secondLayer2WeightsBlockBlackTurn);
#endif
        }

        // Change evaluation from player to move perspective to our perspective
        if (ourTurn)
            return out;

        return 4096 - out;
    }

    int16_t Network::forwardPass(int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const
    // This function should pass using simd instructions an array pInput of 16 int16's through a neural network.
    // There are two first layers of 8 by 4 each taking the same pInput, after concatenating the outputs of both first layers,
    // the second layer is 8 by 4, the third layer is 4 by 1.
    //
    // The input is int16, and the weights are int8. So before multiplying we reduce int16 to int8
    // (by clipping to max int8) and clip negatives to zero before each layer pass.
    {
#if defined(__ARM_NEON)

        // Load inputs
        int16x8_t input_vector = vld1q_s16(pInput);               // Load 8 int16_t elements into an int16x8_t
        int8x8_t narrowed_vector = vqmovn_s16(input_vector);      // Narrow to int8x8_t with saturation
        int8x8_t vector = vmax_s8(narrowed_vector, vdup_n_s8(0)); // Clip negatives to 0

        // Layer 1
        int8x8_t weight1[8];
        for (int i = 0; i < 4; ++i)
        {
            weight1[i] = vld1_s8(pWeights11 + i * 8);
            weight1[i + 4] = vld1_s8(pWeights12 + i * 8);
        }

        int16x8_t output1 = {0};
        for (int i = 0; i < 8; ++i)
        {
            output1[i] = vaddvq_s16(vmull_s8(vector, weight1[i]));
        }

        int16x8_t bias1 = vld1q_s16(weights.secondBias);

        output1 = vmaxq_s16(vshrq_n_s16(vaddq_s16(bias1, output1), 6), vdupq_n_s16(0));

        // Layer 2
        int8x8_t input2 = vqmovn_s16(output1);

        int8x8_t weight2[4];

        // Load weights into vectors
        for (int i = 0; i < 4; ++i)
        {
            weight2[i] = vld1_s8(weights.thirdW + i * 8);
        }

        int16x4_t bias2 = vld1_s16(weights.thirdBias);

        int16x4_t output2;

        // Perform the computations
        for (int i = 0; i < 4; ++i)
        {
            int16_t temp = vaddvq_s16(vmull_s8(input2, weight2[i])) + bias2[i];
            output2[i] = temp >> 6; // Store the result in the array
        }

        // Apply ReLU activation
        output2 = vmax_s16(output2, vdup_n_s16(0));

        // Layer 3
        int8x8_t input3 = vqmovn_s16(vcombine_s16(output2, vdup_n_s16(0)));

        // Load only 4 bytes and zero-pad safely
        int8x8_t weight3 = vld1_s8(weights.finalW);

        int16_t output3 = vaddvq_s16(vmull_s8(input3, weight3)) + weights.finalBias;

        return output3;

#elif defined(__AVX2__) || defined(__AVX__) || defined(__SSSE3__) || defined(__SSE4_1__)
        const int16_t *pBias1 = weights.secondBias;
        const int8_t *pWeights2 = weights.thirdW;
        const int16_t *pBias2 = weights.thirdBias;
        const int8_t *pWeights3 = weights.finalW;
        const int16_t *pBias3 = &weights.finalBias;
        //
        // Layer 0:
        //  - Load 8 x int16
        //  - Narrow to int8 with saturation
        //  - Clip negatives to zero (ReLU)
        //
        __m128i input_16 = _mm_loadu_si128((__m128i *)pInput); // 8 x int16

        // ReLU on int16
        __m128i zero = _mm_setzero_si128();
        input_16 = _mm_max_epi16(input_16, zero);

        // Now saturate down to 8-bit signed.
        // _mm_packs_epi16 packs 8 x int16 into 8 x int8 (lower half) + 8 x int16 into 8 x int8 (upper half)
        // We'll supply zero for the upper half so we only keep 8 real int8 in the lower half.
        __m128i input_16_high = _mm_setzero_si128();
        __m128i packed_8 = _mm_packs_epi16(input_16, input_16_high);
        // packs_epi16 produces 16 int8 in the 128-bit register,
        // but the top 8 are from the second argument (which is zero). We only need the low 8 for further multiply.
        // If you want them in an int8_t array, you can store the low 8 bytes.

        //
        // Layer 1: Suppose we have 8 separate int8-weight vectors, each length=8
        //  - Multiply each weight vector by our 8-element input
        //  - Sum (horizontal sum) => 1 value
        //  - Add bias
        //  - Shift >> 6
        //  - ReLU
        //
        // For 8 separate neurons, let's load each weight as __m128i with zero-extended or sign-extended.
        // Then multiply. SSE doesn't have a single "dot-product" instruction for 8 int8s, so we do sign-extension
        // and multiply in 16 bits, then horizontal add.
        //
        __m128i result1[8];

        // We will compute:
        //   output[i] = sum_{k=0..7} (input8[k] * weight1[i][k]) + bias1[i]
        //   Then shift >> 6 and clamp ReLU.

        // Load input as 8 int8 in the lower half of a register, sign-extend to 16 bits
        __m128i in_8_lo = _mm_cvtepi8_epi16(packed_8);
        // now in_8_lo is 8 x int16 with the sign-extended bytes from input.

        // We'll store the results in a single SSE register at the end.
        __m128i bias1_v = _mm_loadu_si128((__m128i *)pBias1); // 8 x int16

        for (int i = 0; i < 8; i++)
        {
            // Load the i-th weight vector of length=8 (int8).
            // pWeights11 + i*8 or pWeights12 + i*8 as appropriate.
            // In your example, you have 4 from pWeights11, 4 from pWeights12, etc.
            // We'll just show a single version (adjust logic to your layer shape).
            __m128i w_i_8 = _mm_loadl_epi64((__m128i const *)(pWeights11 + i * 8));
            // w_i_8 has 8 x int8 in the lower 64 bits

            // Sign-extend to 8 x int16
            __m128i w_i_16 = _mm_cvtepi8_epi16(w_i_8);

            // Multiply each pair: in_8_lo[k]*w_i_16[k] => 8 x int16
            __m128i prod = _mm_mullo_epi16(in_8_lo, w_i_16);

            // Now we want sum of these 8 int16. Use _mm_hadd_epi16 or do it manually.
            // SSE4.1 doesn't have a single "horizontal add of all 8 lanes to one value",
            // but we can do a few steps:
            __m128i sum1 = _mm_hadd_epi16(prod, prod); // pairwise add => 4 x int16 repeated
            __m128i sum2 = _mm_hadd_epi16(sum1, sum1); // => 2 x int16 repeated
            __m128i sum3 = _mm_hadd_epi16(sum2, sum2); // => 1 x int16 repeated

            // The actual sum is in the low 16 bits of sum3
            int16_t dot = (int16_t)_mm_extract_epi16(sum3, 0);

            // Add bias
            int16_t bias_i = ((int16_t *)&bias1_v)[i];
            dot += bias_i;

            // Shift >> 6
            dot >>= 6;

            // ReLU
            if (dot < 0)
                dot = 0;

            // Keep it in result1[i]. We'll convert to SSE after the loop if we want a vector of these 8.
            ((int16_t *)&result1[0])[i] = dot;
        }

        // Combine the 8 results into an SSE register:
        __m128i output1_16 = _mm_loadu_si128((__m128i *)&result1[0]);

        //
        // Next layers: exactly the same pattern
        //  - Convert to int8 with saturate
        //  - Multiply by weight vectors
        //  - Accumulate + bias
        //  - Shift, ReLU
        //
        // We'll do a short example for the second layer, which has 4 outputs.
        //
        // Convert to int8
        __m128i out1_clamped = _mm_max_epi16(output1_16, zero); // ReLU just in case
        __m128i out1_packed = _mm_packs_epi16(out1_clamped, zero);
        // Now the low 8 bytes of out1_packed hold the 8 int8 we want.

        // For layer 2, which has 4 outputs:
        int16_t out2[4];
        __m128i bias2_v = _mm_loadl_epi64((__m128i *)pBias2); // 4 x int16 in memory
        // Each neuron = sum of 8 products + bias
        for (int i = 0; i < 4; i++)
        {
            // load the i-th weight vector (8 int8)
            __m128i w2_i_8 = _mm_loadl_epi64((__m128i const *)(pWeights2 + i * 8));
            __m128i w2_i_16 = _mm_cvtepi8_epi16(w2_i_8); // 8 x int16

            // sign-extend input as well
            __m128i in2_16 = _mm_cvtepi8_epi16(out1_packed);

            __m128i prod = _mm_mullo_epi16(in2_16, w2_i_16);

            // horizontal sum:
            __m128i sum1 = _mm_hadd_epi16(prod, prod);
            __m128i sum2 = _mm_hadd_epi16(sum1, sum1);
            __m128i sum3 = _mm_hadd_epi16(sum2, sum2);
            int16_t dot = (int16_t)_mm_extract_epi16(sum3, 0);

            // add bias
            int16_t bias_i = ((int16_t *)&bias2_v)[i];
            dot += bias_i;

            // shift >> 6
            dot >>= 6;

            // ReLU
            if (dot < 0)
                dot = 0;

            out2[i] = dot;
        }

        //
        // Layer 3 (4 -> 1)
        //
        int16_t out2_clamped[4];
        for (int i = 0; i < 4; i++)
            out2_clamped[i] = (out2[i] < 0) ? 0 : out2[i];

        // Convert 4 x int16 to int8 for final multiply
        __m128i out2_vec_16 = _mm_loadl_epi64((__m128i const *)out2_clamped); // loads 4 x int16
        out2_vec_16 = _mm_max_epi16(out2_vec_16, zero);                       // ReLU again, if needed
        __m128i out2_packed = _mm_packs_epi16(out2_vec_16, zero);             // saturate to int8

        // multiply by weight3 (4 x int8) => single output
        // plus bias3
        __m128i w3_8 = _mm_loadl_epi64((__m128i const *)pWeights3); // 4 x int8 in the lower half
        __m128i w3_16 = _mm_cvtepi8_epi16(w3_8);

        // sign-extend out2
        __m128i in3_16 = _mm_cvtepi8_epi16(out2_packed); // 4 x int16

        __m128i prod3 = _mm_mullo_epi16(in3_16, w3_16);
        __m128i sum3_1 = _mm_hadd_epi16(prod3, prod3);
        __m128i sum3_2 = _mm_hadd_epi16(sum3_1, sum3_1);
        __m128i sum3_3 = _mm_hadd_epi16(sum3_2, sum3_2);
        int16_t dot3 = (int16_t)_mm_extract_epi16(sum3_3, 0);

        // add bias
        int16_t bias3_v = pBias3[0];
        dot3 += bias3_v;

        // That final dot3 is your result
        return dot3;

#else

#endif
    }

} // namespace NNUEU
