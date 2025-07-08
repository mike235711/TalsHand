#include <catch2/catch_test_macros.hpp>
#include "bit_utils.h"

    TEST_CASE("getLeastSignificantBitIndex works")
{
    REQUIRE(getLeastSignificantBitIndex(0b1000ULL) == 3);
    REQUIRE(getLeastSignificantBitIndex(0) == 65);
}