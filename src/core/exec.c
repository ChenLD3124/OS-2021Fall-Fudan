#include <elf.h>

#include "trap.h"

#include <fs/file.h>
#include <fs/inode.h>

#include <aarch64/mmu.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/virtual_memory.h>
#include <common/string.h>

#include <elf.h>

static u64 auxv[][2] = {{AT_PAGESZ, PAGE_SIZE}};

/* 
 * Step1: Load data from the file stored in `path`.
 * The first `sizeof(struct Elf64_Ehdr)` bytes is the ELF header part.
 * You should check the ELF magic number and get the `e_phoff` and `e_phnum`
 * which is the starting byte of program header.
 * 
 * Step2: Load program headers
 * Program headers are stored like:
 * struct Elf64_Phdr phdr[e_phnum];
 * e_phoff is the address of phdr[0].
 * For each program header, if the type is LOAD, you should:
 * (1) allocate memory, va region [vaddr, vaddr+memsz)
 * (2) copy [offset, offset + filesz) of file to va [vaddr, vaddr+filesz) of memory
 * 
 * Step3: Allocate and initialize user stack.
 * 
 * The va of the user stack is not required to be any fixed value. It can be randomized.
 * 
 * Push argument strings.
 *
 * The initial stack is like
 *
 *   +-------------+
 *   | auxv[o] = 0 | 
 *   +-------------+
 *   |    ....     |
 *   +-------------+
 *   |   auxv[0]   |
 *   +-------------+
 *   | envp[m] = 0 |
 *   +-------------+
 *   |    ....     |
 *   +-------------+
 *   |   envp[0]   |
 *   +-------------+
 *   | argv[n] = 0 |  n == argc
 *   +-------------+
 *   |    ....     |
 *   +-------------+
 *   |   argv[0]   |
 *   +-------------+
 *   |    argc     |
 *   +-------------+  <== sp
 *
 * where argv[i], envp[j] are 8-byte pointers and auxv[k] are
 * called auxiliary vectors, which are used to transfer certain
 * kernel level information to the user processes.
 *
 * ## Example 
 *
 * ```
 * sp -= 8; *(size_t *)sp = AT_NULL;
 * sp -= 8; *(size_t *)sp = PGSIZE;
 * sp -= 8; *(size_t *)sp = AT_PAGESZ;
 *
 * sp -= 8; *(size_t *)sp = 0;
 *
 * // envp here. Ignore it if your don't want to implement envp.
 *
 * sp -= 8; *(size_t *)sp = 0;
 *
 * // argv here.
 *
 * sp -= 8; *(size_t *)sp = argc;
 *
 * // Stack pointer must be aligned to 16B!
 *
 * thisproc()->tf->sp = sp;
 * ```
 * 
 * There are two important entry point addresses:
 * (1) Address of the first user-level instruction: that's stored in elf_header.entry
 * (2) Adresss of the main function: that's stored in memory (loaded in part2)
 *
 */
static int loadseg(PTEntriesPtr pgdir,u64 va,Inode *ip,u32 offset,u32 sz){
    u64 pa;
    asserts(va%PAGE_SIZE==0,"loadseg error,va!");
    for(u32 i=0,n;i<sz;i+=PAGE_SIZE){
        pa = V2K(pgdir,va+i);
        asserts(pa!=0,"loadseg error!");
        n=MIN(PAGE_SIZE,sz-i);
        if(inodes.read(ip,(u8*)pa,offset+i,n)!=n)return -1;
    }
    return 0;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
	/* DONE: Lab9 Shell */
    Inode* ip=NULL;
    OpContext ctx;
    bcache.begin_op(&ctx);
    ip=namei(path,&ctx);
    if(ip==0){
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.lock(ip);
    // check ELF header
    Elf64_Ehdr elf;
    if(inodes.read(ip,(u8*)&elf,0,sizeof(elf))<sizeof(elf))goto bad;
    if(strncmp((const char*)elf.e_ident, ELFMAG, 4))goto bad;
    PTEntriesPtr pgdir=pgdir_init();
    if(pgdir==NULL)goto bad;
    // load memory
    Elf64_Phdr ph;
    u64 sz=0;
    for(int i=0,off=elf.e_phoff;i<elf.e_phnum;i++,off+=sizeof(ph)){
        if(inodes.read(ip,(u8*)&ph,off,sizeof(ph)!=sizeof(ph)))goto bad;
        if(ph.p_type!=PT_LOAD)continue;
        if(ph.p_memsz<ph.p_filesz)goto bad;
        if(ph.p_vaddr+ph.p_memsz<ph.p_vaddr)goto bad;
        if((sz=uvm_alloc(pgdir,0,0,sz,ph.p_vaddr+ph.p_memsz))==0)goto bad;
        if(ph.p_vaddr%PAGE_SIZE!=0)goto bad;
        if(loadseg(pgdir,ph.p_vaddr,ip,ph.p_offset,ph.p_filesz)<0)goto bad;
    }
    inodes.unlock(ip);
    bcache.end_op(&ctx);
    ip=0;
    sz=round_up(sz,PAGE_SIZE);
    sz=uvm_alloc(pgdir,0,0,sz,sz+2*PAGE_SIZE);
    if(sz==0)goto bad;
    uvm_clear(pgdir,(void*)(sz-2*PAGE_SIZE));
    u64 sp=sz,stackbase=sz-PAGE_SIZE;
    int argc=0;
    u64 ustack[32];
    for(;argv[argc];argc++){
        if(argc>=32)goto bad;
        sp-=strlen(argv[argc])+1;// '\0'
        sp=round_down(sp,16);
        if(sp<stackbase)goto bad;
        if(copyout(pgdir,sp,argv[argc],strlen(argv[argc])+1)<0)goto bad;
        ustack[argc]=sp;
    }
    ustack[argc]=0;
    struct proc *p=thiscpu()->proc;
    p->tf->x0=argc;

    sp=round_down(sp,16);
    // align
    if(argc%2==0)sp-=8;
    //---end ,not in stack,a memory place to store argv value,product point
    // auxv
    u64 auxv[] = { 0, AT_PAGESZ, PAGE_SIZE, AT_NULL };
    sp-=sizeof(auxv);
    if(copyout(pgdir,sp,auxv,sizeof(auxv))<0)goto bad;
    // env
    u64 noenv=0;
    sp-=8;
    if(copyout(pgdir,sp,&noenv,8)<0)goto bad;
    // argv
    sp-=(argc+1)*8;
    p->tf->x1=sp;
    if (copyout(pgdir,sp,ustack,(argc+1)*8)<0)goto bad;
    // argc
    sp-=8;
    if (copyout(pgdir,sp,&argc,8)<0)goto bad;
    asserts(sp%16==0,"exec stack error!");
    // ---end
    uint64_t* oldpgdir = p->pgdir;
    p->pgdir=pgdir;
    p->sz=sz;
    p->tf->SP_EL0 = sp;
    p->tf->ELR_EL1= elf.e_entry;
    uvm_switch(p->pgdir);
    vm_free(oldpgdir);
    return argc;

    bad:
    if(pgdir)vm_free(pgdir);
    if(ip)inodes.unlock(ip);
    return -1;
}
