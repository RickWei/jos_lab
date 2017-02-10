/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)\
.data;\
	.long name;\
.text;\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
.data;\
	.long name;\
.text;\
    .globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps


.data
    .global trap_fun
    trap_fun:
/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
    TRAPHANDLER_NOEC(t_divide,T_DIVIDE)
    TRAPHANDLER_NOEC(t_debug,T_DEBUG)
    TRAPHANDLER_NOEC(t_nmi,T_NMI)
    TRAPHANDLER_NOEC(t_brkpt,T_BRKPT)
    TRAPHANDLER_NOEC(t_oflow,T_OFLOW)
    TRAPHANDLER_NOEC(t_bound,T_BOUND)
    TRAPHANDLER_NOEC(t_illop,T_ILLOP)
    TRAPHANDLER_NOEC(t_device,T_DEVICE)
    TRAPHANDLER(t_debflt,T_DBLFLT)
    .data
        .space 4
    TRAPHANDLER(t_tss,T_TSS)
    TRAPHANDLER(t_segnp,T_SEGNP)
    TRAPHANDLER(t_stack,T_STACK)
    TRAPHANDLER(t_gpflt,T_GPFLT)
    TRAPHANDLER(t_pgflt,T_PGFLT)
    .data
        .space 4
    TRAPHANDLER_NOEC(t_fperr,T_FPERR)
    TRAPHANDLER(t_align,T_ALIGN)
    TRAPHANDLER_NOEC(t_mchk,T_MCHK)
    TRAPHANDLER_NOEC(t_simderr,T_SIMDERR)
    .data
        .space 4*12
    TRAPHANDLER_NOEC(routine_irq0,IRQ_OFFSET+0)
    TRAPHANDLER_NOEC(routine_irq1,IRQ_OFFSET+1)
    TRAPHANDLER_NOEC(routine_irq2,IRQ_OFFSET+2)
    TRAPHANDLER_NOEC(routine_irq3,IRQ_OFFSET+3)
    TRAPHANDLER_NOEC(routine_irq4,IRQ_OFFSET+4)
    TRAPHANDLER_NOEC(routine_irq5,IRQ_OFFSET+5)
    TRAPHANDLER_NOEC(routine_irq6,IRQ_OFFSET+6)
    TRAPHANDLER_NOEC(routine_irq7,IRQ_OFFSET+7)
    TRAPHANDLER_NOEC(routine_irq8,IRQ_OFFSET+8)
    TRAPHANDLER_NOEC(routine_irq9,IRQ_OFFSET+9)
    TRAPHANDLER_NOEC(routine_irq10,IRQ_OFFSET+10)
    TRAPHANDLER_NOEC(routine_irq11,IRQ_OFFSET+11)
    TRAPHANDLER_NOEC(routine_irq12,IRQ_OFFSET+12)
    TRAPHANDLER_NOEC(routine_irq13,IRQ_OFFSET+13)
    TRAPHANDLER_NOEC(routine_irq14,IRQ_OFFSET+14)
    TRAPHANDLER_NOEC(routine_irq15,IRQ_OFFSET+15)

    TRAPHANDLER_NOEC(t_syscall,T_SYSCALL)

/*
 * Lab 3: Your code here for _alltraps
 */
.text

_alltraps:
/*
Your _alltraps should:

1.push values to make the stack look like a struct Trapframe
2.load GD_KD into %ds and %es
3.pushl %esp to pass a pointer to the Trapframe as an argument to trap()
4.call trap (can trap ever return?)
*/
    pushl %es
    pushl %ds
    pushal
    //fxsave (%esp) //challenge
    movl $GD_KD,%eax
    movl %eax,%es
    movl %eax,%ds
    pushl %esp
    call trap

//shouldn't return
    popl %esp
    popal
    popl %ds
    popl %es
    iret



