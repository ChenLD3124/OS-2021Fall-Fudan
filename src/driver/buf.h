#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/string.h>
#include<common/queue.h>

#define BSIZE 512

#define B_VALID 0x2 /* Buffer has been read from disk. */
#define B_DIRTY 0x4 /* Buffer needs to be written to disk. */

extern queue* buf_queue;

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

void init_bufq();
bool bufq_empty();
struct buf* bufq_front();
struct buf* bufq_back();
void bufq_push(struct buf* buffer);
void bufq_pop();
void acquire_bufq_lock();
void release_bufq_lock();