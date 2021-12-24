#include <core/console.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/syscall.h>
#include <core/virtual_memory.h>
#include <sys/syscall.h>

int sys_gettid() {
    return thiscpu()->proc->pid;
}
int sys_ioctl() {
	/* TODO: Lab9 Shell */
    /* Assert tf->x1 == 0x5413 */
    return 0;
}
int sys_sigprocmask() {
    return 0;
}
int sys_default() {
    do {
        // sys_yield();
        printf("Unexpected syscall #%d\n", thiscpu()->proc->tf->x8);
        // while (1)
        //             ;
    } while (0);
    return 0;
}

#define NR_SYSCALL 512
const int (*syscall_table[NR_SYSCALL])() = {[0 ... NR_SYSCALL - 1] = sys_default,
                                            [SYS_set_tid_address] = sys_gettid,
                                            [SYS_ioctl] = sys_ioctl,
                                            [SYS_gettid] = sys_gettid,
                                            [SYS_rt_sigprocmask] = sys_sigprocmask,
                                            [SYS_brk] = (const int *)sys_brk,
                                            [SYS_execve] = sys_exec,
                                            [SYS_sched_yield] = sys_yield,
                                            [SYS_clone] = sys_clone,
                                            [SYS_wait4] = sys_wait4,
                                            [SYS_exit_group] = sys_exit,
                                            [SYS_exit] = sys_exit,
                                            [SYS_dup] = sys_dup,
                                            [SYS_chdir] = sys_chdir,
                                            [SYS_fstat] = sys_fstat,
                                            [SYS_newfstatat] = sys_fstatat,
                                            [SYS_mkdirat] = sys_mkdirat,
                                            [SYS_mknodat] = sys_mknodat,
                                            [SYS_openat] = sys_openat,
                                            [SYS_writev] = sys_writev,
                                            [SYS_read] = (const int *)sys_read,
                                            [SYS_write] = sys_write,
                                            [SYS_close] = sys_close,
                                            [SYS_myyield] = sys_yield};

const char(*syscall_table_str[NR_SYSCALL]) = {[0 ... NR_SYSCALL - 1] = "sys_default",
                                              [SYS_set_tid_address] = "sys_gettid",
                                              [SYS_ioctl] = "sys_ioctl",
                                              [SYS_gettid] = "sys_gettid",
                                              [SYS_rt_sigprocmask] = "sys_sigprocmask",
                                              [SYS_brk] = "sys_brk",
                                              [SYS_execve] = "sys_exec",
                                              [SYS_sched_yield] = "sys_yield",
                                              [SYS_clone] = "sys_clone",
                                              [SYS_wait4] = "sys_wait4",
                                              [SYS_exit_group] = "sys_exit",
                                              [SYS_exit] = "sys_exit",
                                              [SYS_dup] = "sys_dup",
                                              [SYS_chdir] = "sys_chdir",
                                              [SYS_fstat] = "sys_fstat",
                                              [SYS_newfstatat] = "sys_fstatat",
                                              [SYS_mkdirat] = "sys_mkdirat",
                                              [SYS_mknodat] = "sys_mknodat",
                                              [SYS_openat] = "sys_openat",
                                              [SYS_writev] = "sys_writev",
                                              [SYS_read] = "sys_read",
                                              [SYS_write] = "sys_write",
                                              [SYS_close] = "sys_close",
                                              [SYS_myyield] = "sys_yield"};

u64 syscall_dispatch(Trapframe *frame) {
    /* TODO: Lab9 Shell */
    int sysno;
    if (sysno < 400) printf("%d %s\n", sysno, syscall_table_str[sysno]);
	syscall_table[sysno]();

    return 0;
}

/* Check if a block of memory lies within the process user space. */
int in_user(void *s, usize n) {
    struct proc *p = thiscpu()->proc;
    if ((p->base <= (u64)s && (u64)s + n <= p->sz) ||
        (USERTOP - p->stksz <= (u64)s && (u64)s + n <= USERTOP))
        return 1;
    return 0;
}

/*
 * Fetch the nul-terminated string at addr from the current process.
 * Doesn't actually copy the string - just sets *pp to point at it.
 * Returns length of string, not including nul.
 */
int fetchstr(u64 addr, char **pp) {
    struct proc *p = thiscpu()->proc;
    char *s;
    *pp = s = (char *)addr;
    if (p->base <= addr && addr < p->sz) {
        for (; (u64)s < p->sz; s++)
            if (*s == 0)
                return s - *pp;
    } else if (USERTOP - p->stksz <= addr && addr < USERTOP) {
        for (; (u64)s < USERTOP; s++)
            if (*s == 0)
                return s - *pp;
    }
    return -1;
}

/*
 * Fetch the nth (starting from 0) 32-bit system call argument.
 * In our ABI, x8 contains system call index, x0-x5 contain parameters.
 * now we support system calls with at most 6 parameters.
 */
int argint(int n, int *ip) {
    struct proc *proc = thiscpu()->proc;
    switch(n){
        case 0:*ip = proc->tf->x0;break;
        case 1:*ip = proc->tf->x1;break;
        case 2:*ip = proc->tf->x2;break;
        case 3:*ip = proc->tf->x3;break;
        case 4:*ip = proc->tf->x4;break;
        case 5:*ip = proc->tf->x5;break;
        default:return -1;
    }
    return 0;
}

/*
 * Fetch the nth (starting from 0) 64-bit system call argument.
 * In our ABI, x8 contains system call index, x0-x5 contain parameters.
 * now we support system calls with at most 6 parameters.
 */
int argu64(int n, u64 *ip) {
    struct proc *proc = thiscpu()->proc;
    switch(n){
        case 0:*ip = proc->tf->x0;break;
        case 1:*ip = proc->tf->x1;break;
        case 2:*ip = proc->tf->x2;break;
        case 3:*ip = proc->tf->x3;break;
        case 4:*ip = proc->tf->x4;break;
        case 5:*ip = proc->tf->x5;break;
        default:return -1;
    }
    return 0;
}

/*
 * Fetch the nth word-sized system call argument as a pointer
 * to a block of memory of size bytes. Check that the pointer+
 * lies within the process address space.
 */
int argptr(int n, char **pp, usize size) {
    u64 i = 0;
    if (argu64(n, &i) < 0) {
        return -1;
    }
    if (in_user((void *)i, size)) {
        *pp = (char *)i;
        return 0;
    } else {
        return -1;
    }
}

/*
 * Fetch the nth word-sized system call argument as a string pointer.
 * Check that the pointer is valid and the string is nul-terminated.
 * (There is no shared writable memory, so the string can't change
 * between this check and being used by the kernel.)
 */
int argstr(int n, char **pp) {
    u64 addr = 0;
    if (argu64(n, &addr) < 0)
        return -1;
    int r = fetchstr(addr, pp);
    return r;
}