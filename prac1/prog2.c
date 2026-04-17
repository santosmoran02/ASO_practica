# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>

void funcion() {
    char vector[14000];
    char cmd[25];

    sprintf(cmd, "cat /proc/%d/maps", getpid());
    system(cmd);
    printf("vector %p %ld \n" ,&vector, sizeof(vector));

}


int main(){
    printf("main %p \n", main);
    printf("funcion %p\n", funcion);

    funcion();
}