#ifndef _COREMAP_H_
#define _COREMAP_H_
#include <machine/vm.h> //why?


typedef enum {FREE, CLEAN, DIRTY, FIXED} status_t;

typedef struct coremap_entry {
	status_t status; 
	paddr_t paddr; //where?
	long size; //= previous allocsize, now all in one
	//potential other info for paging
	int next; //next part of contiguous memory
}coremap_entry_t;

/*declare
//extern coremap_entry_t *freeRamFrames;
coremap_entry_t *freeRamFrames;
long nRamFrames;
int allocTableActive = 0;*/

#endif
