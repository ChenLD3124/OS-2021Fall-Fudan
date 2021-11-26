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
    uint16_t an[4];
} IA;
typedef struct{
    uint8_t V,T;
    uint64_t pa;
    uint16_t Uattr,Lattr;
} PTE;

//
typedef struct {
    PTEntriesPtr (*pgdir_init)(void);
    PTEntriesPtr (*pgdir_walk)(PTEntriesPtr pgdir, void *kernel_address, int alloc);
    void (*vm_free)(PTEntriesPtr pgdir);
    int (*uvm_map)(PTEntriesPtr pgdir,
                   void *kernel_address,
                   size_t size,
                   uint64_t physical_address);
} VMemory;

PTEntriesPtr pgdir_init(void);
PTEntriesPtr pgdir_walk(PTEntriesPtr pgdir, void *kernel_address, int alloc);
void vm_free(PTEntriesPtr pgdir);
int uvm_map(PTEntriesPtr pgdir, void *kernel_address, size_t size, uint64_t physical_address);
void uvm_switch(PTEntriesPtr pgdir);
void virtual_memory_init(VMemory *);
void init_virtual_memory();
void vm_test();

#endif
