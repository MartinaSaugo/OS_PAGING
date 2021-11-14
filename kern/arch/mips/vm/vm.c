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
#include <addrspace.h>
#include <vm.h>

#include <pt.h>
#include <coremap.h>
#include <vm_tlb.h>
#include <swapfile.h>

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

void
vm_bootstrap(void)
{ 
	// do nothing
}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */
void dumbvm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
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
	// static int victim = 0;
	// (void *) victim; // FIX use it
	int i, index = 0, freeSpace;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);
	as = proc_getas();
	// region = as -> start_region();

	if (curproc == NULL) { //No process. This is probably a kernel fault early in boot. Return EFAULT so as to panic instead of getting into an infinite faulting loop.
		return EFAULT;
	}

	if (as == NULL) { //No address space set up. This is probably also a kernel fault early in boot.
		return EFAULT;
	}


	switch (faulttype) {
	    case VM_FAULT_READONLY: // We always create pages read-write, so we can't get this 
		  panic("dumbvm: got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
		    break;
	    case VM_FAULT_WRITE:
		    break;
	    default:
		return EINVAL;
	}

	// Assert that the address space has been set up properly. 
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

  KASSERT(as -> pt != NULL);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

  // physical page index
  /* index = pt_search(as -> pt, faultaddress); */
  /* if(index == -1){ */
    /* pt_add(as -> pt, faultaddress);  */
    // TODO ASS3.3 swap in from disk 
    // update TLB
  /* } */

	/*
	 * check if faultaddress is a valid address
	 * 0. not a valid address = segmentation fault
	 * 1. a valid address: search for a ptentry
	 *		1.1 found (in memory): update TLB and return 0 (restart)
	 *		1.2 not found because not in memory: swap in from disk
	 *		1.3 entry not found: (real fault) search space in memory
	 *			1.3.1 space not found: choose a "random" ptentry; swap out corresponding ppage; change address; also TLB
	 *			1.3.2 space found: create a new ptentry and allocate a new physical page
	 */

  // check if valid address	
  if ((faultaddress >= vbase1 && faultaddress < vtop1) || (faultaddress >= vbase2 && faultaddress < vtop2) || (faultaddress >= stackbase && faultaddress < stacktop)) 
  {
    // 1. valid address, let's see if it's present in pt
    index = pt_search(as -> pt, faultaddress);
    if(index < 0) {
		// 1.2: swapped out 
		if(index == -2){
			// swap in
			panic("implement swap in\n");
		}
		// 1.3: not in memory and not in swap file - search space in coremap 
		if(index == -1){
			// check if free space 
			freeSpace = getfreeppages(1);
			// 1.3.1 space not found: choose a "random" ptentry, swap out ppage; change ptentry address; evict TLB
			if(freeSpace == 0){
				/*
				 * TODO: check if page has been modified; if not evict; if yes
				 * swap out and evict 
				 */
				// we should refactor this
				int result = 0;
				ptentry_t *victim = pt_select_victim(as -> pt);
				index = victim -> ppage_index;
				coremap_entry_t pentry_victim = freeRamFrames[index];
				paddr_t paddr_victim = pentry_victim.paddr;
				// TODO: modify pt if swap ok
				result = swap_out(paddr_victim);
				(void) result;
				(void) victim;
				// pt_swap_out();
				// - selezionare una vittima nella pt
				// - prendere la corrispondente entry della coremap
				// - prendere la corrispondente pagina fisica
				// - copiarla nello swapfile 
				// - segnare la ptentry come SWAPPED 
				// - aggiungere una nuova ptentry e associarla alla stessa coremap entry
				// - ritornare l'index della coremap 
				panic("no more free space - implement swap out\n");
			}
			// 1.3.2 space found: create a new pt entry and allocate new ppage
			else {
				index = pt_add(as -> pt, faultaddress);
				paddr = freeRamFrames[index].paddr;
			}
		}
	}
	// 1.1 found in memory (do nothing) update TLB and restart... 
	else {
		paddr = freeRamFrames[index].paddr;
	}
  }
  // 0. invalid address = segmentation fault
  else {
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
	i = tlb_get_rr_victim();
        ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	tlb_write(ehi, elo, i); 
	splx(spl);
	return 0;
	//return EFAULT;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}
  	
	as->pt = pt_init();

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
  int spl, i;
  spl = splhigh();
  for (i=0; i<NUM_TLB; i++) 
	tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
  freeppages(as->as_pbase1, as->as_npages1);
  freeppages(as->as_pbase2, as->as_npages2);
  freeppages(as->as_stackpbase, DUMBVM_STACKPAGES);
  kfree(as);
 splx(spl);
}

void
as_activate(void)
{
	//int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}
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
  // pt_write(as -> pt, as -> as_pbase1);
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
