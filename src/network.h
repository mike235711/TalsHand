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

        int16_t forwardPass(int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12
#if NNUEU_PSQT_L3
                            , int8_t psqtLane   // the shared int8 lane value for l1[HEAD_CONCAT..+7]
#endif
#if NNUEU_THIRD_PHASE
                            , int thirdBucket   // which of the 8 thirdW/thirdBias stacks to dot against
#endif
                            ) const;
        int32_t materialTerm(const BitPosition &position, const Transformer &transformer) const;
        int32_t psqtTerm(const BitPosition &position) const;
        // stm-signed (wpsqt - bpsqt) at the OUTPUT (x4096) scale, UNHALVED: psqtTerm == this/2.
        // psqt_l3 computes it once per evaluate() so the output term and the 3rd-layer lane agree.
        int32_t psqtRaw(const BitPosition &position) const;
        // SF's PSQT phase bucket: (n_pieces - 1) / 4 clamped to 0..7, n_pieces counting BOTH
        // kings. psqtRaw() and third_phase's stack selection MUST use this exact same bucket (a
        // second, independently-written formula is how a bucket-selection bug hides), so both
        // call this one function instead of each computing it inline.
        int phaseBucket(const BitPosition &position) const;
        void applyKingBias(int16_t *acc, int kTurn, int kNotTurn) const;

#ifndef NDEBUG
        int16_t forwardPassDebug(const int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12
#if NNUEU_PSQT_L3
                                 , int8_t psqtLane
#endif
#if NNUEU_THIRD_PHASE
                                 , int thirdBucket
#endif
                                 ) const;
#endif

    private:
        struct Weight
        {
            // Head: 2nd layer (acc -> HEAD_CONCAT) -> 3rd layer (THIRD_IN -> THIRD_OUT_W) -> final (-> 1).
            // Rows live at THIRD_STRIDE (== HEAD_CONCAT unless PSQT_L3 pads 72 -> 80; pad cols stay 0).
            // THIRD_STACKS is 8 under THIRD_PHASE (one phase-bucketed weight set, bucket-major: all
            // of stack 0's THIRD_OUT_W rows, then stack 1's, ...) and 1 otherwise.
            alignas(64) int8_t thirdW[THIRD_STACKS * THIRD_OUT_W * THIRD_STRIDE] = {0};
            alignas(64) int8_t finalW[THIRD_OUT_W + 8] = {0}; // +8 pad so NEON 8-byte loads never over-read
            int16_t secondBias[HEAD_CONCAT] = {0};
            int16_t thirdBias[THIRD_STACKS * THIRD_OUT_W] = {0};
            int16_t finalBias = {0};
            // Material-residual arms: dp[bucket][piece] pre-scaled by the engine's output
            // scale (4096), so adding it to the eval is exact integer work. hasMaterial stays
            // false for every ordinary net, which keeps their eval bit-identical to before.
            static constexpr int MAT_BUCKETS = 7;
            int32_t materialDp[MAT_BUCKETS][5] = {{0}};
            int matBucketCount = 0;          // 1 = fixed table, 7 = balance-bucketed
            bool hasMaterial = false;
            // king_bias: two 64 x FIRST_OUT tables added to the accumulator pre-activation.
            int16_t kingEmbTurn[64][FIRST_OUT] = {{0}};
            int16_t kingEmbNotTurn[64][FIRST_OUT] = {{0}};
            bool hasKingBias = false;
            // psqt_sf: 8 extra FT outputs, phase-bucketed, added straight to the eval.
            int32_t psqtW[8][F_MAP] = {{0}};
            bool hasPsqt = false;
        };
        Weight weights;

#ifndef NDEBUG
        int16_t forwardPassScalar(const int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12) const;
#endif
    };
} // Namespace NNUEU

int16_t *load_int16_array(const std::string &file_path, size_t cols);
