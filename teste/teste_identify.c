#include <stdio.h>
#include <stdlib.h>
#include "../include/t2fs.h"

int main(){
    char *name = malloc(sizeof(char)*99);
    name = t2fs_identify();
    printf("teste: %s\n", name);

    return 0;
}

