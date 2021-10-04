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

typedef struct swap_entry {
	struct addrspace *as;
	vaddr_t va;
}swap_entry_t;


//funcs

void swap_init(void);

#endif
