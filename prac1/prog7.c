#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(){
    int pid = fork();
    char cmd[25];
    sprintf(cmd, "cat /proc/%d/maps", getpid());

    if (pid == 0){
        printf("Soy el hijo y este es mi PID: %d", getpid());
        sprintf(cmd, "cat /proc/%d/maps", getpid());
        system(cmd);
        char maps[25];
        sprintf(maps, "/proc/%d/maps", getpid());
        execlp("cat", "cat", maps, (char *) 0);
    } else {
        printf("Soy el padre y este es mi PID %d y este es el PID de mi hijo %d\n", getpid(), pid);
        system(cmd);
    }
}