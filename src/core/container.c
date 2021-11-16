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
    c->p->state=EMBRYO;
    c->p->kstack=kalloc()+PAGE_SIZE;
    if(c->p->kstack==PAGE_SIZE){
        c->p->state=UNUSED;
        return 0;
    }
    void* sp=c->p->kstack;
    sp-=sizeof(context);
    c->p->context=sp;
    c->p->context->x30=(u64)container_entry;
    return c;
}

/*
 * Initialize the container system.
 * Initialize the memory pool and root scheduler.
 */
void init_container() {
    /* TODO: lab6 container */
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
    init_arena(&arena, sizeof(container), allocator);
    root_container=alloc_container(1);
    root_container->parent=root_container;
    // memcpy(&root_container->scheduler,&simple_scheduler,sizeof(scheduler));
}

/* 
 * Allocating resource should be recorded by each ancestor of the process.
 * You can add parameters if needed.
 */
void *alloc_resource(struct container *this, struct proc *p, resource_t resource) {
    /* TODO: lab6 container */

}

/* 
 * Spawn a new process.
 */
struct container *spawn_container(struct container *this, struct sched_op *op) {
    /* TODO: lab6 container */

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
