#ifndef _COREMAP_H_
#define _COREMAP_H_
#include <machine/vm.h> //why?

typedef enum {FREE, CLEAN, DIRTY, FIXED} status_t;

typedef struct coremap_entry {
	status_t status; 
	paddr_t paddr; //where?
	//potential other info for paging
}coremap_entry_t;

#endif
