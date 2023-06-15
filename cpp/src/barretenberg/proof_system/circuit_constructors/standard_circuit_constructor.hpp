#pragma once
#include <array>
#include "barretenberg/ecc/curves/bn254/bn254.hpp"
#include "barretenberg/ecc/curves/grumpkin/grumpkin.hpp"
#include "circuit_constructor_base.hpp"
#include "barretenberg/proof_system/types/composer_type.hpp"
#include "barretenberg/proof_system/types/merkle_hash_type.hpp"
#include "barretenberg/proof_system/types/pedersen_commitment_type.hpp"

namespace proof_system {
inline std::vector<std::string> standard_selector_names()
{
    std::vector<std::string> result{ "q_m", "q_1", "q_2", "q_3", "q_c" };
    return result;
}

template <typename Curve>
class StandardCircuitConstructor_ : public CircuitConstructorBase<arithmetization::Standard<Curve>> {
  public:
    using FF = typename Curve::ScalarField;
    static constexpr ComposerType type = ComposerType::STANDARD;
    static constexpr merkle::HashType merkle_hash_type = merkle::HashType::FIXED_BASE_PEDERSEN;
    static constexpr pedersen::CommitmentType commitment_type = pedersen::CommitmentType::FIXED_BASE_PEDERSEN;

    using WireVector = std::vector<uint32_t, barretenberg::ContainerSlabAllocator<uint32_t>>;
    using SelectorVector = std::vector<FF, barretenberg::ContainerSlabAllocator<FF>>;

    WireVector& w_l = std::get<0>(this->wires);
    WireVector& w_r = std::get<1>(this->wires);
    WireVector& w_o = std::get<2>(this->wires);

    SelectorVector& q_m = this->selectors.q_m;
    SelectorVector& q_1 = this->selectors.q_1;
    SelectorVector& q_2 = this->selectors.q_2;
    SelectorVector& q_3 = this->selectors.q_3;
    SelectorVector& q_c = this->selectors.q_c;

    static constexpr size_t UINT_LOG2_BASE = 2;

    // These are variables that we have used a gate on, to enforce that they are
    // equal to a defined value.
    // TODO(#216)(Adrian): Why is this not in CircuitConstructorBase
    std::map<FF, uint32_t> constant_variable_indices;

    StandardCircuitConstructor_(const size_t size_hint = 0)
        : CircuitConstructorBase<Curve>(standard_selector_names(), size_hint)
    {
        w_l.reserve(size_hint);
        w_r.reserve(size_hint);
        w_o.reserve(size_hint);
        // To effieciently constrain wires to zero, we set the first value of w_1 to be 0, and use copy constraints for
        // all future zero values.
        // (#216)(Adrian): This should be done in a constant way, maybe by initializing the constant_variable_indices
        // map
        this->zero_idx = put_constant_variable(FF::zero());
        // TODO(#217)(Cody): Ensure that no polynomial is ever zero. Maybe there's a better way.
        this->one_idx = put_constant_variable(FF::one());
        // 1 * 1 * 1 + 1 * 1 + 1 * 1 + 1 * 1 + -4
        // m           l       r       o        c
        create_poly_gate({ this->one_idx, this->one_idx, this->one_idx, 1, 1, 1, 1, -4 });
    };
    // This constructor is needed to simplify switching between circuit constructor and composer
    StandardCircuitConstructor_(std::string const&, const size_t size_hint = 0)
        : StandardCircuitConstructor_(size_hint){};
    StandardCircuitConstructor_(const StandardCircuitConstructor_& other) = delete;
    StandardCircuitConstructor_(StandardCircuitConstructor_&& other) = default;
    StandardCircuitConstructor_& operator=(const StandardCircuitConstructor_& other) = delete;
    StandardCircuitConstructor_& operator=(StandardCircuitConstructor_&& other)
    {
        CircuitConstructorBase<arithmetization::Standard<FF>>::operator=(std::move(other));
        constant_variable_indices = other.constant_variable_indices;
        return *this;
    };
    ~StandardCircuitConstructor_() override = default;

    void assert_equal_constant(uint32_t const a_idx, FF const& b, std::string const& msg = "assert equal constant");

    void create_add_gate(const add_triple& in) override;
    void create_mul_gate(const mul_triple& in) override;
    void create_bool_gate(const uint32_t a) override;
    void create_poly_gate(const poly_triple& in) override;
    void create_big_add_gate(const add_quad& in);
    void create_big_add_gate_with_bit_extraction(const add_quad& in);
    void create_big_mul_gate(const mul_quad& in);
    void create_balanced_add_gate(const add_quad& in);
    void create_fixed_group_add_gate(const fixed_group_add_quad& in);
    void create_fixed_group_add_gate_with_init(const fixed_group_add_quad& in, const fixed_group_init_quad& init);
    void create_fixed_group_add_gate_final(const add_quad& in);

    fixed_group_add_quad previous_add_quad;

    // TODO(#216)(Adrian): This should be a virtual overridable method in the base class.
    void fix_witness(const uint32_t witness_index, const FF& witness_value);

    std::vector<uint32_t> decompose_into_base4_accumulators(const uint32_t witness_index,
                                                            const size_t num_bits,
                                                            std::string const& msg = "create_range_constraint");

    void create_range_constraint(const uint32_t variable_index,
                                 const size_t num_bits,
                                 std::string const& msg = "create_range_constraint")
    {
        decompose_into_base4_accumulators(variable_index, num_bits, msg);
    }

    accumulator_triple create_logic_constraint(const uint32_t a,
                                               const uint32_t b,
                                               const size_t num_bits,
                                               bool is_xor_gate);
    accumulator_triple create_and_constraint(const uint32_t a, const uint32_t b, const size_t num_bits);
    accumulator_triple create_xor_constraint(const uint32_t a, const uint32_t b, const size_t num_bits);

    // TODO(#216)(Adrian): The 2 following methods should be virtual in the base class
    uint32_t put_constant_variable(const FF& variable);

    size_t get_num_constant_gates() const override { return 0; }

    bool check_circuit();
};

using StandardCircuitConstructor = StandardCircuitConstructor_<curve::BN254>;
// using StandardGrumpkinCircuitConstructor = StandardCircuitConstructor__<grumpkin::fr>;
} // namespace proof_system
