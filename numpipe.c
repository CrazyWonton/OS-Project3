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
static DEFINE_SEMAPHORE(full);  //Declare and initialize semaphore to 1
static DEFINE_SEMAPHORE(empty);  //Declare and initialize semaphore to 1
static DEFINE_MUTEX(mut);       //initialize mutex as unlocked
module_param(buffSize, int, 0); //perameter passed in with insmod
int* buffer;

static int open(struct inode*, struct file*);
static ssize_t read(struct file*, char*, size_t, loff_t*);
static ssize_t write(struct file*, const char*, size_t, loff_t*);
static int release(struct inode*, struct file*);

//available operations for this device
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
        if((retVal = misc_register(&myDevice))){ //return > 0 indicates failure
                printk(KERN_ERR "Could not register the device\n");
                return retVal;
        }
        printk(KERN_INFO "%s registered with buffer size %d\n", deviceName,buffSize);
        int i = 0;
        buffer = (int*)kmalloc(buffSize*sizeof(int), GFP_KERNEL);  //allocate buffSize bytes to normal kernel memory
        for(;i < buffSize;i++)
                buffer[i] = 0;  //scrub data
        sema_init(&full, 0);   //semaphore initialized to 0
        sema_init(&empty, buffSize); //semaphore initialized to buffSize
        mutex_init(&mut); //mutex initialized as unlocked again
        buffUsed = 0;  //amount of buffer used is 0
        return 0;
}

static int open(struct inode* n, struct file* file){
        return 0;
}

static ssize_t read(struct file* file, char* uBuffer, size_t length, loff_t* offset){
        //down decrements the semaphore and waits as long as it needs to
        //down interruptible is the same except interuptable, allows the user to interrupt process
        //      that is waiting on the semaphore
        //error check must exist because we are using down_inter.., if interrupted returns !0 value
        if (down_interruptible(&full)){  //first check if it can gain access to read from the buffer
                return -EINTR;
        }
        if (mutex_lock_interruptible(&mut)){ //then try to gain access to this mutex
                return -EINTR;
        }
        //critical section
        rIndex %= buffSize;
        if(buffUsed > 0)
                //
                if(copy_to_user(uBuffer, &buffer[rIndex], 4)){
                        return -EFAULT;
                }
        rIndex++;
        buffUsed--;
        mutex_unlock(&mut); //unlocks section
        up(&empty); //
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
