#include <common/defines.h>
#include <core/console.h>
#include <core/container.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

#ifdef MULTI_SCHEDULER

/* DONE: Lab6 Add more Scheduler Policies */
static void scheduler_simple(struct scheduler *this);
static struct proc *alloc_pcb_simple(struct scheduler *this);
static void sched_simple(struct scheduler *this);
static void init_sched_simple(struct scheduler *this);
static void acquire_ptable_lock(struct scheduler *this);
static void release_ptable_lock(struct scheduler *this);
struct sched_op simple_op = {.scheduler = scheduler_simple,
                             .alloc_pcb = alloc_pcb_simple,
                             .sched = sched_simple,
                             .init = init_sched_simple,
                             .acquire_lock = acquire_ptable_lock,
                             .release_lock = release_ptable_lock};
struct scheduler simple_scheduler = {.op = &simple_op};

void swtch(struct context **, struct context *);

static void init_sched_simple(struct scheduler *this) {
    init_spinlock(&this->ptable.lock, "ptable");
}

static void acquire_ptable_lock(struct scheduler *this) {
    acquire_spinlock(&this->ptable.lock);
}

static void release_ptable_lock(struct scheduler *this) {
    release_spinlock(&this->ptable.lock);
}

/* 
 * Scheduler yields to its parent scheduler.
 * If this == root, just return.
 * Pay attention to thiscpu() structure and locks.
 */
void yield_scheduler(struct scheduler *this) {
    struct  proc *p=thiscpu()->proc;
    acquire_sched_lock();
    if(this->parent!=this){
        thiscpu()->scheduler=this->parent;
        swtch(this->context[cpuid()],this->parent->context[cpuid()]);
    }
    release_sched_lock();
}

void scheduler_simple(struct scheduler *this) {
    struct proc *p;
    struct cpu *c = thiscpu();
    c->proc = NULL;

    for (;;) {
        /* Loop over process table looking for process to run. */
        for(int i=0;i<NPROC;i++){
            acquire_ptable_lock(this);
            p=&(this->ptable.proc[i]);
            if(p->state==RUNNABLE){
                if(c->proc)c->proc->state=RUNNABLE;
                p->state=RUNNING;
                uvm_switch(p->pgdir);
                c->proc=p;
                swtch(&(c->scheduler->context[cpuid()]),p->context);
                assert(p->state!=RUNNING);
                c->proc=0;
            }
            release_ptable_lock(this);
        }
    }
}

static void sched_simple(struct scheduler *this) {
    if (!holding_spinlock(&(this->ptable.lock))) {
		PANIC("sched: not holding ptable lock");
	}
    if (thiscpu()->proc->state == RUNNING) {
        PANIC("sched: process running");
    }
	swtch(&(thiscpu()->proc->context),thiscpu()->scheduler->context[cpuid()]);
}

static struct proc *alloc_pcb_simple(struct scheduler *this) {
    acquire_ptable_lock(this);
    for(int i=0;i<NPROC;i++){
        if(this->ptable.proc[i].state==UNUSED){
            this->ptable.proc[i].pid=i+1;
            release_ptable_lock(this);
            return &(this->ptable.proc[i]);
        }
    }
    release_ptable_lock(this);
    return 0;
}

#endif
