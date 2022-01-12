#include <aarch64/intrinsic.h>
#include <common/defines.h>
#include <common/string.h>
#include <core/console.h>
#include <common/myfunc.h>
#include <core/physical_memory.h>
#include <core/virtual_memory.h>

/* For simplicity, we only support 4k pages in user pgdir. */

extern PTEntries kpgdir;
VMemory vmem;
//my function

static IA IAinit(u64 num){
    IA ia;
    num>>=12;
    for(int i=3;i>=0;i--){
        ia.an[i]=num&0x1ff;
        num>>=9;
    }
    return ia;
}
static PTE PTEinit(u64 num,u8 level){
    PTE entry;
    entry.V=num&1;entry.T=(num>>1)&1;
    entry.Lattr=0;entry.Uattr=0;entry.pa=0;
    if(entry.V){
        if(entry.T){
            entry.Uattr=(num>>51)&0x1fff;
            entry.Lattr=(num>>2)&0x3ff;
            entry.pa=(num>>12)&0xfffffffff;//36
        }
    }
    return entry;
}
static u64 PTE2int64(PTE entry,u8 level){
    u64 tmp=0;
    if(!entry.V)return 0;
    if(entry.T){
        tmp|=((u64)entry.pa<<12);
        tmp|=(u64)entry.V;
        tmp|=((u64)entry.T<<1);
        tmp|=((u64)entry.Lattr<<2);
        tmp|=((u64)entry.Uattr<<51);
    }
    return tmp;
}

//
PTEntriesPtr pgdir_init() {
    return vmem.pgdir_init();
}

PTEntriesPtr pgdir_walk(PTEntriesPtr pgdir, void *vak, int alloc) {

    return vmem.pgdir_walk(pgdir, vak, alloc);
}

void vm_free(PTEntriesPtr pgdir) {
    vmem.vm_free(pgdir);
}

int uvm_map(PTEntriesPtr pgdir, void *va, usize sz, u64 pa) {
    return vmem.uvm_map(pgdir, va, sz, pa);
}

void uvm_switch(PTEntriesPtr pgdir) {
    // FIXME: Use NG and ASID for efficiency.
    arch_set_ttbr0(K2P(pgdir));
}

/*
 * generate a empty page as page directory
 */

static PTEntriesPtr my_pgdir_init() {
    /* DONE: Lab2 memory*/
    return (PTEntriesPtr)kalloc();
}

/*
 * return the address of the pte in user page table
 * pgdir that corresponds to virtual address va.
 * if alloc != 0, create any required page table pages.
 */

static PTEntriesPtr my_pgdir_walk(PTEntriesPtr pgdir, void *vak, int alloc) {
    /* DONE: Lab2 memory*/
    IA addr=IAinit((u64)vak);
    PTEntriesPtr nxt=pgdir,dist;
    for(int i=0;i<3;i++){
        dist=&(nxt[addr.an[i]]);
        PTE entry=PTEinit(*dist,i);
        if(!entry.V&&!alloc)return NULL;
        if(!entry.V&&alloc){
            PTEntriesPtr tmp=(u64)kalloc();
            if(tmp==0)return NULL;
            entry.pa=K2P(tmp)>>12;
            entry.V=1;
            entry.T=1;
            *dist=PTE2int64(entry,i);
        }
        nxt=(PTEntriesPtr)(P2K(entry.pa<<12));
    }
    dist=&(nxt[addr.an[3]]);
    return dist;
}

/* 
 * Fork a process's page table.
 * Copy all user-level memory resource owned by pgdir.
 * Only used in `fork()`.
 */
static PTEntriesPtr my_uvm_copy(PTEntriesPtr pgdir) {
	/* TODO: Lab9 Shell */

    return 0;
}

/* Free a user page table and all the physical memory pages. */
static void level_free(PTEntriesPtr pgdir,int level){
    if(level==4){
        kfree((void *)(pgdir));
        return;
    }
    for(int i=0;i<N_PTE_PER_TABLE;i++){
        PTE entry=PTEinit(pgdir[i],level);
        if(entry.V){
            if(level<3) _assert(entry.T==1,"table has block!");
            level_free((PTEntriesPtr)(P2K(entry.pa<<12)),level+1);
        }
    }
    kfree(pgdir);
}
void my_vm_free(PTEntriesPtr pgdir) {
    /* DONE: Lab2 memory*/
    level_free(pgdir,0);
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might not
 * be page-aligned.
 * Return -1 if failed else 0.
 */

int my_uvm_map(PTEntriesPtr pgdir, void *va, usize sz, u64 pa) {
    /* DONE: Lab2 memory*/
    if(sz==0)return 0;
    va=round_down(va,PAGE_SIZE);
    pa=round_down(pa,PAGE_SIZE);
    void* last=round_down(va+sz-1,PAGE_SIZE)+PAGE_SIZE;
    for(;va!=last;va+=PAGE_SIZE,pa+=PAGE_SIZE){
        PTEntriesPtr tmp=my_pgdir_walk(pgdir,va,1);
        // _assert(tmp!=0,"map:walk failed");
        if(tmp==0)return -1;
        PTE entry=PTEinit(*tmp,3);//0xffff000000082980
        _assert(entry.V==0,"reused");
        entry.pa=pa>>12;
        entry.T=1;
        entry.V=1;
        entry.Lattr=1|(3<<7)|(1<<4);
        *tmp=PTE2int64(entry,3);
        entry=PTEinit(*tmp,3);
        _assert(entry.pa==pa>>12,"myfunc error");
    }
    return 0;
}

/*
 * Allocate page tables and physical memory to grow process
 * from oldsz to newsz, which need not be page aligned.
 * Stack size stksz should be page aligned.
 * Returns new size or 0 on error.
 */

int my_uvm_alloc(PTEntriesPtr pgdir, usize base, usize stksz, usize oldsz, usize newsz) {
    /* TODO: Lab9 Shell */

    return 0;
}

/*
 * Deallocate user pages to bring the process size from oldsz to
 * newsz.  oldsz and newsz need not be page-aligned, nor does newsz
 * need to be less than oldsz.  oldsz can be larger than the actual
 * process size.  Returns the new process size.
 */

int my_uvm_dealloc(PTEntriesPtr pgdir, usize base, usize oldsz, usize newsz) {
    /* TODO: Lab9 Shell */

    return 0;
}

/*
 * Copy len bytes from p to user address va in page table pgdir.
 * Allocate physical pages if required.
 * Useful when pgdir is not the current page table.
 * 
 * p is kernel virtual address.
 * va is target user virtual address.
 * 
 * this function works as:
 * memcpy(dest=P2V(walk(pgdir, va, alloc=True)), src=p, size=len)
 */

int my_copyout(PTEntriesPtr pgdir, void *va, void *p, usize len) {
    /* TODO: Lab9 Shell */

    return 0;
}

void virtual_memory_init(VMemory *vmt_ptr) {
    vmt_ptr->pgdir_init = my_pgdir_init;
    vmt_ptr->pgdir_walk = my_pgdir_walk;
    vmt_ptr->uvm_copy = my_uvm_copy;
    vmt_ptr->vm_free = my_vm_free;
    vmt_ptr->uvm_map = my_uvm_map;
    vmt_ptr->uvm_alloc = my_uvm_alloc;
    vmt_ptr->uvm_dealloc = my_uvm_dealloc;
    vmt_ptr->copyout = my_copyout;
}

void init_virtual_memory() {
    virtual_memory_init(&vmem);
}

void vm_test() {
    /* DONE: Lab2 memory*/
    // *((int64_t *)P2K(0)) = 0xac;
    // char *p = kalloc();
    // memset(p, 0, PAGE_SIZE);
    // uvm_map((u64 *)p, (void *)0x1000, PAGE_SIZE, 0);
	// uvm_switch(p);
	// PTEntry *pte = pgdir_walk(p, (void *)0x1000, 0);
	// if (pte == 0) {
	// 	puts("walk should not return 0"); while (1);
	// }
	// if (((u64)pte >> 48) == 0) {
	// 	puts("pte should be virtual address"); while (1);
	// }
	// if ((*pte) >> 48 != 0) {
	// 	puts("*pte should store physical address"); while (1);
	// }
	// if (((*pte) & PTE_USER_DATA) != PTE_USER_DATA) {
	// 	puts("*pte should contain USE_DATA flags"); while (1);
	// }
    // if (*((int64_t *)0x1000) == 0xac) {
    //     puts("Test_Map_Region Pass!");
    // } else {
	// 	puts("Test_Map_Region Fail!"); while (1);
    // }
    #ifdef DEBUG
    printf("test begin\n");
    #define N 20000 //kernal stack has just 4096 byte
    static u64 c_p[N],c_v[N],bcnt;
    
    bcnt=cnt;
    printf("%llu\n",cnt);
    //myfunc test begin
    for(u64 i=0;i<N;i++){
        u64 l1=(i<<12)|3;
        PTE an=PTEinit(l1,1);
        u64 bn=PTE2int64(an,1);
        _assert(bn==l1,"myfunc error");
    }
    //myfunc test end
    for(u64 i=0;i<N;i++){
        u64 qqq=(u64)kalloc();
        c_p[i]=qqq;
        if(i>1&&c_p[i-1]-c_p[i]!=PAGE_SIZE)printf("!!%llx %llx %llx %llx %llx\n",c_p[i-2],c_p[i-1],c_p[i],&c_p[i],qqq);
        if(i!=0)_assert(c_p[i-1]-c_p[i]==PAGE_SIZE,"err\n");
    }
    printf("!!!\n");
    _assert(bcnt-cnt==N,"alloc error");
    for(int i=N-1;i>=0;i--){
        kfree((void*)c_p[i]);
        // printf("!%llu\n",c_p[i]);
    }
    printf("@@@\n");
    _assert(bcnt==cnt,"free error");
    for(u64 i=0;i<N;i++){
        c_v[i]=(u64)kalloc();
        _assert(c_p[i]==c_v[i],"pmemory error!");
    }
    printf("%llu\n",cnt);
    _assert(bcnt-cnt==N,"alloc error");
    printf("text1 pass!\n");
    PTEntriesPtr pgdir=pgdir_init();
    for(u64 i=0;i<N;i++){
        c_v[i]=i<<12;
        int a=uvm_map(pgdir,(void*)(c_v[i]),PAGE_SIZE,K2P(c_p[i]));
        _assert(a==0,"map failed");
    }
    printf("text2 pass!\n");
    for(u64 i=0;i<N;i++){
        PTEntriesPtr tmp=pgdir_walk(pgdir,(void*)c_v[i],0);
        _assert(tmp!=0,"can not walk!");
        PTE entry=PTEinit(*tmp,3);
        _assert(entry.V==1,"not alloc!");
        _assert((u64)(P2K(entry.pa<<12))==c_p[i],"not match!");
    }
    printf("text3 pass!\n");
    
    for(u64 i=N;i<2*N;i++){
        PTEntriesPtr tmp=pgdir_walk(pgdir,(void*)((u64)i<<12),0);
        if(tmp==0)continue;
        PTE entry=PTEinit(*tmp,3);
        _assert(entry.V==0,"valid error");
    }
    vm_free(pgdir);
    // printf("%llu %llu\n",cnt,bcnt);
    _assert(bcnt==cnt,"free error");
    printf("\ntest pass!\n");
    #undef N
    #endif
    // Certify that your code works!
}
