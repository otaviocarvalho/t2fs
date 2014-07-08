#include <stdio.h>
#include <stdlib.h>
#include "../include/t2fs.h"
#define MAX_FILE_SIZE 1048832

int copy2t2(char *src_filename, char  *dest_filename){
    FILE  *ptr_src;
    int  a;

    ptr_src  = fopen(src_filename, "r");

    char *buffer = malloc(sizeof(char)*MAX_FILE_SIZE);

    int cont = 0;
    while(1){
        a  =  fgetc(ptr_src);

        if(!feof(ptr_src)){
            buffer[cont] = a;
        }
        else {
            break;
        }

        cont++;
    }

    // Escreve string lida do arquivo no disco do t2fs
    int handle_dest = t2fs_open(dest_filename);
    if (handle_dest >= 0){
        int write_result = t2fs_write(handle_dest, buffer, cont);
        if (write_result >= 0){
            return 0;
        }
    }

    fclose(ptr_src);
    return -1;
}

int main(int argc, char *argv[]){

    // Testa o input
    if(argc < 3){
        printf("Devem ser passados dois parâmetros para a função: Caminho de origem e Caminho de destino.\n");
        return -1;
    }

    printf("SRC: %s \n",argv[1]);
    printf("DEST: %s \n",argv[2]);

    if(copy2t2(argv[1], argv[2]) == 0)
        printf("Arquivo copiado com sucesso.\n");
    else
        printf("Ocorreu um erro durante a cópia! Tente novamente.\n");

    return 0;
}
