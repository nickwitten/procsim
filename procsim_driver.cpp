// You do not need to modify this file!

#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "procsim.hpp"

static size_t n_insts;
static inst_t *insts;
static uint64_t fetch_inst_idx;

// Print error usage
static void print_err_usage(const char *err) {
    fprintf(stderr, "%s\n", err);
    fprintf(stderr, "./procsim -I <trace file> [Options]\n");
    fprintf(stderr, "-F <fetch width>\n");
    fprintf(stderr, "-P <number of Physical Registers>\n");
    fprintf(stderr, "-A <number of ALU FUs>\n");
    fprintf(stderr, "-M <number of MUL FUs>\n");
    fprintf(stderr, "-L <number of load/store FUs>\n");
    fprintf(stderr, "-S <number of SchedQ entries per FU>\n");
    fprintf(stderr, "-D disables Cache Misses and Interrupts\n");
    fprintf(stderr, "-H prints this message\n");

    exit(EXIT_FAILURE);
}

static bool validate_sim_config(procsim_conf_t *sim_conf) {
    size_t f = sim_conf->fetch_width;
    size_t s = sim_conf->num_schedq_entries_per_fu;
    size_t p = sim_conf->num_pregs;
    size_t a = sim_conf->num_alu_fus;
    size_t l = sim_conf->num_lsu_fus;
    size_t m = sim_conf->num_mul_fus;
    bool valid = true;
    if (!(f == 2 || f == 4 || f == 8)) {
        fprintf(stderr, "Invalid F: %" PRIu64 "\n", f);
        valid = false;
    }
    if (!(s == 2 || s == 4 || s == 8)) {
        fprintf(stderr, "Invalid S: %" PRIu64 "\n", s);
        valid = false;
    }
    if (!(p == 64 || p == 96 || p == 128)) {
        fprintf(stderr, "Invalid P: %" PRIu64 "\n", p);
        valid = false;
    }
    if (!(a == 1 || a == 2 || a == 3)) {
        fprintf(stderr, "Invalid A: %" PRIu64 "\n", a);
        valid = false;
    }
    if (!(l == 1 || l == 2 || l == 3)) {
        fprintf(stderr, "Invalid L: %" PRIu64 "\n", l);
        valid = false;
    }
    if (!(m == 1 || m == 2)) {
        fprintf(stderr, "Invalid M: %" PRIu64 "\n", m);
        valid = false;
    }
    return valid;
}

// Function to print the run configuration
static void print_sim_config(procsim_conf_t *sim_conf) {
    printf("SIMULATION CONFIGURATION\n");
    printf("Fetch width:  %lu\n", sim_conf->fetch_width);
    printf("Physical Registers:  %lu\n", sim_conf->num_pregs);
    printf("ROB entries:  %lu\n", sim_conf->num_rob_entries);
    printf("Num. ALU FUs: %lu\n", sim_conf->num_alu_fus);
    printf("Num. MUL FUs: %lu\n", sim_conf->num_mul_fus);
    printf("Num. LSU FUs: %lu\n", sim_conf->num_lsu_fus);
    printf("Num. SchedQ entries per FU: %lu\n", sim_conf->num_schedq_entries_per_fu);
    
    printf("Misses:   %s\n", sim_conf->misses_enabled ? "enabled"
                                                             : "disabled");
}

// Function to print the simulation output
static void print_sim_output(procsim_stats_t *sim_stats) {
    printf("\nSIMULATION OUTPUT\n");
    printf("Cycles:                     %" PRIu64 "\n", sim_stats->cycles);
    printf("Trace instructions:         %" PRIu64 "\n", sim_stats->instructions_in_trace);
    printf("Fetched instructions:       %" PRIu64 "\n", sim_stats->instructions_fetched);
    printf("Retired instructions:       %" PRIu64 "\n", sim_stats->instructions_retired);
    printf("I-Cache misses:             %" PRIu64 "\n", sim_stats->icache_misses);
    printf("D-Cache read misses:        %" PRIu64 "\n", sim_stats->dcache_read_misses);
    printf("D-Cache reads:              %" PRIu64 "\n", sim_stats->dcache_reads);
    printf("Store Buffer hits:          %" PRIu64 "\n", sim_stats->store_buffer_read_hits);
    printf("Read AAT:                   %.3f\n", sim_stats->read_aat);
    printf("Branch Mispredictions:      %" PRIu64 "\n", sim_stats->branch_mispredictions);
    printf("Stall cycles due to PREGs:  %" PRIu64 "\n", sim_stats->no_dispatch_pregs_cycles);
    printf("Stall cycles due to ROB:    %" PRIu64 "\n", sim_stats->rob_stall_cycles);
    printf("Cycles with no fires:       %" PRIu64 "\n", sim_stats->no_fire_cycles);
    printf("Max DispQ usage:      %" PRIu64 "\n", sim_stats->dispq_max_size);
    printf("Average DispQ usage:  %.3f\n", sim_stats->dispq_avg_size);
    printf("Max SchedQ usage:     %" PRIu64 "\n", sim_stats->schedq_max_size);
    printf("Average SchedQ usage: %.3f\n", sim_stats->schedq_avg_size);
    printf("Max ROB usage:        %" PRIu64 "\n", sim_stats->rob_max_size);
    printf("Average ROB usage:    %.3f\n", sim_stats->rob_avg_size);
    printf("IPC:                  %.3f\n", sim_stats->ipc);
}

static inst_t *read_entire_trace(FILE *trace, size_t *size_insts_out, procsim_conf_t *sim_conf) {
    size_t size_insts = 0;
    size_t cap_insts = 0;
    inst_t *insts_arr = NULL;

    while (!feof(trace)) {
        if (size_insts == cap_insts) {
            size_t new_cap_insts = 2 * (cap_insts + 1);
            // redundant c++ cast #1
            inst_t *new_insts_arr = (inst_t *)realloc(insts_arr, new_cap_insts * sizeof *insts_arr);
            if (!new_insts_arr) {
                perror("realloc");
                goto error;
            }
            cap_insts = new_cap_insts;
            insts_arr = new_insts_arr;
        }

        inst_t *inst = insts_arr + size_insts;
        // TODO: update traces to pc, opcode, dr, sr1, sr2, ldst, inst_num, mispred, icmiss, dcmiss
        int mispred;
        int ic_miss;
        int dc_miss;
        int ret = fscanf(trace, "%" SCNx64 " %d %" SCNd8 " %" SCNd8 " %" SCNd8 " %" SCNx64 " %" SCNu64 " %d %d %d\n", &inst->pc, (int *)&inst->opcode, &inst->dest, &inst->src1, &inst->src2, &inst->load_store_addr, &inst->dyn_instruction_count, &mispred, &ic_miss, &dc_miss);

        if (ret == 10) {
            inst->mispredict = mispred && sim_conf->misses_enabled;
            inst->icache_miss = ic_miss && sim_conf->misses_enabled;
            inst->dcache_miss = dc_miss && sim_conf->misses_enabled;
            if (inst->dest == 0) inst->dest = -1;
            size_insts++;
        } else {
            if (ferror(trace)) {
                perror("fscanf");
            } else {
                fprintf(stderr, "could not parse line %d in trace (only %d input items matched). is it corrupt?\n", (int) size_insts, ret);
            }
            goto error;
        }
    }

    *size_insts_out = size_insts;
    return insts_arr;

    error:
    free(insts_arr);
    *size_insts_out = 0;
    return NULL;
}


bool in_mispred = false;
bool in_icache_miss = false;
size_t icache_miss_ctr = 0;
bool finished_miss = false;
const inst_t *procsim_driver_read_inst(void) {
    if (in_mispred) {
        return NULL;
    }
    if (in_icache_miss) {
        return NULL;
    }
    if (fetch_inst_idx >= n_insts) {
        return NULL;
    } else {
        if (insts[fetch_inst_idx].icache_miss) {
            if (!finished_miss) { // if didnt just finish a cache miss
                in_icache_miss = true;
                icache_miss_ctr = L1_MISS_PENALTY;
                finished_miss = false;
                return NULL; // can't give you an instruction that missed in cache
            } else {
                finished_miss = false; // reset state for icache misses
                // carry on to check other details
            }
        }
        if (insts[fetch_inst_idx].mispredict) {
            in_mispred = true;
        }
        return &insts[fetch_inst_idx++];
    }
}

int main(int argc, char *const argv[])
{
    FILE *trace = NULL;

    procsim_stats_t sim_stats;
    memset(&sim_stats, 0, sizeof sim_stats);

    procsim_conf_t sim_conf;
    memset(&sim_conf, 0, sizeof sim_conf);
    // Default configuration
    // TODO: this
    sim_conf.num_pregs = 64;
    sim_conf.num_rob_entries = 96;
    sim_conf.num_schedq_entries_per_fu = 2;
    sim_conf.num_alu_fus = 2;
    sim_conf.num_mul_fus = 1;
    sim_conf.num_lsu_fus = 2;
    sim_conf.fetch_width = 2;
    sim_conf.misses_enabled = true;

    int opt;
    while (-1 != (opt = getopt(argc, argv, "i:I:s:S:a:A:m:M:l:L:f:F:p:P:dDhH"))) {
        switch (opt) {
            case 'i':
            case 'I':
                trace = fopen(optarg, "r");
                if (trace == NULL) {
                    perror("fopen");
                    print_err_usage("Could not open the input trace file");
                }
                break;

            case 's':
            case 'S':
                sim_conf.num_schedq_entries_per_fu = atoi(optarg);
                break;

            case 'a':
            case 'A':
                sim_conf.num_alu_fus = atoi(optarg);
                break;

            case 'm':
            case 'M':
                sim_conf.num_mul_fus = atoi(optarg);
                break;

            case 'l':
            case 'L':
                sim_conf.num_lsu_fus = atoi(optarg);
                break;

            case 'f':
            case 'F':
                sim_conf.fetch_width = atoi(optarg);
                break;

            case 'p':
            case 'P':
                sim_conf.num_pregs = atoi(optarg);
                sim_conf.num_rob_entries = sim_conf.num_pregs + 32;
                break;

            case 'd':
            case 'D':
                sim_conf.misses_enabled = false;
                break;

            case 'h':
            case 'H':
                print_err_usage("");
                break;

            default:
                print_err_usage("Invalid argument to program");
                break;
        }
    }

    if (!trace) {
        print_err_usage("No trace file provided!");
    }
    if (!validate_sim_config(&sim_conf)) {
        fclose(trace);
        exit(EXIT_FAILURE);
    }

    insts = read_entire_trace(trace, &n_insts, &sim_conf);
    fclose(trace);
    if (!insts) {
        return 1;
    }

    print_sim_config(&sim_conf);
    // Initialize the processor
    procsim_init(&sim_conf, &sim_stats);
    printf("SETUP COMPLETE - STARTING SIMULATION\n");

    // We made this number up, but it should never take this many cycles to
    // retire something
    static const uint64_t max_cycles_since_last_retire = 128;
    uint64_t cycles_since_last_retire = 0;

    uint64_t retired_inst_idx = 0;
    fetch_inst_idx = 0;
    while (retired_inst_idx < n_insts) {
        bool retired_mispredict = false;
        uint64_t retired_this_cycle = procsim_do_cycle(&sim_stats, &retired_mispredict);
        retired_inst_idx += retired_this_cycle;
        // Check for deadlocks (e.g., an empty submission)
        if (retired_this_cycle) {
            cycles_since_last_retire = 0;
        } else {
            cycles_since_last_retire++;
        }
        if (cycles_since_last_retire == max_cycles_since_last_retire) {
            printf("\nIt has been %" PRIu64 " cycles since the last retirement."
                   " Does the simulator have a deadlock?\n",
                   max_cycles_since_last_retire);
            return 1;
        }

        if (retired_mispredict) {
            // Start refilling the dispatch queue now that mispredict is handled
            fetch_inst_idx = retired_inst_idx;
            in_mispred = false;
        }

        
        if (icache_miss_ctr != 0) {
            icache_miss_ctr--;
        }
        if (icache_miss_ctr == 0 && in_icache_miss) {
            in_icache_miss = false;
            finished_miss = true;
        }
    }

    sim_stats.instructions_in_trace = n_insts;

    // Free memory and generate final statistics
    procsim_finish(&sim_stats);
    free(insts);

    print_sim_output(&sim_stats);

    return 0;
}
