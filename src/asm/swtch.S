/*
 * Context switch
 *
 *   void swtch(struct context **old, struct context *new);
 * 
 * Save the current registers on the stack, creating
 * a struct context, and save its address in *old.
 * Switch stacks to new and pop previously-saved registers.
 *
 * Function `swtch` is call-return semantic.
 * However, the pc+4 of the caller is saved in the context.
 * Therefore, we can store pc+4 in a caller-saved register (like x15),
 * and use `br x15` instead of `ret` in the return stage.
 */
.global swtch
swtch:
    /* DONE: Lab3 Schedule */
    stp x20, x19, [sp,#-16]!
    stp x22, x21, [sp,#-16]!
    stp x24, x23, [sp,#-16]!
    stp x26, x25, [sp,#-16]!
    stp x28, x27, [sp,#-16]!
    stp x30, x29, [sp,#-16]!
    mov x15, sp
    str x15, [x0]
    mov sp, x1

    ldp x30, x29, [sp], #16
    ldp x28, x27, [sp], #16
    ldp x26, x25, [sp], #16
    ldp x24, x23, [sp], #16
    ldp x22, x21, [sp], #16
    ldp x20, x19, [sp], #16
    
    /*forkret为返回值时特殊处理 */
    ldr x15, =forkret
    ldr x14, =trap_return
    cmp x30, x15
    csel x15, x15, x30, EQ
    csel x30, x14, x30, EQ
    br  x15
