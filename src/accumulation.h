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
#ifndef NNUEU_SPLIT_FT
// split-ft: the FT is 2N wide but each 2nd-layer projection reads only its own N-half
// (turn -> low half, not-turn -> high half). Keeps the 2nd layer at N-width compute while
// doubling FT capacity. 0 = classic behaviour, both projections read the whole accumulator.
#define NNUEU_SPLIT_FT 0
#endif
#ifndef NNUEU_DUAL_ACT
#define NNUEU_DUAL_ACT 0
#endif
    static_assert(NNUEU_FIRST_OUT == 8 || NNUEU_FIRST_OUT == 32
                      || (NNUEU_FIRST_OUT >= 256 && NNUEU_FIRST_OUT % 16 == 0),
                  "NNUEU_FIRST_OUT must be 8, 32, or a wide width >= 256 and divisible by 16");
#ifndef NNUEU_F_MAP
#define NNUEU_F_MAP 640
#endif
    // Input feature count. 640 = the king-free set (10 planes: own P,N,B,R,Q then opp P,N,B,R,Q).
    // 768 adds BOTH kings as planes (12), 704 adds only the not-turn king (11). Putting a king in
    // the input does NOT cost a recompute: a king move is one feature removed and one added, the
    // same incremental path every other piece already uses. Only buckets in the FIRST layer would
    // force a recompute, and this architecture has none.
    static constexpr int F_MAP = NNUEU_F_MAP;
    static_assert(F_MAP == 640 || F_MAP == 704 || F_MAP == 768,
                  "NNUEU_F_MAP must be 640 (king-free), 704 (not-turn king) or 768 (both kings)");
    static constexpr int N_PLANES = F_MAP / 64;
    static constexpr bool KINGS_IN = (F_MAP == 768);        // both kings are input planes
    static constexpr bool KING_NOTURN_IN = (F_MAP == 704);  // only the not-turn king is

    // Own/opponent mirror, used when building the opposite-perspective weight table.
    //   10 planes -> swap the two 5-plane halves
    //   12 planes -> swap the two 6-plane halves (kings included)
    //   11 planes -> swap the 5-plane halves; the lone king plane maps to ITSELF, because from the
    //                other perspective it still denotes "the king of the side not to move".
    // Applying the 10-plane rule at 12 planes is silent corruption rather than a crash: (p+5)%10
    // sends plane 10 to 5 and 11 to 6, overwriting rows already written, and leaves the king rows
    // zero. newCol stays below 640 < F_MAP, so nothing traps.
    static constexpr int mirrorPlane(int p)
    {
        // The 10 piece planes keep the king-free layout EXACTLY as it was, so every existing
        // feature index and every NNUE_BASE entry stays valid and the 640 build is untouched.
        // King planes are APPENDED (10 = own king, 11 = opponent king), which is why they mirror
        // separately instead of falling out of one modulus.
        if (p < 10)
            return (p + 5) % 10;
        return KINGS_IN ? 21 - p   // own king (10) <-> opponent king (11)
                        : p;       // 704: the single plane means "king of the side not to move"
    }                              //      in BOTH perspectives, so it maps to itself

    // Base feature indices of the appended king planes.
    static constexpr int KING_OWN_BASE = 640;   // 768: own king;  704: the not-to-move king
    static constexpr int KING_OPP_BASE = 704;   // 768 only
    static constexpr int FIRST_OUT = NNUEU_FIRST_OUT;
    static constexpr int SECOND_OUT_W = NNUEU_SECOND_OUT;       // 2nd-layer output width per perspective
    static constexpr int THIRD_OUT_W = NNUEU_THIRD_OUT;         // 3rd-layer output width
    static constexpr bool SPLIT_FT = (NNUEU_SPLIT_FT != 0);
    // how many accumulator lanes ONE projection reads (and the per-neuron weight stride)
    static constexpr int SPLIT_READ = SPLIT_FT ? (FIRST_OUT / 2) : FIRST_OUT;
    static_assert(!SPLIT_FT || (FIRST_OUT % 32 == 0),
                  "SPLIT_FT needs FIRST_OUT divisible by 32 so each half stays NEON-aligned");
    static constexpr bool DUAL_ACT = (NNUEU_DUAL_ACT != 0);
    static constexpr int HEAD_CONCAT = (DUAL_ACT ? 4 : 2) * SECOND_OUT_W; // (CReLU[+SqrCReLU]) per perspective
    // bucketed 2nd-layer weights per king square. With SPLIT_FT each neuron only spans its
    // half, so the stored block halves too -- this is what makes the export unpadded.
    static constexpr int SECOND_OUT = SPLIT_READ * SECOND_OUT_W;

    // Single source of truth for the net a build loads by default: the CSV shapes in a model
    // directory are fixed by (FIRST_OUT, SECOND_OUT_W, THIRD_OUT_W, DUAL_ACT), so the width
    // switch *is* the net switch. engine.cpp uses this as the default EvalFile; do not repeat
    // the mapping anywhere else. Which widths each released tag was built with (and therefore
    // which net it loads) is recorded in scripts/nnueu_versions.json — keep the two in sync.
    // A width with no released net (the NNUEU sweep candidates) still builds; it is expected to
    // pick its weights at startup with NNUEU_NET=<dir> (see engine.cpp).
    static constexpr const char *DefaultNetDir =
        (FIRST_OUT == 256)   ? "models/n256_h16x16_sq/"  // v0.4.3+: dual-act N256, head 16/16
        : (FIRST_OUT == 512) ? "models/n512_h32/"        // v0.4.0-v0.4.2: N512, head 32/32
        : (FIRST_OUT == 32)  ? "models/w32_wdl0/"        // v0.3.8-v0.3.18: w32, head 4/4
                             : "models/NNUEU_quantized_model_v4_param_350_epoch_10/"; // <= v0.3.7: width 8, head 4/4
    // NNUEUChange structure: holds the incremental change info
    struct NNUEUChange
    {
        // A move displaces at most TWO features that each need an (add, remove) pair. Normally
        // there is one (the moving piece), but a CASTLING move at F_MAP 704/768 moves the king
        // AND the rook and both are input features. The old record held a single pair, so the
        // two calls overwrote each other and the accumulator drifted silently away from the true
        // position -- no crash, just a wrong eval. Hence a small fixed array plus a count.
        static constexpr int MAX_PAIRS = 2;

        // int16_t/uint8_t, not int: this record is copied into an AccumulatorState at every
        // pushed node (eval is 83-91% of node cost, so the hot path is not the place to grow a
        // struct). A feature index is bounded by F_MAP <= 768, so it fits with room to spare and
        // the whole record is 12 bytes -- SMALLER than the 16-byte one it replaces.
        int16_t added[MAX_PAIRS];
        int16_t removed[MAX_PAIRS];
        int16_t capturedIdx;   // feature of the captured piece; only read when is_capture
        uint8_t n_pairs;       // how many of added[]/removed[] are live
        bool is_capture;

        // Default = "this move changes no input feature". That is a real case, not just an
        // initial value: a null move never changes the board, and at F_MAP 640 the king is not
        // an input feature, so a plain king move leaves the accumulator alone. It used to be
        // spelled `indices[0] == indices[1]` (isKingMove()); at 704/768 a king move DOES change
        // the input, so the condition has to be the pair COUNT, not a king-move test.
        NNUEUChange() : capturedIdx(-1), n_pairs(0), is_capture(false)
        {
            added[0] = removed[0] = 0;
            added[1] = removed[1] = 0;
        }
        // Record the moving piece's (add, remove) pair. REPLACES pair 0: a promotion records the
        // pawn's move first and then overwrites it with (promoted piece at destination, pawn at
        // origin), so appending here would apply the pawn move twice.
        void add(int idx0, int idx1);
        // APPEND an extra pair. Used only by castling for the rook, which is a second displaced
        // feature rather than a correction of the first. It needs no #if at the call site: at 768
        // the king already filled pair 0, at 640 nothing did and the rook lands in pair 0 -- the
        // exact record the king-free build produced before.
        void addPair(int idx0, int idx1);
        // Record the captured piece's feature (it is removed, never re-added).
        void addlast(int idx2);
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
