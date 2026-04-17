#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(){
    char cmd[25];
    //Variable para guardar el comando que se pasará a la terminal para imprimir mapa de memoria.
    sprintf(cmd, "cat /proc/%d/maps", getpid());
    //Se guarda en cmd el comando y se obtine el pid.

    system(cmd);
    //se pone el la terminal el comando guardado en cmd.

    void *handle = dlopen("libm.so.6", RTLD_LAZY);
    //Puntero que accede a la librería matemática en el tiempo de ejecución.
    double (*cos)(double) = dlsym(handle, "cos");
    //Puntero de tipo double que obtiene la dirección de la función coseno porque recibe como parámetro handle.
    double resultado = cos(1.0);
    //variable que guarda el valor númerico que devuelve el coseno.
    printf("coseno %p\n", cos);
    //Se imprime la dirección de memoria de la función coseno.
    printf("Dirección de memoria de la variable que contiene el valor de la función coseno es %p y su tamaño es %ld\n", &resultado, sizeof(resultado));
    //Se imprime la dirección de la variabla que contiene el valor de la función coseno.
    printf("Dirección del main: %p\n",main);
    system(cmd);
    //Se vuelve a imprimir la dirección de memoria del programa.

    dlclose(handle);
    //Se cierra el acceso a la dirección de la librería matemática.
    system(cmd);
    //Se imprime por última vez la dirección de memoria del programa.
}