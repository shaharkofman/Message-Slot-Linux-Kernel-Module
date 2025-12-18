#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "message_slot.h"

MODULE_LICENSE("GPL");

//channel struct for holding message data of a single channel
typedef struct channel
{
    unsigned int id; //the channel's id
    char message[BUF_LEN]; //the message, limited to 128 bytes
    int message_len; //actual length of the message
    struct channel *next; //a pointer to the next active channel (in the same slot/device)
} channel_t;

//slot struct that represents the physical device file
typedef struct slot
{
    unsigned int minor; // this is the unique identifier of a slot
    channel_t *channel_list; //pointer to the head of the channel linked list
    struct slot *next;
} slot_t;

//data state struct to be held in file->private_data
typedef struct data_state
{
    unsigned int channel_id; //the current channel
    int censorship;
} data_state_t;

static slot_t *slot_list_head = NULL;

// a helper function to find a slot in out slot list
// if the minor is found, return the slot that owns it, else return NULL
static slot_t* get_slot(unsigned int minor)
{
    slot_t *curr = slot_list_head;
    while (curr != NULL) 
    {
        if (curr->minor == minor) {return curr;}
        curr = curr->next;
    }
    return NULL;

}

static int device_open(struct inode* inode, struct file* file)
{
    unsigned int minor;
    slot_t *slot;
    data_state_t *state;

    //get minor numberr and find the right slot
    minor = iminor(inode);
    slot = get_slot(minor);
    //it is possible that the slot doesnt exist if its the first time
    //in this case, create it
    if (slot == NULL)
    {
        slot = (slot_t*)kmalloc(sizeof(slot_t), GFP_KERNEL);
        if (slot == NULL) {return -ENOMEM;}
        slot->minor = minor;
        slot->channel_list = NULL;
        //add the slot to the slot linked list
        slot->next = slot_list_head;
        slot_list_head = slot;
    }

    //allocate and initialize the private_data pointer that is passed along the functions
    //this pointer will be set to a data_state struct that holds channel id and censorship commands
    state = (data_state_t*)kmalloc(sizeof(data_state_t), GFP_KERNEL);
    if (state == NULL) {return -ENOMEM;}
    //the following attributes can be changed in device_ioctl
    state->channel_id = 0;
    state->censorship = 0;
    file->private_data = (void*)state;

    return SUCCESS;
}

static int device_release(struct inode* inode, struct file* file)
{
    //the only memory to clean here is the memory we allocated in device_open
    if (file->private_data != NULL)
    {
        kfree(file->private_data);
    }
    return SUCCESS;
}

static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{
    data_state_t *state;
    unsigned int new_channel_id;
    unsigned int new_cen;

    //get the current state of the file
    state = (data_state_t*)file->private_data;

    //case: change the slot channel to ioctl_param
    if (ioctl_command_id == MSG_SLOT_CHANNEL)
    {
        new_channel_id = (unsigned int)ioctl_param;
        if (new_channel_id == 0) {return -EINVAL;} //cant set channel id to 0
        state->channel_id = new_channel_id;
        return SUCCESS;
    }

    //case: change censorship mode to ioctl_param (which is 0 or 1)
    if (ioctl_command_id == MSG_SLOT_SET_CEN)
    {
        new_cen = (unsigned int)ioctl_param;
        if (new_cen != 0 && new_cen != 1) {return -EINVAL;}
        state->censorship = (int)new_cen;
        return SUCCESS;
    }
    return -EINVAL;
}

static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset)
{
    data_state_t *state;
    unsigned int minor;
    slot_t *slot;
    channel_t *curr;
    channel_t *target_channel = NULL;
    long status;

    //get the data state and assert that the channel id is set
    state= (data_state_t*)file->private_data;
    if (state->channel_id == 0) {return -EINVAL;}
    //check that the buffer pointer is not null
    if (buffer == NULL) {return -EINVAL;}

    //get the slot
    minor = iminor(file_inode(file));
    slot = get_slot(minor);
    if (slot == NULL) {return -EINVAL;}

    //search the channel list for the target channel id
    curr = slot->channel_list;
    target_channel = NULL;
    while (curr != NULL) 
    {
        if (curr->id == state->channel_id)
        {
            target_channel = curr;
            break;
        }
        curr = curr->next;   
    }
    //if no channel is found, then no one has written to it (different from write)
    //in that case, return -EWOULDBLOCK
    //do the same thing in case of empty message
    if (target_channel == NULL || target_channel->message_len == 0) {return -EWOULDBLOCK;}
    //if the provided buffer is too small return -ENOSPC
    if (length < target_channel->message_len) {return -ENOSPC;}

    //copy the data with copy_to_user
    status = copy_to_user(buffer, target_channel->message, target_channel->message_len);
    if (status != 0) {return -EFAULT;}

    return target_channel->message_len;
}

static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset)
{
    data_state_t *state;
    unsigned int minor;
    slot_t *slot;
    channel_t *curr;
    channel_t *target_channel = NULL;
    long status;
    int i;

    //check that a channel has been set
    state = (data_state_t*)file->private_data;
    if (state->channel_id == 0) {return -EINVAL;}

    //check that 0 < length <= BUF_LEN
    if ((int) length <= 0 || (int) length > BUF_LEN) {return -EMSGSIZE;}

    //check that the buffer pointer is not null
    if (buffer == NULL) {return -EINVAL;}

    //locate the message channel
    //call get_slot to find the correct message slot 
    minor = iminor(file_inode(file));
    slot = get_slot(minor);
    if (slot == NULL) {return -EINVAL;}

    //search the channel list for the target channel id
    curr = slot->channel_list;
    target_channel = NULL;
    while (curr != NULL) 
    {
        if (curr->id == state->channel_id)
        {
            target_channel = curr;
            break;
        }
        curr = curr->next;
    }
    //if the channel isn't found, create it
    if (target_channel == NULL)
    {
        //allocate and initialize channel
        target_channel = (channel_t*)kmalloc(sizeof(channel_t), GFP_KERNEL);
        if (target_channel == NULL) {return -ENOMEM;}
        target_channel->id = state->channel_id;
        target_channel->message_len = 0;
        //add to the head of the channel list
        target_channel->next = slot->channel_list;
        slot->channel_list = target_channel;
    }

    //call copy_from_user to get the message
    status = copy_from_user(target_channel->message, buffer, length);
    if (status != 0) {return -EFAULT;}
    target_channel->message_len = length;

    //apply censorship if needed
    if (state->censorship == 1)
    {
        for (i = 0; i < target_channel->message_len; ++i)
        {
            if (i % 4 == 3) //this is the 'every 4th character' condition
            {
                target_channel->message[i] = '#';
            }
        }
    }

    return target_channel->message_len;
}


struct file_operations fops = 
{
    .owner = THIS_MODULE, 
    .read = device_read, 
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,
};

static int __init init(void)
{
    int rc = -1;

    rc = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);
    if (rc < 0)
    {
        printk(KERN_ERR "%s registration failed for %d\n", DEVICE_NAME, MAJOR_NUM);
        return rc;
    }
    printk("Registeration is successful. ");
    return 0;
}

static void __exit cleanup(void)
{
    //we need to free every single slot struct
    //inside every slot struct, we need to free every single channel struct
    slot_t *curr_slot = slot_list_head;
    slot_t *next_slot;
    channel_t *curr_channel;
    channel_t *next_channel;

    while (curr_slot != NULL)
    {
        curr_channel = curr_slot->channel_list;
        while (curr_channel != NULL) 
        {
            next_channel = curr_channel->next;
            kfree(curr_channel);
            curr_channel = next_channel;
        }
        next_slot = curr_slot->next;
        kfree(curr_slot);
        curr_slot = next_slot;
    }
    //unregister the device
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

module_init(init);
module_exit(cleanup);
