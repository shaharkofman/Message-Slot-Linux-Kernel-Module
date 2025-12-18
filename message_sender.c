#include "message_slot.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Argument count must be exactly 5, including program name.\n");
        exit(1);
    }
    //parse arguments
    char *path = argv[1];
    int channel_id = atoi(argv[2]);
    int censorship = atoi(argv[3]);
    char *message = argv[4];

    if (channel_id < 0)
    {
        fprintf(stderr, "Channel id must be non negative.");
        exit(1);
    }

    //open the device
    int fd = open(path, O_WRONLY);
    if (fd < 0)
    {
        perror("Error - couldn't open the device at given path.");
        exit(1);
    }

    //set censorship with ioctl command
    int ret1 = ioctl(fd, MSG_SLOT_SET_CEN, censorship);
    if (ret1 < 0)
    {
        perror("Error - couldn't set the given censorship.");
        exit(1);
    }

    //set the cannel with ioctl as well
    int ret2 = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    if (ret2 < 0)
    {
        perror("Error - couldn't set the given channel id.");
        exit(1);
    }

    //get message length and write the message
    int length = strlen(message);
    int bytes_wrote = write(fd, message, length);
    //assert that the number of bytes wrote is == length
    if (bytes_wrote != length)
    {
        perror("Error - partial write.");
        exit(1);
    }

    close(fd);
    return 0;
}