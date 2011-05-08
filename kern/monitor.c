// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#include <kern/pmap.h>

#include <kern/env.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a list of function call frames", mon_backtrace },
	{ "showmappings", "Display the physical page mappings and permission bits", mon_showmappings },
	{ "setmappingperm", "Explicitly set, clear, or change the permissions of any mapping", mon_setmappingperm }, 
	{ "dumpva", "Dump memory residing in between two VAs", mon_dumpva },
	{ "dumpph", "Dump memory residing in between two PAs", mon_dumpph },
	{ "next", "(debug) next instruction", mon_next },
	{ "cont", "(debug) continue exection", mon_stop_dbg }
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start %08x (virt)  %08x (phys)\n", _start, _start - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-_start+1023)/1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t ebp = read_ebp();
	uintptr_t* ptr = (uintptr_t*)ebp;
	struct Eipdebuginfo dbginfo;
	
	// ebp's 1st value is 0
	while (ebp != 0) {
		cprintf("ebp %08x eip %08x args %08x %08x %08x %08x %08x\n",
			ebp, *(ptr + 1),
			*(ptr + 2), *(ptr + 3), *(ptr + 4),
			*(ptr + 5), *(ptr + 6));
		debuginfo_eip( *(ptr + 1), &dbginfo );
		cprintf("\t%s:%d: %.*s+%d\n",
			dbginfo.eip_file,
			dbginfo.eip_line,
			dbginfo.eip_fn_namelen, dbginfo.eip_fn_name,
			*(ptr + 1) - dbginfo.eip_fn_addr);
		
		ebp = *ptr;
		ptr = (uintptr_t*)ebp;
	}
	
	return 0;

}

static uint32_t
str2addr(char *str)
{
	uint32_t addr = 0;
	int i, digit;
	// we only accept hex from form: 0x...
	if ((*str != '\0' && *(str+1) != '\0') && (*str != '0' || *(str + 1) != 'x'))
		return -1;
	
	for (i = 2; *(str+i) != '\0'; ++i) {
		if (*(str+i) >= 97 && *(str+i) <= 102)
			digit = *(str+i) - 97 + 10;
		else if (*(str+i) >= 48 && *(str+i) <= 57)
			digit = *(str+i) - 48;
		else
			return -1;
		addr = addr * 16 + digit;
	}
	
	return addr;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t i;
	uintptr_t low, high;
	uint32_t *pgdir;
	pde_t pde;
	pte_t pte;
	
	if (argc != 3)
		return -1;
	low = str2addr(argv[1]);
	high = str2addr(argv[2]);
	if ((!(low <= high)) || low < PGSIZE || high < PGSIZE)
		return -1;
	pgdir = (uint32_t*) KADDR(rcr3());

	cprintf("\t\tMAPPED TO\t\tPERMISSION BIT\n");
	cprintf("----------------------------------------------------------------------\n");
	
	for (i = low; i <= high; i += PGSIZE) {
		cprintf("0x%x:", i & 0xfffff000);
		pde = pgdir[PDX(i)];
		if (pde & PTE_P) {
			pte = ((pte_t*) KADDR(PTE_ADDR(pde)))[PTX(i)];

			cprintf("\t0x%x", PTE_ADDR(pte));
			cprintf("\t\tPTE_P = %d\n", (pte & PTE_P) ? 1 : 0);
			cprintf("\t\t\t\t\tPTE_W = %d\n", (pte & PTE_W) ? 1 : 0);
			cprintf("\t\t\t\t\tPTE_U = %d\n", (pte & PTE_U) ? 1 : 0);
			cprintf("\t\t\t\t\tPTE_PWT = %d\n", (pte & PTE_PWT) ? 1 : 0);
			cprintf("\t\t\t\t\tPTE_PCD = %d\n", (pte & PTE_PCD) ? 1 : 0);
			cprintf("\t\t\t\t\tPTE_A = %d\n", (pte & PTE_A) ? 1 : 0);
			cprintf("\t\t\t\t\tPTE_D = %d\n", (pte & PTE_D) ? 1 : 0);
			cprintf("\t\t\t\t\tPTE_PS = %d\n", (pte & PTE_PS) ? 1 : 0);
			cprintf("\t\t\t\t\tPTE_MBZ = %d (Must Be Zero)\n", (pte & PTE_MBZ));
		}
		else
			cprintf("\tnot present\n");
		
		cprintf("......................................................\n");
	}
	
	return 0;
}

int
mon_setmappingperm(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t *pgdir;
	uint32_t perm;
	uint32_t addr;
	pde_t pde;
	pte_t *ptep;
	
	if (argc != 3)
		return -1;
	pgdir = (uint32_t*) KADDR(rcr3());
	perm = str2addr(argv[2]) & 0x00000fff;
	addr = str2addr(argv[1]);
	pde = pgdir[PDX(addr)];

	cprintf("addr = %x\n", addr);
	cprintf("perm = %x\n", perm);
	
	
	if (pde & PTE_P) {
		ptep = &((pte_t*) KADDR(PTE_ADDR(pde)))[PTX(addr)];
		cprintf ("pte = %x\n", *ptep);
		*ptep = PTE_ADDR(*ptep) | perm;
		cprintf ("pte = %x\n", *ptep);
	}
	else
		cprintf("There is no mapping for 0x%x\n", addr);

	return 0;
}

int
mon_dumpva(int argc, char **argv, struct Trapframe *tf)
{
	/* uint32_t *pgdir; */
	/* uint32_t perm; */
	/* uint32_t addr; */
	/* pde_t pde; */
	/* pte_t *ptep; */

	/* if (argcPPPP */
	
	/* pgdir = (uint32_t*) KADDR(rcr3()); */
	cprintf("TODO\n");
	
	return 0;
}

int
mon_dumpph(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("TODO\n");
	return 0;
}

int
mon_next(int argc, char **argv, struct Trapframe *tf)
{
	tf->tf_eflags |= FL_TF;
	return -1; // exit monitor
}

int
mon_stop_dbg(int argc, char **argv, struct Trapframe *tf)
{
	tf->tf_eflags &= ~FL_TF;
	return -1; // exit monitor
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

void
monitor_debug(struct Trapframe *tf)
{
	char *buf;
	cprintf("current eip: %x\n", tf->tf_eip);
	
	while (1) {
		buf = readline("DEBUG (next/cont) >> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
