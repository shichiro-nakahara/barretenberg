#pragma once
#include "circuit_constructor_base.hpp"
#include <plonk/proof_system/constants.hpp>
#include <proof_system/flavor/flavor.hpp>

namespace honk {
enum TurboSelectors { QM, QC, Q1, Q2, Q3, Q4, Q5, QARITH, QFIXED, QRANGE, QLOGIC, NUM };

inline std::vector<std::string> turbo_selector_names()
{
    const std::vector<std::string> result{ "q_m", "q_c",     "q_1",          "q_2",     "q_3",    "q_4",
                                           "q_5", "q_arith", "q_fixed_base", "q_range", "q_logic" };
    return result;
}

class TurboCircuitConstructor : public CircuitConstructorBase<TURBO_WIDTH> {
  public:
    // TODO: replace this with Honk enums after we have a verifier and no longer depend on plonk prover/verifier
    static constexpr waffle::ComposerType type = waffle::ComposerType::STANDARD_HONK;
    static constexpr size_t UINT_LOG2_BASE = 2;

    // These are variables that we have used a gate on, to enforce that they are
    // equal to a defined value.
    // TODO(Adrian): Why is this not in CircuitConstructorBase
    std::map<barretenberg::fr, uint32_t> constant_variable_indices;

    TurboCircuitConstructor(const size_t size_hint = 0)
        : CircuitConstructorBase(turbo_selector_names(), TurboSelectors::NUM, size_hint)
    {
        w_l.reserve(size_hint);
        w_r.reserve(size_hint);
        w_o.reserve(size_hint);
        w_4.reserve(size_hint);
        // To effieciently constrain wires to zero, we set the first value of w_1 to be 0, and use copy constraints for
        // all future zero values.
        // TODO(Adrian): This should be done in a constant way, maybe by initializing the constant_variable_indices map
        zero_idx = put_constant_variable(barretenberg::fr::zero());
        // TODO(Cody): Ensure that no polynomial is ever zero. Maybe there's a better way.
        one_idx = put_constant_variable(barretenberg::fr::one());
        // 1 * 1 * 1 + 1 * 1 + 1 * 1 + 1 * 1 + -4
        // m           l       r       o        c
        create_poly_gate({ one_idx, one_idx, one_idx, 1, 1, 1, 1, -4 });
    };

    TurboCircuitConstructor(const TurboCircuitConstructor& other) = delete;
    TurboCircuitConstructor(TurboCircuitConstructor&& other) = default;
    TurboCircuitConstructor& operator=(const TurboCircuitConstructor& other) = delete;
    TurboCircuitConstructor& operator=(TurboCircuitConstructor&& other) = default;
    ~TurboCircuitConstructor() override = default;

    void assert_equal_constant(uint32_t const a_idx,
                               barretenberg::fr const& b,
                               std::string const& msg = "assert equal constant");

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

    // TODO(Adrian): This should be a virtual overridable method in the base class.
    void fix_witness(const uint32_t witness_index, const barretenberg::fr& witness_value);

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

    // TODO(Adrian): The 2 following methods should be virtual in the base class
    uint32_t put_constant_variable(const barretenberg::fr& variable);

    size_t get_num_constant_gates() const override { return 0; }

    bool check_circuit();
};

/**
 * CheckGetter class is used to evaluate widget operations for circuit_checking
 *
 * */
class CheckGetter {
  public:
    static constexpr barretenberg::fr random_value = barretenberg::fr(0xdead);
    static constexpr barretenberg::fr zero = barretenberg::fr::zero();

    /**
     * Get a reference to a value of a witness/selector
     *
     * @param composer Composer object
     * @param index Index of the value in polynomial (array)
     *
     * @tparam value_type Controls if we shift index to the right or not
     * @tparam id The id of the selector/witness polynomial being used
     * */
    template <waffle::EvaluationType value_type, waffle::PolynomialIndex id>
    inline static const barretenberg::fr& get_value(const TurboCircuitConstructor& composer, const size_t index = 0)
    {
        size_t actual_index = index;
        if constexpr (waffle::EvaluationType::SHIFTED == value_type) {
            actual_index += 1;
            if (actual_index >= composer.num_gates) {
                return zero;
            }
        }
        switch (id) {
        case waffle::PolynomialIndex::Q_1:
            return composer.selectors[TurboSelectors::Q1][actual_index];
        case waffle::PolynomialIndex::Q_2:
            return composer.selectors[TurboSelectors::Q2][actual_index];
        case waffle::PolynomialIndex::Q_3:
            return composer.selectors[TurboSelectors::Q3][actual_index];
        case waffle::PolynomialIndex::Q_4:
            return composer.selectors[TurboSelectors::Q4][actual_index];
        case waffle::PolynomialIndex::Q_5:
            return composer.selectors[TurboSelectors::Q5][actual_index];
        case waffle::PolynomialIndex::Q_M:
            return composer.selectors[TurboSelectors::QM][actual_index];
        case waffle::PolynomialIndex::Q_C:
            return composer.selectors[TurboSelectors::QC][actual_index];
        case waffle::PolynomialIndex::Q_ARITHMETIC:
            return composer.selectors[TurboSelectors::QARITH][actual_index];
        case waffle::PolynomialIndex::Q_LOGIC:
            return composer.selectors[TurboSelectors::QLOGIC][actual_index];
        case waffle::PolynomialIndex::Q_RANGE:
            return composer.selectors[TurboSelectors::QRANGE][actual_index];
        case waffle::PolynomialIndex::Q_FIXED_BASE:
            return composer.selectors[TurboSelectors::QFIXED][actual_index];
        case waffle::PolynomialIndex::W_1:
            return composer.get_variable_reference(composer.w_l[actual_index]);
        case waffle::PolynomialIndex::W_2:
            return composer.get_variable_reference(composer.w_r[actual_index]);
        case waffle::PolynomialIndex::W_3:
            return composer.get_variable_reference(composer.w_o[actual_index]);
        case waffle::PolynomialIndex::W_4:
            return composer.get_variable_reference(composer.w_4[actual_index]);
        }
        return CheckGetter::random_value;
    }
};
} // namespace honk
