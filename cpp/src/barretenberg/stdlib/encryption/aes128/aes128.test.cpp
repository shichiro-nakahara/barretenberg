#include "aes128.hpp"
#include "barretenberg/crypto/aes128/aes128.hpp"
#include "barretenberg/proof_system/circuit_builder/ultra_circuit_builder.hpp"

#include <gtest/gtest.h>

using namespace bb;

TEST(stdlib_aes128, encrypt_64_bytes)
{
    typedef stdlib::field_t<bb::UltraCircuitBuilder> field_pt;
    typedef stdlib::witness_t<bb::UltraCircuitBuilder> witness_pt;

    uint8_t key[16]{ 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };
    uint8_t out[64]{ 0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46, 0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
                     0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee, 0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
                     0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b, 0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16,
                     0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09, 0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7 };
    uint8_t iv[16]{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
    uint8_t in[64]{ 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
                    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
                    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
                    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 };

    const auto convert_bytes = [](uint8_t* data) {
        uint256_t converted(0);
        for (uint64_t i = 0; i < 16; ++i) {
            uint256_t to_add = uint256_t((uint64_t)(data[i])) << uint256_t((15 - i) * 8);
            converted += to_add;
        }
        return converted;
    };

    auto builder = bb::UltraCircuitBuilder();

    std::vector<field_pt> in_field{
        witness_pt(&builder, fr(convert_bytes(in))),
        witness_pt(&builder, fr(convert_bytes(in + 16))),
        witness_pt(&builder, fr(convert_bytes(in + 32))),
        witness_pt(&builder, fr(convert_bytes(in + 48))),
    };

    field_pt key_field(witness_pt(&builder, fr(convert_bytes(key))));
    field_pt iv_field(witness_pt(&builder, fr(convert_bytes(iv))));

    std::vector<fr> expected{
        convert_bytes(out), convert_bytes(out + 16), convert_bytes(out + 32), convert_bytes(out + 48)
    };

    const auto result = stdlib::aes128::encrypt_buffer_cbc(in_field, iv_field, key_field);

    for (size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(result[i].get_value(), expected[i]);
    }

    std::cout << "num gates = " << builder.get_num_gates() << std::endl;

    bool proof_result = builder.check_circuit();
    EXPECT_EQ(proof_result, true);
}
