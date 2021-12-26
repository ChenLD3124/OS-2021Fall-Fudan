#include <common/bitmap.h>
#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <fs/cache.h>

static const SuperBlock *sblock;
static const BlockDevice *device;

static SpinLock lock;     // protects block cache.
static Arena arena;       // memory pool for `Block` struct.
static ListNode head;     // the list of all allocated in-memory block.
static LogHeader header;  // in-memory copy of log header block.

// hint: you may need some other variables. Just add them here.
static u32 bcache_num;
struct log{
    SpinLock lock;
    bool is_commiting;
    u32 outstanding;//num of op dont into end
    usize start,maxsize,mayuse;
}log;


// read the content from disk.
static INLINE void device_read(Block *block) {
    device->read(block->block_no, block->data);
}

// write the content back to disk.
static INLINE void device_write(Block *block) {
    device->write(block->block_no, block->data);
}

// read log header from disk.
static INLINE void read_header() {
    device->read(sblock->log_start, (u8 *)&header);
}

// write log header back to disk.
static INLINE void write_header() {
    device->write(sblock->log_start, (u8 *)&header);
}

// initialize block cache.
static void log_to_disk();
void log_replay(){
    read_header();
    if(header.num_blocks==0)return;
    log_to_disk();
    header.num_blocks=0;
    write_header();
}
static void init_log(){
    header.num_blocks=0;
    log.is_commiting=0;
    log.outstanding=0;
    log.maxsize=MIN(sblock->num_log_blocks-1,LOG_MAX_SIZE);
    log.start=sblock->log_start;
    log.mayuse=0;
    init_spinlock(&log.lock,"log");
    log_replay();
}
void init_bcache(const SuperBlock *_sblock, const BlockDevice *_device) {
    sblock = _sblock;
    device = _device;

    // DONE:
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
    init_arena(&arena, sizeof(Block), allocator);
    init_spinlock(&lock,"bcache");
    init_list_node(&head);
    bcache_num=0;
    init_log();
}

// initialize a block struct.
static void init_block(Block *block) {
    block->block_no = 0;
    init_list_node(&block->node);
    // block->acquired = false;
    block->refcnt=0;
    block->pinned = false;

    init_sleeplock(&block->lock, "block");
    block->valid = false;
    asserts((u64)(block->data)%8==0,"block->data should Alignment");
    memset(block->data, 0, sizeof(block->data));
}

// see `cache.h`.
static usize get_num_cached_blocks() {
    // DONE:
    acquire_spinlock(&lock);
    usize cnt=bcache_num;
    release_spinlock(&lock);
    return cnt;
}

//myfunc ,with cache lock
static void cache_limit_capa(){
    Block* b=NULL;
    for(ListNode *pre=head.prev;bcache_num>=
        EVICTION_THRESHOLD&&pre!=&head;){
        b=container_of(pre,Block,node);
        pre=pre->prev;
        if(b->refcnt==0&&b->pinned==false){
            detach_from_list(&b->node);
            bcache_num--;
            free_object(b);
        }
    }
}
// see `cache.h`.
// cld:this dont sync with header,other log block in cache sync
static Block *cache_acquire(usize block_no) {
    // DONE:
    acquire_spinlock(&lock);
    //find
    Block* b=NULL;
    for(ListNode* nxt=head.next;nxt!=&head;nxt=nxt->next){
        if(container_of(nxt,Block,node)->block_no==block_no){
            b=container_of(nxt,Block,node);
            detach_from_list(&b->node);//LRU
            break;
        }
    }
    if(b==NULL){
        cache_limit_capa();//with cache lock,try to limit capa
        b=alloc_object(&arena);asserts(b!=NULL,"have no cache block");
        init_block(b);
        b->block_no=block_no;
        bcache_num++;
    }
    merge_list(&head,&b->node);
    b->refcnt++;
    release_spinlock(&lock);
    acquire_sleeplock(&b->lock);
    // b->acquired=true;
    if(b->valid==false){
        device_read(b);
        b->valid=true;
    }
    return b;
}

// see `cache.h`.
static void cache_release(Block *block) {
    // DONE:
    release_sleeplock(&block->lock);
    acquire_spinlock(&lock);
    assert(block->refcnt>0);
    block->refcnt--;
    release_spinlock(&lock);
}

// see `cache.h`.
static void cache_begin_op(OpContext *ctx) {
    // DONE:
    acquire_spinlock(&log.lock);
    while(1){
        if(log.is_commiting||header.num_blocks+log.mayuse+OP_MAX_NUM_BLOCKS>log.maxsize){
            sleep(&log,&log.lock);
        }
        else{
            log.outstanding++;
            log.mayuse+=OP_MAX_NUM_BLOCKS;
            ctx->ts=log.outstanding;
            ctx->remain_num=OP_MAX_NUM_BLOCKS;
            break;
        }
    }
    release_spinlock(&log.lock);
}

// see `cache.h`.
static void cache_sync(OpContext *ctx, Block *block) {
    if (ctx) {
        // DONE:
        acquire_spinlock(&log.lock);
        assert(log.outstanding>0);
        assert(header.num_blocks<log.maxsize);
        for(int i=0;i<header.num_blocks;i++){
            if(header.block_no[i]==block->block_no){
                release_spinlock(&log.lock);
                return;
            }
        }
        if(ctx->remain_num>0){
            ctx->remain_num--;
            log.mayuse--;
        }
        else PANIC("op block num exceed");
        header.block_no[header.num_blocks++]=block->block_no;
        release_spinlock(&log.lock);
        acquire_spinlock(&lock);
        block->pinned=1;
        release_spinlock(&lock);
    } else
        device_write(block);
}

static void log_to_disk(){
    Block *src_block,*dst_block;
    for(int i=0;i<header.num_blocks;i++){
        src_block=cache_acquire(log.start+i+1);
        dst_block=cache_acquire(header.block_no[i]);
        memcpy(dst_block->data,src_block->data,BLOCK_SIZE);
        cache_sync(NULL,dst_block);
        acquire_spinlock(&lock);
        dst_block->pinned=0;
        release_spinlock(&lock);
        cache_release(src_block);
        cache_release(dst_block);
    }
}
static void write_log(){
    Block *src_block,*dst_block;
    for(int i=0;i<header.num_blocks;i++){
        src_block=cache_acquire(header.block_no[i]);
        dst_block=cache_acquire(log.start+i+1);
        memcpy(dst_block->data,src_block->data,BLOCK_SIZE);
        cache_sync(NULL,dst_block);
        cache_release(src_block);
        cache_release(dst_block);
    }
}
static void log_commit(){
    if(header.num_blocks==0)return;
    write_log();
    write_header();
    log_to_disk();
    header.num_blocks=0;
    write_header();
}
// see `cache.h`.
static void cache_end_op(OpContext *ctx) {
    // DONE:
    bool is_commit=0;
    acquire_spinlock(&log.lock);
    log.outstanding-=1;
    log.mayuse-=ctx->remain_num;
    if(log.outstanding==0){
        is_commit=1;
        log.is_commiting=1;
    }
    else{
        wakeup(&log);
        sleep(&log.outstanding,&log.lock);
    }
    release_spinlock(&log.lock);
    if(is_commit==1){
        log_commit();
        acquire_spinlock(&log.lock);
        log.is_commiting=0;
        wakeup(&log.outstanding);
        wakeup(&log);
        release_spinlock(&log.lock);
    }
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static void block_clear(OpContext* ctx,usize block_no){
    Block* b=cache_acquire(block_no);
    memset(b->data,0,BLOCK_SIZE);
    cache_sync(ctx,b);
    cache_release(b);
}
static usize cache_alloc(OpContext *ctx) {
    // DONE:
    Block* b;
    for(usize i=0,j=0;i<sblock->num_blocks;i+=BIT_PER_BLOCK,j+=1){
        b=cache_acquire(sblock->bitmap_start+j);
        for(u32 k=0;k<BIT_PER_BLOCK&&i+k<sblock->num_blocks;k++){
            u32 offset=1<<(k%8);
            if((b->data[k/8]&offset)==0){
                b->data[k/8]|=offset;
                cache_sync(ctx,b);
                cache_release(b);
                block_clear(ctx,i+k);
                return i+k;
            }
        }
        cache_release(b);
    }
    PANIC("cache_alloc: no free block");
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static void cache_free(OpContext *ctx, usize block_no) {
    // DONE:
    Block* b=cache_acquire(sblock->bitmap_start+block_no/BIT_PER_BLOCK);
    u32 id,offset;
    id=block_no%BIT_PER_BLOCK/8;
    offset=1<<(block_no%BIT_PER_BLOCK%8);
    assert((b->data[id]&offset)!=0);
    b->data[id]^=offset;
    cache_sync(ctx,b);
    cache_release(b);
}

BlockCache bcache = {
    .get_num_cached_blocks = get_num_cached_blocks,
    .acquire = cache_acquire,
    .release = cache_release,
    .begin_op = cache_begin_op,
    .sync = cache_sync,
    .end_op = cache_end_op,
    .alloc = cache_alloc,
    .free = cache_free,
};
