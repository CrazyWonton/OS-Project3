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
static DEFINE_SEMAPHORE(full);
static DEFINE_SEMAPHORE(empty);
static DEFINE_MUTEX(mut);
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
        mutex_init(&mut);
        buffUsed = 0;
        return 0;
}

static int open(struct inode* n, struct file* file){
        return 0;
}

static ssize_t read(struct file* file, char* uBuffer, size_t length, loff_t* offset){
        if (down_interruptible(&full)){
                return -EINTR;
        }
        if (mutex_lock_interruptible(&mut)){
                return -EINTR;
        }
        rIndex %= buffSize;
        if(buffUsed > 0)
                if(copy_to_user(uBuffer, &buffer[rIndex], 4)){
                        return -EFAULT;
                }
        rIndex++;
        buffUsed--;
        mutex_unlock(&mut);
        up(&empty);
        return length;
}

static ssize_t write(struct file* file, const char* uBuffer, size_t length, loff_t* offset){
        if (down_interruptible(&empty)){
                return -EINTR;
        }
        if (mutex_lock_interruptible(&mut)){
                return -EINTR;
        }
        wIndex %= buffSize;
        if(buffUsed < buffSize)
                if(copy_from_user(&buffer[wIndex], uBuffer, 4)){
                        return -EFAULT;
                }
        wIndex++;
        buffUsed++;
        mutex_unlock(&mut);
        up(&full);
        return length;
}

static int release(struct inode* n, struct file* file){
        return 0;
}

void cleanup_module(){
        int i = 0;
        for(; i < buffSize; i++){
                kfree(buffer[i]);
        }
        misc_deregister(&myDevice);
        printk(KERN_INFO "Device %s Unregistered!\n", deviceName);
}
