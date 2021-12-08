#ifndef _SWAPSPACE_H_
#define _SWAPSPACE_H_

#include <types.h>
#include <current.h> //perchè serve?
#include <vnode.h>
#include <addrspace.h>
#include <pt.h>

#define SWAPSIZE 9*1024*1024 //size of swap file
#define SWAPSLOTS SWAPSIZE/PAGE_SIZE

/* status of the swap page */

/* keep all metadata about swap entry */
typedef struct swap_entry {
	// struct addrspace *as;
	char empty; // 1 if empty, 0 if full
} swap_entry_t;


void swap_init(void);
int write_page(int index, paddr_t page);
int read_page(int index, paddr_t page);
int swap_out(paddr_t page); 
int swap_in(int index, paddr_t paddr);

//  int swap_clean(struct addrspace *as, vaddr_t va);

#endif
