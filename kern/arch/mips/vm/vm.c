/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *    The President and Fellows of Harvard College.
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

struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
struct spinlock freemem_lock = SPINLOCK_INITIALIZER;

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

void tlb_invalidate_entry(vaddr_t remove_vaddr) {
	//uint32_t ehi, elo;	
	int spl = splhigh();
	int index = -1;
	index = tlb_probe(remove_vaddr, 0);
	if (index >= 0) {
		tlb_write(TLBHI_INVALID(index), TLBLO_INVALID(), index);
	}
		splx(spl);	
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	vaddr_t faultpage;
	paddr_t paddr;
	int i, index = 0, result;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultpage = faultaddress & PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultpage);
	as = proc_getas();

	//No process. This is probably a kernel fault early in boot. Return EFAULT so as to panic instead of getting into an infinite faulting loop.
	if (curproc == NULL) { 
		return EFAULT;
	}

	//No address space set up. This is probably also a kernel fault early in boot.
	if (as == NULL) { 
		return EFAULT;
	}

	switch (faulttype) {
		/* READONLY = 2 */
		case VM_FAULT_READONLY: // We always create pages read-write, so we can't get this 
		  panic("dumbvm: got VM_FAULT_READONLY\n");
		/* WRITE = 1 */ 
		case VM_FAULT_WRITE:
			break;
		/* READ = 0 */ 
		case VM_FAULT_READ:
			break;
		default:
		return EINVAL;
	}

	// Assert that the address space has been set up properly. 
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);

	KASSERT(as -> pt != NULL);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	/*
	 * check if faultaddress is a valid address
	 * 0. not a valid address = segmentation fault
	 * 1. a valid address: search for a ptentry
	 *		1.1 entry not found: (real fault) find space in the coremap
	 *		1.2 not found because not in memory: swap in from disk
	 *		1.3 found (in memory): update TLB and return 0 (restart)
	 */
	if ((faultpage >= vbase1 && faultpage < vtop1) || (faultpage >= vbase2 && faultpage < vtop2) || (faultpage >= stackbase && faultpage < stacktop)) {
		// 1. valid address, let's see if it's present in pt
		ptentry_t *pte = pt_search(as -> pt, faultpage);
		// 1.1 not in memory and not SWAPPED - search space in coremap and allocate
		if(pte == NULL){
			paddr = getuserppage();
			KASSERT(paddr != 0);
			// add new pagetable entry
			result = pt_add(as -> pt, paddr, faultaddress);
			KASSERT(result != -1);
		}
		// 1.2 not in memory because it has been swapped - implement swap in
		else if(pte -> status == SWAPPED){
			// TODO implement swap in 
			panic("implement swap in\n");
		}
		// 1.3 found in memory => update TLB and return 0 (i.e. restart)
		else {
			paddr = pte -> ppage_index * PAGE_SIZE;
		}
	}
	// 0. invalid address = segmentation fault
	else {
		kprintf("EFAULT!\n");
		return EFAULT;
	}

	index = paddr / PAGE_SIZE;

	if(faulttype == VM_FAULT_READONLY)
		kprintf("(O) index: %d, paddr: %x\n", index, paddr);
	if(faulttype == VM_FAULT_WRITE)
		kprintf("(W) index: %d, paddr: %x\n", index, paddr);
	if(faulttype == VM_FAULT_READ)
		kprintf("(R) index: %d, paddr: %x\n", index, paddr);
	
	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultpage;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultpage, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	i = tlb_get_rr_victim();
	ehi = faultpage;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultpage, paddr);
	tlb_write(ehi, elo, i); 
	splx(spl);
	return 0;
}

// TODO  synchronize
/* get one physical page for the user process */
paddr_t getuserppage(){
	struct addrspace *as;
	int i, victim, result;
	if(!isTableActive())
		panic("getuserppage called while table not active\n");
	for(i = firstFreeFrame; i < nRamFrames; i++){
		if(coremap[i].status == CLEAN || coremap[i].status == FREE){
			coremap[i].status = DIRTY;
			return (paddr_t) (i * PAGE_SIZE);
		}
	}
	as = proc_getas();
	// no more space, swap out
	victim = coremap_victim_selection(1);
	vaddr_t vvaddr = coremap[victim].vaddr;
	ptentry_t *ptvictim = pt_search(as -> pt, vvaddr);
	KASSERT(ptvictim != NULL);
	KASSERT(ptvictim -> ppage_index == victim);
	KASSERT(ptvictim -> vaddr == vvaddr);
	result = swap_out(coremap[victim].paddr);
	KASSERT(result == 0);
	ptvictim -> status = SWAPPED;
	tlb_invalidate_entry(ptvictim -> vaddr);
	coremap[victim].status = DIRTY;
	return (paddr_t) (victim * PAGE_SIZE);
}

paddr_t getfreeppages(unsigned long npages) {
	paddr_t addr;    
	long i, first, found, np = (long)npages;
	 
	if (!isTableActive()) 
		return 0; 

	spinlock_acquire(&freemem_lock);

	first = -1;
	found = -1; 

	for (i = firstFreeFrame; i < nRamFrames; i++) {
		if (coremap[i].status == FREE || coremap[i].status == CLEAN){
			if (coremap[i-1].status != FREE && coremap[i-1].status != CLEAN)
				first = i; // set first free in an interval 
			if (i-first+1 >= np) {
				found = first;
				break;
			}
		}
	}
	// try to find: if not found, found = -1 
	// no more free space :we return 0 as special value since first pages
	// are occupied by kernel
	if (found < 0) 
		return 0;
	addr = (paddr_t) found * PAGE_SIZE;
	spinlock_release(&freemem_lock);
	return addr;
}

/* getppages should only be called by the kernel (via alloc_kpages); 
 * it allocates a contiguous sequence of FIXED frames; these frames are
 * not referenced by the pagetable, so they should NEVER be swapped out */
paddr_t getppages(unsigned long npages) {
	struct addrspace *as;
	paddr_t paddr;
	unsigned int i;
	int result, index, victim;
	// TODO synchronize operations on coremap
	// TODO check if page has been modified, if not, evict without swapping out
	// only during bootstrap 
	if(!isTableActive()){
		spinlock_acquire(&stealmem_lock);
	  	paddr = ram_stealmem(npages);
	  	spinlock_release(&stealmem_lock);
	  	return paddr;
	}
	// try freed pages first 
	// TODO modify here! (remove getfreeppages)
	paddr = getfreeppages(npages);
	// no more free space, SWAP OUT
	if (paddr == 0){
		as = proc_getas();
		// select n consecutive victims
		victim = coremap_victim_selection(npages);
		// swap out victims 
		for(i=0; i < npages; i++){
			// vvaddr == victim's vaddr
			vaddr_t vvaddr = coremap[victim].vaddr;
			ptentry_t *ptvictim = pt_search(as -> pt, vvaddr);
			KASSERT(ptvictim != NULL);
			KASSERT(ptvictim -> ppage_index == victim);
			KASSERT(ptvictim -> vaddr == vvaddr);
			result = swap_out(coremap[victim].paddr);
			KASSERT(result == 0);
			coremap[victim].status = FREE;
			ptvictim -> status = SWAPPED;
			tlb_invalidate_entry(ptvictim -> vaddr);
		}
		// now there should be enough space
		paddr = getfreeppages(npages);
		KASSERT(paddr != 0);
	}
	spinlock_acquire(&freemem_lock);
	index = paddr / PAGE_SIZE;
	coremap[index].size = npages;
	coremap[index].paddr = paddr;
	for (i=0; i < npages; i++)
		coremap[index + i].status = FIXED;
	spinlock_release(&freemem_lock);
	return paddr;
}

int freeppages(paddr_t addr, unsigned long npages){
	panic("implement freeppages\n");
	long i, first, np=(long)npages;	
	// kernel only
	if (!isTableActive()) 
		return 0; 
	first = addr/PAGE_SIZE;
	// KASSERT(allocSize!=NULL);
	KASSERT(coremap!=NULL);
	KASSERT(nRamFrames > first);
	
	spinlock_acquire(&freemem_lock);
	for (i=first; i < first+np; i++) {
	  coremap[i].status = FREE;
	  coremap[i].size = 0;
	}
	spinlock_release(&freemem_lock);
	return 1;
}

vaddr_t alloc_kpages(unsigned npages)
{
	paddr_t pa;
	dumbvm_can_sleep();
	pa = getppages(npages);
	if (pa==0) 
		return 0;
	return PADDR_TO_KVADDR(pa); 
}

void free_kpages(vaddr_t addr)
{
	if (!isTableActive())
		return;
	int i;
	paddr_t paddr = addr - MIPS_KSEG0;
	KASSERT(coremap != NULL);
	spinlock_acquire(&freemem_lock);
	KASSERT(paddr % PAGE_SIZE == 0);
	long first = paddr/PAGE_SIZE;
	KASSERT(nRamFrames > first);
	long size = coremap[first].size;
	for(i = 0; i < size; i++){
		coremap[first + i].status = FREE;
		coremap[first + i].size = 0;
	}
	spinlock_release(&freemem_lock);
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
