#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <proc.h>
#include <vm.h>
#include <coremap.h>
#include <current.h>
#include <spinlock.h>

coremap_entry_t *freeRamFrames;
long nRamFrames;
int allocTableActive = 0;

coremap_entry_t * coremap_init(void){
  int i;
  coremap_entry_t *coremap;
  paddr_t paddr;
  vaddr_t vaddr;
  paddr_t ramsize = ram_getsize(); // returns lastpaddr
  paddr_t kernelsize=ram_getfirstfree(); //returns first available address after initializing kernel+exceptHandler... 
  long ramsizepages = ramsize / PAGE_SIZE;  // size of ram in pages
  nRamFrames=ramsizepages;
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
  paddr=PAGE_SIZE*(kernelpages);

  if(paddr == 0) 
    return NULL;
  vaddr = PADDR_TO_KVADDR(paddr);
  KASSERT(vaddr % PAGE_SIZE == 0);
  coremap=(coremap_entry_t*)vaddr;
	
  for (i=0; i<kernelpages; i++) { //alloc kernel   
    coremap[i].status = FIXED;
    coremap[i].paddr = 0;
    coremap[i].size = kernelpages; 
  }  
  for (; i<kernelpages+pages; i++) {  //alloc coremap
    coremap[i].status = FIXED;
    coremap[i].paddr = paddr;
    coremap[i].size = pages; 
  }

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


