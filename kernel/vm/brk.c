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
#include "util/debug.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/mman.h"
#include "mm/tlb.h"
#include "vm/mmap.h"
#include "vm/vmmap.h"
#include "util/string.h"
#include "proc/proc.h"

/*
 * This function implements the brk(2) system call.
 *
 * This routine manages the calling process's "break" -- the ending address
 * of the process's "dynamic" region (often also referred to as the "heap").
 * The current value of a process's break is maintained in the 'p_brk' member
 * of the proc_t structure that represents the process in question.
 *
 * The 'p_brk' and 'p_start_brk' members of a proc_t struct are initialized
 * by the loader. 'p_start_brk' is subsequently never modified; it always
 * holds the initial value of the break. Note that the starting break is
 * not necessarily page aligned!
 *
 * 'p_start_brk' is the lower limit of 'p_brk' (that is, setting the break
 * to any value less than 'p_start_brk' should be disallowed).
 *
 * The upper limit of 'p_brk' is defined by the minimum of (1) the
 * starting address of the next occuring mapping or (2) USER_MEM_HIGH.
 * That is, growth of the process break is limited only in that it cannot
 * overlap with/expand into an existing mapping or beyond the region of
 * the address space allocated for use by userland. (note the presence of
 * the 'vmmap_is_range_empty' function).
 *
 * The dynamic region should always be represented by at most ONE vmarea.
 * Note that vmareas only have page granularity, you will need to take this
 * into account when deciding how to set the mappings if p_brk or p_start_brk
 * is not page aligned.
 *
 * You are guaranteed that the process data/bss region is non-empty.
 * That is, if the starting brk is not page-aligned, its page has
 * read/write permissions.
 *
 * If addr is NULL, you should NOT fail as the man page says. Instead,
 * "return" the current break. We use this to implement sbrk(0) without writing
 * a separate syscall. Look in user/libc/syscall.c if you're curious.
 *
 * Also, despite the statement on the manpage, you MUST support combined use
 * of brk and mmap in the same process.
 *
 * Note that this function "returns" the new break through the "ret" argument.
 * Return 0 on success, -errno on failure.
 */
int
do_brk(void *addr, void **ret)
{
        /*NOT_YET_IMPLEMENTED("VM: do_brk");*/
        dbg(DBG_PRINT,"go into do_brk\n");
		if(addr==NULL) 
		{
			dbg(DBG_PRINT,"(GRADING3B)\n");
			*ret=curproc->p_brk;
			return 0;
		}
		if(addr < curproc->p_start_brk)
		{
			dbg(DBG_PRINT,"(GRADING3C)\n");
			return -ENOMEM;
		}
		KASSERT((unsigned int)addr <= USER_MEM_HIGH);
		unsigned int old_brk = (unsigned int)curproc->p_brk;
		
		unsigned int new = ((unsigned int)addr - 1) / PAGE_SIZE + 1;
		unsigned int cur = ((unsigned int)curproc->p_brk - 1) / PAGE_SIZE + 1;


		if(new == cur)
		{
			dbg(DBG_PRINT,"(GRADING3C)\n");
			*ret = addr;
			curproc->p_brk =  addr;
			return 0;
		}

void * i;
	if(new < cur)
	{
			dbg(DBG_PRINT,"(GRADING3C)\n");
		*ret = addr;
		curproc->p_brk = addr;

		i = page_alloc_n( cur - new);
		memset(i,0, (cur* PAGE_SIZE - new* PAGE_SIZE) );
		vmmap_write(curproc->p_vmmap,PN_TO_ADDR(new),i,(uint32_t)old_brk-(uint32_t)PN_TO_ADDR(new));
		page_free_n(i, cur - new);
		vmmap_remove(curproc->p_vmmap, new ,cur - new);
		return 0;
	}

	KASSERT(new <= ADDR_TO_PN(USER_MEM_HIGH));
	if(vmmap_is_range_empty(curproc->p_vmmap , cur , new - cur)==NULL)
	{
			dbg(DBG_PRINT,"(GRADING3C)\n");
		return -ENOMEM;
	}
	vmarea_t *extend_area = NULL;

	KASSERT(extend_area = vmmap_lookup(curproc->p_vmmap,cur - 1));
	extend_area->vma_end = new;
	*ret = addr;
	curproc->p_brk = addr;

        return 0;
}
