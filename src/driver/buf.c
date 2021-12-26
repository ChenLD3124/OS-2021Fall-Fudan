#include<driver/buf.h>

queue* buf_queue=0;

void init_bufq(){
    buf_queue=alloc_queue();
    assert(buf_queue!=0);
}

bool bufq_empty(){
    if(buf_queue->op->empty(buf_queue))assert(buf_queue->size==0);
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
}
void bufq_pop(){
    assert(bufq_empty()==0);
    buf_queue->op->pop(buf_queue);
}
void acquire_bufq_lock(){
    acquire_spinlock(&(buf_queue->lock));
}
void release_bufq_lock(){
    release_spinlock(&(buf_queue->lock));
}