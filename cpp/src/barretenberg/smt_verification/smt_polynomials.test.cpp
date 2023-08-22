#include "barretenberg/crypto/generators/generator_data.hpp"
#include "barretenberg/crypto/pedersen_commitment/pedersen.hpp"
#include "barretenberg/proof_system/circuit_builder/standard_circuit_builder.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#include "barretenberg/stdlib/primitives/field/field.hpp"

#include "barretenberg/serialize/cbind.hpp"
#include "barretenberg/smt_verification/circuit/circuit.hpp"

using namespace barretenberg;
using namespace proof_system;
using namespace smt_circuit;

using field_ct = proof_system::plonk::stdlib::field_t<StandardCircuitBuilder>;
using witness_t = proof_system::plonk::stdlib::witness_t<StandardCircuitBuilder>;
using pub_witness_t = proof_system::plonk::stdlib::public_witness_t<StandardCircuitBuilder>;

namespace {
auto& engine = numeric::random::get_debug_engine();
}

msgpack::sbuffer polynomial_evaluation_circuit(size_t n, bool pub_coeffs)
{
    StandardCircuitBuilder builder = StandardCircuitBuilder();
    std::vector<fr> coeffs;
    std::vector<uint32_t> idxs;
    for (size_t i = 0; i < n; i++) {
        fr tmp_coeff = fr::random_element();
        uint32_t idx;
        if (pub_coeffs) {
            idx = builder.add_public_variable(tmp_coeff);
        } else {
            idx = builder.add_variable(tmp_coeff);
        }
        idxs.push_back(idx);
        coeffs.push_back(tmp_coeff);
        builder.set_variable_name(idx, "coeff_" + std::to_string(i));
    }

    fr z(10);
    uint32_t z_idx = builder.add_variable(z);
    builder.set_variable_name(z_idx, "point");
    fr res = fr::zero();
    uint32_t res_idx = builder.zero_idx; // i think assert_equal was needed for zero initialization
    builder.assert_equal(res_idx, 0);

    for (size_t i = 0; i < n; i++) {
        res = res * z;
        uint32_t mul_idx = builder.add_variable(res);
        // builder.set_variable_name(mul_idx, "mul_" + std::to_string(i));
        builder.create_mul_gate({ res_idx, z_idx, mul_idx, fr::one(), fr::neg_one(), fr::zero() });

        res = res + coeffs[i];
        uint32_t add_idx = builder.add_variable(res);
        builder.create_add_gate({
            mul_idx,
            idxs[i],
            add_idx,
            fr::one(),
            fr::one(),
            fr::neg_one(),
            fr::zero(),
        });

        res_idx = add_idx;
    }
    builder.set_variable_name(res_idx, "result");

    info("evaluation at point ", z, ": ", res);
    info("gates: ", builder.num_gates);
    info("variables: ", builder.get_num_variables());
    info("public inputs: ", builder.get_num_public_inputs());

    return builder.export_circuit();
}

const std::string r = "21888242871839275222246405745257275088548364400416034343698204186575808495617";
const std::string q = "21888242871839275222246405745257275088696311157297823662689037894645226208583";
const std::string r_hex = "30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001";
const std::string q_hex = "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47";

FFTerm polynomial_evaluation(Circuit& c, size_t n, bool is_correct = true)
{
    std::vector<smt_terms::FFTerm> coeffs(n);
    for (size_t i = 0; i < n; i++) {
        coeffs[i] = c["coeff_" + std::to_string(i)];
    }

    FFTerm point = c["point"];
    FFTerm result = c["result"];

    FFTerm ev = is_correct ? c["zero"] : c["one"];
    for (size_t i = 0; i < n; i++) {
        ev = ev * point + coeffs[i];
    }

    result != ev;
    return ev;
}

void model_variables(Circuit& c, Solver* s, FFTerm& evaluation)
{
    std::unordered_map<std::string, cvc5::Term> terms;
    terms.insert({ "point", c["point"] });
    terms.insert({ "result", c["result"] });
    terms.insert({ "evaluation", evaluation });

    auto values = s->model(terms);

    info("point = ", values["point"]);
    info("circuit_result = ", values["result"]);
    info("function_evaluation = ", values["evaluation"]);
}

TEST(polynomial_evaluation, correct)
{
    size_t n = 30;
    auto buf = create_circuit(n, true);

    CircuitSchema circuit_info = unpack_from_buffer(buf);

    Solver s(r, true, 10);
    Circuit circuit(circuit_info, &s);
    FFTerm ev = polynomial_evaluation(circuit, n, true);

    auto start = std::chrono::high_resolution_clock::now();
    bool res = s.check();
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

    ASSERT_FALSE(res);
    info();
    info("Gates: ", circuit.get_num_gates());
    info("Result: ", s.getResult());
    info("Time elapsed: ", static_cast<double>(duration.count()) / 1e6, " sec");
}

TEST(polynomial_evaluation, incorrect)
{
    size_t n = 30;
    auto buf = create_circuit(n, true);

    CircuitSchema circuit_info = unpack_from_buffer(buf);

    Solver s(r, true, 10);
    Circuit circuit(circuit_info, &s);
    FFTerm ev = polynomial_evaluation(circuit, n, false);

    auto start = std::chrono::high_resolution_clock::now();
    bool res = s.check();
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

    ASSERT_FALSE(res);
    info();
    info("Gates: ", circuit.get_num_gates());
    info("Result: ", s.getResult());
    info("Time elapsed: ", static_cast<double>(duration.count()) / 1e6, " sec");

    if (res) {
        model_variables(circuit, &s, ev);
    }
}
}