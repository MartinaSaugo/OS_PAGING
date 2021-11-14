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
    ptentry_t *pe;
    if(pt -> head == NULL)  // list is empty
        return -1;
    for(pe = pt -> head; pe != NULL; pe = pe -> next){
    if(vaddr >= pe -> vaddr && vaddr < pe -> vaddr + PAGE_SIZE){
        if(pe -> status == SWAPPED)
            return -2;
        return pe -> ppage_index;
    }
  }
  return -1;
}

/* add an entry to the pagetable, also allocates page in memory (dynamic allocation) - called only when you're sure there's enough space */
int pt_add(pt_t *pt, vaddr_t vaddr) {
    paddr_t paddr;
    ptentry_t *p;     
    int index = -1;
    paddr = getppages(1);   // get a new page, notice that you don't know where it will be in physical memory
    // TODO checks
    index = paddr / PAGE_SIZE;
    // create a new pte and update its fields 
    p = kmalloc(sizeof(ptentry_t));
    p -> vaddr = vaddr & PAGE_FRAME;  // get starting vaddr of vpage
    p -> ppage_index = index;
    p -> next = pt -> head;
    p -> status = PRESENT;
    pt -> head = p;
    return index;
}

/* implement a round-robin-like victim selection algorithm 
 * (if at the end, start from head) */
ptentry_t *pt_select_victim(pt_t *pt){
    static ptentry_t *next_victim = NULL;
    if(next_victim == NULL){
        next_victim = pt -> head;
    }
    ptentry_t *victim = next_victim; 
    next_victim = next_victim -> next;
    return victim;
}

