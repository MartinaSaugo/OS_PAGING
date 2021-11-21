#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <proc.h>
#include <vm.h>
#include <coremap.h>
#include <current.h>
#include <spinlock.h>

int allocTableActive=0;
struct spinlock stealmem_lock= SPINLOCK_INITIALIZER;
struct spinlock freemem_lock= SPINLOCK_INITIALIZER;

coremap_entry_t * coremap_init(void){
  int i;
  coremap_entry_t *coremap;
  paddr_t paddr;
  vaddr_t vaddr;
  paddr_t ramsize = ram_getsize(); // returns lastpaddr
  paddr_t kernelsize = ram_getfirstfree(); //returns first available address after initializing kernel+exceptHandler... 
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

paddr_t getfreeppages(unsigned long npages) {
  paddr_t addr;	
  long i, first, found, np = (long)npages;
     
  if (!isTableActive()) 
	  return 0; 

  spinlock_acquire(&freemem_lock);
  for (i=0, first=found=-1; i<nRamFrames; i++) {
	if (freeRamFrames[i].status==FREE||freeRamFrames[i].status==CLEAN){
	if (i==0 || (freeRamFrames[i-1].status!=FREE && freeRamFrames[i-1].status!=CLEAN))
        first = i; // set first free in an interval 
      if (i-first+1 >= np) {
        found = first;
        break;
      }
    }
  }
  // try to find: if not found, found = -1 
  
  // no more free space (we return 0 as special value since first pages
  // are occupied by kernel (FIXED) and kernel doesn't use coremap...)
  if (found < 0) 
        // TODO modify here
        return 0;

  addr = (paddr_t) found * PAGE_SIZE;

  spinlock_release(&freemem_lock);
  return addr;
}

/* returns 0 if the page is found, -1 if not found */
/* the paddr of the page is put into result */
int getfreeppage(paddr_t *paddr){
    paddr_t addr;  
    long i, first, found;

    if(!isTableActive())
        return -1;

    spinlock_acquire(&freemem_lock);

    first = -1;
    found = -1;

    for(i=0; i < nRamFrames; i++){
        /* if a free/clean frame is found */
        if(freeRamFrames[i].status == FREE || freeRamFrames[i].status == CLEAN){
            if(i==0 || (freeRamFrames[i-1].status != FREE && freeRamFrames[i-1].status != CLEAN))
                first = i;
            if(i-first+1 >= 1){
                found = first;
                break;
            }
        }
    }

    if(found < 0)
        return -1;
    addr = (paddr_t) found * PAGE_SIZE;

    /* return the address */
    *paddr = addr;  

    spinlock_release(&freemem_lock);
    return 0;

}

int isTableActive () {
  int active;
  spinlock_acquire(&freemem_lock);
  active = allocTableActive;
  spinlock_release(&freemem_lock);
  return active;
}

paddr_t getppages(unsigned long npages)
{
  paddr_t addr;
  unsigned int i;
  if(!isTableActive())	/* kernel only */
  {
    spinlock_acquire(&stealmem_lock);
    addr = ram_stealmem(npages);
    spinlock_release(&stealmem_lock);
    return addr;
  }
  // try freed pages first 
  addr = getfreeppages(npages);
  if (addr == 0){
	  // at this point getfreeppages should never return a 0 value
	  panic("No more free pages, you should rely on swap.\n");
  }
  spinlock_acquire(&freemem_lock);
  freeRamFrames[addr/PAGE_SIZE].size=npages;
  freeRamFrames[addr/PAGE_SIZE].paddr=addr;
  for (i=0; i<npages; i++)
	freeRamFrames[(addr/PAGE_SIZE)+i].status=DIRTY;
  spinlock_release(&freemem_lock);
  return addr;
}

int freeppages(paddr_t addr, unsigned long npages){
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

vaddr_t alloc_kpages(unsigned npages)
{
	paddr_t pa;
	dumbvm_can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa); 
}

void free_kpages(vaddr_t addr)
{
  if (isTableActive()) {
    paddr_t paddr = addr - MIPS_KSEG0;
    long first = paddr/PAGE_SIZE;	
    //KASSERT(allocSize!=NULL);
    KASSERT(freeRamFrames!=NULL);
    KASSERT(nRamFrames>first);
    freeppages(paddr, freeRamFrames[first].size);	
  }
}
