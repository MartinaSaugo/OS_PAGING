#ifndef _COREMAP_H_
#define _COREMAP_H_
#include <machine/vm.h> //why?

typedef enum {FREE, CLEAN, DIRTY, FIXED} status_t;

typedef struct coremap_entry {
	status_t status; 
	paddr_t paddr;  
	long size;      //= previous allocsize, now all in one
} coremap_entry_t;

coremap_entry_t *freeRamFrames;

coremap_entry_t * coremap_init(void);


#endif
