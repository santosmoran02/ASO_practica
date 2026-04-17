#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {

    char *vector = NULL;
    char cmd[25];

    sprintf(cmd, "cat /proc/%d/maps", getpid());

    system(cmd);
    printf("Dirección %p, tamaño %ld\n", vector, sizeof(vector));

    vector = malloc(14000);

    system(cmd);
    printf("Dirección %p, tamaño %d, contenido %d\n", vector, 14000, *vector);

    free(vector);

    system(cmd);
    printf("Dirección %p, tamaño %ld\n", vector, sizeof(vector));
    
    printf("main %p\n", main);
}
