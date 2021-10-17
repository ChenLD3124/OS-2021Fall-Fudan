#include <core/console.h>
#include <core/syscall.h>

/*
 * Based on the syscall number, call the corresponding syscall handler.
 * The syscall number and parameters are all stored in the trapframe.
 * See `syscallno.h` for syscall number macros.
 */
u64 syscall_dispatch(Trapframe *frame) {
    /* TODO: Lab3 Syscall */
	switch(frame->x8){
        case SYS_myexecve:{
            sys_myexecve(frame->x0);
        }break;
        case SYS_myexit:{
            sys_myexit();
        }break;
        default:_assert(1==2,"do not have this syscal!");
    }
    return 0;
}
