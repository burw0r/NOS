/*
 * shofer.c -- module implementation
 *
 * Example module which creates a virtual device driver.
 * Circular buffer (kfifo) is used to store received data (with write) and
 * reply with them on read operation.
 *
 * Copyright (C) 2021 Leonardo Jelenkovic
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form.
 * No warranty is attached.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/log2.h>

#include "config.h"


/* Max number of messages */
int MAX_MSG_NUM;
module_param(MAX_MSG_NUM, int, S_IRUGO);

/* Max message size */
int MAX_MSG_SIZE;
module_param(MAX_MSG_SIZE, int, S_IRUGO);

/* Max threads/procs allowed to use msgqueue*/
int MAX_THREAD_NUM;
module_param(MAX_THREAD_NUM, int, S_IRUGO);

/* Buffer size */
static int buffer_size = BUFFER_SIZE;

/* Parameter buffer_size can be given at module load time */
module_param(buffer_size, int, S_IRUGO);
MODULE_PARM_DESC(buffer_size, "Buffer size in bytes, must be a power of 2");

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

struct shofer_dev *Shofer = NULL;
struct buffer *Buffer = NULL;
static dev_t Dev_no = 0;

/* prototypes */
static struct buffer *buffer_create(size_t, int *);
static void buffer_delete(struct buffer *);
static struct shofer_dev *shofer_create(dev_t, struct file_operations *,
	struct buffer *, int *);
static void shofer_delete(struct shofer_dev *);
static void cleanup(void);
static void dump_buffer(struct buffer *);

static int shofer_open(struct inode *, struct file *);
static int shofer_release(struct inode *, struct file *);
static ssize_t shofer_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t shofer_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations shofer_fops = {
	.owner =    THIS_MODULE,
	.open =     shofer_open,
	.release =  shofer_release,
	.read =     shofer_read,
	.write =    shofer_write
};

/* init module */
static int __init shofer_module_init(void)
{
	int retval;
	struct buffer *buffer;
	struct shofer_dev *shofer;
	dev_t dev_no = 0;

	printk(KERN_NOTICE "Module 'shofer' started initialization\n");
	LOG("[+] MAX_MSG_NUM     = %d\n", MAX_MSG_NUM);
	LOG("[+] MAX_MSG_SIZE    = %d\n", MAX_MSG_SIZE);
	LOG("[+] MAX_THREAD_NUM  = %d\n", MAX_THREAD_NUM);

	/* get device number(s) */
	retval = alloc_chrdev_region(&dev_no, 0, 1, DRIVER_NAME);
	if (retval < 0) {
		printk(KERN_WARNING "%s: can't get major device number %d\n",
			DRIVER_NAME, MAJOR(dev_no));
		return retval;
	}

	/* create a buffer */
	/* buffer size must be a power of 2 */
	if (!is_power_of_2(buffer_size))
		buffer_size = roundup_pow_of_two(buffer_size);
	buffer = buffer_create(buffer_size, &retval);
	if (!buffer)
		goto no_driver;
	Buffer = buffer;

	/* create a device */
	shofer = shofer_create(dev_no, &shofer_fops, buffer, &retval);
	if (!shofer)
		goto no_driver;

	printk(KERN_NOTICE "Module 'shofer' initialized with major=%d, minor=%d\n",
		MAJOR(dev_no), MINOR(dev_no));

	Shofer = shofer;
	Dev_no = dev_no;

	return 0;

no_driver:
	cleanup();

	return retval;
}

static void cleanup(void) {
	if (Shofer)
		shofer_delete(Shofer);
	if (Buffer)
		buffer_delete(Buffer);
	if (Dev_no)
		unregister_chrdev_region(Dev_no, 1);
}

/* called when module exit */
static void __exit shofer_module_exit(void)
{
	printk(KERN_NOTICE "Module 'shofer' started exit operation\n");
	cleanup();
	printk(KERN_NOTICE "Module 'shofer' finished exit operation\n");
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);

/* Create and initialize a single buffer */
static struct buffer *buffer_create(size_t size, int *retval)
{
	struct buffer *buffer = kmalloc(sizeof(struct buffer) + size, GFP_KERNEL);
	if (!buffer) {
		*retval = -ENOMEM;
		printk(KERN_NOTICE "shofer:kmalloc failed\n");
		return NULL;
	}
	*retval = kfifo_init(&buffer->fifo, buffer + 1, size);
	if (*retval) {
		kfree(buffer);
		printk(KERN_NOTICE "shofer:kfifo_init failed\n");
		return NULL;
	}
	mutex_init(&buffer->lock);
	*retval = 0;

	return buffer;
}

static void buffer_delete(struct buffer *buffer)
{
	kfree(buffer);
}

/* Create and initialize a single shofer_dev */
static struct shofer_dev *shofer_create(dev_t dev_no, struct file_operations *fops, struct buffer *buffer, int *retval)
{
	struct shofer_dev *shofer = kmalloc(sizeof(struct shofer_dev), GFP_KERNEL);
	if (!shofer){
		*retval = -ENOMEM;
		printk(KERN_NOTICE "shofer:kmalloc failed\n");
		return NULL;
	}
	memset(shofer, 0, sizeof(struct shofer_dev));
	shofer->buffer = buffer;

	cdev_init(&shofer->cdev, fops);
	shofer->cdev.owner = THIS_MODULE;
	shofer->cdev.ops = fops;

	/*keep track of how many threads use msgq*/
	shofer->thread_cnt = 0;  


	/*keep track of how many messages are in  msgq*/
	shofer->msg_cnt = 0;

	*retval = cdev_add (&shofer->cdev, dev_no, 1);
	shofer->dev_no = dev_no;
	if (*retval) {
		printk(KERN_NOTICE "Error (%d) when adding device shofer\n",
			*retval);
		kfree(shofer);
		shofer = NULL;
	}

	return shofer;
}
static void shofer_delete(struct shofer_dev *shofer)
{
	cdev_del(&shofer->cdev);
	kfree(shofer);
}

/* Called when a process calls "open" on this device */
static int shofer_open(struct inode *inode, struct file *filp)
{

	struct shofer_dev *shofer; /* device information */

	LOG("####### OPEN #######");

	shofer = container_of(inode->i_cdev, struct shofer_dev, cdev);
	filp->private_data = shofer; /* for other methods */

	if(shofer->thread_cnt == MAX_THREAD_NUM ){
		LOG("[+] Error: maximum number of threads/procs reached");
		return -1;
	}
	shofer->thread_cnt += 1;   
	
	if ( ((filp->f_flags & O_ACCMODE) != O_RDONLY) && (((filp->f_flags & O_ACCMODE) != O_WRONLY)) ){
		LOG("[+] incorrect file open flag, only O_WRONLY and O_RDONLY allowed");
		return -1;
		// return -EPERM;
	}


	return 0;
}

/* Called when a process performs "close" operation */
static int shofer_release(struct inode *inode, struct file *filp)
{

	struct shofer_dev *shofer; /* device information */

	LOG("####### RELEASE #######");

	shofer = container_of(inode->i_cdev, struct shofer_dev, cdev);
	filp->private_data = shofer; /* for other methods */

	if(shofer->thread_cnt == 0 ){
		/* shouldn't happen */
		LOG("[+] Error: threac_cnt = 0 ");
		return -1;
	}
	shofer->thread_cnt -= 1;   
	return 0; /* nothing to do; could not set this function in fops */
}

/* Read count bytes from buffer to user space ubuf */
static ssize_t shofer_read(struct file *filp, char __user *ubuf, size_t count,
	loff_t *f_pos /* ignoring f_pos */)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	unsigned int copied;


	LOG("####### READ #######");


	// LOG("illegal read uvjeti : ");
	// LOG("	filp->f_flags & O_ACCMODE = %d    ", (filp->f_flags & O_ACCMODE));
	// LOG("	O_WRONLY = %d\n ", O_WRONLY);


	if ( ((filp->f_flags & O_ACCMODE) == O_WRONLY)) {
		LOG("[+] Cannot read from a device opened as write only (O_WRONLY flag) ");
		return -1;
		// return -EPERM;
	}

	if(shofer->msg_cnt == 0){
		LOG("[+] Error: minimum number of messages(0) in message queue reached");
		return -1;
	}

	if (mutex_lock_interruptible(&buffer->lock))
		return -ERESTARTSYS;

	dump_buffer(buffer);

	retval = kfifo_to_user(fifo, (char __user *) ubuf, count, &copied);
	if (retval)
		printk(KERN_NOTICE "shofer:kfifo_to_user failed\n");
	else
		retval = copied;

	dump_buffer(buffer);

	/* reading from devices simulates reading message from msgq*/
	shofer->msg_cnt-=1;
	LOG("[+] Number of messages in msgq = %d", shofer->msg_cnt);

	mutex_unlock(&buffer->lock);

	return retval;
}

/* Write count bytes from user space ubuf to buffer */
static ssize_t shofer_write(struct file *filp, const char __user *ubuf,
	size_t count, loff_t *f_pos /* ignoring f_pos */)
{
	ssize_t retval = 0;
	struct shofer_dev *shofer = filp->private_data;
	struct buffer *buffer = shofer->buffer;
	struct kfifo *fifo = &buffer->fifo;
	unsigned int copied;

	LOG("####### WRITE #######");

	// LOG("illegal write uvjeti : ");
	// LOG("	filp->f_flags & O_ACCMODE = %d", (filp->f_flags & O_ACCMODE));
	// LOG("	O_RDONLY = %d\n ", O_RDONLY);

	if ( ((filp->f_flags & O_ACCMODE) == O_RDONLY)) {
		LOG("[+] Cannot write to a device opened as read only (O_RDONLY flag) ");
		return -1;
		// return -EPERM;
	}

	if(shofer->msg_cnt == MAX_MSG_NUM){
		LOG("\n\n\n\nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAaa\n\n\n");
		LOG("[+] Error: maximum number of messages(%d) in message queue reached", MAX_MSG_NUM);
		return -1;
	}

	if (mutex_lock_interruptible(&buffer->lock))
		return -ERESTARTSYS;

	dump_buffer(buffer);

	retval = kfifo_from_user(fifo, (char __user *) ubuf, count, &copied);
	// LOG("\n\n\n++++++++++++++++++++++------------> %s\n\n\n", ubuf);
	if (retval)
		printk(KERN_NOTICE "shofer:kfifo_from_user failed\n");
	else
		retval = copied;

	dump_buffer(buffer);

	/* writing to devices simulates sending message to msgq*/
	shofer->msg_cnt+=1;
	LOG("[+] Number of messages in msgq = %d", shofer->msg_cnt);

	mutex_unlock(&buffer->lock);

	return retval;
}

static void dump_buffer(struct buffer *b)
{
	char buf[BUFFER_SIZE];
	size_t copied;

	memset(buf, 0, BUFFER_SIZE);
	copied = kfifo_out_peek(&b->fifo, buf, BUFFER_SIZE);

	printk(KERN_NOTICE "shofer:buffer:size=%u:contains=%u:buf=%s\n",
		kfifo_size(&b->fifo), kfifo_len(&b->fifo), buf);
}
