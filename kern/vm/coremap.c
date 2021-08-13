#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <proc.h>
#include <vm.h>
#include <coremap.h>
#include <current.h>

extern long nRamFrames; // defined in vm.c

coremap_entry_t *
coremap_init(void){
  int i;
  coremap_entry_t *coremap;
  paddr_t paddr;
  vaddr_t vaddr;
  paddr_t ramsize = ram_getsize(); // returns lastpaddr
  long ramsizepages = ramsize / PAGE_SIZE;  // size of ram in pages
  nRamFrames = ramsizepages;
  long kernelpages;  // kernel size in pages
  if(sizeof(struct proc) % PAGE_SIZE == 0)
    kernelpages = sizeof(struct proc) / PAGE_SIZE;
  else 
    kernelpages = sizeof(struct proc) / PAGE_SIZE + 1;
  long size = ramsizepages * sizeof(coremap_entry_t);   // size to alloc for coremap
  long pages;
  if(size % PAGE_SIZE == 0) 
    pages = size / PAGE_SIZE;       // if multiple of PAGE_SIZE
  else 
    pages = (size / PAGE_SIZE) + 1; // if not a multiple of PAGE_SIZE...

  paddr = ram_getmem(kernelpages, pages);
  if(paddr == 0) 
    return NULL;
  vaddr = PADDR_TO_KVADDR(paddr);
  KASSERT(vaddr % PAGE_SIZE == 0);

  // now initialize coremap entries
  coremap = (coremap_entry_t *) vaddr;
  for(i = 0; i < kernelpages; i++) {
    /* set FIXED status for pages which are allocated for kernel */
    coremap[i].status = FIXED;
    coremap[i].paddr = 0; // we should get the firstpaddr from RAM, TODO: modify in future
    coremap[i].size = kernelpages;
  }
  for(; i < kernelpages + pages; i++){
    /* set FIXED status for pages which are allocated for coremap */
    coremap[i].status = FIXED;
    coremap[i].paddr = paddr;
    coremap[i].size = pages;
  }
  for(; i < ramsizepages; i++) {
    /* init all the other entries, initial status = CLEAN */
    coremap[i].status = CLEAN;
    coremap[i].paddr = -1;
    coremap[i].size = 0;
  }
  return coremap;
}

