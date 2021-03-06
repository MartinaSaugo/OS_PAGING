/*
 * AUthor: G.Cabodi
 * Very simple implementation of sys__exit.
 * It just avoids crash/panic. Full process exit still TODO
 * Address space is released
 */

#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>

/*
 * simple proc management system calls
 */
void
sys__exit(int status)
{
/*  
  //get address space of current process and destroy
  struct addrspace *as = proc_getas();
  as_destroy(as);
*/

//////////////////////////////////////////////////////////we made dis :)
  struct proc *p = curproc;
  p->p_status=status & 0xff;
  proc_remthread(curthread); 
  V(p->s_exit);

/* thread exits. proc data structure will be lost */
  thread_exit();

  panic("thread_exit returned (should not happen)\n");
  (void) status; // TODO: status handling
}

int sys_waitpid(pid_t pid, int* ret_status, int options)
{
	(void)options; //notused
	struct proc *p=proc_search_pid(pid);
	if(p==NULL) return -1;
	int s= proc_wait(p);
	if(ret_status!=NULL)
		*ret_status=s;
	return pid; 
}

int sys_getpid(void)
{
	KASSERT(curproc!=NULL);
	return curproc->p_pid;
}
