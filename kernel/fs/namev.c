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
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "mm/kmalloc.h"
/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{        /*NOT_YET_IMPLEMENTED("VFS: lookup");*/
        KASSERT(NULL != dir);
        dbg(DBG_PRINT,"(GRADING2A 2.a)\n");
        KASSERT(NULL != name);
        dbg(DBG_PRINT,"(GRADING2A 2.a)\n");
        KASSERT(NULL != result);
        dbg(DBG_PRINT,"(GRADING2A 2.a)\n");

		KASSERT(!((dir->vn_ops->lookup==NULL)||(!S_ISDIR(dir->vn_mode))));
    if( strncmp(name, ".", len) == 0 ){
        *result = dir;
        vref(*result);
        dbg(DBG_PRINT,"GRADING2B\n");
        return 0;
    }
        int rd=dir->vn_ops->lookup(dir,name,len,result);
        if(rd <0)
		{
            dbg(DBG_PRINT,"GRADING2B\n");
            return rd;
        }
        else
		{
            dbg(DBG_PRINT,"GRADING2B\n");
            return 0;
        }
}

/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name, vnode_t *base, vnode_t **res_vnode)
{
/*NOT_YET_IMPLEMENTED("VFS: dir_namev");*/
        KASSERT(NULL != pathname);
        dbg(DBG_PRINT,"(GRADING2A 2.b)\n");
        KASSERT(NULL != namelen);
        dbg(DBG_PRINT,"(GRADING2A 2.b)\n");
        KASSERT(NULL != name);
        dbg(DBG_PRINT,"(GRADING2A 2.b)\n");
        KASSERT(NULL != res_vnode);
        dbg(DBG_PRINT,"(GRADING2A 2.b)\n");

	vnode_t *cur_dir;
	const char *startPtr = pathname;
	size_t temp_lenth = 0;
	int entry_len;
KASSERT(strlen(pathname)<=MAXPATHLEN);

   
        if (strlen(pathname)==0)
        {
                dbg(DBG_PRINT,"GRADING2B\n");

                return -EINVAL;
        }

	if (pathname[0] == '/') 
	{
        dbg(DBG_PRINT,"GRADING2B\n");
		while (*pathname == '/')
			pathname++;
		if (*pathname == '\0') 
		{
            dbg(DBG_PRINT,"GRADING2B\n");
			*res_vnode = vfs_root_vn;
			vref(vfs_root_vn);
			*name = pathname;
			*namelen = 0;
			return 0;
		}
		cur_dir = vfs_root_vn;
	}
	else {
        dbg(DBG_PRINT,"GRADING2B\n");
		if (base == NULL)
		{
            dbg(DBG_PRINT,"GRADING2B\n");
			base = curproc->p_cwd;
			cur_dir=base;
		}
		cur_dir = base;
	}

	KASSERT(NULL != cur_dir);
	dbg(DBG_PRINT, "(GRADING2A 2.b)\n");
	vref(cur_dir);


	while (1) 
	{
		entry_len = 0;
		startPtr = pathname;
		
		while (!((*pathname == '\0') || (*pathname == '/')))
		{
			if (entry_len+1 == NAME_LEN )
			{
				vput(cur_dir);
				return -ENAMETOOLONG;
			}
			entry_len++;
			pathname++;
		}
		if (*pathname == '/') 
		{
			while (*pathname == '/')
				pathname++;
			if (*pathname == '\0') 
			{
                dbg(DBG_PRINT,"GRADING2B\n");
				temp_lenth = entry_len;
				break;
			}

               
			vnode_t *tmp_dir = cur_dir;
			int ret = lookup(tmp_dir, startPtr, entry_len, &cur_dir);
			vput(tmp_dir);
			if (ret<0)
			{	
            	dbg(DBG_PRINT,"GRADING2B\n");
				return ret;
			}
		}
		else 
		{
            dbg(DBG_PRINT,"GRADING2B\n");
			temp_lenth = entry_len;
			break;
		}
	}

	*res_vnode = cur_dir;
	*namelen = temp_lenth;
	*name = startPtr;

	if (!S_ISDIR(cur_dir->vn_mode)) 
	{
        dbg(DBG_PRINT,"GRADING2B\n");
		vput(cur_dir);
		return -ENOTDIR;
	}

	return 0;
}


/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
        /*NOT_YET_IMPLEMENTED("VFS: open_namev");*/
        size_t length;
        char* name;
        vnode_t* result_vnode;
        int res;
        int result=dir_namev(pathname,&length,(const char**)&name,base,&result_vnode);
        if(result<0)
		{
            dbg(DBG_PRINT,"GRADING2B\n");
            return result;
        }
        int lookup_result = lookup(result_vnode, name, length, res_vnode);
        if(lookup_result == -ENOENT)
		{
            dbg(DBG_PRINT,"GRADING2B\n");
            if(flag&O_CREAT)
			{
                dbg(DBG_PRINT,"GRADING2B\n");
                KASSERT(NULL != result_vnode->vn_ops->create);
                dbg(DBG_PRINT,"(GRADING2A 2.c)\n");

				res = result_vnode->vn_ops->create(result_vnode,name,length,res_vnode );
                if(res<0)
                    {
                        dbg(DBG_PRINT,"GRADING2B\n");
                        vput(result_vnode);
                        return res;
                    }
            }
            else
			{
                dbg(DBG_PRINT,"GRADING2B\n");
                vput(result_vnode);
                return lookup_result;
            }
        }
        dbg(DBG_PRINT,"GRADING2B\n");
        vput(result_vnode);
        return 0;
}


#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
