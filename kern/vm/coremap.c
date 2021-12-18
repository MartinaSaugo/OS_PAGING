#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <proc.h>
#include <vm.h>
#include <coremap.h>
#include <current.h>
#include <spinlock.h>

coremap_entry_t * coremap_init(void){
	int i;
	long ramsizepages, kernelpages, coremap_size, coremap_pages;
  	coremap_entry_t *coremap;
  	vaddr_t vaddr;
  	paddr_t paddr, ramsize, kernelsize;

  	ramsize = ram_getsize();          // returns lastpaddr
  	kernelsize = ram_getfirstfree();  //returns first available address after initializing kernel+exceptHandler... 
  	ramsizepages = ramsize / PAGE_SIZE;  // size of ram in pages
  	nRamFrames = ramsizepages;
  	if(kernelsize % PAGE_SIZE == 0)
		kernelpages = kernelsize / PAGE_SIZE;
  	else 
		kernelpages = kernelsize / PAGE_SIZE + 1;
  	coremap_size = ramsizepages * sizeof(coremap_entry_t);   // size to alloc for coremap
  	if(coremap_size % PAGE_SIZE == 0) 
		coremap_pages = coremap_size / PAGE_SIZE;       // if multiple of PAGE_SIZE
  	else 
		coremap_pages = (coremap_size / PAGE_SIZE) + 1; // if not a multiple of PAGE_SIZE...

  	//paddr = ram_getmem(kernelpages, pages);
  	if(kernelpages + coremap_pages > ramsizepages)
		panic("Too little memory for the bare minumin\n");

  	paddr = PAGE_SIZE * (kernelpages);

  	if(paddr == 0) 
		return NULL;
  	vaddr = PADDR_TO_KVADDR(paddr);
  	KASSERT(vaddr % PAGE_SIZE == 0);
  	coremap = (coremap_entry_t *) vaddr;

	// kernel pages
  	for (i=0; i < kernelpages; i++) {
		coremap[i].status = FIXED;
  	  	coremap[i].paddr = i * PAGE_SIZE; 
  	  	coremap[i].size = kernelpages; 
  	}  

	// coremap pages
  	for (; i < kernelpages + coremap_pages; i++) {
		coremap[i].status = FIXED;
		coremap[i].paddr = paddr;
		coremap[i].size = coremap_pages; 
  	}

	firstFreeFrame = i;

	// all other pages
  	for (; i < ramsizepages; i++) {				// alloc make everything CLEAN 
		coremap[i].status = CLEAN;
  	  	coremap[i].paddr = -1;
  	  	coremap[i].size = 0; 
  	}

  	//spinlock_acquire(&freemem_lock);
  	allocTableActive = 1;
  	//spinlock_release(&freemem_lock);

  	return coremap;
}

// select n consecutive victims (to swap out)
// (in most cases nvictims should be equal to 1)
// @return the first victim of the interval (index in the coremap)
int coremap_victim_selection(int nvictims){
	static int next_victim = 0;
	int victim, found = 0, i, iteration = 0, victimsfound = 0;
	// if at the beginning, avoid the FIXED section 
	if(next_victim == 0)
		next_victim = firstFreeFrame;
	// if at the end, restart from the first non-FIXED page
	if(next_victim >= nRamFrames)
		next_victim = firstFreeFrame;
	victim = next_victim;
	i = victim;
	// TODO spinlock_acquire?
	while(!found){
		found = 1;
		// check if there's an interval of consecutive victims
		for(; i < nRamFrames && victimsfound < nvictims; i++){
			victimsfound++;
			if(coremap[i].status == FIXED){
				found = 0;
				victimsfound = 0;
			}
		}
		if(victimsfound == nvictims){
			victim = i - nvictims;
			found = 1;
		}
		if(i >= nRamFrames){
			i = firstFreeFrame;
			iteration++;
			if(iteration >= 2)
				panic("no more victims in the coremap\n");
		}
	}
	KASSERT(victimsfound == nvictims);
	next_victim = victim + nvictims;
	return victim;
}
