/******************************************************************************/
/* Important Fall 2014 CSCI 402 usage information:							*/
/*																			*/
/* This fils is part of CSCI 402 kernel programming assignments at USC.	   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*		 display a copy of this file (or ANY PART of it) for any reason.	*/
/* If anyone (including your prospective employer) asks you to post the code,*/
/*		 you must inform them that you do NOT have permissions to do so.	*/
/* You are also NOT permitted to remove or alter this comment block.		  */
/******************************************************************************/

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs,void *kstack)
{
		/* Pointer argument and dummy return address,and userland dummy return
		 * address */
		uint32_t esp=((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
		*(void **)(esp + 4)=(void *)(esp + 8); /* Set the argument to point to location of struct on stack */
		memcpy((void *)(esp + 8),regs,sizeof(regs_t)); /* Copy over struct */
		return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{
        dbg(DBG_PRINT,"go into do_fork\n");
	KASSERT(regs!=NULL);
	dbg(DBG_PRINT,"(GRADING3A 7.a)\n" );
	KASSERT(curproc!=NULL);
	dbg( DBG_PRINT,"(GRADING3A 7.a)\n" );
	KASSERT(curproc->p_state==PROC_RUNNING);
	dbg( DBG_PRINT,"(GRADING3A 7.a)\n" );
	proc_t *ret=proc_create("child");
	KASSERT(ret->p_state==PROC_RUNNING);
	dbg( DBG_PRINT,"(GRADING3A 7.a)\n" );
	KASSERT(ret->p_pagedir!=NULL);
	dbg( DBG_PRINT,"(GRADING3A 7.a)\n" );

	int linyin = 0;
    struct file *f = NULL;
	vmarea_t *i,*prev;
	ret->p_brk=curproc->p_brk;
	ret->p_start_brk=curproc->p_start_brk;
	ret->p_vmmap=vmmap_clone (curproc->p_vmmap);
	ret->p_vmmap->vmm_proc=ret;

	list_iterate_begin(&(ret->p_vmmap -> vmm_list),i,vmarea_t,vma_plink)
	{

		prev=vmmap_lookup(curproc->p_vmmap, i->vma_start);
		if (prev->vma_flags &  MAP_SHARED)
		{
			dbg(DBG_PRINT,"(GRADING3C)\n" );
			i->vma_obj=prev->vma_obj;
			prev->vma_obj ->mmo_ops->ref(i->vma_obj );
			list_insert_tail(&(i->vma_obj->mmo_un.mmo_vmas),&(i -> vma_olink));
		}
		
		if (prev->vma_flags & MAP_PRIVATE)
		{
			dbg(DBG_PRINT,"(GRADING3B)\n" );
			mmobj_t *new1=shadow_create(),*new2=shadow_create(),*old=prev->vma_obj;
                KASSERT(new1!= NULL);
                KASSERT(new2!= NULL);
			new1 ->mmo_un.mmo_bottom_obj=old->mmo_un.mmo_bottom_obj;
			new2->mmo_un.mmo_bottom_obj=old->mmo_un.mmo_bottom_obj;
			prev->vma_obj=new1;
			i->vma_obj=new2;
			
			new1 ->mmo_shadowed= old;
			new2->mmo_shadowed= old;

			old->mmo_ops->ref(old);
			list_insert_tail(&(new2->mmo_un.mmo_bottom_obj->mmo_un.mmo_vmas),&(i -> vma_olink));
	   }

	} list_iterate_end();

	pt_unmap_range(curproc->p_pagedir,USER_MEM_LOW,USER_MEM_HIGH);
	pt_unmap_range(ret->p_pagedir,USER_MEM_LOW,USER_MEM_HIGH);

	kthread_t *newthr=kthread_clone(curthr);
	KASSERT(newthr->kt_kstack!=NULL);
	dbg(DBG_PRINT,"(GRADING3A 7.a)\n" );

	regs->r_eax=0;
	newthr->kt_ctx.c_eip=(uintptr_t) userland_entry;
	newthr->kt_ctx.c_ebp=newthr->kt_ctx.c_esp;
	newthr->kt_ctx.c_pdptr=ret->p_pagedir;
	newthr->kt_ctx.c_esp=(uintptr_t) fork_setup_stack(regs,(void *)newthr->kt_ctx.c_kstack);
	list_insert_tail(&(ret->p_threads),&(newthr->kt_plink));
    for (linyin = 0; linyin < NFILES; linyin++) 
	{
        	f = curproc->p_files[linyin];
        	if(f != NULL)
			{
				dbg(DBG_PRINT,"(GRADING3B)\n" );
        		ret->p_files[linyin] = f;
        		fref(f);
        	}
	}

	sched_make_runnable(newthr);
	newthr->kt_proc=ret;
	regs->r_eax=ret->p_pid;


	tlb_flush_all();
	return ret->p_pid;
}
