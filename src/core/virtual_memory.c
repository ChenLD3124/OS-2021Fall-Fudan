#include <aarch64/intrinsic.h>
#include <common/string.h>
#include <core/virtual_memory.h>
#include <core/physical_memory.h>
#include <common/types.h>
#include <core/console.h>

/* For simplicity, we only support 4k pages in user pgdir. */

extern PTEntries kpgdir;
VMemory vmem;
//my function
void _assert(int e,char* m){
    if(!e){printf("assert error : %s\n",m);while(1);}
}
static IA IAinit(uint64_t num){
    IA ia;
    num>>=12;
    for(int i=0;i<=3;i++){
        ia.an[i]=num&0x1ff;
        num>>=9;
    }
    return ia;
}
static PTE PTEinit(uint64_t num,uint8_t level){
    PTE entry;
    entry.V=num&1;entry.T=(num>>1)&1;
    entry.Lattr=0;entry.Uattr=0;entry.pa=0;
    if(entry.V){
        if(entry.T){
            entry.pa=(num>>12)&0xfffffffff;//36
        }
        else{
            entry.Uattr=(num>>51)&0x1fff;
            entry.Lattr=(num>>2)&0x3ff;
            _assert(level==3,"table has block");
            entry.pa=(num>>12)&0xfffffffff;//36
        }
    }
    return entry;
}
static uint64_t PTE2int64(PTE entry,uint8_t level){
    uint64_t tmp=0;
    if(!entry.V)return 0;
    if(entry.T){
        tmp|=((uint64_t)entry.pa<<12);
        tmp|=(uint64_t)entry.V;
        tmp|=((uint64_t)entry.T<<1);
    }
    else{
        _assert(level==3,"PTE2int64 err\n");
        tmp|=((uint64_t)entry.pa<<12);
        tmp|=(uint64_t)entry.V;
        tmp|=((uint64_t)entry.T<<1);
        tmp|=((uint64_t)entry.Lattr<<2);
        tmp|=((uint64_t)entry.Uattr<<51);
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

int uvm_map(PTEntriesPtr pgdir, void *va, size_t sz, uint64_t pa) {
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
    /* TODO: Lab2 memory*/
    return (PTEntriesPtr)kalloc();
}


/*
 * return the address of the pte in user page table
 * pgdir that corresponds to virtual address va.
 * if alloc != 0, create any required page table pages.
 */

static PTEntriesPtr my_pgdir_walk(PTEntriesPtr pgdir, void *vak, int alloc) {
    /* TODO: Lab2 memory*/
    IA addr=IAinit((uint64_t)vak);
    PTEntriesPtr nxt=pgdir,dist;
    for(int i=0;i<3;i++){
        dist=&(nxt[addr.an[i]]);
        PTE entry=PTEinit(*dist,i);
        if(!entry.V&&!alloc)return NULL;
        if(!entry.V&&alloc){
            entry.V=1;
            entry.pa=K2P((uint64_t)kalloc())>>12;
            entry.T=1;
            *dist=PTE2int64(entry,i);
        }
        nxt=(PTEntriesPtr)(P2K(entry.pa<<12));
    }
    dist=&(nxt[addr.an[3]]);
    return dist;
}


/* Free a user page table and all the physical memory pages. */
static void level_free(PTEntriesPtr pgdir,int level){
    if(level==3){
        kfree((void *)(pgdir));
        return;
    }
    for(int i=0;i<512;i++){
        PTE entry=PTEinit(pgdir[i],level);
        if(entry.V){
            _assert(entry.T==1,"table has block!");
            level_free((PTEntriesPtr)(P2K(entry.pa<<12)),level+1);
        }
    }
    kfree(pgdir);
}
void my_vm_free(PTEntriesPtr pgdir) {
    /* TODO: Lab2 memory*/
    level_free(pgdir,0);
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might not
 * be page-aligned.
 * Return -1 if failed else 0.
 */

int my_uvm_map(PTEntriesPtr pgdir, void *va, size_t sz, uint64_t pa) {
    /* TODO: Lab2 memory*/
    if(sz==0)return 0;
    va=ROUNDDOWN(va,PAGE_SIZE);
    pa=ROUNDDOWN(pa,PAGE_SIZE);
    void* last=ROUNDDOWN(va+sz-1,PAGE_SIZE)+PAGE_SIZE;
    for(;va!=last;va+=PAGE_SIZE,pa+=PAGE_SIZE){
        PTEntriesPtr tmp=my_pgdir_walk(pgdir,va,1);
        _assert(tmp!=0,"map:walk failed");
        PTE entry=PTEinit(*tmp,3);
        if(entry.V)return -1;
        entry.pa=pa>>12;
        entry.T=0;
        entry.V=1;
        *tmp=PTE2int64(entry,3);
        entry=PTEinit(*tmp,3);
        _assert(entry.pa==pa>>12,"myfunc error");
    }
    return 0;
}

void virtual_memory_init(VMemory *vmem_ptr) {
    vmem_ptr->pgdir_init = my_pgdir_init;
    vmem_ptr->pgdir_walk = my_pgdir_walk;
    vmem_ptr->vm_free = my_vm_free;
    vmem_ptr->uvm_map = my_uvm_map;
}

void init_virtual_memory() {
    virtual_memory_init(&vmem);
}

void vm_test() {
    /* TODO: Lab2 memory*/
    printf("test begin:\n");
    #define N 20000
    uint64_t p[N],v[N];
    //myfunc test begin
    for(uint64_t i=0;i<N;i++){
        uint64_t l1=(i<<12)|3;
        PTE an=PTEinit(l1,1);
        uint64_t bn=PTE2int64(an,1);
        _assert(bn==l1,"myfunc error");
    }
    //myfunc test end
    for(uint64_t i=0;i<N;i++){
        p[i]=(uint64_t)kalloc();
        if(i!=0)_assert(p[i-1]-p[i]==PAGE_SIZE,"err\n");
    }
    for(int i=N-1;i>=0;i--){
        kfree((void*)p[i]);
    }
    for(uint64_t i=0;i<N;i++){
        v[i]=(uint64_t)kalloc();
        _assert(p[i]==v[i],"pmemory error!");
    }
    for(uint64_t i=1;i<N;i++){
        p[i]=(uint64_t)kalloc();
    }
    PTEntriesPtr pgdir=pgdir_init();
    for(uint64_t i=1;i<N;i++){
        v[i]=i<<12;
        int a=uvm_map(pgdir,(void*)(v[i]),PAGE_SIZE,K2P(p[i]));
        _assert(a==0,"map failed");
    }
    for(uint64_t i=1;i<N;i++){
        PTEntriesPtr tmp=pgdir_walk(pgdir,(void*)v[i],0);
        _assert(tmp!=0,"can not walk!");
        PTE entry=PTEinit(*tmp,3);
        _assert(entry.V==1,"not alloc!");
        _assert((uint64_t)(P2K(entry.pa<<12))==p[i],"not match!");
    }
    for(uint64_t i=N;i<2*N;i++){
        PTEntriesPtr tmp=pgdir_walk(pgdir,(void*)((uint64_t)i<<12),0);
        if(tmp==0)continue;
        PTE entry=PTEinit(*tmp,3);
        _assert(entry.V==0,"valid error");
    }
    vm_free(pgdir);
    printf("\ntest pass!\n");
    #undef N
    // Certify that your code works!
}