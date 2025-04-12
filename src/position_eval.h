#ifndef POSITION_EVAL_H
#define POSITION_EVAL_H

#include <cstdint>
#include <vector>

class BitPosition;
namespace NNUEU
{

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
        bool isEmpty() const;
        bool isKingCaptureOnly() const;
    };

    // AccumulatorState structure: holds the NNUEU accumulators for one node.
    struct AccumulatorState
    {
        int16_t inputTurn[2][8]; // [0] white, [1] black NNUEU input arrays.
        bool computed;           // True if the state is fully updated.
        NNUEUChange changes;     // The incremental change that led to this state.
        void newAcc(const NNUEUChange &chngs)
        {
            changes = chngs;
            computed = false;
        }
    };

    // AccumulatorStack class: manages a vector of AccumulatorState nodes.
    class AccumulatorStack
    {
    private:
        std::vector<AccumulatorState> stack;

        int nnueu_king_positions[2]; // For each color, store the last king positions.
    public:
        size_t m_current_idx;
        AccumulatorStack() : m_current_idx(0) 
        {
            stack.resize(128);
        }
        // Reset the stack to a new root position.
        void reset(const BitPosition &rootPos);
        void reset(const BitPosition &rootPos, size_t current_idx);
        // Change the stored king positions.
        void changeWhiteKingPosition(int kingPos);
        void changeBlackKingPosition(int kingPos);
        int getNNUEUKingPosition(int color) const;
        // Push a new AccumulatorState with incremental change info.
        void push(const NNUEUChange &chngs);
        // Pop the top node (when unmaking a move).
        void pop();
        // Return a const reference to the current top state.
        AccumulatorState &top();
        // From the top down, find the last node that is fully computed.
        int findLastComputedNode() const;
        // Forward-update the stack from a given node index to the top.
        void forward_update_incremental(const std::size_t begin);


    private:
        // Apply incremental changes from a previous state to a current state.
        void applyIncrementalChanges(AccumulatorState &curr, const AccumulatorState &prev);
    };

    // Declare a global accumulator stack (for single-threaded use)
    extern AccumulatorStack globalAccumulatorStack;

    // NNUEU Input and update functions.
    // These functions initialize the NNUE accumulators and update them.
    void initializeNNUEInput(const BitPosition &position, AccumulatorState &accumulatorState);
    void addAndRemoveOnInput(AccumulatorState &st, int subIndexAdd, int subIndexRemove);
    void addOnInput(AccumulatorState &st, int subIndex);
    void removeOnInput(AccumulatorState &st, int subIndex);
    void moveWhiteKingNNUEInput(int kingPos);
    void moveBlackKingNNUEInput(int kingPos);

    // NNUEU model parameter initialization.
    void initNNUEParameters();

    // Evaluation function.
    int16_t evaluationFunction(BitPosition &position, bool ourTurn);
}

#endif // POSITION_EVAL_H
