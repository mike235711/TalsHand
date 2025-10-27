#ifndef MAGICMOVES_H_INCLUDED
#define MAGICMOVES_H_INCLUDED

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>

void initmagicmoves();

constexpr uint64_t FileABB = 0x0101010101010101ULL;
constexpr uint64_t FileBBB = FileABB << 1;
constexpr uint64_t FileCBB = FileABB << 2;
constexpr uint64_t FileDBB = FileABB << 3;
constexpr uint64_t FileEBB = FileABB << 4;
constexpr uint64_t FileFBB = FileABB << 5;
constexpr uint64_t FileGBB = FileABB << 6;
constexpr uint64_t FileHBB = FileABB << 7;

constexpr uint64_t Rank1BB = 0xFF;
constexpr uint64_t Rank2BB = Rank1BB << (8 * 1);
constexpr uint64_t Rank3BB = Rank1BB << (8 * 2);
constexpr uint64_t Rank4BB = Rank1BB << (8 * 3);
constexpr uint64_t Rank5BB = Rank1BB << (8 * 4);
constexpr uint64_t Rank6BB = Rank1BB << (8 * 5);
constexpr uint64_t Rank7BB = Rank1BB << (8 * 6);
constexpr uint64_t Rank8BB = Rank1BB << (8 * 7);

extern uint8_t SquareDistance[64][64];

// Magic holds all magic uint64_ts relevant data for a single square
struct Magic {
    uint64_t  mask;
    uint64_t* attacks;
    uint64_t magic;
    unsigned shift;

    // Compute the attack's index using the 'magic uint64_ts' approach
    unsigned index(uint64_t occupied) const {
        return unsigned(((occupied & mask) * magic) >> shift);
    }

    uint64_t attacks_bb(uint64_t occupied) const { return attacks[index(occupied)]; }
};

extern Magic Magics[64][2];


// Returns the attacks by the given piece
// assuming the board is occupied according to the passed uint64_t.
// Sliding piece attacks do not continue passed an occupied square.
inline uint64_t BmagicNOMASK(int s, uint64_t occupied) {
        return Magics[s][0].attacks_bb(occupied);
}
inline uint64_t RmagicNOMASK(int s, uint64_t occupied) {
        return Magics[s][1].attacks_bb(occupied);
}
inline uint64_t QmagicNOMASK(int s, uint64_t occupied) {
        return Magics[s][0].attacks_bb(occupied) | Magics[s][1].attacks_bb(occupied);
}

#endif // MAGICS_V2_H_INCLUDED