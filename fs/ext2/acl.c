/*
 * linux/fs/ext2/acl.c
 *
 * Copyright (C) 2001 by Andreas Gruenbacher, <a.gruenbacher@computer.org>
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "ext2.h"
#include "xattr.h"
#include "acl.h"

/*
 * Convert from filesystem to in-memory representation.
 */
static struct posix_acl *
ext2_acl_from_disk(const void *value, size_t size)
{
	const char *end = (char *)value + size;
	int n, count;
	struct posix_acl *acl;

	if (!value)
		return NULL;
	if (size < sizeof(ext2_acl_header))
		 return ERR_PTR(-EINVAL);
	if (((ext2_acl_header *)value)->a_version !=
	    cpu_to_le32(EXT2_ACL_VERSION))
		return ERR_PTR(-EINVAL);
	value = (char *)value + sizeof(ext2_acl_header);
	count = ext2_acl_count(size);
	if (count < 0)
		return ERR_PTR(-EINVAL);
	if (count == 0)
		return NULL;
	acl = posix_acl_alloc(count, GFP_KERNEL);
	if (!acl)
		return ERR_PTR(-ENOMEM);
	for (n=0; n < count; n++) {
		ext2_acl_entry *entry =
			(ext2_acl_entry *)value;
		if ((char *)value + sizeof(ext2_acl_entry_short) > end)
			goto fail;
		acl->a_entries[n].e_tag  = le16_to_cpu(entry->e_tag);
		acl->a_entries[n].e_perm = le16_to_cpu(entry->e_perm);
		switch(acl->a_entries[n].e_tag) {
			case ACL_USER_OBJ:
			case ACL_GROUP_OBJ:
			case ACL_MASK:
			case ACL_OTHER:
				value = (char *)value +
					sizeof(ext2_acl_entry_short);
				acl->a_entries[n].e_id = ACL_UNDEFINED_ID;
				break;

			case ACL_USER:
			case ACL_GROUP:
				value = (char *)value + sizeof(ext2_acl_entry);
				if ((char *)value > end)
					goto fail;
				acl->a_entries[n].e_id =
					le32_to_cpu(entry->e_id);
				break;

			default:
				goto fail;
		}
	}
	if (value != end)
		goto fail;
	return acl;

fail:
	posix_acl_release(acl);
	return ERR_PTR(-EINVAL);
}

/*
 * Convert from in-memory to filesystem representation.
 */
static void *
ext2_acl_to_disk(const struct posix_acl *acl, size_t *size)
{
	ext2_acl_header *ext_acl;
	char *e;
	int n;

	*size = ext2_acl_size(acl->a_count);
	ext_acl = (ext2_acl_header *)kmalloc(sizeof(ext2_acl_header) +
		acl->a_count * sizeof(ext2_acl_entry), GFP_KERNEL);
	if (!ext_acl)
		return ERR_PTR(-ENOMEM);
	ext_acl->a_version = cpu_to_le32(EXT2_ACL_VERSION);
	e = (char *)ext_acl + sizeof(ext2_acl_header);
	for (n=0; n < acl->a_count; n++) {
		ext2_acl_entry *entry = (ext2_acl_entry *)e;
		entry->e_tag  = cpu_to_le16(acl->a_entries[n].e_tag);
		entry->e_perm = cpu_to_le16(acl->a_entries[n].e_perm);
		switch(acl->a_entries[n].e_tag) {
			case ACL_USER:
			case ACL_GROUP:
				entry->e_id =
					cpu_to_le32(acl->a_entries[n].e_id);
				e += sizeof(ext2_acl_entry);
				break;

			case ACL_USER_OBJ:
			case ACL_GROUP_OBJ:
			case ACL_MASK:
			case ACL_OTHER:
				e += sizeof(ext2_acl_entry_short);
				break;

			default:
				goto fail;
		}
	}
	return (char *)ext_acl;

fail:
	kfree(ext_acl);
	return ERR_PTR(-EINVAL);
}

/*
 * inode->i_sem: down
 */
static struct posix_acl *
ext2_get_acl(struct inode *inode, int type)
{
	int name_index;
	char *value;
	struct posix_acl *acl, **p_acl;
	const size_t size = ext2_acl_size(EXT2_ACL_MAX_ENTRIES);
	int retval;

	if (!test_opt(inode->i_sb, POSIX_ACL))
		return 0;

	switch(type) {
		case ACL_TYPE_ACCESS:
			p_acl = &EXT2_I(inode)->i_acl;
			name_index = EXT2_XATTR_INDEX_POSIX_ACL_ACCESS;
			break;

		case ACL_TYPE_DEFAULT:
			p_acl = &EXT2_I(inode)->i_default_acl;
			name_index = EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT;
			break;

		default:
			return ERR_PTR(-EINVAL);
	}
	if (*p_acl != EXT2_ACL_NOT_CACHED)
		return posix_acl_dup(*p_acl);
	value = kmalloc(size, GFP_KERNEL);
	if (!value)
		return ERR_PTR(-ENOMEM);

	retval = ext2_xattr_get(inode, name_index, "", value, size);

	if (retval == -ENODATA || retval == -ENOSYS)
		*p_acl = acl = NULL;
	else if (retval < 0)
		acl = ERR_PTR(retval);
	else {
		acl = ext2_acl_from_disk(value, retval);
		if (!IS_ERR(acl))
			*p_acl = posix_acl_dup(acl);
	}
	kfree(value);
	return acl;
}

/*
 * inode->i_sem: down
 */
static int
ext2_set_acl(struct inode *inode, int type, struct posix_acl *acl)
{
	int name_index;
	void *value = NULL;
	struct posix_acl **p_acl;
	size_t size;
	int error;

	if (S_ISLNK(inode->i_mode))
		return -EOPNOTSUPP;
	if (!test_opt(inode->i_sb, POSIX_ACL))
		return 0;

	switch(type) {
		case ACL_TYPE_ACCESS:
			name_index = EXT2_XATTR_INDEX_POSIX_ACL_ACCESS;
			p_acl = &EXT2_I(inode)->i_acl;
			if (acl) {
				mode_t mode = inode->i_mode;
				error = posix_acl_equiv_mode(acl, &mode);
				if (error < 0)
					return error;
				else {
					inode->i_mode = mode;
					mark_inode_dirty(inode);
					if (error == 0)
						acl = NULL;
				}
			}
			break;

		case ACL_TYPE_DEFAULT:
			name_index = EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT;
			p_acl = &EXT2_I(inode)->i_default_acl;
			if (!S_ISDIR(inode->i_mode))
				return acl ? -EACCES : 0;
			break;

		default:
			return -EINVAL;
	}
 	if (acl) {
		if (acl->a_count > EXT2_ACL_MAX_ENTRIES)
			return -EINVAL;
		value = ext2_acl_to_disk(acl, &size);
		if (IS_ERR(value))
			return (int)PTR_ERR(value);
	}

	error = ext2_xattr_set(inode, name_index, "", value, size, 0);

	if (value)
		kfree(value);
	if (!error) {
		if (*p_acl && *p_acl != EXT2_ACL_NOT_CACHED)
			posix_acl_release(*p_acl);
		*p_acl = posix_acl_dup(acl);
	}
	return error;
}

static int
__ext2_permission(struct inode *inode, int mask, int lock)
{
	int mode = inode->i_mode;

	/* Nobody gets write access to a read-only fs */
	if ((mask & MAY_WRITE) && IS_RDONLY(inode) &&
	    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
		return -EROFS;
	/* Nobody gets write access to an immutable file */
	if ((mask & MAY_WRITE) && IS_IMMUTABLE(inode))
	    return -EACCES;
	if (current->fsuid == inode->i_uid) {
		mode >>= 6;
	} else if (test_opt(inode->i_sb, POSIX_ACL)) {
		/* ACL can't contain additional permissions if
		   the ACL_MASK entry is 0 */
		if (!(mode & S_IRWXG))
			goto check_groups;
		if (EXT2_I(inode)->i_acl == EXT2_ACL_NOT_CACHED) {
			struct posix_acl *acl;

			if (lock) {
				down(&inode->i_sem);
				acl = ext2_get_acl(inode, ACL_TYPE_ACCESS);
				up(&inode->i_sem);
			} else
				acl = ext2_get_acl(inode, ACL_TYPE_ACCESS);

			if (IS_ERR(acl))
				return PTR_ERR(acl);
			posix_acl_release(acl);
			if (EXT2_I(inode)->i_acl == EXT2_ACL_NOT_CACHED)
				return -EIO;
		}
		if (EXT2_I(inode)->i_acl) {
			int error = posix_acl_permission(inode,
				EXT2_I(inode)->i_acl, mask);
			if (error == -EACCES)
				goto check_capabilities;
			return error;
		} else
			goto check_groups;
	} else {
check_groups:
		if (in_group_p(inode->i_gid))
			mode >>= 3;
	}
	if ((mode & mask & S_IRWXO) == mask)
		return 0;

check_capabilities:
	/* Allowed to override Discretionary Access Control? */
	if ((mask & (MAY_READ|MAY_WRITE)) || (inode->i_mode & S_IXUGO))
		if (capable(CAP_DAC_OVERRIDE))
			return 0;
	/* Read and search granted if capable(CAP_DAC_READ_SEARCH) */
	if (capable(CAP_DAC_READ_SEARCH) && ((mask == MAY_READ) ||
	    (S_ISDIR(inode->i_mode) && !(mask & MAY_WRITE))))
		return 0;
	return -EACCES;
}

/*
 * Inode operation permission().
 *
 * inode->i_sem: up
 * BKL held [before 2.5.x]
 */
int
ext2_permission(struct inode *inode, int mask)
{
	return __ext2_permission(inode, mask, 1);
}

/*
 * Used internally if i_sem is already down.
 */
int
ext2_permission_locked(struct inode *inode, int mask)
{
	return __ext2_permission(inode, mask, 0);
}

/*
 * Initialize the ACLs of a new inode. Called from ext2_new_inode.
 *
 * dir->i_sem: down
 * inode->i_sem: up (access to inode is still exclusive)
 * BKL held [before 2.5.x] 
 */
int
ext2_init_acl(struct inode *inode, struct inode *dir)
{
	struct posix_acl *acl = NULL;
	int error = 0;

	if (!S_ISLNK(inode->i_mode)) {
		if (test_opt(dir->i_sb, POSIX_ACL)) {
			acl = ext2_get_acl(dir, ACL_TYPE_DEFAULT);
			if (IS_ERR(acl))
				return PTR_ERR(acl);
		}
		if (!acl)
			inode->i_mode &= ~current->fs->umask;
	}
	if (test_opt(inode->i_sb, POSIX_ACL) && acl) {
               struct posix_acl *clone;
	       mode_t mode;

		if (S_ISDIR(inode->i_mode)) {
			error = ext2_set_acl(inode, ACL_TYPE_DEFAULT, acl);
			if (error)
				goto cleanup;
		}
		clone = posix_acl_clone(acl, GFP_KERNEL);
		error = -ENOMEM;
		if (!clone)
			goto cleanup;
		mode = inode->i_mode;
		error = posix_acl_create_masq(clone, &mode);
		if (error >= 0) {
			inode->i_mode = mode;
			if (error > 0) {
				/* This is an extended ACL */
				error = ext2_set_acl(inode,
						     ACL_TYPE_ACCESS, clone);
			}
		}
		posix_acl_release(clone);
	}
cleanup:
       posix_acl_release(acl);
       return error;
}

/*
 * Does chmod for an inode that may have an Access Control List. The
 * inode->i_mode field must be updated to the desired value by the caller
 * before calling this function.
 * Returns 0 on success, or a negative error number.
 *
 * We change the ACL rather than storing some ACL entries in the file
 * mode permission bits (which would be more efficient), because that
 * would break once additional permissions (like  ACL_APPEND, ACL_DELETE
 * for directories) are added. There are no more bits available in the
 * file mode.
 *
 * inode->i_sem: down
 * BKL held [before 2.5.x]
 */
int
ext2_acl_chmod(struct inode *inode)
{
	struct posix_acl *acl, *clone;
        int error;

	if (!test_opt(inode->i_sb, POSIX_ACL))
		return 0;
	if (S_ISLNK(inode->i_mode))
		return -EOPNOTSUPP;
	acl = ext2_get_acl(inode, ACL_TYPE_ACCESS);
	if (IS_ERR(acl) || !acl)
		return PTR_ERR(acl);
	clone = posix_acl_clone(acl, GFP_KERNEL);
	posix_acl_release(acl);
	if (!clone)
		return -ENOMEM;
	error = posix_acl_chmod_masq(clone, inode->i_mode);
	if (!error)
		error = ext2_set_acl(inode, ACL_TYPE_ACCESS, clone);
	posix_acl_release(clone);
	return error;
}

/*
 * Extended attribut handlers
 */
static size_t
ext2_xattr_list_acl_access(char *list, struct inode *inode,
			   const char *name, int name_len, int flags)
{
	const size_t size = sizeof(XATTR_NAME_ACL_ACCESS);

	if (!test_opt(inode->i_sb, POSIX_ACL))
		return 0;
	if (list)
		memcpy(list, XATTR_NAME_ACL_ACCESS, size);
	return size;
}

static size_t
ext2_xattr_list_acl_default(char *list, struct inode *inode,
			    const char *name, int name_len, int flags)
{
	const size_t size = sizeof(XATTR_NAME_ACL_DEFAULT);

	if (!test_opt(inode->i_sb, POSIX_ACL))
		return 0;
	if (list)
		memcpy(list, XATTR_NAME_ACL_DEFAULT, size);
	return size;
}

static int
ext2_xattr_get_acl(struct inode *inode, int type, void *buffer, size_t size)
{
	struct posix_acl *acl;
	int error;

	if (!test_opt(inode->i_sb, POSIX_ACL))
		return -EOPNOTSUPP;

	acl = ext2_get_acl(inode, type);
	if (IS_ERR(acl))
		return PTR_ERR(acl);
	if (acl == NULL)
		return -ENODATA;
	error = posix_acl_to_xattr(acl, buffer, size);
	posix_acl_release(acl);

	return error;
}

static int
ext2_xattr_get_acl_access(struct inode *inode, const char *name,
			  void *buffer, size_t size, int flags)
{
	if (strcmp(name, "") != 0)
		return -EINVAL;
	return ext2_xattr_get_acl(inode, ACL_TYPE_ACCESS, buffer, size);
}

static int
ext2_xattr_get_acl_default(struct inode *inode, const char *name,
			   void *buffer, size_t size, int flags)
{
	if (strcmp(name, "") != 0)
		return -EINVAL;
	return ext2_xattr_get_acl(inode, ACL_TYPE_DEFAULT, buffer, size);
}

static int
ext2_xattr_set_acl(struct inode *inode, int type, const void *value,
		   size_t size)
{
	struct posix_acl *acl;
	int error;

	if (!test_opt(inode->i_sb, POSIX_ACL))
		return -EOPNOTSUPP;
	if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
		return -EPERM;

	if (value) {
		acl = posix_acl_from_xattr(value, size);
		if (IS_ERR(acl))
			return PTR_ERR(acl);
		else if (acl) {
			error = posix_acl_valid(acl);
			if (error)
				goto release_and_out;
		}
	} else
		acl = NULL;

	error = ext2_set_acl(inode, type, acl);

release_and_out:
	posix_acl_release(acl);
	return error;
}

static int
ext2_xattr_set_acl_access(struct inode *inode, const char *name,
			  const void *value, size_t size, int flags)
{
	if (strcmp(name, "") != 0)
		return -EINVAL;
	return ext2_xattr_set_acl(inode, ACL_TYPE_ACCESS, value, size);
}

static int
ext2_xattr_set_acl_default(struct inode *inode, const char *name,
			   const void *value, size_t size, int flags)
{
	if (strcmp(name, "") != 0)
		return -EINVAL;
	return ext2_xattr_set_acl(inode, ACL_TYPE_DEFAULT, value, size);
}

struct ext2_xattr_handler ext2_xattr_acl_access_handler = {
	.prefix	= XATTR_NAME_ACL_ACCESS,
	.list	= ext2_xattr_list_acl_access,
	.get	= ext2_xattr_get_acl_access,
	.set	= ext2_xattr_set_acl_access,
};

struct ext2_xattr_handler ext2_xattr_acl_default_handler = {
	.prefix	= XATTR_NAME_ACL_DEFAULT,
	.list	= ext2_xattr_list_acl_default,
	.get	= ext2_xattr_get_acl_default,
	.set	= ext2_xattr_set_acl_default,
};

void
exit_ext2_acl(void)
{
	ext2_xattr_unregister(EXT2_XATTR_INDEX_POSIX_ACL_ACCESS,
			      &ext2_xattr_acl_access_handler);
	ext2_xattr_unregister(EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT,
			      &ext2_xattr_acl_default_handler);
}

int __init
init_ext2_acl(void)
{
	int error;

	error = ext2_xattr_register(EXT2_XATTR_INDEX_POSIX_ACL_ACCESS,
				    &ext2_xattr_acl_access_handler);
	if (error)
		goto fail;
	error = ext2_xattr_register(EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT,
				    &ext2_xattr_acl_default_handler);
	if (error)
		goto fail;
	return 0;

fail:
	exit_ext2_acl();
	return error;
}
