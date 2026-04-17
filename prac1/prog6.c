#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void){
    int variable;
    printf("variable: %p\n", &variable);
    printf("Tamaño variable: %ld\n", sizeof(variable));
    char cmd[25];
    sprintf(cmd, "cat /proc/%d/maps", getpid());
    system(cmd);
    int pid = fork();

    if (pid == 0){
        printf("Soy el hijo y este es mi PID: %5d\n", getpid());
        int variable_hijo;
        printf("dirección variable hijo: %p\n", &variable_hijo);
        printf("tamaño variable hijo: %ld\n", sizeof(variable_hijo));
        sprintf(cmd, "cat /proc/%d/maps", getpid());
        system(cmd);
    } else {
        printf("Soy el padre y este es mi Pid %d y este es el Pid de mi hijo %d\n", getpid(), pid);
        int variable_padre;
        printf("dirección variable padre: %p\n", &variable_padre);
        printf("tamaño variable padre: %ld\n", sizeof(variable_padre));
        system(cmd);
    }
}