#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/sched.h>

static struct miscdevice myDevice;
static int buffSize;
char* deviceName = "numpipe";
static int rIndex = 0;
static int wIndex = 0;
static int buffUsed;

static struct semaphore full;
static struct semaphore empty;
static struct semaphore readLock;
static struct semaphore writeLock;

module_param(buffSize, int, 0);

int* buffer;

static int open(struct inode*, struct file*);
static ssize_t read(struct file*, char*, size_t, loff_t*);
static ssize_t write(struct file*, const char*, size_t, loff_t*);
static int release(struct inode*, struct file*);

static struct file_operations fops = {
	.open = &open,
	.read = &read,
	.write = &write,
	.release = &release
};

int init_module(){
	myDevice.name = "numpipe";
	myDevice.minor = MISC_DYNAMIC_MINOR;
	myDevice.fops = &fops;

	int retVal;

	if((retVal = misc_register(&myDevice))){
		printk(KERN_ERR "Could not register the device\n");
		return retVal;
	}

	printk(KERN_INFO "%s registered with buffer size %d\n", deviceName,buffSize);

	int i = 0;
	buffer = (int*)kmalloc(buffSize*sizeof(int), GFP_KERNEL);

	for(;i < buffSize;i++)
		buffer[i] = 0;

	sema_init(&full, 0);
	sema_init(&empty, buffSize);
	sema_init(&readLock, 1);
	sema_init(&writeLock, 1);

	buffUsed = 0;

	return 0;
}

/*function is called when device is opened*/
static int open(struct inode* n, struct file* file){
	return 0;
}

/*function is called when read() is called on the device*/
static ssize_t read(struct file* file, char* uBuffer, size_t length, loff_t* offset){
	int uIndex = 0;
	down_interruptible(&readLock);
	down_interruptible(&full);
	rIndex %= buffSize;
	for(uIndex = 0; uIndex < length; uIndex++)
		if(buffUsed > 0)
			copy_to_user(&uBuffer[0], &buffer[rIndex], 4);

	rIndex++;
	buffUsed--;

	up(&empty);
	up(&readLock);
	return uIndex;
}

/*function called when device is written to*/
static ssize_t write(struct file* file, const char* uBuffer, size_t length, loff_t* offset){
	int uIndex = 0;

	down_interruptible(&writeLock);
	down_interruptible(&empty);
	wIndex %= buffSize;
	for(uIndex = 0; uIndex < length; uIndex++)
		if(buffUsed < buffSize)
			copy_from_user(&buffer[wIndex], &uBuffer[uIndex], 4);
	wIndex++;
	buffUsed++;
	up(&full);
	up(&writeLock);
	return uIndex;
}

/*function that is called when device is closed*/
static int release(struct inode* n, struct file* file){
	return 0;
}

/*function to cleanup the module*/
void cleanup_module(){
	/*freeing memory*/
	int i = 0;
	for(; i < buffSize; i++){
		kfree(buffer[i]);
	}
	misc_deregister(&myDevice);
	printk(KERN_INFO "Device %s Unregistered!\n", deviceName);
}
