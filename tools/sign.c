#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_SIZE 510

int main(int argc, char*argv[])
{
    if (argc < 2) {
        printf("please input sign arguments\n");
        return (1);
    }
    int fd = open(argv[1], O_RDWR |O_APPEND);
    int len = lseek(fd, 0, SEEK_END);
    if (len > MAX_SIZE) {
        printf("error: the size of file:%s is %d\n", argv[1], len);
        return (1);
    }
    unsigned char buf[2] = "\0";
    for (; len < MAX_SIZE; len++) {
        write(fd, buf, 1);
    }
    buf[0] = 0x55;
    buf[1] = 0xaa;
    write(fd, buf, 2);
    close(fd);
}
