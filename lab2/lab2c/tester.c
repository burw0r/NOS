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

    
    printf("\n------------------ opening devices ------------------\n");

    printf("[+] opening /dev/shofer with O_WRONLY\n");
    int retval_wr = open("/dev/shofer", O_WRONLY);
    printf("  > returned %d\n", retval_wr);

    printf("[+] opening /dev/shofer with O_RDONLY\n");
    int retval_rd = open("/dev/shofer", O_RDONLY);
    printf("  > returned %d\n", retval_rd);

    printf("[+] opening /dev/shofer with O_RDWR\n");
    int retval_rw = open("/dev/shofer", O_RDWR);
    printf("  > returned %d\n", retval_rw);


    printf("\n------------------ writing correctly ------------------\n");

    printf("[+] writing to \"ABCDEFGHI\" to device\n");
    char buff_w[] = "ABCDEFGHI";
    int ret_write = write(retval_wr, buff_w, sizeof(buff_w));
    printf(" >  writing %s to device\n", buff_w);
    printf("  > write returned %d\n", ret_write);



    printf("\n------------------ reading correctly ------------------\n");

    printf("[+] trying to write to a read only  fd\n");
    char buff_r[100];
    int ret_read = read(retval_rd, buff_r, sizeof(buff_r));
    printf("  > read %s from device\n", buff_r);
    printf("  > read returned %d\n", ret_read);


    printf("\n------------------ illegal write ------------------\n");

    printf("[+] trying to write to a read only  fd\n");
    char buff2[] = "illegal write!";
    int ret_write_illegal = write(retval_rd, buff2, sizeof(buff2));
    printf("  > write returned %d\n", ret_write_illegal);


    printf("\n------------------ illegal read ------------------\n");

    printf("[+] trying to read from a write only  fd\n");
    char buff3[15];
    int ret_read_illegal = read(retval_wr, buff3, sizeof(buff3));
    printf("  > read returned %d\n", ret_read_illegal);


    printf("------------------------------------------------------\n");


}