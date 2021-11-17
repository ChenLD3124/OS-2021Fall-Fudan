#include <common/string.h>
#include <core/arena.h>
#include <core/container.h>
#include <core/physical_memory.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

struct container *root_container = 0;
static Arena arena;
bool do_cont_test = false;

extern void add_loop_test(int times);

/* 
 * The entry of a new spawned scheduler.
 * Similar to `forkret`.
 * Maintain thiscpu()->scheduler.
 */
static NO_RETURN void container_entry() {
    /* DONE: lab6 container */
    release_sched_lock();
    thiscpu()->scheduler=&((container*)thiscpu()->proc->cont)->scheduler;
    enter_scheduler();
	/* container_entry should enter scheduler and should not return */
    PANIC("scheduler should not return");
}

/* 
 * Allocate memory for a container.
 * For root container, a container scheduler is enough. 
 * Memory of struct proc is from another ptable, if root is false.
 * Similar to `alloc_proc`.
 * Initialize some pointers.
 */
struct container *alloc_container(bool root) {
    /* DONE: lab6 container */
    struct container *c;
    c=(container*)alloc_object(&arena);
    if(root==1)return c;

    c->p=alloc_pcb();
    if(c->p==0)return 0;
    c->p->is_scheduler=1;
    c->p->cont=c;
    c->p->state=EMBRYO;
    c->p->kstack=kalloc()+PAGE_SIZE;
    if(c->p->kstack==PAGE_SIZE){
        c->p->state=UNUSED;
        return 0;
    }
    void* sp=c->p->kstack;
    for(int i=0;i<NCPU;i++){
        sp-=sizeof(context);
        c->scheduler.context[i]=sp;
        c->scheduler.context[i]->x30=(u64)container_entry;
    }
    return c;
}

/*
 * Initialize the container system.
 * Initialize the memory pool and root scheduler.
 */
void init_container() {
    /* DONE: lab6 container */
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
    init_arena(&arena, sizeof(container), allocator);
    root_container=alloc_container(1);
    root_container->parent=root_container;
    root_container->scheduler.cont=&root_container;
    root_container->scheduler.op=&simple_op;
    root_container->scheduler.op->init();
}

/* 
 * Allocating resource should be recorded by each ancestor of the process.
 * You can add parameters if needed.
 */
void *alloc_resource(struct container *this, struct proc *p, resource_t resource) {
    /* DONE: lab6 container */
    void* trn;
    switch(resource){
        case PID:{
            int i;
            for(i=0;i<NPID;i++){
                if(this->pmap[i].valid==1)continue;
                this->pmap[i].valid=1;
                this->pmap[i].pid_local=++(this->scheduler.pid);
                this->pmap[i].p=p;
            }
            if(i==NPID)PANIC("has no pid map!!");
            trn=(void*)i;
            break;
        }
        default:trn=(void*)0;
    }
    if(this->parent!=this)alloc_resource(this->parent,p,resource);
    return (void*)this->scheduler.pid;
}

/* 
 * Spawn a new process.
 */
struct container *spawn_container(struct container *this, struct sched_op *op) {
    /* DONE: lab6 container */
    container* c=alloc_container(0);
    if(c==0)PANIC("alloc_container fail!");
    c->parent=this;
    c->scheduler.cont=c;
    c->scheduler.op=op;
    c->scheduler.op->init();

    strncpy(c->p->name,"container",sizeof(c->p->name));
    c->p->parent=this->p;
    c->p->state=RUNNABLE;
}

/*
 * Add containers for test
 */
void container_test_init() {
    struct container *c;

    do_cont_test = true;
    add_loop_test(1);
    c = spawn_container(root_container, &simple_op);
    assert(c != NULL);
}
