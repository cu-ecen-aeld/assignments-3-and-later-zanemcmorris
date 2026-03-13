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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

#define TEMP_BUFFER_STARTING_SIZE (32)

MODULE_AUTHOR("ZaneMcMorris"); /** TODO: fill in your name - DONE **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     * Check for errors, init driver, update fops if needed, allocate/set private data
     */

    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    mutex_lock(&aesd_device.deviceMutex);
    aesd_device.numUsers += 1;
    mutex_unlock(&aesd_device.deviceMutex);

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

    // Should probably decrement numUsers here...   
    // And maybe set flip to null
    
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev;
    dev = filp->private_data;

    mutex_lock(&dev->deviceMutex);
    size_t entryOffset = 0;
    struct aesd_buffer_entry* entry = NULL;
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->circBufferPtr, *f_pos, &entryOffset);
    if(entry == NULL){
        // something bad or could not find offset within entries.
        PDEBUG("buffer returned null on read. Check is offset is valid");
        retval = 0;
        goto exit_aesd_read;
    }

    size_t bytesToCopy = 0;
    size_t bytesLeftInEntry = entry->size - entryOffset;
    if(bytesLeftInEntry < count){
        bytesToCopy = bytesLeftInEntry; // We can copy the whole entry out but they wanted more

    } else {
        bytesToCopy = count; // Entry is longer than buffer we're allowed to write to
    }


    size_t rc = copy_to_user(buf, entry->buffptr + entryOffset, bytesToCopy);
    if(rc != 0){
        // Did not copy all bytes -- abort....
        PDEBUG("Failed to copy all bytes to user.");
        retval = -EFAULT;
        goto exit_aesd_read;
    }

    *f_pos += bytesToCopy;         
    retval = bytesToCopy;


exit_aesd_read:
    mutex_unlock(&dev->deviceMutex);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle write
     */

    mutex_lock(&aesd_device.deviceMutex);
    if (aesd_device.onGoingWrite == 0)
    {
        // Setup device for new write.
        aesd_device.tempBuffer = kmalloc(TEMP_BUFFER_STARTING_SIZE, GFP_KERNEL);
        if (aesd_device.tempBuffer == NULL)
        {
            PDEBUG("Could not alloc starting size for input buffer in write.");
            mutex_unlock(&aesd_device.deviceMutex);
            return -ENOMEM;
        }
        aesd_device.tempBufferSize = TEMP_BUFFER_STARTING_SIZE;
        aesd_device.tempBufferIndex = 0;
        aesd_device.onGoingWrite = 1; // Until we see a newline we're in an ongoing write.
    }
    // Logic for checking if a resize is needed for tempbuffer
    size_t neededBufferSize = aesd_device.tempBufferIndex + count;
    size_t newBufferSize = aesd_device.tempBufferSize;
    int resizeNeeded = 0;
    while (newBufferSize < neededBufferSize)
    {
        newBufferSize *= 2;
        resizeNeeded = 1;
    }
    // And if resizing is needed, do so.
    if (resizeNeeded == 1)
    {
        char *expandedTempBuffer = krealloc(aesd_device.tempBuffer, newBufferSize, GFP_KERNEL);
        if (expandedTempBuffer == NULL)
        {
            // realloc failed -- no more heap left. die I guess...
            PDEBUG("Could not realloc in write w/ proposed size %zu", newBufferSize);
            kfree(aesd_device.tempBuffer);
            aesd_device.onGoingWrite = 0; // Should refactor resetting of the state to a function
            aesd_device.tempBufferIndex = 0;
            aesd_device.tempBuffer = NULL;
            mutex_unlock(&aesd_device.deviceMutex);
            return -ENOMEM;
        }
        else
        {
            aesd_device.tempBuffer = expandedTempBuffer;
            aesd_device.tempBufferSize = newBufferSize;
        }
    }
    // Now we have an appropriately sized buffer, so copy the data from the user
    int rc = copy_from_user(aesd_device.tempBuffer + aesd_device.tempBufferIndex, buf, count);
    if (rc != 0)
    {
        PDEBUG("Got non-zero return from copy_from_user. Failed w/ rc=%d", rc);
        kfree(aesd_device.tempBuffer);
        aesd_device.onGoingWrite = 0;
        aesd_device.tempBuffer = NULL;
        aesd_device.tempBufferIndex = 0;
        aesd_device.tempBufferSize = 0;
        mutex_unlock(&aesd_device.deviceMutex);
        return -EFAULT;
    }

    retval = count;
    aesd_device.tempBufferIndex += count;
    // Check for a complete message or not, i.e. does this message end with \n
    if (aesd_device.tempBufferIndex > 0 &&
        aesd_device.tempBuffer[aesd_device.tempBufferIndex - 1] == '\n')
    {
        // Last char was a new line, so let's write the message to the circ buffer and quit!
        struct aesd_buffer_entry newCircEntry = {0};
        newCircEntry.buffptr = kmalloc(aesd_device.tempBufferIndex, GFP_KERNEL);
        newCircEntry.size = aesd_device.tempBufferIndex;
        if (newCircEntry.buffptr == NULL)
        {
            PDEBUG("Could not alloc new buffentry in write.");
            kfree(aesd_device.tempBuffer);
            aesd_device.onGoingWrite = 0;
            mutex_unlock(&aesd_device.deviceMutex);
            return -ENOMEM;
        }

        memset(newCircEntry.buffptr, 0, newCircEntry.size);
        memcpy(newCircEntry.buffptr, aesd_device.tempBuffer, newCircEntry.size);
        const char *oldEntryBufPtr = aesd_circular_buffer_add_entry(aesd_device.circBufferPtr, &newCircEntry);

        if (oldEntryBufPtr != NULL)
        {
            // There was an old entry that was removed as a result of adding the new entry.
            PDEBUG("Freeing old entry @ %p w/ string %s", oldEntryBufPtr, oldEntryBufPtr);
            kfree(oldEntryBufPtr);
        }

        aesd_device.onGoingWrite = 0; // Write just finished. Cleanup state and return
        PDEBUG("Write complete! Wrote %s", newCircEntry.buffptr);
        kfree(aesd_device.tempBuffer);
        aesd_device.tempBuffer = NULL;
        aesd_device.tempBufferIndex = 0;
        aesd_device.tempBufferSize = 0;
    }

    mutex_unlock(&aesd_device.deviceMutex);

    *f_pos += retval;
    return retval;
}

loff_t aesd_llseek(struct file* filp, loff_t offset, int whence){

    PDEBUG("llseek with offset %d and whence %d", offset, whence);

    struct aesd_dev *dev;
    dev = filp->private_data;

    if (whence != SEEK_SET || whence != SEEK_CUR || whence != SEEK_END){
        return -EINVAL;
    }

    loff_t proposedOffset = 0;

    switch (whence) {
    case SEEK_SET:
        /* offset is absolute */
        proposedOffset = offset;
        break;
    case SEEK_CUR:
        /* offset is relative to current file position */
        proposedOffset = offset + filp->f_pos;
        break;
    case SEEK_END:
        /* offset is relative to end of file/device data */
        // proposedOffset = size_of_buffer + offset (offset could be < 0)
        // We need to find bytes within our circ buffer before we can figure out the return value.
        int numBytes = 0;
        mutex_lock(&dev->deviceMutex);
        for(int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
            // Loop through all the entries
            if(dev->circBufferPtr->entry[i].buffptr != NULL){
                // This feels a little dubious because size MIGHT NOT be assigned correctly\
                to the buffer length in all cases. It should be good, but might be a bug
                numBytes += dev->circBufferPtr->entry[i].size;
            }
        }
        mutex_unlock(&dev->deviceMutex);

        proposedOffset = offset + numBytes;

        break;
    default:
        return -EINVAL;
    }

    // set filp->f_pos to new postion
    // return that value as well.
    filp->f_pos = proposedOffset;
    return proposedOffset;

}


struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,

};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
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
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.deviceMutex);
    aesd_device.circBufferPtr = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    if (aesd_device.circBufferPtr == NULL)
    {
        PDEBUG("Could not allocate memory for circ buffer in module init");
    }
    aesd_circular_buffer_init(aesd_device.circBufferPtr);

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        // Failure to setup cdev
        unregister_chrdev_region(dev, 1);
        kfree(aesd_device.circBufferPtr);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    for(int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
        if(aesd_device.circBufferPtr->entry[i].buffptr != NULL)
        {
            kfree(aesd_device.circBufferPtr->entry[i].buffptr);
            aesd_device.circBufferPtr->entry[i].buffptr = NULL;
        }
                
    }
    kfree(aesd_device.circBufferPtr);
    // I need to free circcuff entries here too. I should have a memory leak right now
    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
