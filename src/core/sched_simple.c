#include <core/sched.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/virtual_memory.h>

struct {
    struct proc proc[NPROC];
} ptable;

static void scheduler_simple();
static struct proc *alloc_pcb_simple();
static void sched_simple();
struct sched_op simple_op = {
    .scheduler = scheduler_simple, .alloc_pcb = alloc_pcb_simple, .sched = sched_simple};
struct scheduler simple_scheduler = {.op = &simple_op};

int nextpid = 1;
void swtch(struct context **, struct context *);
/*
 * Per-CPU process scheduler
 * Each CPU calls scheduler() after setting itself up.
 * Scheduler never returns.  It loops, doing:
 *  - choose a process to run
 *  - swtch to start running that process
 *  - eventually that process transfers control
 *        via swtch back to the scheduler.
 */
static void scheduler_simple() {
    struct proc *p;
    struct cpu *c = thiscpu();
    c->proc = NULL;

    for (;;) {
        /* Loop over process table looking for process to run. */
        /* DONE: Lab3 Schedule */
        for(int i=0;i<NPROC;i++){
            p=&(ptable.proc[i]);
            if(ptable.proc[i].state==RUNNABLE){
                p->state=RUNNING;
                uvm_switch(p->pgdir);
                c->proc=p;
                swtch(&(c->scheduler->context),p->context);
                assert(p->state!=RUNNING);
                c->proc=0;
            }
        }
    }
}

/*
 * `Swtch` to thiscpu->scheduler.
 */
static void sched_simple() {
    /* DONE: Lab3 Schedule */
	swtch(&(thiscpu()->proc->context),thiscpu()->scheduler->context);
}

/* 
 * Allocate an unused entry from ptable.
 * Allocate a new pid for it.
 */
static struct proc *alloc_pcb_simple() {
    /* DONE: Lab3 Schedule */
    for(int i=0;i<NPROC;i++){
        if(ptable.proc[i].state==UNUSED){
            ptable.proc[i].pid=i+1;
            return &(ptable.proc[i]);
        }
    }
    return 0;
}