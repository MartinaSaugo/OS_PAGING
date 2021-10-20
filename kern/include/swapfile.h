#ifndef _SWAPSPACE_H_
#define _SWAPSPACE_H_

#include <types.h>
#include <current.h> //perchè serve?
#include <vnode.h>
#include <addrspace.h>
#include <pt.h>

#define SWAPSIZE 9*1024*1024 //size of swap file
#define SWAPSLOTS SWAPSIZE/ PAGE_SIZE

//structs

/* status of the swap page */

/* keep all metadata about swap entry */
typedef struct swap_entry {
	struct addrspace *as;
	vaddr_t va;
	status_t status;
} swap_entry_t;


//funcs

void swap_init(void);
int write_page(int index, paddr_t page);
int read_page(int index, paddr_t page);
int swap_out(paddr_t page); 

/*int evict_page(struct page* page);
int swap_in (struct page* page); 
int swap_clean(struct addrspace *as, vaddr_t va);*/

#endif
