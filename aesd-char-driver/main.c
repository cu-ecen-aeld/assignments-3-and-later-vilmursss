/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include "aesdchar.h"
#include "aesd-circular-buffer.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/fs.h> // file_operations

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

#define MAX_HISTORY 10

MODULE_AUTHOR("vilmursss"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

struct aesd_circular_buffer aesd_buf;
static char* current_buffer = NULL;
static size_t current_length = 0;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;

    // Allocate memory for the incoming data
    char* kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
    {
        return -ENOMEM;
    }

    // Copy data from user space to kernel space
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    for (size_t i = 0; i < count; i++)
    {
        current_buffer = krealloc(current_buffer, current_length + 1, GFP_KERNEL);
        if (!current_buffer) {
            kfree(kbuf);
            kfree(current_buffer);
            current_buffer = NULL;
            return -ENOMEM;
        }

        current_buffer[current_length] = kbuf[i];
        current_length++;

        if (kbuf[i] == '\n')
        {
            printk(KERN_WARNING "%s: Written word: %s", __func__, current_buffer);
            struct aesd_buffer_entry write_entry = {
                .buffptr = current_buffer, .size = current_length};

            aesd_circular_buffer_add_entry(&aesd_buf, &write_entry);

            current_buffer = NULL;
            current_length = 0;
        }
    }

    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);
    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    
    printk(KERN_WARNING "%s: Aesd-char-driver\n", __func__);
    return result;

}

void aesd_cleanup_module(void)
{
    printk(KERN_WARNING "%s: Aesd-char-driver\n", __func__);
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    for (size_t i = 0; i < MAX_HISTORY; i++) {
        if (aesd_buf.entry[i].buffptr) {
            kfree(aesd_buf.entry[i].buffptr);
        }
    }

    if (current_buffer) {
        kfree(current_buffer);
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
