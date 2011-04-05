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

#define CMDBUF_SIZE	80	// enough for one VGA text line

// Challege 2:
#define CHECKERR(a)	{if(a)	goto ERR;}
#define LERR		ERR:{cprintf("Wrong parameters!\n");return 0;}
// -----------

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a backtrack of the stack", mon_backtrace },
	{ "showmappings", "Display in a all of the physical page mappings that\n \
		apply to a particular range of virtual/linear addresses in the\n \
		currently active address space.", show_mapping },
	{ "dump", "Dump the contents of a range of memory given either a\n \
		 virtual or physical address range. ", dump },
	{ "priority", "Explicitly set, clear, or change the permissions of\n \
		any mapping in the current address space. \n \
		you can add (+) or remove (-) the flags p, u and w", set_pagepriority }
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
	unsigned int* ebp = (unsigned int*)read_ebp();
	unsigned int eip;
	struct Eipdebuginfo info;
	while (ebp != 0) {
		eip = *(ebp+1);

		cprintf("ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", 
			ebp, eip, *(ebp+2), *(ebp+3), *(ebp+4), *(ebp+5), *(ebp+6));
		debuginfo_eip(eip, &info);
		cprintf("\t%s:%d: %.*s+%d\n", 
			info.eip_file, info.eip_line, info.eip_fn_namelen, 
			info.eip_fn_name, eip-info.eip_fn_addr);				
		ebp = (unsigned int*)*ebp;
		
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

// Challege 2:
int
show_mapping(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t start ,end;
	char *nextcharb, *nextchare;

	// input: showmappings addr
	if (argc == 2)
	{	
		start = ROUNDDOWN((uint32_t)strtol(argv[1], &nextcharb, 0), 
			PGSIZE);
		end = start + PGSIZE;
		CHECKERR(*nextcharb != '\0');
	}

	// input: showmappings baddr eaddr 
	else if (argc == 3) 
	{
		start = ROUNDDOWN((uint32_t)strtol(argv[1], &nextcharb, 0), 
			PGSIZE);
		end = ROUNDUP((uint32_t)strtol(argv[2], &nextchare, 0), 
			PGSIZE);
		CHECKERR ( *nextcharb != '\0' || *nextchare != '\0');
	}
	else
	{	
		goto ERR;
	}
	cprintf("\tVirtual\tPhysical\tPriority\tRefer\n");
	for (; start!=end; start += PGSIZE)
	{
		struct Page *pp;
		pte_t *ppte;
		char buf[13];
		pp = page_lookup(PGADDR(PDX(VPT),PDX(VPT),0), (void *)start, 
			&ppte);
		if (pp == NULL || *ppte ==0)
			cprintf("\t%08x\t%s\t%s\t\t%d\n", start, "Not mapping", 
				"None", 0);
		else
			cprintf("\t%08x\t%08x\t%s\t%d\n", start, page2pa(pp),
				pagepri2str(*ppte, buf), pp->pp_ref );
	}
	
	return 0;	 
ERR:
	cprintf("Wrong parameters!\n");
	return 0; 
}

int
set_pagepriority(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t pte ;
	uint32_t start;
	pte_t * ppte;
	struct Page *pp;
	char *nextchar;
	char buf_old[13],buf_new[13];
	CHECKERR(argc < 3);
	start = ROUNDDOWN((uint32_t)strtol(argv[1], &nextchar, 0), PGSIZE);
	CHECKERR (*nextchar != '\0');
	pp = page_lookup(PGADDR(PDX(VPT),PDX(VPT),0), (void *)start, &ppte);
	
	if (pp == NULL || *ppte == 0)
	{	
		cprintf("\tVirtual\tPhysical\tPrev Priority\tNew Priority\tRefer\n");
		cprintf("\t%08x\t%s\t%s\t\t%s\t\t%d\n",start, "No mapping", 
			"None", "None", 0);
		return 0;
	}
	pte = *ppte;
	
	int i;

	for (i = 2; i < argc; i++)
	{
		if ((*argv[i] != '+') && (*argv[i] != '-'))
			goto ERR;
	}

	for (i = 2; i < argc; i++)
	{
		if (*argv[i] == '+')
		{
			*ppte |= str2pagepri(argv[i]+1);
		}
		else if (*argv[i] == '-')
		{
			*ppte &= ~str2pagepri(argv[i]+1);
		}
	}
	
	cprintf("\tVirtual\tPhysical\tPrev Priority\tNew Priority\tRefer\n");
	cprintf("\t%08x\t%08x\t%s\t%s\t%d\n", start,  page2pa(pp),  
		pagepri2str(pte,buf_old), pagepri2str(*ppte, buf_new), pp->pp_ref);
	return 0;
ERR:
	cprintf("Wrong parameters!\n");
	return 0;
}

int
dump(int argc, char **argv, struct Trapframe *tf)
{
	int flag = 0;
	uint32_t start,end;
	char *nextcharb, *nextchare;
	CHECKERR (argc !=2 && argc != 3 && argc !=4);

	// input: dump addr (Virtual Address)
	if (argc == 2) 
	{
		start = strtol(argv[1], &nextcharb, 0);
		end = start + 16;
		CHECKERR(*nextcharb != '\0');
	}
	else if (argc == 3)
	{
		if (*argv[1]== '-')
		{
			// input: dump -p addr (Physical Address)
			if (argv[1][1] == 'p') 
				flag = 1;

			//input: dump -v addr (Virtual Address)
			else CHECKERR (argv[1][1] != 'v');
			start = strtol(argv[2], &nextcharb, 0); 
			end = start +16;
			CHECKERR(*nextcharb!='\0');
		}
		else
		{	
			// input: dump xxxx xxxx
			start = strtol(argv[1], &nextcharb, 0);
			end = strtol(argv[2], &nextchare, 0);
			CHECKERR (*nextcharb != '\0' || *nextchare != '\0');
		}
	}
	else
	{
		// input: dump -p baddr eaddr (Physical Address)
		if (strcmp(argv[1],"-p") == 0) 
			flag = 1;
		
		//input: dump -v baddr eaddr (Virtual Address)
		else CHECKERR (strcmp(argv[1], "-v") !=0 ); 
		start = strtol(argv[2], &nextcharb, 0);
		end = strtol(argv[3], &nextchare, 0);
		CHECKERR (*nextcharb != '\0' || *nextchare != '\0');
	}

	// Process physical memory
	if (flag)
	{	
		static physaddr_t maxpa;
		i386_detect_memory();
		if (start > maxpa || end > maxpa)
		{
			cprintf("Address is larger than max physical memory\n");
			return 0;
		}
		start = (uint32_t)KADDR(start);
		end = (uint32_t)KADDR(end);
	}
	
	while (start < end)
	{
		int i;
		pte_t *ppte;
		cprintf("%08x ",start);
		if (page_lookup(PGADDR(PDX(VPT),PDX(VPT),0), (void*)start, &ppte) == NULL 
			|| *ppte == 0)
		{
			cprintf("No mapping\n");
			start += PGSIZE - start%PGSIZE;
			continue;
		}
		cprintf("%08x\t", PTE_ADDR(*ppte)|PGOFF(start));
		for (i=0; i < 16 ; i++, start ++)
		{
			cprintf("%02x ",*(unsigned char *)start);
		}
		cprintf("\n");
	}
	return 0;
ERR:
	cprintf("Wrong parameters!\n");
	return 0;
}


// -----------
