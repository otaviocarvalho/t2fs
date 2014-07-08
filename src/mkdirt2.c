#include <stdio.h>
#include <stdlib.h>
#include "../include/t2fs.h"

int main(int argc, char *argv[]){
    // Testa o input
    if(argc < 2){
        printf("Devem ser passado o caminho para a criação do diretório.\n");
        return -1;
    }

    if(t2fs_create_folder(argv[1]) > 0)
        printf("Diretório criado com sucesso.\n");
    else
        printf("Ocorreu um erro durante a criação do diretório! Tente novamente.\n");

    return 0;
}
