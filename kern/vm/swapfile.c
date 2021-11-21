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
static char swapfile[]="SWAPFILE";
// static struct lock *swaplock=NULL;
// static struct spinlock *swapspin; 

void swap_init (void){
	int i;
	int res = vfs_open(swapfile, O_RDWR|O_CREAT|O_TRUNC, 0, &swapspace); //0=mode, not supported
	if(res)
		// TODO handle in some way...
		panic("CAN'T OPEN SWAP FILE, ERROR: %d\n", res);
	
	//make the swap table empty
	for(i=0; i<SWAPSLOTS; i++)
	{
		swaptable[i].as=NULL;
		swaptable[i].status = FREE;
		//swaptable[i].va=NULL;	
	}

	/* 
		swaplock=lock_create("swaplock");
		swapspin=kmalloc(sizeof(struct spinlock*));
		spinlock_init(swapspin);
	*/

	return;
}

/*
 * where index is the index of a free page in the swap file
 * and page is the physical page to write in the swap file
 */
// TODO: when the swap file is full, return an error and free a page 
// anyway (it means that you must reload the page from the ELF)
int write_page(int index, paddr_t page){
	//struct thread *th=curthread; //serve?
	struct iovec iov;
	struct uio pageuio;
	// pos is the position inside the swap file in which we write 
	off_t pos = index * PAGE_SIZE; 
	
	int result = 0;
	(void) result;
	//MUST FIX! -> significa che ci serve l'indirizzo virtuale? In teoria ce l'abbiamo(?)
	page = PADDR_TO_KVADDR(page); 
	
	uio_kinit(&iov, &pageuio, (void *)page, PAGE_SIZE, pos, UIO_WRITE);
	result = VOP_WRITE(swapspace, &pageuio); 
	swaptable[index].status = DIRTY;
    // TODO if swap file is full return an error and free a swaptable page (or panic)
	return 0;
}

int read_page(int index, paddr_t page){
	struct iovec iov;
	struct uio pageuio;
	int result=0;
	//MUST FIX!
	page=PADDR_TO_KVADDR(page);
	off_t pos=index*PAGE_SIZE;
	uio_kinit(&iov, &pageuio, (void*)page, PAGE_SIZE, pos, UIO_READ);
	result=VOP_READ(swapspace, &pageuio);
	return result;
}	

/* select a free swapfile page 
 * and write on it the content of page */
int swap_out(paddr_t page){
	int i, index = -1, result = -1;
	for(i = 0; i < SWAPSLOTS; i++){
		if(swaptable[i].status == FREE){
			index = i;
			break;
		}
	}
	if(index == -1){
		// TODO manage
		panic("no more space in the swap file\n");
	}
	else {
		result = write_page(index, page);
	}
	return result;
} 


/*
	int evict_page(struct page* page); 
	int swap_in (struct page* page); 
	int swap_clean(struct addrspace *as, vaddr_t va);
*/
