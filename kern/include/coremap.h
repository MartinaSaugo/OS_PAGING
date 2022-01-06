#ifndef _COREMAP_H_
#define _COREMAP_H_

typedef enum {FREE, CLEAN, DIRTY, FIXED} status_t;

typedef struct coremap_entry {
	status_t status; 
	paddr_t paddr;  
	vaddr_t vaddr;	// owner vaddr - pagetable entry
	long size;      //= previous allocsize, now all in one
} coremap_entry_t;

coremap_entry_t *coremap;
int nRamFrames;
int allocTableActive;
int firstFreeFrame;

coremap_entry_t *coremap_init(void);
int coremap_victim_selection(int nvictims);
void coremap_dealloc(void);

#endif
