// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4:
	if (!(err & FEC_WR))
		panic("pgfault: faulting access not a write");
	if (!(vpt[VPN(addr)] & PTE_COW))
		panic("pgfault: faulting access not to a CoW page");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4:
	// Allocate a new page, map it at a temporary location (PFTEMP)...
	int errno;
	errno = r=sys_page_alloc(0, PFTEMP, PTE_W|PTE_U|PTE_P);
	if (errno < 0)
		panic("pgfault: page alloc %e", errno);

	// copy the data from the old page to the new page...
	memmove(PFTEMP, (void *)ROUNDDOWN(addr, PGSIZE), PGSIZE);
	
	// then move the new page to the old page's address...
	errno = r=sys_page_map(0, PFTEMP, 0, (void *)ROUNDDOWN(addr, PGSIZE), 
		PTE_P|PTE_W|PTE_U);	
	if(errno < 0)
		panic("pgfault: page map %e", errno);

	// unmap the temporary page.
	errno = r=sys_page_unmap(0, PFTEMP);
	if (errno < 0)
		panic("pgfault: page unmap %e", errno);

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4:
	// Remember: The PTE for page number N is stored in vpt[N].
	pte_t pte = vpt[pn];
	void *addr = (void *) (pn << PGSHIFT);

	int errno;
	if ((pte | PTE_W) == pte || (pte | PTE_COW) == pte) {
		errno = sys_page_map(0, addr, envid, addr, PTE_U|PTE_P|PTE_COW);
		if (errno < 0)
			return errno;
		errno = sys_page_map(0, addr, 0, addr, PTE_U|PTE_P|PTE_COW);
		if (errno < 0)
			return errno;
	}
	
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "env" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
