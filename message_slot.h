#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#include <linux/ioctl.h>

//major device number that was specified in the assignment
#define MAJOR_NUM 235 

//set the unique numbers of the ioctl commands
//_IOW makes sure the numbers are unique and avoids collisions with other drivers
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)
#define MSG_SLOT_SET_CEN _IOW(MAJOR_NUM, 1, unsigned int)

//set the device name for registration
#define DEVICE_NAME "message_slot"

//buffer length
#define BUF_LEN 128

#define SUCCESS 0

#endif