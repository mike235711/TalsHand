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
        int16_t evaluate(const BitPosition &position, bool ourTurn, NNUEU::AccumulatorStack &accumulatorStack, const Transformer &transformer) const;

        int16_t forwardPass(int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;

#ifndef NDEBUG
        int16_t forwardPassDebug(const int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;
#endif

    private:
        struct Weight
        {
            alignas(64) int8_t thirdW[SECOND_OUT] = {0};
            alignas(64) int8_t finalW[8] = {0}; // For this one only the first 4 elements are weights, the rest are left as zero so that the forward pass works fine
            int16_t secondBias[FIRST_OUT] = {0};
            int16_t thirdBias[4] = {0};
            int16_t finalBias = {0};
        };
        Weight weights;

#ifndef NDEBUG
        int16_t forwardPassScalar(const int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;
#endif
    };
} // Namespace NNUEU

int16_t *load_int16_array(const std::string &file_path, size_t cols);
