
#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/sched.h>
#include <core/sched_simple.c>
#include <core/console.h>
#include<common/myfunc.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <core/virtual_memory.h>
#include <driver/sd.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>

void forkret();
void wakeup_inlock(void *chan);
extern void trap_return();
static void free_proc(struct proc *p){
  if(p->kstack)kfree((void*)p->kstack);
  if(p->pgdir)vm_free(p->pgdir);
  p->tf= 0;
  p->pgdir = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->state = UNUSED;
}
/*
 * Look through the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state (allocate stack, clear trapframe, set context for switch...)
 * required to run in the kernel. Otherwise return 0.
 * Step 1 (TODO): Call `alloc_pcb()` to get a pcb.
 * Step 2 (TODO): Set the state to `EMBRYO`.
 * Step 3 (TODO): Allocate memory for the kernel stack of the process.
 * Step 4 (TODO): Reserve regions for trapframe and context in the kernel stack.
 * Step 5 (TODO): Set p->tf and p->context to the start of these regions.
 * Step 6 (TODO): Clear trapframe.
 * Step 7 (TODO): Set the context to work with `swtch()`, `forkret()` and `trap_return()`.
 */
static struct proc *alloc_proc() {
    struct proc *p;
    /* DONE: Lab3 Process */
    p=alloc_pcb();

    if(p==0)return 0;
    p->kstack=kalloc();
    if(p->kstack==0){
        p->state=UNUSED;
        PANIC("no kstack!!");
        return 0;
    }
    void* sp=p->kstack+PAGE_SIZE;
    sp-=sizeof(Trapframe);
    p->tf=sp;
    sp-=sizeof(context);
    p->context=sp;
    p->base=0;
    memset(p->tf,0,sizeof(Trapframe));
    p->context->x30=(u64)forkret;
    return p;
}

/*
 * Set up first user process(Only used once).
 * Step 1: Allocate a configured proc struct by `alloc_proc()`.
 * Step 2 (TODO): Allocate memory for storing the code of init process.
 * Step 3 (TODO): Copy the code (ranging icode to eicode) to memory.
 * Step 4 (TODO): Map any va to this page.
 * Step 5 (TODO): Set the address after eret to this va.
 * Step 6 (TODO): Set proc->sz.
 */
struct proc* initproc;
void spawn_init_process() {
    struct proc *p;
    // extern char loop_start[], loop_end[];
    extern char icode[], eicode[];
    p = alloc_proc();
    if(p==0)PANIC("alloc proc fail!!");
    /* DONE: Lab3 Process */
    p->pgdir=pgdir_init();
    char * initcode=(char*)kalloc();
    assert(initcode!=0);
    // memcpy(initcode,loop_start,loop_end-loop_start);
    memcpy(initcode,icode,eicode-icode);
    strncpy(p->name,"init_process",sizeof(p->name));
    uvm_map(p->pgdir,(void*)0,PAGE_SIZE,K2P(initcode));
    p->tf->ELR_EL1=0;
    p->tf->SP_EL0=PAGE_SIZE;
    p->tf->x30=0;
    p->sz=PAGE_SIZE;
    p->parent=0;
    OpContext ctx;
    bcache.begin_op(&ctx);
    p->cwd=namei("/",&ctx);
    bcache.end_op(&ctx);
    initproc=p;
    p->state=RUNNABLE;
}
void spawn_idle_process() {
    struct proc *p;
    // extern char loop_start[], loop_end[];
    extern char ispin[], eicode[];
    p = alloc_proc();
    if(p==0)PANIC("alloc proc fail!!");
    /* DONE: Lab3 Process */
    p->pgdir=pgdir_init();
    char * initcode=(char*)kalloc();
    assert(initcode!=0);
    // memcpy(initcode,loop_start,loop_end-loop_start);
    memcpy(initcode,ispin,eicode-ispin);
    strncpy(p->name,"idle_process",sizeof(p->name));
    uvm_map(p->pgdir,(void*)0,PAGE_SIZE,K2P(initcode));
    p->tf->ELR_EL1=0;
    p->tf->SP_EL0=PAGE_SIZE;
    p->tf->x30=0;
    p->sz=PAGE_SIZE;
    p->parent=0;
    p->state=RUNNABLE;
}
/*
 * A fork child will first swtch here, and then "return" to user space.
 */
static int sd_test_num=0;

void forkret() {
	/* DONE: Lab3 Process */
    u32 now=sd_test_num;
    sd_test_num=1;
    release_sched_lock();
    if(now==0){init_filesystem();spawn_init_process();}//sd_test();//
    return;
}

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 * 
 * In Lab9, you should add the following:
 * (1) close open files
 * (2) release inode `pwd`
 * (3) wake up its parent
 * (4) pass its children to `init` process
 * 
 * Why not set the state to UNUSED in this function?
 */
void exit() {
    // acquire_sched_lock();
    struct proc *p = thiscpu()->proc;
    struct proc *ptable=thiscpu()->scheduler->ptable.proc;
    asserts(p!=initproc,"init proc exit!");
    /* DONE: Lab3 Process */
    /* DONE: Lab9 Shell */
    for(int i=0;i<NOFILE;i++){
        if(p->ofile[i]){
            fileclose(p->ofile[i]);
            p->ofile[i]=0;
        }
    }
    OpContext ctx;
    bcache.begin_op(&ctx);
    inodes.put(&ctx,p->cwd);
    bcache.end_op(&ctx);
    p->cwd=0;
    acquire_sched_lock();
    wakeup_inlock(p->parent);
    for(struct proc* np=ptable;np<ptable+NPROC;np++){
        if(np->parent==p){
            np->parent=initproc;
            wakeup_inlock(p->parent);
        }
    }
	p->state=ZOMBIE;
    sched();
    _assert(1==0,"zombie exit!");
}

/*
 * Give up CPU.
 * Switch to the scheduler of this proc.
 */
void yield() {
    /* DONE: lab6 container */
    acquire_sched_lock();
    struct  proc *p=thiscpu()->proc;
    p->state=RUNNABLE;
    sched();
    release_sched_lock();
}

/*
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void sleep(void *chan, SpinLock *lock) {
    /* DONE: lab6 container */
    SpinLock *lk=&(thiscpu()->scheduler->ptable.lock);
    if(lock!=lk){
        acquire_sched_lock();
        release_spinlock(lock);
    }
    struct proc* p=thiscpu()->proc;
    p->chan=chan;
    p->state=SLEEPING;
    sched();
    p->chan=0;
    if(lock!=lk){
        release_sched_lock();
        acquire_spinlock(lock);
    }
}

/* Wake up all processes sleeping on chan. */
void wakeup_inlock(void *chan){
    struct proc* cp=thiscpu()->proc;
    struct proc* p;
    for(int i=0;i<NPROC;i++){
        p=&thiscpu()->scheduler->ptable.proc[i];
        if(p!=cp&&p->state==SLEEPING&&p->chan==chan){
            p->state=RUNNABLE;
        }
    }
}
void wakeup(void *chan) {
    /* DONE: lab6 container */
    acquire_sched_lock();
    wakeup_inlock(chan);
    release_sched_lock();
}

/*
 * Add process at thiscpu()->container,
 * execute code in src/user/loop.S
 */
void add_loop_test(int times) {
    for (int i = 0; i < times; i++) {
        /* DONE: lab6 container */
        spawn_init_process();
    }
}

/*
 * Call allocuvm or deallocuvm.
 * This function is used in `sys_brk`.
 */
int growproc(int n) {
	/* DONE: lab9 shell */
    u32 sz;
    struct proc* p=thiscpu()->proc;
    sz=p->sz;
    if(n>0){
        if((sz=uvm_alloc(p->pgdir,p->base,p->stksz,sz,sz+n))==0)return -1;
    }
    else if(n<0){
        sz=uvm_dealloc(p->pgdir,p->base,sz,sz+n);
    }
    p->sz=sz;
    return 0;
}

/*
 * Create a new process copying p as the parent.
 * Sets up stack to return as if from system call.
 * Caller must set state of returned proc to RUNNABLE.
 * 
 * Don't forget to copy file descriptors and `cwd` inode.
 */
int fork() {
    /* DONE: Lab9 shell */
    struct proc* np=NULL;
    struct proc* p=thiscpu()->proc;
    if((np=alloc_proc())==0)return -1;
    np->parent=p;
    np->pgdir=uvm_copy(p->pgdir);
    if(np->pgdir==NULL){
        free_proc(np);
        return -1;
    }
    np->sz=p->sz;
    *(np->tf)=*(p->tf);
    np->tf->x0=0;
    for(int i=0;i<NOFILE;i++){
        if(p->ofile[i])np->ofile[i]=filedup(p->ofile[i]);
    }
    np->cwd=inodes.share(p->cwd);
    strncpy(np->name,p->name,sizeof(p->name));
    int pid=np->pid;
    np->state=RUNNABLE;
    return pid;
}

/*
 * Wait for a child process to exit and return its pid.
 * Return -1 if this process has no children.
 * 
 * You can release the PCB (set state to UNUSED) of its dead children.
 */
int wait() {
    /* DONE: Lab9 shell. */
    acquire_sched_lock();
    struct proc* np;
    struct proc *p=thiscpu()->proc;
    struct proc *ptable=thiscpu()->scheduler->ptable.proc;
    SpinLock* lk=&(thiscpu()->scheduler->ptable.lock);
    bool havekid;
    int pid;
    while(1){
        havekid=0;
        for(np=ptable;np<ptable+NPROC;np++){
            if(np->parent!=p)continue;
            havekid=1;
            if(np->state==ZOMBIE){
                pid=np->pid;
                free_proc(np);
                release_sched_lock();
                return pid;
            }
        }
        if(havekid==0||p->killed){
            release_sched_lock();
            return -1;
        }
        sleep(p,lk);
    }
    return 0;
}
