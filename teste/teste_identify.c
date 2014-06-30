#include <stdio.h>
#include <stdlib.h>
#include "../include/t2fs.h"

int main(){
    char *name = malloc(sizeof(char)*99);
    name = t2fs_identify();
    printf("teste: %s\n", name);

    int delete_file = t2fs_delete("/dir1/teste4-dir1");
    printf("delete file %s result: %d\n", "teste1/var/log", delete_file);

    return 0;
}

