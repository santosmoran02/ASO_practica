#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>


int variable_global;
int main();

void *manejador(void *arg){
    int variable_local;
    printf("dirección variable  local: %p\n", &variable_local);
    printf("tamaño variable local: %ld\n", sizeof(variable_local));
    printf("dirección variable global: %p\n", &variable_global);
    printf("tamaño variable global: %ld\n", sizeof(variable_global));
    printf("Dirección del main: %p\n", main);
}


int main(){
    char cmd[25];
    sprintf(cmd, "cat /proc/%d/maps", getpid());
    system(cmd);
    pthread_t hilo;
    pthread_t hilo2;
    pthread_create(&hilo, NULL, manejador, NULL);
    pthread_create(&hilo2, NULL, manejador, NULL);
    system(cmd);
    pthread_join(hilo, NULL);
    pthread_join(hilo2, NULL);
    system(cmd);
}