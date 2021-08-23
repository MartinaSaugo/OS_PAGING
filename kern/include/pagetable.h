#ifndef _PAGETABLE_H_
#define _PAGETABLE_H_
#include <machine/vm.h> // TODO: check if needed

typedef struct pagetable_entry {
  vaddr_t vaddr;                  // starting virtual address of the page 
  int ppage_index;                // maps a virtual page to a physical page (-> coremap)
  struct pagetable_entry *next;   // implement as linked list
} pagetable_entry_t; 

typedef struct pagetable {
  struct pagetable_entry *head;
  struct pagetable_entry *tail;
  int npages;
} pagetable_t;

#endif
