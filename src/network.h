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

        int16_t fullNnueuPass(int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;

#ifndef NDEBUG
        int16_t fullNnueuPassDebug(const int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;
#endif

    private:
        struct Weights
        {
            alignas(64) int8_t thirdW[SECOND_OUT]{};
            alignas(64) int8_t finalW[4]{};
            int16_t secondBias[FIRST_OUT]{};
            int16_t thirdBias[4]{};
            int16_t finalBias{};
        };
        Weights weights;

#ifndef NDEBUG
        int16_t fullNnueuPassScalar(const int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;
#endif
    };
} // Namespace NNUEU

int16_t *load_int16_array(const std::string &file_path, size_t cols);
