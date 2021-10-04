#include <swapfile.h>
#include <types.h>
#include <addrspace.h> 
#include <vnode.h>
#include <lib.h> 
#include <vfs.h> 
#include <vm.h> 
#include <uio.h> 
#include <cpu.h> //?
#include <elf.h> //?
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
	//CAN I DO IT W/ SYS_OPEN?
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


