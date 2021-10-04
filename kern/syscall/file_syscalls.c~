/*
 * AUthor: G.Cabodi
 * Very simple implementation of sys_read and sys_write.
 * just works (partially) on stdin/stdout
 */
#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <limits.h>
#include <vfs.h>
#include <vnode.h>
#include <proc.h>
#include <uio.h>
#include <current.h>

#define SYSTEM_OPEN_MAX (10*OPEN_MAX)

struct openfile {
	struct vnode *vn;
	unsigned int refCount;
	off_t offset;
};

struct openfile SYSTEMFileTable[SYSTEM_OPEN_MAX];

int sys_open (userptr_t path, int openflags, mode_t mode, int *err)
{
	int res, i;
	struct vnode *v;
	struct openfile *of=NULL;
	res= vfs_open((char *)path, openflags, mode, &v);
	if (res)
	{
		*err=ENOENT;
		return -1;
	}
	for (i=0; i<SYSTEM_OPEN_MAX; i++)
	{
		if(SYSTEMFileTable[i].vn==NULL)
		{
			of=&SYSTEMFileTable[i];
			of->vn=v;
			of->refCount=1;
			of->offset=0;
			break;
		}
	}
	if(of==NULL)
		*err=ENFILE;
	else
	{
		for(i=STDERR_FILENO+1; i<OPEN_MAX; i++)
		{
			if(curproc->fileTable[i]==NULL)
			{
				curproc->fileTable[i]=of;
				return i;
			}
		}
		*err=EMFILE;			
	}
	vfs_close(v);
	return -1;
}

int sys_close (int i)
{
	struct vnode *v;
	struct openfile *of;	
	if(i<0||i>OPEN_MAX)
		return -1;
	of=curproc->fileTable[i];
	if(of==NULL)
		return -1;
	v=of->vn;
	of->vn=NULL;
	if(v==NULL)
		return -1;	
	vfs_close(v);
	curproc->fileTable[i]=NULL;
	return 0;		
}

/*
 * simple file system calls for write/read
 */
int
sys_write(int fd, userptr_t buf_ptr, size_t size)
{
  int i;
  char *p = (char *)buf_ptr;

  if (fd!=STDOUT_FILENO && fd!=STDERR_FILENO) {
    return file_write(fd, buf_ptr, size);
  }

  for (i=0; i<(int)size; i++) {
    putch(p[i]);
  }

  return (int)size;
}

int
sys_read(int fd, userptr_t buf_ptr, size_t size)
{
  int i;
  char *p = (char *)buf_ptr;

  if (fd!=STDIN_FILENO) {
    return file_read(fd,buf_ptr,size);
  }

  for (i=0; i<(int)size; i++) {
    p[i] = getch();
    if (p[i] < 0) 
      return i;
  }

  return (int)size;
}

int file_read(int fd, userptr_t buf, size_t size){ //static?
	struct iovec iov; 
	struct uio u;
	struct openfile *of;
	struct vnode *vn;
	int result;	

	if(fd<0||fd>OPEN_MAX) return -1;
	of=curproc->fileTable[fd];
	if(of==NULL) return -1;
	vn=of->vn;
	if(vn==NULL) return -1;

	iov.iov_ubase=buf;
	iov.iov_len=size;
	
	u.uio_iov = &iov;
	u.uio_iovcnt=1;
	u.uio_resid=size;
	u.uio_offset = of->offset;
	u.uio_segflg = UIO_USERSPACE; //UIO_USERISPACE?
	u.uio_rw= UIO_READ;
	u.uio_space=curproc->p_addrspace;

	result=VOP_READ(vn, &u);
	if(result) return result;
	of->offset=u.uio_offset;
	return (size-u.uio_resid);	
}

int file_write(int fd, userptr_t buf, size_t size) //static?
{
	struct iovec iov;
	struct uio u;
	struct openfile *of;
	struct vnode *vn;
	int result;

	if(fd<0||fd>OPEN_MAX) return -1;
	of=curproc->fileTable[fd];
	if(of==NULL) return -1;
	vn=of->vn;
	if(vn==NULL) return -1;

	iov.iov_ubase=buf;
	iov.iov_len=size;
	
	u.uio_resid=size;
	u.uio_iov=&iov;
	u.uio_iovcnt=1;
	u.uio_offset=of->offset;
	u.uio_segflg=UIO_USERSPACE; //UIO_USERISPACE?
	u.uio_rw=UIO_WRITE;
	u.uio_space=curproc->p_addrspace;

	result=VOP_WRITE(vn, &u);
	if(result) return result;
	of->offset=u.uio_offset;
	return (size-u.uio_resid);
}
