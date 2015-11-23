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

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/shadowd.h"

#define SHADOW_SINGLETON_THRESHOLD 5

int shadow_count = 0; /* for debugging/verification purposes */
#ifdef __SHADOWD__
/*
 * number of shadow objects with a single parent, that is another shadow
 * object in the shadow objects tree(singletons)
 */
static int shadow_singleton_count = 0;
#endif

static slab_allocator_t *shadow_allocator;

static void shadow_ref(mmobj_t *o);
static void shadow_put(mmobj_t *o);
static int  shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  shadow_fillpage(mmobj_t *o, pframe_t *pf);
static int  shadow_dirtypage(mmobj_t *o, pframe_t *pf);
static int  shadow_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t shadow_mmobj_ops = {
        .ref = shadow_ref,
        .put = shadow_put,
        .lookuppage = shadow_lookuppage,
        .fillpage  = shadow_fillpage,
        .dirtypage = shadow_dirtypage,
        .cleanpage = shadow_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * shadow page sub system. Currently it only initializes the
 * shadow_allocator object.
 */
void
shadow_init()
{
        dbg(DBG_PRINT,"go into shadow_init\n");
        shadow_allocator = slab_allocator_create("shadow_allocator", sizeof(mmobj_t));
        KASSERT(shadow_allocator);
        dbg(DBG_PRINT,"(GRADING3A 6.a)\n");
        /*NOT_YET_IMPLEMENTED("VM: shadow_init");*/
}

/*
 * You'll want to use the shadow_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
shadow_create()
{
       /* NOT_YET_IMPLEMENTED("VM: shadow_create");*/
        dbg(DBG_PRINT,"go into shadow_create\n");
    mmobj_t *mmobj= (mmobj_t*)slab_obj_alloc(shadow_allocator);
    mmobj_init(mmobj, &shadow_mmobj_ops);
    mmobj->mmo_un.mmo_bottom_obj=NULL;
    mmobj->mmo_refcount=1;
    return mmobj;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
shadow_ref(mmobj_t *o)
{
        dbg(DBG_PRINT,"go into shadow_ref\n");
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
        dbg(DBG_PRINT,"(GRADING3A 6.a)\n");
    	(o->mmo_refcount) ++ ;
       /* NOT_YET_IMPLEMENTED("VM: shadow_ref");*/
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is a shadow object, it will never
 * be used again. You should unpin and uncache all of the object's
 * pages and then free the object itself.
 */
static void
shadow_put(mmobj_t *o)
{
        dbg(DBG_PRINT,"go into shadow_put\n");
	KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
	dbg(DBG_PRINT,"(GRADING3A 6.c)\n");	
    (o->mmo_refcount)--;

    pframe_t* pframe;

    slab_allocator_t *pf_allocator = slab_allocator_create("pframe", sizeof(pframe_t));
    if((o->mmo_refcount)==(o->mmo_nrespages))
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
		list_iterate_begin(&o->mmo_respages, pframe, pframe_t, pf_olink)
		{
            pframe_unpin(pframe);  /*unpin*/
            pframe_clean(pframe);
            mmobj_t *tempObj = pframe->pf_obj;
            tlb_flush((uintptr_t) pframe->pf_addr);
            pframe_remove_from_pts(pframe);
            list_remove(&pframe->pf_hlink);
            pframe->pf_obj = NULL;
            list_remove(&pframe->pf_link);
            page_free(pframe->pf_addr);
            slab_obj_free(pf_allocator, pframe);
            tempObj->mmo_nrespages--;
            tempObj->mmo_refcount --;
            list_remove(&pframe->pf_olink);
        }list_iterate_end();
    
        o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);
        o->mmo_shadowed = NULL;
        slab_obj_free(shadow_allocator,o);
    }
    
}

/* This function looks up the given page in this shadow object. The
 * forwrite argument is true if the page is being looked up for
 * writing, false if it is being looked up for reading. This function
 * must handle all do-not-copy-on-not-write magic (i.e. when forwrite
 * is false find the first shadow object in the chain which has the
 * given page resident). copy-on-write magic (necessary when forwrite
 * is true) is handled in shadow_fillpage, not here. It is important to
 * use iteration rather than recursion here as a recursive implementation
 * can overflow the kernel stack when looking down a long shadow chain */
static int
shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        dbg(DBG_PRINT,"go into shadow_lookuppage\n");
    pframe_t *given_page = pframe_get_resident(o, pagenum);
    if(forwrite){ /*for write*/
        if(given_page!=NULL)
		{
		dbg(DBG_PRINT,"(GRADING3B)\n");
            *pf=given_page;
            return 0;
        }
        else
		{
		dbg(DBG_PRINT,"(GRADING3B)\n");
            int errno = pframe_get(o, pagenum, pf);
            if(0!=errno)
			{
				dbg(DBG_PRINT,"(GRADING3C)\n");
                return errno;
            }
            shadow_dirtypage(o,*pf);
            return 0;
        }
    }
    else{/*for read*/
        mmobj_t *cur = o;
        pframe_t *cur_page;
        while(cur != NULL){
            cur_page = pframe_get_resident(cur, pagenum);
            if(cur_page!=NULL)
			{
				dbg(DBG_PRINT,"(GRADING3B)\n");
                *pf=cur_page;
                return 0;
            }
            else{
				dbg(DBG_PRINT,"(GRADING3B)\n");
                if(cur->mmo_shadowed == NULL)
				{
					dbg(DBG_PRINT,"(GRADING3B)\n");
                    int errno = cur->mmo_ops->lookuppage(cur, pagenum, 0, pf);
                    return errno;
                }
            }
            cur = cur->mmo_shadowed;
        }
    }
    return 0;
}

/* As per the specification in mmobj.h, fill the page frame starting
 * at address pf->pf_addr with the contents of the page identified by
 * pf->pf_obj and pf->pf_pagenum. This function handles all
 * copy-on-write magic (i.e. if there is a shadow object which has
 * data for the pf->pf_pagenum-th page then we should take that data,
 * if no such shadow object exists we need to follow the chain of
 * shadow objects all the way to the bottom object and take the data
 * for the pf->pf_pagenum-th page from the last object in the chain).
 * It is important to use iteration rather than recursion here as a 
 * recursive implementation can overflow the kernel stack when 
 * looking down a long shadow chain */
static int
shadow_fillpage(mmobj_t *o, pframe_t *pf)
{
        dbg(DBG_PRINT,"go into shadow_fillpage\n");
        KASSERT(pframe_is_busy(pf));
	dbg(DBG_PRINT,"(GRADING3A 6.d)\n");
        KASSERT(!pframe_is_pinned(pf));
	dbg(DBG_PRINT,"(GRADING3A 6.d)\n");
    pframe_t *pframe;
    if(0!=pframe_lookup(o->mmo_shadowed,pf->pf_pagenum,0,&pframe))
	{
		dbg(DBG_PRINT,"(GRADING3C)\n");
        return -1;
    }
    pframe_pin(pf);
    memcpy(pf->pf_addr,pframe->pf_addr,PAGE_SIZE);
    return 0;

}

/* These next two functions are not difficult. */

static int
shadow_dirtypage(mmobj_t *o, pframe_t *pf)
{
        dbg(DBG_PRINT,"go into shadow_dirtypage\n");
    pframe_set_dirty(pf);
    return 0;
}

static int
shadow_cleanpage(mmobj_t *o, pframe_t *pf)
{
        dbg(DBG_PRINT,"go into shadow_cleanpage\n");
    pframe_t  *page;
	KASSERT(pframe_lookup(o,pf->pf_pagenum,1,&page)==0);
    memcpy(page->pf_addr,pf->pf_addr,PAGE_SIZE);
    pframe_clear_dirty(page);
    return 0;

}
