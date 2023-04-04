#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <cstring>
#include <list>
#include <vector>
#include <stdlib.h>

#include "procsim.hpp"



//
// TODO: Define any useful data structures and functions here
//
// qentry_t is used for all queue data structures
typedef struct queue_entry {
    const inst_t *inst;
    unsigned long src1_preg;
    unsigned long src2_preg;
    unsigned long dest_preg;
    unsigned long prev_preg;
    bool completed;
    struct queue_entry *disp_next;
    struct queue_entry *sched_prev;
    struct queue_entry *sched_next;
    struct queue_entry *rob_next;
} qentry_t;
qentry_t *dispatch_head = NULL;
qentry_t *dispatch_tail = NULL;
qentry_t *ROB_head = NULL;
qentry_t *ROB_tail = NULL;
qentry_t *ALU_RS_head = NULL;
qentry_t *ALU_RS_tail = NULL;
qentry_t *MUL_RS_head = NULL;
qentry_t *MUL_RS_tail = NULL;
qentry_t *LS_RS_head = NULL;
qentry_t *LS_RS_tail = NULL;

typedef struct reg {
    bool free;
    bool ready;
} reg_t;

unsigned long RAT[32];
struct reg *reg_file;
size_t FETCH_WIDTH;
size_t NUM_PREGS;
size_t NUM_ROB_ENTRIES;
size_t MAX_ROB_ENTRIES;
size_t NUM_ALU_RS_ENTRIES;
size_t MAX_ALU_RS_ENTRIES;
size_t NUM_MUL_RS_ENTRIES;
size_t MAX_MUL_RS_ENTRIES;
size_t NUM_LS_RS_ENTRIES;
size_t MAX_LS_RS_ENTRIES;

// The helper functions in this#ifdef are optional and included here for your
// convenience so you can spend more time writing your simulator logic and less
// time trying to match debug trace formatting! (If you choose to use them)
#ifdef DEBUG
// TODO: Fix the debug outs
static void print_operand(int8_t rx) {
    if (rx < 0) {
        printf("(none)"); //  PROVIDED
    } else {
        printf("R%" PRId8, rx); //  PROVIDED
    }
}

// Useful in the fetch and dispatch stages
static void print_instruction(const inst_t *inst) {
    if (!inst) return;
    static const char *opcode_names[] = {NULL, NULL, "ADD", "MUL", "LOAD", "STORE", "BRANCH"};

    printf("opcode=%s, dest=", opcode_names[inst->opcode]); //  PROVIDED
    print_operand(inst->dest); //  PROVIDED
    printf(", src1="); //  PROVIDED
    print_operand(inst->src1); //  PROVIDED
    printf(", src2="); //  PROVIDED
    print_operand(inst->src2); //  PROVIDED
    printf(", dyncount=%lu", inst->dyn_instruction_count); //  PROVIDED
}

// This will print out the state of the RAT
static void print_rat(void) {
    for (uint64_t regno = 0; regno < NUM_REGS; regno++) {
        if (regno == 0) {
            printf("    { R%02" PRIu64 ": P%03" PRIu64 " }", regno, RAT[regno]); // TODO: fix me
        } else if (!(regno & 0x3)) {
            printf("\n    { R%02" PRIu64 ": P%03" PRIu64 " }", regno, RAT[regno]); //  TODO: fix me
        } else {
            printf(", { R%02" PRIu64 ": P%03" PRIu64 " }", regno, RAT[regno]); //  TODO: fix me
        }
    }
    printf("\n"); //  PROVIDED
}

// This will print out the state of the register file, where P0-P31 are architectural registers 
// and P32 is the first PREG 
static void print_prf(void) {
    for (uint64_t regno = 0; /* ??? */ false; regno++) { // TODO: fix me
        if (regno == 0) {
            printf("    { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, reg_file[regno].ready, reg_file[regno].free); // TODO: fix me
        } else if (!(regno & 0x3)) {
            printf("\n    { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, reg_file[regno].ready, reg_file[regno].free);
        } else {
            printf(", { P%03" PRIu64 ": Ready: %d, Free: %d }", regno, reg_file[regno].ready, reg_file[regno].free);
        }
    }
    printf("\n"); //  PROVIDED
}

// This will print the state of the ROB where instructions are identified by their dyn_instruction_count
static void print_rob(void) {
    size_t printed_idx = 0;
    printf("\tAllocated Entries in ROB: %lu\n", 0ul); // TODO: Fix Me
    for (qentry_t *entry = ROB_head; entry != NULL; entry = entry->rob_next) { // TODO: Fix Me
        if (printed_idx == 0) {
            printf("    { dyncount=%05" PRIu64 ", completed: %d, mispredict: %d }", entry->inst->dyn_instruction_count, entry->completed, entry->inst->mispredict); // TODO: Fix Me
        } else if (!(printed_idx & 0x3)) {
            printf("\n    { dyncount=%05" PRIu64 ", completed: %d, mispredict: %d }", entry->inst->dyn_instruction_count, entry->completed, entry->inst->mispredict); // TODO: Fix Me
        } else {
            printf(", { dyncount=%05" PRIu64 " completed: %d, mispredict: %d }", entry->inst->dyn_instruction_count, entry->completed, entry->inst->mispredict);
        }
        printed_idx++;
    }
    if (!printed_idx) {
        printf("    (ROB empty)"); //  PROVIDED
    }
    printf("\n"); //  PROVIDED
}
#endif




// Optional helper function which pops previously retired store buffer entries
// and pops instructions from the head of the ROB. (In a real system, the
// destination register value from the ROB would be written to the
// architectural registers, but we have no register values in this
// simulation.) This function returns the number of instructions retired.
// Immediately after retiring a mispredicting branch, this function will set
// *retired_mispredict_out = true and will not retire any more instructions. 
// Note that in this case, the mispredict must be counted as one of the retired instructions.
static uint64_t stage_state_update(procsim_stats_t *stats,
                                   bool *retired_mispredict_out) {
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Retire: \n"); //  PROVIDED
#endif
    return 0;
}

// Optional helper function which is responsible for moving instructions
// through pipelined Function Units and then when instructions complete (that
// is, when instructions are in the final pipeline stage of an FU and aren't
// stalled there), setting the ready bits in the register file. This function 
// should remove an instruction from the scheduling queue when it has completed.
static void stage_exec(procsim_stats_t *stats) {
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Exec: \n"); //  PROVIDED
#endif

    // Progress ALUs
#ifdef DEBUG
    printf("Progressing ALU units\n");  // PROVIDED
#endif

    // Progress MULs
#ifdef DEBUG
    printf("Progressing MUL units\n");  // PROVIDED
#endif

    // Progress LSU loads
#ifdef DEBUG
    printf("Progressing LSU units for loads\n");  // PROVIDED
#endif

    // Progress LSU stores
#ifdef DEBUG
    printf("Progressing LSU units for stores\n");  // PROVIDED
#endif

    // Apply Result Busses
#ifdef DEBUG
    printf("Processing Result Busses\n"); // PROVIDED
#endif
}

// Optional helper function which is responsible for looking through the
// scheduling queue and firing instructions that have their source pregs
// marked as ready. Note that when multiple instructions are ready to fire
// in a given cycle, they must be fired in program order. 
// Also, load and store instructions must be fired according to the 
// memory disambiguation algorithm described in the assignment PDF. Finally,
// instructions stay in their reservation station in the scheduling queue until
// they complete (at which point stage_exec() above should free their RS).
static void stage_schedule(procsim_stats_t *stats) {
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Schedule: \n"); //  PROVIDED
#endif
}

// Optional helper function which looks through the dispatch queue, decodes
// instructions, and inserts them into the scheduling queue. Dispatch should
// not add an instruction to the scheduling queue unless there is space for it
// in the scheduling queue and the ROB and a free preg exists if necessary; 
// effectively, dispatch allocates pregs, reservation stations and ROB space for 
// each instruction dispatched and stalls if there any are unavailable. 
// You will also need to update the RAT if need be.
// Note the scheduling queue has a configurable size and the ROB has P+32 entries.
// The PDF has details.
static void stage_dispatch(procsim_stats_t *stats) {
    // TODO: fill me in
    // Find the first empty entry in the ROB
    while (1) {
        qentry_t *entry = dispatch_head;
        if (entry == NULL) {
            return;
        }
        const inst_t *inst = entry->inst;

        if (NUM_ROB_ENTRIES >= MAX_ROB_ENTRIES) {
            return;
        }

        unsigned long dest_preg = -1;
        if (inst->dest >= 0) {
            for (unsigned long i = 32; i < 32 + NUM_PREGS; i++) {
                if (reg_file[i].free) {
                    dest_preg = i;
                    break;
                }
            }
            return;  // No free pregs
        }

        // From here down are changes to the state of the system //

        switch(inst->opcode) {
            case OPCODE_ADD:
                if (NUM_ALU_RS_ENTRIES >= MAX_ALU_RS_ENTRIES) return;
                if (ALU_RS_head == NULL) ALU_RS_head = entry;
                if (ALU_RS_tail != NULL) ALU_RS_tail->sched_next = entry;
                entry->sched_prev = ALU_RS_tail;
                ALU_RS_tail = entry;
                NUM_ALU_RS_ENTRIES++;
                break;
            case OPCODE_MUL:
                if (NUM_MUL_RS_ENTRIES >= MAX_MUL_RS_ENTRIES) return;
                if (MUL_RS_head == NULL) MUL_RS_head = entry;
                if (MUL_RS_tail != NULL) MUL_RS_tail->sched_next = entry;
                entry->sched_prev = MUL_RS_tail;
                MUL_RS_tail = entry;
                NUM_MUL_RS_ENTRIES++;
                break;
            case OPCODE_LOAD:
            case OPCODE_STORE:
                if (NUM_LS_RS_ENTRIES >= MAX_LS_RS_ENTRIES) return;
                if (LS_RS_head == NULL) LS_RS_head = entry;
                if (LS_RS_tail != NULL) LS_RS_tail->sched_next = entry;
                entry->sched_prev = LS_RS_tail;
                LS_RS_tail = entry;
                NUM_LS_RS_ENTRIES++;
                break;
            case OPCODE_BRANCH:
                ////??????
                break;
            default:
                break;
        }

        if (inst->src1 >= 0) entry->src1_preg = RAT[inst->src1];
        if (inst->src2 >= 0) entry->src2_preg = RAT[inst->src2];
        if (inst->dest >= 0) {
            entry->prev_preg = RAT[inst->dest];  // Save previous preg
            RAT[inst->dest] = dest_preg;
        }

        // Place in the ROB
        if (ROB_tail != NULL) {
            ROB_tail->rob_next = entry;
        }
        if (ROB_head == NULL) ROB_head = entry;
        ROB_tail = entry;
        NUM_ROB_ENTRIES++;
        // Remove from dispatch head
        dispatch_head = entry->disp_next;
    }

#ifdef DEBUG
    printf("Stage Dispatch: \n"); //  PROVIDED
#endif
}

// Optional helper function which fetches instructions from the instruction
// cache using the provided procsim_driver_read_inst() function implemented
// in the driver and appends them to the dispatch queue. To simplify the
// project, the dispatch queue is infinite in size.
static void stage_fetch(procsim_stats_t *stats) {
    // Fetch instructions and add them to the dispatch queue
    for (size_t i = 0; i < FETCH_WIDTH; i++) {
        const inst_t *inst = procsim_driver_read_inst();
        if (inst == NULL) return;
        qentry_t *entry = (qentry_t *)malloc(sizeof(qentry_t));
        entry->inst = inst;
        entry->disp_next = NULL;
        entry->sched_next = NULL;
        entry->rob_next = NULL;
        entry->completed = false;
        if (dispatch_tail != NULL) {
            dispatch_tail->disp_next = entry;
        }
        if (dispatch_head == NULL) {
            dispatch_head = entry;  // The queue was empty
        }
        dispatch_tail = entry;
    }
#ifdef DEBUG
    printf("Stage Fetch: \n"); //  PROVIDED
#endif
}

// Use this function to initialize all your data structures, simulator
// state, and statistics.
void procsim_init(const procsim_conf_t *sim_conf, procsim_stats_t *stats) {
    FETCH_WIDTH = sim_conf->fetch_width;
    NUM_PREGS = sim_conf->num_pregs;
    NUM_ROB_ENTRIES = 0;
    MAX_ROB_ENTRIES = 32 + NUM_PREGS;
    NUM_ALU_RS_ENTRIES = 0;
    MAX_ALU_RS_ENTRIES = sim_conf->num_alu_fus * sim_conf->num_schedq_entries_per_fu;
    NUM_MUL_RS_ENTRIES = 0;
    MAX_MUL_RS_ENTRIES = sim_conf->num_mul_fus * sim_conf->num_schedq_entries_per_fu;
    NUM_LS_RS_ENTRIES = 0;
    MAX_LS_RS_ENTRIES = sim_conf->num_lsu_fus * sim_conf->num_schedq_entries_per_fu;

    // Initialize the register file
    reg_file = (reg_t *)malloc(sizeof(reg_t) * (32 + sim_conf->num_pregs));
    for (uint32_t i = 0; i < 32 + sim_conf->num_pregs; i++) {
        reg_file[i].free = 1;
        reg_file[i].ready = 1;
    }
    // Initialize RAT with respective architectural reg number
    for (int i = 0; i < 32; i++) {
        RAT[i] = i;
    }
//     // Initialize reservation stations
//     ALU_RS = (qentry_t*)malloc(sizeof(qentry_t) *
//             sim_conf->num_schedq_entries_per_fu * sim_conf->num_alu_fus);
//     MUL_RS = (qentry_t*)malloc(sizeof(qentry_t) *
//             sim_conf->num_schedq_entries_per_fu * sim_conf->num_mul_fus);
//     LS_RS = (qentry_t*)malloc(sizeof(qentry_t) *
//             sim_conf->num_schedq_entries_per_fu * sim_conf->num_lsu_fus);

#ifdef DEBUG
    printf("\nScheduling queue capacity: %lu instructions\n", sim_conf->num_schedq_entries_per_fu * 
            (sim_conf->num_alu_fus + sim_conf->num_mul_fus + sim_conf->num_lsu_fus)); // TODO: Fix ME
    printf("Initial RAT state:\n"); //  PROVIDED
    print_rat();
    printf("\n"); //  PROVIDED
#endif
}

// To avoid confusion, we have provided this function for you. Notice that this
// calls the stage functions above in reverse order! This is intentional and
// allows you to avoid having to manage pipeline registers between stages by
// hand. This function returns the number of instructions retired, and also
// returns if a mispredict was retired by assigning true or false to
// *retired_mispredict_out, an output parameter.
uint64_t procsim_do_cycle(procsim_stats_t *stats,
                          bool *retired_mispredict_out) {
#ifdef DEBUG
    printf("================================ Begin cycle %" PRIu64 " ================================\n", stats->cycles); //  PROVIDED
#endif

    // stage_state_update() should set *retired_mispredict_out for us
    uint64_t retired_this_cycle = stage_state_update(stats, retired_mispredict_out);

    if (*retired_mispredict_out) {
#ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Retired mispredict, so notifying driver to fetch correctly!\n", retired_this_cycle); //  PROVIDED
#endif

        // After we retire a misprediction, the other stages don't need to run
        stats->branch_mispredictions++;
    } else {
#ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Did not retire mispredict, so proceeding with other pipeline stages.\n", retired_this_cycle); //  PROVIDED
#endif

        // If we didn't retire an interupt, then continue simulating the other
        // pipeline stages
        stage_exec(stats);
        stage_schedule(stats);
        stage_dispatch(stats);
        stage_fetch(stats);
    }

#ifdef DEBUG
    printf("End-of-cycle dispatch queue usage: %lu\n", 0ul); // TODO: Fix Me
    printf("End-of-cycle sched queue usage: %lu\n", 0ul); // TODO: Fix Me
    printf("End-of-cycle ROB usage: %lu\n", 0ul); // TODO: Fix Me
    printf("End-of-cycle RAT state:\n"); //  PROVIDED
    print_rat();
    printf("End-of-cycle Physical Register File state:\n"); //  PROVIDED
    print_prf();
    printf("End-of-cycle ROB state:\n"); //  PROVIDED
    print_rob();
    printf("================================ End cycle %" PRIu64 " ================================\n", stats->cycles); //  PROVIDED
    print_instruction(NULL); // this makes the compiler happy, ignore it
#endif

    // TODO: Increment max_usages and avg_usages in stats here!
    stats->cycles++;

    // Return the number of instructions we retired this cycle (including the
    // interrupt we retired, if there was one!)
    return retired_this_cycle;
}

// Use this function to free any memory allocated for your simulator and to
// calculate some final statistics.
void procsim_finish(procsim_stats_t *stats) {
    // TODO: fill me in
}
