/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];

void idt_init(void);
void print_regs(struct PushRegs *regs);
void print_trapframe(struct Trapframe *tf);
void page_fault_handler(struct Trapframe *);
void backtrace(struct Trapframe *);

void handle_divide(void);
void handle_debug(void);
void handle_nmi(void);
void handle_brkpt(void);
void handle_oflow(void);
void handle_bound(void);
void handle_illop(void);
void handle_device(void);
void handle_dblflt(void);
void handle_tss(void);
void handle_segnp(void);
void handle_stack(void);
void handle_gpflt(void);
void handle_pgflt(void);
void handle_fperr(void);
void handle_align(void);
void handle_mchk(void);
void handle_simderr(void);
void handle_syscall(void);

#endif /* JOS_KERN_TRAP_H */
