/******************************************************************************/
/* Important Fall 2014 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/******************************************************************************/

/*
 *  FILE: open.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Mon Apr  6 19:27:49 1998
 */

#include "globals.h"
#include "errno.h"
#include "fs/fcntl.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/stat.h"
#include "util/debug.h"

/* find empty index in p->p_files[] */
int
get_empty_fd(proc_t *p)
{
        int fd;

        for (fd = 0; fd < NFILES; fd++) {
                if (!p->p_files[fd])
                        return fd;
        }

        dbg(DBG_ERROR | DBG_VFS, "ERROR: get_empty_fd: out of file descriptors "
            "for pid %d\n", curproc->p_pid);
        return -EMFILE;
}

/*
 * There a number of steps to opening a file:
 *      1. Get the next empty file descriptor.
 *      2. Call fget to get a fresh file_t.
 *      3. Save the file_t in curproc's file descriptor table.
 *      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
 *         oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
 *         O_APPEND or O_CREAT.
 *      5. Use open_namev() to get the vnode for the file_t.
 *      6. Fill in the fields of the file_t.
 *      7. Return new fd.
 *
 * If anything goes wrong at any point (specifically if the call to open_namev
 * fails), be sure to remove the fd from curproc, fput the file_t and return an
 * error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        oflags is not valid.
 *      o EMFILE
 *        The process already has the maximum number of files open.
 *      o ENOMEM
 *        Insufficient kernel memory was available.
 *      o ENAMETOOLONG
 *        A component of filename was too long.
 *      o ENOENT
 *        O_CREAT is not set and the named file does not exist.  Or, a
 *        directory component in pathname does not exist.
 *      o EISDIR
 *        pathname refers to a directory and the access requested involved
 *        writing (that is, O_WRONLY or O_RDWR is set).
 *      o ENXIO
 *        pathname refers to a device special file and no corresponding device
 *        exists.
 */
static
int check_name_len(const char* path){
    dbg(DBG_PRINT,"(GRADING2B)\n");
	  const char* head = path;
    
	  while( 1){
          while(*head == '/' && *head != '\0'){
              head += 1;
          }
          if(*head == '\0'){
              break;
          }
          
          int count = 0;
          while(*head != '/' && *head != '\0'){
              head += 1;
              count++;
          }
          if(count > NAME_LEN){
              return -ENAMETOOLONG;
          }
          if(*head == '\0'){
              break;
          }
	  }
	  
	  return 0;
}

int
do_open(const char *filename, int oflags)
{
	KASSERT(filename !=NULL && strlen(filename)>=1);

	if ( strlen(filename) > MAXPATHLEN ||check_name_len(filename) != 0 )
	{
		dbg(DBG_PRINT,"(GRADING2B)\n");
		
		return -ENAMETOOLONG;
	}

    dbg(DBG_PRINT,"(GRADING2B)\n");
    file_t *systemFile;
	int ret;
	vnode_t *res_vnode;

	if ((oflags & (O_WRONLY | O_RDWR)) == (O_WRONLY | O_RDWR))
	{
        dbg(DBG_PRINT,"(GRADING2B)\n");
		return -EINVAL;
	}
KASSERT(!(oflags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND)));

	int fd = get_empty_fd(curproc);
KASSERT(fd>=0);

	systemFile = fget(-1);
KASSERT(systemFile!=NULL);
	curproc->p_files[fd] = systemFile;
    if(!(oflags & O_WRONLY))
    {
        dbg(DBG_PRINT,"(GRADING2B)\n");
        curproc->p_files[fd]->f_mode |=FMODE_READ;
    }
    if(oflags & (O_WRONLY | O_RDWR))
    {
        dbg(DBG_PRINT,"(GRADING2B)\n");
        curproc->p_files[fd]->f_mode |=FMODE_WRITE;
    }
    if(oflags & O_APPEND)
    {
        dbg(DBG_PRINT,"(GRADING2B)\n");
        curproc->p_files[fd]->f_mode |=FMODE_APPEND;
    }

	ret = open_namev(filename, oflags, &res_vnode, NULL);
	if (ret < 0) 
	{
        dbg(DBG_PRINT,"(GRADING2B)\n");
		curproc->p_files[fd] = NULL;
		fput(systemFile);
		return ret;
	}

	if (S_ISDIR(res_vnode->vn_mode) && (oflags & (O_WRONLY | O_RDWR))) {
        dbg(DBG_PRINT,"(GRADING2B)\n");
		curproc->p_files[fd] = NULL;
		fput(systemFile);
		vput(res_vnode);
		return -EISDIR;
	}
KASSERT(!((S_ISBLK(res_vnode->vn_mode) && !res_vnode->vn_bdev)));
KASSERT(!((S_ISCHR(res_vnode->vn_mode) && !res_vnode->vn_cdev)));
    
    dbg(DBG_PRINT,"(GRADING2B)\n");
	systemFile->f_vnode = res_vnode;
	return fd;
/*

*/


}
