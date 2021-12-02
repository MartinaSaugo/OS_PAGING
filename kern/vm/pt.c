#include <pt.h>
/* add this to check nRamFrames */
#include <coremap.h> 

pt_t* pt_init(void)
{
    pt_t *pt = (pt_t *) kmalloc(sizeof(pt_t));
    if(pt == NULL)
        panic("Couldn't allocate pagetable\n");
    pt -> nil = (ptentry_t *) kmalloc(sizeof(ptentry_t));
    pt -> nil -> next = pt -> nil;
    pt -> npages = 0;
    return pt;
}

/* given a vaddr, search the pagetable for the corresponding entry.
 * a positive integer if found (the index of the ppage in the coremap). */
ptentry_t *pt_search(pt_t *pt, vaddr_t vaddr) {
    ptentry_t *pte;
    for(pte = pt -> nil -> next; pte != pt -> nil; pte = pte -> next){
        /* check if the current entry corresponds to vaddr */
        if(vaddr >= (pte -> vaddr) && vaddr < (pte -> vaddr) + PAGE_SIZE){
            return pte;
        }
    }
    /* list is empty or entry is not found */
    return NULL;
}

/* add a new entry to the pagetable, associated 
 * also allocate page in memory (dynamic allocation) - 
 * this function is called only when you're sure there's 
 * enough space 
 * */
int pt_add(pt_t *pt, paddr_t paddr, vaddr_t vaddr) {
	int ppindex = -1;
	ptentry_t *new;
	// TODO checks: the paddr may be invalid
	ppindex = paddr / PAGE_SIZE;
	// create the new ptentry 
	new = (ptentry_t *) kmalloc(sizeof(ptentry_t));
	KASSERT(new != NULL);
	new -> vaddr = vaddr & PAGE_FRAME;
	new -> ppage_index = ppindex;
	new -> status = PRESENT;
	// update also the coremap (TODO: synchronize)
	freeRamFrames[ppindex].vaddr = vaddr & PAGE_FRAME;
	// attach the node to the rest of the list 
	new -> next = pt -> nil -> next;
	// put new entry as first element of the list
	pt -> nil -> next = new;
	return ppindex;
}

/* implement a round-robin-like victim selection algorithm */
ptentry_t *pt_select_victim(pt_t *pt){
	static ptentry_t *next_victim = NULL;
	/* pagetable is empty */
	if(pt -> nil -> next == pt -> nil)
		return NULL;
	/* if first time, start from the beginning */
	if(next_victim == NULL)
		next_victim = pt -> nil -> next;  
	/* if at the end restart from the top */
	if(next_victim == pt -> nil){ 
		next_victim = next_victim -> next;
	}
	/* copy the victim */
	ptentry_t *victim = next_victim; 
	next_victim = next_victim -> next;
	return victim;
}

