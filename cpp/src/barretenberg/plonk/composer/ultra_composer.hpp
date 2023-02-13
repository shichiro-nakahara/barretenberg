#pragma once
#include "composer_base.hpp"
#include "plookup_tables/plookup_tables.hpp"
#include <optional>

using namespace bonk;

namespace plonk {

class UltraComposer : public ComposerBase {

  public:
    static constexpr ComposerType type = ComposerType::PLOOKUP;
    static constexpr merkle::HashType merkle_hash_type = merkle::HashType::LOOKUP_PEDERSEN;
    static constexpr pedersen::CommitmentType commitment_type = pedersen::CommitmentType::FIXED_BASE_PEDERSEN;
    static constexpr size_t NUM_RESERVED_GATES = 4; // This must be >= num_roots_cut_out_of_vanishing_polynomial
                                                    // See the comment in plonk/proof_system/prover/prover.cpp
                                                    // ProverBase::compute_quotient_commitments() for why 4 exactly.
    static constexpr size_t UINT_LOG2_BASE = 6;     // DOCTODO: explain what this is, or rename.
    // The plookup range proof requires work linear in range size, thus cannot be used directly for
    // large ranges such as 2^64. For such ranges the element will be decomposed into smaller
    // chuncks according to the parameter below
    static constexpr size_t DEFAULT_PLOOKUP_RANGE_BITNUM = 14;
    static constexpr size_t DEFAULT_PLOOKUP_RANGE_STEP_SIZE = 3;
    static constexpr size_t DEFAULT_PLOOKUP_RANGE_SIZE = (1 << DEFAULT_PLOOKUP_RANGE_BITNUM) - 1;
    static constexpr size_t DEFAULT_NON_NATIVE_FIELD_LIMB_BITS = 68;
    static constexpr uint32_t UNINITIALIZED_MEMORY_RECORD = UINT32_MAX;

    struct non_native_field_witnesses {
        // first 4 array elements = limbs
        // 5th element = prime basis limb
        std::array<uint32_t, 5> a;
        std::array<uint32_t, 5> b;
        std::array<uint32_t, 5> q;
        std::array<uint32_t, 5> r;
        std::array<barretenberg::fr, 5> neg_modulus;
        barretenberg::fr modulus;
    };

    enum AUX_SELECTORS {
        NONE,
        LIMB_ACCUMULATE_1,
        LIMB_ACCUMULATE_2,
        NON_NATIVE_FIELD_1,
        NON_NATIVE_FIELD_2,
        NON_NATIVE_FIELD_3,
        RAM_CONSISTENCY_CHECK,
        ROM_CONSISTENCY_CHECK,
        RAM_TIMESTAMP_CHECK,
        ROM_READ,
        RAM_READ,
        RAM_WRITE,
    };

    struct RangeList {
        uint64_t target_range;
        uint32_t range_tag;
        uint32_t tau_tag;
        std::vector<uint32_t> variable_indices;
    };

    /**
     * @brief A ROM memory record that can be ordered
     *
     *
     */
    struct RomRecord {
        uint32_t index_witness = 0;
        uint32_t value_column1_witness = 0;
        uint32_t value_column2_witness = 0;
        uint32_t index = 0;
        uint32_t record_witness = 0;
        size_t gate_index = 0;
        bool operator<(const RomRecord& other) const { return index < other.index; }
    };

    /**
     * @brief A RAM memory record that can be ordered
     *
     *
     */
    struct RamRecord {
        enum AccessType {
            READ,
            WRITE,
        };
        uint32_t index_witness = 0;
        uint32_t timestamp_witness = 0;
        uint32_t value_witness = 0;
        uint32_t index = 0;
        uint32_t timestamp = 0;
        AccessType access_type = AccessType::READ; // read or write?
        uint32_t record_witness = 0;
        size_t gate_index = 0;
        bool operator<(const RamRecord& other) const
        {
            bool index_test = (index) < (other.index);
            return index_test || (index == other.index && timestamp < other.timestamp);
        }
    };

    /**
     * @brief Each ram array is an instance of memory transcript. It saves values and indexes for a particular memory
     * array
     *
     *
     */
    struct RamTranscript {
        // Represents the current state of the array. Elements are variable indices.
        // Every update requires a new entry in the `records` vector below.
        std::vector<uint32_t> state;

        // A vector of records, each of which contains:
        // - Witnesses for [index, timestamp, value, record]
        //   (record is initialized during the proof creation, and points to 0 until then)
        // - Index of the element in the `state` vector
        // - READ/WRITE flag
        // - Real timestamp value, initialized to the current `access_count`
        std::vector<RamRecord> records;

        // used for RAM records, to compute the timestamp when performing a read/write
        // Incremented at every init/read/write operation.
        size_t access_count = 0;
    };

    /**
     * @brief Each rom array is an instance of memory transcript. It saves values and indexes for a particular memory
     * array
     *
     *
     */
    struct RomTranscript {
        // Contains the value of each index of the array
        std::vector<std::array<uint32_t, 2>> state;

        // A vector of records, each of which contains:
        // + The constant witness with the index
        // + The value in the memory slot
        // + The actual index value
        std::vector<RomRecord> records;
    };

    enum UltraSelectors { QM, QC, Q1, Q2, Q3, Q4, QARITH, QFIXED, QSORT, QELLIPTIC, QAUX, QLOOKUPTYPE, NUM };

    UltraComposer();
    UltraComposer(std::string const& crs_path, const size_t size_hint = 0);
    UltraComposer(std::shared_ptr<ReferenceStringFactory> const& crs_factory, const size_t size_hint = 0);
    UltraComposer(std::shared_ptr<proving_key> const& p_key,
                  std::shared_ptr<verification_key> const& v_key,
                  size_t size_hint = 0);
    UltraComposer(UltraComposer&& other) = default;
    UltraComposer& operator=(UltraComposer&& other) = default;
    ~UltraComposer() {}

    std::shared_ptr<proving_key> compute_proving_key() override;
    std::shared_ptr<verification_key> compute_verification_key() override;
    void compute_witness() override;

    UltraProver create_prover();
    UltraVerifier create_verifier();

    UltraToStandardProver create_ultra_to_standard_prover();
    UltraToStandardVerifier create_ultra_to_standard_verifier();

    void create_add_gate(const add_triple& in) override;

    void create_big_add_gate(const add_quad& in, const bool use_next_gate_w_4 = false);
    void create_big_add_gate_with_bit_extraction(const add_quad& in);
    void create_big_mul_gate(const mul_quad& in);
    void create_balanced_add_gate(const add_quad& in);

    void create_mul_gate(const mul_triple& in) override;
    void create_bool_gate(const uint32_t a) override;
    void create_poly_gate(const poly_triple& in) override;
    void create_fixed_group_add_gate(const fixed_group_add_quad& in);
    void create_fixed_group_add_gate_with_init(const fixed_group_add_quad& in, const fixed_group_init_quad& init);
    void create_fixed_group_add_gate_final(const add_quad& in);

    void create_ecc_add_gate(const ecc_add_gate& in);

    void fix_witness(const uint32_t witness_index, const barretenberg::fr& witness_value);

    void add_recursive_proof(const std::vector<uint32_t>& proof_output_witness_indices)
    {
        if (contains_recursive_proof) {
            failure("added recursive proof when one already exists");
        }
        contains_recursive_proof = true;

        for (const auto& idx : proof_output_witness_indices) {
            set_public_input(idx);
            recursive_proof_public_input_indices.push_back((uint32_t)(public_inputs.size() - 1));
        }
    }

    void create_new_range_constraint(const uint32_t variable_index, const uint64_t target_range);
    void create_range_constraint(const uint32_t variable_index, const size_t num_bits, std::string const&)
    {
        if (num_bits <= DEFAULT_PLOOKUP_RANGE_BITNUM) {
            /**
             * N.B. if `variable_index` is not used in any arithmetic constraints, this will create an unsatisfiable
             *      circuit!
             *      this range constraint will increase the size of the 'sorted set' of range-constrained integers by 1.
             *      The 'non-sorted set' of range-constrained integers is a subset of the wire indices of all arithmetic
             *      gates. No arithemtic gate => size imbalance between sorted and non-sorted sets. Checking for this
             *      and throwing an error would require a refactor of the Composer to catelog all 'orphan' variables not
             *      assigned to gates.
             **/
            create_new_range_constraint(variable_index, 1ULL << num_bits);
        } else {
            decompose_into_default_range(variable_index, num_bits);
        }
    }

    accumulator_triple create_logic_constraint(const uint32_t a,
                                               const uint32_t b,
                                               const size_t num_bits,
                                               bool is_xor_gate);
    accumulator_triple create_and_constraint(const uint32_t a, const uint32_t b, const size_t num_bits);
    accumulator_triple create_xor_constraint(const uint32_t a, const uint32_t b, const size_t num_bits);

    uint32_t put_constant_variable(const barretenberg::fr& variable);

    size_t get_num_constant_gates() const override { return 0; }

    /**
     * @brief Get the final number of gates in a circuit, which consists of the sum of:
     * 1) Current number number of actual gates
     * 2) Number of public inputs, as we'll need to add a gate for each of them
     * 3) Number of Rom array-associated gates
     * 4) NUmber of range-list associated gates
     *
     *
     * @param count return arument, number of existing gates
     * @param rangecount return argument, extra gates due to range checks
     * @param romcount return argument, extra gates due to rom reads
     * @param ramcount return argument, extra gates due to ram read/writes
     */
    void get_num_gates_split_into_components(size_t& count,
                                             size_t& rangecount,
                                             size_t& romcount,
                                             size_t& ramcount) const
    {
        count = num_gates;
        rangecount = 0;
        romcount = 0;
        ramcount = 0;
        // each ROM gate adds +1 extra gate due to the rom reads being copied to a sorted list set
        for (size_t i = 0; i < rom_arrays.size(); ++i) {
            for (size_t j = 0; j < rom_arrays[i].state.size(); ++j) {
                if (rom_arrays[i].state[j][0] == UNINITIALIZED_MEMORY_RECORD) {
                    romcount += 2;
                }
            }
            romcount += (rom_arrays[i].records.size());
            romcount += 1; // we add an addition gate after procesing a rom array
        }

        constexpr size_t gate_width = ultra_settings::program_width;
        // each RAM gate adds +2 extra gates due to the ram reads being copied to a sorted list set,
        // as well as an extra gate to validate timestamps
        for (size_t i = 0; i < ram_arrays.size(); ++i) {
            for (size_t j = 0; j < ram_arrays[i].state.size(); ++j) {
                if (ram_arrays[i].state[j] == UNINITIALIZED_MEMORY_RECORD) {
                    ramcount += 2;
                }
            }
            ramcount += (ram_arrays[i].records.size() * 2);
            ramcount += 1; // we add an addition gate after procesing a ram array

            // there will be 'max_timestamp' number of range checks, need to calculate.
            const auto max_timestamp = ram_arrays[i].access_count - 1;

            // TODO: if a range check of length `max_timestamp` already exists, this will be innacurate!
            // TODO: fix this
            size_t padding = (gate_width - (max_timestamp % gate_width)) % gate_width;
            if (max_timestamp == gate_width)
                padding += gate_width;
            const size_t ram_range_check_list_size = max_timestamp + padding;

            size_t ram_range_check_gate_count = (ram_range_check_list_size / gate_width);
            ram_range_check_gate_count += 1; // we need to add 1 extra addition gates for every distinct range list

            ramcount += ram_range_check_gate_count;
        }
        for (const auto& list : range_lists) {
            auto list_size = list.second.variable_indices.size();
            size_t padding = (gate_width - (list.second.variable_indices.size() % gate_width)) % gate_width;
            if (list.second.variable_indices.size() == gate_width)
                padding += gate_width;
            list_size += padding;
            rangecount += (list_size / gate_width);
            rangecount += 1; // we need to add 1 extra addition gates for every distinct range list
        }
    }

    /**
     * @brief Get the final number of gates in a circuit, which consists of the sum of:
     * 1) Current number number of actual gates
     * 2) Number of public inputs, as we'll need to add a gate for each of them
     * 3) Number of Rom array-associated gates
     * 4) NUmber of range-list associated gates
     *
     * @return size_t
     */
    virtual size_t get_num_gates() const override
    {
        // if circuit finalised already added extra gates
        if (circuit_finalised) {
            return num_gates;
        }
        size_t count = 0;
        size_t rangecount = 0;
        size_t romcount = 0;
        size_t ramcount = 0;
        get_num_gates_split_into_components(count, rangecount, romcount, ramcount);
        return count + romcount + ramcount + rangecount;
    }

    virtual void print_num_gates() const override
    {

        size_t count = 0;
        size_t rangecount = 0;
        size_t romcount = 0;
        size_t ramcount = 0;
        size_t constant_rangecount = 0;

        size_t plookupcount = 0;

        get_num_gates_split_into_components(count, rangecount, romcount, ramcount);

        for (const auto& table : lookup_tables) {
            plookupcount += table.lookup_gates.size();
            count -= table.lookup_gates.size();
        }

        for (const auto& list : range_lists) {
            // rough estimate
            const auto constant_cost = static_cast<size_t>(list.second.target_range / 6);
            constant_rangecount += constant_cost;
            rangecount -= constant_cost;
        }
        size_t total = count + romcount + rangecount;
        std::cout << "gates = " << total << " (arith " << count << ", plookup " << plookupcount << ", rom " << romcount
                  << ", ram " << ramcount << romcount << ", range " << rangecount
                  << ", range table init cost = " << constant_rangecount << "), pubinp = " << public_inputs.size()
                  << std::endl;
    }

    void assert_equal_constant(const uint32_t a_idx,
                               const barretenberg::fr& b,
                               std::string const& msg = "assert equal constant")
    {
        if (variables[a_idx] != b && !failed()) {
            failure(msg);
        }
        auto b_idx = put_constant_variable(b);
        assert_equal(a_idx, b_idx, msg);
    }

    /**
     * Plookup Methods
     **/
    void add_table_column_selector_poly_to_proving_key(polynomial& small, const std::string& tag);
    void initialize_precomputed_table(
        const plookup::BasicTableId id,
        bool (*generator)(std::vector<barretenberg::fr>&,
                          std::vector<barretenberg::fr>&,
                          std::vector<barretenberg::fr>&),
        std::array<barretenberg::fr, 2> (*get_values_from_key)(const std::array<uint64_t, 2>));

    plookup::BasicTable& get_table(const plookup::BasicTableId id);
    plookup::MultiTable& create_table(const plookup::MultiTableId id);

    plookup::ReadData<uint32_t> create_gates_from_plookup_accumulators(
        const plookup::MultiTableId& id,
        const plookup::ReadData<barretenberg::fr>& read_values,
        const uint32_t key_a_index,
        std::optional<uint32_t> key_b_index = std::nullopt);

    /**
     * Generalized Permutation Methods
     **/
    std::vector<uint32_t> decompose_into_default_range(
        const uint32_t variable_index,
        const uint64_t num_bits,
        const uint64_t target_range_bitnum = DEFAULT_PLOOKUP_RANGE_BITNUM,
        std::string const& msg = "decompose_into_default_range");
    std::vector<uint32_t> decompose_into_default_range_better_for_oddlimbnum(
        const uint32_t variable_index,
        const size_t num_bits,
        std::string const& msg = "decompose_into_default_range_better_for_oddlimbnum");
    void create_dummy_constraints(const std::vector<uint32_t>& variable_index);
    void create_sort_constraint(const std::vector<uint32_t>& variable_index);
    void create_sort_constraint_with_edges(const std::vector<uint32_t>& variable_index,
                                           const barretenberg::fr&,
                                           const barretenberg::fr&);
    void assign_tag(const uint32_t variable_index, const uint32_t tag)
    {
        ASSERT(tag <= current_tag);
        ASSERT(real_variable_tags[real_variable_index[variable_index]] == DUMMY_TAG);
        real_variable_tags[real_variable_index[variable_index]] = tag;
    }

    uint32_t create_tag(const uint32_t tag_index, const uint32_t tau_index)
    {
        tau.insert({ tag_index, tau_index });
        current_tag++; // Why exactly?
        return current_tag;
    }

    uint32_t get_new_tag()
    {
        current_tag++;
        return current_tag;
    }

    RangeList create_range_list(const uint64_t target_range);
    void process_range_list(const RangeList& list);
    void process_range_lists();

    /**
     * Custom Gate Selectors
     **/
    void apply_aux_selectors(const AUX_SELECTORS type);

    /**
     * Non Native Field Arithmetic
     **/
    void range_constrain_two_limbs(const uint32_t lo_idx,
                                   const uint32_t hi_idx,
                                   const size_t lo_limb_bits = DEFAULT_NON_NATIVE_FIELD_LIMB_BITS,
                                   const size_t hi_limb_bits = DEFAULT_NON_NATIVE_FIELD_LIMB_BITS);
    std::array<uint32_t, 2> decompose_non_native_field_double_width_limb(
        const uint32_t limb_idx, const size_t num_limb_bits = (2 * DEFAULT_NON_NATIVE_FIELD_LIMB_BITS));
    std::array<uint32_t, 2> evaluate_non_native_field_multiplication(
        const non_native_field_witnesses& input, const bool range_constrain_quotient_and_remainder = true);
    std::array<uint32_t, 2> evaluate_partial_non_native_field_multiplication(const non_native_field_witnesses& input);
    typedef std::pair<uint32_t, barretenberg::fr> scaled_witness;
    typedef std::tuple<scaled_witness, scaled_witness, barretenberg::fr> add_simple;
    std::array<uint32_t, 5> evaluate_non_native_field_subtraction(
        add_simple limb0,
        add_simple limb1,
        add_simple limb2,
        add_simple limb3,
        std::tuple<uint32_t, uint32_t, barretenberg::fr> limbp);
    std::array<uint32_t, 5> evaluate_non_native_field_addition(add_simple limb0,
                                                               add_simple limb1,
                                                               add_simple limb2,
                                                               add_simple limb3,
                                                               std::tuple<uint32_t, uint32_t, barretenberg::fr> limbp);

    /**
     * Memory
     **/

    // size_t create_RAM_array(const size_t array_size);
    size_t create_ROM_array(const size_t array_size);

    void set_ROM_element(const size_t rom_id, const size_t index_value, const uint32_t value_witness);
    void set_ROM_element_pair(const size_t rom_id,
                              const size_t index_value,
                              const std::array<uint32_t, 2>& value_witnesses);
    uint32_t read_ROM_array(const size_t rom_id, const uint32_t index_witness);
    std::array<uint32_t, 2> read_ROM_array_pair(const size_t rom_id, const uint32_t index_witness);
    void create_ROM_gate(RomRecord& record);
    void create_sorted_ROM_gate(RomRecord& record);
    void process_ROM_array(const size_t rom_id, const size_t gate_offset_from_public_inputs);
    void process_ROM_arrays(const size_t gate_offset_from_public_inputs);

    void create_RAM_gate(RamRecord& record);
    void create_sorted_RAM_gate(RamRecord& record);
    void create_final_sorted_RAM_gate(RamRecord& record, const size_t ram_array_size);

    size_t create_RAM_array(const size_t array_size);
    void init_RAM_element(const size_t ram_id, const size_t index_value, const uint32_t value_witness);
    uint32_t read_RAM_array(const size_t ram_id, const uint32_t index_witness);
    void write_RAM_array(const size_t ram_id, const uint32_t index_witness, const uint32_t value_witness);
    void process_RAM_array(const size_t ram_id, const size_t gate_offset_from_public_inputs);
    void process_RAM_arrays(const size_t gate_offset_from_public_inputs);

    /**
     * Member Variables
     **/

    uint32_t zero_idx = 0;
    bool circuit_finalised = false;

    // This variable controls the amount with which the lookup table and witness values need to be shifted
    // above to make room for adding randomness into the permutation and witness polynomials in the plookup widget.
    // This must be (num_roots_cut_out_of_the_vanishing_polynomial - 1), since the variable num_roots_cut_out_of_
    // vanishing_polynomial cannot be trivially fetched here, I am directly setting this to 4 - 1 = 3.
    static constexpr size_t s_randomness = 3;

    // these are variables that we have used a gate on, to enforce that they are equal to a defined value
    std::map<barretenberg::fr, uint32_t> constant_variable_indices;

    std::vector<plookup::BasicTable> lookup_tables;
    std::vector<plookup::MultiTable> lookup_multi_tables;
    std::map<uint64_t, RangeList> range_lists; // DOCTODO: explain this.

    /**
     * @brief Each entry in ram_arrays represents an independent RAM table.
     * RamTranscript tracks the current table state,
     * as well as the 'records' produced by each read and write operation.
     * Used in `compute_proving_key` to generate consistency check gates required to validate the RAM read/write history
     */
    std::vector<RamTranscript> ram_arrays;

    /**
     * @brief Each entry in rom_arrays represents an independent ROM table.
     * RomTranscript tracks the current table state,
     * as well as the 'records' produced by each read operation.
     * Used in `compute_proving_key` to generate consistency check gates required to validate the ROM read history
     */
    std::vector<RomTranscript> rom_arrays;

    // Stores gate index of ROM and RAM reads (required by proving key)
    std::vector<uint32_t> memory_read_records;
    // Stores gate index of RAM writes (required by proving key)
    std::vector<uint32_t> memory_write_records;

    std::vector<uint32_t> recursive_proof_public_input_indices;
    bool contains_recursive_proof = false;

    static transcript::Manifest create_manifest(const size_t num_public_inputs)
    {
        // add public inputs....
        constexpr size_t g1_size = 64;
        constexpr size_t fr_size = 32;
        const size_t public_input_size = fr_size * num_public_inputs;
        const transcript::Manifest output = transcript::Manifest(

            { transcript::Manifest::RoundManifest(
                  { // { name, num_bytes, derived_by_verifier }
                    { "circuit_size", 4, true },
                    { "public_input_size", 4, true } },
                  "init", // challenge_name
                  1       // num_challenges_in
                  ),

              transcript::Manifest::RoundManifest(
                  { // { name, num_bytes, derived_by_verifier }
                    { "public_inputs", public_input_size, false },
                    { "W_1", g1_size, false },
                    { "W_2", g1_size, false },
                    { "W_3", g1_size, false } },
                  "eta", // challenge_name
                  1      // num_challenges_in
                  ),

              transcript::Manifest::RoundManifest(
                  { // { name, num_bytes, derived_by_verifier }
                    { "W_4", g1_size, false },
                    { "S", g1_size, false } },
                  "beta", // challenge_name
                  2       // num_challenges_in
                  ),

              transcript::Manifest::RoundManifest(
                  { // { name, num_bytes, derived_by_verifier }
                    { "Z_PERM", g1_size, false },
                    { "Z_LOOKUP", g1_size, false } },
                  "alpha", // challenge_name
                  1        // num_challenges_in
                  ),

              transcript::Manifest::RoundManifest(
                  { // { name, num_bytes, derived_by_verifier }
                    { "T_1", g1_size, false },
                    { "T_2", g1_size, false },
                    { "T_3", g1_size, false },
                    { "T_4", g1_size, false } },
                  "z", // challenge_name
                  1    // num_challenges_in
                  ),

              // N.B. THE SHFITED EVALS (_omega) MUST HAVE THE SAME CHALLENGE INDEX AS THE NON SHIFTED VALUES
              transcript::Manifest::RoundManifest(
                  {
                      // { name, num_bytes, derived_by_verifier, challenge_map_index }
                      { "t", fr_size, true, -1 }, // *
                      { "w_1", fr_size, false, 0 },
                      { "w_2", fr_size, false, 1 },
                      { "w_3", fr_size, false, 2 },
                      { "w_4", fr_size, false, 3 },
                      { "s", fr_size, false, 4 },
                      { "z_perm", fr_size, false, 5 }, // *
                      { "z_lookup", fr_size, false, 6 },
                      { "q_1", fr_size, false, 7 },
                      { "q_2", fr_size, false, 8 },
                      { "q_3", fr_size, false, 9 },
                      { "q_4", fr_size, false, 10 },
                      { "q_m", fr_size, false, 11 },
                      { "q_c", fr_size, false, 12 },
                      { "q_arith", fr_size, false, 13 },
                      { "q_sort", fr_size, false, 14 },     // *
                      { "q_elliptic", fr_size, false, 15 }, // *
                      { "q_aux", fr_size, false, 16 },
                      { "q_fixed_base", fr_size, false, 30 },
                      { "sigma_1", fr_size, false, 17 },
                      { "sigma_2", fr_size, false, 18 },
                      { "sigma_3", fr_size, false, 19 },
                      { "sigma_4", fr_size, false, 20 },
                      { "table_value_1", fr_size, false, 21 },
                      { "table_value_2", fr_size, false, 22 },
                      { "table_value_3", fr_size, false, 23 },
                      { "table_value_4", fr_size, false, 24 },
                      { "table_type", fr_size, false, 25 },
                      { "id_1", fr_size, false, 26 },
                      { "id_2", fr_size, false, 27 },
                      { "id_3", fr_size, false, 28 },
                      { "id_4", fr_size, false, 29 },
                      { "w_1_omega", fr_size, false, 0 },
                      { "w_2_omega", fr_size, false, 1 },
                      { "w_3_omega", fr_size, false, 2 },
                      { "w_4_omega", fr_size, false, 3 },
                      { "s_omega", fr_size, false, 4 },
                      { "z_perm_omega", fr_size, false, 5 },
                      { "z_lookup_omega", fr_size, false, 6 },
                      { "table_value_1_omega", fr_size, false, 21 },
                      { "table_value_2_omega", fr_size, false, 22 },
                      { "table_value_3_omega", fr_size, false, 23 },
                      { "table_value_4_omega", fr_size, false, 24 },
                  },
                  "nu",                // challenge_name
                  ULTRA_MANIFEST_SIZE, // num_challenges_in
                  true                 // map_challenges_in
                  ),

              transcript::Manifest::RoundManifest(
                  { // { name, num_bytes, derived_by_verifier, challenge_map_index }
                    { "PI_Z", g1_size, false },
                    { "PI_Z_OMEGA", g1_size, false } },
                  "separator", // challenge_name
                  3            // num_challenges_in
                  ) });

        return output;
    }
};
} // namespace plonk
