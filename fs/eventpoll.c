/*
 *  fs/eventpoll.c ( Efficent event polling implementation )
 *  Copyright (C) 2001,...,2002	 Davide Libenzi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>
#include <linux/wait.h>
#include <linux/eventpoll.h>
#include <linux/mount.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/mman.h>
#include <asm/atomic.h>



#define EVENTPOLLFS_MAGIC 0x03111965 /* My birthday should work for this :) */

#define DEBUG_EPOLL 0

#if DEBUG_EPOLL > 0
#define DPRINTK(x) printk x
#define DNPRINTK(n, x) do { if ((n) <= DEBUG_EPOLL) printk x; } while (0)
#else /* #if DEBUG_EPOLL > 0 */
#define DPRINTK(x) (void) 0
#define DNPRINTK(n, x) (void) 0
#endif /* #if DEBUG_EPOLL > 0 */

#define DEBUG_EPI 0

#if DEBUG_EPI != 0
#define EPI_SLAB_DEBUG (SLAB_DEBUG_FREE | SLAB_RED_ZONE /* | SLAB_POISON */)
#else /* #if DEBUG_EPI != 0 */
#define EPI_SLAB_DEBUG 0
#endif /* #if DEBUG_EPI != 0 */


/* Maximum number of poll wake up nests we are allowing */
#define EP_MAX_POLLWAKE_NESTS 4

/* Maximum size of the hash in bits ( 2^N ) */
#define EP_MAX_HASH_BITS 17

/* Minimum size of the hash in bits ( 2^N ) */
#define EP_MIN_HASH_BITS 9

/* Number of hash entries ( "struct list_head" ) inside a page */
#define EP_HENTRY_X_PAGE (PAGE_SIZE / sizeof(struct list_head))

/* Maximum size of the hash in pages */
#define EP_MAX_HPAGES ((1 << EP_MAX_HASH_BITS) / EP_HENTRY_X_PAGE + 1)

/* Number of pages allocated for an "hbits" sized hash table */
#define EP_HASH_PAGES(hbits) ((int) ((1 << (hbits)) / EP_HENTRY_X_PAGE + \
				     ((1 << (hbits)) % EP_HENTRY_X_PAGE ? 1: 0)))

/* Macro to allocate a "struct epitem" from the slab cache */
#define EPI_MEM_ALLOC()	(struct epitem *) kmem_cache_alloc(epi_cache, SLAB_KERNEL)

/* Macro to free a "struct epitem" to the slab cache */
#define EPI_MEM_FREE(p) kmem_cache_free(epi_cache, p)

/* Macro to allocate a "struct eppoll_entry" from the slab cache */
#define PWQ_MEM_ALLOC()	(struct eppoll_entry *) kmem_cache_alloc(pwq_cache, SLAB_KERNEL)

/* Macro to free a "struct eppoll_entry" to the slab cache */
#define PWQ_MEM_FREE(p) kmem_cache_free(pwq_cache, p)

/* Fast test to see if the file is an evenpoll file */
#define IS_FILE_EPOLL(f) ((f)->f_op == &eventpoll_fops)

/*
 * Remove the item from the list and perform its initialization.
 * This is useful for us because we can test if the item is linked
 * using "EP_IS_LINKED(p)".
 */
#define EP_LIST_DEL(p) do { list_del(p); INIT_LIST_HEAD(p); } while (0)

/* Tells us if the item is currently linked */
#define EP_IS_LINKED(p) (!list_empty(p))

/* Get the "struct epitem" from a wait queue pointer */
#define EP_ITEM_FROM_WAIT(p) ((struct epitem *) container_of(p, struct eppoll_entry, wait)->base)

/* Get the "struct epitem" from an epoll queue wrapper */
#define EP_ITEM_FROM_EPQUEUE(p) (container_of(p, struct ep_pqueue, pt)->epi)

/*
 * This is used to optimize the event transfer to userspace. Since this
 * is kept on stack, it should be pretty small.
 */
#define EP_MAX_BUF_EVENTS 32

/*
 * Used to optimize ready items collection by reducing the irqlock/irqunlock
 * switching rate. This is kept in stack too, so do not go wild with this number.
 */
#define EP_MAX_COLLECT_ITEMS 64


/*
 * Node that is linked into the "wake_task_list" member of the "struct poll_safewake".
 * It is used to keep track on all tasks that are currently inside the wake_up() code
 * to 1) short-circuit the one coming from the same task and same wait queue head
 * ( loop ) 2) allow a maximum number of epoll descriptors inclusion nesting
 * 3) let go the ones coming from other tasks.
 */
struct wake_task_node {
	struct list_head llink;
	task_t *task;
	wait_queue_head_t *wq;
};

/*
 * This is used to implement the safe poll wake up avoiding to reenter
 * the poll callback from inside wake_up().
 */
struct poll_safewake {
	struct list_head wake_task_list;
	spinlock_t lock;
};

/*
 * This structure is stored inside the "private_data" member of the file
 * structure and rapresent the main data sructure for the eventpoll
 * interface.
 */
struct eventpoll {
	/* Protect the this structure access */
	rwlock_t lock;

	/* Wait queue used by sys_epoll_wait() */
	wait_queue_head_t wq;

	/* Wait queue used by file->poll() */
	wait_queue_head_t poll_wait;

	/* List of ready file descriptors */
	struct list_head rdllist;

	/* Size of the hash */
	unsigned int hashbits;

	/* Pages for the "struct epitem" hash */
	char *hpages[EP_MAX_HPAGES];
};

/* Wait structure used by the poll hooks */
struct eppoll_entry {
	/* List header used to link this structure to the "struct epitem" */
	struct list_head llink;

	/* The "base" pointer is set to the container "struct epitem" */
	void *base;

	/*
	 * Wait queue item that will be linked to the target file wait
	 * queue head.
	 */
	wait_queue_t wait;

	/* The wait queue head that linked the "wait" wait queue item */
	wait_queue_head_t *whead;
};

/*
 * Each file descriptor added to the eventpoll interface will
 * have an entry of this type linked to the hash.
 */
struct epitem {
	/* List header used to link this structure to the eventpoll hash */
	struct list_head llink;

	/* List header used to link this structure to the eventpoll ready list */
	struct list_head rdllink;

	/* Number of active wait queue attached to poll operations */
	int nwait;

	/* List containing poll wait queues */
	struct list_head pwqlist;

	/* The "container" of this item */
	struct eventpoll *ep;

	/* The file this item refers to */
	struct file *file;

	/* The structure that describe the interested events and the source fd */
	struct epoll_event event;

	/*
	 * Used to keep track of the usage count of the structure. This avoids
	 * that the structure will desappear from underneath our processing.
	 */
	atomic_t usecnt;

	/* List header used to link this item to the "struct file" items list */
	struct list_head fllink;
};

/* Wrapper struct used by poll queueing */
struct ep_pqueue {
	poll_table pt;
	struct epitem *epi;
};



static void ep_poll_safewake_init(struct poll_safewake *psw);
static void ep_poll_safewake(struct poll_safewake *psw, wait_queue_head_t *wq);
static unsigned int ep_get_hash_bits(unsigned int hintsize);
static int ep_getfd(int *efd, struct inode **einode, struct file **efile);
static int ep_alloc_pages(char **pages, int numpages);
static int ep_free_pages(char **pages, int numpages);
static int ep_file_init(struct file *file, unsigned int hashbits);
static unsigned int ep_hash_index(struct eventpoll *ep, struct file *file);
static struct list_head *ep_hash_entry(struct eventpoll *ep, unsigned int index);
static int ep_init(struct eventpoll *ep, unsigned int hashbits);
static void ep_free(struct eventpoll *ep);
static struct epitem *ep_find(struct eventpoll *ep, struct file *file);
static void ep_use_epitem(struct epitem *epi);
static void ep_release_epitem(struct epitem *epi);
static void ep_ptable_queue_proc(struct file *file, wait_queue_head_t *whead, poll_table *pt);
static int ep_insert(struct eventpoll *ep, struct epoll_event *event, struct file *tfile);
static int ep_modify(struct eventpoll *ep, struct epitem *epi, struct epoll_event *event);
static void ep_unregister_pollwait(struct eventpoll *ep, struct epitem *epi);
static int ep_unlink(struct eventpoll *ep, struct epitem *epi);
static int ep_remove(struct eventpoll *ep, struct epitem *epi);
static int ep_poll_callback(wait_queue_t *wait, unsigned mode, int sync);
static int ep_eventpoll_close(struct inode *inode, struct file *file);
static unsigned int ep_eventpoll_poll(struct file *file, poll_table *wait);
static int ep_collect_ready_items(struct eventpoll *ep, struct epitem **aepi, int maxepi);
static int ep_send_events(struct eventpoll *ep, struct epitem **aepi, int nepi,
			  struct epoll_event *events);
static int ep_events_transfer(struct eventpoll *ep, struct epoll_event *events, int maxevents);
static int ep_poll(struct eventpoll *ep, struct epoll_event *events, int maxevents,
		   long timeout);
static int eventpollfs_delete_dentry(struct dentry *dentry);
static struct inode *ep_eventpoll_inode(void);
static struct super_block *eventpollfs_get_sb(struct file_system_type *fs_type,
					      int flags, char *dev_name, void *data);


/* Safe wake up implementation */
static struct poll_safewake psw;

/*
 * This semaphore is used to ensure that files are not removed
 * while epoll is using them. Namely the f_op->poll(), since
 * it has to be called from outside the lock, must be protected.
 * This is read-held during the event transfer loop to userspace
 * and it is write-held during the file cleanup path and the epoll
 * file exit code.
 */
static struct rw_semaphore epsem;

/* Slab cache used to allocate "struct epitem" */
static kmem_cache_t *epi_cache;

/* Slab cache used to allocate "struct eppoll_entry" */
static kmem_cache_t *pwq_cache;

/* Virtual fs used to allocate inodes for eventpoll files */
static struct vfsmount *eventpoll_mnt;

/* File callbacks that implement the eventpoll file behaviour */
static struct file_operations eventpoll_fops = {
	.release	= ep_eventpoll_close,
	.poll		= ep_eventpoll_poll
};

/*
 * This is used to register the virtual file system from where
 * eventpoll inodes are allocated.
 */
static struct file_system_type eventpoll_fs_type = {
	.name		= "eventpollfs",
	.get_sb		= eventpollfs_get_sb,
	.kill_sb	= kill_anon_super,
};

/* Very basic directory entry operations for the eventpoll virtual file system */
static struct dentry_operations eventpollfs_dentry_operations = {
	.d_delete	= eventpollfs_delete_dentry,
};



/* Initialize the poll safe wake up structure */
static void ep_poll_safewake_init(struct poll_safewake *psw)
{

	INIT_LIST_HEAD(&psw->wake_task_list);
	spin_lock_init(&psw->lock);
}


/*
 * Perform a safe wake up of the poll wait list. The problem is that
 * with the new callback'd wake up system, it is possible that the
 * poll callback is reentered from inside the call to wake_up() done
 * on the poll wait queue head. The rule is that we cannot reenter the
 * wake up code from the same task more than EP_MAX_POLLWAKE_NESTS times,
 * and we cannot reenter the same wait queue head at all. This will
 * enable to have a hierarchy of epoll file descriptor of no more than
 * EP_MAX_POLLWAKE_NESTS deep. We need the irq version of the spin lock
 * because this one gets called by the poll callback, that in turn is called
 * from inside a wake_up(), that might be called from irq context.
 */
static void ep_poll_safewake(struct poll_safewake *psw, wait_queue_head_t *wq)
{
	int wake_nests = 0;
	unsigned long flags;
	task_t *this_task = current;
	struct list_head *lsthead = &psw->wake_task_list, *lnk;
	struct wake_task_node tnode;

	spin_lock_irqsave(&psw->lock, flags);

	/* Try to see if the current task is already inside this wakeup call */
	list_for_each(lnk, lsthead) {
		struct wake_task_node *tncur = list_entry(lnk, struct wake_task_node, llink);

		if (tncur->task == this_task) {
			if (tncur->wq == wq || ++wake_nests > EP_MAX_POLLWAKE_NESTS) {
				/*
				 * Ops ... loop detected or maximum nest level reached.
				 * We abort this wake by breaking the cycle itself.
				 */
				spin_unlock_irqrestore(&psw->lock, flags);
				return;
			}
		}
	}

	/* Add the current task to the list */
	tnode.task = this_task;
	tnode.wq = wq;
	list_add(&tnode.llink, lsthead);

	spin_unlock_irqrestore(&psw->lock, flags);

	/* Do really wake up now */
	wake_up(wq);

	/* Remove the current task from the list */
	spin_lock_irqsave(&psw->lock, flags);
	list_del(&tnode.llink);
	spin_unlock_irqrestore(&psw->lock, flags);
}


/*
 * Calculate the size of the hash in bits. The returned size will be
 * bounded between EP_MIN_HASH_BITS and EP_MAX_HASH_BITS.
 */
static unsigned int ep_get_hash_bits(unsigned int hintsize)
{
	unsigned int i, val;

	for (i = 0, val = 1; val < hintsize && i < EP_MAX_HASH_BITS; i++, val <<= 1);
	return i <  EP_MIN_HASH_BITS ?  EP_MIN_HASH_BITS: i;
}


/* Used to initialize the epoll bits inside the "struct file" */
void eventpoll_init_file(struct file *file)
{

	INIT_LIST_HEAD(&file->f_ep_links);
	spin_lock_init(&file->f_ep_lock);
}


/*
 * This is called from inside fs/file_table.c:__fput() to unlink files
 * from the eventpoll interface. We need to have this facility to cleanup
 * correctly files that are closed without being removed from the eventpoll
 * interface.
 */
void eventpoll_release(struct file *file)
{
	struct list_head *lsthead = &file->f_ep_links;
	struct epitem *epi;

	/*
	 * Fast check to avoid the get/release of the semaphore. Since
	 * we're doing this outside the semaphore lock, it might return
	 * false negatives, but we don't care. It'll help in 99.99% of cases
	 * to avoid the semaphore lock. False positives simply cannot happen
	 * because the file in on the way to be removed and nobody ( but
	 * eventpoll ) has still a reference to this file.
	 */
	if (list_empty(lsthead))
		return;

	/*
	 * We don't want to get "file->f_ep_lock" because it is not
	 * necessary. It is not necessary because we're in the "struct file"
	 * cleanup path, and this means that noone is using this file anymore.
	 * The only hit might come from ep_free() but by holding the semaphore
	 * will correctly serialize the operation.
	 */
	down_write(&epsem);
	while (!list_empty(lsthead)) {
		epi = list_entry(lsthead->next, struct epitem, fllink);

		EP_LIST_DEL(&epi->fllink);
		ep_remove(epi->ep, epi);
	}
	up_write(&epsem);
}


/*
 * It opens an eventpoll file descriptor by suggesting a storage of "size"
 * file descriptors. The size parameter is just an hint about how to size
 * data structures. It won't prevent the user to store more than "size"
 * file descriptors inside the epoll interface. It is the kernel part of
 * the userspace epoll_create(2).
 */
asmlinkage long sys_epoll_create(int size)
{
	int error, fd;
	unsigned int hashbits;
	struct inode *inode;
	struct file *file;

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: sys_epoll_create(%d)\n",
		     current, size));

	/* Correctly size the hash */
	hashbits = ep_get_hash_bits((unsigned int) size);

	/*
	 * Creates all the items needed to setup an eventpoll file. That is,
	 * a file structure, and inode and a free file descriptor.
	 */
	error = ep_getfd(&fd, &inode, &file);
	if (error)
		goto eexit_1;

	/* Setup the file internal data structure ( "struct eventpoll" ) */
	error = ep_file_init(file, hashbits);
	if (error)
		goto eexit_2;


	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: sys_epoll_create(%d) = %d\n",
		     current, size, fd));

	return fd;

eexit_2:
	sys_close(fd);
eexit_1:
	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: sys_epoll_create(%d) = %d\n",
		     current, size, error));
	return error;
}


/*
 * The following function implement the controller interface for the eventpoll
 * file that enable the insertion/removal/change of file descriptors inside
 * the interest set. It rapresents the kernel part of the user spcae epoll_ctl(2).
 */
asmlinkage long sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	int error;
	struct file *file, *tfile;
	struct eventpoll *ep;
	struct epitem *epi;
	struct epoll_event epds;

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: sys_epoll_ctl(%d, %d, %d, %u)\n",
		     current, epfd, op, fd, event->events));

	error = -EFAULT;
	if (copy_from_user(&epds, event, sizeof(struct epoll_event)))
		goto eexit_1;

	/* Get the "struct file *" for the eventpoll file */
	error = -EBADF;
	file = fget(epfd);
	if (!file)
		goto eexit_1;

	/* Get the "struct file *" for the target file */
	tfile = fget(fd);
	if (!tfile)
		goto eexit_2;

	/* The target file descriptor must support poll */
	error = -EPERM;
	if (!tfile->f_op || !tfile->f_op->poll)
		goto eexit_3;

	/*
	 * We have to check that the file structure underneath the file descriptor
	 * the user passed to us _is_ an eventpoll file. And also we do not permit
	 * adding an epoll file descriptor inside itself.
	 */
	error = -EINVAL;
	if (file == tfile || !IS_FILE_EPOLL(file))
		goto eexit_3;

	/*
	 * At this point it is safe to assume that the "private_data" contains
	 * our own data structure.
	 */
	ep = file->private_data;

	/*
	 * Try to lookup the file inside our hash table. When an item is found
	 * ep_find() increases the usage count of the item so that it won't
	 * desappear underneath us. The only thing that might happen, if someone
	 * tries very hard, is a double insertion of the same file descriptor.
	 * This does not rapresent a problem though and we don't really want
	 * to put an extra syncronization object to deal with this harmless condition.
	 */
	epi = ep_find(ep, tfile);

	error = -EINVAL;
	switch (op) {
	case EPOLL_CTL_ADD:
		if (!epi) {
			epds.events |= POLLERR | POLLHUP;

			error = ep_insert(ep, &epds, tfile);
		} else
			error = -EEXIST;
		break;
	case EPOLL_CTL_DEL:
		if (epi)
			error = ep_remove(ep, epi);
		else
			error = -ENOENT;
		break;
	case EPOLL_CTL_MOD:
		if (epi) {
			epds.events |= POLLERR | POLLHUP;
			error = ep_modify(ep, epi, &epds);
		} else
			error = -ENOENT;
		break;
	}

	/*
	 * The function ep_find() increments the usage count of the structure
	 * so, if this is not NULL, we need to release it.
	 */
	if (epi)
		ep_release_epitem(epi);

eexit_3:
	fput(tfile);
eexit_2:
	fput(file);
eexit_1:
	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: sys_epoll_ctl(%d, %d, %d, %u) = %d\n",
		     current, epfd, op, fd, event->events, error));

	return error;
}


/*
 * Implement the event wait interface for the eventpoll file. It is the kernel
 * part of the user space epoll_wait(2).
 */
asmlinkage long sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents,
			       int timeout)
{
	int error;
	struct file *file;
	struct eventpoll *ep;

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: sys_epoll_wait(%d, %p, %d, %d)\n",
		     current, epfd, events, maxevents, timeout));

	/* The maximum number of event must be greater than zero */
	if (maxevents <= 0)
		return -EINVAL;

	/* Verify that the area passed by the user is writeable */
	if ((error = verify_area(VERIFY_WRITE, events, maxevents * sizeof(struct epoll_event))))
		goto eexit_1;

	/* Get the "struct file *" for the eventpoll file */
	error = -EBADF;
	file = fget(epfd);
	if (!file)
		goto eexit_1;

	/*
	 * We have to check that the file structure underneath the file descriptor
	 * the user passed to us _is_ an eventpoll file.
	 */
	error = -EINVAL;
	if (!IS_FILE_EPOLL(file))
		goto eexit_2;

	/*
	 * At this point it is safe to assume that the "private_data" contains
	 * our own data structure.
	 */
	ep = file->private_data;

	/* Time to fish for events ... */
	error = ep_poll(ep, events, maxevents, timeout);

eexit_2:
	fput(file);
eexit_1:
	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: sys_epoll_wait(%d, %p, %d, %d) = %d\n",
		     current, epfd, events, maxevents, timeout, error));

	return error;
}


/*
 * Creates the file descriptor to be used by the epoll interface.
 */
static int ep_getfd(int *efd, struct inode **einode, struct file **efile)
{
	struct qstr this;
	char name[32];
	struct dentry *dentry;
	struct inode *inode;
	struct file *file;
	int error, fd;

	/* Get an ready to use file */
	error = -ENFILE;
	file = get_empty_filp();
	if (!file)
		goto eexit_1;

	/* Allocates an inode from the eventpoll file system */
	inode = ep_eventpoll_inode();
	error = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto eexit_2;

	/* Allocates a free descriptor to plug the file onto */
	error = get_unused_fd();
	if (error < 0)
		goto eexit_3;
	fd = error;

	/*
	 * Link the inode to a directory entry by creating a unique name
	 * using the inode number.
	 */
	error = -ENOMEM;
	sprintf(name, "[%lu]", inode->i_ino);
	this.name = name;
	this.len = strlen(name);
	this.hash = inode->i_ino;
	dentry = d_alloc(eventpoll_mnt->mnt_sb->s_root, &this);
	if (!dentry)
		goto eexit_4;
	dentry->d_op = &eventpollfs_dentry_operations;
	d_add(dentry, inode);
	file->f_vfsmnt = mntget(eventpoll_mnt);
	file->f_dentry = dget(dentry);

	/*
	 * Initialize the file as read/write because it could be used
	 * with write() to add/remove/change interest sets.
	 */
	file->f_pos = 0;
	file->f_flags = O_RDONLY;
	file->f_op = &eventpoll_fops;
	file->f_mode = FMODE_READ;
	file->f_version = 0;
	file->private_data = NULL;

	/* Install the new setup file into the allocated fd. */
	fd_install(fd, file);

	*efd = fd;
	*einode = inode;
	*efile = file;
	return 0;

eexit_4:
	put_unused_fd(fd);
eexit_3:
	iput(inode);
eexit_2:
	put_filp(file);
eexit_1:
	return error;
}


static int ep_alloc_pages(char **pages, int numpages)
{
	int i;

	for (i = 0; i < numpages; i++) {
		pages[i] = (char *) __get_free_pages(GFP_KERNEL, 0);
		if (!pages[i]) {
			for (--i; i >= 0; i--) {
				ClearPageReserved(virt_to_page(pages[i]));
				free_pages((unsigned long) pages[i], 0);
			}
			return -ENOMEM;
		}
		SetPageReserved(virt_to_page(pages[i]));
	}
	return 0;
}


static int ep_free_pages(char **pages, int numpages)
{
	int i;

	for (i = 0; i < numpages; i++) {
		ClearPageReserved(virt_to_page(pages[i]));
		free_pages((unsigned long) pages[i], 0);
	}
	return 0;
}


static int ep_file_init(struct file *file, unsigned int hashbits)
{
	int error;
	struct eventpoll *ep;

	if (!(ep = kmalloc(sizeof(struct eventpoll), GFP_KERNEL)))
		return -ENOMEM;

	memset(ep, 0, sizeof(*ep));

	error = ep_init(ep, hashbits);
	if (error) {
		kfree(ep);
		return error;
	}

	file->private_data = ep;

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: ep_file_init() ep=%p\n",
		     current, ep));
	return 0;
}


/*
 * Calculate the index of the hash relative to "file".
 */
static unsigned int ep_hash_index(struct eventpoll *ep, struct file *file)
{

	return (unsigned int) hash_ptr(file, ep->hashbits);
}


/*
 * Returns the hash entry ( struct list_head * ) of the passed index.
 */
static struct list_head *ep_hash_entry(struct eventpoll *ep, unsigned int index)
{

	return (struct list_head *) (ep->hpages[index / EP_HENTRY_X_PAGE] +
				     (index % EP_HENTRY_X_PAGE) * sizeof(struct list_head));
}


static int ep_init(struct eventpoll *ep, unsigned int hashbits)
{
	int error;
	unsigned int i, hsize;

	rwlock_init(&ep->lock);
	init_waitqueue_head(&ep->wq);
	init_waitqueue_head(&ep->poll_wait);
	INIT_LIST_HEAD(&ep->rdllist);

	/* Hash allocation and setup */
	ep->hashbits = hashbits;
	error = ep_alloc_pages(ep->hpages, EP_HASH_PAGES(ep->hashbits));
	if (error)
		goto eexit_1;

	/* Initialize hash buckets */
	for (i = 0, hsize = 1 << hashbits; i < hsize; i++)
		INIT_LIST_HEAD(ep_hash_entry(ep, i));

	return 0;
eexit_1:
	return error;
}


static void ep_free(struct eventpoll *ep)
{
	unsigned int i, hsize;
	struct list_head *lsthead, *lnk;

	/*
	 * We need to lock this because we could be hit by
	 * eventpoll_release() while we're freeing the "struct eventpoll".
	 */
	down_write(&epsem);

	/*
	 * Walks through the whole hash by unregistering poll callbacks.
	 */
	for (i = 0, hsize = 1 << ep->hashbits; i < hsize; i++) {
		lsthead = ep_hash_entry(ep, i);

		list_for_each(lnk, lsthead) {
			struct epitem *epi = list_entry(lnk, struct epitem, llink);

			ep_unregister_pollwait(ep, epi);
		}
	}

	/*
	 * Walks through the whole hash by freeing each "struct epitem". At this
	 * point we are sure no poll callbacks will be lingering around, and also by
	 * write-holding "epsem" we can be sure that no file cleanup code will hit
	 * us during this operation. So we can avoid the lock on "ep->lock".
	 */
	for (i = 0, hsize = 1 << ep->hashbits; i < hsize; i++) {
		lsthead = ep_hash_entry(ep, i);

		while (!list_empty(lsthead)) {
			struct epitem *epi = list_entry(lsthead->next, struct epitem, llink);

			ep_remove(ep, epi);
		}
	}

	up_write(&epsem);

	/* Free hash pages */
	ep_free_pages(ep->hpages, EP_HASH_PAGES(ep->hashbits));
}


/*
 * Search the file inside the eventpoll hash. It add usage count to
 * the returned item, so the caller must call ep_release_epitem()
 * after finished using the "struct epitem".
 */
static struct epitem *ep_find(struct eventpoll *ep, struct file *file)
{
	unsigned long flags;
	struct list_head *lsthead, *lnk;
	struct epitem *epi = NULL;

	read_lock_irqsave(&ep->lock, flags);

	lsthead = ep_hash_entry(ep, ep_hash_index(ep, file));
	list_for_each(lnk, lsthead) {
		epi = list_entry(lnk, struct epitem, llink);

		if (epi->file == file) {
			ep_use_epitem(epi);
			break;
		}
		epi = NULL;
	}

	read_unlock_irqrestore(&ep->lock, flags);

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: ep_find(%p) -> %p\n",
		     current, file, epi));

	return epi;
}


/*
 * Increment the usage count of the "struct epitem" making it sure
 * that the user will have a valid pointer to reference.
 */
static void ep_use_epitem(struct epitem *epi)
{

	atomic_inc(&epi->usecnt);
}


/*
 * Decrement ( release ) the usage count by signaling that the user
 * has finished using the structure. It might lead to freeing the
 * structure itself if the count goes to zero.
 */
static void ep_release_epitem(struct epitem *epi)
{

	if (atomic_dec_and_test(&epi->usecnt))
		EPI_MEM_FREE(epi);
}


/*
 * This is the callback that is used to add our wait queue to the
 * target file wakeup lists.
 */
static void ep_ptable_queue_proc(struct file *file, wait_queue_head_t *whead, poll_table *pt)
{
	struct epitem *epi = EP_ITEM_FROM_EPQUEUE(pt);
	struct eppoll_entry *pwq;

	if (epi->nwait >= 0 && (pwq = PWQ_MEM_ALLOC()))
	{
		init_waitqueue_func_entry(&pwq->wait, ep_poll_callback);
		pwq->whead = whead;
		pwq->base = epi;
		add_wait_queue(whead, &pwq->wait);
		list_add_tail(&pwq->llink, &epi->pwqlist);
		epi->nwait++;
	}
	else
	{
		/* We have to signal that an error occurred */
		epi->nwait = -1;
	}
}


static int ep_insert(struct eventpoll *ep, struct epoll_event *event, struct file *tfile)
{
	int error, revents, pwake = 0;
	unsigned long flags;
	struct epitem *epi;
	struct ep_pqueue epq;

	error = -ENOMEM;
	if (!(epi = EPI_MEM_ALLOC()))
		goto eexit_1;

	/* Item initialization follow here ... */
	INIT_LIST_HEAD(&epi->llink);
	INIT_LIST_HEAD(&epi->rdllink);
	INIT_LIST_HEAD(&epi->fllink);
	INIT_LIST_HEAD(&epi->pwqlist);
	epi->ep = ep;
	epi->file = tfile;
	epi->event = *event;
	atomic_set(&epi->usecnt, 1);
	epi->nwait = 0;

	/* Initialize the poll table using the queue callback */
	epq.epi = epi;
	init_poll_funcptr(&epq.pt, ep_ptable_queue_proc);

	/*
	 * Attach the item to the poll hooks and get current event bits.
	 * We can safely use the file* here because its usage count has
	 * been increased by the caller of this function.
	 */
	revents = tfile->f_op->poll(tfile, &epq.pt);

	/*
	 * We have to check if something went wrong during the poll wait queue
	 * install process. Namely an allocation for a wait queue failed due
	 * high memory pressure.
	 */
	if (epi->nwait < 0)
		goto eexit_2;

	/* Add the current item to the list of active epoll hook for this file */
	spin_lock(&tfile->f_ep_lock);
	list_add_tail(&epi->fllink, &tfile->f_ep_links);
	spin_unlock(&tfile->f_ep_lock);

	/* We have to drop the new item inside our item list to keep track of it */
	write_lock_irqsave(&ep->lock, flags);

	/* Add the current item to the hash table */
	list_add(&epi->llink, ep_hash_entry(ep, ep_hash_index(ep, tfile)));

	/* If the file is already "ready" we drop it inside the ready list */
	if ((revents & event->events) && !EP_IS_LINKED(&epi->rdllink)) {
		list_add_tail(&epi->rdllink, &ep->rdllist);

		/* Notify waiting tasks that events are available */
		if (waitqueue_active(&ep->wq))
			wake_up(&ep->wq);
		if (waitqueue_active(&ep->poll_wait))
			pwake++;
	}

	write_unlock_irqrestore(&ep->lock, flags);

	/* We have to call this outside the lock */
	if (pwake)
		ep_poll_safewake(&psw, &ep->poll_wait);

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: ep_insert(%p, %p)\n",
		     current, ep, tfile));

	return 0;

eexit_2:
	ep_unregister_pollwait(ep, epi);

	/*
	 * We need to do this because an event could have been arrived on some
	 * allocated wait queue.
	 */
	write_lock_irqsave(&ep->lock, flags);
	if (EP_IS_LINKED(&epi->rdllink))
		EP_LIST_DEL(&epi->rdllink);
	write_unlock_irqrestore(&ep->lock, flags);

	EPI_MEM_FREE(epi);
eexit_1:
	return error;
}


/*
 * Modify the interest event mask by dropping an event if the new mask
 * has a match in the current file status.
 */
static int ep_modify(struct eventpoll *ep, struct epitem *epi, struct epoll_event *event)
{
	int pwake = 0;
	unsigned int revents;
	unsigned long flags;

	/*
	 * Set the new event interest mask before calling f_op->poll(), otherwise
	 * a potential race might occur. In fact if we do this operation inside
	 * the lock, an event might happen between the f_op->poll() call and the
	 * new event set registering.
	 */
	epi->event.events = event->events;

	/*
	 * Get current event bits. We can safely use the file* here because
	 * its usage count has been increased by the caller of this function.
	 */
	revents = epi->file->f_op->poll(epi->file, NULL);

	write_lock_irqsave(&ep->lock, flags);

	/* Copy the data member from inside the lock */
	epi->event.data = event->data;

	/* If the file is already "ready" we drop it inside the ready list */
	if ((revents & event->events) && EP_IS_LINKED(&epi->llink) &&
	    !EP_IS_LINKED(&epi->rdllink)) {
		list_add_tail(&epi->rdllink, &ep->rdllist);

		/* Notify waiting tasks that events are available */
		if (waitqueue_active(&ep->wq))
			wake_up(&ep->wq);
		if (waitqueue_active(&ep->poll_wait))
			pwake++;
	}

	write_unlock_irqrestore(&ep->lock, flags);

	/* We have to call this outside the lock */
	if (pwake)
		ep_poll_safewake(&psw, &ep->poll_wait);

	return 0;
}


/*
 * This function unregister poll callbacks from the associated file descriptor.
 * Since this must be called without holding "ep->lock" the atomic exchange trick
 * will protect us from multiple unregister.
 */
static void ep_unregister_pollwait(struct eventpoll *ep, struct epitem *epi)
{
	int nwait;
	struct list_head *lsthead = &epi->pwqlist;
	struct eppoll_entry *pwq;

	/* This is called without locks, so we need the atomic exchange */
	nwait = xchg(&epi->nwait, 0);

	if (nwait)
	{
		while (!list_empty(lsthead)) {
			pwq = list_entry(lsthead->next, struct eppoll_entry, llink);

			EP_LIST_DEL(&pwq->llink);
			remove_wait_queue(pwq->whead, &pwq->wait);
			PWQ_MEM_FREE(pwq);
		}
	}
}


/*
 * Unlink the "struct epitem" from all places it might have been hooked up.
 * This function must be called with write IRQ lock on "ep->lock".
 */
static int ep_unlink(struct eventpoll *ep, struct epitem *epi)
{
	int error;

	/*
	 * It can happen that this one is called for an item already unlinked.
	 * The check protect us from doing a double unlink ( crash ).
	 */
	error = -ENOENT;
	if (!EP_IS_LINKED(&epi->llink))
		goto eexit_1;

	/*
	 * At this point is safe to do the job, unlink the item from our list.
	 * This operation togheter with the above check closes the door to
	 * double unlinks.
	 */
	EP_LIST_DEL(&epi->llink);

	/*
	 * If the item we are going to remove is inside the ready file descriptors
	 * we want to remove it from this list to avoid stale events.
	 */
	if (EP_IS_LINKED(&epi->rdllink))
		EP_LIST_DEL(&epi->rdllink);

	error = 0;
eexit_1:

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: ep_unlink(%p, %p) = %d\n",
		     current, ep, epi->file, error));

	return error;
}


/*
 * Removes a "struct epitem" from the eventpoll hash and deallocates
 * all the associated resources.
 */
static int ep_remove(struct eventpoll *ep, struct epitem *epi)
{
	int error;
	unsigned long flags;

	/*
	 * Removes poll wait queue hooks. We _have_ to do this without holding
	 * the "ep->lock" otherwise a deadlock might occur. This because of the
	 * sequence of the lock acquisition. Here we do "ep->lock" then the wait
	 * queue head lock when unregistering the wait queue. The wakeup callback
	 * will run by holding the wait queue head lock and will call our callback
	 * that will try to get "ep->lock".
	 */
	ep_unregister_pollwait(ep, epi);

	/* Remove the current item from the list of epoll hooks */
	spin_lock(&epi->file->f_ep_lock);
	if (EP_IS_LINKED(&epi->fllink))
		EP_LIST_DEL(&epi->fllink);
	spin_unlock(&epi->file->f_ep_lock);

	/* We need to acquire the write IRQ lock before calling ep_unlink() */
	write_lock_irqsave(&ep->lock, flags);

	/* Really unlink the item from the hash */
	error = ep_unlink(ep, epi);

	write_unlock_irqrestore(&ep->lock, flags);

	if (error)
		goto eexit_1;

	/* At this point it is safe to free the eventpoll item */
	ep_release_epitem(epi);

	error = 0;
eexit_1:
	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: ep_remove(%p, %p) = %d\n",
		     current, ep, epi->file, error));

	return error;
}


/*
 * This is the callback that is passed to the wait queue wakeup
 * machanism. It is called by the stored file descriptors when they
 * have events to report.
 */
static int ep_poll_callback(wait_queue_t *wait, unsigned mode, int sync)
{
	int pwake = 0;
	unsigned long flags;
	struct epitem *epi = EP_ITEM_FROM_WAIT(wait);
	struct eventpoll *ep = epi->ep;

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: poll_callback(%p) epi=%p ep=%p\n",
		     current, epi->file, epi, ep));

	write_lock_irqsave(&ep->lock, flags);

	/* If this file is already in the ready list we exit soon */
	if (EP_IS_LINKED(&epi->rdllink))
		goto is_linked;

	list_add_tail(&epi->rdllink, &ep->rdllist);

is_linked:
	/*
	 * Wake up ( if active ) both the eventpoll wait list and the ->poll()
	 * wait list.
	 */
	if (waitqueue_active(&ep->wq))
		wake_up(&ep->wq);
	if (waitqueue_active(&ep->poll_wait))
		pwake++;

	write_unlock_irqrestore(&ep->lock, flags);

	/* We have to call this outside the lock */
	if (pwake)
		ep_poll_safewake(&psw, &ep->poll_wait);

	return 1;
}


static int ep_eventpoll_close(struct inode *inode, struct file *file)
{
	struct eventpoll *ep = file->private_data;

	if (ep) {
		ep_free(ep);
		kfree(ep);
	}

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: close() ep=%p\n", current, ep));
	return 0;
}


static unsigned int ep_eventpoll_poll(struct file *file, poll_table *wait)
{
	unsigned int pollflags = 0;
	unsigned long flags;
	struct eventpoll *ep = file->private_data;

	/* Insert inside our poll wait queue */
	poll_wait(file, &ep->poll_wait, wait);

	/* Check our condition */
	read_lock_irqsave(&ep->lock, flags);
	if (!list_empty(&ep->rdllist))
		pollflags = POLLIN | POLLRDNORM;
	read_unlock_irqrestore(&ep->lock, flags);

	return pollflags;
}


/*
 * Since we have to release the lock during the __copy_to_user() operation and
 * during the f_op->poll() call, we try to collect the maximum number of items
 * by reducing the irqlock/irqunlock switching rate.
 */
static int ep_collect_ready_items(struct eventpoll *ep, struct epitem **aepi, int maxepi)
{
	int nepi;
	unsigned long flags;
	struct list_head *lsthead = &ep->rdllist;

	write_lock_irqsave(&ep->lock, flags);

	for (nepi = 0; nepi < maxepi && !list_empty(lsthead);) {
		struct epitem *epi = list_entry(lsthead->next, struct epitem, rdllink);

		/* Remove the item from the ready list */
		EP_LIST_DEL(&epi->rdllink);

		/*
		 * We need to increase the usage count of the "struct epitem" because
		 * another thread might call EPOLL_CTL_DEL on this target and make the
		 * object to vanish underneath our nose.
		 */
		ep_use_epitem(epi);

		aepi[nepi++] = epi;
	}

	write_unlock_irqrestore(&ep->lock, flags);

	return nepi;
}


/*
 * This function is called without holding the "ep->lock" since the call to
 * __copy_to_user() might sleep, and also f_op->poll() might reenable the IRQ
 * because of the way poll() is traditionally implemented in Linux.
 */
static int ep_send_events(struct eventpoll *ep, struct epitem **aepi, int nepi,
			  struct epoll_event *events)
{
	int i, eventcnt, eventbuf, revents;
	struct epitem *epi;
	struct epoll_event event[EP_MAX_BUF_EVENTS];

	for (i = 0, eventcnt = 0, eventbuf = 0; i < nepi; i++, aepi++) {
		epi = *aepi;

		/* Get the ready file event set */
		revents = epi->file->f_op->poll(epi->file, NULL);

		if (revents & epi->event.events) {
			event[eventbuf] = epi->event;
			event[eventbuf].events &= revents;
			eventbuf++;
			if (eventbuf == EP_MAX_BUF_EVENTS) {
				if (__copy_to_user(&events[eventcnt], event,
						   eventbuf * sizeof(struct epoll_event))) {
					for (; i < nepi; i++, aepi++)
						ep_release_epitem(*aepi);
					return -EFAULT;
				}
				eventcnt += eventbuf;
				eventbuf = 0;
			}
		}

		ep_release_epitem(epi);
	}

	if (eventbuf) {
		if (__copy_to_user(&events[eventcnt], event,
				   eventbuf * sizeof(struct epoll_event)))
			return -EFAULT;
		eventcnt += eventbuf;
	}

	return eventcnt;
}


/*
 * Perform the transfer of events to user space.
 */
static int ep_events_transfer(struct eventpoll *ep, struct epoll_event *events, int maxevents)
{
	int eventcnt, nepi, sepi, maxepi;
	struct epitem *aepi[EP_MAX_COLLECT_ITEMS];

	/*
	 * We need to lock this because we could be hit by
	 * eventpoll_release() while we're transfering
	 * events to userspace. Read-holding "epsem" will lock
	 * out eventpoll_release() during the whole
	 * transfer loop and this will garantie us that the
	 * file will not vanish underneath our nose when
	 * we will call f_op->poll() from ep_send_events().
	 */
	down_read(&epsem);

	for (eventcnt = 0; eventcnt < maxevents;) {
		/* Maximum items we can extract this time */
		maxepi = min(EP_MAX_COLLECT_ITEMS, maxevents - eventcnt);

		/* Collect/extract ready items */
		nepi = ep_collect_ready_items(ep, aepi, maxepi);

		if (nepi) {
			/* Send events to userspace */
			sepi = ep_send_events(ep, aepi, nepi, &events[eventcnt]);
			if (sepi < 0) {
				up_read(&epsem);
				return sepi;
			}
			eventcnt += sepi;
		}

		if (nepi < maxepi)
			break;
	}

	up_read(&epsem);

	return eventcnt;
}


static int ep_poll(struct eventpoll *ep, struct epoll_event *events, int maxevents,
		   long timeout)
{
	int res, eavail;
	unsigned long flags;
	long jtimeout;
	wait_queue_t wait;

	/*
	 * Calculate the timeout by checking for the "infinite" value ( -1 )
	 * and the overflow condition. The passed timeout is in milliseconds,
	 * that why (t * HZ) / 1000.
	 */
	jtimeout = timeout == -1 || timeout > (MAX_SCHEDULE_TIMEOUT - 1000) / HZ ?
		MAX_SCHEDULE_TIMEOUT: (timeout * HZ + 999) / 1000;

retry:
	write_lock_irqsave(&ep->lock, flags);

	res = 0;
	if (list_empty(&ep->rdllist)) {
		/*
		 * We don't have any available event to return to the caller.
		 * We need to sleep here, and we will be wake up by
		 * ep_poll_callback() when events will become available.
		 */
		init_waitqueue_entry(&wait, current);
		add_wait_queue(&ep->wq, &wait);

		for (;;) {
			/*
			 * We don't want to sleep if the ep_poll_callback() sends us
			 * a wakeup in between. That's why we set the task state
			 * to TASK_INTERRUPTIBLE before doing the checks.
			 */
			set_current_state(TASK_INTERRUPTIBLE);
			if (!list_empty(&ep->rdllist) || !jtimeout)
				break;
			if (signal_pending(current)) {
				res = -EINTR;
				break;
			}

			write_unlock_irqrestore(&ep->lock, flags);
			jtimeout = schedule_timeout(jtimeout);
			write_lock_irqsave(&ep->lock, flags);
		}
		remove_wait_queue(&ep->wq, &wait);

		set_current_state(TASK_RUNNING);
	}

	/* Is it worth to try to dig for events ? */
	eavail = !list_empty(&ep->rdllist);

	write_unlock_irqrestore(&ep->lock, flags);

	/*
	 * Try to transfer events to user space. In case we get 0 events and
	 * there's still timeout left over, we go trying again in search of
	 * more luck.
	 */
	if (!res && eavail &&
	    !(res = ep_events_transfer(ep, events, maxevents)) && jtimeout)
		goto retry;

	return res;
}


static int eventpollfs_delete_dentry(struct dentry *dentry)
{

	return 1;
}


static struct inode *ep_eventpoll_inode(void)
{
	int error = -ENOMEM;
	struct inode *inode = new_inode(eventpoll_mnt->mnt_sb);

	if (!inode)
		goto eexit_1;

	inode->i_fop = &eventpoll_fops;

	/*
	 * Mark the inode dirty from the very beginning,
	 * that way it will never be moved to the dirty
	 * list because mark_inode_dirty() will think
	 * that it already _is_ on the dirty list.
	 */
	inode->i_state = I_DIRTY;
	inode->i_mode = S_IRUSR | S_IWUSR;
	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_blksize = PAGE_SIZE;
	return inode;

eexit_1:
	return ERR_PTR(error);
}


static struct super_block *eventpollfs_get_sb(struct file_system_type *fs_type,
					      int flags, char *dev_name, void *data)
{

	return get_sb_pseudo(fs_type, "eventpoll:", NULL, EVENTPOLLFS_MAGIC);
}


static int __init eventpoll_init(void)
{
	int error;

	/* Initialize the semaphore used to syncronize the file cleanup code */
	init_rwsem(&epsem);

	/* Initialize the structure used to perform safe poll wait head wake ups */
	ep_poll_safewake_init(&psw);

	/* Allocates slab cache used to allocate "struct epitem" items */
	error = -ENOMEM;
	epi_cache = kmem_cache_create("eventpoll_epi",
				      sizeof(struct epitem),
				      0,
				      SLAB_HWCACHE_ALIGN | EPI_SLAB_DEBUG, NULL, NULL);
	if (!epi_cache)
		goto eexit_1;

	/* Allocates slab cache used to allocate "struct eppoll_entry" */
	error = -ENOMEM;
	pwq_cache = kmem_cache_create("eventpoll_pwq",
				      sizeof(struct eppoll_entry),
				      0,
				      EPI_SLAB_DEBUG, NULL, NULL);
	if (!pwq_cache)
		goto eexit_2;

	/*
	 * Register the virtual file system that will be the source of inodes
	 * for the eventpoll files
	 */
	error = register_filesystem(&eventpoll_fs_type);
	if (error)
		goto eexit_3;

	/* Mount the above commented virtual file system */
	eventpoll_mnt = kern_mount(&eventpoll_fs_type);
	error = PTR_ERR(eventpoll_mnt);
	if (IS_ERR(eventpoll_mnt))
		goto eexit_4;

	DNPRINTK(3, (KERN_INFO "[%p] eventpoll: successfully initialized.\n", current));

	return 0;

eexit_4:
	unregister_filesystem(&eventpoll_fs_type);
eexit_3:
	kmem_cache_destroy(pwq_cache);
eexit_2:
	kmem_cache_destroy(epi_cache);
eexit_1:

	return error;
}


static void __exit eventpoll_exit(void)
{
	/* Undo all operations done inside eventpoll_init() */
	unregister_filesystem(&eventpoll_fs_type);
	mntput(eventpoll_mnt);
	kmem_cache_destroy(pwq_cache);
	kmem_cache_destroy(epi_cache);
}

module_init(eventpoll_init);
module_exit(eventpoll_exit);

MODULE_LICENSE("GPL");

