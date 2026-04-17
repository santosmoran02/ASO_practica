#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

int main(){
    double resultado = cos(1.0);
    char cmd[25];

    sprintf(cmd, "cat /proc/%d/maps", getpid());
    system(cmd);

    printf("coseno: %p\n", cos);
    printf("Dirección de memoria de la variable que contiene el valor de la función coseno es %p y su tamaño es %ld\n", &resultado, sizeof(resultado));
    printf("Dirección del main es %p\n", main);
}