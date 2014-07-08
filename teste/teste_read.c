#include <stdio.h>
#include <stdlib.h>
#include "../include/t2fs.h"

int main(){
    char *buffer = malloc(sizeof(char)*100);

    int handle = t2fs_open("/dir1/teste4-dir1");
    t2fs_read(handle, buffer, 20);

    int i;
    for (i = 0; i < 20; i++) {
        printf("%c", buffer[i]);
    }
    printf("\n");

    return 0;
}
