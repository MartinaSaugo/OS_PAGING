#ifndef _PAGETABLE_H_
#define _PAGETABLE_H_
#include <types.h>
#include <lib.h>
#include <cpu.h>
#include <proc.h>
#include <vm.h>
#include <coremap.h>
#include <current.h>
#include <spinlock.h>

// PRESENT = in memory
// SWAPPED = in disk
typedef enum { PRESENT, SWAPPED } page_status;

typedef struct ptentry {
  vaddr_t vaddr;                    // starting virtual address of the page 
  int ppage_index;                  // maps a virtual page to a physical page (-> coremap)
  struct ptentry *next;             // implement as linked list
  page_status status;               // current status (PRESENT / SWAPPED)
} ptentry_t; 

typedef struct pt {
  ptentry_t *nil;                   // use sentinel node
  int npages;
  int nswapped;
} pt_t;

pt_t* pt_init(void);
ptentry_t *pt_search(pt_t *, vaddr_t);
int pt_add(pt_t *, paddr_t, vaddr_t);

#endif
