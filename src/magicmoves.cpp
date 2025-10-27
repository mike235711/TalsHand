#include "magicmoves.h"
#include "bit_utils.h"

#include <algorithm>
#include <bitset>
#include <initializer_list>

class PRNG {

    uint64_t s;

    uint64_t rand64() {

        s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
        return s * 2685821657736338717LL;
    }

   public:
    PRNG(uint64_t seed) :
        s(seed) {
        assert(seed);
    }

    template<typename T>
    T rand() {
        return T(rand64());
    }

    // Special generator used to fast init magic numbers.
    // Output values only have 1/8th of their bits set on average.
    template<typename T>
    T sparse_rand() {
        return T(rand64() & rand64() & rand64());
    }
};

uint8_t PopCnt16[1 << 16];
uint8_t SquareDistance[64][64];


alignas(64) Magic Magics[64][2];

namespace {

uint64_t RookTable[0x19000];   // To store rook attacks
uint64_t BishopTable[0x1480];  // To store bishop attacks

void init_magics(int pt, uint64_t table[], Magic magics[][2]);
constexpr bool is_ok(int s) { return s >= 0 && s <= 63; }
constexpr int rank_of(int s) { return int(s >> 3); }
constexpr int file_of(int s) { return int(s & 7); }

constexpr uint64_t rank_bb1(int r) { return Rank1BB << (8 * r); }

constexpr uint64_t rank_bb(int s) { return rank_bb1(rank_of(s)); }

constexpr uint64_t file_bb1(int f) { return FileABB << f; }

constexpr uint64_t file_bb(int s) { return file_bb1(file_of(s)); }

inline int file_distance(int x, int y) {
    return std::abs(file_of(x) - file_of(y));
}

inline int rank_distance(int x, int y) {
    return std::abs(rank_of(x) - rank_of(y));
}

inline int square_distance(int x, int y) {
    return SquareDistance[x][y];
}

// Returns the bitboard of target square for the given step
// from the given square. If the step is off the board, returns empty bitboard.
uint64_t safe_destination(int s, int step) {
    int to = int(s + step);
    return is_ok(to) && square_distance(s, to) <= 2 ? (1ULL << to) : uint64_t(0);
}
}

// Initializes various bitboard tables. It is called at
// startup and relies on global objects to be already zero-initialized.
void initmagicmoves() {
    init_magics(3, RookTable, Magics);
    init_magics(2, BishopTable, Magics);
}

namespace {

uint64_t sliding_attack(int pt, int sq, uint64_t occupied) {

    uint64_t  attacks             = 0;
    const int rookDirs[4] = {8, -8, 1, -1};
    const int bishopDirs[4] = {9, -7, -9, 7};
    const int* dirs = (pt == 3) ? rookDirs : bishopDirs;

    for (int i = 0; i < 4; ++i)
    {
        int d = dirs[i];
        int s = sq;
        while (true)
		{
            int to = s + d;
            if (!is_ok(to) || square_distance(s, to) != 1)
					break;

            s = to;
            attacks |= (1ULL << s);

            if (occupied & (1ULL << s))
					break;
				}
			}

    return attacks;
}


// Computes all rook and bishop attacks at startup. Magic
// bitboards are used to look up attacks of sliding pieces. As a reference see
// https://www.chessprogramming.org/Magic_Bitboards. In particular, here we use
// the so called "fancy" approach.
void init_magics(int pt, uint64_t table[], Magic magics[][2]) {
    for (int s1 = 0; s1 <= 63; ++s1)
        for (int s2 = 0; s2 <= 63; ++s2)
            SquareDistance[s1][s2] = std::max(file_distance(s1, s2), rank_distance(s1, s2));

    // Optimal PRNG seeds to pick the correct magics in the shortest time
    int seeds[][8] = {{8977, 44560, 54343, 38998, 5731, 95205, 104912, 17020},
                            {728, 10316, 55013, 32803, 12281, 15100, 16645, 255}};

    uint64_t occupancy[4096];
    int      epoch[4096] = {}, cnt = 0;

    uint64_t reference[4096];
    int      size = 0;

    for (int s = 0; s <= 63; ++s)
    {
        // Board edges are not considered in the relevant occupancies
        uint64_t edges = ((Rank1BB | Rank8BB) & ~rank_bb(s)) | ((FileABB | FileHBB) & ~file_bb(s));

        // Given a square 's', the mask is the bitboard of sliding attacks from
        // 's' computed on an empty board. The index must be big enough to contain
        // all the attacks for each possible subset of the mask and so is 2 power
        // the number of 1s of the mask. Hence we deduce the size of the shift to
        // apply to the 64 or 32 bits word to get the index.
        Magic& m = magics[s][pt - 2];
        m.mask   = sliding_attack(pt, s, 0) & ~edges;
        m.shift = 64 - countBits(m.mask);

        // Set the offset for the attacks table of the square. We have individual
        // table sizes for each square with "Fancy Magic Bitboards".
        m.attacks = s == 0 ? table : magics[s - 1][pt - 2].attacks + size;
        size      = 0;

        // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
        // store the corresponding sliding attack bitboard in reference[].
        uint64_t b = 0;
        do
        {
            occupancy[size] = b;
            reference[size] = sliding_attack(pt, s, b);

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);

    PRNG rng(seeds[pt - 2][rank_of(s)]);

        // Find a magic for square 's' picking up an (almost) random number
        // until we find the one that passes the verification test.
        for (int i = 0; i < size;)
			{
            for (m.magic = 0; countBits((m.magic * m.mask) >> 56) < 6;)
                m.magic = rng.sparse_rand<uint64_t>();

            // A good magic must map every possible occupancy to an index that
            // looks up the correct sliding attack in the attacks[s] database.
            // Note that we build up the database for square 's' as a side
            // effect of verifying the magic. Keep track of the attempt count
            // and save it in epoch[], little speed-up trick to avoid resetting
            // m.attacks[] after every failed attempt.
            for (++cnt, i = 0; i < size; ++i)
				{
                unsigned idx = m.index(occupancy[i]);

                if (epoch[idx] < cnt)
                {
                    epoch[idx]     = cnt;
                    m.attacks[idx] = reference[i];
                }
                else if (m.attacks[idx] != reference[i])
					break;
				}
			}
		}
	}
}