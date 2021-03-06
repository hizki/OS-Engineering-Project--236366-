/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_PMAP_H
#define JOS_KERN_PMAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/memlayout.h>
#include <inc/assert.h>
struct Env;


/* This macro takes a kernel virtual address -- an address that points above
 * KERNBASE, where the machine's maximum 256MB of physical memory is mapped --
 * and returns the corresponding physical address.  It panics if you pass it a
 * non-kernel virtual address.
 */
#define PADDR(kva)						\
({								\
	physaddr_t __m_kva = (physaddr_t) (kva);		\
	if (__m_kva < KERNBASE)					\
		panic("PADDR called with invalid kva %08lx", __m_kva);\
	__m_kva - KERNBASE;					\
})

/* This macro takes a physical address and returns the corresponding kernel
 * virtual address.  It panics if you pass an invalid physical address. */
#define KADDR(pa)						\
({								\
	physaddr_t __m_pa = (pa);				\
	uint32_t __m_ppn = PPN(__m_pa);				\
	if (__m_ppn >= npage)					\
		panic("KADDR called with invalid pa %08lx", __m_pa);\
	(void*) (__m_pa + KERNBASE);				\
})



extern char bootstacktop[], bootstack[];

extern struct Page *pages;
extern size_t npage;

extern physaddr_t boot_cr3;
extern pde_t *boot_pgdir;

extern struct Segdesc gdt[];
extern struct Pseudodesc gdt_pd;

void	i386_vm_init();
void	i386_detect_memory();

void	page_init(void);
int	page_alloc(struct Page **pp_store);
void	page_free(struct Page *pp);
int	page_insert(pde_t *pgdir, struct Page *pp, void *va, int perm);
void	page_remove(pde_t *pgdir, void *va);
struct Page *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store);
void	page_decref(struct Page *pp);

void	tlb_invalidate(pde_t *pgdir, void *va);

int	user_mem_check(struct Env *env, const void *va, size_t len, int perm);
void	user_mem_assert(struct Env *env, const void *va, size_t len, int perm);

// Returns physical page number.
// Note: pages = UPAGES.
static inline ppn_t
page2ppn(struct Page *pp)
{
	return pp - pages;
}

// Returns physical page address (page_number*(2^12)).
// Note: PGSHIFT = log2(PGSIZE) = 12.
static inline physaddr_t
page2pa(struct Page *pp)
{
	return page2ppn(pp) << PGSHIFT;
}

static inline struct Page*
pa2page(physaddr_t pa)
{
	if (PPN(pa) >= npage)
		panic("pa2page called with invalid pa");
	return &pages[PPN(pa)];
}

static inline void*
page2kva(struct Page *pp)
{
	return KADDR(page2pa(pp));
}

pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);

// Challege 2:
static inline char *
pagepri2str(pte_t pte, char *buf)
{
	int i;
	static const char *str[]={"_________SR_","AVLGPDACTUWP"};
	for(i=0; i<12; ++i)
	{
		buf[i] = str[(pte>>(11-i))&0x1][i];
	}
	buf[i] = '\0';
	return buf;
}

static inline int
str2pagepri(const char *buf)
{
	int pri = 0;
	while (*buf != '\0')
	{
		switch (*buf++)
		{
			case 'p':
			case 'P':	
				pri |= PTE_P;
				break;
			case 'w':
			case 'W':	
				pri |= PTE_W;
				break;

			case 'u':
			case 'U':	
				pri |= PTE_U;
				break;

			default:
				break;
		}
	}
	return pri;
}
// ----------


#endif /* !JOS_KERN_PMAP_H */
