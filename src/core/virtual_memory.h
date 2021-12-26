#include <aarch64/mmu.h>
#include <common/types.h>
#pragma once
#include <common/types.h>
#ifndef _CORE_VIRTUAL_MEMORY_
#define _CORE_VIRTUAL_MEMORY_

#define USERTOP  0x0001000000000000
#define KERNBASE 0xFFFF000000000000

/*
 * uvm stands user vitual memory.
 */
//my struct
typedef struct{
    u16 an[4];
} IA;
typedef struct{
    u8 V,T;
    u64 pa;
    u16 Uattr,Lattr;
} PTE;

//
typedef struct {
    PTEntriesPtr (*pgdir_init)(void);
    PTEntriesPtr (*pgdir_walk)(PTEntriesPtr pgdir, void *kernel_address, int alloc);
    PTEntriesPtr (*uvm_copy)(PTEntriesPtr pgdir);
    void (*vm_free)(PTEntriesPtr pgdir);
    int (*uvm_map)(PTEntriesPtr pgdir,
                   void *kernel_address,
                   usize size,
                   u64 physical_address);
    int (*uvm_alloc)(PTEntriesPtr pgdir, usize base, usize stksz, usize oldsz, usize newsz);
    int (*uvm_dealloc)(PTEntriesPtr pgdir, usize base, usize oldsz, usize newsz);
    int (*copyout)(PTEntriesPtr pgdir, void *tgt_address, void *src_address, usize len);
} VMemory;

PTEntriesPtr pgdir_init(void);
PTEntriesPtr pgdir_walk(PTEntriesPtr pgdir, void *kernel_address, int alloc);
PTEntriesPtr uvm_copy(PTEntriesPtr pgdir);
void vm_free(PTEntriesPtr pgdir);
int uvm_map(PTEntriesPtr pgdir, void *kernel_address, usize size, u64 physical_address);
int uvm_alloc(PTEntriesPtr pgdir, usize base, usize stksz, usize oldsz, usize newsz);
int uvm_dealloc(PTEntriesPtr pgdir, usize base, usize oldsz, usize newsz);
void uvm_switch(PTEntriesPtr pgdir);
int copyout(PTEntriesPtr pgdir, void *tgt_address, void *src_address, usize len);
void virtual_memory_init(VMemory *);
void init_virtual_memory();
void vm_test();

#endif
