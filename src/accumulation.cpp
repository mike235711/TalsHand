#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <cassert>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifdef __SSE4_1__
#include <smmintrin.h> // SSE4.1
#endif

#include "bitposition.h"
#include "bit_utils.h" // Bit utility functions
#include "precomputed_moves.h"
#include "accumulation.h"
#include "network.h"

// Shape check shared by the three 2D loaders below.
//
// Every one of them was already BOUNDED (`row < ...`, `col < ...`), so none could overflow --
// but a bound is not a check: a file with the wrong shape was silently truncated or silently
// left the tail zero, and the engine went on to play with a scrambled net. That is the exact
// failure the famE naming trap produces (the token "N256" means accumulator 512 in an arm name
// and accumulator 256 in an engine build name): pairing an N256 net with an N512 build loaded
// with exit 0 and empty stderr, and moved the start position's eval from -26250 to -9426.
// Report the shape instead, so a mismatch is a refused load rather than a worse engine.
namespace
{
struct Shape2D
{
    size_t rows = 0;
    size_t cols = 0;   // column count of the FIRST non-blank row
    bool ragged = false;
    bool extra_rows = false;
    bool extra_cols = false;
};

bool check_2d_shape(const std::string &file_path, const Shape2D &s,
                    size_t want_rows, size_t want_cols)
{
    if (s.rows == want_rows && s.cols == want_cols && !s.ragged && !s.extra_rows && !s.extra_cols)
        return true;
    std::cerr << file_path << ": expected " << want_rows << " rows x " << want_cols
              << " columns, got " << (s.extra_rows ? ">" : "") << s.rows << " x "
              << (s.extra_cols ? ">" : "") << s.cols << (s.ragged ? " (ragged)" : "")
              << " -- this net does not match this build's geometry, refusing to load\n";
    return false;
}

// Row/column walker shared by the three loaders. `store(row, col, value)` does the layout-
// specific write; everything else -- blank-line skipping, the bounds that keep a wrong-shaped
// file from writing past the array, and the shape bookkeeping -- is identical between them.
template <typename Store>
Shape2D read_2d(std::ifstream &file, size_t want_rows, size_t want_cols, Store store)
{
    Shape2D s;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos)
            continue; // blank line / trailing newline
        if (s.rows >= want_rows)
        {
            s.extra_rows = true;
            break;
        }
        std::stringstream ss(line);
        std::string value;
        size_t col = 0;
        while (std::getline(ss, value, ','))
        {
            if (value.find_first_not_of(" \t\r\n") == std::string::npos)
                continue;
            if (col >= want_cols)
            {
                s.extra_cols = true;
                break;
            }
            store(s.rows, col, static_cast<int>(std::stoi(value)));
            col++;
        }
        if (s.rows == 0)
            s.cols = col;
        else if (col != s.cols)
            s.ragged = true;
        s.rows++;
    }
    return s;
}
} // namespace

// Function to load a 2D int8_t array from a file
bool load_int8_2D_array1(const std::string &file_path, int8_t weights[64][NNUEU::SECOND_OUT])
{
    std::ifstream file(file_path);
    // Block stride is SPLIT_READ, not FIRST_OUT: under SPLIT_FT each neuron's weights span only
    // the half of the accumulator its projection reads, so the CSV rows are half as long and the
    // per-king blocks are half as big. Using FIRST_OUT here silently mis-slices the file (and
    // overruns SECOND_OUT) -- it filled the 2nd layer with garbage while the engine ran fine.
    constexpr size_t kStride = static_cast<size_t>(NNUEU::SPLIT_READ);
    const size_t want_rows = static_cast<size_t>(NNUEU::SECOND_OUT_W);
    const size_t want_cols = 64 * kStride;
    const Shape2D s = read_2d(file, want_rows, want_cols, [&](size_t row, size_t col, int v) {
        weights[col / kStride][(col % kStride) + row * kStride] = static_cast<int8_t>(v);
    });
    return check_2d_shape(file_path, s, want_rows, want_cols);
}
bool load_int16_2D_array1(const std::string &file_path, int16_t weights[NNUEU::FT_ROWS][NNUEU::FIRST_OUT])
{
    std::ifstream file(file_path);
    const size_t want_rows = static_cast<size_t>(NNUEU::FIRST_OUT);
    // FT_ROWS == F_MAP unless FT_PHASE, where the file carries 8 bucket-major copies.
    const size_t want_cols = static_cast<size_t>(NNUEU::FT_ROWS);
    const Shape2D s = read_2d(file, want_rows, want_cols, [&](size_t row, size_t col, int v) {
        weights[col][row] = static_cast<int16_t>(v);
    });
    return check_2d_shape(file_path, s, want_rows, want_cols);
}

bool load_inverted_int16_2D_array1(const std::string &file_path, int16_t weights[NNUEU::FT_ROWS][NNUEU::FIRST_OUT])
{
    std::ifstream file(file_path);
    const size_t want_rows = static_cast<size_t>(NNUEU::FIRST_OUT);
    const size_t want_cols = static_cast<size_t>(NNUEU::FT_ROWS);
    const Shape2D s = read_2d(file, want_rows, want_cols, [&](size_t row, size_t col, int v) {
        // THE MIRROR IS PER BUCKET. Under FT_PHASE the file is 8 stacked copies of the feature
        // space; permuting planes over the whole tensor would mirror bucket 0's planes into
        // bucket 1's rows. Every shape would still check out and 7 of the 8 buckets would
        // evaluate a scrambled board -- the same trap export_nnueu.py hit on the way out.
        const int c = static_cast<int>(col);
        const int bucket = c / NNUEU::F_MAP;
        const int feature = c % NNUEU::F_MAP;
        const int pieceType = feature / 64;
        const int square = feature % 64;
        // Compute the new column after inverting color and square
        const int newPieceType = NNUEU::mirrorPlane(pieceType);
        const int newCol = NNUEU::ftRow(bucket, newPieceType * 64 + invertIndex(square));
        weights[newCol][row] = static_cast<int16_t>(v);
    });
    return check_2d_shape(file_path, s, want_rows, want_cols);
}
    // Initialize Accumulators
void NNUEU::AccumulatorState::initialize(const BitPosition &position, const Transformer &transformer)
{
    // Piece count (BOTH kings included) and, under FT_PHASE, the bucket it selects. The count is
    // maintained incrementally from here on -- only a capture changes it.
    pieceCount = 0;
    for (int c = 0; c < 2; ++c)
        for (int t = 0; t < 6; ++t)
            pieceCount += countBits(position.getPieces(c, t));
    const int bkt = FT_PHASE ? phaseBucketOf(pieceCount) : 0;
    feats.clearAll();

    // Start accumulators with the firstLayerBiases (shared across buckets, as SF's is)
    std::memcpy(inputTurn[0], transformer.weights.firstBias, sizeof(transformer.weights.firstBias));
    std::memcpy(inputTurn[1], transformer.weights.firstBias, sizeof(transformer.weights.firstBias));

    // White pawns
    for (unsigned short index : getBitIndices(position.getPieces(0, 0)))
    {
        feats.set(index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, index)]);
    }
    // White knights
    for (unsigned short index : getBitIndices(position.getPieces(0, 1)))
    {
        feats.set(64 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 + index)]);
    }
    // White bishops
    for (unsigned short index : getBitIndices(position.getPieces(0, 2)))
    {
        feats.set(64 * 2 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 * 2 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 * 2 + index)]);
    }
    // White rooks
    for (unsigned short index : getBitIndices(position.getPieces(0, 3)))
    {
        feats.set(64 * 3 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 * 3 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 * 3 + index)]);
    }
    // White queens
    for (unsigned short index : getBitIndices(position.getPieces(0, 4)))
    {
        feats.set(64 * 4 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 * 4 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 * 4 + index)]);
    }

    // Black pawns
    for (unsigned short index : getBitIndices(position.getPieces(1, 0)))
    {
        feats.set(64 * 5 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 * 5 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 * 5 + index)]);
    }
    // Black knights
    for (unsigned short index : getBitIndices(position.getPieces(1, 1)))
    {
        feats.set(64 * 6 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 * 6 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 * 6 + index)]);
    }
    // Black bishops
    for (unsigned short index : getBitIndices(position.getPieces(1, 2)))
    {
        feats.set(64 * 7 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 * 7 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 * 7 + index)]);
    }
    // Black rooks
    for (unsigned short index : getBitIndices(position.getPieces(1, 3)))
    {
        feats.set(64 * 8 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 * 8 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 * 8 + index)]);
    }
    // Black queens
    for (unsigned short index : getBitIndices(position.getPieces(1, 4)))
    {
        feats.set(64 * 9 + index); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, 64 * 9 + index)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, 64 * 9 + index)]);
    }
    // King planes (F_MAP 704/768). Compile-time guarded, so the king-free build emits nothing.
    // firstW is the white-perspective table and firstWInv the black one; mirrorPlane() already
    // decided which appended plane each maps to, so both perspectives are just an add here.
    if constexpr (NNUEU::KINGS_IN)
    {
        const int wk = position.getKingPosition(0);
        const int bk = position.getKingPosition(1);
        feats.set(NNUEU::KING_OWN_BASE + wk); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, NNUEU::KING_OWN_BASE + wk)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, NNUEU::KING_OWN_BASE + wk)]);
        feats.set(NNUEU::KING_OPP_BASE + bk); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, NNUEU::KING_OPP_BASE + bk)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, NNUEU::KING_OPP_BASE + bk)]);
    }
    else if constexpr (NNUEU::KING_NOTURN_IN)
    {
        // Only the king of the side NOT to move is a feature. Which physical king that is depends
        // on whose turn it is, and each perspective sees the other one -- hence the two indices.
        const int notTurnKing = position.getKingPosition(position.getTurn() ? 1 : 0);
        const int turnKing = position.getKingPosition(position.getTurn() ? 0 : 1);
        feats.set(NNUEU::KING_OWN_BASE + notTurnKing); add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, NNUEU::KING_OWN_BASE + notTurnKing)]);
        add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, NNUEU::KING_OWN_BASE + turnKing)]);
    }

    computed[0] = true;
    computed[1] = true;
}

    // Functions to add/remove features from the accumulators
    inline void NNUEU::AccumulatorState::addAndRemoveOnInput(int subIndexAdd, int subIndexRemove, bool turn, int bkt, const Transformer &transformer)
    {
        assert(subIndexAdd >= 0 && subIndexAdd < NNUEU::F_MAP && subIndexRemove >= 0 && subIndexRemove < NNUEU::F_MAP);
        // Equivalent to the old fused firstW2Indices[add][remove] = firstW[add] - firstW[remove],
        // done as two passes so no quadratic table is needed (essential at width 512).
        if (not turn)
        {
            add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, subIndexAdd)]);
            substract_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, subIndexRemove)]);
        }
        else
        {
            add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, subIndexAdd)]);
            substract_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, subIndexRemove)]);
        }
    }

    inline void NNUEU::AccumulatorState::add_8_int16(int16_t *a, const int16_t *b)
    {
#if defined(__ARM_NEON)
        for (int k = 0; k < FIRST_OUT; k += 8)
            vst1q_s16(a + k, vaddq_s16(vld1q_s16(a + k), vld1q_s16(b + k)));
#elif defined(__AVX2__) || defined(__SSE2__) || defined(__SSE4_1__)
        __m128i v1 = _mm_loadu_si128((__m128i *)a);
        __m128i v2 = _mm_loadu_si128((__m128i *)b);
        __m128i sum = _mm_add_epi16(v1, v2);
        _mm_storeu_si128((__m128i *)a, sum);
#else
        // Fallback scalar code
        for (int i = 0; i < FIRST_OUT; i++)
            a[i] += b[i];
#endif
    }

    inline void NNUEU::AccumulatorState::substract_8_int16(int16_t *a, const int16_t *b) // For NNUE accumulation
    {
#if defined(__ARM_NEON)
        for (int k = 0; k < FIRST_OUT; k += 8)
            vst1q_s16(a + k, vsubq_s16(vld1q_s16(a + k), vld1q_s16(b + k)));

#elif defined(__AVX2__) || defined(__SSE2__) || defined(__SSE4_1__)
        __m128i v1 = _mm_loadu_si128((const __m128i *)a);
        __m128i v2 = _mm_loadu_si128((const __m128i *)b);
        __m128i v_sub = _mm_sub_epi16(v1, v2);
        _mm_storeu_si128((__m128i *)a, v_sub);
#else
        // Fallback scalar code
        for (int i = 0; i < FIRST_OUT; i++)
            a[i] -= b[i];

#endif
    }

    inline void NNUEU::AccumulatorState::addOnInput(int subIndex, bool turn, int bkt, const Transformer &transformer)
    {
        assert(subIndex >= 0 && subIndex < NNUEU::F_MAP);
        if (not turn)
            add_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, subIndex)]);
        else
            add_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, subIndex)]);
    }

    inline void NNUEU::AccumulatorState::removeOnInput(int subIndex, bool turn, int bkt, const Transformer &transformer)
    {
        assert(subIndex >= 0 && subIndex < NNUEU::F_MAP);
        if (not turn)
            substract_8_int16(inputTurn[0], transformer.weights.firstW[ftRow(bkt, subIndex)]);
        else
            substract_8_int16(inputTurn[1], transformer.weights.firstWInv[ftRow(bkt, subIndex)]);
    }

    // The moving piece's pair. Writes slot 0 in place -- see the header for why this REPLACES
    // (promotions) while castling's rook APPENDS.
    void NNUEU::NNUEUChange::add(int idx0, int idx1)
    {
        assert(idx0 >= 0 && idx0 < NNUEU::F_MAP);
        assert(idx1 >= 0 && idx1 < NNUEU::F_MAP);
        is_capture = false; // No capture
        added[0] = static_cast<int16_t>(idx0);
        removed[0] = static_cast<int16_t>(idx1);
        if (n_pairs == 0)
            n_pairs = 1;
    }

    // The castling rook. Appends, so it never clobbers the king pair recorded at F_MAP 768.
    void NNUEU::NNUEUChange::addPair(int idx0, int idx1)
    {
        assert(idx0 >= 0 && idx0 < NNUEU::F_MAP);
        assert(idx1 >= 0 && idx1 < NNUEU::F_MAP);
        assert(n_pairs < MAX_PAIRS);
        added[n_pairs] = static_cast<int16_t>(idx0);
        removed[n_pairs] = static_cast<int16_t>(idx1);
        ++n_pairs;
    }

    void NNUEU::NNUEUChange::addlast(int idx2)
    {
        assert(idx2 >= 0 && idx2 < NNUEU::F_MAP);
        is_capture = true; // It's a capture
        capturedIdx = static_cast<int16_t>(idx2);
    }
    inline bool NNUEU::NNUEUChange::isCapture() const
    {
        // King captured something, but didn't affect NNUEU input for the king
        return is_capture;
    }

#ifndef NDEBUG
    void NNUEU::AccumulatorStack::verifyTopAgainstFresh(const BitPosition &pos, bool turn, const Transformer &transformer)
    {
        // Build a *fresh* accumulator for reference
        AccumulatorState fresh;
        fresh.initialize(pos, transformer);

        // Compare with the incrementally-updated top of the stack
        const AccumulatorState &inc = top();

        // ALL lanes, not the first 8. At width 512 an 8-lane window is 1.6% of the accumulator,
        // and the drift this check exists to catch (a change record that dropped one of a
        // castling move's two add/remove pairs, F_MAP 704/768) shows up in whichever lanes that
        // rook's weight row happens to touch -- it can sit entirely outside the window.
        bool mismatch_found = false;
        for (int i = 0; i < FIRST_OUT; ++i)
        {
            if (fresh.inputTurn[turn][i] != inc.inputTurn[turn][i])
            {
                mismatch_found = true;
                break; // Exit the loop as soon as a mismatch is found
            }
        }

        if (mismatch_found)
        {
            std::cerr << "NNUEU incremental accumulation mismatch detected!" << std::endl;

            std::cerr << "Fresh array: [";
            for (int i = 0; i < 8; ++i)
            {
                std::cerr << fresh.inputTurn[turn][i] << (i == 7 ? "" : ", ");
            }
            std::cerr << "]" << std::endl;

            std::cerr << "Inc (top) array: [";
            for (int i = 0; i < 8; ++i)
            {
                std::cerr << inc.inputTurn[turn][i] << (i == 7 ? "" : ", ");
            }
            std::cerr << "]" << std::endl;
            std::abort(); // Abort program
        }
    }
#endif // NDEBUG

    // Reset to a new root position
    void NNUEU::AccumulatorStack::reset(const BitPosition &rootPos, const Transformer &transformer)
    {
        m_current_idx = 1;
        AccumulatorState &rootState = stack[0];

        // The finny entries belong to the PREVIOUS root's tree. Keeping them would still be
        // correct -- they are only ever a diff base, and the diff is computed against the entry's
        // own feature set -- but it would start a bucket from a position arbitrarily far away.
        if constexpr (FT_PHASE)
            for (int b = 0; b < FT_BUCKETS; ++b)
                for (int t = 0; t < 2; ++t)
                    m_finny[b][t].valid = false;

        // Build a fresh accumulator for the root
        rootState.initialize(rootPos, transformer);

        // Seed the root's own bucket so the first phase change in the search diffs against the
        // root instead of rebuilding from the bias.
        if constexpr (FT_PHASE)
        {
            const int bkt = phaseBucketOf(rootState.pieceCount);
            for (int t = 0; t < 2; ++t)
            {
                FinnyEntry &fe = m_finny[bkt][t];
                std::memcpy(fe.acc, rootState.inputTurn[t], sizeof(int16_t) * FIRST_OUT);
                fe.feats = rootState.feats;
                fe.valid = true;
            }
        }

        const int whiteKing = rootPos.getKingPosition(0);
        const int blackKing = rootPos.getKingPosition(1);

        assert(whiteKing >= 0 && whiteKing < 64);
        assert(blackKing >= 0 && blackKing < 64);

        // Set the king positions
        nnueu_king_positions[0] = rootPos.getKingPosition(0);
        nnueu_king_positions[1] = rootPos.getKingPosition(1);

        secondLayer1WeightsBlockWhiteTurn = transformer.weights.second1[whiteKing];
        secondLayer2WeightsBlockBlackTurn = transformer.weights.second2[invertIndex(whiteKing)];
        secondLayer2WeightsBlockWhiteTurn = transformer.weights.second2[blackKing];
        secondLayer1WeightsBlockBlackTurn = transformer.weights.second1[invertIndex(blackKing)];
    }
    void NNUEU::AccumulatorStack::changeWhiteKingPosition(int kingPos, const Transformer &transformer)
    {
        assert(kingPos >= 0 && kingPos < 64);
        secondLayer1WeightsBlockWhiteTurn = transformer.weights.second1[kingPos];
        secondLayer2WeightsBlockBlackTurn = transformer.weights.second2[invertIndex(kingPos)];
        nnueu_king_positions[0] = kingPos;
    }
    void NNUEU::AccumulatorStack::changeBlackKingPosition(int kingPos, const Transformer &transformer)
    {
        assert(kingPos >= 0 && kingPos < 64);
        secondLayer2WeightsBlockWhiteTurn = transformer.weights.second2[kingPos];
        secondLayer1WeightsBlockBlackTurn = transformer.weights.second1[invertIndex(kingPos)];
        nnueu_king_positions[1] = kingPos;
    }

    void NNUEU::AccumulatorStack::push(const NNUEUChange &chngs)
    {
        assert(m_current_idx < stack.size()); // Ensure space exists
        stack[m_current_idx].newAcc(chngs);
        if constexpr (FT_PHASE)
        {
            // Derived from the PARENT, never from the board: push() has no position, and walking
            // the bitboards here would cost more than the accumulator update it feeds.
            AccumulatorState &node = stack[m_current_idx];
            const AccumulatorState &par = stack[m_current_idx - 1];
            node.pieceCount = par.pieceCount - (chngs.isCapture() ? 1 : 0);
            node.feats = par.feats;
            if (chngs.isCapture() && chngs.capturedIdx >= 0)
                node.feats.clear(chngs.capturedIdx);
            for (unsigned i = 0; i < chngs.n_pairs; ++i)
            {
                node.feats.clear(chngs.removed[i]);
                node.feats.set(chngs.added[i]);
            }
        }
        m_current_idx++;
    }

    // Pop the top state when unmaking a move
    void NNUEU::AccumulatorStack::pop()
    {
        assert(m_current_idx > 1); // Never pop below 1, since root accumulator should be computed
        m_current_idx--;
    }

    NNUEU::AccumulatorState &NNUEU::AccumulatorStack::top()
    {
        assert(m_current_idx - 1 < stack.size());
        assert(stack[m_current_idx - 1].computed[0] || stack[m_current_idx - 1].computed[1]);
        return stack[m_current_idx - 1];
    }
    // From top down to 0, find the first node that has both sides computed
    int NNUEU::AccumulatorStack::findLastComputedNode(bool turn) const
    {
        for (int curr_idx = static_cast<int>(m_current_idx) - 2; curr_idx > 0; curr_idx--)
        {
            if (stack[curr_idx].computed[not turn])
                return curr_idx;
        }
        return 0;
    }
    void NNUEU::AccumulatorStack::forward_update_incremental(const int begin, bool turn, const Transformer &transformer)
    {
        for (int next = begin + 1; next < m_current_idx; next++)
            applyIncrementalChanges(stack[next], stack[next - 1], not turn, transformer);
    }

    // This applies the “NNUEUChange” to the current node
    // Rebuild `node`'s accumulator for `turn` in `bucket`, from the bucket's finny entry when it
    // has one and from the bias when it does not, and then adopt `node` as the entry.
    void NNUEU::AccumulatorStack::refreshFromFinny(AccumulatorState &node, bool turn, int bucket,
                                                   const Transformer &transformer)
    {
        FinnyEntry &fe = m_finny[bucket][turn];
        int16_t *dst = node.inputTurn[turn];

        if (!fe.valid)
        {
            // Cold bucket: from the bias, every active feature. ~30 toggles, paid once per
            // bucket per search.
            std::memcpy(dst, transformer.weights.firstBias, sizeof(int16_t) * FIRST_OUT);
            for (int wi = 0; wi < FeatureSet::WORDS; ++wi)
            {
                uint64_t m = node.feats.w[wi];
                while (m)
                {
                    const int f = (wi << 6) + __builtin_ctzll(m);
                    m &= m - 1;
                    node.addOnInput(f, turn, bucket, transformer);
                }
            }
        }
        else
        {
            // Warm: start from the cached accumulator and apply the symmetric difference. ~4-5
            // toggles in the middlegame, measured.
            std::memcpy(dst, fe.acc, sizeof(int16_t) * FIRST_OUT);
            for (int wi = 0; wi < FeatureSet::WORDS; ++wi)
            {
                const uint64_t cur = node.feats.w[wi];
                const uint64_t sto = fe.feats.w[wi];
                uint64_t add = cur & ~sto;
                uint64_t rem = sto & ~cur;
                while (add)
                {
                    const int f = (wi << 6) + __builtin_ctzll(add);
                    add &= add - 1;
                    node.addOnInput(f, turn, bucket, transformer);
                }
                while (rem)
                {
                    const int f = (wi << 6) + __builtin_ctzll(rem);
                    rem &= rem - 1;
                    node.removeOnInput(f, turn, bucket, transformer);
                }
            }
        }

        std::memcpy(fe.acc, dst, sizeof(int16_t) * FIRST_OUT);
        fe.feats = node.feats;
        fe.valid = true;
    }

    void NNUEU::AccumulatorStack::applyIncrementalChanges(AccumulatorState &curr, const AccumulatorState &prev, bool turn, const Transformer &transformer)
    {
        assert(prev.computed[turn]);

        const NNUEUChange &c = curr.changes;
        // n_pairs is 0 for a null move (and, at F_MAP 640, for a plain king move -- the king is
        // not an input feature there), 1 for essentially every real move, and 2 only for castling
        // once the king IS an input feature. MAX_PAIRS is a compile-time 2, so the trip count is
        // known and the rare second iteration costs one predictable branch.

        if constexpr (FT_PHASE)
        {
            const int bkt = phaseBucketOf(curr.pieceCount);
            const int bktPrev = phaseBucketOf(prev.pieceCount);
            if (bkt != bktPrev)
            {
                // A phase change: the parent's accumulator is a sum over a DIFFERENT weight
                // table, so not one lane of it is reusable. Rebuild through the finny table for
                // the new bucket. This is the entire cost of FT_PHASE and the reason it is a
                // build switch rather than a free win.
                refreshFromFinny(curr, turn, bkt, transformer);
                curr.computed[turn] = true;
                return;
            }
            // Same bucket: the ordinary running-sum edit, indexed into that bucket's rows.
            std::memcpy(curr.inputTurn[turn], prev.inputTurn[turn], sizeof(curr.inputTurn[turn]));
            if (c.isCapture())
                curr.removeOnInput(c.capturedIdx, turn, bkt, transformer);
            for (unsigned i = 0; i < c.n_pairs; ++i)
                curr.addAndRemoveOnInput(c.added[i], c.removed[i], turn, bkt, transformer);

            // Keep the bucket's finny entry fresh. One memcpy on an already-hot line, and it is
            // what keeps the refresh diff small: without it the entry would age to whatever
            // position last CHANGED phase, and the symmetric difference would grow without bound.
            FinnyEntry &fe = m_finny[bkt][turn];
            std::memcpy(fe.acc, curr.inputTurn[turn], sizeof(int16_t) * FIRST_OUT);
            fe.feats = curr.feats;
            fe.valid = true;
            curr.computed[turn] = true;
            return;
        }
        else
        {
            std::memcpy(curr.inputTurn[turn], prev.inputTurn[turn], sizeof(curr.inputTurn[turn]));
            if (c.isCapture())
                curr.removeOnInput(c.capturedIdx, turn, 0, transformer);
            for (unsigned i = 0; i < c.n_pairs; ++i)
                curr.addAndRemoveOnInput(c.added[i], c.removed[i], turn, 0, transformer);
            curr.computed[turn] = true;
        }
    }

    // No default argument here: the declaration in accumulation.h has none (so this one was dead
    // code anyway) and the width -> net mapping has a single home, NNUEU::DefaultNetDir.
    bool NNUEU::Transformer::load(const std::string &modelDir)
    {
        try
        {
            // Load weights into fixed-size arrays. Every loader now VALIDATES the CSV's shape
            // against this build's compile-time geometry and returns false on a mismatch --
            // refusing the net is the point: an N256 net in an N512 build used to load with
            // exit 0, empty stderr and a scrambled king-bucket table.
            bool ok = true;
            ok &= load_int16_2D_array1(modelDir + "first_linear_weights.csv", weights.firstW);
            ok &= load_inverted_int16_2D_array1(modelDir + "first_linear_weights.csv", weights.firstWInv);

            ok &= load_int8_2D_array1(modelDir + "second_layer_turn_weights.csv", weights.second1);
            ok &= load_int8_2D_array1(modelDir + "second_layer_not_turn_weights.csv", weights.second2);
            if (!ok)
                return false;

            // Load biases
            auto tempFirstLayerBiases = load_int16_array(modelDir + "first_linear_biases.csv", FIRST_OUT);
            std::memcpy(weights.firstBias, tempFirstLayerBiases, sizeof(int16_t) * FIRST_OUT);
            delete[] tempFirstLayerBiases;

        }
        catch (const std::exception &e)
        {
            std::cerr << "NNUEU load failed: " << e.what() << '\n';
            return false;
        }

        // (No fused firstW2Indices table to build — addAndRemoveOnInput does add+remove.)
        return true;
    }
