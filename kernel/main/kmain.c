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

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadowd.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/disk/ata.h"
#include "drivers/tty/virtterm.h"
#include "drivers/pci.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"


#include "test/kshell/kshell.h"
#include "errno.h"



GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void       hard_shutdown(void);
static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);

static context_t bootstrap_context;
static int gdb_wait = GDBWAIT;

extern void * faber_thread_test(int arg1, void *arg2);
extern void * sunghan_test(int arg1, void *arg2);
extern void * sunghan_deadlock_test(int arg1, void *arg2);

extern int vfstest_main(int argc, char **argv);
extern int faber_fs_thread_test(kshell_t *ksh, int argc, char **argv);
extern int faber_directory_test(kshell_t *ksh, int argc, char **argv);

/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);

        dbg_init();
        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
	      pci_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
        /* This little loop gives gdb a place to synch up with weenix.  In the
         * past the weenix command started qemu was started with -S which
         * allowed gdb to connect and start before the boot loader ran, but
         * since then a bug has appeared where breakpoints fail if gdb connects
         * before the boot loader runs.  See
         *
         * https://bugs.launchpad.net/qemu/+bug/526653
         *
         * This loop (along with an additional command in init.gdb setting
         * gdb_wait to 0) sticks weenix at a known place so gdb can join a
         * running weenix, set gdb_wait to zero  and catch the breakpoint in
         * bootstrap below.  See Config.mk for how to set GDBWAIT correctly.
         *
         * DANGER: if GDBWAIT != 0, and gdb is not running, this loop will never
         * exit and weenix will not run.  Make SURE the GDBWAIT is set the way
         * you expect.
         */
        while (gdb_wait) ;
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
bootstrap(int arg1, void *arg2)
{
	/* necessary to finalize page table information */
	pt_template_init();
	/*NOT_YET_IMPLEMENTED("PROCS: bootstrap");*/
	curproc = proc_create("idle");
	KASSERT(NULL != curproc);
	dbg(DBG_PRINT, "(GRADING1A 1.a) The \"idle\" process has been created successfully\n");
	KASSERT(PID_IDLE == curproc->p_pid);
	dbg(DBG_PRINT, "(GRADING1A 1.a) The process which is just created is \"idle\" process\n");
	if (curproc!=NULL)
		curthr = kthread_create(curproc,idleproc_run,0,NULL);
	KASSERT(NULL != curthr);
	dbg(DBG_PRINT, "(GRADING1A 1.a) The thread for the \"idle\" process has been created successfully\n");
	if (curthr!=NULL)
		curthr->kt_state=KT_RUN;
    if (curthr!=NULL)
		context_make_active(&(curthr->kt_ctx));
	panic("weenix returned to bootstrap()!!! BAD!!!\n");
	return NULL;
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
        int status;
        pid_t child;


    	dbg(DBG_PRINT,"(GRADING1E) Idle process is running\n");

        kthread_t *initthr = initproc_create();
        init_call_all();
        GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
        /* Once you have VFS remember to set the current working directory
         * of the idle and init processes */
        /*NOT_YET_IMPLEMENTED("VFS: idleproc_run");*/
    
        proc_lookup(PID_IDLE)->p_cwd = vfs_root_vn;
        vref(proc_lookup(PID_IDLE)->p_cwd);
        proc_lookup(PID_INIT)->p_cwd = vfs_root_vn;
        vref(proc_lookup(PID_INIT)->p_cwd);

        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */
        do_mkdir("/dev");
        do_mknod("/dev/null", S_IFCHR, MKDEVID(1, 0));
        do_mknod("/dev/zero", S_IFCHR, MKDEVID(1, 1));
        do_mknod("/dev/tty0", S_IFCHR, MKDEVID(2, 0));

#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();

        /* Run initproc */
        sched_make_runnable(initthr);
        /* Now wait for it */
        child = do_waitpid(-1, 0, &status);
        KASSERT(PID_INIT == child);

#ifdef __MTP__
        kthread_reapd_shutdown();
#endif


#ifdef __SHADOWD__
        /* wait for shadowd to shutdown */
        shadowd_shutdown();
#endif

#ifdef __VFS__
        /* Shutdown the vfs: */
        dbg_print("weenix: vfs shutdown...\n");
        vput(curproc->p_cwd);
        if (vfs_shutdown())
                panic("vfs shutdown FAILED!!\n");

#endif

        /* Shutdown the pframe system */
#ifdef __S5FS__
        pframe_shutdown();
#endif

        dbg_print("\nweenix: halted cleanly!\n");
        GDB_CALL_HOOK(shutdown);
        hard_shutdown();
        return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
	proc_t * initprocess;
	kthread_t * initthread;
	initprocess=proc_create("init");
	KASSERT(NULL != initprocess/* pointer to the "init" process */);
	dbg(DBG_PRINT, "(GRADING1A 1.b) The  \"init\" process in not NULL\n");
	KASSERT(PID_INIT == /* pointer to the "init" process */initprocess->p_pid);
	dbg(DBG_PRINT, "(GRADING1A 1.b) The  \"init\" process has correct pid\n");
	if (initprocess!=NULL)
	{
		dbg(DBG_PRINT,"(GRADING1E) The init process is not NULL\n");
		initthread=kthread_create(initprocess,initproc_run, 0, 0);
	}
    KASSERT(/* pointer to the thread for the "init" process */ initthread!= NULL);
	dbg(DBG_PRINT, "(GRADING1A 1.b) The  thread of \"init\" process is not NULL\n");
	return initthread;
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/sbin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */

#ifdef __DRIVERS__
int faber_invoker(kshell_t *kshell, int argc, char **argv)
{
	proc_t* newproc;
    kthread_t* newthread;	
	KASSERT(kshell != NULL);
	dbg(DBG_PRINT,"(GRADING1C) faber_invoker() is invoked, argc = %d, argv = 0x%08x\n",argc, (unsigned int)argv);
	newproc = proc_create("faber_invoker");
    KASSERT(newproc != NULL);
    if (newproc!=NULL)
	{
		dbg(DBG_PRINT,"(GRADING1E) The faber test process is created\n");
		newthread = kthread_create(newproc, faber_thread_test, argc, argv);
		sched_make_runnable(newthread);
	}
    do_waitpid(newproc->p_pid, 0, NULL);
    return 0;
}
int sunghan_invoker(kshell_t *kshell, int argc, char **argv)
{
	proc_t* newproc;
    kthread_t* newthread;	
	KASSERT(kshell != NULL);
	dbg(DBG_PRINT,"(GRADING1D) sunghan_invoker() is invoked, argc = %d, argv = 0x%08x\n",argc, (unsigned int)argv);
	newproc = proc_create("sunghan_invoker");
    KASSERT(newproc != NULL);
    if (newproc!=NULL)
	{
		dbg(DBG_PRINT,"(GRADING1E) The sunghan test process is created\n");
		newthread = kthread_create(newproc, sunghan_test, argc, argv);
		sched_make_runnable(newthread);
	}
    do_waitpid(newproc->p_pid, 0, NULL);
    return 0;
}
int sunghan_deadlock_invoker(kshell_t *kshell, int argc, char **argv)
{
	proc_t* newproc;
    kthread_t* newthread;	
	KASSERT(kshell != NULL);
	dbg(DBG_PRINT,"(GRADING1D): sunghan_deadlock_invoker() is invoked, argc = %d, argv = 0x%08x\n",argc, (unsigned int)argv);
	newproc = proc_create("sunghan_deadlock_invoker");
    KASSERT(newproc != NULL);
    if (newproc!=NULL)
	{
		dbg(DBG_PRINT,"(GRADING1E) The sunghan's deadlock test process is created\n");
		newthread = kthread_create(newproc, sunghan_deadlock_test, argc, argv);
		sched_make_runnable(newthread);
	}
    do_waitpid(newproc->p_pid, 0, NULL);
    return 0;
}
int vfstest_invoker(kshell_t *kshell, int argc, char **argv)
{
    proc_t* newproc;
    kthread_t* newthread;
    KASSERT(kshell != NULL);
    dbg(DBG_PRINT,"(GRADING1D): vfstest_invoker() is invoked, argc = %d, argv = 0x%08x\n",argc, (unsigned int)argv);
    newproc = proc_create("vfstest_invoker");
    KASSERT(newproc != NULL);
    if (newproc!=NULL)
    {
        dbg(DBG_PRINT,"(GRADING1E) The vfstest process is created\n");
        /*-------------path????---------------*/
        /*-------kernel/test/vfstest/vfstest.c-------invoke vfstest_main() */
        newthread = kthread_create(newproc,(kthread_func_t)vfstest_main, argc, argv);
        /*------------------------------------*/
        sched_make_runnable(newthread);
    }
    do_waitpid(newproc->p_pid, 0, NULL);
    return 0;
}


#endif

static void *
initproc_run(int arg1, void *arg2)
{
    char *argv[] = { NULL };
    char *envp[] = { NULL };
    kernel_execve("/sbin/init", argv, envp);
    /*kernel_execve("/usr/bin/hello", argv, envp);*/

#ifdef __DRIVERS__
	kshell_add_command("faber_test", faber_invoker, "invoke faber_invoker()");
	kshell_add_command("sunghan_test", sunghan_invoker, "invoke sunghan_invoker()");
	kshell_add_command("sunghan_deadlock_test", sunghan_deadlock_invoker, "invoke sunghan_deadlock_invoker()");
    kshell_add_command("vfstest", vfstest_invoker, "invoke vfstest_invoker()");
    kshell_add_command("faber_directory_test", faber_directory_test, "invoke faber_directory_test()");
    kshell_add_command("faber_fs_thread_test", faber_fs_thread_test, "invoke faber_fs_thread_test()");

	kshell_t *kshell = kshell_create(0);
	if (NULL == kshell) panic("Couldn't create kshell\n");
	while (kshell_execute_next(kshell));
	kshell_destroy(kshell);
#endif
	return NULL;
}
