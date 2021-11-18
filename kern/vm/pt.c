#include <pt.h>

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
 * @return -1 if not found, -2 if swapped,  
 * a positive integer if found (the index of the ppage in the coremap). */
int pt_search(pt_t *pt, vaddr_t vaddr) {
    ptentry_t *pte;
    for(pte = pt -> nil -> next; pte != pt -> nil; pte = pte -> next){
        /* check if the current entry corresponds to vaddr */
        if(vaddr >= (pte -> vaddr) && vaddr < (pte -> vaddr) + PAGE_SIZE){
            if(pte -> status == SWAPPED)
                return -2;
            return pte -> ppage_index;
        }
    }
    /* list is empty or the entry is not found */
    return -1;
}

/* add an entry to the pagetable, 
 * also allocate page in memory (dynamic allocation) - 
 * this function is called only when you're sure there's 
 * enough space 
 * */
int pt_add(pt_t *pt, vaddr_t vaddr) {
    paddr_t paddr;
    ptentry_t *new;
    int index = -1;
    // get a new page, notice that you don't know where it will be in physical memory
    paddr = getppages(1);   
    // TODO checks
    index = paddr / PAGE_SIZE;
    /* create the new entry */
    new = (ptentry_t *) kmalloc(sizeof(ptentry_t));
    new -> index = index;
    new -> vaddr = vaddr & PAGE_FRAME;
    new -> status = PRESENT;
    /* attach the node to the rest of the list */
    new -> next = pt -> nil -> next;
    /* put new entry as first element of the list */
    pt -> nil -> next = new;
    return index;
}

/* implement a round-robin-like victim selection algorithm */
ptentry_t *pt_select_victim(pt_t *pt){
    /* pagetable is empty */
    if(pt -> nil -> next == pt -> nil)
        return NULL;
    static ptentry_t *next_victim = NULL;
    next_victim = pt -> nil;
    /* restart from the top */
    if(next_victim == pt -> nil){ 
        next_victim = next_victim -> next;
    }
    /* copy the victim */
    ptentry_t *victim = next_victim; 
    next_victim = next_victim -> next;
    return victim;
}

