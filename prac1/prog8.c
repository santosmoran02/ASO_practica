#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

int main(){
    char cmd[25];
    sprintf(cmd, "cat /proc/%d/maps", getpid());
    system(cmd);

    int fd = open("archivo_prueba.txt", O_RDONLY);
    //Se guarda el descriptor del archivo que voy a usar para el mapeo.
    void *addr = mmap(NULL, 1024,PROT_READ, MAP_PRIVATE, fd, 0);
    //Se utiliza mmap para hacer un mapeo.
    system(cmd);
    munmap(addr, 1024);
    system(cmd);

}