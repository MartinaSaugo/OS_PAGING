#include <swapfile.h>
#include <types.h>
#include <addrspace.h> 
#include <vnode.h>
#include <lib.h> 
#include <vfs.h> 
#include <vm.h> 
#include <uio.h> 
#include <cpu.h>
#include <elf.h> 
#include <spl.h>  
#include <synch.h>

#include <kern/errno.h>
#include <kern/fcntl.h>

#include <coremap.h>
#include <pt.h>


static swap_entry_t swaptable[SWAPSLOTS];
static struct vnode *swapspace;
static char swapfile[] = "SWAPFILE";
// static struct lock *swaplock = NULL;
// static struct spinlock *swapspin; 

void swap_init (void){
	int i;
	int res = vfs_open(swapfile, O_RDWR|O_CREAT|O_TRUNC, 0, &swapspace); //0 = mode, not supported
	if(res)
		// TODO handle in some way...
		panic("CAN'T OPEN SWAP FILE, ERROR: %d\n", res);
	
	//make the swap table empty
	for(i = 0; i<SWAPSLOTS; i++)
	{
		// swaptable[i].as = NULL;
		swaptable[i].empty = 1;
	}

	/* 
		swaplock = lock_create("swaplock");
		swapspin = kmalloc(sizeof(struct spinlock*));
		spinlock_init(swapspin);
	*/

	return;
}

/*
 * where index is the index of a free page in the swap file
 * and page is the physical page to write in the swap file
 */
// anyway (it means that you must reload the page from the ELF)
int write_page(int index, paddr_t page){
	//struct thread *th = curthread; //serve?
	struct iovec iov;
	struct uio pageuio;
	// pos is the position inside the swap file in which we write 
	off_t pos = index * PAGE_SIZE; 
	int err = 0;
	(void) err;
	page = PADDR_TO_KVADDR(page); 
	uio_kinit(&iov, &pageuio, (void *)page, PAGE_SIZE, pos, UIO_WRITE);
	err = VOP_WRITE(swapspace, &pageuio); 
	return err;
}

int read_page(int index, paddr_t page){
	struct iovec iov;
	struct uio pageuio;
	int err = 0;
	page = PADDR_TO_KVADDR(page);
	off_t pos = index * PAGE_SIZE;
	uio_kinit(&iov, &pageuio, (void*)page, PAGE_SIZE, pos, UIO_READ);
	err = VOP_READ(swapspace, &pageuio);
	return err;
}

/* select a free swapfile page 
 * and write on it the content of page 
 * returns the index of the page in the swapfile
 * */
int swap_out(paddr_t page){
	int i, index = -1, err = -1;
	for(i = 0; i < SWAPSLOTS; i++){
		if(swaptable[i].empty == 1){
			index = i;
			break;
		}
	}
	if(index == -1){
		// TODO manage
		panic("no more space in the swap file\n");
	}
	else {
		err = write_page(index, page);
		swaptable[index].empty = 0;
		// kprintf("swapout %d: %x\n", index, *((int *) (page + 10) ));
	}
	if(err)
		return -1;
	return index;
} 

/*
 * given an index, load the page from disk 
 * and update the swaptable entry
 */
int swap_in(int index, paddr_t paddr){
	int err;
	err = read_page(index, paddr);
	if(err)
		return -1;
	swaptable[index].empty = 1; // swap entry becomes empty
	return 0;
}

void swap_unmark(int index){
	// check that page is swapped
	KASSERT(swaptable[index].empty == 0);
	swaptable[index].empty = 1;
}

/*
	int evict_page(struct page* page); 
	int swap_clean(struct addrspace *as, vaddr_t va);
*/
