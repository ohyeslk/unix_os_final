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

#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/proc.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/pagetable.h"

#include "vm/pagefault.h"
#include "vm/vmmap.h"
#include "api/access.h"

/*
 * This gets called by _pt_fault_handler in mm/pagetable.c The
 * calling function has already done a lot of error checking for
 * us. In particular it has checked that we are not page faulting
 * while in kernel mode. Make sure you understand why an
 * unexpected page fault in kernel mode is bad in Weenix. You
 * should probably read the _pt_fault_handler function to get a
 * sense of what it is doing.
 *
 * Before you can do anything you need to find the vmarea that
 * contains the address that was faulted on. Make sure to check
 * the permissions on the area to see if the process has
 * permission to do [cause]. If either of these checks does not
 * pass kill the offending process, setting its exit status to
 * EFAULT (normally we would send the SIGSEGV signal, however
 * Weenix does not support signals).
 *
 * Now it is time to find the correct page (don't forget
 * about shadow objects, especially copy-on-write magic!). Make
 * sure that if the user writes to the page it will be handled
 * correctly.
 *
 * Finally call pt_map to have the new mapping placed into the
 * appropriate page table.
 *
 * @param vaddr the address that was accessed to cause the fault
 *
 * @param cause this is the type of operation on the memory
 *              address which caused the fault, possible values
 *              can be found in pagefault.h
 */
void
handle_pagefault(uintptr_t vaddr, uint32_t cause)
{
        /*NOT_YET_IMPLEMENTED("VM: handle_pagefault");*/
        dbg(DBG_PRINT,"go into handle_pagefault\n");
	uint32_t vfn = ADDR_TO_PN(vaddr);
    int perm = 0;
    vmarea_t* targetArea = vmmap_lookup(curproc->p_vmmap, vfn);
    int write1 = 0;
    int linyin;
    uint32_t pagenum;
    mmobj_t* targetObject = NULL;
    pframe_t* targetPframe = NULL;

    perm |= PROT_READ;

    if(!(ADDR_TO_PN(USER_MEM_LOW) <= vfn && vfn <= ADDR_TO_PN(USER_MEM_HIGH)))
	{
		dbg(DBG_PRINT,"(GRADING3C)\n");
    	proc_kill(curproc, EFAULT);
    }

    if(targetArea == NULL)
	{
		dbg(DBG_PRINT,"(GRADING3C)\n");
        proc_kill(curproc, EFAULT);
    }
	KASSERT(!(cause & FAULT_EXEC));
    if(cause & FAULT_WRITE)
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
        perm |= PROT_WRITE;
        write1 = 1;
    }



	pagenum = ADDR_TO_PN(vaddr) - targetArea->vma_start + targetArea->vma_off;
    if(addr_perm(curproc, (void*)vaddr, perm) == 0)
	{
		dbg(DBG_PRINT,"(GRADING3C)\n");
        proc_kill(curproc, EFAULT);
    }
    targetObject = targetArea->vma_obj;

	linyin = pframe_lookup(targetObject, pagenum, write1, &targetPframe);

	if(linyin!=0)
	{
		dbg(DBG_PRINT,"(GRADING3C)\n");
        proc_kill(curproc, EFAULT);
    }

	if(write1)
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");    
		pframe_dirty(targetPframe);
	}    
    pframe_pin(targetPframe);

    int pdwirte = (write1 == 1) ? PD_WRITE : 0;
   	uintptr_t paddr = pt_virt_to_phys((uintptr_t)targetPframe->pf_addr);
    uint32_t flag =  PT_PRESENT|pdwirte|PT_USER; 

    pt_map(curproc->p_pagedir, (uintptr_t)PAGE_ALIGN_DOWN(vaddr), (uintptr_t)PAGE_ALIGN_DOWN(paddr), flag, flag);
    pframe_unpin(targetPframe);
}
