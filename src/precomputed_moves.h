#include <array>
#include <map>

#ifndef PRECOMPUTED_MOVES_H
#define PRECOMPUTED_MOVES_H

// inline keyword is used to replace everytime there's a mention of the variable
// with the variable definition. However the compiler can choose to not replace it,
// since if the replacement is too big the performance might decrease at runtime.

namespace precomputed_moves
{
    constexpr uint64_t getBishopExclusiveRayFromTo(unsigned short square, uint64_t blockers_bit)
    // Given a square and a blockers bit get the bitboard representing the moveable squares on the line excluding blocker capture
    {
        int r = square / 8;
        int c = square % 8;
        int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}; // Diagonals
        for (int i = 0; i < 4; ++i)                                  // For each direction
        {
            int dr = directions[i][0];
            int dc = directions[i][1];
            int nr = r + dr;
            int nc = c + dc;
            uint64_t pos_bit = 0;
            bool isInLine{false};

            while (0 <= nr && nr <= 7 && 0 <= nc && nc <= 7)
            {
                if ((blockers_bit & (1ULL << (nr * 8 + nc))) != 0)
                {
                    isInLine = true;
                    break;
                }
                pos_bit |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
            if (isInLine)
                return pos_bit;
        }
        return 0;
    }

    constexpr uint64_t getRookExclusiveRayFromTo(unsigned short square, uint64_t blockers_bit)
    // Given a square and a blockers bit get the bitboard representing the rook moveable squares on the line excluding blocker capture
    {
        int r = square / 8;
        int c = square % 8;
        int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; // Diagonals
        for (int i = 0; i < 4; ++i)                                // For each direction
        {
            int dr = directions[i][0];
            int dc = directions[i][1];
            int nr = r + dr;
            int nc = c + dc;
            uint64_t pos_bit = 0;
            bool isInLine{false};

            while (0 <= nr && nr <= 7 && 0 <= nc && nc <= 7)
            {
                if ((blockers_bit & (1ULL << (nr * 8 + nc))) != 0)
                {
                    isInLine = true;
                    break;
                }
                pos_bit |= 1ULL << (nr * 8 + nc);
                nr += dr;
                nc += dc;
            }
            if (isInLine)
                return pos_bit;
        }
        return 0;
    }
    constexpr uint64_t getBishopInclusiveRayFromTo(unsigned short square, uint64_t blockers_bit)
    // Given a square and a blockers bit get the bitboard representing the moveable squares on the line including blocker capture
    {
        int r = square / 8;
        int c = square % 8;
        int directions[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}; // Diagonals
        for (int i = 0; i < 4; ++i)                                  // For each direction
        {
            int dr = directions[i][0];
            int dc = directions[i][1];
            int nr = r + dr;
            int nc = c + dc;
            uint64_t pos_bit = 0;
            bool isInLine{false};

            while (0 <= nr && nr <= 7 && 0 <= nc && nc <= 7)
            {
                pos_bit |= 1ULL << (nr * 8 + nc);
                if ((blockers_bit & (1ULL << (nr * 8 + nc))) != 0)
                {
                    isInLine = true;
                    break;
                }
                nr += dr;
                nc += dc;
            }
            if (isInLine)
                return pos_bit;
        }
        return 0;
    }

    constexpr uint64_t getRookInclusiveRayFromTo(unsigned short square, uint64_t blockers_bit)
    // Given a square and a blockers bit get the bitboard representing the rook moveable squares on the line including blocker capture
    {
        int r = square / 8;
        int c = square % 8;
        int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; // Diagonals
        for (int i = 0; i < 4; ++i)                                // For each direction
        {
            int dr = directions[i][0];
            int dc = directions[i][1];
            int nr = r + dr;
            int nc = c + dc;
            uint64_t pos_bit = 0;
            bool isInLine{false};

            while (0 <= nr && nr <= 7 && 0 <= nc && nc <= 7)
            {
                pos_bit |= 1ULL << (nr * 8 + nc);
                if ((blockers_bit & (1ULL << (nr * 8 + nc))) != 0)
                {
                    isInLine = true;
                    break;
                }
                nr += dr;
                nc += dc;
            }
            if (isInLine)
                return pos_bit;
        }
        return 0;
    }

    constexpr std::array<std::array<uint64_t, 64>,64> getBishopOneBlockerTable()
    // Get bitboard from square_1 to square_2, excluding both. Else 0
    {
        std::array<std::array<uint64_t, 64>, 64> table{};
        for (int i = 0; i < 64; ++i)
        {
            for (int j = 0; j < 64; ++j)
            {
                // We compute a ray from i to j including i and excluding j
                if (i == j)
                    table[i][j] = 0;
                else
                    table[i][j] = getBishopExclusiveRayFromTo(i, (1ULL << j));
            }
        }
        return table;
    }
    constexpr std::array<std::array<uint64_t, 64>, 64> getRookOneBlockerTable()
    // Get bitboard from square_1 to square_2, excluding both. Else 0
    {
        std::array<std::array<uint64_t, 64>, 64> table{};
        for (int i = 0; i < 64; ++i)
        {
            for (int j = 0; j < 64; ++j)
            {
                // We compute a ray from i to j including i and excluding j
                if (i == j)
                    table[i][j] = 0;
                else
                    table[i][j] = getRookExclusiveRayFromTo(i, (1ULL << j));
            }
        }
        return table;
    }

    constexpr std::array<std::array<uint64_t, 64>, 64> getBishopOneBlockerTable2()
    // Get bitboard from square_1 to square_2, excluding square_1. Else 0
    {
        std::array<std::array<uint64_t, 64>, 64> table{};
        uint64_t ray{0};
        for (int i = 0; i < 64; ++i)
        {
            for (int j = 0; j < 64; ++j)
            {
                // We compute a ray from i to j including i and excluding j
                if (i == j)
                    table[i][j] = 0;
                else
                {
                    ray = getBishopInclusiveRayFromTo(i, (1ULL << j));
                    table[i][j] = ray;
                }
            }
        }
        return table;
    }
    constexpr std::array<std::array<uint64_t, 64>, 64> getRookOneBlockerTable2()
    // Get bitboard from square_1 to square_2, excluding square_1. Else 0
    {
        std::array<std::array<uint64_t, 64>, 64> table{};
        uint64_t ray{0};
        for (int i = 0; i < 64; ++i)
        {
            for (int j = 0; j < 64; ++j)
            {
                // We compute a ray from i to j including i and excluding j
                if (i == j)
                    table[i][j] = 0;
                else
                {
                    ray = getRookInclusiveRayFromTo(i, (1ULL << j));
                    table[i][j] = ray;
                }
            }
        }
        return table;
    }
    constexpr std::array<std::array<uint64_t, 64>, 64> getOnLineBitboardsTable()
    {
        std::array<std::array<uint64_t, 64>, 64> table{};

        for (int i = 0; i < 64; ++i)
        {
            int r1 = i / 8;
            int c1 = i % 8;

            for (int j = 0; j < 64; ++j)
            {
                if (i == j)
                {
                    table[i][j] = 0;
                    continue;
                }

                int r2 = j / 8;
                int c2 = j % 8;

                // Check if squares are on the same row, column, or diagonal
                if (r1 == r2 || c1 == c2 || (r2 - r1 == c2 - c1) || (r2 - r1 == c1 - c2))
                {
                    uint64_t bitboard = (1ULL << i);

                    // Calculate direction vector without using abs()
                    int dr = (r2 == r1) ? 0 : (r2 > r1 ? 1 : -1);
                    int dc = (c2 == c1) ? 0 : (c2 > c1 ? 1 : -1);

                    int nr = r1 + dr;
                    int nc = c1 + dc;

                    // Traverse the line until we go out of bounds
                    while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
                    {
                        bitboard |= 1ULL << (nr * 8 + nc);
                        nr += dr;
                        nc += dc;
                    }

                    table[i][j] |= bitboard;
                    table[j][i] |= bitboard;
                }
                else
                {
                    table[i][j] = 0;
                }
            }
        }

        return table;
    }

    constexpr std::array<std::array<uint64_t, 64>, 64> getOnLineBitboardsTable2()
    {
        std::array<std::array<uint64_t, 64>, 64> table{};

        for (int i = 0; i < 64; ++i)
        {
            int r1 = i / 8;
            int c1 = i % 8;

            for (int j = 0; j < 64; ++j)
            {
                if (i == j)
                {
                    table[i][j] = 0;
                    continue;
                }

                int r2 = j / 8;
                int c2 = j % 8;

                // Check if squares are on the same row, column, or diagonal
                if (r1 == r2 || c1 == c2 || (r2 - r1 == c2 - c1) || (r2 - r1 == c1 - c2))
                {
                    uint64_t bitboard = (1ULL << i);

                    // Calculate direction vector without using abs()
                    int dr = (r2 == r1) ? 0 : (r2 > r1 ? 1 : -1);
                    int dc = (c2 == c1) ? 0 : (c2 > c1 ? 1 : -1);

                    int nr = r1 + dr;
                    int nc = c1 + dc;

                    // Traverse the line until we go out of bounds
                    while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
                    {
                        bitboard |= 1ULL << (nr * 8 + nc);
                        nr += dr;
                        nc += dc;
                    }

                    table[i][j] |= bitboard;
                    table[j][i] |= bitboard;
                }
                else
                {
                    table[i][j] = ~0ULL;
                }
            }
        }

        return table;
    }

    // Initialize precomputed move tables (generated using my python move generator)
    inline constexpr std::array<uint64_t, 64> knight_moves = {132096ull, 329728ull, 659712ull, 1319424ull, 2638848ull, 5277696ull, 10489856ull, 4202496ull, 33816580ull, 84410376ull, 168886289ull, 337772578ull, 675545156ull, 1351090312ull, 2685403152ull, 1075839008ull, 8657044482ull, 21609056261ull, 43234889994ull, 86469779988ull, 172939559976ull, 345879119952ull, 687463207072ull, 275414786112ull, 2216203387392ull, 5531918402816ull, 11068131838464ull, 22136263676928ull, 44272527353856ull, 88545054707712ull, 175990581010432ull, 70506185244672ull, 567348067172352ull, 1416171111120896ull, 2833441750646784ull, 5666883501293568ull, 11333767002587136ull, 22667534005174272ull, 45053588738670592ull, 18049583422636032ull, 145241105196122112ull, 362539804446949376ull, 725361088165576704ull, 1450722176331153408ull, 2901444352662306816ull, 5802888705324613632ull, 11533718717099671552ull, 4620693356194824192ull, 288234782788157440ull, 576469569871282176ull, 1224997833292120064ull, 2449995666584240128ull, 4899991333168480256ull, 9799982666336960512ull, 1152939783987658752ull, 2305878468463689728ull, 1128098930098176ull, 2257297371824128ull, 4796069720358912ull, 9592139440717824ull, 19184278881435648ull, 38368557762871296ull, 4679521487814656ull, 9077567998918656ull};
    inline constexpr std::array<uint64_t, 64> king_moves = {770ull, 1797ull, 3594ull, 7188ull, 14376ull, 28752ull, 57504ull, 49216ull, 197123ull, 460039ull, 920078ull, 1840156ull, 3680312ull, 7360624ull, 14721248ull, 12599488ull, 50463488ull, 117769984ull, 235539968ull, 471079936ull, 942159872ull, 1884319744ull, 3768639488ull, 3225468928ull, 12918652928ull, 30149115904ull, 60298231808ull, 120596463616ull, 241192927232ull, 482385854464ull, 964771708928ull, 825720045568ull, 3307175149568ull, 7718173671424ull, 15436347342848ull, 30872694685696ull, 61745389371392ull, 123490778742784ull, 246981557485568ull, 211384331665408ull, 846636838289408ull, 1975852459884544ull, 3951704919769088ull, 7903409839538176ull, 15806819679076352ull, 31613639358152704ull, 63227278716305408ull, 54114388906344448ull, 216739030602088448ull, 505818229730443264ull, 1011636459460886528ull, 2023272918921773056ull, 4046545837843546112ull, 8093091675687092224ull, 16186183351374184448ull, 13853283560024178688ull, 144959613005987840ull, 362258295026614272ull, 724516590053228544ull, 1449033180106457088ull, 2898066360212914176ull, 5796132720425828352ull, 11592265440851656704ull, 4665729213955833856ull};
    
    inline constexpr std::array<uint64_t, 64> white_pawn_attacks = {512ull, 1280ull, 2560ull, 5120ull, 10240ull, 20480ull, 40960ull, 16384ull, 131072ull, 327680ull, 655360ull, 1310720ull, 2621440ull, 5242880ull, 10485760ull, 4194304ull, 33554432ull, 83886080ull, 167772160ull, 335544320ull, 671088640ull, 1342177280ull, 2684354560ull, 1073741824ull, 8589934592ull, 21474836480ull, 42949672960ull, 85899345920ull, 171798691840ull, 343597383680ull, 687194767360ull, 274877906944ull, 2199023255552ull, 5497558138880ull, 10995116277760ull, 21990232555520ull, 43980465111040ull, 87960930222080ull, 175921860444160ull, 70368744177664ull, 562949953421312ull, 1407374883553280ull, 2814749767106560ull, 5629499534213120ull, 11258999068426240ull, 22517998136852480ull, 45035996273704960ull, 18014398509481984ull, 144115188075855872ull, 360287970189639680ull, 720575940379279360ull, 1441151880758558720ull, 2882303761517117440ull, 5764607523034234880ull, 11529215046068469760ull, 4611686018427387904ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull};
    inline constexpr std::array<uint64_t, 64> black_pawn_attacks = {0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 2ull, 5ull, 10ull, 20ull, 40ull, 80ull, 160ull, 64ull, 512ull, 1280ull, 2560ull, 5120ull, 10240ull, 20480ull, 40960ull, 16384ull, 131072ull, 327680ull, 655360ull, 1310720ull, 2621440ull, 5242880ull, 10485760ull, 4194304ull, 33554432ull, 83886080ull, 167772160ull, 335544320ull, 671088640ull, 1342177280ull, 2684354560ull, 1073741824ull, 8589934592ull, 21474836480ull, 42949672960ull, 85899345920ull, 171798691840ull, 343597383680ull, 687194767360ull, 274877906944ull, 2199023255552ull, 5497558138880ull, 10995116277760ull, 21990232555520ull, 43980465111040ull, 87960930222080ull, 175921860444160ull, 70368744177664ull, 562949953421312ull, 1407374883553280ull, 2814749767106560ull, 5629499534213120ull, 11258999068426240ull, 22517998136852480ull, 45035996273704960ull, 18014398509481984ull};
    

    // Moveable squares bitboard for bishop and rook without taking into account the edge squares
    // Used for computing blocker_bits given a position, used in BitPosition class
    inline constexpr std::array<uint64_t, 64> rook_unfull_rays = {282578800148862ull, 565157600297596ull, 1130315200595066ull, 2260630401190006ull, 4521260802379886ull, 9042521604759646ull, 18085043209519166ull, 36170086419038334ull, 282578800180736ull, 565157600328704ull, 1130315200625152ull, 2260630401218048ull, 4521260802403840ull, 9042521604775424ull, 18085043209518592ull, 36170086419037696ull, 282578808340736ull, 565157608292864ull, 1130315208328192ull, 2260630408398848ull, 4521260808540160ull, 9042521608822784ull, 18085043209388032ull, 36170086418907136ull, 282580897300736ull, 565159647117824ull, 1130317180306432ull, 2260632246683648ull, 4521262379438080ull, 9042522644946944ull, 18085043175964672ull, 36170086385483776ull, 283115671060736ull, 565681586307584ull, 1130822006735872ull, 2261102847592448ull, 4521664529305600ull, 9042787892731904ull, 18085034619584512ull, 36170077829103616ull, 420017753620736ull, 699298018886144ull, 1260057572672512ull, 2381576680245248ull, 4624614895390720ull, 9110691325681664ull, 18082844186263552ull, 36167887395782656ull, 35466950888980736ull, 34905104758997504ull, 34344362452452352ull, 33222877839362048ull, 30979908613181440ull, 26493970160820224ull, 17522093256097792ull, 35607136465616896ull, 9079539427579068672ull, 8935706818303361536ull, 8792156787827803136ull, 8505056726876686336ull, 7930856604974452736ull, 6782456361169985536ull, 4485655873561051136ull, 9115426935197958144ull};
    inline constexpr std::array<uint64_t, 64> bishop_unfull_rays = {18049651735527936ull, 70506452091904ull, 275415828992ull, 1075975168ull, 38021120ull, 8657588224ull, 2216338399232ull, 567382630219776ull, 9024825867763712ull, 18049651735527424ull, 70506452221952ull, 275449643008ull, 9733406720ull, 2216342585344ull, 567382630203392ull, 1134765260406784ull, 4512412933816832ull, 9024825867633664ull, 18049651768822272ull, 70515108615168ull, 2491752130560ull, 567383701868544ull, 1134765256220672ull, 2269530512441344ull, 2256206450263040ull, 4512412900526080ull, 9024834391117824ull, 18051867805491712ull, 637888545440768ull, 1135039602493440ull, 2269529440784384ull, 4539058881568768ull, 1128098963916800ull, 2256197927833600ull, 4514594912477184ull, 9592139778506752ull, 19184279556981248ull, 2339762086609920ull, 4538784537380864ull, 9077569074761728ull, 562958610993152ull, 1125917221986304ull, 2814792987328512ull, 5629586008178688ull, 11259172008099840ull, 22518341868716544ull, 9007336962655232ull, 18014673925310464ull, 2216338399232ull, 4432676798464ull, 11064376819712ull, 22137335185408ull, 44272556441600ull, 87995357200384ull, 35253226045952ull, 70506452091904ull, 567382630219776ull, 1134765260406784ull, 2832480465846272ull, 5667157807464448ull, 11333774449049600ull, 22526811443298304ull, 9024825867763712ull, 18049651735527936ull};

    // Moveable squares bitboard for bishop and rook taking into account the edge squares
    // Used for computing pin_bits in BitPosition class (setPinsBits and setChecksAndPinsBits)
    inline constexpr std::array<uint64_t, 64> rook_full_rays = {72340172838076926ull, 144680345676153597ull, 289360691352306939ull, 578721382704613623ull, 1157442765409226991ull, 2314885530818453727ull, 4629771061636907199ull, 9259542123273814143ull, 72340172838141441ull, 144680345676217602ull, 289360691352369924ull, 578721382704674568ull, 1157442765409283856ull, 2314885530818502432ull, 4629771061636939584ull, 9259542123273813888ull, 72340172854657281ull, 144680345692602882ull, 289360691368494084ull, 578721382720276488ull, 1157442765423841296ull, 2314885530830970912ull, 4629771061645230144ull, 9259542123273748608ull, 72340177082712321ull, 144680349887234562ull, 289360695496279044ull, 578721386714368008ull, 1157442769150545936ull, 2314885534022901792ull, 4629771063767613504ull, 9259542123257036928ull, 72341259464802561ull, 144681423712944642ull, 289361752209228804ull, 578722409201797128ull, 1157443723186933776ull, 2314886351157207072ull, 4629771607097753664ull, 9259542118978846848ull, 72618349279904001ull, 144956323094725122ull, 289632270724367364ull, 578984165983651848ull, 1157687956502220816ull, 2315095537539358752ull, 4629910699613634624ull, 9259541023762186368ull, 143553341945872641ull, 215330564830528002ull, 358885010599838724ull, 645993902138460168ull, 1220211685215703056ull, 2368647251370188832ull, 4665518383679160384ull, 9259260648297103488ull, 18302911464433844481ull, 18231136449196065282ull, 18087586418720506884ull, 17800486357769390088ull, 17226286235867156496ull, 16077885992062689312ull, 13781085504453754944ull, 9187484529235886208ull};
    inline constexpr std::array<uint64_t, 64> bishop_full_rays = {9241421688590303744ull, 36099303471056128ull, 141012904249856ull, 550848566272ull, 6480472064ull, 1108177604608ull, 283691315142656ull, 72624976668147712ull, 4620710844295151618ull, 9241421688590368773ull, 36099303487963146ull, 141017232965652ull, 1659000848424ull, 283693466779728ull, 72624976676520096ull, 145249953336262720ull, 2310355422147510788ull, 4620710844311799048ull, 9241421692918565393ull, 36100411639206946ull, 424704217196612ull, 72625527495610504ull, 145249955479592976ull, 290499906664153120ull, 1155177711057110024ull, 2310355426409252880ull, 4620711952330133792ull, 9241705379636978241ull, 108724279602332802ull, 145390965166737412ull, 290500455356698632ull, 580999811184992272ull, 577588851267340304ull, 1155178802063085600ull, 2310639079102947392ull, 4693335752243822976ull, 9386671504487645697ull, 326598935265674242ull, 581140276476643332ull, 1161999073681608712ull, 288793334762704928ull, 577868148797087808ull, 1227793891648880768ull, 2455587783297826816ull, 4911175566595588352ull, 9822351133174399489ull, 1197958188344280066ull, 2323857683139004420ull, 144117404414255168ull, 360293502378066048ull, 720587009051099136ull, 1441174018118909952ull, 2882348036221108224ull, 5764696068147249408ull, 11529391036782871041ull, 4611756524879479810ull, 567382630219904ull, 1416240237150208ull, 2833579985862656ull, 5667164249915392ull, 11334324221640704ull, 22667548931719168ull, 45053622886727936ull, 18049651735527937ull};
    inline constexpr std::array<uint64_t, 64> queen_full_rays = []()
    {
        std::array<uint64_t, 64> result{};
        for (size_t i = 0; i < 64; ++i)
        {
            result[i] = rook_full_rays[i] | bishop_full_rays[i];
        }
        return result;
    }();

    constexpr std::array<std::array<uint64_t, 64>, 64> elementWiseOr(
        const std::array<std::array<uint64_t, 64>, 64> &table1,
        const std::array<std::array<uint64_t, 64>, 64> &table2)
    {

        std::array<std::array<uint64_t, 64>, 64> result{};
        for (size_t i = 0; i < 64; ++i)
        {
            for (size_t j = 0; j < 64; ++j)
            {
                result[i][j] = table1[i][j] | table2[i][j];
            }
        }
        return result;
    }

    // Bitboards of rays from square_1 to square_2, excluding square_1 and excluding square_2
    inline constexpr std::array<std::array<uint64_t, 64>, 64> precomputedBishopMovesTableOneBlocker {getBishopOneBlockerTable()};
    inline constexpr std::array<std::array<uint64_t, 64>, 64> precomputedRookMovesTableOneBlocker {getRookOneBlockerTable()};
    inline constexpr std::array<std::array<uint64_t, 64>, 64> precomputedQueenMovesTableOneBlocker{elementWiseOr(getRookOneBlockerTable(), getBishopOneBlockerTable())};

    // Bitboards of rays from square_1 to square_2, excluding square_1 and including square_2 (for direct checks)
    inline constexpr std::array<std::array<uint64_t, 64>, 64> precomputedBishopMovesTableOneBlocker2 {getBishopOneBlockerTable2()};
    inline constexpr std::array<std::array<uint64_t, 64>, 64> precomputedRookMovesTableOneBlocker2 {getRookOneBlockerTable2()};
    inline constexpr std::array<std::array<uint64_t, 64>, 64> precomputedQueenMovesTableOneBlocker2{elementWiseOr(getRookOneBlockerTable2(), getBishopOneBlockerTable2())};

    // Bitboards of full line (8 squares) containing squares, otherwise 0 
    inline constexpr std::array<std::array<uint64_t, 64>, 64> OnLineBitboards{getOnLineBitboardsTable()};

    // Bitboards of full line (8 squares) containing squares, otherwise full bitboard (for discovered checks)
    inline constexpr std::array<std::array<uint64_t, 64>, 64> OnLineBitboards2{getOnLineBitboardsTable2()};
}
#endif