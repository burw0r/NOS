//  1) opean all shofer (0-5) devices for reading 
//  2) every 5 seconds check available devices with poll 
//  3) randomy choose one that is and write a char


#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


const char* devices[6] = 
{
    "/dev/shofer0",
    "/dev/shofer1",
    "/dev/shofer2",
    "/dev/shofer3",
    "/dev/shofer4",
    "/dev/shofer5"
};

const int device_num = 6;


#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)



int main(int argc, char *argv[])
{

    srand(time(NULL));

    int nfds = device_num;
    struct pollfd *pfds;

    pfds = calloc(nfds, sizeof(struct pollfd));
    if (pfds == NULL)
        errExit("malloc");

    /* 1) Open all shofer devices for writing and add its fd to pfds array */

    for (int j = 0; j < nfds; j++) {
        pfds[j].fd = open(devices[j], O_WRONLY);
        if (pfds[j].fd == -1)
            errExit("open");

        printf("Opened \"%s\" on fd %d\n", devices[j], pfds[j].fd);

        pfds[j].events = POLLOUT;
    }

    /* 2) wait to write to devices with poll */
    /* 3) write random char to any of available devices */

    while (1) {

        sleep(5);

        int ready = poll(pfds, nfds, -1);
        if (ready == -1)
            errExit("poll");


        int ready_for_write;
        while(1){
            int index = (rand() % device_num);
            if (pfds[index].revents & POLLOUT){
                ready_for_write = index;
                break;
            }
        }

        char buf = 'A' + (rand() % 26);
        ssize_t s = write(pfds[ready_for_write].fd, &buf, sizeof(buf));
        if (s == -1)
            errExit("write");

        printf("[+] written charachter %c\n", buf);

        
    }

}