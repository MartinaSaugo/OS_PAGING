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

static swap_entry_t swaptable[SWAPSLOTS];
static struct vnode *swapspace;
static char swapfile[]="SWAPFILE";
static struct lock *swaplock=NULL;
static struct spinlock *swapspin; 

void swap_init (void)
{
	int i;
	int res = vfs_open(swapfile, O_RDWR|O_CREAT|O_TRUNC, 0, &swapspace); //0=mode, not supported
	if(res)
		panic("CAN'T OPEN SWAP FILE, ERROR: %d\n", res);
	
	//make the swap table empty
	for(i=0; i<SWAPSLOTS; i++)
	{
		swaptable[i].as=NULL;
		//swaptable[i].va=NULL;	
	}
	swaplock=lock_create("swaplock");
	swapspin=kmalloc(sizeof(struct spinlock*));
	spinlock_init(swapspin);
	return;
}

int write_page(int index, paddr_t page)
{
	//struct thread *th=curthread; //serve?
	struct iovec iov;
	struct uio pageuio;
	
	int result=0;
	//MUST FIX! -> significa che ci serve l'indirizzo virtuale? In teoria ce l'abbiamo(?)
	page=PADDR_TO_KVADDR(page);
	off_t pos= index*PAGE_SIZE; 
	
	uio_kinit(&iov, &pageuio, (void*)page, PAGE_SIZE, pos, UIO_WRITE);
	result=VOP_WRITE(swapspace, &pageuio);
	return result;
}

int read_page(int index, paddr_t page)
{
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

/*int evict_page(struct page* page); 
int swap_out(struct page* page);
int swap_in (struct page* page); 
int swap_clean(struct addrspace *as, vaddr_t va);*/
