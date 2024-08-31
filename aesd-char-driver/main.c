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
#include "aesd_ioctl.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/slab.h>
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

loff_t aesd_llseek(struct file *file, loff_t offset, int whence) {
    loff_t new_pos = 0;
    size_t buff_size = 0;
    size_t write_cmd_size = 0;

    buff_size = aesd_circular_buffer_size(&aesd_buf);
    write_cmd_size = aesd_circular_write_cmd_size(&aesd_buf);

    switch (whence) {
        case SEEK_SET:
        {
            if (offset < 0 || offset > buff_size)
            {
                return -EINVAL;
            }
            new_pos = offset;
            break;
        }
        case SEEK_CUR:
        {
            if (file->f_pos + offset < 0 || file->f_pos + offset > buff_size)
            {
                return -EINVAL;
            }
            new_pos = file->f_pos + offset;
            break;
        }
        case SEEK_END:
        {
            if (buff_size + offset < 0 || buff_size + offset > buff_size) {
                return -EINVAL;
            }
            new_pos = buff_size + offset;
            break;
        }
        default:
            return -EINVAL;
    }

    if (new_pos < 0 || write_cmd_size >= MAX_HISTORY)
    {
        return -EINVAL;
    }

    file->f_pos = new_pos;
    return new_pos;
}

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

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    size_t entry_offset_byte;
    size_t bytes_read = 0;
    struct aesd_buffer_entry *entry;

    if (mutex_lock_interruptible(&aesd_device.mutex)) {
        return -ERESTARTSYS;
    }

    // Find the starting entry and offset within the entry
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_buf, *f_pos, &entry_offset_byte);
    if (!entry) {
        mutex_unlock(&aesd_device.mutex);
        return 0; // No data available, indicating EOF
    }

    // Copy data to user buffer
    while (entry && bytes_read < count) {
        size_t copy_size = entry->size - entry_offset_byte;
        if (copy_size > count - bytes_read) {
            copy_size = count - bytes_read;
        }

        if (copy_to_user(buf + bytes_read, entry->buffptr + entry_offset_byte, copy_size)) {
            mutex_unlock(&aesd_device.mutex);
            return -EFAULT;
        }

        bytes_read += copy_size;
        *f_pos += copy_size;

        // Move to the next entry
        entry_offset_byte = 0;
        entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_buf, *f_pos, &entry_offset_byte);
    }

    mutex_unlock(&aesd_device.mutex);
    return bytes_read;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    char* kbuf = NULL;

    // Allocate memory for the incoming data
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
    {
        return -ENOMEM;
    }

    // Copy data from user space to kernel space
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    // Lock the mutex
    if (mutex_lock_interruptible(&aesd_device.mutex)) {
        kfree(kbuf);
        return -ERESTARTSYS;
    }

    for (size_t i = 0; i < count; i++)
    {
        char *new_buffer = krealloc(current_buffer, current_length + 1, GFP_KERNEL);
        if (!new_buffer) {
            kfree(kbuf);
            mutex_unlock(&aesd_device.mutex);
            return -ENOMEM;
        }

        current_buffer = new_buffer;
        current_buffer[current_length] = kbuf[i];
        current_length++;
        retval++;

        if (kbuf[i] == '\n')
        {
            struct aesd_buffer_entry write_entry = {
                .buffptr = current_buffer, .size = current_length};

            aesd_circular_buffer_add_entry(&aesd_buf, &write_entry);

            current_buffer = NULL;
            current_length = 0;
        }
    }

    mutex_unlock(&aesd_device.mutex);
    kfree(kbuf);

    return retval;
}

long aesd_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct aesd_seekto seekto;
    loff_t new_pos = 0;
    size_t write_cmd_size = 0;

    write_cmd_size = aesd_circular_write_cmd_size(&aesd_buf);

    switch (cmd) {
        case AESDCHAR_IOCSEEKTO:
            if (copy_from_user(&seekto, (struct aesd_seekto __user *) arg, sizeof(seekto)))
            {
                return -EFAULT;
            }

            // Validate the command and offset
            if (seekto.write_cmd >= MAX_HISTORY || seekto.write_cmd >= write_cmd_size)
            {
                return -EINVAL;
            }
            
            if (aesd_buf.entry[seekto.write_cmd].size < seekto.write_cmd_offset)
            {
                return -EINVAL;
            }

            // Calculate the new file position based on the command and offset
            new_pos = aesd_circular_calculate_cmd_offset(&aesd_buf, seekto.write_cmd);
            new_pos += seekto.write_cmd_offset;
            break;
        default:
            return -ENOTTY;
    }

    file->f_pos = new_pos;
    return new_pos;
}


struct file_operations aesd_fops = {
    .owner          = THIS_MODULE,
    .read           = aesd_read,
    .write          = aesd_write,
    .open           = aesd_open,
    .release        = aesd_release,
    .llseek         = aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);
    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev\n", err);
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
    
    aesd_circular_buffer_init(&aesd_buf);

    return result;

}

void aesd_cleanup_module(void)
{
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
