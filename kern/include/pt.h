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

pagetable_t* pagetable_init(void);
int pagetable_search(struct pagetable *, vaddr_t);
int pagetable_add(struct pagetable *, vaddr_t);

#endif
