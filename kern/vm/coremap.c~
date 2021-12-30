#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <proc.h>
#include <vm.h>
#include <coremap.h>
#include <current.h>

coremap_entry_t *
coremap_init(void){
  paddr_t paddr;
  vaddr_t vaddr;
  paddr_t ramsize = ram_getsize(); // returns lastpaddr
  long ramsizepages = ramsize / PAGE_SIZE;  // size of ram in pages
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
  return (coremap_entry_t *) vaddr;
}

