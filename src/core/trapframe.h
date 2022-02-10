#pragma once

#ifndef _CORE_TRAPFRAME_H_
#define _CORE_TRAPFRAME_H_

#include <common/defines.h>

/*
 * Trapframe should contain register x0~x30,
 * elr_el1, spsr_el1 and  sp_el0.
 * Pay attention to the order of these registers
 * in your trapframe.
 */
typedef struct {
	/* DONE: Lab3 Interrupt */
	// NOTE: TPIDR_EL0 is necessary for multi core !
	u64 TPIDR_EL0,ELR_EL1,SPSR_EL1,SP_EL0;
    u64 x30,x29,x28,x27,x26,x25,x24,x23,x22,x21,x20,x19,x18,x17,
		x16,x15,x14,x13,x12,x11,x10,x9,x8,x7,x6,x5,x4,x3,x2,x1,x0;
} Trapframe;

#endif
