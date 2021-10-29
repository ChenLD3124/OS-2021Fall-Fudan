#include <core/sched.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/virtual_memory.h>
#include <common/spinlock.h>

struct {
    struct proc proc[NPROC];
    SpinLock lock;
} ptable /* DONE: Lab4 multicore: Add locks where needed in this file or others */;

static void scheduler_simple();
static struct proc *alloc_pcb_simple();
static void sched_simple();
static void init_sched_simple();
static void acquire_ptable_lock();
static void release_ptable_lock();
struct sched_op simple_op = {.scheduler = scheduler_simple,
                             .alloc_pcb = alloc_pcb_simple,
                             .sched = sched_simple,
                             .init = init_sched_simple,
							 .acquire_lock = acquire_ptable_lock,
							 .release_lock = release_ptable_lock};
struct scheduler simple_scheduler = {.op = &simple_op};

int nextpid = 1;
void swtch(struct context **, struct context *);

static void init_sched_simple() {
    init_spinlock(&ptable.lock, "ptable");
}

static void acquire_ptable_lock() {
    acquire_spinlock(&ptable.lock);
}

static void release_ptable_lock() {
    release_spinlock(&ptable.lock);
}
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
            acquire_ptable_lock();
            p=&(ptable.proc[i]);
            if(p->state==RUNNABLE){
                if(c->proc)c->proc->state=RUNNABLE;
                p->state=RUNNING;
                uvm_switch(p->pgdir);
                c->proc=p;
                swtch(&(c->scheduler->context),p->context);
                assert(p->state!=RUNNING);
                c->proc=0;
            }
            release_ptable_lock();
        }
    }
}

/*
 * `Swtch` to thiscpu->scheduler.
 */
static void sched_simple() {

    /* DONE: Your code here. */
	if (!holding_spinlock(&ptable.lock)) {
		PANIC("sched: not holding ptable lock");
	}
    if (thiscpu()->proc->state == RUNNING) {
        PANIC("sched: process running");
    }
    /* DONE: Lab3 Schedule */
	swtch(&(thiscpu()->proc->context),thiscpu()->scheduler->context);
}

/* 
 * Allocate an unused entry from ptable.
 * Allocate a new pid for it.
 */
static struct proc *alloc_pcb_simple() {
    /* DONE: Lab3 Schedule */
    acquire_ptable_lock();
    for(int i=0;i<NPROC;i++){
        if(ptable.proc[i].state==UNUSED){
            ptable.proc[i].pid=nextpid++;
            release_ptable_lock();
            return &(ptable.proc[i]);
        }
    }
    release_ptable_lock();
    return 0;
}