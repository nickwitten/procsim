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
    int src1_preg;
    int src2_preg;
    int dest_preg;
    int prev_preg;
    uint8_t exec_cycle;
    bool store_buffer_hit;
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
queue_t qalu_rs;  // ALU RS queue
queue_t qmul_rs;  // MUL RS queue
queue_t qlsu_rs;  // LSU RS queue
queue_t qstb;  // Store Buffer queue
queue_t *qalu_fus;  // List of ALU FU pipes
size_t NUM_ALU_FUS;
queue_t *qmul_fus;  // List of MUL FU pipes
size_t NUM_MUL_FUS;
queue_t *qlsu_fus;  // List of LSU FU pipes
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
    dst->store_buffer_hit = src->store_buffer_hit;
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

/* Search a queue by unique instruction ID and pop that entry
 * Returns NULL when not found
 */
qentry_t *fifo_search_and_pop(queue_t *q, uint64_t dyn_instruction_count) {
    qentry_t *entry = q->head;
    if (entry == NULL) return NULL;
    // Special case it's at the head
    if (entry->inst->dyn_instruction_count == dyn_instruction_count) {
        return fifo_pop_head(q);
    }
    qentry_t *prev = entry;
    entry = entry->next;
    while (entry != NULL) {
        if (entry->inst->dyn_instruction_count == dyn_instruction_count) {
            // Special case it's at the tail
            if (q->tail == entry) {
                q->tail = prev;
            }
            prev->next = entry->next;
            entry->next = NULL;
            return entry;
        }
        prev = entry;
        entry = entry->next;
    }
    return NULL;
}

/* Searches through a list of FU pipelines for a FU with an open
 * first stage.
 * Returns NULL when not found
 */
queue_t *find_free_fu(queue_t *fus, size_t num_fus) {
    for (size_t i = 0; i < num_fus; i++) {
        if (fus[i].size >= fus[i].max_size) continue;
        if (fus[i].tail == NULL || fus[i].tail->exec_cycle >= 0) {
            return &(fus[i]);
        }
    }
    return NULL;
}

/* Copies an entry and places it in the first stage of the function unit */
int insert_entry_copy_pipe_tail(qentry_t *entry, queue_t *fu) {
    qentry_t *pipe_entry = (qentry_t *)malloc(sizeof(qentry_t));
    qentry_copy(entry, pipe_entry);
    pipe_entry->exec_cycle = 0;
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

qentry_t *progress_function_unit(queue_t *fu, size_t pipe_length) {
    qentry_t *entry;

    // If the function unit is empty, don't need to progress anything
    if (fu->head == NULL) {
        return NULL;
    }
    // Progress each entry
    entry = fu->head;
    while (entry != NULL) {
        entry->exec_cycle++;
        /******** Special operations for load **********/
        if (entry->inst->opcode == OPCODE_LOAD && entry->exec_cycle == 1) {
            // Search the store buffer
            qentry_t *stb_entry = qstb.head;
            while (stb_entry != NULL) {
                if (stb_entry->inst->load_store_addr == entry->inst->load_store_addr) {
                    entry->store_buffer_hit = true;
                }
                stb_entry = stb_entry->next;
            }
        }
        /******* Special operations for store ************/
        if (entry->inst->opcode == OPCODE_STORE) {
            fifo_insert_tail(&qstb, entry);
        }
        /*************************************************/
        entry = entry->next;
    }
    // Calculate the deepest entry's finish cycle
    int complete_cycle = pipe_length;
    if (fu->head->inst->dcache_miss) {
        complete_cycle += L1_MISS_PENALTY;
    }
    // Special case for store buffer operations
    if (fu->head->store_buffer_hit || fu->head->inst->opcode == OPCODE_STORE) {
        complete_cycle = 1;  // Finishes immediately
    }
    // If it's completed pop it and return
    if (fu->head->exec_cycle >= complete_cycle) {
        entry = fifo_pop_head(fu);
        if (entry == NULL) printf("MY ERROR, where did the head go?\n");
        return entry;
    }
    return NULL;
}

void progress_function_units(queue_t *rs, queue_t *fus, size_t num_fus, size_t pipe_length) {
    // Allocate entry buffers
    qentry_t *entry;
    qentry_t *entry_tmp;
    // Loop through all FUs
    for (size_t i = 0; i < num_fus; i++) {
        queue_t fu = fus[i];
        // If the function unit is empty, don't need to progress anything
        if (fu.head == NULL) {
            continue;
        }
        // Progress each entry
        entry = fu.head;
        while (entry != NULL) {
            entry->exec_cycle++;
            /******** Special operations for load **********/
            if (entry->inst->opcode == OPCODE_LOAD && entry->exec_cycle == 1) {
                // Search the store buffer
                qentry_t *stb_entry = qstb.head;
                while (stb_entry != NULL) {
                    if (stb_entry->inst->load_store_addr == entry->inst->load_store_addr) {
                        entry->store_buffer_hit = true;
                    }
                    stb_entry = stb_entry->next;
                }
            }
            /******* Special operations for store ************/
            if (entry->inst->opcode == OPCODE_STORE) {
                fifo_insert_tail(&qstb, entry);
            }
            /*************************************************/
            entry = entry->next;
        }
        // Calculate the deepest entry's finish cycle
        int complete_cycle = pipe_length;
        if (fu.head->inst->dcache_miss) {
            complete_cycle += L1_MISS_PENALTY;
        }
        // Special case for store buffer operations
        if (fu.head->store_buffer_hit || fu.head->inst->opcode == OPCODE_STORE) {
            complete_cycle = 1;  // Finishes immediately
        }
        // If it's completed remove it and update the ROB entry
        if (fu.head->exec_cycle >= complete_cycle) {
            entry = fifo_pop_head(&fu);
            if (entry == NULL) printf("MY ERROR, where did the head go?\n");
            // Remove from the RS
            entry_tmp = fifo_search_and_pop(rs, entry->inst->dyn_instruction_count);
            if (entry_tmp == NULL) printf("MY ERROR, where did RS entry go?\n");
            // Copy over to the ROB entry and mark as completed
            entry_tmp = search_queue(&qrob, entry->inst->dyn_instruction_count);
            qentry_copy(entry, entry_tmp);
            entry_tmp->completed = true;  // Mark ROB entry as completed
        }
    }
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

// This will hold the previous number of completed stores and will be updated
// every cycle
int STORES_COMPLETED = 0;
static uint64_t stage_state_update(procsim_stats_t *stats,
                                   bool *retired_mispredict_out) {
    // TODO: fill me in
#ifdef DEBUG
    printf("Stage Retire: \n"); //  PROVIDED
#endif
    qentry_t *entry;  // Variable to hold entries

    // Pop as many stores entries as store instructions were retired last cycle
    for (int i = 0; i < STORES_COMPLETED; i++) {
        entry = fifo_pop_head(&qstb);
        if (entry == NULL) printf("MY ERROR, why is the store buffer empty?\n");
    }

    STORES_COMPLETED = 0;  // Reset
    int completed = 0;

    while (1) {
        entry = qrob.head;  // Keep getting the ROB head
        if (entry == NULL) break;
        if (entry->completed) {
            // Free previous preg if it's not an architectural register
            if (entry->prev_preg >= 32) reg_file[entry->prev_preg].free = true;
            // Set preg to ready
            reg_file[entry->dest_preg].ready = true;
            // Increment counters
            if (entry->inst->opcode == OPCODE_STORE) STORES_COMPLETED++;
            completed++;
            // Remove from the ROB
            entry = fifo_pop_head(&qrob);
            if (entry == NULL) printf("MY ERROR, why isn't it in the ROB?\n");
            // Stop if this instruction was mispredicted
            if (entry->inst->mispredict) {
                *retired_mispredict_out = true;
                break;
            }
        } else {
            break;  // Stop at the first incomplete entry
        }
    }

    return completed;
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
    qentry_t *entry;
    qentry_t *entry_tmp;

    // Progress ALUs
    for (size_t i = 0; i < NUM_ALU_FUS; i++) {
        entry = progress_function_unit(&(qalu_fus[i]), 1);
        if (entry != NULL) {
            // Remove from the RS
            entry_tmp = fifo_search_and_pop(&qalu_rs, entry->inst->dyn_instruction_count);
            if (entry_tmp == NULL) printf("MY ERROR, where did RS entry go?\n");
            // Copy over to the ROB entry and mark as completed
            entry_tmp = search_queue(&qrob, entry->inst->dyn_instruction_count);
            qentry_copy(entry, entry_tmp);
            entry_tmp->completed = true;  // Mark ROB entry as completed
        }
    }

#ifdef DEBUG
    printf("Progressing ALU units\n");  // PROVIDED
#endif

    // Progress MULs
    for (size_t i = 0; i < NUM_MUL_FUS; i++) {
        entry = progress_function_unit(&(qmul_fus[i]), 3);
        if (entry != NULL) {
            // Remove from the RS
            entry_tmp = fifo_search_and_pop(&qmul_rs, entry->inst->dyn_instruction_count);
            if (entry_tmp == NULL) printf("MY ERROR, where did RS entry go?\n");
            // Copy over to the ROB entry and mark as completed
            entry_tmp = search_queue(&qrob, entry->inst->dyn_instruction_count);
            qentry_copy(entry, entry_tmp);
            entry_tmp->completed = true;  // Mark ROB entry as completed
        }
    }

#ifdef DEBUG
    printf("Progressing MUL units\n");  // PROVIDED
#endif

    // Progress Load/Stores
    for (size_t i = 0; i < NUM_LSU_FUS; i++) {
        entry = progress_function_unit(&(qlsu_fus[i]), 2);
        if (entry != NULL) {
            // Remove from the RS
            entry_tmp = fifo_search_and_pop(&qlsu_rs, entry->inst->dyn_instruction_count);
            if (entry_tmp == NULL) printf("MY ERROR, where did RS entry go?\n");
            // Copy over to the ROB entry and mark as completed
            entry_tmp = search_queue(&qrob, entry->inst->dyn_instruction_count);
            qentry_copy(entry, entry_tmp);
            entry_tmp->completed = true;  // Mark ROB entry as completed
        }
    }

#ifdef DEBUG
    printf("Progressing LSU units for loads\n");  // PROVIDED
#endif

#ifdef DEBUG
    printf("Progressing LSU units for stores\n");  // PROVIDED
#endif

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
    entry = qalu_rs.head;
    while (entry != NULL) {
        success = try_fire(qalu_fus, NUM_ALU_FUS, entry);
        if (success == -2) {
            break;  // Stop scheduling because all FUs are taken
        }
        entry = entry->next;
    }

    // Schedule from MUL RS
    entry = qmul_rs.head;
    while (entry != NULL) {
        success = try_fire(qmul_fus, NUM_MUL_FUS, entry);
        if (success == -2) {
            break;  // Stop scheduling because all FUs are taken
        }
        entry = entry->next;
    }

    // Schedule from LS RS
    entry = qlsu_rs.head;
    while (entry != NULL) {
        /************* Memory Disambiguation Logic *************/
        int ok_to_fire = true;
        if (entry->inst->opcode == OPCODE_LOAD) {
            qentry_t *preceding_op_entry = qlsu_rs.head;
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
            if (entry != qlsu_rs.head) {
                ok_to_fire = false;
            }
        }
        if (!ok_to_fire) {
            break;
        }
        /*******************************************************/
        success = try_fire(qlsu_fus, NUM_LSU_FUS, entry);
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
    qentry_t *entry = qdis.head;  // Start at dispatch queue head
    while (entry != NULL) {

        // Don't commit any changes to queues until all conditions satisfied

        const inst_t *inst = entry->inst;  // Used frequently

        // Check if the ROB has room
        if (qrob.size >= qrob.max_size) {
            return;
        }

        // Search for a free preg
        int dest_preg_num = -1;
        if (inst->dest >= 0) {
            for (size_t i = 32; i < 32 + NUM_PREGS; i++) {
                if (reg_file[i].free) {
                    dest_preg_num = i;
                    break;
                }
            }
            if (dest_preg_num < 0) return;  // No free pregs
        }

        // Now queue changes will be committed

        // Add to respective RS
        int success = -1;
        switch(inst->opcode) {
            case OPCODE_BRANCH:
                // ??? Check if mispredict here ??? //
                // Fallthrough to ALU
            case OPCODE_ADD:
                if (qalu_rs.size < qalu_rs.max_size) {
                    success = fifo_insert_tail(&qalu_rs, fifo_pop_head(&qdis));
                    if (success != 0) {
                        printf("MY ERROR, why was the ALU RS full?\n");
                        return;
                    }
                } else {
                    return;
                }
                break;
            case OPCODE_MUL:
                if (qmul_rs.size < qmul_rs.max_size) {
                    success = fifo_insert_tail(&qmul_rs, fifo_pop_head(&qdis));
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
                if (qlsu_rs.size < qlsu_rs.max_size) {
                    success = fifo_insert_tail(&qlsu_rs, fifo_pop_head(&qdis));
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
            reg_file[dest_preg_num].free = false;
            reg_file[dest_preg_num].ready = false;
        } else {
            entry->dest_preg = -1;
        }

        // Allocate an entry in the ROB
        qentry_t *rob_entry = (qentry_t *)malloc(sizeof(qentry_t));
        qentry_copy(entry, rob_entry);
        success = fifo_insert_tail(&qrob, rob_entry);
        if (success != 0) {
            printf("MY ERROR, why was the ROB full?\n");
            return;
        }

        entry = entry->next;
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
    queue_init(&qalu_rs, NUM_ALU_FUS * sim_conf->num_schedq_entries_per_fu);
    queue_init(&qmul_rs, NUM_MUL_FUS * sim_conf->num_schedq_entries_per_fu);
    queue_init(&qlsu_rs, NUM_LSU_FUS * sim_conf->num_schedq_entries_per_fu);
    queue_init(&qrob, max_rob_entries);

    // Initialize FU pipe queues
    qalu_fus = (queue_t *)calloc(NUM_ALU_FUS, sizeof(queue_t));
    for (size_t i = 0; i < NUM_ALU_FUS; i++) {
        queue_init(&(qalu_fus[i]), 1);  // 1 stage pipe
    }

    qmul_fus = (queue_t *)calloc(NUM_MUL_FUS, sizeof(queue_t));
    for (size_t i = 0; i < NUM_MUL_FUS; i++) {
        queue_init(&(qmul_fus[i]), 3);  // 3 stage pipe
    }

    qlsu_fus = (queue_t *)calloc(NUM_LSU_FUS, sizeof(queue_t));
    for (size_t i = 0; i < NUM_LSU_FUS; i++) {
        queue_init(&(qlsu_fus[i]), 1);  // 1 stage pipe
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
    printf("End-of-cycle sched queue usage: %lu\n", qalu_rs.size + qmul_rs.size + qlsu_rs.size); // TODO: Fix Me
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
