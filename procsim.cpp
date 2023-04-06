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
    uint8_t exec_cycle;
    bool completed;
    struct queue_entry *next;
} qentry_t;

typedef struct queue {
    qentry_t *head;
    qentry_t *tail;
    size_t max_size;
    size_t size;
} queue_t;

queue_t qdis;  // Dispatch queue
queue_t qrob;  // ROB queue
queue_t qalu;  // ALU RS queue
queue_t qmul;  // MUL RS queue
queue_t qlsu;  // LSU RS queue
queue_t qstb;  // Store Buffer queue
queue_t *qalu_pipes;  // List of ALU FU pipes
size_t NUM_ALU_FUS;
queue_t *qmul_pipes;  // List of MUL FU pipes
size_t NUM_MUL_FUS;
queue_t *qlsu_pipes;  // List of LSU FU pipes
size_t NUM_LSU_FUS;

// qentry_t *dispatch_head = NULL;
// qentry_t *dispatch_tail = NULL;
// qentry_t *ROB_head = NULL;
// qentry_t *ROB_tail = NULL;
// qentry_t *ALU_RS_head = NULL;
// qentry_t *ALU_RS_tail = NULL;
// qentry_t *MUL_RS_head = NULL;
// qentry_t *MUL_RS_tail = NULL;
// qentry_t *LS_RS_head = NULL;
// qentry_t *LS_RS_tail = NULL;

typedef struct reg {
    bool free;
    bool ready;
} reg_t;

unsigned long RAT[32];
struct reg *reg_file;
size_t FETCH_WIDTH;
size_t NUM_PREGS;

/* initialize queue, pass max_size == -1 for unlimited size */
void queue_init(queue_t *queue, size_t max_size) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->max_size = max_size;
    queue->size = 0;
}

/* copy all values of qentry src to dst */
void qentry_copy(qentry_t *src, qentry_t *dst) {
    dst->inst = src->inst;
    dst->src1_preg = src->src1_preg;
    dst->src2_preg = src->src2_preg;
    dst->dest_preg = src->dest_preg;
    dst->prev_preg = src->prev_preg;
    dst->exec_cycle = src->exec_cycle;
    dst->completed = src->completed;
}

/* insert entry at the tail of the FIFO queue.
 * Returns 0 on success
 * Returns -1 on full queue
 */
int fifo_insert_tail(queue_t *q, qentry_t *entry) {
    // Check if fifo is full
    if (q->max_size >= 0 && q->size >= q->max_size) {
        return -1;
    }
    if (q->head == NULL) {
        q->head = entry;
    }
    if (q->tail != NULL) {
        q->tail->next = entry;
    }
    entry->next = NULL;
    q->tail = entry;
    q->size++;
    return 0;
}

/* pop the FIFO queue head.
 * Returns NULL if FIFO is empty
 */
qentry_t *fifo_pop_head(queue_t *q) {
    if (q->head == NULL) {
        return NULL;
    }
    qentry_t *entry = q->head;
    q->head = entry->next;
    entry->next = NULL;
    q->size--;
    return entry;
}

/* Search a queue by unique instruction ID 
 * Returns NULL when not found
 */
qentry_t *search_queue(queue_t *q, uint64_t dyn_instruction_count) {
    qentry_t *entry = q->head;
    while (entry != NULL) {
        if (entry->inst->dyn_instruction_count == dyn_instruction_count) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Searches through a list of FU pipelines for a FU without an entry
 * in the first execution cycle.
 * Returns NULL when not found
 */
queue_t *find_free_fu(queue_t *fus, size_t num_fus) {
    for (size_t i = 0; i < num_fus; i++) {
        if (fus[i].tail == NULL || fus[i].tail->exec_cycle != 1) {
            return &(fus[i]);
        }
    }
    return NULL;
}

int insert_entry_copy_pipe_tail(qentry_t *entry, queue_t *fu) {
    qentry_t *pipe_entry = (qentry_t *)malloc(sizeof(qentry_t));
    qentry_copy(entry, pipe_entry);
    pipe_entry->exec_cycle = 1;
    return fifo_insert_tail(fu, pipe_entry);
}


/* Try to fire the instruction from the RS 
 * Returns 0 on successful fire
 * Returns -1 on instruction not ready
 * Returns -2 on no free FUs
 */
int try_fire(queue_t *fus, size_t num_fus, qentry_t *entry) {
    // Check if src pregs are ready
    if (entry->src1_preg < 0 || reg_file[entry->src1_preg].ready) {
        if (entry->src2_preg < 0 || reg_file[entry->src2_preg].ready) {
            // Instruction is ready, is there a free FU?
            queue_t *free_fu = find_free_fu(fus, num_fus);
            if (free_fu == NULL) {
                return -2;  // Stop scheduling because all FUs are taken
            }
            // Insert a copy into the pipeline
            int success = insert_entry_copy_pipe_tail(entry, free_fu);
            if (success != 0) {
                printf("MY ERROR, why is the FU full?\n");
            }
            return success;
        }
    }
    return -1;
}



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
    printf("\tAllocated Entries in ROB: %lu\n", qrob.size); // TODO: Fix Me
    for (qentry_t *entry = qrob.head; entry != NULL; entry = entry->next) { // TODO: Fix Me
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
    qentry_t *entry;
    int success;

    // Schedule from ALU RS
    entry = qalu.head;
    while (entry != NULL) {
        success = try_fire(qalu_pipes, NUM_ALU_FUS, entry);
        if (success == -2) {
            break;  // Stop scheduling because all FUs are taken
        }
        entry = entry->next;
    }

    // Schedule from MUL RS
    entry = qmul.head;
    while (entry != NULL) {
        success = try_fire(qmul_pipes, NUM_MUL_FUS, entry);
        if (success == -2) {
            break;  // Stop scheduling because all FUs are taken
        }
        entry = entry->next;
    }

    // Schedule from LS RS
    entry = qlsu.head;
    while (entry != NULL) {
        /************* Memory Disambiguation Logic *************/
        int ok_to_fire = true;
        if (entry->inst->opcode == OPCODE_LOAD) {
            qentry_t *preceding_op_entry = qlsu.head;
            // Check from the start of the RS Queue up to this instruction
            // if there are any stores
            while (preceding_op_entry != entry) {
                if (preceding_op_entry->inst->opcode == OPCODE_STORE) {
                    ok_to_fire = false;
                    break;
                }
                preceding_op_entry = preceding_op_entry->next;
            }
        } else {
            // A store can only occur if it's at the head of the LSU RS
            if (entry != qlsu.head) {
                ok_to_fire = false;
            }
        }
        if (!ok_to_fire) {
            break;
        }
        /*******************************************************/
        success = try_fire(qalu_pipes, NUM_ALU_FUS, entry);
        if (success == -2) {
            break;  // Stop scheduling because all FUs are taken
        }
        entry = entry->next;
    }

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
    while (1) {
        // Don't commit any changes to the queues until we know all conditions are satisfied
        qentry_t *entry = qdis.head;  // Get dispatch queue head
        if (entry == NULL) {
            return;  // Dispatch queue was empty
        }
        const inst_t *inst = entry->inst;

        if (qrob.size >= qrob.max_size) {
            return;  // No room in the ROB
        }

        unsigned long dest_preg_num = -1;
        if (inst->dest >= 0) {
            for (unsigned long i = 32; i < 32 + NUM_PREGS; i++) {
                if (reg_file[i].free) {
                    dest_preg_num = i;
                    break;
                }
            }
            if (dest_preg_num < 0) return;  // No free pregs
        }

        int success = -1;
        switch(inst->opcode) {
            case OPCODE_BRANCH:
                // ??? Check if mispredict here ??? //
                // Fallthrough to ALU
            case OPCODE_ADD:
                if (qalu.size < qalu.max_size) {
                    success = fifo_insert_tail(&qalu, fifo_pop_head(&qdis));
                    if (success != 0) {
                        printf("MY ERROR, why was the ALU RS full?\n");
                        return;
                    }
                } else {
                    return;
                }
                break;
            case OPCODE_MUL:
                if (qmul.size < qmul.max_size) {
                    success = fifo_insert_tail(&qmul, fifo_pop_head(&qdis));
                    if (success != 0) {
                        printf("MY ERROR, why was the MUL RS full?\n");
                        return;
                    }
                } else {
                    return;
                }
                break;
            case OPCODE_LOAD:
            case OPCODE_STORE:
                if (qlsu.size < qlsu.max_size) {
                    success = fifo_insert_tail(&qlsu, fifo_pop_head(&qdis));
                    if (success != 0) {
                        printf("MY ERROR, why was the LSU RS full?\n");
                        return;
                    }
                } else {
                    return;
                }
                break;
            default:
                break;
        }

        // Set physical registers in entry
        if (inst->src1 >= 0) {
            entry->src1_preg = RAT[inst->src1];
        } else {
            entry->src1_preg = -1;
        }

        if (inst->src2 >= 0) {
            entry->src2_preg = RAT[inst->src2];
        } else {
            entry->src2_preg = -1;
        }

        if (inst->dest >= 0) {
            entry->prev_preg = RAT[inst->dest];  // Save previous preg
            entry->dest_preg = dest_preg_num;
            RAT[inst->dest] = dest_preg_num;
            reg_file[dest_preg_num].ready = false;
        } else {
            entry->dest_preg = -1;
        }

        qentry_t *rob_entry = (qentry_t *)malloc(sizeof(qentry_t));
        qentry_copy(entry, rob_entry);
        success = fifo_insert_tail(&qrob, rob_entry);
        if (success != 0) {
            printf("MY ERROR, why was the ROB full?\n");
            return;
        }
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
        qentry_t *entry = (qentry_t *)calloc(1, sizeof(qentry_t));
        entry->inst = inst;
        int success = fifo_insert_tail(&qdis, entry);
        if (success != 0) {
            printf("MY ERROR, why couldn't we add to the dispatch queue?\n");
        }
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
    size_t max_rob_entries = 32 + NUM_PREGS;

    NUM_ALU_FUS = sim_conf->num_alu_fus;
    NUM_MUL_FUS = sim_conf->num_mul_fus;
    NUM_LSU_FUS = sim_conf->num_lsu_fus;

    queue_init(&qdis, -1);
    queue_init(&qalu, NUM_ALU_FUS * sim_conf->num_schedq_entries_per_fu);
    queue_init(&qmul, NUM_MUL_FUS * sim_conf->num_schedq_entries_per_fu);
    queue_init(&qlsu, NUM_LSU_FUS * sim_conf->num_schedq_entries_per_fu);
    queue_init(&qrob, max_rob_entries);

    // Initialize FU pipe queues
    qalu_pipes = (queue_t *)calloc(NUM_ALU_FUS, sizeof(queue_t));
    for (size_t i = 0; i < NUM_ALU_FUS; i++) {
        queue_init(&(qalu_pipes[i]), 1);  // 1 stage pipe
    }

    qmul_pipes = (queue_t *)calloc(NUM_MUL_FUS, sizeof(queue_t));
    for (size_t i = 0; i < NUM_MUL_FUS; i++) {
        queue_init(&(qmul_pipes[i]), 3);  // 3 stage pipe
    }

    qlsu_pipes = (queue_t *)calloc(NUM_LSU_FUS, sizeof(queue_t));
    for (size_t i = 0; i < NUM_LSU_FUS; i++) {
        queue_init(&(qlsu_pipes[i]), 1);  // 1 stage pipe
    }

    // Initialize store buffer
    queue_init(&qstb, max_rob_entries);

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
    printf("End-of-cycle dispatch queue usage: %lu\n", qdis.size); // TODO: Fix Me
    printf("End-of-cycle sched queue usage: %lu\n", qalu.size + qmul.size + qlsu.size); // TODO: Fix Me
    printf("End-of-cycle ROB usage: %lu\n", qrob.size); // TODO: Fix Me
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
