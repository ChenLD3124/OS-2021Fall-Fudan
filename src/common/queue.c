#include <common/queue.h>

static Arena arena;
static bool is_init;
struct queue_op myqueue_op={.push = queue_push,
                             .pop = queue_pop,
                             .front = queue_front,
                             .back = queue_back,
                             .empty = queue_empty};


// initialize a queue
void init_queue(){
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
    init_arena(&arena, sizeof(queue), allocator);
    is_init=1;
}
queue* alloc_queue(){
    assert(is_init==1);
    queue* q=alloc_object(&arena);
    q->op=&myqueue_op;
    init_spinlock(&(q->lock),"queue");
    init_list_node(&q->head);
    return q;
}

// push a node
void queue_push(queue* this,ListNode* node){
    init_list_node(node);
    merge_list(node,&this->head);
}

// pop a node
void queue_pop(queue* this){
    assert(this->op->empty(this)!=1);
    detach_from_list(this->head.next);
}
// first node
ListNode* queue_front(queue* this){
    return this->head.next;
}
// last node
ListNode* queue_back(queue* this){
    return this->head.prev;
}
// empty
bool queue_empty(queue* this){
    return this->head.next==&this->head?true:false;
}