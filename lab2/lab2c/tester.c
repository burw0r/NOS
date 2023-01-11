#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/*
 
Upon successful completion, the function will open the file and return a non-negative integer representing the lowest numbered unused file descriptor. Otherwise, -1 is returned and errno is set to indicate the error. No files will be created or modified if the function returns -1.
 */



int main(){

    
    printf("[+] opening /dev/shofer with O_WRONLY\n");
    int retval_wr = open("/dev/shofer", O_WRONLY);
    printf("returned %d\n", retval_wr);

    printf("[+] opening /dev/shofer with O_RDONLY\n");
    int retval_rd = open("/dev/shofer", O_RDONLY);
    printf("returned %d\n", retval_rd);

    /* printf("[+] opening /dev/shofer with O_APPEND\n");
    int retval_x = open("/dev/shofer", O_APPEND);
    printf("returned %d\n", retval_x);
*/


    printf("[+] opening /dev/shofer with O_RDWR\n");
    int retval_x = open("/dev/shofer", O_RDWR);
    printf("returned %d\n", retval_x);

}
