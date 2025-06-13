#ifndef ACCUMULATION_H
#define ACCUMULATION_H

#include <cstdint>
#include <vector>
#include <cassert>

class BitPosition;
namespace NNUEU
{
    class Transformer;
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
        int16_t inputTurn[2][8]; // [0] white, [1] black NNUEU input arrays.
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
    
    static constexpr int F_MAP = 640;
    static constexpr int FIRST_OUT = 8;
    static constexpr int SECOND_OUT = 8 * 4;

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

            // For accumulating non-king moves
            alignas(64) int16_t firstW[F_MAP][FIRST_OUT] = {0}; // removeOnInput()
            alignas(64) int16_t firstWInv[F_MAP][FIRST_OUT] = {0}; // removeOnInput()
            alignas(64) int16_t firstW2Indices[F_MAP][F_MAP][FIRST_OUT] = {0}; // addAndRemoveOnInput()
            alignas(64) int16_t firstW2IndicesInv[F_MAP][F_MAP][FIRST_OUT] = {0}; // addAndRemoveOnInput()

            // For king moves
            alignas(64) int8_t second1[64][SECOND_OUT] = {0};
            alignas(64) int8_t second2[64][SECOND_OUT] = {0};
        };
        Weights weights;
    };
} //Namespace NNUEU


#endif // ACCUMULATION_H
