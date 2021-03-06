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
#include <vmstats.h>

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
		//update stats
		stats.tlb_invalidation++;
	}
		splx(spl);	
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	//vaddr_t vbase1, vtop1, vbase2, vtop2, 
	vaddr_t vbase, vtop, stackbase, stacktop;
	vaddr_t faultpage;
	paddr_t paddr;
	int i, index = 0, err;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl, valid=0;
	struct region *curr;

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
		  panic("dumbvm: got VM_FAULT_READONLY \n");
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
	KASSERT(as -> pt != NULL);
	KASSERT(as-> start_region != NULL);

	curr = as->start_region;
	while(curr!=NULL)
	{
		vbase = curr->start;
		vtop = vbase + curr->npages*PAGE_SIZE;
		if(faultaddress >= vbase && faultaddress <= vtop)
		{	valid=1;
			break;
		}
		curr=curr->next;
	}	
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;
	if(!valid && faultaddress >= stackbase && faultaddress <= stacktop)
		valid=1;

	/*
	 * check if faultpage is a valid virtual address
	 * 0. not a valid address = segmentation fault
	 * 1. a valid address: search for a ptentry
	 *		1.1 entry not found: (real fault) find space in the coremap
	 *		1.2 not found because not in memory: swap in from disk
	 *		1.3 found (in memory): update TLB and return 0 (restart)
	 */
	if (valid) 
	{
		// 1. valid address, let's see if it's present in pt
		ptentry_t *pte = pt_search(as -> pt, faultpage);
		// 1.1 not in memory and not SWAPPED - search space in coremap and allocate
		if(pte == NULL){
			paddr = getuserppage();
			index = paddr / PAGE_SIZE;
			coremap[index].vaddr = faultpage;
			KASSERT(paddr != 0);
			// add new pagetable entry
			err = pt_add(as -> pt, paddr, faultpage);
			KASSERT(err != -1);

			//update stats
			stats.tlb_faults++;
			stats.page_faults_disk++;
			stats.page_faults_elf++;
		}
		// 1.2 not in memory because it has been swapped - implement swap in
		else if(pte -> swapped){
			paddr = getuserppage();				// get a new free frame 
			// kprintf("\tswap in: page 0x%x in frame %d\n", faultpage, paddr/PAGE_SIZE);
			err = swap_in(pte -> swap_index, paddr);	// swap in
			KASSERT(err == 0);
			coremap[paddr/PAGE_SIZE].vaddr = faultpage;	// update coremap entry
			pte -> ppage_index = paddr/PAGE_SIZE;		// update ptentry
			pte -> swapped = 0;				// page is now present

			//update stats
			stats.tlb_faults++;
			stats.page_faults_disk++;
			stats.page_faults_swapfile++;
		}
		// 1.3 found in memory => update TLB and return 0 (i.e. restart)
		else {
			paddr = pte -> ppage_index * PAGE_SIZE;
			(void) paddr;
			//update stats
			stats.tlb_faults++;
			stats.tlb_reloads++;
		}
	}
	// 0. invalid address = segmentation fault
	else {
		kprintf("***** EFAULT!\n");
		return EFAULT;
	}

	index = paddr / PAGE_SIZE;
	(void) index;
	
	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i < NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultpage;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultpage, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		//update stats
		stats.tlb_faults_w_free++;
		return 0;
	}

	i = tlb_get_rr_victim();
	ehi = faultpage;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultpage, paddr);
	tlb_write(ehi, elo, i); 
	splx(spl);
	//update stats
	stats.tlb_faults_w_replace++;
	return 0;
}

// TODO  synchronize
/* get one physical page for the user process; 
 * if it's not available another page it is swapped */
paddr_t getuserppage(){
	struct addrspace *as;
	int victim, swap_index, freefound = 0;

	if(!isTableActive())
		panic("getuserppage called while table not active\n");

	// try to find a free frame
	for(victim = firstFreeFrame; !freefound && victim < nRamFrames; victim++){
		if(coremap[victim].status == CLEAN || coremap[victim].status == FREE){
			freefound = 1;
			break;
		}
	}

	if(!freefound){
		// no more free frames: swap out
		as = proc_getas();
		victim = coremap_victim_selection(1);
		vaddr_t vvaddr = coremap[victim].vaddr;
		ptentry_t *ptvictim = pt_search(as -> pt, vvaddr);
		KASSERT(ptvictim != NULL);
		KASSERT(ptvictim -> ppage_index == victim);
		KASSERT(ptvictim -> vaddr == vvaddr);
		// kprintf("\tswap out: frame %d (0x%x) which contains 0x%x\n", victim, coremap[victim].paddr, coremap[victim].vaddr);
		swap_index = swap_out(coremap[victim].paddr);
		KASSERT(swap_index >= 0);
		ptvictim -> swap_index = swap_index;
		ptvictim -> swapped = 1;
		tlb_invalidate_entry(ptvictim -> vaddr);
	}

	coremap[victim].status = DIRTY;
	coremap[victim].paddr = victim * PAGE_SIZE;
	coremap[victim].size = 1;
	return (paddr_t) (victim * PAGE_SIZE);
}

/* getppages should only be called by the kernel (via alloc_kpages); 
 * it allocates a contiguous sequence of FIXED frames; these frames are
 * not referenced by the pagetable, so they should NEVER be swapped out */
paddr_t getppages(unsigned long npages) {
	struct addrspace *as = NULL;
	paddr_t paddr = (paddr_t) 0x0;
	int i = 0;
	unsigned int freePages = 0;
	int index = 0, victim = 0, swap_index = 0;

	// TODO synchronize operations on coremap
	// TODO check if page has been modified, if not, evict without swapping out
	// only during bootstrap 
	if(!isTableActive()){
		spinlock_acquire(&stealmem_lock);
	  	paddr = ram_stealmem(npages);
	  	spinlock_release(&stealmem_lock);
	  	return paddr;
	}

	for(i = firstFreeFrame; i < nRamFrames; i++){
		if(coremap[i].status == FREE || coremap[i].status == CLEAN)
			freePages++;
		else 
			freePages = 0;
		if(freePages == npages){
			index = (i - npages + 1);
			paddr = index * PAGE_SIZE;
			break;
		}
	}

	// swap out
	if(freePages != npages){
		as = proc_getas();
		victim = coremap_victim_selection(npages);
		for(i = 0; i < (long) npages; i++){
			// vvaddr == victim's vaddr
			vaddr_t vvaddr = coremap[victim + i].vaddr;
			ptentry_t *ptvictim = pt_search(as -> pt, vvaddr);
			KASSERT(ptvictim != NULL);
			KASSERT(ptvictim -> ppage_index == (victim + i));
			KASSERT(ptvictim -> vaddr == vvaddr);
			swap_index = swap_out(coremap[victim + i].paddr);
			KASSERT(swap_index >= 0);
			coremap[victim + i].status = FIXED;
			ptvictim -> swapped = 1;
			ptvictim -> swap_index = swap_index;
			tlb_invalidate_entry(ptvictim -> vaddr);
		}
		index = victim;
		paddr = index * PAGE_SIZE;
	}

	coremap[index].size = npages;
	coremap[index].paddr = paddr;
	coremap[index].vaddr = PADDR_TO_KVADDR(paddr);
	coremap[index].status = FIXED;
	return paddr;
}

/* this should be called for user pages only, so it frees a single page */
int freeuserppage(paddr_t addr){

	// spinlock_acquire(&freemem_lock);

	KASSERT((addr % PAGE_SIZE) == 0);			// make sure it's page-aligned
	unsigned int index = addr / PAGE_SIZE;
	KASSERT(coremap[index].status != FIXED);	// make sure it's not a kernel page

	coremap[index].status = FREE;
	coremap[index].size = 0;
	coremap[index].vaddr = 0;
	coremap[index].paddr = 0x0;
	
	// spinlock_release(&freemem_lock);
	return 0;
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
	// KASSERT page == FIXED
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
	as -> start_region = NULL; 
	return as;
}

void as_destroy(struct addrspace *as)
{
	KASSERT(as != NULL);
	KASSERT(as -> pt != NULL);
	KASSERT(as-> start_region!=NULL);
	struct region *curr, *prev;
	int spl;
	spl = splhigh();
	//free regions
	curr = as -> start_region;
	while(curr!=NULL)
	{
		prev = curr;
		curr = curr->next;
		kfree(prev);
	}
	//free pt
	pt_destroy(&(as -> pt));
	for(int i = 0; i < NUM_TLB; i++)
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	//free as
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
	KASSERT(as != NULL);
	size_t npages;
	struct region *curr;
	
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if(as->start_region==NULL)
	{
		curr = kmalloc(sizeof(struct region));
		if(curr == NULL)
			return ENOMEM;
		as->start_region=curr;
	}
	else
	{
		curr = as->start_region;
		while(curr->next!=NULL)
			curr=curr->next;
		curr->next=kmalloc(sizeof(struct region));
		if(curr->next==NULL)
			return ENOMEM;
		curr=curr->next;
	}
	curr->start=vaddr;
	curr->size=sz;
	curr->npages=npages;
	curr->next=NULL;
	return 0;
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
	struct region *oldR, *tempR;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	oldR = old->start_region;

	//copy pt

	while(oldR != NULL)
	{
		if(new -> start_region == NULL)
		{
			tempR = kmalloc(sizeof(struct region));
			if(tempR == NULL)
				return ENOMEM;
			new -> start_region = tempR;
		}
		else
		{
			tempR->next = kmalloc(sizeof(struct region));
			if(tempR -> next ==NULL)
				return ENOMEM;
			tempR = tempR->next;
		}
		tempR->start=oldR->start;
		tempR->size=oldR->size;
		tempR->npages = oldR->npages;
		//tempR -> read = oldR->read;
		//tempR -> write = oldR->write;
		//tempR -> exec = oldR -> exec;
		tempR -> next = NULL;
		oldR = oldR->next;		
	}

	*ret = new;
	return 0;
}
