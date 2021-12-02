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

  	for (i=0; i<kernelpages; i++) { //alloc kernel   
		coremap[i].status = FIXED;
  	  	// TODO: approccio molto ignorante
  	  	coremap[i].paddr = i * PAGE_SIZE; 
  	  	coremap[i].size = kernelpages; 
  	}  
  	for (; i<kernelpages+pages; i++) {  //alloc coremap
		coremap[i].status = FIXED;
		coremap[i].paddr = paddr;
		coremap[i].size = pages; 
  	}

	firstFreeFrame = i;

  	for (; i<ramsizepages; i++) { //alloc make everything empty   
		coremap[i].status = CLEAN;
  	  	coremap[i].paddr = -1;
  	  	coremap[i].size = 0; 
  	}

  	//spinlock_acquire(&freemem_lock);
  	allocTableActive = 1;
  	//spinlock_release(&freemem_lock);

  	return coremap;
}

// select n consecutive victims 
// (in most cases nvictims should be equal to 1)
int coremap_victim_selection(int nvictims){
	static int next_victim = 0;
	int victim, found = 0, i, iteration = 0;
	// if at the end, restart from the first non-FIXED page
	if(next_victim >= nRamFrames)
		next_victim = firstFreeFrame;
	victim = next_victim;
	while(!found){
		found = 1;
		// check if there's an interval of consecutive victims
		for(i=0; i < nvictims; i++)
			// this should never happen, this function should always return, but 
			// for the sake of safety let's make this additional check...
			if(freeRamFrames[victim + i].status == FIXED){
				found = 0;
				victim++;
				if(victim >= nRamFrames){
					victim = firstFreeFrame;
					iteration ++;
					// if second iteration then space not found
					if(iteration >= 2)
						return -1;
				}
			}
	}
	next_victim += nvictims;
	return victim;
}
