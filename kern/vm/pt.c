#include <pt.h>

pagetable_t* pagetable_init(void)
{
	pagetable_t *res = (pagetable_t*)kmalloc(sizeof(pagetable_t));
  	if(res == NULL)
    		panic("Couldn't allocate pagetable\n");
  	res -> head = NULL;
  	res -> tail = NULL;
  	res -> npages = 0;
	return res;
}

/* given a vaddr, search the pagetable for the corresponding entry.
 * @return -1 if not found, -2 if swapped,  
 * a positive integer if found (the index of the ppage in the coremap). */
int pagetable_search(struct pagetable *pt, vaddr_t vaddr) {
  struct pagetable_entry *pe;
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

/* add an entry to the pagetable, also allocates page in memory (dynamic allocation) */
int pagetable_add(struct pagetable *pt, vaddr_t vaddr) {
  paddr_t paddr;
  struct pagetable_entry *p;     
  int index = -1;
  paddr = getppages(1);   // get a new page, notice that you don't know where it will be in physical memory
  // TODO checks
  index = paddr / PAGE_SIZE;
  // create a new pte and update its fields 
  p = kmalloc(sizeof(struct pagetable_entry));
  p -> vaddr = vaddr & PAGE_FRAME;  // get starting vaddr of vpage
  p -> ppage_index = index;
  p -> next = pt -> head;
  pt -> head = p;
  return index;
}
