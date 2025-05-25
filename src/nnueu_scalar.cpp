// nnueu_scalar.cpp – Debug‑only scalar reference implementation of the NNUEU forward pass

#ifndef NDEBUG

#include "simd.h"
#include <cassert>
#include <cstdint>
#include <algorithm> // std::clamp

// -------------------------------------------------------------------------------------------------
// Small helpers
// -------------------------------------------------------------------------------------------------
static inline int8_t relu_and_saturate_int8(int16_t x)
{
    // ReLU first, then clip into int8 range.
    if (x < 0)
        x = 0;
    if (x > 127)
        x = 127;
    return static_cast<int8_t>(x);
}

static inline int16_t relu_shift6_int16(int32_t x)
{
    // Add >> 6 that the real network applies, then ReLU, finally narrow to int16.
    x >>= 6; // arithmetic shift – sign preserved
    if (x < 0)
        x = 0;
    if (x > 32767)
        x = 32767;
    return static_cast<int16_t>(x);
}

// Plain scalar forward pass (no SIMD)
int16_t fullNnueuPassScalar(const int16_t *pInput,
                            const int8_t *pWeights11,
                            const int8_t *pWeights12)
{
    // 1. ---------------------------------------------------------------- Layer 0 (input preprocessing)
    int8_t input8[8];
    for (int i = 0; i < 8; ++i)
    {
        // Same pipeline as the SIMD code: clip to int8, ReLU (negatives → 0).
        int16_t x = pInput[i];
        input8[i] = relu_and_saturate_int8(x);
    }

    // 2. ---------------------------------------------------------------- First hidden layer (8 neurons)
    int16_t l1[8];
    for (int n = 0; n < 8; ++n)
    {
        const int8_t *w = (n < 4 ? pWeights11 + n * 8
                                 : pWeights12 + (n - 4) * 8);
        int32_t acc = 0;
        for (int k = 0; k < 8; ++k)
            acc += static_cast<int32_t>(input8[k]) * static_cast<int32_t>(w[k]);

        acc += static_cast<int32_t>(secondLayerBiases[n]);
        l1[n] = relu_shift6_int16(acc);
    }

    // 3. ---------------------------------------------------------------- Second hidden layer (4 neurons)
    int8_t l1_int8[8];
    for (int i = 0; i < 8; ++i)
    {
        int16_t x = l1[i];
        if (x > 127)
            x = 127;
        if (x < -128)
            x = -128; // shouldn’t happen after ReLU but keep symmetric to SIMD code
        l1_int8[i] = static_cast<int8_t>(x);
    }

    int16_t l2[4];
    for (int n = 0; n < 4; ++n)
    {
        const int8_t *w = thirdLayerWeights + n * 8;
        int32_t acc = 0;
        for (int k = 0; k < 8; ++k)
            acc += static_cast<int32_t>(l1_int8[k]) * static_cast<int32_t>(w[k]);

        acc += static_cast<int32_t>(thirdLayerBiases[n]);
        l2[n] = relu_shift6_int16(acc);
    }

    // 4. ---------------------------------------------------------------- Output layer (1 neuron)
    int8_t l2_int8[8] = {0};
    for (int i = 0; i < 4; ++i)
    {
        int16_t x = l2[i];
        if (x > 127)
            x = 127;
        if (x < -128)
            x = -128;
        l2_int8[i] = static_cast<int8_t>(x);
    }

    int32_t acc = 0;
    for (int k = 0; k < 8; ++k)
        acc += static_cast<int32_t>(l2_int8[k]) * static_cast<int32_t>(finalLayerWeights[k]);

    acc += static_cast<int32_t>(finalLayerBias);

    // No shift/ReLU on the very last layer – we mirror SIMD exactly.
    if (acc > 32767)
        acc = 32767;
    if (acc < -32768)
        acc = -32768;
    return static_cast<int16_t>(acc);
}

int16_t fullNnueuPassDebug(const int16_t *pInput,
                           const int8_t *pWeights11,
                           const int8_t *pWeights12)
{
    int16_t fast = fullNnueuPass(const_cast<int16_t *>(pInput), // existing SIMD routine takes non‑const
                                 pWeights11,
                                 pWeights12);
    int16_t slow = fullNnueuPassScalar(pInput, pWeights11, pWeights12);
    assert(fast == slow && "NNUEU SIMD/scalar mismatch evaluation detected!");
    return fast; // propagate the fast‑path result so callers remain unchanged.
}

#endif // NDEBUG
