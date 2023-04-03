#ifndef PROCSIM_H
#define PROCSIM_H

#include <inttypes.h>

// Number of architectural registers / GPRs
#define NUM_REGS 32

#define L1_MISS_PENALTY 10
#define L1_HIT_TIME 2

#define ALU_STAGES 1
#define MUL_STAGES 3

#ifdef DEBUG
// Beautiful preprocessor hack to convert some syntax (e.g., a constant int) to
// a string. Only used for printing debug outputs
#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#endif

typedef enum {
    OPCODE_ADD = 2,
    OPCODE_MUL,
    OPCODE_LOAD,
    OPCODE_STORE,
    OPCODE_BRANCH,
} opcode_t;

typedef struct {
    uint64_t pc;
    opcode_t opcode;
    int8_t dest;
    int8_t src1;
    int8_t src2;
    uint64_t load_store_addr;
    // This is a unique identifier for each instruction in the trace
    // This is used for memory disambiguation
    uint64_t dyn_instruction_count;
    // A value of true indicates that this instruction is a mispredicted branch. The
    // trace/driver sets this flag for you. This bit should be passed through the
    // pipeline all the way to the State Update stage.
    bool mispredict;
    // A value of true indicates that this instruction experiences an Instruction Cache miss. The
    // trace/driver sets this flag for you. This bit should be passed through the
    // pipeline all the way to the State Update stage.
    bool icache_miss;
    // A value of true indicates that this instruction experiences a Data Cache miss. The
    // trace/driver sets this flag for you. This bit should be passed through the
    // pipeline all the way to the State Update stage.
    bool dcache_miss;
} inst_t;

// This config struct is populated by the driver for you
typedef struct {
    size_t fetch_width;
    size_t num_rob_entries;
    size_t num_schedq_entries_per_fu;
    size_t num_pregs;
    size_t num_alu_fus;
    size_t num_mul_fus;
    size_t num_lsu_fus;

    // The driver sets this, you do not need to use this
    bool misses_enabled;
} procsim_conf_t;

typedef struct {
    // You need to update these! You can assume all are initialized to 0.
    uint64_t cycles;
    uint64_t instructions_fetched;
    uint64_t instructions_retired;

    uint64_t branch_mispredictions;
    uint64_t icache_misses;

    uint64_t reads;
    uint64_t store_buffer_read_hits;
    uint64_t dcache_reads;
    uint64_t dcache_read_misses;
    uint64_t dcache_read_hits;
    
    double store_buffer_hit_ratio;
    double dcache_read_miss_ratio;
    double dcache_ratio;

    double dcache_read_aat;
    double read_aat;

    // Incremented for each cycle in which we cannot dispatch 
    // due to not being able to find a free preg
    uint64_t no_dispatch_pregs_cycles;

    // Incremented for each cycle in which we stop dispatching only because of
    // insufficient ROB space
    uint64_t rob_stall_cycles;

    // Incremented for each cycle in which we fired no instructions
    uint64_t no_fire_cycles;

    // Maximum valid DispQ entries at the END of procsim_do_cycle()
    uint64_t dispq_max_size;

    // Maximum valid SchedQ entries at the END of procsim_do_cycle()
    uint64_t schedq_max_size;

    // Maximum valid ROB entries at the END of procsim_do_cycle()
    uint64_t rob_max_size;

    // Average valid DispQ entries at the END of procsim_do_cycle()
    double dispq_avg_size;

    // Average valid SchedQ entries at the END of procsim_do_cycle()
    double schedq_avg_size;

    // Average valid ROB entries at the END of procsim_do_cycle()
    double rob_avg_size;

    double ipc;

    // The driver populates the stat below for you
    uint64_t instructions_in_trace;
} procsim_stats_t;

// We have implemented this function for you in the driver. By calling it, you
// are effectively reading from an icache with 100% hit rate, where branch
// prediction is 100% correct and handled for you.
extern const inst_t *procsim_driver_read_inst(void);

// There is more information on these functions in procsim.cpp
extern void procsim_init(const procsim_conf_t *sim_conf,
                         procsim_stats_t *stats);
extern uint64_t procsim_do_cycle(procsim_stats_t *stats,
                                 bool *retired_mispredict_out);
extern void procsim_finish(procsim_stats_t *stats);

#endif
