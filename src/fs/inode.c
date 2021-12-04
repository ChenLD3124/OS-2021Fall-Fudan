#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <fs/inode.h>

// this lock mainly prevents concurrent access to inode list `head`, reference
// count increment and decrement.
static SpinLock lock;
static ListNode head;

static const SuperBlock *sblock;
static const BlockCache *cache;
static Arena arena;

// return which block `inode_no` lives on.
static INLINE usize to_block_no(usize inode_no) {
    return sblock->inode_start + (inode_no / (INODE_PER_BLOCK));
}

// return the pointer to on-disk inode.
static INLINE InodeEntry *get_entry(Block *block, usize inode_no) {
    return ((InodeEntry *)block->data) + (inode_no % INODE_PER_BLOCK);
}

// return address array in indirect block.
static INLINE u32 *get_addrs(Block *block) {
    return ((IndirectBlock *)block->data)->addrs;
}

// initialize inode tree.
void init_inodes(const SuperBlock *_sblock, const BlockCache *_cache) {
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};

    init_spinlock(&lock, "InodeTree");
    init_list_node(&head);
    sblock = _sblock;
    cache = _cache;
    init_arena(&arena, sizeof(Inode), allocator);

    inodes.root = inodes.get(ROOT_INODE_NO);
}

// initialize in-memory inode.
static void init_inode(Inode *inode) {
    init_spinlock(&inode->lock, "Inode");
    init_rc(&inode->rc);
    init_list_node(&inode->node);
    inode->inode_no = 0;
    inode->valid = false;
}

// see `inode.h`.
static usize inode_alloc(OpContext *ctx, InodeType type) {
    assert(type != INODE_INVALID);
    // DONE:Lab5
    u32 inum;
    Block* bp;
    InodeEntry* sip;
    for(inum=1;inum< sblock->num_inodes;inum++){
        bp=cache->acquire(to_block_no(inum));
        sip=get_entry(bp,inum);
        if(sip->type==INODE_INVALID){
            memset(sip,0,sizeof(InodeEntry));
            sip->type=type;
            cache->sync(ctx,bp);
            cache->release(bp);
            return inum;
        }
        cache->release(bp);
    }

    PANIC("failed to allocate inode on disk");
    return 0;
}

// see `inode.h`.
static void inode_lock(Inode *inode) {
    assert(inode->rc.count > 0);
    acquire_spinlock(&inode->lock);
}

// see `inode.h`.
static void inode_unlock(Inode *inode) {
    assert(holding_spinlock(&inode->lock));
    assert(inode->rc.count > 0);
    release_spinlock(&inode->lock);
}

// see `inode.h`.
static void inode_sync(OpContext *ctx, Inode *inode, bool do_write) {
    // DONE:Lab5
    InodeEntry* sip;
    Block* bp;
    int inum=inode->inode_no;
    bp=cache->acquire(to_block_no(inum));
    sip=get_entry(bp,inum);
    if(inode->valid==1&&do_write==1){
        memmove(sip,&(inode->entry),sizeof(InodeEntry));
        assert(ctx!=NULL);
        cache->sync(ctx,bp);
    }
    else if(inode->valid==0&&do_write==0){
        memmove(&(inode->entry),sip,sizeof(InodeEntry));
        inode->valid=1;
    }
    cache->release(bp);
}

// see `inode.h`.
static Inode *inode_get(usize inode_no) {
    assert(inode_no > 0);
    assert(inode_no < sblock->num_inodes);
    // DONE:Lab5
    Inode* mip;
    acquire_spinlock(&lock);
    ListNode* hp=&head;
    for(ListNode* p=hp->next;p!=hp;p=p->next){
        mip=container_of(p,Inode,node);
        if(mip->valid&&mip->rc.count >0&&mip->inode_no==inode_no){
            increment_rc(&(mip->rc));
            release_spinlock(&lock);
            return mip;
        }
    }
    // alloc inode in list
    mip=(Inode*)alloc_object(&arena);
    assert(mip!=NULL);
    memset(mip,0,sizeof(Inode));
    init_inode(mip);
    increment_rc(&(mip->rc));
    mip->inode_no=inode_no;
    inode_lock(mip);
    inode_sync(NULL,mip,0);
    inode_unlock(mip);
    // mip->valid=1;
    assert(mip->valid==1);
    merge_list(hp,&(mip->node));//head->next=mip->node
    release_spinlock(&lock);
    return mip;
}

// see `inode.h`.
static void inode_clear(OpContext *ctx, Inode *inode) {
    InodeEntry *entry = &inode->entry;

    // DONE:Lab5
    for(int i=0;i<INODE_NUM_DIRECT;i++){
        if(entry->addrs[i]){
            cache->free(ctx,entry->addrs[i]);
            entry->addrs[i]=0;
        }
    }
    if(entry->indirect){
        Block* bp=cache->acquire(entry->indirect);
        IndirectBlock* ibp=(IndirectBlock*)bp;
        for(u32 i=0;i<INODE_NUM_INDIRECT;i++){
            if(ibp->addrs[i]){
                cache->free(ctx,ibp->addrs[i]);
            }
        }
        cache->release(bp);
        cache->free(ctx,entry->indirect);
        entry->indirect=0;
    }
    entry->num_bytes=0;
    inode_sync(ctx,inode,1);
}

// see `inode.h`.
static Inode *inode_share(Inode *inode) {
    acquire_spinlock(&lock);
    increment_rc(&inode->rc);
    release_spinlock(&lock);
    return inode;
}

// see `inode.h`.
static void inode_put(OpContext *ctx, Inode *inode) {
    // DONE:Lab5
    acquire_spinlock(&lock);
    if(inode->rc.count==1&&inode->valid&&inode->entry.num_links==0){
        inode_lock(inode);
        inode_clear(ctx,inode);
        inode->entry.type=INODE_INVALID;
        inode_sync(ctx,inode,1);
        inode->valid=0;
        detach_from_list(&(inode->node));
        inode_unlock(inode);
        free_object(inode);
    }
    else decrement_rc(&(inode->rc));
    release_spinlock(&lock);
}

// this function is private to inode layer, because it can allocate block
// at arbitrary offset, which breaks the usual file abstraction.
//
// retrieve the block in `inode` where offset lives. If the block is not
// allocated, `inode_map` will allocate a new block and update `inode`, at
// which time, `*modified` will be set to true.
// the block number is returned.
//
// NOTE: caller must hold the lock of `inode`.
static usize inode_map(OpContext *ctx, Inode *inode, usize offset, bool *modified) {
    InodeEntry *entry = &inode->entry;
    // DONE:Lab5
    assert(inode->valid!=0);
    if(offset>=INODE_MAX_BYTES)PANIC("inode_map range error!");
    usize block_no,bnum=offset/BLOCK_SIZE;
    IndirectBlock* iaddrp;
    Block* bp;
    if(bnum<INODE_NUM_DIRECT){
        if(entry->addrs[bnum]==0){
            entry->addrs[bnum]=cache->alloc(ctx);
            *modified=1;
        }
        block_no=inode->entry.addrs[bnum];
    }
    else{
        if(entry->indirect==0){
            entry->indirect=cache->alloc(ctx);
            *modified=1;
        }
        bp=cache->acquire(entry->indirect);
        iaddrp=(IndirectBlock*)bp;
        if(iaddrp->addrs[bnum-INODE_NUM_DIRECT]==0){
            iaddrp->addrs[bnum-INODE_NUM_DIRECT]=cache->alloc(ctx);
            *modified=1;
            cache->sync(ctx,bp);
        }
        block_no=iaddrp->addrs[bnum-INODE_NUM_DIRECT];
        cache->release(bp);
    }
    if(*modified){
        assert(ctx!=NULL);
        inode_sync(ctx,inode,1);
    }
    return block_no;
}

// see `inode.h`.
static void inode_read(Inode *inode, u8 *dest, usize offset, usize count) {
    InodeEntry *entry = &inode->entry;
    usize end = offset + count;
    assert(offset <= entry->num_bytes);
    assert(end <= entry->num_bytes);
    assert(offset <= end);
    // DONE:Lab5
    assert(inode->valid!=0);
    usize step=0,bpnum,tot=0;
    bool modified=0;
    Block* bp;
    for(usize p=offset;p<end;p+=step){
        step=MIN(BLOCK_SIZE-p%BLOCK_SIZE,end-p);
        bpnum=inode_map(NULL,inode,p,&modified);
        bp=cache->acquire(bpnum);
        memmove(dest+tot,bp->data+p%BLOCK_SIZE,step);
        cache->release(bp);
        tot+=step;
    }
    if(modified)PANIC("inode read edit inode!");
}

// see `inode.h`.
static void inode_write(OpContext *ctx, Inode *inode, u8 *src, usize offset, usize count) {
    InodeEntry *entry = &inode->entry;
    usize end = offset + count;
    assert(offset <= entry->num_bytes);
    assert(end <= INODE_MAX_BYTES);
    assert(offset <= end);
    // DONE:Lab5
    assert(inode->valid!=0);
    usize step=0,bpnum,tot=0;
    bool modified=0;
    Block* bp;
    for(usize p=offset;p<end;p+=step){
        step=MIN(BLOCK_SIZE-p%BLOCK_SIZE,end-p);
        bpnum=inode_map(ctx,inode,p,&modified);
        bp=cache->acquire(bpnum);
        memmove(bp->data+p%BLOCK_SIZE,src+tot,step);
        cache->sync(ctx,bp);
        cache->release(bp);
        tot+=step;
    }
    entry->num_bytes=MAX(end,entry->num_bytes);
    inode_sync(ctx,inode,1);
}

// see `inode.h`.
static usize inode_lookup(Inode *inode, const char *name, usize *index) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    // DONE:Lab5
    assert(inode->valid!=0);
    DirEntry direntry;
    if(strlen(name)>FILE_NAME_MAX_LENGTH)return 0;
    for(usize off=0;off<entry->num_bytes;off+=sizeof(DirEntry)){
        
        inode_read(inode,(u8*)(&direntry),off,sizeof(DirEntry));
        if(direntry.inode_no==0)continue;
        if(strncmp(direntry.name,name,FILE_NAME_MAX_LENGTH)==0){
            if(index!=NULL)*index=off;
            return direntry.inode_no;
        }
    }
    return 0;
}

// see `inode.h`.
static usize inode_insert(OpContext *ctx, Inode *inode, const char *name, usize inode_no) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);
    // DONE:Lab5
    assert(inode->valid!=0);
    assert(inode_no!=0);
    usize off=0;
    /*inode_lookup(inode,name,&off);
    if(off!=0){
        inode_put(ctx,inode);
        return -1;
    }*/
    DirEntry direntryin,direntryout;
    strncpy(direntryin.name,name,FILE_NAME_MAX_LENGTH);
    direntryin.inode_no=inode_no;
    assert(strlen(name)<=FILE_NAME_MAX_LENGTH);
    for(off=0;off<entry->num_bytes;off+=sizeof(DirEntry)){
        inode_read(inode,(u8*)(&direntryout),off,sizeof(DirEntry));
        if(direntryout.inode_no==0){
            inode_write(ctx,inode,(u8*)(&direntryin),off,sizeof(DirEntry));
            return off;
        }
    }
    //do not have space
    if(off==entry->num_bytes&&off+sizeof(DirEntry)<INODE_MAX_BYTES){
        inode_write(ctx,inode,(u8*)&direntryin,off,sizeof(DirEntry));
        return off;
    }
    PANIC("not find space for direntry!");
    return 0;
}

// see `inode.h`.
static void inode_remove(OpContext *ctx, Inode *inode, usize index) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    // DONE:Lab5
    DirEntry direntry;
    memset((u8*)&direntry,0,sizeof(direntry));
    inode_write(ctx,inode,(u8*)&direntry,index,sizeof(DirEntry));
}

InodeTree inodes = {
    .alloc = inode_alloc,
    .lock = inode_lock,
    .unlock = inode_unlock,
    .sync = inode_sync,
    .get = inode_get,
    .clear = inode_clear,
    .share = inode_share,
    .put = inode_put,
    .read = inode_read,
    .write = inode_write,
    .lookup = inode_lookup,
    .insert = inode_insert,
    .remove = inode_remove,
};
