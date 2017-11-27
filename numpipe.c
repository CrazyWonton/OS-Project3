#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/uaccess.h>


static int sample_open(struct inode *inode, struct file *file)
{
	pr_info("I have been awoken\n");
	return 0;
}

static int sample_close(struct inode *inodep, struct file *filp)
{
	pr_info("Sleepy time\n");
	return 0;
}

static ssize_t sample_write(struct file *file, const char __user *buf,
		size_t len, loff_t *ppos)
{
	pr_info("Yummy - I just ate %d bytes\n", len);
	return len; /* But we don't actually do anything with the data */
}

static ssize_t dev_read(struct file *filp, /* see include/linux/fs.h   */
		char *buffer,      /* buffer to fill with data */
		size_t length,     /* length of the buffer     */
		loff_t *offset){
	/*
	 * Number of bytes actually written to the buffer
	 */
	int bytes_read = 0;
	/*
	 * If we're at the end of the message, return 0 signifying end of file.
	 */
	//if (*msg_Ptr == 0)
	//	return 0;

	printk("AT least got here\n");

	struct timespec time1;
	time1 = current_kernel_time();
	struct timespec time2;
	getnstimeofday(&time2);

	bytes_read = sprintf(buffer, "current_kernel_time: %lu %lu \n getnstimeofday: %lu %lu",time1.tv_sec, time1.tv_nsec, time2.tv_sec, time2.tv_nsec);	

	/*
	 * Actually put the data into the buffer
	 */
	//while (length && *msg_Ptr) {
	/*
	 * The buffer is in the user data segment, not the kernel segment so "*"
	 * assignment won't work. We have to use put_user which copies data from the
	 * kernel data segment to the user data segment.
	 */
	//put_user(*(msg_Ptr++), buffer++);
	//length--;
	//bytes_read++;
	//}

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}
static const struct file_operations sample_fops = {
	.owner			= THIS_MODULE,
	.write			= sample_write,
	.open			= sample_open,
	.release		= sample_close,
	.llseek 		= no_llseek,
	.read			= dev_read,
};
struct miscdevice sample_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "numpipe",
	.fops = &sample_fops,
};

static int __init misc_init(void)
{
	int error;

	error = misc_register(&sample_device);
	if (error) {
		pr_err("can't misc_register :(\n");
		return error;
	}

	printk("Alright I'm in the computer now\n");
	return -1;
}

static void __exit misc_exit(void)
{
	misc_deregister(&sample_device);
	pr_info("I'm out\n");
}




	module_init(misc_init)
module_exit(misc_exit)
