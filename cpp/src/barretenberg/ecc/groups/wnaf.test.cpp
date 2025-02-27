#include "wnaf.hpp"
#include "../curves/bn254/fr.hpp"
#include "barretenberg/numeric/random/engine.hpp"
#include <gtest/gtest.h>

using namespace bb;

namespace {
auto& engine = numeric::random::get_debug_engine();
}

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
namespace {

void recover_fixed_wnaf(const uint64_t* wnaf, bool skew, uint64_t& hi, uint64_t& lo, size_t wnaf_bits)
{
    size_t wnaf_entries = (127 + wnaf_bits - 1) / wnaf_bits;
    uint128_t scalar = 0; // (uint128_t)(skew);
    for (int i = 0; i < static_cast<int>(wnaf_entries); ++i) {
        uint64_t entry_formatted = wnaf[static_cast<size_t>(i)];
        bool negative = (entry_formatted >> 31) != 0U;
        uint64_t entry = ((entry_formatted & 0x0fffffffU) << 1) + 1;
        if (negative) {
            scalar -= (static_cast<uint128_t>(entry))
                      << static_cast<uint128_t>(wnaf_bits * (wnaf_entries - 1 - static_cast<size_t>(i)));
        } else {
            scalar += (static_cast<uint128_t>(entry))
                      << static_cast<uint128_t>(wnaf_bits * (wnaf_entries - 1 - static_cast<size_t>(i)));
        }
    }
    scalar -= static_cast<uint128_t>(skew);
    hi = static_cast<uint64_t>(scalar >> static_cast<uint128_t>(64));
    lo = static_cast<uint64_t>(static_cast<uint128_t>(scalar & static_cast<uint128_t>(0xffff'ffff'ffff'ffff)));
}
} // namespace

TEST(wnaf, WnafZero)
{
    uint64_t buffer[2]{ 0, 0 };
    uint64_t wnaf[WNAF_SIZE(5)] = { 0 };
    bool skew = false;
    wnaf::fixed_wnaf<1, 5>(buffer, wnaf, skew, 0);
    uint64_t recovered_hi = 0;
    uint64_t recovered_lo = 0;
    recover_fixed_wnaf(wnaf, skew, recovered_hi, recovered_lo, 5);
    EXPECT_EQ(recovered_lo, 0UL);
    EXPECT_EQ(recovered_hi, 0UL);
    EXPECT_EQ(buffer[0], recovered_lo);
    EXPECT_EQ(buffer[1], recovered_hi);
}

TEST(wnaf, WnafTwoBitWindow)
{
    /**
     * We compute the 2-bit windowed NAF form of `input`.
     */
    uint256_t input = fr::random_element();
    constexpr uint32_t window = 2;
    constexpr uint32_t num_bits = 254;
    constexpr uint32_t num_quads = (num_bits >> 1) + 1;
    uint64_t wnaf[num_quads] = { 0 };
    bool skew = false;
    bb::wnaf::fixed_wnaf<256, 1, window>(&input.data[0], wnaf, skew, 0);

    /**
     * For representing even numbers, we define a skew:
     *
     *        / false   if input is odd
     * skew = |
     *        \ true    if input is even
     *
     * The i-th quad value is defined as:
     *
     *        / -(2b + 1)   if sign = 1
     * q[i] = |
     *        \ (2b + 1)    if sign = 0
     *
     * where sign = ((wnaf[i] >> 31) == 0) and b = (wnaf[i] & 1).
     * We can compute back the original number from the quads as:
     *                127
     *               -----
     *               \
     * R = -skew  +  |    4^{127 - i} . q[i].
     *               /
     *               -----
     *                i=0
     *
     */
    uint256_t recovered = 0;
    uint256_t four_power = (uint256_t(1) << num_bits);
    for (uint64_t i : wnaf) {
        int extracted = 2 * (static_cast<int>(i) & 1) + 1;
        bool sign = (i >> 31) == 0;

        if (sign) {
            recovered += uint256_t(static_cast<uint64_t>(extracted)) * four_power;
        } else {
            recovered -= uint256_t(static_cast<uint64_t>(extracted)) * four_power;
        }
        four_power >>= 2;
    }
    recovered -= static_cast<uint64_t>(skew);
    EXPECT_EQ(recovered, input);
}

TEST(wnaf, WnafFixed)
{
    uint256_t buffer = engine.get_random_uint256();
    buffer.data[1] &= 0x7fffffffffffffffUL;
    uint64_t wnaf[WNAF_SIZE(5)] = { 0 };
    bool skew = false;
    wnaf::fixed_wnaf<1, 5>(&buffer.data[0], wnaf, skew, 0);
    uint64_t recovered_hi = 0;
    uint64_t recovered_lo = 0;
    recover_fixed_wnaf(wnaf, skew, recovered_hi, recovered_lo, 5);
    EXPECT_EQ(buffer.data[0], recovered_lo);
    EXPECT_EQ(buffer.data[1], recovered_hi);
}

TEST(wnaf, WnafFixedSimpleLo)
{
    uint64_t rand_buffer[2]{ 1, 0 };
    uint64_t wnaf[WNAF_SIZE(5)]{ 0 };
    bool skew = false;
    wnaf::fixed_wnaf<1, 5>(rand_buffer, wnaf, skew, 0);
    uint64_t recovered_hi = 0;
    uint64_t recovered_lo = 0;
    recover_fixed_wnaf(wnaf, skew, recovered_hi, recovered_lo, 5);
    EXPECT_EQ(rand_buffer[0], recovered_lo);
    EXPECT_EQ(rand_buffer[1], recovered_hi);
}

TEST(wnaf, WnafFixedSimpleHi)
{
    uint64_t rand_buffer[2] = { 0, 1 };
    uint64_t wnaf[WNAF_SIZE(5)] = { 0 };
    bool skew = false;
    wnaf::fixed_wnaf<1, 5>(rand_buffer, wnaf, skew, 0);
    uint64_t recovered_hi = 0;
    uint64_t recovered_lo = 0;
    recover_fixed_wnaf(wnaf, skew, recovered_hi, recovered_lo, 5);
    EXPECT_EQ(rand_buffer[0], recovered_lo);
    EXPECT_EQ(rand_buffer[1], recovered_hi);
}

TEST(wnaf, WnafFixedWithEndoSplit)
{
    fr k = engine.get_random_uint256();
    k.data[3] &= 0x0fffffffffffffffUL;

    fr k1{ 0, 0, 0, 0 };
    fr k2{ 0, 0, 0, 0 };

    fr::split_into_endomorphism_scalars(k, k1, k2);
    uint64_t wnaf[WNAF_SIZE(5)] = { 0 };
    uint64_t endo_wnaf[WNAF_SIZE(5)] = { 0 };
    bool skew = false;
    bool endo_skew = false;
    wnaf::fixed_wnaf<1, 5>(&k1.data[0], wnaf, skew, 0);
    wnaf::fixed_wnaf<1, 5>(&k2.data[0], endo_wnaf, endo_skew, 0);

    fr k1_recovered{ 0, 0, 0, 0 };
    fr k2_recovered{ 0, 0, 0, 0 };

    recover_fixed_wnaf(wnaf, skew, k1_recovered.data[1], k1_recovered.data[0], 5);
    recover_fixed_wnaf(endo_wnaf, endo_skew, k2_recovered.data[1], k2_recovered.data[0], 5);

    fr result;
    fr lambda = fr::cube_root_of_unity();
    result = k2_recovered * lambda;
    result = k1_recovered - result;

    EXPECT_EQ(result, k);
}

// NOLINTEND(cppcoreguidelines-avoid-c-arrays)
