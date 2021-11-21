#ifndef _COREMAP_H_
#define _COREMAP_H_
#include <machine/vm.h> //why?
#include <spinlock.h>

typedef enum {FREE, CLEAN, DIRTY, FIXED} status_t;

typedef struct coremap_entry {
	status_t status; 
	paddr_t paddr;  
	long size;      //= previous allocsize, now all in one
} coremap_entry_t;

coremap_entry_t *freeRamFrames;
int nRamFrames;

coremap_entry_t * coremap_init(void);
paddr_t getfreeppages(unsigned long npages);
int getfreeppage(paddr_t *paddr);
int isTableActive (void);
paddr_t getppages(unsigned long npages);
int getppage(paddr_t *);
int freeppages(paddr_t addr, unsigned long npages);
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);

#endif
