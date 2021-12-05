#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/string.h>
#include<common/queue.h>

#define BSIZE 512

#define B_VALID 0x2 /* Buffer has been read from disk. */
#define B_DIRTY 0x4 /* Buffer needs to be written to disk. */

queue* buf_queue=0;
int buf_queue_num;
struct buf {
    int flags;
    u32 blockno;
    u8 data[BSIZE];  // 1B*512

    /* 
     * Add other necessary elements. It depends on you.
     */
    /* DONE: Lab7 driver. */
    ListNode node;

};

/* 
 * Add some useful functions to use your buffer list, such as push, pop and so on.
 */

/* DONE: Lab7 driver. */

void init_bufq(){
    buf_queue=alloc_queue();
    buf_queue_num=0;
    assert(buf_queue!=0);
}

bool bufq_empty(){
    if(buf_queue->op->empty(buf_queue))assert(buf_queue_num==0);
    return buf_queue->op->empty(buf_queue);
}
struct buf* bufq_front(){
    assert(bufq_empty()==0);
    return container_of(buf_queue->op->front(buf_queue),struct buf,node);
}
struct buf* bufq_back(){
    return container_of(buf_queue->op->back(buf_queue),struct buf,node);
}
void bufq_push(struct buf* buffer){
    buf_queue->op->push(buf_queue,&buffer->node);
    buf_queue_num++;
}
void bufq_pop(){
    assert(bufq_empty()==0);
    buf_queue->op->pop(buf_queue);
    buf_queue_num--;
}
void acquire_bufq_lock(){
    acquire_spinlock(&(buf_queue->lock));
}
void release_bufq_lock(){
    release_spinlock(&(buf_queue->lock));
}