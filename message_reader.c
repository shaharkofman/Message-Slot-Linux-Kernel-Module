#include "message_slot.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Argument count must be exactly 3, including program name.\n");
        exit(1);
    }
    //parse arguments
    char *path = argv[1];
    int channel_id = atoi(argv[2]);
    
    if (channel_id < 0)
    {
        fprintf(stderr, "Channel id must be non negative.");
        exit(1);
    }
    
    //open the device
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        perror("Error - couldn't open the device at given path.");
        exit(1);
    }

    //set the cannel with ioctl as well
    int ret = ioctl(fd, MSG_SLOT_CHANNEL, channel_id);
    if (ret < 0)
    {
        perror("Error - couldn't set the given channel id.");
        exit(1);
    }

    //set a buffer and read a message into it
    char buffer[BUF_LEN];
    int bytes_read = read(fd, buffer, BUF_LEN);
    //in case of negative return value, there was a reading error
    if (bytes_read < 0)
    {
        perror("Error - couldn't read the message from the channel.");
        exit(1);
    }
    close(fd);

    //print the message from the buffer to stdout
    int bytes_wrote = write(STDOUT_FILENO, buffer, bytes_read);
    if (bytes_wrote < 0)
    {
        perror("Error - couldn't write the message to stdout");
        exit(1);
    }

    return 0;    
}