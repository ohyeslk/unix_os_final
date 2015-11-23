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

#include "kernel.h"
#include "config.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "proc/kthread.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/proc.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/mm.h"
#include "mm/mman.h"

#include "vm/vmmap.h"

#include "fs/vfs.h"
#include "fs/vfs_syscall.h"
#include "fs/vnode.h"
#include "fs/file.h"

proc_t *curproc = NULL; /* global */
static slab_allocator_t *proc_allocator = NULL;

static list_t _proc_list;
static proc_t *proc_initproc = NULL; /* Pointer to the init process (PID 1) */

void
proc_init()
{
    list_init(&_proc_list);
    proc_allocator = slab_allocator_create("proc", sizeof(proc_t));
    KASSERT(proc_allocator != NULL);
}

proc_t *
proc_lookup(int pid)
{
    proc_t *p;
    list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
        if (p->p_pid == pid) {
            return p;
        }
    } list_iterate_end();
    return NULL;
}

list_t *
proc_list()
{
    return &_proc_list;
}

static pid_t next_pid = 0;

/**
 * Returns the next available PID.
 *
 * Note: Where n is the number of running processes, this algorithm is
 * worst case O(n^2). As long as PIDs never wrap around it is O(n).
 *
 * @return the next available PID
 */
static int
_proc_getid()
{
    proc_t *p;
    pid_t pid = next_pid;
    while (1) {
    failed:
        list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
            if (p->p_pid == pid) {
                if ((pid = (pid + 1) % PROC_MAX_COUNT) == next_pid) {
                    return -1;
                } else {
                    goto failed;
                }
            }
        } list_iterate_end();
        next_pid = (pid + 1) % PROC_MAX_COUNT;
        return pid;
    }
}

/*
 * The new process, although it isn't really running since it has no
 * threads, should be in the PROC_RUNNING state.
 *
 * Don't forget to set proc_initproc when you create the init
 * process. You will need to be able to reference the init process
 * when reparenting processes to the init process.
 */
proc_t *
proc_create(char *name)
{
	proc_t *new_proc;
	new_proc=(proc_t *)slab_obj_alloc(proc_allocator);
	KASSERT(new_proc != NULL);
	if (new_proc!=NULL)
	{
		strcpy(new_proc->p_comm, name);
		new_proc->p_pid = _proc_getid();
		new_proc->p_pproc = curproc;
		dbg(DBG_PRINT,"(GRADING1B) Created process is valid\n");
	}
	KASSERT(PID_IDLE != new_proc->p_pid || list_empty(&_proc_list)); /* pid can only be PID_IDLE if this is the first process (middle)*/
    dbg(DBG_PRINT, "(GRADING1A 2.a) pid can only be PID_IDLE if this is the first process\n");
    KASSERT(PID_INIT != new_proc->p_pid || PID_IDLE == curproc->p_pid); /* pid can only be PID_INIT when creating from idle process (middle)*/
    dbg(DBG_PRINT, "(GRADING1A 2.a) pid can only be PID_INIT when creating from idle process\n");
    /*set proc_initproc when you create the init process*/
    if(new_proc->p_pid==PID_INIT)
    {
        proc_initproc=new_proc;
        dbg(DBG_PRINT,"(GRADING1B) Setting proc_initproc when creating the init process.\n");
    }

	new_proc->p_status=0;
	new_proc->p_state=PROC_RUNNING;
    /*add to process*/
	sched_queue_init(&new_proc->p_wait);
	new_proc->p_pagedir = pt_create_pagedir();	
    /*Initialization*/

		list_init(&(new_proc->p_threads));
    	list_init(&(new_proc->p_children));
    	list_link_init(&new_proc->p_list_link);
    	list_link_init(&new_proc->p_child_link);
		list_insert_tail(&_proc_list,&(new_proc->p_list_link));	
    /*link all processes*/
    if (new_proc->p_pproc!=NULL)
    {
        list_insert_tail(&(new_proc->p_pproc->p_children),&(new_proc->p_child_link));
        dbg(DBG_PRINT,"(GRADING1B) If created process is not the first one, insert it to parent process list \n");
        
    }

        /*VFS: TO_DO*/
		/*code for VFS*/
        	/*new_proc->p_files = {NULL};*/
        	/*new_proc->p_cwd = NULL;*/
		memset(new_proc->p_files,NULL,sizeof(new_proc->p_files));
		if(curproc != NULL)
			new_proc->p_cwd = curproc->p_cwd;
		else
			new_proc->p_cwd = vfs_root_vn;
		if(new_proc->p_cwd != NULL)
			vref(new_proc->p_cwd);
		/*end of code for VFS*/

	new_proc->p_vmmap = vmmap_create();


    return new_proc;
}

/**
 * Cleans up as much as the process as can be done from within the
 * process. This involves:
 *    - Closing all open files (VFS)
 *    - Cleaning up VM mappings (VM)
 *    - Waking up its parent if it is waiting
 *    - Reparenting any children to the init process
 *    - Setting its status and state appropriately
 *
 * The parent will finish destroying the process within do_waitpid (make
 * sure you understand why it cannot be done here). Until the parent
 * finishes destroying it, the process is informally called a 'zombie'
 * process.
 *
 * This is also where any children of the current process should be
 * reparented to the init process (unless, of course, the current
 * process is the init process. However, the init process should not
 * have any children at the time it exits).
 *
 * Note: You do _NOT_ have to special case the idle process. It should
 * never exit this way.
 *
 * @param status the status to exit the process with
 */
void
proc_cleanup(int status)
{
	proc_t *process;
	KASSERT(NULL != proc_initproc); 
	dbg(DBG_PRINT,"(GRADING1A 2.b) Should have an init process\n");
    KASSERT(1 <= curproc->p_pid); 
	dbg(DBG_PRINT,"(GRADING1A 2.b) This process should not be idle process\n");
	KASSERT(NULL != curproc->p_pproc);
	dbg(DBG_PRINT,"(GRADING1A 2.b) This process should have parent process\n");
	curproc->p_state=PROC_DEAD;
	curproc->p_status=status;
    KASSERT(NULL != curproc->p_pproc);
	dbg(DBG_PRINT,"(GRADING1A 2.b) This process should have parent process\n");

        int fd;
        /*close opened file*/
        for(fd =0; fd < NFILES; fd++){
            if(curproc->p_files[fd])
				do_close(fd);
		}
		if(curproc->p_cwd)
			vput(curproc->p_cwd);


	proc_t *parent=curproc->p_pproc;
	/*reparent all processe children*/
	if (!list_empty(&curproc->p_children))
    {
        dbg(DBG_PRINT,"(GRADING1C 6) Current process has children\n");
        if (curproc==proc_initproc)
        {
           dbg(DBG_PRINT,"(GRADING1B) Init Process has children when it exits\n");	/*print this line only when the kernel is exiting*/
        }
        else
        {
            dbg(DBG_PRINT,"(GRADING1C 6) The current process is not init process\n");
            list_iterate_begin(&curproc->p_children, process, proc_t, p_child_link)
            {
            	list_remove(&(process->p_child_link));
                process->p_pproc = proc_initproc;
                list_insert_tail(&proc_initproc->p_children,&process->p_child_link);  /*inserts at the end of the list.*/
            }
            list_iterate_end();
        }
        
    }
    /*wake up parent, the parent is waiting so*/
	vmmap_destroy(curproc->p_vmmap);
	curproc->p_vmmap = NULL;
    sched_wakeup_on(&parent->p_wait);
}

/*
 * This has nothing to do with signals and kill(1).
 *
 * Calling this on the current process is equivalent to calling
 * do_exit().
 *
 * In Weenix, this is only called from proc_kill_all.
 */
void
proc_kill(proc_t *p, int status)
{
    kthread_t *killed_thread;
    if (curproc==p)
    { 
	dbg(DBG_PRINT,"(GRADING1C 9) Process is killed by itself\n");
        do_exit(status);
        /*Calling this on the current process is equivalent to calling do_exit().*/
    }
    else
    {
        dbg(DBG_PRINT,"(GRADING1C 8) Process is killed by %s\n",curproc->p_comm);
        list_iterate_begin(&(p->p_threads),killed_thread,kthread_t,kt_plink)
        {
            kthread_cancel(killed_thread,0);
        }
        list_iterate_end();
    }
}

/*
 * Remember, proc_kill on the current process will _NOT_ return.
 * Don't kill direct children of the idle process.
 *
 * In Weenix, this is only called by sys_halt.
 */
void
proc_kill_all()
{
    /*NOT_YET_IMPLEMENTED("PROCS: proc_kill_all");*/
    /*In Weenix, this is only called by sys_halt. So we don't need to care about return value*/
    proc_t *process;
    list_iterate_begin(&_proc_list,process,proc_t,p_list_link)
    {
        if((process->p_pid!=PID_IDLE)&&(process->p_pid!=PID_INIT)&&(process->p_pproc->p_pid!=PID_IDLE))
        {
            dbg(DBG_PRINT,"(GRADING1C 9) Make sure init process and idle process are not killed\n");
            if(process!= curproc)
            {
                dbg(DBG_PRINT,"(GRADING1C 9) Make sure current process is not killed\n");
                proc_kill(process, 0);

            }
        }
    }
    list_iterate_end();

	if (curproc->p_pid!=PID_IDLE && curproc->p_pproc->p_pid!=PID_IDLE)
    {
        dbg(DBG_PRINT,"(GRADING1C 9) Make sure current process is not idle process and its parent is not idle process\n");
        proc_kill(curproc,0);
    }
    sched_switch();
}

/*
 * This function is only called from kthread_exit.
 *
 * Unless you are implementing MTP, this just means that the process
 * needs to be cleaned up and a new thread needs to be scheduled to
 * run. If you are implementing MTP, a single thread exiting does not
 * necessarily mean that the process should be exited.
 */
void
proc_thread_exited(void *retval)
{
    /*NOT_YET_IMPLEMENTED("PROCS: proc_thread_exited");*/
    proc_cleanup((int)retval);   /*the process needs to be cleaned up, not sure about input parameter*/
    /*I don't know what retval means*/
    dbg(DBG_PRINT,"(GRADING1C 0) Process thread exited successfully\n");
	sched_switch();
    /*dbg print sth*/
}

/* If pid is -1 dispose of one of the exited children of the current
 * process and return its exit status in the status argument, or if
 * all children of this process are still running, then this function
 * blocks on its own p_wait queue until one exits.
 *
 * If pid is greater than 0 and the given pid is a child of the
 * current process then wait for the given pid to exit and dispose
 * of it.
 *
 * If the current process has no children, or the given pid is not
 * a child of the current process return -ECHILD.
 *t
 * Pids other than -1 and positive numbers are not supported.
 * Options other than 0 are not supported.
 */
pid_t
do_waitpid(pid_t pid, int options, int *status)
{
    /*NOT_YET_IMPLEMENTED("PROCS: do_waitpid");
    return 0;*/
    if (list_empty(&(curproc->p_children)))
	{
		dbg(DBG_PRINT,"(GRADING1C 1) The current process has no children\n");
		return -ECHILD;
	}
    /*if (pid==0 || pid<-1 || options!=0)
	{
		dbg(DBG_PRINT,"(GRADING1E) The pid is not valid\n");
		return -EINVAL;
	}*/
    proc_t *i;
    pid_t ret;
    kthread_t *j;
    if (pid==-1)
    {
		dbg(DBG_PRINT,"(GRADING1B) The input pid is -1\n");
        while(1)
        {
            list_iterate_begin(&(curproc->p_children),i,proc_t,p_child_link)
            {
				KASSERT(NULL != i); /* the process should not be NULL */
                dbg(DBG_PRINT, "(GRADING1A 2.c) The process is not NULL\n");
                KASSERT(-1 == pid || i->p_pid == pid); /* should be able to find the process */
                dbg(DBG_PRINT, "(GRADING1A 2.c) The process could be found\n");
                KASSERT(NULL != i->p_pagedir); /* this process should have pagedir */
                dbg(DBG_PRINT, "(GRADING1A 2.c) The process has pagedir\n");
                if (i->p_state==PROC_DEAD)
                {
                    dbg(DBG_PRINT,"(GRADING1C 1) This child's status is dead\n");
                    list_iterate_begin(&(i->p_threads),j,kthread_t,kt_plink)
                    {
                        KASSERT(KT_EXITED == j->kt_state);/* thr points to a thread to be destroied */ 
                        dbg(DBG_PRINT, "(GRADING1A 2.c) The to be destroied thread has been found\n");
                        kthread_destroy(j);
                    }list_iterate_end();
                    pt_destroy_pagedir(i->p_pagedir);
                    i->p_pagedir=NULL;
                    list_remove(&(i->p_child_link));
                    list_remove(&(i->p_list_link));
                    if (status!=NULL)
		    {
			dbg(DBG_PRINT,"(GRADING1C 1) The status is not NULL\n");
			*status=i->p_status;
		    }
                    ret=i->p_pid;
                    slab_obj_free(proc_allocator,i);
                    return ret;
                }
            }list_iterate_end();
            sched_sleep_on(&(curproc->p_wait));
        }
    }
    else
    {
	dbg(DBG_PRINT,"(GRADING1C 0) The input pid is possitive\n");
        list_iterate_begin(&(curproc->p_children),i,proc_t,p_child_link)
        {
            KASSERT(NULL != i); /* the process should not be NULL */
            dbg(DBG_PRINT, "(GRADING1A 2.c) The process is not NULL\n");
            if (i->p_pid==pid)
            {
				dbg(DBG_PRINT,"(GRADING1C 0) The input pid has been found\n");
                KASSERT(-1 == pid || i->p_pid == pid); /* should be able to find the process */
                dbg(DBG_PRINT, "(GRADING1A 2.c) The process could be found\n");
                KASSERT(NULL != i->p_pagedir); /* this process should have pagedir */
                dbg(DBG_PRINT, "(GRADING1A 2.c) The process has pagedir\n");
                while (!(i->p_state==PROC_DEAD)) sched_sleep_on(&(curproc->p_wait));
                list_iterate_begin(&(i->p_threads),j,kthread_t,kt_plink)
                {
                    KASSERT(KT_EXITED == j->kt_state);/* thr points to a thread to be destroied */ 
                    dbg(DBG_PRINT, "(GRADING1A 2.c) The to be destroied thread has been found\n");
                    kthread_destroy(j);
                }list_iterate_end();
                pt_destroy_pagedir(i->p_pagedir);
                i->p_pagedir=NULL;
                list_remove(&(i->p_child_link));
                list_remove(&(i->p_list_link));
                if (status!=NULL)
		{
			dbg(DBG_PRINT,"(GRADING1C 1) The status is not NULL\n");
			*status=i->p_status;
		}
                ret=i->p_pid;
                slab_obj_free(proc_allocator,i);
                return ret;
            }
        }list_iterate_end();
        return -ECHILD;
    }
    return -ECHILD;
}

/*
 * Cancel all threads, join with them, and exit from the current
 * thread.
 *
 * @param status the exit status of the process
 */
void do_exit(int status)
{
       kthread_cancel(curthr,(void*) status);
}


size_t
proc_info(const void *arg, char *buf, size_t osize)
{
    const proc_t *p = (proc_t *) arg;
    size_t size = osize;
    proc_t *child;
    
    KASSERT(NULL != p);
    KASSERT(NULL != buf);
    
    iprintf(&buf, &size, "pid:          %i\n", p->p_pid);
    iprintf(&buf, &size, "name:         %s\n", p->p_comm);
    if (NULL != p->p_pproc) {
        iprintf(&buf, &size, "parent:       %i (%s)\n",
                p->p_pproc->p_pid, p->p_pproc->p_comm);
    } else {
        iprintf(&buf, &size, "parent:       -\n");
    }
    
#ifdef __MTP__
    int count = 0;
    kthread_t *kthr;
    list_iterate_begin(&p->p_threads, kthr, kthread_t, kt_plink) {
        ++count;
    } list_iterate_end();
    iprintf(&buf, &size, "thread count: %i\n", count);
#endif
    
    if (list_empty(&p->p_children)) {
        iprintf(&buf, &size, "children:     -\n");
    } else {
        iprintf(&buf, &size, "children:\n");
    }
    list_iterate_begin(&p->p_children, child, proc_t, p_child_link) {
        iprintf(&buf, &size, "     %i (%s)\n", child->p_pid, child->p_comm);
    } list_iterate_end();
    
    iprintf(&buf, &size, "status:       %i\n", p->p_status);
    iprintf(&buf, &size, "state:        %i\n", p->p_state);
    
#ifdef __VFS__
#ifdef __GETCWD__cl
    if (NULL != p->p_cwd) {
        char cwd[256];
        lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
        iprintf(&buf, &size, "cwd:          %-s\n", cwd);
    } else {
        iprintf(&buf, &size, "cwd:          -\n");
    }
#endif /* __GETCWD__ */
#endif
    
#ifdef __VM__
    iprintf(&buf, &size, "start brk:    0x%p\n", p->p_start_brk);
    iprintf(&buf, &size, "brk:          0x%p\n", p->p_brk);
#endif
    
    return size;
}

size_t
proc_list_info(const void *arg, char *buf, size_t osize)
{
    size_t size = osize;
    proc_t *p;
    
    KASSERT(NULL == arg);
    KASSERT(NULL != buf);
    
#if defined(__VFS__) && defined(__GETCWD__)
    iprintf(&buf, &size, "%5s %-13s %-18s %-s\n", "PID", "NAME", "PARENT", "CWD");
#else
    iprintf(&buf, &size, "%5s %-13s %-s\n", "PID", "NAME", "PARENT");
#endif
    
    list_iterate_begin(&_proc_list, p, proc_t, p_list_link) {
        char parent[64];
        if (NULL != p->p_pproc) {
            snprintf(parent, sizeof(parent),
                     "%3i (%s)", p->p_pproc->p_pid, p->p_pproc->p_comm);
        } else {
            snprintf(parent, sizeof(parent), "  -");
        }
        
#if defined(__VFS__) && defined(__GETCWD__)
        if (NULL != p->p_cwd) {
            char cwd[256];
            lookup_dirpath(p->p_cwd, cwd, sizeof(cwd));
            iprintf(&buf, &size, " %3i  %-13s %-18s %-s\n",
                    p->p_pid, p->p_comm, parent, cwd);
        } else {
            iprintf(&buf, &size, " %3i  %-13s %-18s -\n",
                    p->p_pid, p->p_comm, parent);
        }
#else
        iprintf(&buf, &size, " %3i  %-13s %-s\n",
                p->p_pid, p->p_comm, parent);
#endif
    } list_iterate_end();
    return size;
}

