// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/trap.h>
#include <kern/kdebug.h>

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
	{	"backtrace", "Display function backtrace information", mon_backtrace }
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("\27[33m%s\27[m - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], etext[], edata[], end[];

	cprintf("\27[33mSpecial kernel symbols:\27[m\n");
	cprintf("  \27[32m_start \27[31m%08x (virt)  %08x (phys)\27[m\n", _start, _start - KERNBASE);
	cprintf("  \27[32metext  \27[31m%08x (virt)  %08x (phys)\27[m\n", etext, etext - KERNBASE);
	cprintf("  \27[32medata  \27[31m%08x (virt)  %08x (phys)\27[m\n", edata, edata - KERNBASE);
	cprintf("  \27[32mend    \27[31m%08x (virt)  %08x (phys)\27[m\n", end, end - KERNBASE);
	cprintf("\27[33mKernel executable memory footprint: \27[31m%dKB\27[m\n",
		(end-_start+1023)/1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	uint32_t ebp;
	uint32_t eip;

	uint32_t * pebp;

	struct Eipdebuginfo debug_info;
	char function_name[100];

	ebp = read_ebp();
	eip = read_eip();

	cprintf("\27[33mStack backtrace:\27[m\n");
	while(ebp != 0x00000000) {
		pebp = (uint32_t *) ebp;
		debuginfo_eip(eip, &debug_info);

		strcpy(function_name, debug_info.eip_fn_name);
		function_name[debug_info.eip_fn_namelen] = '\0';
		
		cprintf("\27[39m%s\27[m:\27[34m%d\27[m: \27[39m%s\27[m+\27[34m%x\27[m\n", debug_info.eip_file, debug_info.eip_line, function_name, eip-debug_info.eip_fn_addr);
		cprintf("  \27[32mebp \27[31;47m%08x\27[m  \27[32meip \27[31;47m%08x\27[;m  \27[32margs \27[31;47m%08x %08x %08x %08x %08x\n", 
			ebp, eip, *(pebp+2), *(pebp+3), *(pebp+4), *(pebp+5), *(pebp+6));
		ebp = *pebp;
		eip = *(pebp+1);
	}
	return 0;
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
			cprintf("\27[34mToo many arguments (max %d)\27[m\n", MAXARGS);
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
	cprintf("\27[34mUnknown command '%s'\27[m\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the \27[32mJOS\27[m kernel monitor!\n");
	cprintf("Type \27[33;45m'help'\27[m for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
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
