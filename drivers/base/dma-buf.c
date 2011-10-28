/*
 * Framework for buffer objects that can be shared across devices/subsystems.
 *
 * Copyright(C) 2011 Linaro Limited. All rights reserved.
 * Author: Sumit Semwal <sumit.semwal@ti.com>
 *
 * Many thanks to linaro-mm-sig list, and specially
 * Arnd Bergmann <arnd@arndb.de>, Rob Clark <rob@ti.com> and
 * Daniel Vetter <daniel@ffwll.ch> for their support in creation and
 * refining of this idea.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/anon_inodes.h>

static inline int is_dma_buf_file(struct file *);

static int dma_buf_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct dma_buf *dmabuf;

	if (!is_dma_buf_file(file))
		return -EINVAL;

	dmabuf = file->private_data;

	if (!dmabuf->ops->mmap)
		return -EINVAL;

	return dmabuf->ops->mmap(dmabuf, vma);
}

static int dma_buf_release(struct inode *inode, struct file *file)
{
	struct dma_buf *dmabuf;

	if (!is_dma_buf_file(file))
		return -EINVAL;

	dmabuf = file->private_data;

	dmabuf->ops->release(dmabuf);
	kfree(dmabuf);
	return 0;
}

static const struct file_operations dma_buf_fops = {
	.mmap		= dma_buf_mmap,
	.release	= dma_buf_release,
};

/*
 * is_dma_buf_file - Check if struct file* is associated with dma_buf
 */
static inline int is_dma_buf_file(struct file *file)
{
	return file->f_op == &dma_buf_fops;
}

/**
 * dma_buf_export - Creates a new dma_buf, and associates an anon file
 * with this buffer,so it can be exported.
 * Also connect the allocator specific data and ops to the buffer.
 *
 * @priv:	[in]	Attach private data of allocator to this buffer
 * @ops:	[in]	Attach allocator-defined dma buf ops to the new buffer.
 * @flags:	[in]	mode flags for the file.
 *
 * Returns, on success, a newly created dma_buf object, which wraps the
 * supplied private data and operations for dma_buf_ops. On failure to
 * allocate the dma_buf object, it can return NULL.
 *
 */
struct dma_buf *dma_buf_export(void *priv, struct dma_buf_ops *ops,
				int flags)
{
	struct dma_buf *dmabuf;
	struct file *file;

	BUG_ON(!priv || !ops);

	dmabuf = kzalloc(sizeof(struct dma_buf), GFP_KERNEL);
	if (dmabuf == NULL)
		return dmabuf;

	dmabuf->priv = priv;
	dmabuf->ops = ops;

	file = anon_inode_getfile("dmabuf", &dma_buf_fops, dmabuf, flags);

	dmabuf->file = file;

	mutex_init(&dmabuf->lock);
	INIT_LIST_HEAD(&dmabuf->attachments);

	return dmabuf;
}
EXPORT_SYMBOL(dma_buf_export);


/**
 * dma_buf_fd - returns a file descriptor for the given dma_buf
 * @dmabuf:	[in]	pointer to dma_buf for which fd is required.
 *
 * On success, returns an associated 'fd'. Else, returns error.
 */
int dma_buf_fd(struct dma_buf *dmabuf)
{
	int error, fd;

	if (!dmabuf->file)
		return -EINVAL;

	error = get_unused_fd_flags(0);
	if (error < 0)
		return error;
	fd = error;

	fd_install(fd, dmabuf->file);

	return fd;
}
EXPORT_SYMBOL(dma_buf_fd);

/**
 * dma_buf_get - returns the dma_buf structure related to an fd
 * @fd:	[in]	fd associated with the dma_buf to be returned
 *
 * On success, returns the dma_buf structure associated with an fd; uses
 * file's refcounting done by fget to increase refcount. returns ERR_PTR
 * otherwise.
 */
struct dma_buf *dma_buf_get(int fd)
{
	struct file *file;

	file = fget(fd);

	if (!file)
		return ERR_PTR(-EBADF);

	if (!is_dma_buf_file(file)) {
		fput(file);
		return ERR_PTR(-EINVAL);
	}

	return file->private_data;
}
EXPORT_SYMBOL(dma_buf_get);

/**
 * dma_buf_put - decreases refcount of the buffer
 * @dmabuf:	[in]	buffer to reduce refcount of
 *
 * Uses file's refcounting done implicitly by fput()
 */
void dma_buf_put(struct dma_buf *dmabuf)
{
	BUG_ON(!dmabuf->file);

	fput(dmabuf->file);

	return;
}
EXPORT_SYMBOL(dma_buf_put);

/**
 * dma_buf_attach - Add the device to dma_buf's attachments list; optionally,
 * calls attach() of dma_buf_ops to allow device-specific attach functionality
 * @dmabuf:	[in]	buffer to attach device to.
 * @dev:	[in]	device to be attached.
 *
 * Returns struct dma_buf_attachment * for this attachment; may return NULL.
 *
 */
struct dma_buf_attachment *dma_buf_attach(struct dma_buf *dmabuf,
						struct device *dev)
{
	struct dma_buf_attachment *attach;
	int ret;

	BUG_ON(!dmabuf || !dev);

	mutex_lock(&dmabuf->lock);

	attach = kzalloc(sizeof(struct dma_buf_attachment), GFP_KERNEL);
	if (attach == NULL)
		goto err_alloc;

	attach->dev = dev;
	if (dmabuf->ops->attach) {
		ret = dmabuf->ops->attach(dmabuf, dev, attach);
		if (!ret)
			goto err_attach;
	}
	list_add(&attach->node, &dmabuf->attachments);

err_alloc:
	mutex_unlock(&dmabuf->lock);
	return attach;
err_attach:
	kfree(attach);
	mutex_unlock(&dmabuf->lock);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(dma_buf_attach);

/**
 * dma_buf_detach - Remove the given attachment from dmabuf's attachments list;
 * optionally calls detach() of dma_buf_ops for device-specific detach
 * @dmabuf:	[in]	buffer to detach from.
 * @attach:	[in]	attachment to be detached; is free'd after this call.
 *
 */
void dma_buf_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attach)
{
	BUG_ON(!dmabuf || !attach);

	mutex_lock(&dmabuf->lock);
	list_del(&attach->node);
	if (dmabuf->ops->detach)
		dmabuf->ops->detach(dmabuf, attach);

	kfree(attach);
	mutex_unlock(&dmabuf->lock);
	return;
}
EXPORT_SYMBOL(dma_buf_detach);
