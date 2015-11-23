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

#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
	  	dbg(DBG_PRINT,"go into do_mmap\n");
		file_t* file = NULL;

    	if(!PAGE_ALIGNED(off))
		{
			dbg(DBG_PRINT,"(GRADING3C)\n");
        	return -EINVAL;
		}

		if(len <= 0||len > 0xc0000000)
	    {
			dbg(DBG_PRINT,"(GRADING3C)\n");
	    	return -EINVAL;
	   	}

    	if (((uint32_t)addr < USER_MEM_LOW || (uint32_t)addr > USER_MEM_HIGH) && flags& MAP_FIXED)
		{      
			dbg(DBG_PRINT,"(GRADING3C)\n");  
			return -1;
		}

    if(!(flags & MAP_SHARED || flags & MAP_PRIVATE))
{			
	dbg(DBG_PRINT,"(GRADING3C)\n");
        return -EINVAL;
}
   	file = NULL;
    vnode_t *vn = NULL;
    int status = 0;
    uint32_t lopages = 0;
    size_t npages = (len - 1)/PAGE_SIZE  + 1;
    vmarea_t *newvma = NULL;
    if(flags & MAP_FIXED)
    {
			dbg(DBG_PRINT,"(GRADING3C)\n");
            lopages = ADDR_TO_PN( addr );
    }
    if(!(flags & MAP_ANON))
	{
			dbg(DBG_PRINT,"(GRADING3B)\n");
        if(fd < 0 || fd > NFILES)
		{
			dbg(DBG_PRINT,"(GRADING3C)\n");
            return -1;
        }
        file = fget(fd);
        if((prot & PROT_WRITE && MAP_SHARED & flags) && (file->f_mode == FMODE_READ))
		{
			dbg(DBG_PRINT,"(GRADING3C)\n");
            fput(file);
            return -1;
        }
        if(file == NULL) 
		{			
			dbg(DBG_PRINT,"(GRADING3C)\n");
			return -1;
		}
        vn = file->f_vnode;
    }




    status = vmmap_map(curproc->p_vmmap, vn, lopages, npages, prot, flags, off, VMMAP_DIR_HILO, &newvma);
    if(file != NULL) 
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
		fput(file);
	}
    if(newvma != NULL)
	{
			dbg(DBG_PRINT,"(GRADING3B)\n");
    *ret = PN_TO_ADDR(newvma->vma_start);
	}    
	if(status < 0)
	{
		dbg(DBG_PRINT,"(GRADING3C)\n");
		KASSERT(file == NULL);
        return status;
    }

    tlb_flush_all();
    KASSERT(curproc->p_pagedir != NULL);
    dbg(DBG_VM, "(GRADING3A 2.a)\n");
    return 0;

}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
        /*NOT_YET_IMPLEMENTED("VM: do_munmap");
        return -1;*/
	  	dbg(DBG_PRINT,"go into do_mumap\n");
		if((uint32_t)addr % PAGE_SIZE)
		{
			dbg(DBG_PRINT,"(GRADING3C)\n");
			return -EINVAL;
		}
		if(len <= 0||len > 0xc0000000)
	    {
			dbg(DBG_PRINT,"(GRADING3C)\n");
	    	return -EINVAL;
	   	}
		KASSERT(PAGE_ALIGNED(addr));
	  	if(((uint32_t)addr < USER_MEM_LOW) || ((uint32_t)addr > USER_MEM_HIGH) || ((uint32_t)addr+len > USER_MEM_HIGH))
	    {
			dbg(DBG_PRINT,"(GRADING3C)\n");
	    	return -EINVAL;
	    }

		vmmap_remove( curproc->p_vmmap, ADDR_TO_PN(addr), ((len - 1)/PAGE_SIZE  + 1));

		tlb_flush_all();

	  	KASSERT(NULL != curproc->p_pagedir);
	  	dbg(DBG_PRINT,"(GRADING3A 2.b)\n");

	  	return 0;
}

