#include <stdio.h>
#include <stdint.h>
#include <limits.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif


////////////////
// NNUE
////////////////

void add_8_int16(int16_t *a, const int16_t *b)
{
#if defined(__ARM_NEON)
    int16x8_t v1 = vld1q_s16(a);  // Load 8 int16_t values from array a
    int16x8_t v2 = vld1q_s16(b);  // Load 8 int16_t values from array b

    vst1q_s16(a, vaddq_s16(v1, v2)); // Store the result back to array a

#elif defined(__AVX512BW__) || defined(__AVX512CD__) || defined(__AVX512DQ__) || defined(__AVX512F__) || defined(__AVX512VL__)

#elif defined(__AVX2__) || defined(__AVX__)

#elif defined(__SSSE3__) || defined(__SSE4_1__)

#else

#endif
}

void substract_8_int16(int16_t *a, const int16_t *b) // For NNUE accumulation
{
#if defined(__ARM_NEON)
    int16x8_t v1 = vld1q_s16(a); // Load 8 int16_t values from array a
    int16x8_t v2 = vld1q_s16(b); // Load 8 int16_t values from array b

    vst1q_s16(a, vsubq_s16(v1, v2)); // Store the result back to array a

#elif defined(__AVX512BW__) || defined(__AVX512CD__) || defined(__AVX512DQ__) || defined(__AVX512F__) || defined(__AVX512VL__)

#elif defined(__AVX2__) || defined(__AVX__)

#elif defined(__SSSE3__) || defined(__SSE4_1__)

#else

#endif
}

int16_t fullNnuePass(int16_t *pInput1, int16_t *pInput2, int8_t *pWeights1, int16_t *pBias1,
                   int8_t *pWeights2, int16_t *pBias2, int8_t *pWeights3, int16_t *pBias3)
{
#if defined(__ARM_NEON)
    // Load 8 int16_t elements from the array into a NEON register
    int16x8_t vector1 = vld1q_s16(pInput1);
    int16x8_t vector2 = vld1q_s16(pInput2);

    // Narrow the 16-bit vector to 8-bit, handling overflow by saturating
    int8x8_t narrow_vector1 = vqmovn_s16(vector1);
    int8x8_t narrow_vector2 = vqmovn_s16(vector2);
    
    // Ensure no values are below 0
    int8x8_t clipped_vector1 = vmax_s8(narrow_vector1, vdup_n_s8(0)); 
    int8x8_t clipped_vector2 = vmax_s8(narrow_vector2, vdup_n_s8(0));

    // Load first layer weights (8 columns, each 16 elements, split into 16 int8x8_t)
    int8x8_t weight1[8][2];
    for (int i = 0; i < 8; ++i)
    {
        weight1[i][0] = vld1_s8(pWeights1 + i * 16 + 0);
        weight1[i][1] = vld1_s8(pWeights1 + i * 16 + 8);
    }
    // Compute first layer output
    int16x8_t output1 = {0};
    for (int i = 0; i < 8; ++i)
    {
        int16x8_t col = vaddq_s16(vmull_s8(clipped_vector1, weight1[i][0]),
                                  vmull_s8(clipped_vector2, weight1[i][1]));
        output1[i] = vaddvq_s16(col); // Horizontal sum of all 8 elements
    }
    // Load first layer biases
    int16x8_t bias1 = vld1q_s16(pBias1);
    // Add biases apply 6bit shift and then apply ReLU activation
    output1 = vmaxq_s16(vshrq_n_s16(vaddq_s16(bias1, output1), 6), vdupq_n_s16(0));

    // Convert int16x8 to int8x8
    // Layer 2
    int8x8_t input2 = vqmovn_s16(output1);
    int8x8_t weight2[4];

    // Load weights into vectors
    for (int i = 0; i < 4; ++i)
    {
        weight2[i] = vld1_s8(pWeights2 + i * 8);
    }

    int16x4_t bias2 = vld1_s16(pBias2);
    int16x4_t output2;

    // Perform the computations
    for (int i = 0; i < 4; ++i)
    {
        int16_t temp = vaddvq_s16(vmull_s8(input2, weight2[i])) + bias2[i];
        output2[i] = temp >> 6; // Store the result in the array
    }

    // Apply ReLU activation
    output2 = vmax_s16(output2, vdup_n_s16(0));

    // Change int16x4_t output2 to int8x8_t by reducing values to int8 and putting them in the lower part
    int8x8_t input3 = vqmovn_s16(vcombine_s16(output2, vdup_n_s16(0)));
    // Load third layer weights (4-element vector of int8's) into the lower part of an int8x8_t
    int8x8_t weight3 = vld1_s8(pWeights3);
    // Load third layer bias
    int16_t bias3 = pBias3[0];
    // Compute third layer output
    int16_t output3 = vaddvq_s16(vmull_s8(input3, weight3)) + bias3;

    return output3;

#elif defined(__AVX512BW__) || defined(__AVX512CD__) || defined(__AVX512DQ__) || defined(__AVX512F__) || defined(__AVX512VL__)

#elif defined(__AVX2__) || defined(__AVX__)

#elif defined(__SSSE3__) || defined(__SSE4_1__)

#else

#endif
}


int16_t fullNnueuPass(int16_t *pInput, int8_t *pWeights11, int8_t *pWeights12, int16_t *pBias1,
                     int8_t *pWeights2, int16_t *pBias2, int8_t *pWeights3, int16_t *pBias3)

// This function should pass using simd instructions an array of 16 int16's through a neural network.
// There are two first layers of 8 by 4 each taking the same input, after concatenating the outputs of both first layers, 
// the second layer is 8 by 4, the third layer is 4 by 1.

// the input is int16, and the weights are int8. So before multiplying we reduce int16 to int8, by clipping to max int8.
// We also clip negatives to zero before each layer pass.
{
#if defined(__ARM_NEON)
    // Layer 0
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

    int16x8_t bias1 = vld1q_s16(pBias1);
    output1 = vmaxq_s16(vshrq_n_s16(vaddq_s16(bias1, output1), 6), vdupq_n_s16(0));

    // Layer 2
    int8x8_t input2 = vqmovn_s16(output1);
    int8x8_t weight2[4];

    // Load weights into vectors
    for (int i = 0; i < 4; ++i)
    {
        weight2[i] = vld1_s8(pWeights2 + i * 8);
    }

    int16x4_t bias2 = vld1_s16(pBias2);
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
    int8x8_t weight3 = vld1_s8(pWeights3);
    int16_t bias3 = pBias3[0];
    int16_t output3 = vaddvq_s16(vmull_s8(input3, weight3)) + bias3;

    return output3;

#elif defined(__AVX512BW__) || defined(__AVX512CD__) || defined(__AVX512DQ__) || defined(__AVX512F__) || defined(__AVX512VL__)

#elif defined(__AVX2__) || defined(__AVX__)

#elif defined(__SSSE3__) || defined(__SSE4_1__)

#else

#endif
}