#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "bitposition.h"
#include "accumulation.h"

namespace NNUEU
{
    class Network
    {
    public:
        // An *empty* network is still default-constructible – useful for unit tests
        Network() = default;
        explicit Network(const char *def) { load(def); }

        bool load(const std::string &dir);

        /**  Thread-safe, read-only evaluation.                                */
        int16_t evaluate(const BitPosition &position, NNUEU::AccumulatorStack &accumulatorStack, const Transformer &transformer) const;

        int16_t forwardPass(int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;

#ifndef NDEBUG
        int16_t forwardPassDebug(const int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;
#endif

    private:
        struct Weight
        {
            // Head: 2nd layer (acc -> HEAD_CONCAT) -> 3rd layer (HEAD_CONCAT -> THIRD_OUT_W) -> final (-> 1).
            alignas(64) int8_t thirdW[THIRD_OUT_W * HEAD_CONCAT] = {0};
            alignas(64) int8_t finalW[THIRD_OUT_W + 8] = {0}; // +8 pad so NEON 8-byte loads never over-read
            int16_t secondBias[HEAD_CONCAT] = {0};
            int16_t thirdBias[THIRD_OUT_W] = {0};
            int16_t finalBias = {0};
        };
        Weight weights;

#ifndef NDEBUG
        int16_t forwardPassScalar(const int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;
#endif
    };
} // Namespace NNUEU

int16_t *load_int16_array(const std::string &file_path, size_t cols);
