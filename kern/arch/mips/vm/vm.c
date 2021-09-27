/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

#include <pagetable.h>
#include <coremap.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */

/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */
#define DUMBVM_STACKPAGES    18


/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static struct spinlock freemem_lock = SPINLOCK_INITIALIZER;

//static unsigned char *freeRamFrames = NULL;
//static unsigned long *allocSize = NULL;
//static coremap_entry_t *freeRamFrames = NULL;
//static int nRamFrames = 0;
//static int allocTableActive = 0;
extern int allocTableActive;
extern coremap_entry_t *freeRamFrames;
extern int nRamFrames;

static int isTableActive () {
  int active;
  spinlock_acquire(&freemem_lock);
  active = allocTableActive;
  spinlock_release(&freemem_lock);
  return active;
}

void
vm_bootstrap(void)
{
  /*int i;
  nRamFrames = ((int)ram_getsize())/PAGE_SIZE;  

  freeRamFrames = (coremap_entry_t*) kmalloc (sizeof(coremap_entry_t)*nRamFrames);
  if(freeRamFrames==NULL) return;

  for (i=0; i<nRamFrames; i++) {    
    freeRamFrames[i].status = CLEAN;
    freeRamFrames[i].paddr = -1;
    freeRamFrames[i].size = -1; 
  }

  spinlock_acquire(&freemem_lock);
  allocTableActive = 1;
  spinlock_release(&freemem_lock);*/
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */
static void
dumbvm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

extern int kernelAlloc;

static paddr_t 
getfreeppages(unsigned long npages) {
  paddr_t addr;	
  long i, first, found, np = (long)npages;

  if (!isTableActive()) return 0; 
  spinlock_acquire(&freemem_lock);
  for (i=0,first=found=-1; i<nRamFrames; i++) {
	if (freeRamFrames[i].status==FREE||freeRamFrames[i].status==CLEAN){
	if (i==0 || (freeRamFrames[i-1].status!=FREE && freeRamFrames[i-1].status!=CLEAN))
        first = i; /* set first free in an interval */   
      if (i-first+1 >= np) {
        found = first;
        break;
      }
    }
  }
	
  if (found>=0) {
    	for (i=found; i<found+np; i++) {
		freeRamFrames[i].status=DIRTY; //starts as dirty, becomes clean after flush   
    	}
    	freeRamFrames[found].size = np;
    	addr = (paddr_t) found*PAGE_SIZE;
    	freeRamFrames[found].paddr=(paddr_t) found*PAGE_SIZE;
  }
  else {
    addr = 0;
  }
  spinlock_release(&freemem_lock);
  return addr;
}

static paddr_t
getppages(unsigned long npages)
{
  paddr_t addr;
  unsigned int i;
  /* try freed pages first */
  addr = getfreeppages(npages);
  if (addr == 0) {
    /* call stealmem (kernel only) */
    spinlock_acquire(&stealmem_lock);
    addr = ram_stealmem(npages);
    spinlock_release(&stealmem_lock);
  }
  if (addr!=0 && isTableActive()) {
    spinlock_acquire(&freemem_lock);
    freeRamFrames[addr/PAGE_SIZE].size=npages;
    freeRamFrames[addr/PAGE_SIZE].paddr=addr;
    for (i=0; i<npages; i++)
	freeRamFrames[(addr/PAGE_SIZE)+i].status=DIRTY;
    spinlock_release(&freemem_lock);
  } 
  return addr;
}

static int 
freeppages(paddr_t addr, unsigned long npages){
  long i, first, np=(long)npages;	
  if (!isTableActive()) return 0; 
  first = addr/PAGE_SIZE;
  //KASSERT(allocSize!=NULL);
  KASSERT(freeRamFrames!=NULL);
  KASSERT(nRamFrames>first);

  spinlock_acquire(&freemem_lock);
  for (i=first; i<first+np; i++) {
    freeRamFrames[i].status = FREE;
  }
  spinlock_release(&freemem_lock);

  return 1;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;
	dumbvm_can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr){
  if (isTableActive()) {
    paddr_t paddr = addr - MIPS_KSEG0;
    long first = paddr/PAGE_SIZE;	
    //KASSERT(allocSize!=NULL);
    KASSERT(freeRamFrames!=NULL);
    KASSERT(nRamFrames>first);
    freeppages(paddr, freeRamFrames[first].size);	
  }
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i, index;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		  /* We always create pages read-write, so we can't get this */
		  panic("dumbvm: got VM_FAULT_READONLY\n");
		  //kprintf("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
		    break;
	    case VM_FAULT_WRITE:
		    break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	// KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	// KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	// KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	// KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	// KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	// KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

  KASSERT(as -> pagetable != NULL);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

//////////////////////////////////////////check inside tlb//////////////////////
	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID && ehi == faultaddress) {
			kprintf("CHECKPOINT_ PAGE IS HERE! %d", i); 
		}
	}
///////////////////////////////////////////////////////////////////////////////////


  // physical page index
  /* index = pagetable_search(as -> pagetable, faultaddress); */
  /* if(index == -1){ */
    /* pagetable_add(as -> pagetable, faultaddress);  */
    // TODO ASS3.3 swap in from disk 
    // update TLB
  /* } */

  // search for a pagetable entry
    // if pagetable entry found in memory, update TLB and return 0 (restart)
    // if not found, swap in from disk (ASS3.3)
  // if pagetable entry not found for faultaddress,
  // create a new pagetable entry in pagetable, and allocate a new physical page

	if ((faultaddress >= vbase1 && faultaddress < vtop1) || (faultaddress >= vbase2 && faultaddress < vtop2) || (faultaddress >= stackbase && faultaddress < stacktop)) {
    // valid address, let's see if it's present in pagetable
    index = pagetable_search(as -> pagetable, faultaddress);
    if(index == -1) // not present as pagetable entry
      index = pagetable_add(as -> pagetable, faultaddress);
	  paddr = freeRamFrames[index].paddr;
	}
	else {
    // segmentation fault 
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}
	//kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n"); //no more!
	//wmd
	i = tlb_victim();
        ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	tlb_write(ehi, elo, i); 
	splx(spl);
	return 0;
	//return EFAULT;
}

int tlb_victim(void){
	kprintf("Victim >:(\n");
	return 1;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}
  as -> pagetable = kmalloc(sizeof(struct pagetable));
  if(as -> pagetable == NULL)
    return NULL;
  // TODO check
  as -> pagetable -> head = NULL;
  as -> pagetable -> tail = NULL;
  as -> pagetable -> npages = 0;

	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;

	return as;
}

void as_destroy(struct addrspace *as){
  dumbvm_can_sleep();
  freeppages(as->as_pbase1, as->as_npages1);
  freeppages(as->as_pbase2, as->as_npages2);
  freeppages(as->as_stackpbase, DUMBVM_STACKPAGES);
  kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;

	dumbvm_can_sleep();

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return ENOSYS;
}

/* 
static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}
*/

int
as_prepare_load(struct addrspace *as)
{
  /* 
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	dumbvm_can_sleep();
  // take only one page per segment, let's see if it works... TODO

	as->as_pbase1 = getppages(1);
  // pagetable_write(as -> pagetable, as -> as_pbase1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(1);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(1);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}

	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
  */
  (void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	dumbvm_can_sleep();
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	(void)as;
//KASSERT(as->as_stackpbase != 0);
	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	dumbvm_can_sleep();

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);

	*ret = new;
	return 0;
}

/* given a vaddr, search the pagetable for the corresponding entry.
 * @return -1 if not found, a positive integer if found (the index of the
 * ppage in the coremap). */
int pagetable_search(struct pagetable *pt, vaddr_t vaddr) {
  struct pagetable_entry *pe;
  if(pt -> head == NULL)  // list is empty
    return -1;
  for(pe = pt -> head; pe != NULL; pe = pe -> next){
    if(vaddr >= pe -> vaddr && vaddr < pe -> vaddr + PAGE_SIZE)
      return pe -> ppage_index;
  }
  return -1;
}

/* add an entry to the pagetable, also allocates page in memory (dynamic allocation) */
int pagetable_add(struct pagetable *pt, vaddr_t vaddr) {
  paddr_t paddr;
  struct pagetable_entry *p;     
  int index = -1;
  paddr = getppages(1);   // get a new page, notice that you don't know where it will be in physical memory
  // TODO checks
  index = paddr / PAGE_SIZE;
  // create a new pte and update its fields 
  p = kmalloc(sizeof(struct pagetable_entry));
  p -> vaddr = vaddr & PAGE_FRAME;  // get starting vaddr of vpage
  p -> ppage_index = index;
  p -> next = pt -> head;
  pt -> head = p;
  return index;
}
