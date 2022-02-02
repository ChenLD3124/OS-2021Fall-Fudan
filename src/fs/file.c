/* File descriptors */

#include "file.h"
#include "fs.h"
#include <common/defines.h>
#include <common/spinlock.h>
#include <core/console.h>
#include <core/sleeplock.h>
#include <fs/inode.h>

// struct devsw devsw[NDEV];
struct {
    struct SpinLock lock;
    struct file file[NFILE];
} ftable;

/* Optional since BSS is zero-initialized. */
void fileinit() {
    init_spinlock(&ftable.lock, "file table");
}

/* Allocate a file structure. */
struct file *filealloc() {
    /* DONE: Lab9 Shell */
    struct file* f=NULL;
    acquire_spinlock(&ftable.lock);
    for(u32 i=0;i<NFILE;i++){
        if(ftable.file[i].ref==0){
            ftable.file[i].ref=1;
            f=&ftable.file[i];
            release_spinlock(&ftable.lock);
            return f;
        }
    }
    release_spinlock(&ftable.lock);
    return NULL;
}

/* Increment ref count for file f. */
struct file *filedup(struct file *f) {
    /* DONE: Lab9 Shell */
    acquire_spinlock(&ftable.lock);
    asserts(f!=NULL&&f->ref>0,"file dup err");
    f->ref++;
    release_spinlock(&ftable.lock);
    return f;
}

/* Close file f. (Decrement ref count, close when reaches 0.) */
void fileclose(struct file *f) {
    /* DONE: Lab9 Shell */
    struct file ff;
    acquire_spinlock(&ftable.lock);
    asserts(f!=NULL&&f->ref>0,"fileclose err");
    f->ref--;
    if(f->ref>0){
        release_spinlock(&ftable.lock);
        return;
    }
    ff=*f;
    f->type=FD_NONE;
    release_spinlock(&ftable.lock);
    if(ff.type==FD_INODE){
        OpContext ctx;
        bcache.begin_op(&ctx);
        inodes.put(&ctx,ff.ip);
        bcache.end_op(&ctx);
    }
    else PANIC("ff.type==%d\n",ff.type);
}

/* Get metadata about file f. */
int filestat(struct file *f, struct stat *st) {
    /* DONE: Lab9 Shell */
    asserts(f!=NULL&&f->type==FD_INODE,"filestat err");
    inodes.lock(f->ip);
    stati(f->ip,st);
    inodes.unlock(f->ip);
    return 0;
}

/* Read from file f. */
isize fileread(struct file *f, char *addr, isize n) {
    /* DONE: Lab9 Shell */
    if(f->readable==0)return -1;
    asserts(f!=NULL&&f->type==FD_INODE&&n>=0,"fileread");
    int inc=0;
    inodes.lock(f->ip);
    inc=inodes.read(f->ip,addr,f->off,n);
    f->off+=inc;
    inodes.unlock(f->ip);
    return 0;
}

/* Write to file f. */
isize filewrite(struct file *f, char *addr, isize n) {
    /* DONE: Lab9 Shell */
    if(f->writable==0)return -1;
    asserts(f->type==FD_INODE,"filewrite");
    int maxn=(OP_MAX_NUM_BLOCKS-1-1-2)/2*BLOCK_SIZE;
    OpContext ctx;
    for(int i=0,j,inc;i<n;i+=j){
        j=MIN(n-i,maxn);
        bcache.begin_op(&ctx);
        inodes.lock(f->ip);
        inc=inodes.write(&ctx,f->ip,addr+i,f->off,j);
        f->off+=inc;
        inodes.unlock(f->ip);
        bcache.end_op(&ctx);
        if(j!=inc)return -1;
    }
    return 0;
}
