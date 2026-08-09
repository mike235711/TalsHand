#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <iostream> // For std::cerr, std::endl
#include <cstdlib>  // For exit()
#include <cassert>
#include <algorithm> // for std::clamp if you like
#include <cstdint>
#include <limits>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifdef __SSE4_1__
#include <smmintrin.h> // SSE4.1
#endif

#include "network.h"

// Define static members declared in AccumulatorStack
namespace NNUEU
{
    const int8_t *AccumulatorStack::secondLayer1WeightsBlockWhiteTurn = nullptr;
    const int8_t *AccumulatorStack::secondLayer2WeightsBlockWhiteTurn = nullptr;
    const int8_t *AccumulatorStack::secondLayer1WeightsBlockBlackTurn = nullptr;
    const int8_t *AccumulatorStack::secondLayer2WeightsBlockBlackTurn = nullptr;
}

/////////////////////////////////
// Parameter loading utilities
/////////////////////////////////

// Reads exactly `cols` comma-separated int8 values (across any number of lines).
//
// THE BOUNDS CHECK IS NOT COSMETIC. This was the only loader in this file WITHOUT one
// (load_int16_array and both 2D loaders in accumulation.cpp always had theirs), and the
// missing `index < cols` was a live heap overflow: a build whose head geometry does not match
// the net directory it loads writes past the end of `arr`. Reproduced 2026-08-07 on the famE
// N512_H16_* builds -- NNUEU::DefaultNetDir mapped FIRST_OUT==512 to models/n512_h32/, whose
// final_layer_weights.csv holds 32 values, into a 16-byte buffer: SIGABRT (exit 134) on 7 of 8
// startups. It also silently corrupted whatever the allocator had put after the buffer on the
// runs that did not abort, which is the worse half of the bug.
//
// A count mismatch (too few OR too many values) is now a HARD LOAD ERROR rather than a
// zero-filled tail / an overflow: the caller gets nullptr and refuses the whole net. A net
// whose head geometry disagrees with the build is not a net that plays slightly worse, it is a
// different function -- see the arm.json written next to every famE arm for the cmake line a
// given directory needs.
int8_t *load_int8_1D_array(const std::string &file_path, size_t cols)
{
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return nullptr;
    }

    int8_t *arr = new int8_t[cols](); // Zero-initialize the array
    std::string line;
    std::size_t index = 0;
    bool overflow = false;
    while (!overflow && std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ','))
        {
            if (item.find_first_not_of(" \t\r\n") == std::string::npos)
                continue; // trailing newline / blank field
            if (index >= cols)
            {
                overflow = true;
                break;
            }
            arr[index++] = static_cast<int8_t>(std::stoi(item));
        }
    }
    if (overflow || index != cols)
    {
        std::cerr << file_path << ": expected exactly " << cols << " values, file has "
                  << (overflow ? "more than " : "") << index
                  << " -- this net does not match this build's geometry, refusing to load\n";
        delete[] arr;
        return nullptr;
    }
    return arr;
}

int16_t *load_int16_array(const std::string &file_path, size_t cols)
{
    int16_t *arr = new int16_t[cols](); // Zero-initialize the array
    std::ifstream file(file_path);
    std::string line;
    std::size_t index = 0;

    while (std::getline(file, line) && index < cols)
    {
        arr[index++] = static_cast<int16_t>(std::stoi(line));
    }
    return arr;
}

int16_t load_int16(const std::string &file_path)
{
    int16_t result;
    std::ifstream file(file_path);
    std::string line;
    std::size_t index = 0;

    while (std::getline(file, line) && index < 1)
    {
        result = static_cast<int16_t>(std::stoi(line));
    }
    return result;
}


namespace NNUEU
{
#if NNUEU_HEAD_SKIP
    // famE HEAD_SKIP: rescale a skip scalar from where it is computed to where it is added.
    //
    // The skip scalar is lane SECOND_OUT_W-1 of an ordinary 2nd-layer neuron's `s = dot + bias`
    // -- the SAME "x8128" fixed point every other 2nd-layer neuron's `s` lives at before ITS OWN
    // >>6 shift (x8128 = x64 hidden-layer-weight scale * x127 FT-activation scale; hidden-layer
    // BIASES are quantized at that same x8128 product so `dot + bias` stays one consistent scale).
    // It needs to land in evaluate()'s final sum, which lives at the "x4096" output scale (see
    // engine.cpp/accumulation.h header comment: final weights x(4096/127), final bias x4096,
    // output centered by -2048; materialTerm/psqtTerm are likewise pre-scaled to x4096 before
    // being added there).
    //
    // No weight in this net supplies that x8128 -> x4096 conversion for the skip lane: it BYPASSES
    // the 3rd layer AND the final layer, i.e. bypasses the only two places such a conversion is
    // normally learned (an ordinary neuron gets there via >>6 (x8128->x127) then two MORE trained
    // linear layers). Per the spec, the skip path is a straight, UNCLAMPED linear residual (no
    // CReLU) -- nnue-pytorch's factorizer pattern (l1c_out/l1f_out folded straight into the final
    // sum) -- so there is no natural place to hang a learned scale either.
    //
    // Design choice (documented here since nothing upstream forces it): treat the skip lane as an
    // IDENTITY-weighted residual, i.e. give it exactly the per-unit contribution an ORDINARY
    // neuron would get from a final-layer weight of 1.0. That fixes the scale to the same
    // x8128->x127->x4096 chain an ordinary neuron gets from >>6 composed with finalW's x(4096/127)
    // at unit weight: (1/64) * (4096/127) = 4096/8128 = 64/127 EXACTLY (no rounding in the ratio
    // itself). Concretely: skip_output_x4096 = floor(skip_raw_x8128 * 64 / 127).
    //
    // 127 is not a power of two, and (unlike every other quantity in this file) the skip value is
    // NOT ReLU'd -- it can be negative -- so this needs an explicit floor, not C++'s truncate-
    // toward-zero `/`, to match the floor convention every `>>`-based rescale in this file already
    // uses (see e.g. the psqt_l3 comment in evaluate() below).
    static inline int32_t headSkipToOutputScale(int32_t skipRawX8128)
    {
        const int64_t num = static_cast<int64_t>(skipRawX8128) * 64; // still exact, no rounding yet
        int64_t q = num / 127;
        if (num % 127 != 0 && num < 0)
            --q; // C++ '/' truncates toward zero; step down by 1 to floor negative quotients
        return static_cast<int32_t>(q);
    }
#endif

    // No default argument here: the declaration in network.h has none (so this one was dead code
    // anyway) and the width -> net mapping has a single home, NNUEU::DefaultNetDir.
    bool Network::load(const std::string &modelDir)
    {
        try
        {
            // third layer: THIRD_STACKS*THIRD_OUT_W neurons x THIRD_IN inputs (flat row-major in
            // the CSV). THIRD_STACKS is 8 under NNUEU_THIRD_PHASE (bucket-major: all of bucket 0's
            // THIRD_OUT_W rows, then bucket 1's, ... -- the same slicing PyTorch's
            // `view(-1, 8, H2)` on a [H2*8, third_in] nn.Linear.weight produces) and 1 otherwise.
            // THIRD_IN == HEAD_CONCAT except under NNUEU_PSQT_L3, where the CSV rows carry
            // HEAD_CONCAT + 8 columns (the 8 psqt lanes) and are re-strided to THIRD_STRIDE
            // in memory (pad columns stay zero for the NEON 16-lane dot).
            auto tempThirdLayerWeights = load_int8_1D_array(modelDir + "third_layer_weights.csv", THIRD_STACKS * THIRD_OUT_W * THIRD_IN);
            if (!tempThirdLayerWeights)
                return false; // wrong shape for this build -- load_int8_1D_array already explained
            for (int o = 0; o < THIRD_STACKS * THIRD_OUT_W; ++o)
                std::memcpy(weights.thirdW + o * THIRD_STRIDE, tempThirdLayerWeights + o * THIRD_IN,
                            sizeof(int8_t) * THIRD_IN);
            delete[] tempThirdLayerWeights;

            // final layer: THIRD_OUT_W weights (buffer padded +8 for NEON over-read, pad stays 0)
            auto tempFinalLayerWeights = load_int8_1D_array(modelDir + "final_layer_weights.csv", THIRD_OUT_W);
            if (!tempFinalLayerWeights)
                return false;
            std::memcpy(weights.finalW, tempFinalLayerWeights, sizeof(int8_t) * THIRD_OUT_W);
            delete[] tempFinalLayerWeights;

            // second-layer biases: turn (SECOND_OUT_W) then not-turn (SECOND_OUT_W) -> HEAD_CONCAT
            auto tempSecondLayer1Biases = load_int16_array(modelDir + "second_layer_turn_biases.csv", SECOND_OUT_W);
            auto tempSecondLayer2Biases = load_int16_array(modelDir + "second_layer_not_turn_biases.csv", SECOND_OUT_W);
            std::memcpy(weights.secondBias, tempSecondLayer1Biases, sizeof(int16_t) * SECOND_OUT_W);
            std::memcpy(weights.secondBias + SECOND_OUT_W, tempSecondLayer2Biases, sizeof(int16_t) * SECOND_OUT_W);
            delete[] tempSecondLayer1Biases;
            delete[] tempSecondLayer2Biases;

            // third_layer_biases.csv: THIRD_STACKS*THIRD_OUT_W values, same bucket-major order as
            // the weights (bias has no per-input stride, so no re-striding needed here).
            auto tempThirdLayerBiases = load_int16_array(modelDir + "third_layer_biases.csv", THIRD_STACKS * THIRD_OUT_W);
            std::memcpy(weights.thirdBias, tempThirdLayerBiases, sizeof(int16_t) * THIRD_STACKS * THIRD_OUT_W);
            delete[] tempThirdLayerBiases;

            weights.finalBias = load_int16(modelDir + "final_layer_biases.csv");

            // Optional king-bias tables (king_bias arm).
            {
                std::ifstream f1(modelDir + "king_emb_turn.csv"), f2(modelDir + "king_emb_not_turn.csv");
                if (f1 && f2)
                {
                    auto readTable = [](std::ifstream &f, int16_t tbl[64][FIRST_OUT]) {
                        std::string line; int r = 0;
                        while (std::getline(f, line) && r < 64) {
                            if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
                            std::stringstream ss(line); std::string v; int c = 0;
                            while (std::getline(ss, v, ',') && c < FIRST_OUT)
                                tbl[r][c++] = static_cast<int16_t>(std::stoi(v));
                            ++r;
                        }
                        return r;
                    };
                    const int r1 = readTable(f1, weights.kingEmbTurn);
                    const int r2 = readTable(f2, weights.kingEmbNotTurn);
                    weights.hasKingBias = (r1 == 64 && r2 == 64);
                    if (!weights.hasKingBias)
                        std::cerr << "king_emb_*.csv: got " << r1 << "/" << r2
                                  << " rows, expected 64/64 -- IGNORED\n";
                }
            }

            // Optional PSQT table (psqt_sf arm): 8 rows x F_MAP, already at the output scale.
            {
                std::ifstream pf(modelDir + "psqt_weights.csv");
                if (pf)
                {
                    // TWO LAYOUTS, one destination. Classic psqt_sf writes 8 rows x F_MAP, one
                    // per phase bucket. famF's FT_PHASE writes ONE row of 8*F_MAP: the psqt is a
                    // single FT column there, and the bucket was already chosen on the input
                    // side, so its values arrive in the same bucket-major order the accumulator
                    // file uses. Both land in psqtW[bucket][feature], which is the layout
                    // psqtRaw() already indexes -- so psqtRaw() itself needs no change.
                    std::string line; int r = 0; int flat = 0;
                    while (std::getline(pf, line) && r < 8) {
                        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
                        std::stringstream ss(line); std::string v; int c = 0;
                        while (std::getline(ss, v, ','))
                        {
                            if (r == 0 && flat < 8 * F_MAP)
                                weights.psqtW[flat / F_MAP][flat % F_MAP] = std::stoi(v), ++flat;
                            else if (c < F_MAP)
                                weights.psqtW[r][c++] = std::stoi(v);
                        }
                        if (r == 0 && flat == 8 * F_MAP)
                            break;          // famF single-row form, fully consumed
                        ++r;
                    }
                    weights.hasPsqt = (r == 8) || (r == 0 && flat == 8 * F_MAP);
                    if (!weights.hasPsqt && (r != 0 || flat != 0))
                        std::cerr << "psqt_weights.csv: " << r << " rows / " << flat
                                  << " flat values, expected 8 rows of " << F_MAP << " or one row of "
                                  << 8 * F_MAP << " -- IGNORED\n";
                }
                if (PSQT_L3 && !weights.hasPsqt)
                {
                    // a psqt_l3 build without the psqt table would feed a constant 64 into the
                    // 8 extra third-layer lanes AND drop the output term -- a silently wrong net.
                    std::cerr << "NNUEU_PSQT_L3 build but " << modelDir
                              << "psqt_weights.csv is missing/invalid -- refusing to load\n";
                    return false;
                }
            }

            // Optional material-residual table. Absent for every ordinary net -> hasMaterial
            // stays false and evaluate() is untouched.
            {
                std::ifstream mf(modelDir + "material_dp.csv");
                if (mf)
                {
                    std::string line;
                    int r = 0;
                    while (std::getline(mf, line) && r < Network::Weight::MAT_BUCKETS)
                    {
                        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
                        std::stringstream ss(line);
                        std::string v;
                        int c = 0;
                        while (std::getline(ss, v, ',') && c < 5)
                            weights.materialDp[r][c++] = std::stoi(v);
                        ++r;
                    }
                    weights.matBucketCount = r;
                    weights.hasMaterial = (r == 1 || r == Network::Weight::MAT_BUCKETS);
                    if (r != 0 && !weights.hasMaterial)
                        std::cerr << "material_dp.csv has " << r
                                  << " rows; expected 1 (fixed) or 7 (bucketed) -- IGNORED\n";
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "NNUEU load failed: " << e.what() << '\n';
            return false;
        }

        return true;
    }

    // The NNUE is built to give an evaluation of the position with high values being good for whose turn it is.
    // Side-to-move material term, in the engine's output units. Mirrors the training-side
    // `material_dp`: counts are own-minus-opponent per piece type FROM THE SIDE TO MOVE, because
    // the label the net was trained against is side-to-move relative. Getting that sign wrong
    // makes the term cancel itself across the dataset (measured: +0.69 white / -0.69 black).
    int32_t Network::materialTerm(const BitPosition &position, const Transformer &transformer) const
    {
        // the material table lives on the NETWORK's weights (it is head-side, loaded with the
        // head CSVs), not on the transformer's accumulator weights
        const auto &w = weights;
        if (!w.hasMaterial)
            return 0;
        const bool white = position.getTurn();
        int diff[5];
        for (int t = 0; t < 5; ++t)
        {
            // API is getPieces(colour, pieceType) -- colour first. The colour INDEX is not
            // getTurn() directly: verified empirically (white-to-move down a rook produced
            // +832 instead of -832), so the side-to-move index is the complement.
            const int stm = white ? 0 : 1;
            diff[t] = countBits(position.getPieces(stm, t))
                    - countBits(position.getPieces(1 - stm, t));
        }
        int bucket = 0;
        if (w.matBucketCount == Network::Weight::MAT_BUCKETS)
        {
            // balance in pawns, classical weights, only to select the bucket
            static constexpr int kVal[5] = {1, 3, 3, 5, 9};
            int bal = 0;
            for (int t = 0; t < 5; ++t) bal += diff[t] * kVal[t];
            // Same bucketing as training's torch.bucketize(bal, [-5,-2,-0.5,0.5,2,5]):
            // bucket = #edges strictly below bal. Edges are doubled so the half-pawn
            // boundaries stay integer.
            static constexpr int kEdges2[6] = {-10, -4, -1, 1, 4, 10};
            const int bal2 = 2 * bal;
            for (int e = 0; e < 6; ++e) if (bal2 > kEdges2[e]) ++bucket;
        }
        int32_t s = 0;
        for (int t = 0; t < 5; ++t) s += diff[t] * w.materialDp[bucket][t];
        return s;
    }

    // king_bias: add the two king rows to the accumulator, pre-activation. The king indices
    // must use the SAME indices as the 2nd-layer block selection (accumulation.cpp:357-360):
    // BOTH kings in the side-to-move's frame -- white turn: second1[wk], second2[bk] (raw);
    // black turn: second1[invertIndex(bk)], second2[invertIndex(wk)]. Training feeds
    // king_emb_turn/not_turn the very same psqt_indices/layer_stack_indices, so any other
    // orientation reads the wrong table row (caught by the vs-float gate on the first run
    // with non-zero tables; invisible while the tables were zero-init).
    void Network::applyKingBias(int16_t *acc, int kTurn, int kNotTurn) const
    {
        if (!weights.hasKingBias)
            return;
        const int16_t *a = weights.kingEmbTurn[kTurn];
        const int16_t *b = weights.kingEmbNotTurn[kNotTurn];
        for (int i = 0; i < FIRST_OUT; ++i)
            acc[i] = static_cast<int16_t>(acc[i] + a[i] + b[i]);
    }

    // SF's PSQT phase bucket: (n_pieces - 1) / 4 clamped to 0..7, counting ALL pieces on the
    // board -- BOTH colours, all 6 types INCLUDING king (getPieces' pieceType 0..5 is P,N,B,R,Q,K;
    // see bitposition.h/.cpp m_pieces[0..5]). This equals the training-side phase_bucket()'s
    // "valid king-free-FT features + 2" exactly, since a king-free feature count already IS the
    // non-king piece count and the "+2" is the two kings this loop counts directly.
    // psqtRaw() (psqt_sf) and third_phase's stack selection both call this SAME function so the
    // bucket can never drift between the two consumers.
    int Network::phaseBucket(const BitPosition &position) const
    {
        int nPieces = 0;
        for (int c = 0; c < 2; ++c)
            for (int t = 0; t < 6; ++t)
                nPieces += countBits(position.getPieces(c, t));
        return std::min(7, std::max(0, (nPieces - 1) / 4));
    }

    // psqt_sf: Stockfish's skip term. Sums the 8-wide PSQT row over the ACTIVE features of both
    // perspectives, picks the piece-count phase bucket, and returns the stm-signed perspective
    // difference (wpsqt - bpsqt) UNHALVED, at the output (x4096) scale. F_MAP is 640 and ~30
    // features are active, so this is cheap. psqtTerm() halves it for the output add; psqt_l3
    // additionally maps it onto a third-layer input lane (see evaluate()).
    int32_t Network::psqtRaw(const BitPosition &position) const
    {
        if (!weights.hasPsqt)
            return 0;
        const int bucket = phaseBucket(position);

        int32_t wp = 0, bp = 0;
        for (int c = 0; c < 2; ++c)
            for (int t = 0; t < 5; ++t)          // king-free FT: 5 piece types
            {
                uint64_t bb = position.getPieces(c, t);
                while (bb)
                {
                    const int sq = getLeastSignificantBitIndex(bb);
                    bb &= bb - 1;
                    // psqt_weights.csv is exported in the ENGINE's BLOCKED plane order
                    // [own P..Q, opp P..Q], the same order the FT rows use. Indexing it with
                    // the fork's interleaved t*2+side reads another piece type's column and
                    // costs the whole term (measured: corr 0.26 against the trained net).
                    const int pw = (c == 0 ? 0 : 5) + t;   // c==0 is white
                    const int pb = (c == 1 ? 0 : 5) + t;
                    wp += weights.psqtW[bucket][pw * 64 + sq];
                    bp += weights.psqtW[bucket][pb * 64 + invertIndex(sq)];
                }
            }
        // King-plane psqt columns (F_MAP 704/768 only -- compiles to nothing at 640). The
        // fork's psqt_sf columns are an ordinary slice of self.input's OUTPUT, which sees every
        // active feature including the king planes; _seed_psqt() only zeros those rows at
        // init, and by a few epochs past psqt_freeze_epochs they carry real, non-negligible
        // weight (measured on the famD kings_in__main epoch=9 ckpt: king rows mean|.|~=0.012 vs
        // piece rows'~=0.164 -- not noise). Dropping them (as this function did until now) cost
        // ~0.001-0.0015 of correlation against the true float model on real positions, small but
        // measurable and easy to close for free: it is the SAME columns the FT/psqtW export
        // already carries (psqtW is sized [8][F_MAP], not [8][640]), just never read.
        // Indices mirror the FT's own convention exactly (accumulation.cpp initialize()/
        // psqtRaw's own mirror trick above): wp uses each perspective's OWN raw king square(s),
        // bp uses the mirrored plane at the INVERTED square (mirrorPlane swaps 10<->11 under
        // KINGS_IN, and maps 10->10 under KING_NOTURN_IN -- see accumulation.h::mirrorPlane).
        if constexpr (KINGS_IN)
        {
            const int wk = position.getKingPosition(0);
            const int bk = position.getKingPosition(1);
            wp += weights.psqtW[bucket][KING_OWN_BASE + wk] + weights.psqtW[bucket][KING_OPP_BASE + bk];
            bp += weights.psqtW[bucket][KING_OPP_BASE + invertIndex(wk)] + weights.psqtW[bucket][KING_OWN_BASE + invertIndex(bk)];
        }
        else if constexpr (KING_NOTURN_IN)
        {
            const int wk = position.getKingPosition(0);
            const int bk = position.getKingPosition(1);
            wp += weights.psqtW[bucket][KING_OWN_BASE + bk];
            bp += weights.psqtW[bucket][KING_OWN_BASE + invertIndex(wk)];
        }
        return position.getTurn() ? (wp - bp) : (bp - wp);
    }

    // (us - 0.5) is +/- 1/2; the difference of the two perspectives carries the factor 2.
    // Same truncation as the old inline `(wp - bp) / 2` / `(bp - wp) / 2` (bit-identical).
    int32_t Network::psqtTerm(const BitPosition &position) const
    {
        return psqtRaw(position) / 2;
    }

    int16_t Network::evaluate(const BitPosition &position, NNUEU::AccumulatorStack &accumulatorStack, const Transformer &transformer) const
    {
        // Update incrementally from the last computed node
        accumulatorStack.forward_update_incremental(accumulatorStack.findLastComputedNode(position.getTurn()), position.getTurn(), transformer);

#ifndef NDEBUG
        accumulatorStack.verifyTopAgainstFresh(position, not position.getTurn(), transformer);
#endif

        // Change the NNUEU king positions if needed
        if (accumulatorStack.getStackKingPosition(0) != position.getKingPosition(0))
            accumulatorStack.changeWhiteKingPosition(position.getKingPosition(0), transformer);

        if (accumulatorStack.getStackKingPosition(1) != position.getKingPosition(1))
            accumulatorStack.changeBlackKingPosition(position.getKingPosition(1), transformer);

        assert(position.getKingPosition(0) == accumulatorStack.getStackKingPosition(0));
        assert(position.getKingPosition(1) == accumulatorStack.getStackKingPosition(1));

        AccumulatorState &updatedAcc = accumulatorStack.top();

        // king_bias works on a COPY: the accumulator is shared/incremental across nodes, so
        // adding the king rows in place would accumulate them again at every evaluation.
        alignas(16) int16_t accBuf[FIRST_OUT];
        const int wk = accumulatorStack.getStackKingPosition(0);
        const int bk = accumulatorStack.getStackKingPosition(1);
        if (weights.hasKingBias)
        {
            const bool white = position.getTurn();
            const int16_t *src = updatedAcc.inputTurn[white ? 0 : 1];
            std::memcpy(accBuf, src, sizeof(accBuf));
            // same indices as the 2nd-layer blocks (accumulation.cpp:357-360): both kings in
            // the side-to-move's frame
            const int kTurn    = white ? wk : invertIndex(bk);
            const int kNotTurn = white ? bk : invertIndex(wk);
            applyKingBias(accBuf, kTurn, kNotTurn);
        }

#if NNUEU_PSQT_L3
        // psqt_l3: the phase-bucketed PSQT difference is used TWICE, exactly as in training:
        //   (a) added to the output, halved: out += ps, ps = (wpsqt-bpsqt)*(us-0.5)  -> raw/2
        //   (b) fed to the third layer as 8 identical lanes: clamp(ps*0.25 + 0.5, 0, 1)
        // Scale bookkeeping for (b): psqt_weights.csv is at the OUTPUT scale (x4096), so
        // ps = raw/8192 in the model's float units; the engine's activations live at x127.
        //   127 * clamp(0.25*(raw/8192) + 0.5, 0, 1) = clamp(raw*127/32768 + 63.5, 0, 127)
        // computed in fixed point as floor((raw*127 + 2080768) / 32768)  [2080768 = 63.5*32768].
        // The floor matches the engine's truncating ">>" activation convention everywhere else;
        // the third-layer weights over these lanes then quantize at the ordinary x64 int8 grid,
        // identical to the 64 head columns (training STE rounds the WHOLE third.weight at 1/64).
        const int32_t psqtRawStm = psqtRaw(position);
        const int32_t psqtAdd = psqtRawStm / 2;   // same truncation as psqtTerm()
        const int8_t psqtLane = static_cast<int8_t>(std::clamp(
            static_cast<int32_t>((static_cast<int64_t>(psqtRawStm) * 127 + 2080768) >> 15), 0, 127));
#define NNUEU_PSQT_L3_ARG , psqtLane
#define NNUEU_PSQT_OUT_TERM psqtAdd
#else
#define NNUEU_PSQT_L3_ARG
#define NNUEU_PSQT_OUT_TERM psqtTerm(position)
#endif

#if NNUEU_THIRD_PHASE
        // third_phase: pick the ONE phase-bucketed thirdW/thirdBias stack this position needs and
        // dot only against it (see accumulation.h). Same bucket phaseBucket() gives psqtRaw() --
        // computed once here so it can never disagree with the psqt term's bucket.
        const int thirdBucket = phaseBucket(position);
#define NNUEU_THIRD_PHASE_ARG , thirdBucket
#else
#define NNUEU_THIRD_PHASE_ARG
#endif

        if (position.getTurn())
        {
#ifndef NDEBUG
            return static_cast<int16_t>(forwardPassDebug(weights.hasKingBias ? accBuf : updatedAcc.inputTurn[0], accumulatorStack.secondLayer1WeightsBlockWhiteTurn, accumulatorStack.secondLayer2WeightsBlockWhiteTurn NNUEU_PSQT_L3_ARG NNUEU_THIRD_PHASE_ARG) - 2048 + materialTerm(position, transformer) + NNUEU_PSQT_OUT_TERM);
#else
            return static_cast<int16_t>(forwardPass(weights.hasKingBias ? accBuf : updatedAcc.inputTurn[0], accumulatorStack.secondLayer1WeightsBlockWhiteTurn, accumulatorStack.secondLayer2WeightsBlockWhiteTurn NNUEU_PSQT_L3_ARG NNUEU_THIRD_PHASE_ARG) - 2048 + materialTerm(position, transformer) + NNUEU_PSQT_OUT_TERM);
#endif
        }

        else
        {
#ifndef NDEBUG
            return static_cast<int16_t>(forwardPassDebug(weights.hasKingBias ? accBuf : updatedAcc.inputTurn[1], accumulatorStack.secondLayer1WeightsBlockBlackTurn, accumulatorStack.secondLayer2WeightsBlockBlackTurn NNUEU_PSQT_L3_ARG NNUEU_THIRD_PHASE_ARG) - 2048 + materialTerm(position, transformer) + NNUEU_PSQT_OUT_TERM);
#else
            return static_cast<int16_t>(forwardPass(weights.hasKingBias ? accBuf : updatedAcc.inputTurn[1], accumulatorStack.secondLayer1WeightsBlockBlackTurn, accumulatorStack.secondLayer2WeightsBlockBlackTurn NNUEU_PSQT_L3_ARG NNUEU_THIRD_PHASE_ARG) - 2048 + materialTerm(position, transformer) + NNUEU_PSQT_OUT_TERM);
#endif
        }
    }

    int16_t Network::forwardPass(int16_t *pInput, const int8_t *pWeights11, const int8_t *pWeights12
#if NNUEU_PSQT_L3
                                 , int8_t psqtLane
#endif
#if NNUEU_THIRD_PHASE
                                 , int thirdBucket
#endif
                                 ) const
    // This function should pass using simd instructions an array pInput of 16 int16's through a neural network.
    // There are two first layers of 8 by 4 each taking the same pInput, after concatenating the outputs of both first layers,
    // the second layer is 8 by 4, the third layer is 4 by 1.
    //
    // The input is int16, and the weights are int8. So before multiplying we reduce int16 to int8
    // (by clipping to max int8) and clip negatives to zero before each layer pass.
    {
        // Wide-net head (e.g. N128/256/512/768/1024: acc(N) -> HEAD_OUT_W -> THIRD_OUT_W -> 1).
        // FIRST_OUT, HEAD_OUT_W (== HEAD_CONCAT unless famE's HEAD_SUM is on) and THIRD_OUT_W are
        // all multiples of 16, so the dot loops are remainder-free for any wide width. NEON (SDOT)
        // path; scalar fallback. Both bit-exact with the numpy/torch quant reference (re-verify per
        // shape via the Debug forwardPassDebug assert).
        if constexpr (FIRST_OUT >= 128)
        {
#if NNUEU_THIRD_PHASE
            // third_phase: the ONE phase-bucketed thirdW/thirdBias stack this position needs,
            // selected up front (bucket-major: stack k's THIRD_OUT_W rows start at row k*THIRD_OUT_W).
            // Dotting only against this slice -- not all 8 -- is the whole point: training computes
            // all 8 because it needs gradients for every stack, the engine needs exactly one.
            const int thirdRowBase = thirdBucket * THIRD_OUT_W;
#else
            constexpr int thirdRowBase = 0;
#endif
#if defined(__ARM_NEON)
            // activations: narrow the int16 accumulator to int8 with ReLU clamp [0,127]
            alignas(16) int8_t a[FIRST_OUT];
            for (int i = 0; i < FIRST_OUT; i += 16)
            {
                int8x16_t v = vcombine_s8(vqmovn_s16(vld1q_s16(pInput + i)),
                                          vqmovn_s16(vld1q_s16(pInput + i + 8)));
                vst1q_s8(a + i, vmaxq_s8(v, vdupq_n_s8(0)));
            }
            // layer 1 (second): per perspective (turn, not_turn) compute SECOND_OUT_W pre-activations.
            // Classic (HEAD_SUM=0): activate each perspective SEPARATELY then concatenate.
            // Single-act -> CReLU only (l1 = crelu_turn ‖ crelu_nott). Dual-act -> per perspective
            // CReLU ‖ SqrCReLU, so l1 = crelu_turn ‖ sqrelu_turn ‖ crelu_nott ‖ sqrelu_nott (matches
            // the "_sq" training head: cat(CReLU(pre), SqrCReLU(pre)) per projection).
            // famE (HEAD_SUM=1): SUM the two perspectives' RAW pre-activation outputs into ONE
            // HEAD_SUM_RAW_W-wide vector FIRST, and activate only the sum (see accumulation.h's
            // HEAD_SUM_* constants). Under HEAD_SKIP, lane SECOND_OUT_W-1 of EACH perspective never
            // joins the sum -- it is carried as a separate "skip scalar" and folded straight into
            // the final output below (bypassing the summed/activated path and the 3rd/final layers).
            // PSQT_L3 widens l1 to THIRD_STRIDE: 8 psqt lanes after HEAD_OUT_W, zero pad after
            // (PSQT_L3 cannot coexist with HEAD_SUM -- see accumulation.h's static_assert).
            alignas(16) int8_t l1[THIRD_STRIDE] = {0};
#if NNUEU_HEAD_SKIP
            int32_t skipTurnRaw = 0, skipNottRaw = 0; // x8128 scale (dot + bias, unshifted, unclamped)
#endif
            if constexpr (HEAD_SUM)
            {
                int32_t combined[HEAD_SUM_RAW_W];
                for (int blk = 0; blk < 2; ++blk)
                {
                    const int8_t *pW = (blk == 0) ? pWeights11 : pWeights12;
                    const int biasBase = blk * SECOND_OUT_W;
                    const int8_t *aBlk = a + (SPLIT_FT ? blk * SPLIT_READ : 0);
                    for (int o = 0; o < SECOND_OUT_W; ++o)
                    {
                        const int8_t *w = pW + o * SPLIT_READ;
                        int32x4_t acc = vdupq_n_s32(0);
                        for (int i = 0; i < SPLIT_READ; i += 16)
                            acc = vdotq_s32(acc, vld1q_s8(aBlk + i), vld1q_s8(w + i));
                        const int32_t s = vaddvq_s32(acc) + weights.secondBias[biasBase + o];
#if NNUEU_HEAD_SKIP
                        if (o == SECOND_OUT_W - 1)
                        {
                            (blk == 0 ? skipTurnRaw : skipNottRaw) = s;
                            continue;
                        }
#endif
                        if (blk == 0)
                            combined[o] = s;
                        else
                            combined[o] += s;
                    }
                }
                for (int o = 0; o < HEAD_SUM_RAW_W; ++o)
                {
                    const int c = std::min(127, std::max(0, static_cast<int>(combined[o] >> 6)));
                    l1[o] = static_cast<int8_t>(c); // CReLU(combined)
                    if constexpr (DUAL_ACT)
                        l1[HEAD_SUM_RAW_W + o] = static_cast<int8_t>((c * c) >> 7); // SqrCReLU(combined)
                }
            }
            else
            {
                for (int blk = 0; blk < 2; ++blk)
                {
                    const int8_t *pW = (blk == 0) ? pWeights11 : pWeights12;
                    const int biasBase = blk * SECOND_OUT_W;                 // secondBias: turn then not_turn
                    const int outBase = blk * (DUAL_ACT ? 2 : 1) * SECOND_OUT_W;
                    // SPLIT_FT: this projection reads only its own half of the accumulator,
                    // and its weights are stored at that same (halved) stride.
                    const int8_t *aBlk = a + (SPLIT_FT ? blk * SPLIT_READ : 0);
                    for (int o = 0; o < SECOND_OUT_W; ++o)
                    {
                        const int8_t *w = pW + o * SPLIT_READ;
                        int32x4_t acc = vdupq_n_s32(0);
                        for (int i = 0; i < SPLIT_READ; i += 16)
                            acc = vdotq_s32(acc, vld1q_s8(aBlk + i), vld1q_s8(w + i));
                        int32_t s = vaddvq_s32(acc) + weights.secondBias[biasBase + o];
                        const int c = std::min(127, std::max(0, static_cast<int>(s >> 6)));
                        l1[outBase + o] = static_cast<int8_t>(c); // CReLU
                        if constexpr (DUAL_ACT)
                            l1[outBase + SECOND_OUT_W + o] = static_cast<int8_t>((c * c) >> 7); // SqrCReLU = c^2/128
                    }
                }
            }
#if NNUEU_PSQT_L3
            // the 8 psqt lanes all carry the SAME per-position scalar, exactly like training's
            // `.expand(-1, 8)`; lanes THIRD_IN..THIRD_STRIDE-1 stay zero (and so do their weights).
            for (int i = HEAD_CONCAT; i < THIRD_IN; ++i)
                l1[i] = psqtLane;
#endif
            // layer 2 (third): THIRD_OUT_W neurons, dot over THIRD_STRIDE (== HEAD_OUT_W unless
            // PSQT_L3 pads 72 -> 80). l2 padded up to a multiple of 16 (zero) so the final NEON
            // 16-lane dot never over-reads when THIRD_OUT_W < 16 (h*x8).
            alignas(16) int8_t l2[((THIRD_OUT_W + 15) / 16) * 16] = {0};
            for (int o = 0; o < THIRD_OUT_W; ++o)
            {
                const int8_t *w = weights.thirdW + (thirdRowBase + o) * THIRD_STRIDE;
                int32x4_t acc = vdupq_n_s32(0);
                for (int i = 0; i < THIRD_STRIDE; i += 16)
                    acc = vdotq_s32(acc, vld1q_s8(l1 + i), vld1q_s8(w + i));
                int32_t s = vaddvq_s32(acc) + weights.thirdBias[thirdRowBase + o];
                l2[o] = static_cast<int8_t>(std::min(127, std::max(0, static_cast<int>(s >> 6))));
            }
            // layer 3 (final): 1 output, dot over THIRD_OUT_W
            int32x4_t accf = vdupq_n_s32(0);
            for (int i = 0; i < THIRD_OUT_W; i += 16)
                accf = vdotq_s32(accf, vld1q_s8(l2 + i), vld1q_s8(weights.finalW + i));
            int32_t finalResult = vaddvq_s32(accf) + weights.finalBias;
#if NNUEU_HEAD_SKIP
            // famE: fold the two skip scalars straight into the final output, at the fixed-point
            // scale derived in headSkipToOutputScale()'s comment above.
            finalResult += headSkipToOutputScale(skipTurnRaw) + headSkipToOutputScale(skipNottRaw);
#endif
            return static_cast<int16_t>(finalResult);
#else
            int8_t a[FIRST_OUT];
            for (int i = 0; i < FIRST_OUT; ++i)
                a[i] = static_cast<int8_t>(std::min(127, std::max(0, static_cast<int>(pInput[i]))));
            // PSQT_L3/HEAD_SUM: same widening/restructuring as the NEON path -- the two MUST agree
            // (a previous change patched only NEON and Debug/Release diverged).
            int8_t l1[THIRD_STRIDE] = {0};
#if NNUEU_HEAD_SKIP
            int32_t skipTurnRaw = 0, skipNottRaw = 0; // x8128 scale (dot + bias, unshifted, unclamped)
#endif
            if constexpr (HEAD_SUM)
            {
                int32_t combined[HEAD_SUM_RAW_W];
                for (int blk = 0; blk < 2; ++blk)
                {
                    const int8_t *pW = (blk == 0) ? pWeights11 : pWeights12;
                    const int biasBase = blk * SECOND_OUT_W;
                    const int8_t *aBlk = a + (SPLIT_FT ? blk * SPLIT_READ : 0);
                    for (int o = 0; o < SECOND_OUT_W; ++o)
                    {
                        const int8_t *w = pW + o * SPLIT_READ;
                        int32_t s = weights.secondBias[biasBase + o];
                        for (int i = 0; i < SPLIT_READ; ++i)
                            s += static_cast<int32_t>(aBlk[i]) * static_cast<int32_t>(w[i]);
#if NNUEU_HEAD_SKIP
                        if (o == SECOND_OUT_W - 1)
                        {
                            (blk == 0 ? skipTurnRaw : skipNottRaw) = s;
                            continue;
                        }
#endif
                        if (blk == 0)
                            combined[o] = s;
                        else
                            combined[o] += s;
                    }
                }
                for (int o = 0; o < HEAD_SUM_RAW_W; ++o)
                {
                    const int c = std::min(127, std::max(0, static_cast<int>(combined[o] >> 6)));
                    l1[o] = static_cast<int8_t>(c); // CReLU(combined)
                    if constexpr (DUAL_ACT)
                        l1[HEAD_SUM_RAW_W + o] = static_cast<int8_t>((c * c) >> 7); // SqrCReLU(combined)
                }
            }
            else
            {
                for (int blk = 0; blk < 2; ++blk)
                {
                    const int8_t *pW = (blk == 0) ? pWeights11 : pWeights12;
                    const int biasBase = blk * SECOND_OUT_W;
                    const int outBase = blk * (DUAL_ACT ? 2 : 1) * SECOND_OUT_W;
                    for (int o = 0; o < SECOND_OUT_W; ++o)
                    {
                        // SPLIT_FT: same halving as the NEON path -- the two MUST agree, or a Debug
                        // build would evaluate differently from Release and nothing would look broken.
                        const int8_t *aBlk = a + (SPLIT_FT ? blk * SPLIT_READ : 0);
                        const int8_t *w = pW + o * SPLIT_READ;
                        int32_t s = weights.secondBias[biasBase + o];
                        for (int i = 0; i < SPLIT_READ; ++i)
                            s += static_cast<int32_t>(aBlk[i]) * static_cast<int32_t>(w[i]);
                        const int c = std::min(127, std::max(0, static_cast<int>(s >> 6)));
                        l1[outBase + o] = static_cast<int8_t>(c); // CReLU
                        if constexpr (DUAL_ACT)
                            l1[outBase + SECOND_OUT_W + o] = static_cast<int8_t>((c * c) >> 7); // SqrCReLU = c^2/128
                    }
                }
            }
#if NNUEU_PSQT_L3
            for (int i = HEAD_CONCAT; i < THIRD_IN; ++i)
                l1[i] = psqtLane;                       // 8 identical lanes, as in training
#endif
            int8_t l2[THIRD_OUT_W];
            for (int o = 0; o < THIRD_OUT_W; ++o)
            {
                int32_t s = weights.thirdBias[thirdRowBase + o];
                for (int i = 0; i < THIRD_IN; ++i)
                    s += static_cast<int32_t>(l1[i]) * static_cast<int32_t>(weights.thirdW[(thirdRowBase + o) * THIRD_STRIDE + i]);
                l2[o] = static_cast<int8_t>(std::min(127, std::max(0, static_cast<int>(s >> 6))));
            }
            int32_t s = weights.finalBias;
            for (int i = 0; i < THIRD_OUT_W; ++i)
                s += static_cast<int32_t>(l2[i]) * static_cast<int32_t>(weights.finalW[i]);
#if NNUEU_HEAD_SKIP
            s += headSkipToOutputScale(skipTurnRaw) + headSkipToOutputScale(skipNottRaw);
#endif
            return static_cast<int16_t>(s);
#endif
        }
#if defined(__ARM_NEON)

        // ---- Layer 1: FIRST_OUT-wide accumulator -> 8 neurons (4 from each block) ----
        int8x8_t input2;
        if constexpr (FIRST_OUT == 8)
        {
            // Width-8 path: int16 vmull. Kept bit-identical for the w8 nets.
            int8x8_t vector = vmax_s8(vqmovn_s16(vld1q_s16(pInput)), vdup_n_s8(0));
            int8x8_t weight1[8];
            for (int i = 0; i < 4; ++i)
            {
                weight1[i] = vld1_s8(pWeights11 + i * 8);
                weight1[i + 4] = vld1_s8(pWeights12 + i * 8);
            }
            int16x8_t output1 = {0};
            for (int i = 0; i < 8; ++i)
                output1[i] = vaddvq_s16(vmull_s8(vector, weight1[i]));
            int16x8_t bias1 = vld1q_s16(weights.secondBias);
            output1 = vmaxq_s16(vshrq_n_s16(vaddq_s16(bias1, output1), 6), vdupq_n_s16(0));
            input2 = vqmovn_s16(output1);
        }
        else
        {
            // Width-32 path: int32 SDOT (int16 would overflow over 32 elements).
            // Narrow the 32-wide int16 accumulator to int8 ONCE via SIMD and keep it in
            // two registers reused by all 8 neurons; each neuron is then two SDOTs.
            // Validated bit-exact vs the scalar pass (forwardPassDebug assert) and
            // NNUEU_Optim/nnueu_bench_w32.cpp. This branch is specialised for
            // FIRST_OUT==32 (the only non-8 width allowed by accumulation.h's
            // top-level static_assert), so it reads pInput[0..31] unconditionally.
            const int8x16_t in0 = vmaxq_s8(vcombine_s8(vqmovn_s16(vld1q_s16(pInput)),
                                                       vqmovn_s16(vld1q_s16(pInput + 8))), vdupq_n_s8(0));
            const int8x16_t in1 = vmaxq_s8(vcombine_s8(vqmovn_s16(vld1q_s16(pInput + 16)),
                                                       vqmovn_s16(vld1q_s16(pInput + 24))), vdupq_n_s8(0));
            int8_t l1[8];
            for (int r = 0; r < 8; ++r)
            {
                const int8_t *w = (r < 4 ? pWeights11 : pWeights12) + (r & 3) * FIRST_OUT;
                int32x4_t acc = vdotq_s32(vdupq_n_s32(0), in0, vld1q_s8(w));
                acc = vdotq_s32(acc, in1, vld1q_s8(w + 16));
                int32_t s = vaddvq_s32(acc) + weights.secondBias[r];
                l1[r] = static_cast<int8_t>(std::min<int>(127, std::max<int>(0, s >> 6)));
            }
            input2 = vld1_s8(l1);
        }

        // Layer 2

        int8x8_t weight2[4];

        // Load weights into vectors
        for (int i = 0; i < 4; ++i)
        {
            weight2[i] = vld1_s8(weights.thirdW + i * 8);
        }

        int16x4_t bias2 = vld1_s16(weights.thirdBias);

        int16x4_t output2;

        // Perform the computations
        for (int i = 0; i < 4; ++i)
        {
            int16_t temp = vaddvq_s16(vmull_s8(input2, weight2[i])) + bias2[i];
            output2[i] = temp >> 6; // Store the result in the array
        }

        // Apply ReLU activation
        output2 = vmax_s16(output2, vdup_n_s16(0));

        // Layer 3
        int8x8_t input3 = vqmovn_s16(vcombine_s16(output2, vdup_n_s16(0)));

        // Load only 4 bytes and zero-pad safely
        int8x8_t weight3 = vld1_s8(weights.finalW);

        int16_t output3 = vaddvq_s16(vmull_s8(input3, weight3)) + weights.finalBias;

        return output3;

#elif defined(__AVX2__) || defined(__AVX__) || defined(__SSSE3__) || defined(__SSE4_1__)
        const int16_t *pBias1 = weights.secondBias;
        const int8_t *pWeights2 = weights.thirdW;
        const int16_t *pBias2 = weights.thirdBias;
        const int8_t *pWeights3 = weights.finalW;
        const int16_t *pBias3 = &weights.finalBias;
        //
        // Layer 0:
        //  - Load 8 x int16
        //  - Narrow to int8 with saturation
        //  - Clip negatives to zero (ReLU)
        //
        __m128i input_16 = _mm_loadu_si128((__m128i *)pInput); // 8 x int16

        // ReLU on int16
        __m128i zero = _mm_setzero_si128();
        input_16 = _mm_max_epi16(input_16, zero);

        // Now saturate down to 8-bit signed.
        // _mm_packs_epi16 packs 8 x int16 into 8 x int8 (lower half) + 8 x int16 into 8 x int8 (upper half)
        // We'll supply zero for the upper half so we only keep 8 real int8 in the lower half.
        __m128i input_16_high = _mm_setzero_si128();
        __m128i packed_8 = _mm_packs_epi16(input_16, input_16_high);
        // packs_epi16 produces 16 int8 in the 128-bit register,
        // but the top 8 are from the second argument (which is zero). We only need the low 8 for further multiply.
        // If you want them in an int8_t array, you can store the low 8 bytes.

        //
        // Layer 1: Suppose we have 8 separate int8-weight vectors, each length=8
        //  - Multiply each weight vector by our 8-element input
        //  - Sum (horizontal sum) => 1 value
        //  - Add bias
        //  - Shift >> 6
        //  - ReLU
        //
        // For 8 separate neurons, let's load each weight as __m128i with zero-extended or sign-extended.
        // Then multiply. SSE doesn't have a single "dot-product" instruction for 8 int8s, so we do sign-extension
        // and multiply in 16 bits, then horizontal add.
        //
        __m128i result1[8];

        // We will compute:
        //   output[i] = sum_{k=0..7} (input8[k] * weight1[i][k]) + bias1[i]
        //   Then shift >> 6 and clamp ReLU.

        // Load input as 8 int8 in the lower half of a register, sign-extend to 16 bits
        __m128i in_8_lo = _mm_cvtepi8_epi16(packed_8);
        // now in_8_lo is 8 x int16 with the sign-extended bytes from input.

        // We'll store the results in a single SSE register at the end.
        __m128i bias1_v = _mm_loadu_si128((__m128i *)pBias1); // 8 x int16

        for (int i = 0; i < 8; i++)
        {
            // Load the i-th weight vector of length=8 (int8).
            // pWeights11 + i*8 or pWeights12 + i*8 as appropriate.
            // In your example, you have 4 from pWeights11, 4 from pWeights12, etc.
            // We'll just show a single version (adjust logic to your layer shape).
            __m128i w_i_8 = _mm_loadl_epi64((__m128i const *)(pWeights11 + i * 8));
            // w_i_8 has 8 x int8 in the lower 64 bits

            // Sign-extend to 8 x int16
            __m128i w_i_16 = _mm_cvtepi8_epi16(w_i_8);

            // Multiply each pair: in_8_lo[k]*w_i_16[k] => 8 x int16
            __m128i prod = _mm_mullo_epi16(in_8_lo, w_i_16);

            // Now we want sum of these 8 int16. Use _mm_hadd_epi16 or do it manually.
            // SSE4.1 doesn't have a single "horizontal add of all 8 lanes to one value",
            // but we can do a few steps:
            __m128i sum1 = _mm_hadd_epi16(prod, prod); // pairwise add => 4 x int16 repeated
            __m128i sum2 = _mm_hadd_epi16(sum1, sum1); // => 2 x int16 repeated
            __m128i sum3 = _mm_hadd_epi16(sum2, sum2); // => 1 x int16 repeated

            // The actual sum is in the low 16 bits of sum3
            int16_t dot = (int16_t)_mm_extract_epi16(sum3, 0);

            // Add bias
            int16_t bias_i = ((int16_t *)&bias1_v)[i];
            dot += bias_i;

            // Shift >> 6
            dot >>= 6;

            // ReLU
            if (dot < 0)
                dot = 0;

            // Keep it in result1[i]. We'll convert to SSE after the loop if we want a vector of these 8.
            ((int16_t *)&result1[0])[i] = dot;
        }

        // Combine the 8 results into an SSE register:
        __m128i output1_16 = _mm_loadu_si128((__m128i *)&result1[0]);

        //
        // Next layers: exactly the same pattern
        //  - Convert to int8 with saturate
        //  - Multiply by weight vectors
        //  - Accumulate + bias
        //  - Shift, ReLU
        //
        // We'll do a short example for the second layer, which has 4 outputs.
        //
        // Convert to int8
        __m128i out1_clamped = _mm_max_epi16(output1_16, zero); // ReLU just in case
        __m128i out1_packed = _mm_packs_epi16(out1_clamped, zero);
        // Now the low 8 bytes of out1_packed hold the 8 int8 we want.

        // For layer 2, which has 4 outputs:
        int16_t out2[4];
        __m128i bias2_v = _mm_loadl_epi64((__m128i *)pBias2); // 4 x int16 in memory
        // Each neuron = sum of 8 products + bias
        for (int i = 0; i < 4; i++)
        {
            // load the i-th weight vector (8 int8)
            __m128i w2_i_8 = _mm_loadl_epi64((__m128i const *)(pWeights2 + i * 8));
            __m128i w2_i_16 = _mm_cvtepi8_epi16(w2_i_8); // 8 x int16

            // sign-extend input as well
            __m128i in2_16 = _mm_cvtepi8_epi16(out1_packed);

            __m128i prod = _mm_mullo_epi16(in2_16, w2_i_16);

            // horizontal sum:
            __m128i sum1 = _mm_hadd_epi16(prod, prod);
            __m128i sum2 = _mm_hadd_epi16(sum1, sum1);
            __m128i sum3 = _mm_hadd_epi16(sum2, sum2);
            int16_t dot = (int16_t)_mm_extract_epi16(sum3, 0);

            // add bias
            int16_t bias_i = ((int16_t *)&bias2_v)[i];
            dot += bias_i;

            // shift >> 6
            dot >>= 6;

            // ReLU
            if (dot < 0)
                dot = 0;

            out2[i] = dot;
        }

        //
        // Layer 3 (4 -> 1)
        //
        int16_t out2_clamped[4];
        for (int i = 0; i < 4; i++)
            out2_clamped[i] = (out2[i] < 0) ? 0 : out2[i];

        // Convert 4 x int16 to int8 for final multiply
        __m128i out2_vec_16 = _mm_loadl_epi64((__m128i const *)out2_clamped); // loads 4 x int16
        out2_vec_16 = _mm_max_epi16(out2_vec_16, zero);                       // ReLU again, if needed
        __m128i out2_packed = _mm_packs_epi16(out2_vec_16, zero);             // saturate to int8

        // multiply by weight3 (4 x int8) => single output
        // plus bias3
        __m128i w3_8 = _mm_loadl_epi64((__m128i const *)pWeights3); // 4 x int8 in the lower half
        __m128i w3_16 = _mm_cvtepi8_epi16(w3_8);

        // sign-extend out2
        __m128i in3_16 = _mm_cvtepi8_epi16(out2_packed); // 4 x int16

        __m128i prod3 = _mm_mullo_epi16(in3_16, w3_16);
        __m128i sum3_1 = _mm_hadd_epi16(prod3, prod3);
        __m128i sum3_2 = _mm_hadd_epi16(sum3_1, sum3_1);
        __m128i sum3_3 = _mm_hadd_epi16(sum3_2, sum3_2);
        int16_t dot3 = (int16_t)_mm_extract_epi16(sum3_3, 0);

        // add bias
        int16_t bias3_v = pBias3[0];
        dot3 += bias3_v;

        // That final dot3 is your result
        return dot3;

#else

#endif
    }

} // namespace NNUEU
