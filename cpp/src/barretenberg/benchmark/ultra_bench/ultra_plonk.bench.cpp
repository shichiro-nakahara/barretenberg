#include "barretenberg/benchmark/ultra_bench/benchmark_utilities.hpp"
#include "barretenberg/plonk/composer/ultra_composer.hpp"
#include "barretenberg/proof_system/circuit_builder/ultra_circuit_builder.hpp"

using namespace benchmark;
using namespace bb;

/**
 * @brief Benchmark: Construction of a Ultra Plonk proof for a circuit determined by the provided circuit function
 */
static void construct_proof_ultraplonk(State& state,
                                       void (*test_circuit_function)(UltraCircuitBuilder&, size_t)) noexcept
{
    size_t num_iterations = 10; // 10x the circuit
    bench_utils::construct_proof_with_specified_num_iterations<plonk::UltraComposer>(
        state, test_circuit_function, num_iterations);
}

/**
 * @brief Benchmark: Construction of a Ultra Plonk proof with 2**n gates
 */
static void construct_proof_ultraplonk_power_of_2(State& state) noexcept
{
    auto log2_of_gates = static_cast<size_t>(state.range(0));
    bench_utils::construct_proof_with_specified_num_iterations<plonk::UltraComposer>(
        state, &bench_utils::generate_basic_arithmetic_circuit<UltraCircuitBuilder>, log2_of_gates);
}

// Define benchmarks
BENCHMARK_CAPTURE(construct_proof_ultraplonk, sha256, &bench_utils::generate_sha256_test_circuit<UltraCircuitBuilder>)
    ->Unit(kMillisecond);
BENCHMARK_CAPTURE(construct_proof_ultraplonk, keccak, &bench_utils::generate_keccak_test_circuit<UltraCircuitBuilder>)
    ->Unit(kMillisecond);
BENCHMARK_CAPTURE(construct_proof_ultraplonk,
                  ecdsa_verification,
                  &bench_utils::generate_ecdsa_verification_test_circuit<UltraCircuitBuilder>)
    ->Unit(kMillisecond);
BENCHMARK_CAPTURE(construct_proof_ultraplonk,
                  merkle_membership,
                  &bench_utils::generate_merkle_membership_test_circuit<UltraCircuitBuilder>)
    ->Unit(kMillisecond);

BENCHMARK(construct_proof_ultraplonk_power_of_2)
    // 2**15 gates to 2**20 gates
    ->DenseRange(15, 20)
    ->Unit(kMillisecond);