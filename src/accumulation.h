#ifndef ACCUMULATION_H
#define ACCUMULATION_H

#include <cstdint>
#include <vector>
#include <cassert>

class BitPosition;
namespace NNUEU
{
    class Transformer;

    // Accumulator width is a build-time switch. The default build is the
    // width-32 net (w32_wdl0, v0.3.8 baseline); width-8 builds pass
    // -DNNUEU_FIRST_OUT=8 (CMake: -DNNUEU_FIRST_OUT=8). Only 8 and 32 are
    // supported by the forward pass (see network.cpp).
#ifndef NNUEU_FIRST_OUT
#define NNUEU_FIRST_OUT 32
#endif
    // Head widths: 2nd-layer output (per perspective) and 3rd-layer output. Default 4/4 =
    // the w8/w32 nets; the N512 net uses 32/32 (-DNNUEU_SECOND_OUT=32 -DNNUEU_THIRD_OUT=32).
#ifndef NNUEU_SECOND_OUT
#define NNUEU_SECOND_OUT 4
#endif
#ifndef NNUEU_THIRD_OUT
#define NNUEU_THIRD_OUT 4
#endif
    // Dual activation (Stockfish trick): each 2nd-layer projection emits CReLU + SqrCReLU, so the
    // head sees 4*SECOND_OUT instead of 2. The "_sq" cloud nets use this; default off (single CReLU).
#ifndef NNUEU_DUAL_ACT
#define NNUEU_DUAL_ACT 0
#endif
    static_assert(NNUEU_FIRST_OUT == 8 || NNUEU_FIRST_OUT == 32
                      || (NNUEU_FIRST_OUT >= 256 && NNUEU_FIRST_OUT % 16 == 0),
                  "NNUEU_FIRST_OUT must be 8, 32, or a wide width >= 256 and divisible by 16");
    static constexpr int F_MAP = 640;
    static constexpr int FIRST_OUT = NNUEU_FIRST_OUT;
    static constexpr int SECOND_OUT_W = NNUEU_SECOND_OUT;       // 2nd-layer output width per perspective
    static constexpr int THIRD_OUT_W = NNUEU_THIRD_OUT;         // 3rd-layer output width
    static constexpr bool DUAL_ACT = (NNUEU_DUAL_ACT != 0);
    static constexpr int HEAD_CONCAT = (DUAL_ACT ? 4 : 2) * SECOND_OUT_W; // (CReLU[+SqrCReLU]) per perspective
    static constexpr int SECOND_OUT = FIRST_OUT * SECOND_OUT_W; // bucketed 2nd-layer weights per king square
    // NNUEUChange structure: holds the incremental change info
    struct NNUEUChange
    {
        bool is_capture;
        int indices[3];

        // Default constructor initializing members. It is used to detect empty changes due to only moving the king.
        NNUEUChange() : is_capture(false)
        {
            indices[0] = 0;  
            indices[1] = 0;  
            indices[2] = -1;
        }
        // Overload for normal (non-capture) move: two indices.
        void add(int idx0, int idx1);
        // Overload for capture move: three indices.
        void add(int idx0, int idx1, int idx2);
        // For adding the last index in a multi-step update.
        void addlast(int idx2);
        bool isKingMove() const;
        bool isCapture() const;
    };

    // AccumulatorState structure: holds the NNUEU accumulators for one node.
    struct AccumulatorState
    {
        int16_t inputTurn[2][FIRST_OUT]; // [0] white, [1] black NNUEU input arrays.
        bool computed[2];           // True if the state is fully updated for whites/blacks perspective.
        NNUEUChange changes;     // The incremental change that led to this state.
        void newAcc(const NNUEUChange &chngs)
        {
            changes = chngs;
            computed[0] = false;
            computed[1] = false;
        }
        void initialize(const BitPosition &position, const Transformer &transformer);
        inline void substract_8_int16(int16_t *a, const int16_t *b);
        inline void add_8_int16(int16_t *a, const int16_t *b);
        void addOnInput(int subIndex, bool turn, const Transformer &transformer);
        void removeOnInput(int subIndex, bool turn, const Transformer &transformer);
        void addAndRemoveOnInput(int subIndexAdd, int subIndexRemove, bool turn, const Transformer &transformer);
    };

    // AccumulatorStack class: manages a vector of AccumulatorState nodes.
    class AccumulatorStack
    {
    private:
        std::vector<AccumulatorState> stack;
        size_t m_current_idx;
        int nnueu_king_positions[2]; // For each color, store the last king positions.

    public:
        static const int8_t *secondLayer1WeightsBlockWhiteTurn;
        static const int8_t *secondLayer2WeightsBlockWhiteTurn;
        static const int8_t *secondLayer1WeightsBlockBlackTurn;
        static const int8_t *secondLayer2WeightsBlockBlackTurn;

        AccumulatorStack() : m_current_idx(0) 
        {
            stack.resize(128);
        }
        // Reset a stack with a new position
        void reset(const BitPosition &rootPos, const Transformer &transformer);
        // Change the stored king positions.
        void changeWhiteKingPosition(int kingPos, const Transformer &transformer);
        void changeBlackKingPosition(int kingPos, const Transformer &transformer);

        inline int getStackKingPosition(int color) const { return nnueu_king_positions[color]; };
        // Push a new AccumulatorState with incremental change info.
        void push(const NNUEUChange &chngs);
        // Pop the top node (when unmaking a move).
        void pop();
        // Return a const reference to the current top state.
        AccumulatorState &top();
        // From the top down, find the last node that is fully computed.
        int findLastComputedNode(bool turn) const;
        // Forward-update the stack from a given node index to the top.
        void forward_update_incremental(const int begin, bool turn, const Transformer &transformer);

#ifndef NDEBUG
        /** Re-compute the accumulator from scratch and compare with the
            incrementally-updated one.  Implemented in accumulation.cpp. */
        void verifyTopAgainstFresh(const BitPosition &pos, bool turn, const Transformer &transformer);
#endif

    private:
        // Apply incremental changes from a previous state to a current state.
        void applyIncrementalChanges(AccumulatorState &curr, const AccumulatorState &prev, bool turn, const Transformer &transformer);
    };
    
    // Transformer class contains the weights necessary to update accumulators (first and second layers)
    class Transformer
    {
    public:
        // An *empty* transformer is still default-constructible – useful for unit tests
        Transformer() = default;
        explicit Transformer(const char *def) { load(def); }

        bool load(const std::string &dir);

        struct Weights
        {
            // For initializing accumulators
            int16_t firstBias[FIRST_OUT] = {0};

            // For accumulating moves: add/removeOnInput use firstW/firstWInv directly.
            // (The old fused firstW2Indices[F_MAP][F_MAP][FIRST_OUT] table is gone — it was
            //  quadratic in F_MAP and ~838 MB at width 512; addAndRemove now does add+remove.)
            alignas(64) int16_t firstW[F_MAP][FIRST_OUT] = {0};
            alignas(64) int16_t firstWInv[F_MAP][FIRST_OUT] = {0};

            // For king moves
            alignas(64) int8_t second1[64][SECOND_OUT] = {0};
            alignas(64) int8_t second2[64][SECOND_OUT] = {0};
        };
        Weights weights;
    };
} //Namespace NNUEU


#endif // ACCUMULATION_H
