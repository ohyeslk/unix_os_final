/******************************************************************************/
/* Important Fall 2014 CSCI 402 usage information:							*/
/*																			*/
/* This fils is part of CSCI 402 kernel programming assignments at USC.	   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*		 display a copy of this file (or ANY PART of it) for any reason.	*/
/* If anyone (including your prospective employer) asks you to post the code, */
/*		 you must inform them that you do NOT have permissions to do so.	*/
/* You are also NOT permitted to remove or alter this comment block.		  */
/******************************************************************************/

#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/tlb.h"
#include "mm/mmobj.h"
static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{

		vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
		KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
		vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
		KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
		vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
		if (newvma) {
				newvma->vma_vmmap = NULL;
		}
		return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
		KASSERT(NULL != vma);
		slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
		KASSERT(0 < osize);
		KASSERT(NULL != buf);
		KASSERT(NULL != vmmap);

		vmmap_t *map = (vmmap_t *)vmmap;
		vmarea_t *vma;
		ssize_t size = (ssize_t)osize;

		int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
						   "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
						   "VFN RANGE");

		list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
				size -= len;
				buf += len;
				if (0 >= size) {
						goto end;
				}

				len = snprintf(buf, size,
							   "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
							   vma->vma_start << PAGE_SHIFT,
							   vma->vma_end << PAGE_SHIFT,
							   (vma->vma_prot & PROT_READ ? 'r' : '-'),
							   (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
							   (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
							   (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
							   vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
		} list_iterate_end();

end:
		if (size <= 0) {
				size = osize;
				buf[osize - 1] = '\0';
		}

		return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        dbg(DBG_PRINT,"go into vmmap_create\n");
	vmmap_t *ret;
    if((ret = (vmmap_t *)slab_obj_alloc(vmmap_allocator) )!= NULL) 
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
		list_init(&(ret -> vmm_list));
		ret->vmm_proc=NULL;
	}
	return ret;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
        dbg(DBG_PRINT,"go into vmmap_destroy\n");
	KASSERT(NULL != map);
	dbg(DBG_PFRAME, "(GRADING3A 3.a)\n");
	vmarea_t *i = NULL;

	list_iterate_begin(&(map -> vmm_list), i, vmarea_t, vma_plink)
	{
		if(i->vma_obj!=NULL) 
		{
			dbg(DBG_PRINT,"(GRADING3B)\n");
			i->vma_obj->mmo_ops->put(i->vma_obj);
		}		
		list_remove(&i->vma_olink);
		vmarea_free(i);
	} list_iterate_end();

	slab_obj_free(vmmap_allocator, map);
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
    dbg(DBG_PRINT,"go into vmmap_insert\n");
	KASSERT(NULL != map && NULL != newvma);
	dbg(DBG_PFRAME, "(GRADING3A 3.b)\n");
	KASSERT(NULL == newvma->vma_vmmap);
	dbg(DBG_PFRAME, "(GRADING3A 3.b)\n");
	KASSERT(newvma->vma_start < newvma->vma_end);
	dbg(DBG_PFRAME, "(GRADING3A 3.b)\n");
	KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
	dbg(DBG_PFRAME, "(GRADING3A 3.b)\n");
	vmarea_t *i;
	newvma->vma_vmmap = map;
	list_iterate_begin(&map->vmm_list, i, vmarea_t, vma_plink)
	{
		if(newvma->vma_start <= i->vma_start)
		{
			dbg(DBG_PRINT,"(GRADING3B)\n");
			list_insert_before(&i->vma_plink, &newvma->vma_plink);
			return;
		}
	}list_iterate_end();
	list_insert_tail(&map->vmm_list, &newvma->vma_plink);
}
/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range linyins.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
        dbg(DBG_PRINT,"go into vmmap_find_range\n");
	KASSERT(NULL != map);
	dbg(DBG_PFRAME, "(GRADING3A 3.c)\n");
	KASSERT(0 < npages);
	dbg(DBG_PFRAME, "(GRADING3A 3.c)\n");

	vmarea_t *i;
	int ret,tmp,gap;

	KASSERT(dir!=VMMAP_DIR_LOHI);
	if(dir==VMMAP_DIR_HILO) 
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
	tmp=ADDR_TO_PN(USER_MEM_HIGH);
	}
	KASSERT(dir!=VMMAP_DIR_LOHI);
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
			list_iterate_reverse(&map->vmm_list, i, vmarea_t, vma_plink){
			gap=tmp-i->vma_end;
			if (gap>=(int)npages) return tmp-npages;
			tmp=i->vma_start;
			}list_iterate_end();
		}

	KASSERT(dir!=VMMAP_DIR_LOHI);
	{
		dbg(DBG_PRINT,"(GRADING3C)\n");
		gap=tmp-ADDR_TO_PN(USER_MEM_LOW);
		if (gap>=(int)npages) 
		{		
			dbg(DBG_PRINT,"(GRADING3C)\n");
			return tmp-npages;
		}
	}
	return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
    dbg(DBG_PRINT,"go into vmmap_lookup\n");
	KASSERT(NULL != map);
	dbg(DBG_PFRAME, "(GRADING3A 3.d)\n");
	vmarea_t *i = NULL;

	list_iterate_begin(&(map -> vmm_list), i, vmarea_t, vma_plink)
	{
		if((i-> vma_start) <= vfn &&(i -> vma_end > vfn)) 
		{
			dbg(DBG_PRINT,"(GRADING3B)\n");
			return i;
		}
	} list_iterate_end();
	return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        dbg(DBG_PRINT,"go into vmmap_clone\n");
	vmarea_t *i = NULL;
	vmarea_t *tmp = NULL;	
	vmmap_t *ret;


	if((ret=vmmap_create()) == NULL) return NULL;
	ret -> vmm_proc = map -> vmm_proc;

	list_iterate_begin(&(map -> vmm_list), i, vmarea_t, vma_plink)
	{
		KASSERT((tmp = vmarea_alloc()));

		tmp -> vma_prot = i -> vma_prot;
		tmp -> vma_end = i -> vma_end;
		tmp -> vma_obj = NULL;
		tmp -> vma_flags = i -> vma_flags;
		tmp -> vma_vmmap = ret;
		tmp -> vma_off = i-> vma_off;
		tmp -> vma_start = i -> vma_start;

		list_insert_tail(&(ret -> vmm_list), &(tmp -> vma_plink));
	} list_iterate_end();

	return ret;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
		  int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        dbg(DBG_PRINT,"go into vmmap_map\n");
	KASSERT(NULL != map);
	dbg(DBG_PFRAME, "(GRADING3A 3.f)\n");
	KASSERT(0 < npages);
	dbg(DBG_PFRAME, "(GRADING3A 3.f)\n");
	KASSERT(!(~(PROT_NONE | PROT_READ | PROT_WRITE | PROT_EXEC) & prot));
	dbg(DBG_PFRAME, "(GRADING3A 3.f)\n");
	KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags));
	dbg(DBG_PFRAME, "(GRADING3A 3.f)\n");
	KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage));
	dbg(DBG_PFRAME, "(GRADING3 3.f)\n");
	KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages)));
	dbg(DBG_PFRAME, "(GRADING3A 3.f)\n");
	KASSERT(PAGE_ALIGNED(off));
	dbg(DBG_PFRAME, "(GRADING3A 3.f)\n");

	mmobj_t *actual,*shadow;

	if(lopage == 0)
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
		int tmp=vmmap_find_range(map, npages, dir);
		if (tmp<0) return tmp;
		lopage=tmp;
	}
	else 
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
	vmmap_remove(map, lopage, npages);
	}

	vmarea_t *tmp = vmarea_alloc();
	tmp -> vma_end = lopage + npages ;
	tmp -> vma_off = off / PAGE_SIZE;
	tmp -> vma_start = lopage;
	tmp -> vma_prot = prot;
	tmp -> vma_flags = flags;
	vmmap_insert(map,tmp);

	if(file == NULL) 
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
		actual = anon_create();
	}
	else 
	{		
		dbg(DBG_PRINT,"(GRADING3B)\n");
		file->vn_ops->mmap(file,tmp,&actual);
	}

	list_insert_head(&actual->mmo_un.mmo_vmas,&tmp->vma_olink);

	if((flags & MAP_PRIVATE) == MAP_PRIVATE)
	{
		dbg(DBG_PRINT,"(GRADING3B)\n");
		shadow = shadow_create();
		shadow->mmo_shadowed = actual;
		shadow->mmo_un.mmo_bottom_obj = actual;
		tmp -> vma_obj = shadow;
	}
	else 
{		dbg(DBG_PRINT,"(GRADING3C)\n");
tmp -> vma_obj = actual;
}
	if(new) 
{		dbg(DBG_PRINT,"(GRADING3B)\n");
	*new = tmp;
}
	return 0;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *		  [			 ]   linyining VM Area
 *		*******			 Region to be unmapped
 *
 * Case 1:  [   ******	]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [	  *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****		]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
        dbg(DBG_PRINT,"go into vmmap_remove\n");
	vmarea_t *tmp,*i;
	list_iterate_begin(&(map->vmm_list),i,vmarea_t,vma_plink)
	{
		if(lopage>i->vma_start && lopage+npages<i->vma_end)
		{
			dbg(DBG_PRINT,"(GRADING3C)\n");
			tmp=vmarea_alloc();
			tmp->vma_flags=i->vma_flags;
			tmp->vma_off=i->vma_off-i->vma_start+lopage+npages;	
			tmp->vma_prot=i->vma_prot;
			tmp->vma_vmmap=map;
			tmp->vma_start=npages+lopage;
			tmp->vma_end=i->vma_end;
			vmarea_t *tmp1=	list_item((i->vma_plink).l_next,vmarea_t,vma_plink);
			i->vma_end =lopage;
			list_insert_before(&(tmp1->vma_plink),&(tmp -> vma_plink));


			if(tmp->vma_flags & MAP_PRIVATE)
			{
				dbg(DBG_PRINT,"(GRADING3C)\n");
				mmobj_t *new1=shadow_create(),*new2=shadow_create(), *old=i->vma_obj;
				KASSERT(new1 && new2);
				i->vma_obj=new1;
				tmp->vma_obj=new2;
			
				new2->mmo_un.mmo_bottom_obj=old->mmo_un.mmo_bottom_obj;
				new1->mmo_un.mmo_bottom_obj=old->mmo_un.mmo_bottom_obj;
				new1->mmo_shadowed=old;
				new2->mmo_shadowed=old;
				old->mmo_ops->ref(old);
				list_insert_tail(&(old->mmo_un.mmo_bottom_obj->mmo_un.mmo_vmas), &(tmp -> vma_olink));
			}
		}
		else if((lopage<=i->vma_start)&&(lopage+npages>=i->vma_end))
		{
			dbg(DBG_PRINT,"(GRADING3B)\n");
			i->vma_obj->mmo_ops->put(i->vma_obj);
			list_remove(&(i -> vma_plink));
			list_remove(&i->vma_olink);
			vmarea_free(i);
		}
		else if((lopage <= i -> vma_start) &&( lopage + npages > i -> vma_start) && (lopage + npages < i -> vma_end)) 
		{
		dbg(DBG_PRINT,"(GRADING3C)\n");
			i -> vma_off=i -> vma_off + lopage + npages - i -> vma_start;
			i -> vma_start=lopage + npages;
		}
		else if(lopage > i -> vma_start && lopage < i -> vma_end && lopage + npages >= i -> vma_end) 
		{
		dbg(DBG_PRINT,"(GRADING3C)\n");
		i-> vma_end = lopage;
		}


	} list_iterate_end();

	tlb_flush_all();
	pt_unmap_range(curproc->p_pagedir,(uintptr_t)PN_TO_ADDR(lopage),(uintptr_t)PN_TO_ADDR(lopage + npages));
	return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_is_range_empty");
        return 0;*/
        dbg(DBG_PRINT,"go into vmmap_is_range_empty\n");
    vmarea_t *tempvm;
	unsigned int endvfn=startvfn+npages;

	KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
	dbg(DBG_PRINT,"(GRADING3A 3.e)\n");

	list_iterate_begin(&map->vmm_list,tempvm,vmarea_t,vma_plink)
	{
		if((startvfn < tempvm -> vma_start&& startvfn + npages > tempvm -> vma_start))
		{
            dbg(DBG_PRINT, "(GRADING3C)\n");
            return 0;
        }
		if(startvfn >= tempvm -> vma_start&& startvfn < tempvm -> vma_end)
		{
            dbg(DBG_PRINT, "(GRADING3C)\n");
            return 0;
        }
	}list_iterate_end();

		return 1;

}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing linyin.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_read");*/
        dbg(DBG_PRINT,"go into vmmap_read\n");
    KASSERT(NULL != map);
    uint32_t shift = 0;
    uint32_t vfn = ADDR_TO_PN(vaddr);
    uint32_t curvfn = vfn;
    uintptr_t curVaddr = (uintptr_t)vaddr; 
    uintptr_t offset = PAGE_OFFSET(vaddr);

    while(count > 0)
	{
        vmarea_t* curArea = vmmap_lookup(map, curvfn);
		KASSERT(curArea && curArea->vma_obj);
        pframe_t* curPframe;
        int page_number,linyin; 
		page_number= ADDR_TO_PN(curVaddr) - curArea->vma_start + curArea->vma_off;
		linyin = curArea->vma_obj->mmo_ops->lookuppage(curArea->vma_obj, page_number, 0, &curPframe);
		KASSERT(linyin==0);
		KASSERT(count <=PAGE_SIZE - offset);
        memcpy((void*)((uint32_t)buf+shift), (void*)((uint32_t)curPframe->pf_addr + offset), count);
        return 0;
    }
    return 0;

}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing linyin. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        /*NOT_YET_IMPLEMENTED("VM: vmmap_write");
        return 0;
        */
        dbg(DBG_PRINT,"go into vmmap_write\n");
        KASSERT(NULL != map);
    uint32_t shift = 0;
    uint32_t vfn = ADDR_TO_PN(vaddr);
    uint32_t nextvfn = vfn;

    uintptr_t curVaddr = (uintptr_t)vaddr; 
    uintptr_t offset = PAGE_OFFSET(vaddr);

    while(count > 0)
	{
        vmarea_t* curArea;
        pframe_t* curPframe;
        int page_number,linyin,available;
 		curArea= vmmap_lookup(map, nextvfn);
		KASSERT(curArea);

		page_number = ADDR_TO_PN(curVaddr)+curArea->vma_off-curArea->vma_start ;
 		linyin= curArea->vma_obj->mmo_ops->lookuppage(curArea->vma_obj, page_number, 0, &curPframe);
        available = pframe_dirty(curPframe);
		KASSERT(linyin==0);
		KASSERT(available==0);

        if(count<=PAGE_SIZE-offset)
		{
			dbg(DBG_PRINT,"(GRADING3B)\n");
            memcpy((void*)((uint32_t)curPframe->pf_addr + offset), (void*)((uint32_t)buf+shift), count);
            return 0;
			
        }
        else
		{
			dbg(DBG_PRINT,"(GRADING3C)\n");
            nextvfn = ADDR_TO_PN((uint32_t)curVaddr  - offset+ PAGE_SIZE);
            memcpy((void*)((uint32_t)curPframe->pf_addr + offset), (void*)((uint32_t)buf+shift), PAGE_SIZE - offset);
			curVaddr += PAGE_SIZE - offset;	
            shift += PAGE_SIZE - offset;            
			count -= PAGE_SIZE - offset;
            offset = 0;
        }
    }

    return 0;
}
