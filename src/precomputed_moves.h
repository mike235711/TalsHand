#ifndef PRECOMPUTED_MOVES_H
#define PRECOMPUTED_MOVES_H
// compile-time tables – C++20

#include <array>
#include <cstdint>

namespace precomputed_moves
{
    constexpr bool is_valid_square(int f, int r) { return unsigned(f) < 8 && unsigned(r) < 8; }

    constexpr int cabs(int x) { return x < 0 ? -x : x; }

    /* knight / king moves ---------------------------------------*/

    constexpr uint64_t calc_knight(int sq)
    {
        constexpr std::array<std::pair<int, int>, 8> deltas{{{2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}}};

        const int f = sq % 8, r = sq / 8;
        uint64_t m = 0;
        for (auto [df, dr] : deltas)
            if (is_valid_square(f + df, r + dr))
                m |= 1ULL << ((r + dr) * 8 + f + df);
        return m;
    }

    constexpr uint64_t calc_king(int sq)
    {
        constexpr std::array<std::pair<int, int>, 8> deltas{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};

        const int f = sq % 8, r = sq / 8;
        uint64_t m = 0;
        for (auto [df, dr] : deltas)
            if (is_valid_square(f + df, r + dr))
                m |= 1ULL << ((r + dr) * 8 + f + df);
        return m;
    }

    /* pawn attacks ----------------------------------------------*/

    constexpr uint64_t calc_pawn_attack(int sq, bool white)
    {
        const int f = sq % 8, r = sq / 8, dr = white ? 1 : -1;
        if ((white && r == 7) || (!white && r == 0))
            return 0;

        uint64_t m = 0;
        if (is_valid_square(f - 1, r + dr))
            m |= 1ULL << ((r + dr) * 8 + f - 1);
        if (is_valid_square(f + 1, r + dr))
            m |= 1ULL << ((r + dr) * 8 + f + 1);
        return m;
    }

    /* straight / diagonal helpers --------------------------------*/

    constexpr uint64_t calc_straight_between(int s1, int s2, bool inc2)
    {
        if (s1 == s2)
            return 0;
        const int f1 = s1 % 8, r1 = s1 / 8, f2 = s2 % 8, r2 = s2 / 8;
        const int df = (f2 > f1) - (f2 < f1), dr = (r2 > r1) - (r2 < r1);
        if (df && dr)
            return 0; // not same file/rank

        uint64_t m = 0;
        int f = f1 + df, r = r1 + dr;
        while (f != f2 || r != r2)
        {
            m |= 1ULL << (r * 8 + f);
            f += df;
            r += dr;
        }
        if (inc2)
            m |= 1ULL << s2;
        return m;
    }

    constexpr uint64_t calc_diagonal_between(int s1, int s2, bool inc2)
    {
        if (s1 == s2)
            return 0;
        const int f1 = s1 % 8, r1 = s1 / 8, f2 = s2 % 8, r2 = s2 / 8;
        const int df = (f2 > f1) - (f2 < f1), dr = (r2 > r1) - (r2 < r1);
        if (cabs(f2 - f1) != cabs(r2 - r1))
            return 0; // not same diagonal

        uint64_t m = 0;
        int f = f1 + df, r = r1 + dr;
        while (f != f2 || r != r2)
        {
            m |= 1ULL << (r * 8 + f);
            f += df;
            r += dr;
        }
        if (inc2)
            m |= 1ULL << s2;
        return m;
    }

    constexpr uint64_t calc_full_between(int s1, int s2)
    {
        return calc_straight_between(s1, s2, true) |
               calc_diagonal_between(s1, s2, true);
    }

    /* small table builders --------------------------------------*/

    template <typename F>
    consteval std::array<uint64_t, 64> make64(F fun)
    {
        std::array<uint64_t, 64> a{};
        for (int s = 0; s < 64; ++s)
            a[s] = fun(s);
        return a;
    }

    template <typename F>
    consteval std::array<std::array<uint64_t, 64>, 64> make64x64(F fun)
    {
        std::array<std::array<uint64_t, 64>, 64> a{};
        for (int s1 = 0; s1 < 64; ++s1)
            for (int s2 = 0; s2 < 64; ++s2)
                a[s1][s2] = fun(s1, s2);
        return a;
    }

    /* public compile-time tables --------------------------------*/

    inline constexpr auto knight_moves = make64(calc_knight);
    inline constexpr auto king_moves = make64(calc_king);

    inline constexpr std::array<std::array<uint64_t, 64>, 2> pawn_attacks = []
    {
        std::array<std::array<uint64_t, 64>, 2> a{};
        for (int s = 0; s < 64; ++s)
        {
            a[0][s] = calc_pawn_attack(s, true);  // white
            a[1][s] = calc_pawn_attack(s, false); // black
        }
        return a;
    }();

    /* bishop / rook rays ---------------------------------------*/

    inline constexpr auto bishop_unfull_rays = []
    {
        std::array<uint64_t, 64> a{};
        for (int s1 = 0; s1 < 64; ++s1)
            for (int s2 = 0; s2 < 64; ++s2)
                a[s1] |= calc_diagonal_between(s1, s2, false);
        return a;
    }();

    inline constexpr auto rook_unfull_rays = []
    {
        std::array<uint64_t, 64> a{};
        for (int s1 = 0; s1 < 64; ++s1)
            for (int s2 = 0; s2 < 64; ++s2)
                a[s1] |= calc_straight_between(s1, s2, false);
        return a;
    }();

    /* “full” rays ----------------------------------------------*/

    inline constexpr auto bishop_full_rays = []
    {
        std::array<uint64_t, 64> a{};
        for (int s1 = 0; s1 < 64; ++s1)
            for (int s2 = 0; s2 < 64; ++s2)
                a[s1] |= calc_diagonal_between(s1, s2, true);
        return a;
    }();

    inline constexpr auto rook_full_rays = []
    {
        std::array<uint64_t, 64> a{};
        for (int s1 = 0; s1 < 64; ++s1)
            for (int s2 = 0; s2 < 64; ++s2)
                a[s1] |= calc_straight_between(s1, s2, true);
        return a;
    }();

    constexpr uint64_t full_line(int s1, int s2)
    {
        if (s1 == s2)
            return 0;

        const int f1 = s1 % 8, r1 = s1 / 8;
        const int f2 = s2 % 8, r2 = s2 / 8;
        const int df = (f2 > f1) - (f2 < f1);
        const int dr = (r2 > r1) - (r2 < r1);

        // use cabs instead of std::abs → now constexpr-safe under C++20
        if (df && dr && cabs(f2 - f1) != cabs(r2 - r1))
            return 0; // not on the same diagonal / line

        uint64_t bb = 0;

        /* forward direction (includes square-1) */
        for (int f = f1, r = r1; is_valid_square(f, r); f += df, r += dr)
            bb |= 1ULL << (r * 8 + f);

        /* backward direction */
        for (int f = f1 - df, r = r1 - dr; is_valid_square(f, r); f -= df, r -= dr)
            bb |= 1ULL << (r * 8 + f);

        return bb;
    }

    /* one-blocker tables ---------------------------------------*/

    inline constexpr auto precomputedBishopMovesTableOneBlocker = make64x64([](int s1, int s2)
                                                                            { return calc_diagonal_between(s1, s2, false); });

    inline constexpr auto precomputedRookMovesTableOneBlocker = make64x64([](int s1, int s2)
                                                                          { return calc_straight_between(s1, s2, false); });

    inline constexpr auto precomputedQueenMovesTableOneBlocker = make64x64([](int s1, int s2)
                                                                           { return precomputedBishopMovesTableOneBlocker[s1][s2] |
                                                                                    precomputedRookMovesTableOneBlocker[s1][s2]; });

    inline constexpr auto precomputedBishopMovesTableOneBlocker2 = make64x64([](int s1, int s2)
                                                                             { return calc_diagonal_between(s1, s2, true); });

    inline constexpr auto precomputedRookMovesTableOneBlocker2 = make64x64([](int s1, int s2)
                                                                           { return calc_straight_between(s1, s2, true); });

    inline constexpr auto precomputedQueenMovesTableOneBlocker2 = make64x64([](int s1, int s2)
                                                                            { return precomputedBishopMovesTableOneBlocker2[s1][s2] |
                                                                                     precomputedRookMovesTableOneBlocker2[s1][s2]; });

    /* 8-square “on-line” ---------------------------------------*/

    inline constexpr auto OnLineBitboards = make64x64(full_line);

} // namespace precomputed_moves
#endif
