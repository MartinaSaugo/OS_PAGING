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
  	coremap_entry_t *coremap;
  	paddr_t paddr;
  	vaddr_t vaddr;
  	paddr_t ramsize = ram_getsize(); // returns lastpaddr
  	paddr_t kernelsize = ram_getfirstfree(); //returns first available address after initializing kernel+exceptHandler... 
  	long ramsizepages = ramsize / PAGE_SIZE;  // size of ram in pages
  	nRamFrames = ramsizepages;
  	long kernelpages;  // kernel size in pages
  	if(kernelsize % PAGE_SIZE == 0)
		kernelpages = kernelsize / PAGE_SIZE;
  	else 
		kernelpages = kernelsize / PAGE_SIZE + 1;
  	long size = ramsizepages * sizeof(coremap_entry_t);   // size to alloc for coremap
  	long pages;
  	if(size % PAGE_SIZE == 0) 
		pages = size / PAGE_SIZE;       // if multiple of PAGE_SIZE
  	else 
		pages = (size / PAGE_SIZE) + 1; // if not a multiple of PAGE_SIZE...

  	//paddr = ram_getmem(kernelpages, pages);
  	if(kernelpages+pages>ramsizepages)
		panic("Too little memory for the bare minumin\n");

  	paddr = PAGE_SIZE*(kernelpages);

  	if(paddr == 0) 
		return NULL;
  	vaddr = PADDR_TO_KVADDR(paddr);
  	KASSERT(vaddr % PAGE_SIZE == 0);
  	coremap=(coremap_entry_t*)vaddr;

  	for (i=0; i<kernelpages; i++) {			// alloc kernel   
		coremap[i].status = FIXED;
  	  	coremap[i].paddr = i * PAGE_SIZE; 
  	  	coremap[i].size = kernelpages; 
  	}  
  	for (; i<kernelpages+pages; i++) {		// alloc coremap
		coremap[i].status = FIXED;
		coremap[i].paddr = paddr;
		coremap[i].size = pages; 
  	}

	firstFreeFrame = i;

  	for (; i<ramsizepages; i++) {			// alloc make everything CLEAN 
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
	int victim, found = 0, i, iteration = 0;
	// if at the beginning, avoid the FIXED section 
	if(next_victim == 0)
		next_victim = firstFreeFrame;
	// if at the end, restart from the first non-FIXED page
	if(next_victim >= nRamFrames)
		next_victim = firstFreeFrame;
	victim = next_victim;
	// TODO spinlock_acquire?
	while(!found){
		found = 1;
		// check if there's an interval of consecutive victims
		for(i = 0; found && victim + i < nRamFrames && i < nvictims; i++){
			// a FIXED page cannot be a victim
			if(freeRamFrames[victim + i].status == FIXED){
				found = 0;
				victim += i + 1; // jump directly to the frame after FIXED
				if(victim >= nRamFrames){ 
					// restart from the beginning
					victim = firstFreeFrame;
					iteration++;
					// if second iteration then space not found
					if(iteration >= 2)
						panic("no more victims in the coremap\n");
				}
			}
		}
	}
	next_victim += nvictims;
	return victim;
}
