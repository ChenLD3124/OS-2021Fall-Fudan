#include <aarch64/mmu.h>
#include <core/physical_memory.h>
#include <common/types.h>
#include <common/string.h>
#include <core/console.h>
#include <common/myfunc.h>

extern char end[];
PMemory pmem;

FreeListNode head;
/*
 * Editable, as long as it works as a memory manager.
 */
static void freelist_init(void *datastructure_ptr, void *start, void *end);
static void *freelist_alloc(void *datastructure_ptr);
static void freelist_free(void *datastructure_ptr, void *page_address);

/*
 * Allocate one 4096-byte page of physical memory.
 * Returns a pointer that the kernel can use.
 * Returns 0 if the memory cannot be allocated.
 */
static void *freelist_alloc(void *datastructure_ptr) {
    FreeListNode *f = (FreeListNode *) datastructure_ptr; 
    /* DONE: Lab2 memory*/
    void *tmp=f->next;
    if(tmp){
        f->next=((FreeListNode*)tmp)->next;
        memset(tmp,0,PAGE_SIZE);//junk data
        #ifdef DEBUG
        cnt--;
        #endif
    }
    return tmp;
}

/*
 * Free the page of physical memory pointed at by page_address.
 */
static void freelist_free(void *datastructure_ptr, void *page_address) {
    FreeListNode* f = (FreeListNode*) datastructure_ptr; 
    /* DONE: Lab2 memory*/
    void *tmp=f->next;
    f->next=page_address;
    _assert(page_address!=0,"page_address errro");
    #ifdef DEBUG
    cnt++;
    #endif
    // memset(page_address,1,PAGE_SIZE);//junk data
    ((FreeListNode*)page_address)->next=tmp;
}

/*
 * Record all memory from start to end to freelist as initialization.
 */

static void freelist_init(void *datastructure_ptr, void *start, void *end) {
    FreeListNode* f = (FreeListNode*) datastructure_ptr; 
    /* DONE: Lab2 memory*/
    f->next=0;
    #ifdef DEBUG
    cnt=0;
    #endif
    for(void* ite=start;ite+PAGE_SIZE<=end;ite+=PAGE_SIZE)freelist_free(datastructure_ptr,ite);
}


static void init_PMemory(PMemory *pmem_ptr) {
    pmem_ptr->struct_ptr = (void *)&head;
    pmem_ptr->page_init = freelist_init;
    pmem_ptr->page_alloc = freelist_alloc;
    pmem_ptr->page_free = freelist_free;
}

void init_memory_manager(void) {
    // HACK Raspberry pi 4b.
    // size_t phystop = MIN(0x3F000000, mbox_get_arm_memory());
    size_t phystop = 0x3F000000;
    
    // notice here for roundup
    void *ROUNDUP_end = ROUNDUP((void *)end, PAGE_SIZE);
    init_PMemory(&pmem);
    // printf("%llu %llu\n",ROUNDUP_end, (void *)P2K(phystop));
    pmem.page_init(pmem.struct_ptr, ROUNDUP_end, (void *)P2K(phystop));
}

/*
 * Record all memory from start to end to memory manager.
 */
void free_range(void *start, void *end) {
    for (void *p = start; p + PAGE_SIZE <= end; p += PAGE_SIZE)
        pmem.page_free(pmem.struct_ptr, p);
}

/*
 * Allocate a page of physical memory.
 * Returns 0 if failed else a pointer.
 * Corrupt the page by filling non-zero value in it for debugging.
 */
void *kalloc(void) {
    void *p = pmem.page_alloc(pmem.struct_ptr);
    return p;
}

/* Free the physical memory pointed at by page_address. */
void kfree(void *page_address) {
    pmem.page_free(pmem.struct_ptr, page_address);
}