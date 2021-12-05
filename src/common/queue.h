#pragma once

#include <common/defines.h>
#include<common/spinlock.h>
#include<core/arena.h>
#include<core/physical_memory.h>
#include<common/list.h>
#include<core/console.h>

typedef struct queue {
    ListNode head;
    SpinLock lock;
    struct queue_op *op;
} queue;
struct queue_op{
    void (*pop)(queue* this);
    void (*push)(queue* this,ListNode* node);
    ListNode* (*front)(queue* this);
    ListNode* (*back)(queue* this);
    bool (*empty)(queue* this);
};


// initialize a queue
void init_queue();
void test_queue();
queue* alloc_queue();

// push a node
void queue_push(queue* this,ListNode* node);

// pop a node
void queue_pop(queue* this);
// first node
ListNode* queue_front(queue* this);
// last node
ListNode* queue_back(queue* this);
// empty
bool queue_empty(queue* this);