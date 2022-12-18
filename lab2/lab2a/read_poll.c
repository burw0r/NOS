//  1) open all devices (0-5) for reading
//  2) wait for charachters with poll
//  3) read charchters when available and print them to stdout 


#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


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
    int nfds = device_num;
    struct pollfd *pfds;

    pfds = calloc(nfds, sizeof(struct pollfd));
    if (pfds == NULL)
        errExit("malloc");

    /* 1) Open all shofer devices  and add its fd to pfds array */

    for (int j = 0; j < nfds; j++) {
        pfds[j].fd = open(devices[j], O_RDONLY);
        if (pfds[j].fd == -1)
            errExit("open");

        printf("Opened \"%s\" on fd %d\n", devices[j], pfds[j].fd);

        pfds[j].events = POLLIN;
    }

    /* 2) wait to read from devices with poll */
    /* 3) write read chars to stdout */

    while (1) {
        int ready;

        ready = poll(pfds, nfds, -1);
        if (ready == -1)
            errExit("poll");

        printf("Ready: %d\n", ready);

        /* Deal with array returned by poll(). */

        for (int j = 0; j < nfds; j++) {
            char buf[100];


            /* print occured events */
            if (pfds[j].revents != 0) {
                printf("  fd=%d; events: %s%s%s\n", pfds[j].fd,
                        (pfds[j].revents & POLLIN)  ? "POLLIN "  : "",
                        (pfds[j].revents & POLLHUP) ? "POLLHUP " : "",
                        (pfds[j].revents & POLLERR) ? "POLLERR " : "");

                /* read if ready to read */
                if (pfds[j].revents & POLLIN) {
                    ssize_t s = read(pfds[j].fd, buf, sizeof(buf));
                    if (s == -1)
                        errExit("read");

                    printf("[+] read %zd bytes: %.*s\n", s, (int) s, buf);

                } //else {                /* POLLERR | POLLHUP */
                    //printf("    closing fd %d\n", pfds[j].fd);
                    //if (close(pfds[j].fd) == -1)
                     //   errExit("close");
                //}
            }
        }
    }

}