#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>



int main(){
    double resultado = cos(1);
    //Almacena el valor de la función cos.
    char cmd[25];

    sprintf(cmd, "cat /proc/%d/maps", getpid());
    system(cmd);
    printf("coseno %p\n", cos); 
    //Imprime la dirección de memoria de la función coseno.
    printf("Dirección de la variable que almacena su contenido es %p y su tamaño es %ld\n", &resultado, sizeof(resultado));
    //Imprime la dirección de memoria de la variable que almacena su contenido.
    printf("Dirección de main: %p\n", main);
}

