#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apidisk.h"
#include "t2fs.h"

#define MAX_BLOCK_SIZE 65536
#define FILE_REGISTER 64
#define MAX_FILES 20
#define MAX_FILE_NAME 39

#define SIZE_SECTOR_BYTES 256
#define SIZE_SUPERBLOCK_BYTES 256

#define SET_BIT 1
#define UNSET_BIT 0

struct file {
    t2fs_file handle;
    unsigned int currentBytesPos; // Deslocamento em bytes do primeiro byte do arquivo
    unsigned int currentBlockPos; // Deslocamento em blocos do primeiro bloco direto do arquivo
    struct t2fs_record record;
    struct file *next;
    struct file *prev;
};

// Estruturas com equivalência
unsigned long int diskInitialized = 0;
unsigned short diskVersion;
unsigned short diskSuperBlockSize;
unsigned long int diskSize;
unsigned long int diskBlocksNumber;
unsigned long int diskBlockSize;
struct t2fs_record diskBitMapReg;
struct t2fs_record diskRootDirReg;

// Estruturas de buffer a serem persistidas no disco físico
struct t2fs_superbloco superBlockBuffer;
char bitmapBuffer[SIZE_SECTOR_BYTES];
/*unsigned char **bitmapBuffer;*/

/*unsigned char diskVersion;*/
/*unsigned int diskSize;*/ // Era dado em blocos, agora é em bytes (diskSize / blockSize == diskSizeAnterior)
/*unsigned short diskBlockSize;*/
/*unsigned short diskFileEntrySize;*/
// Substituir estruturas sem equivalência nessa versão
unsigned char diskCtrlSize; // Não existe, o ctrlSize é determinado pelo SuperBlockSize
unsigned short diskFreeBlockSize; // Era o tamanho do bitmap, agora o bitmap é passado por parâmetro
unsigned short diskRootSize; // Era o número de blocos do diretório, agora o diretório é passado por parâmetro
unsigned short diskFileEntrySize; // Era o tamanho de um registro no diretório. Desnecessário, é o tamanho do t2fs_record

struct t2fs_superbloco superblock;

struct file *openFiles;
int openFilesMap[20] = {0};
int countOpenFiles = 0;
int countFiles = 0;

// Protótipos de funções auxiliares
struct t2fs_record *findFile(char *name);

// Função auxiliar que converte o valor de um bloco para o seu equivalente em trilhas físicas do disco
int convertBlockToSector(int block_size, int block_number){
    return (block_number * (block_size / SIZE_SECTOR_BYTES)) + 1;
}

int t2fs_first(struct t2fs_superbloco *findStruct);

// função para identificar os componentes do grupo responsável pelo desenvolvimento deste trabalho
char *t2fs_identify(void){
    char *str = malloc(sizeof(char) * 90);

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    str = memcpy(str, "Otavio (180470) - Lisandro (143764) - Tagline (180229)\0", 90);

    return str;
}

// Função que altera o bitmap e persiste o resultado no disco físico
void markBlockBitmap(int block, int setbit){
    int posByte, posBit;
    unsigned char block_copy;

    // Encontra byte e bit especifico a serem modificados
    posByte = block / 8; // Encontra o byte no qual escrever (8 blocos representados por byte)
    posBit = block % 8; // Encontra em qual bit do byte escrever (representação da direita para a esquerda, do 0 ao 7, bit 7 ativo == 0x80)
    /*printf("posByte %x\n", posByte);*/
    /*printf("posBit %x\n", posBit);*/

    memcpy(&block_copy, bitmapBuffer+(posByte*sizeof(unsigned char)), sizeof(unsigned char));
    /*printf("blockCopy %x\n", block_copy);*/
    // Faz set ou unset do bit especificado
    if (setbit){
        block_copy = block_copy | (0x1 << posBit);
    }
    else {
        block_copy = block_copy & (0xFE << posBit);
    }
    /*printf("blockCopy after set/unset %x\n", block_copy);*/

    // Salva o valor no bitmap
    memcpy(bitmapBuffer+(posByte*sizeof(unsigned char)), &block_copy, sizeof(unsigned char));
    memcpy(&block_copy, bitmapBuffer+(posByte*sizeof(unsigned char)), sizeof(unsigned char));
    /*printf("novo valor no bitmap %x\n", block_copy);*/

    // Escolha do bloco onde escrever o bitmap
    int blocoWriteBitMap = diskBitMapReg.dataPtr[0];
    // Persiste o bitmap alterado no disco
    /*printf("bitmap block %d bitmap sector %d\n", blocoWriteBitMap, convertBlockToSector(diskBlockSize, blocoWriteBitMap));*/
    write_sector(convertBlockToSector(diskBlockSize, blocoWriteBitMap), bitmapBuffer);
}

// Função que inicializa as variáveis necessárias para a correta execução do sistema de arquivos
void initDisk(struct t2fs_superbloco *sblock){
    // Inicializa o cache de arquivos abertos
    openFiles = NULL;
    countOpenFiles = 0;

    // Inicialização das variáveis globais do disco
    diskVersion = (unsigned short) sblock->Version;
    diskSuperBlockSize = (unsigned short) sblock->SuperBlockSize; // diskSize, tamanho do superbloco em blocos/setores
    diskSize = (unsigned long int) sblock->DiskSize; // blockSize * diskSize, tamanho do disco em bytes
    diskBlocksNumber = (unsigned long int) sblock->NofBlocks;
    diskBlockSize = (unsigned long int) sblock->BlockSize;

    // Inicialização do bitmap a partir da variável global do disco BitMapReg
    diskBitMapReg = sblock->BitMapReg;
    char *find = malloc(SIZE_SECTOR_BYTES);
    int status_read = read_sector(1, find);
    if (status_read == 0){
        /*bitmapBuffer = malloc(SIZE_SECTOR_BYTES);*/
        memcpy(bitmapBuffer, find, SIZE_SECTOR_BYTES);
    }
    free(find);

    // Inicialização do diretório corrente do disco a partir da variável global do disco RootDirReg
    diskRootDirReg = sblock->RootDirReg;

    // Inicialização do buffer do superbloco
    memcpy(&superBlockBuffer, sblock, sizeof(struct t2fs_superbloco));

    // Ativa a flag de conclusão da inicialização do disco
    diskInitialized = 1;

    /*printf("read superblock copy struct: %c%c%c%c\n", superBlockBuffer.Id[0], superBlockBuffer.Id[1], superBlockBuffer.Id[2], superBlockBuffer.Id[3]);*/

    // Teste leitura dos campos do superbloco
    /*printf("read superblock struct: %c%c%c%c\n", sblock->Id[0], sblock->Id[1], sblock->Id[2], sblock->Id[3]);*/
    /*printf("version: %x\n", diskVersion);*/
    /*printf("SuperBlockSize: %d\n", diskSuperBlockSize);*/
    /*printf("diskSize: %ld\n", diskSize);*/
    /*printf("diskBlocksNumber: %ld\n", diskBlocksNumber);*/
    /*printf("blockSize: %ld\n", diskBlockSize);*/
    /*printf("diskRootDirReg name: %s\n", diskRootDirReg.name);*/

    // Teste de impressão do t2fs_record do diretório raiz
    printf("\nprint diretório raiz t2fs_record:\n");
    printf("%s\n", diskRootDirReg.name);
    printf("blocksFileSize: %ld\n", (unsigned long int) diskRootDirReg.blocksFileSize);
    printf("bytesFileSize: %ld\n", (unsigned long int) diskRootDirReg.bytesFileSize);
    printf("dataPtr[0]: %x\n", diskRootDirReg.dataPtr[0]);
    printf("dataPtr[1]: %x\n", diskRootDirReg.dataPtr[1]);
    printf("singleIndPtr: %x\n", diskRootDirReg.singleIndPtr);
    printf("doubleIndPtr: %x\n", diskRootDirReg.doubleIndPtr);

    // Teste de impressão do t2fs_record do bitmap
    /*printf("\nprint bitmap t2fs_record:\n");*/
    /*printf("%s\n", diskBitMapReg.name);*/
    /*printf("blocksFileSize: %ld\n", (unsigned long int) diskBitMapReg.blocksFileSize);*/
    /*printf("bytesFileSize: %ld\n", (unsigned long int) diskBitMapReg.bytesFileSize);*/
    /*printf("dataPtr[0]: %x\n", diskBitMapReg.dataPtr[0]);*/
    /*printf("dataPtr[1]: %x\n", diskBitMapReg.dataPtr[1]);*/
    /*printf("singleIndPtr: %x\n", diskBitMapReg.singleIndPtr);*/
    /*printf("doubleIndPtr: %x\n", diskBitMapReg.doubleIndPtr);*/

    // Teste de leitura do primeiro arquivo do diretório raiz
    /*struct t2fs_record *test_record = malloc(sizeof(struct t2fs_record)) ;*/
    /*memcpy(test_record, &diskRootDirReg.dataPtr[0]+sizeof(struct t2fs_record)*0, sizeof(struct t2fs_record));*/
    /*printf("\nfilename: %s\n", test_record->name);*/
    /*printf("typeval: %x\n", test_record->TypeVal);*/
    /*printf("blocksFileSize: %ld\n", (unsigned long int) test_record->blocksFileSize);*/

    // Teste de mapeamento do bloco lógico para setor do disco físico
    /*printf("block to sector map: %d block to %d sector\n", diskBitMapReg.dataPtr[0], convertBlockToSector(diskBlockSize, diskBitMapReg.dataPtr[0]));*/

    // Teste de leitura do bitmap
    int i;
    for (i = 0; i < diskBitMapReg.bytesFileSize/8; i++) {
        printf("%x\n", (unsigned char) bitmapBuffer[i]);
    }
    // Teste de mudança no bitmap
    /*markBlockBitmap(126,SET_BIT);*/

    // Teste de mudança no superbloco, escrita no disco e posterior leitura do superbloco
    /*unsigned char *find = malloc(SIZE_SECTOR_BYTES); // Lê um setor do disco 'físico'*/
    /*printf("valor do primeiro byte do bitmap alterado para %x\n", 1);*/
    /*superBlockBuffer.BitMapReg.dataPtr[0] = 1;*/
    /*memcpy(find, &superBlockBuffer, sizeof(struct t2fs_superbloco));*/
    /*write_sector(0, find);*/
    /*read_sector(0, find);*/
    /*struct t2fs_superbloco *findStruct = malloc(sizeof(struct t2fs_superbloco));*/
    /*memcpy(findStruct, find, sizeof(struct t2fs_superbloco));*/
    /*printf("read superblock struct: %c%c%c%c\n", findStruct->Id[0], findStruct->Id[1], findStruct->Id[2], findStruct->Id[3]);*/
    /*printf("valor do primeiro byte do bitmap lido do disco %x\n", findStruct->BitMapReg.dataPtr[0]);*/
    /*free(find);*/
    /*free(findStruct);*/

    // Teste de varredura dos arquivos/pastas de um diretório
    char *find_folder = malloc(SIZE_SECTOR_BYTES); // Lê um setor do disco 'físico'
    printf("%d -> %d\n", diskRootDirReg.dataPtr[0], convertBlockToSector(diskBlockSize, diskRootDirReg.dataPtr[0]));
    read_sector(convertBlockToSector(diskBlockSize, 1), find_folder);
    /*read_sector(convertBlockToSector(diskBlockSize, diskRootDirReg.dataPtr[0]), find_folder);*/
    int k=0;
    struct t2fs_record *read_rec = malloc(sizeof(struct t2fs_record));
    while(k < SIZE_SECTOR_BYTES){
        memcpy(read_rec, find_folder+k, sizeof(struct t2fs_record));
        printf("%s\n", read_rec->name);
        printf("tipo: %x\n", read_rec->TypeVal);
        printf("bloco direto[0]: %x\n", read_rec->dataPtr[0]);
        printf("bloco direto[1]: %x\n", read_rec->dataPtr[1]);
        printf("singleIndPtr: %x\n", read_rec->singleIndPtr);
        printf("doubleIndPtr: %x\n", read_rec->doubleIndPtr);

        k += sizeof(struct t2fs_record);
    }
    free(read_rec);
    free(find_folder);

    // Teste fileGetHandle/fileDeleteHandle
    /*struct file *file_aux[4];*/
    /*file_aux[0] = malloc(sizeof(struct file));*/
    /*memcpy(file_aux[0]->record.name, "test\0", sizeof(char)*MAX_FILE_NAME);*/
    /*file_aux[1] = malloc(sizeof(struct file));*/
    /*memcpy(file_aux[1]->record.name, "test1\0", sizeof(char)*MAX_FILE_NAME);*/
    /*file_aux[2] = malloc(sizeof(struct file));*/
    /*memcpy(file_aux[2]->record.name, "test2\0", sizeof(char)*MAX_FILE_NAME);*/
    /*file_aux[3] = malloc(sizeof(struct file));*/
    /*memcpy(file_aux[3]->record.name, "test3\0", sizeof(char)*MAX_FILE_NAME);*/

    /*for (i = 0; i < 4; i++) {*/
        /*int handle_print = fileGetHandle(file_aux[i]);*/
        /*printf("handle created: %d\n", handle_print);*/
        /*printf("countOpenFiles %d\n", countOpenFiles);*/

        /*if (i == 3){*/
            /*int result_delete = fileDeleteHandle(2); // meio*/

            /*struct file *file_aux_insert = malloc(sizeof(struct file));*/
            /*memcpy(file_aux_insert->record.name, "testmiddle\0", sizeof(char)*MAX_FILE_NAME);*/
            /*int handle_middle = fileGetHandle(file_aux_insert);*/
            /*printf("handle insert middle: %d\n", handle_middle); // insere após remover 1*/

            /*int result_delete_1 = fileDeleteHandle(2); // fim*/
            /*int result_delete_2 = fileDeleteHandle(0); // inicio*/
            /*int result_delete_3 = fileDeleteHandle(1);*/
            /*int result_delete_5 = fileDeleteHandle(3); // unico*/
            /*int result_delete_4 = fileDeleteHandle(3); // lista vazia*/
        /*}*/
    /*}*/
    /*struct file *trav = openFiles;*/
    /*while (trav != NULL){*/
        /*printf("%d\n", trav->handle);*/
        /*trav = trav->next;*/
    /*}*/

    // Teste read/write
    /*struct file *file_aux_read = malloc(sizeof(struct file));*/
    /*file_aux_read->currentBytesPos = 0;*/

    /*struct t2fs_record *aux = malloc(sizeof(struct file));*/
    /*aux = findFile("/dir1/teste1");*/
    /*memcpy(&file_aux_read->record, aux, sizeof(struct t2fs_record));*/
    /*printf("teste record %s\n", file_aux_read->record.name);*/
    /*printf("teste record first pointer %d\n", convertBlockToSector(diskBlockSize, file_aux_read->record.dataPtr[0]));*/
    /*printf("teste record bytes %d\n", file_aux_read->record.bytesFileSize);*/
    /*file_aux_read->handle = fileGetHandle(file_aux_read);*/
    /*char *buffer = malloc(sizeof(char)*diskBlockSize);*/
    /*t2fs_read(file_aux_read->handle, buffer, file_aux_read->record.bytesFileSize);*/
    /*for (i = 0; i < file_aux_read->record.bytesFileSize; i++) {*/
        /*printf("%c", buffer[i]);*/
    /*}*/
    /*printf("\n");*/
}

// Função que lê o superbloco do disco na inicialização
int t2fs_first(struct t2fs_superbloco *findStruct){
    int status_read;
    char *find = malloc(SIZE_SECTOR_BYTES); // Lê um setor do disco 'físico'

    /*status_read = read_block(0, find);*/
    status_read = read_sector(0, find);

    if (status_read != 0){
        return -1;
    }
    else {
        memcpy(findStruct, find, sizeof(struct t2fs_superbloco));
    }

    if(!diskInitialized){
        initDisk(findStruct);
    }

    free(find);

    return 0;
}

// Função que percorre a árvore em busca da estrutura de dados, que representa um arquivo, em função do seu caminho
struct t2fs_record *findFile(char *name){
    char *token, *string, *tofree;
    struct t2fs_record *file = malloc(sizeof(struct t2fs_record));
    char *find_folder = malloc(SIZE_SECTOR_BYTES);
    int found, count_bytes;

    tofree = string = strdup(name);
    if (string != NULL){
        // Testa leitura do primeiro arquivo do diretório raiz "/"
        if (strcmp(&string[0],"/")){
            read_sector(convertBlockToSector(diskBlockSize, diskRootDirReg.dataPtr[0]), find_folder);
            memcpy(file, find_folder, sizeof(struct t2fs_record));
            string++;
        }
        else {
            return NULL;
        }

        // Testa a leitura dos subdiretórios ou arquivos
        while((token = strsep(&string, "/")) != NULL){
            printf("%s\n", token);

            // Procura pelo nome no primeiro setor
            found = 0;
            count_bytes = 0;
            while (count_bytes < SIZE_SECTOR_BYTES && !found){
                memcpy(file, find_folder+count_bytes, sizeof(struct t2fs_record));
                printf("percorreu %s\n", file->name);
                // Testa se encontrou o pedaço do caminho
                if (strcmp(token,file->name) == 0){
                    if (file->TypeVal == TYPEVAL_DIRETORIO || file->TypeVal == TYPEVAL_REGULAR){
                        printf("encontrou %s\n", token);
                        found = 1;
                    }
                }
                count_bytes += sizeof(struct t2fs_record);
            }
            if (strcmp(token," ") == 0)
                printf("here1\n");

            // Lê subdiretório se for uma pasta e ainda não encontrou a pasta/arquivo que procurava
            if (file->TypeVal == TYPEVAL_DIRETORIO && !found){
                read_sector(convertBlockToSector(diskBlockSize, file->dataPtr[0]), find_folder);
                memcpy(file, find_folder, sizeof(struct t2fs_record));
            }
        }

        // Retorna arquivo se encontrou o caminho completo
        if (found){
            return file;
        }

        free(tofree);
    }

    free(find_folder);
    free(file);
    return NULL;
}

// Função auxiliar para invalidação dos blocos alocados para um arquivo
void invalidateFile(char *name){
    char *token, *string, *tofree;
    struct t2fs_record *file = malloc(sizeof(struct t2fs_record));
    char *find_folder = malloc(SIZE_SECTOR_BYTES);
    int found, count_bytes;
    int currentSector;

    tofree = string = strdup(name);
    if (string != NULL){
        // Testa leitura do primeiro arquivo do diretório raiz "/"
        if (strcmp(&string[0],"/")){
            currentSector = convertBlockToSector(diskBlockSize, diskRootDirReg.dataPtr[0]);
            read_sector(currentSector, find_folder);
            memcpy(file, find_folder, sizeof(struct t2fs_record));
            string++;
        }
        else {
            return;
        }

        // Testa a leitura dos subdiretórios ou arquivos
        while((token = strsep(&string, "/")) != NULL){
            printf("%s\n", token);

            // Procura pelo nome no primeiro setor
            found = 0;
            count_bytes = 0;
            while (count_bytes < SIZE_SECTOR_BYTES && !found){
                memcpy(file, find_folder+count_bytes, sizeof(struct t2fs_record));
                printf("percorreu %s\n", file->name);
                // Testa se encontrou o pedaço do caminho
                if (strcmp(token,file->name) == 0){
                    if (file->TypeVal == TYPEVAL_DIRETORIO || file->TypeVal == TYPEVAL_REGULAR){
                        printf("encontrou %s\n", token);
                        found = 1;
                    }
                }
                else {
                    count_bytes += sizeof(struct t2fs_record);
                }
            }
            if (strcmp(token," ") == 0)
                printf("here1\n");

            // Lê subdiretório se for uma pasta
            if (file->TypeVal == TYPEVAL_DIRETORIO){
                currentSector = convertBlockToSector(diskBlockSize, file->dataPtr[0]);
                read_sector(currentSector, find_folder);
                memcpy(file, find_folder, sizeof(struct t2fs_record));
            }
        }

        // Invalida arquivo se encontrou o caminho completo
        if (found){
            file->TypeVal = TYPEVAL_INVALIDO;
            memcpy(find_folder+count_bytes, file, sizeof(struct t2fs_record));
            write_sector(currentSector, find_folder);
        }

        free(tofree);
    }

    free(find_folder);
    free(file);
    return;
}

// Função que deleta um arquivo do sistema de arquivos e libera seus blocos
int t2fs_delete(char *name){
    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    // Busca o arquivo a ser deletado
    /*int handle = t2fs_open(name);*/
    struct t2fs_record *del_rec = findFile(name);

    // Testa se encontrou o arquivo
    if (del_rec != NULL){
        // Fecha o arquivo se ele se encontra entre os abertos
        /*deleteFile = findFile(handle);*/

        // Descobre o número de blocos a serem deletados
        int numBlocks = del_rec->blocksFileSize;

        // Deleta blocos apontados por ponteiros diretos
        int i;
        for (i = 0; i < 2; i++) {
            if (numBlocks > 0){
                markBlockBitmap(del_rec->dataPtr[i], UNSET_BIT);
                numBlocks--;
            }
            /*else {*/
                /*break;*/
            /*}*/
        }

        // Deleta blocos apontados por ponteiros de indireção simples
        /*if (numBlocks > 0){*/
            /*markBitmap(deleteFile->record.singleIndPtr, 0);*/
            /*read_sector(deleteFile->record.singleIndPtr, block);*/

            /*for (i = 0; i < diskBlockSize; i++) {*/
                /*if (numBlocks > 0){*/
                    /*markBitmap(block[i], 0);*/
                    /*numBlocks--;*/
                /*}*/
                /*else {*/
                    /*break;*/
                /*}*/
            /*}*/
        /*}*/

        // Deleta blocos apontados por ponteiros de indireção dupla

        // Invalida registro no disco físico
        /*del_rec->TypeVal = TYPEVAL_INVALIDO;*/
        invalidateFile(name);
        /*read_sector(deleteFile->pos, block);*/
        /*block[deleteFile->blockPos] = block[deleteFile->blockPos] & 0x7F;*/
        /*write_sector(deleteFile->pos, block);*/

        return 0;
    }
    else {
        return -1;
    }
}

char *validateFileName(char *name){
    /*char *token, *tofree, *string;*/
    char *token, *string;
    char *token_return;

    string = strdup(name);
    while((token = strsep(&string, "/")) != NULL){
        token_return = token;
    }

    /*printf("%s\n", token_return);*/
    if(strlen(token_return)>MAX_FILE_NAME){
        token_return[MAX_FILE_NAME-1] = '\0';
    }

    /*printf("%s\n", token_return);*/
    /*free(tofree);*/
    return token_return;
}

char *validateFolderName(char *name){
    char *fileName;
    char *folderName = malloc(sizeof(char)*MAX_FILE_NAME);

    fileName = validateFileName(name);
    /*printf("%s\n", fileName);*/
    memset(folderName, '\0', sizeof(char)*MAX_FILE_NAME);
    memcpy(folderName, name, strlen(name)-strlen(fileName)-1); // -1 retira a última barra do caminho

    return folderName;
}

int diskInsertRecord(struct t2fs_record *new_record, char *folder_name){
    struct t2fs_record *find_folder;
    struct t2fs_record *find_record = malloc(sizeof(struct t2fs_record));
    struct t2fs_record *free_record = malloc(sizeof(struct t2fs_record));
    char *find_folder_sector = malloc(SIZE_SECTOR_BYTES);
    int find_record_displacement = 0;

    // Procura pasta no disco
    /*printf("%s\n", folder_name);*/
    find_folder = findFile(folder_name);

    // Retorna se não encontrou a pasta
    if (find_folder == NULL){
        return -1;
    }

    // Insere novo arquivo na pasta encontrada
    if (find_folder->TypeVal == TYPEVAL_DIRETORIO){
        // Ponteiro direto
        int currentSector = convertBlockToSector(diskBlockSize, find_folder->dataPtr[0]);
        int currentBlock = find_folder->dataPtr[0];
        printf("currenBlock %d\n", currentBlock);
        // Indireção simples
        // Indireção dupla

        // Lê a pasta para escrita do disco
        read_sector(currentSector, find_folder_sector);
        memcpy(free_record, find_folder_sector, sizeof(struct t2fs_record));

        // Procura a primeira entrada inválida para escrita do novo record
        int i = 0;
        int found = 0;
        while(i < SIZE_SECTOR_BYTES){
            /*printf("%s %d\n", free_record->name, i);*/
            // Guarda marcador do primeiro espaço livre
            if (free_record->TypeVal != TYPEVAL_DIRETORIO && free_record->TypeVal != TYPEVAL_REGULAR && !found){
                memcpy(find_record, free_record, sizeof(struct t2fs_record));
                find_record_displacement = i;
                found = 1;
            }

            // Se o arquivo já existe sobreescreve
            if (strcmp(free_record->name, new_record->name) == 0){
                memcpy(find_record, free_record, sizeof(struct t2fs_record));
                find_record_displacement = i;
                // Limpa os blocos alocados anteriormente
                /*diskCleanBlocks(free_record);*/
                found = 1;
            }

            // Lê a próxima entrada da pasta
            i += sizeof(struct t2fs_record);
            memcpy(free_record, find_folder_sector+i, sizeof(struct t2fs_record));
        }

        // Escreve o record e atualiza o bitmap ao encontrar uma entrada livre para escrita
        if (found){
            // Escreve o novo record
            /*memcpy(find_folder_sector+i, new_record, sizeof(struct t2fs_record));*/
            memcpy(find_folder_sector+find_record_displacement, new_record, sizeof(struct t2fs_record));
            write_sector(currentSector, find_folder_sector);

                    /*// Teste leitura*/
                    /*printf("Leitura\n");*/
                    /*read_sector(currentSector, find_folder_sector);*/
                    /*memcpy(free_record, find_folder_sector, sizeof(struct t2fs_record));*/
                    /*int i = 0;*/
                    /*while(i < SIZE_SECTOR_BYTES){*/
                        /*printf("%s %d\n", free_record->name, i);*/

                        /*// Lê a próxima entrada da pasta*/
                        /*i += sizeof(struct t2fs_record);*/
                        /*memcpy(free_record, find_folder_sector+i, sizeof(struct t2fs_record));*/
                    /*}*/

            // Atualiza o bitmap
            markBlockBitmap(currentBlock, SET_BIT);
        }
    }

    free(find_folder_sector);
    free(find_record);
    free(free_record);
    return 1;
}

// Retorna o próximo indicador livre da lista mapa de arquivos abertos vazios
int fileFindNewHandle(){
    int i;

    // Percorre o array mapa dos handles e retorna o primeiro valor vazio
    for (i = 0; i < 20; i++) {
        if (openFilesMap[i] == 0){
            openFilesMap[i] = 1;
            return i;
        }
    }

    return -1;
}

// Deleta um file da lista de arquivos abertos
t2fs_file fileDeleteHandle(int handle_delete){
    struct file *file_aux;

    file_aux = openFiles;
    while (file_aux != NULL){
        // Deleta arquivo ao encontrar
        if (file_aux->handle == handle_delete){
            // Desmarca do Mapa
            openFilesMap[handle_delete] = 0;

            // Deleta o primeiro
            if (file_aux->prev == NULL){
                openFiles = NULL;
            }
            // Deleta no fim da lista
            else if (file_aux->next == NULL){
                file_aux->prev->next = NULL;
            }
            // Deleta no meio da lista
            else {
                file_aux->prev->next = file_aux->next;
                file_aux->next->prev = file_aux->prev;
            }

            // Decrementa contador de arquivos abertos
            countOpenFiles--;

            // Libera o ponteiro e retorna sucesso
            free(file_aux);
            return 1;
        }

        // Percorre até encontrar o último da lista
        file_aux = file_aux->next;
    }

    return -1;
}

// Cria um novo file na lista de arquivos abertos
t2fs_file fileGetHandle(struct file *new){
    struct file *file_aux;

    file_aux = openFiles;

    int found_last = 0;
    while (file_aux != NULL && !found_last){
        // Retorna o handle se encontrou arquivo a ser adicionado já abertos
        if (strcmp(file_aux->record.name, new->record.name) == 0){
            return file_aux->handle;
        }

        // Percorre até encontrar o último da lista
        if (file_aux->next == NULL){
            found_last = 1;
        }
        else {
            file_aux = file_aux->next;
        }
    }

    // Insere novo arquivo aberto no fim da lista
    if (file_aux == NULL)
        openFiles = file_aux = new;
    else
        file_aux->next = new;

    // Preenche campos do novo arquivo aberto
    new->handle = fileFindNewHandle();
    new->currentBytesPos = 0;
    new->currentBlockPos = 0;
    new->next = NULL;
    new->prev = file_aux;

    // Incrementa contador de arquivos abertos
    countOpenFiles++;

    return new->handle;
}

// Função que cria um novo arquivo do sistema de arquivos no disco físico
t2fs_file t2fs_create(char *name){
    char* validatedName;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    /*// Verifica se um arquivo pode ser criado*/
    if (countFiles > MAX_FILES-1){
        return -1;
    }

    // Valida se a estrutura de pastas para a criação do arquivo já existe
    validatedName = validateFileName(name);

    // Monta arquivo a ser salvo no disco
    struct file* newFile = malloc(sizeof(struct file));
    memcpy(newFile->record.name, validatedName, sizeof(char)*MAX_FILE_NAME);
    newFile->record.TypeVal = 0x01; // 0xFF (registro inválido) OU  0x01 (arquivo regular) OU 0x02 (arquivo de diretório)
    newFile->record.blocksFileSize = 0;
    newFile->record.bytesFileSize = 0;
    newFile->record.dataPtr[0] = 0xFFFFFFFF;
    newFile->record.dataPtr[1] = 0xFFFFFFFF;
    newFile->record.singleIndPtr = 0xFFFFFFFF;
    newFile->record.doubleIndPtr = 0xFFFFFFFF;

    // Testa se o arquivo a ser criado poderá receber um handle
    newFile->handle = fileGetHandle(newFile);
    if (newFile->handle >= 0 && countOpenFiles <= 20){
        // Salva arquivo no disco e atualiza apontadores
        /*printf("%s\n", name);*/
        diskInsertRecord(&newFile->record, validateFolderName(name));
    }
    else {
        return -1;
    }

    return newFile->handle;
}

//Reposiciona o current pointer do arquivo
int t2fs_seek(t2fs_file handle, unsigned int offset){
    struct file *file_aux;
    file_aux = openFiles;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    // Percorre os arquivos abertos
    while (file_aux != NULL){
        // Encontrou o handle
        if (file_aux->handle == handle){
            // Offset para ponteiro direto
            if (file_aux->currentBytesPos + offset < diskBlockSize){
                file_aux->currentBytesPos += offset;
            }
            // Offset para ponteiro indireto
            return 1;
        }
        file_aux = file_aux->next;
    }

    return -1;
}

struct file *findFileHandle(int handle){
    struct file *file_aux;
    file_aux = openFiles;

    // Percorre os arquivos abertos
    while (file_aux != NULL){
        // Encontrou o handle
        if (file_aux->handle == handle){
            return file_aux;
        }
        file_aux = file_aux->next;
    }

    return NULL;
}

int t2fs_read(t2fs_file handle, char *buffer, int size){
    int curPosBytes;
    int curPosBlocks;
    int readNewBlock;
    int blockAddress;
    int bytesRead = 0;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    // Aloca bloco para leitura dos setores do disco
    char *block_read = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES)); // Quatro setores

    // Inicializa arquivo e variáveis
    struct file *fileRead = findFileHandle(handle);
    if(fileRead != NULL){
        curPosBytes = fileRead->currentBytesPos;

        //Lê o tamanho em caracteres passado por parâmetro
        readNewBlock = 1;
        while (size > 0){
            // Controla troca de bloco ao terminar de ler um bloco completo
            if (curPosBytes > diskBlockSize-1)
                readNewBlock = 1;

            // Incrementa ponteiro para bloco na primeira lida e nas trocas de bloco
            if (readNewBlock){
                curPosBlocks = curPosBytes % diskBlockSize; // Deslocamento em blocos com relação ao primeiro bloco

                // Ponteiro direto 1
                if (curPosBytes < diskBlockSize)
                    blockAddress = fileRead->record.dataPtr[0];

                // Ponteiro direto 2
                else if (curPosBytes < 2*diskBlockSize)
                    blockAddress = fileRead->record.dataPtr[1];

                // Indireção simples
                /*else if (curPos < (diskBlockSize+2)*diskBlockSize){*/
                    /*blockAddress = block[curPos / diskBlockSize - 2];*/

                // Lê o conteúdo dos setores do bloco para um buffer
                int i;
                for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
                    read_sector(convertBlockToSector(diskBlockSize, blockAddress)+i, block_read+(i*SIZE_SECTOR_BYTES));
                }

                // Incrementa bloco na estrutura do file aberto
                fileRead->currentBlockPos++;

                // Marca para não ler novo bloco até chegar ao fim do novo bloco lido
                readNewBlock = 0;
            }

            // Lê cada caractere
            buffer[curPosBytes] = block_read[curPosBytes];

            // Incrementa os contadores a cada caractere lido
            curPosBytes++;
            bytesRead++;
            size--;
        }

        // Atualiza a posição em que parou de ler o file aberto
        fileRead->currentBytesPos = curPosBytes;
        free(block_read);
        return bytesRead;
    }

    free(block_read);
    return -1;
}

/*t2fs_file t2fs_open(char *name){*/
    /*// Inicializa o disco*/
    /*if(!diskInitialized){*/
        /*t2fs_first(&superblock);*/
    /*}*/

    /*char block[diskBlockSize];*/
    /*int i, j;*/

    /*int posRootBlock = diskCtrlSize + diskFreeBlockSize;*/

   /*// Verifica se um arquivo pode ser criado*/
    /*if (countFiles > MAX_FILES-1){*/
        /*printf("ERROR: Max files limit reached!");*/
    /*}*/

    /*// Coloca nome passado como parâmetro no padrão para efetuar a pesquisa*/
    /*name = validateFilename(name);*/
    /**name = *name | 0x80;*/

    /*// Percorre o diretório raiz*/
    /*for (i = 0; i < diskRootSize; i++) {*/
        /*[>read_block(posRootBlock + i, block);<]*/
        /*read_sector(posRootBlock + i, block);*/

        /*j = 0;*/
        /*while (j < diskBlockSize) {*/
            /*[>printf("i = %d arquivo: %s e Nome pesquisa: %s\n", j, block + j, name);<]*/

            /*if (strcmp(block + j, name) == 0){*/

                /*file *fileFound = (file *) malloc(sizeof(file));*/
                /*fileFound->pos = posRootBlock + i;*/
                /*fileFound->blockPos = j;*/
                /*fileFound->currentPos = 0;*/
                /*fileFound->handle = countFiles;*/

                /*memcpy(&(fileFound->record), block+j, sizeof(struct t2fs_record));*/
                /*files[countFiles] = fileFound;*/

                /*countFiles++;*/

                /*return fileFound->handle;*/
            /*}*/

            /*j = j + diskFileEntrySize;*/
        /*}*/
    /*}*/

    /*return -1;*/
/*}*/

/*int t2fs_close(t2fs_file handle){*/
    /*int i, j;*/

    /*// Inicializa o disco*/
    /*if(!diskInitialized){*/
        /*t2fs_first(&superblock);*/
    /*}*/

    /*for (i = 0; i < MAX_FILES; i++){*/
        /*if (files[i]->handle == handle){*/
            /*free(files[i]);*/

            /*for (j = 0; j < MAX_FILES; j++) {*/
                /*files[j-1] = files[j];*/
            /*}*/

            /*countFiles--;*/

            /*return 0;*/
        /*}*/
    /*}*/

    /*return -1;*/
/*}*/

/*int diskReserveBlock(){*/
    /*char block[diskBlockSize];*/
    /*int i;*/
    /*int bitmapBlock, dataInit;*/
    /*int posByte, posBit, auxByte;*/

    /*bitmapBlock = diskCtrlSize;*/
    /*dataInit = diskCtrlSize + diskFreeBlockSize + diskRootSize;*/

    /*[>read_block(bitmapBlock, block);<]*/
    /*for (i = dataInit; i < diskSize; i++) {*/
        /*if (i > (bitmapBlock-diskCtrlSize+1)*diskBlockSize*8){*/
            /*bitmapBlock++;*/
            /*[>read_block(bitmapBlock, block);<]*/
        /*}*/

        /*posByte = (i - (bitmapBlock-diskCtrlSize)) / 8;*/
        /*posBit = 7 - (i % 8);*/

        /*auxByte = block[posByte] &  (1 << posBit);*/

        /*if (auxByte == 0){*/
            /*markBitmap(i, 1);*/
            /*return i;*/
        /*}*/
    /*}*/

    /*return -1;*/
/*}*/

/*int t2fs_write(t2fs_file handle, char *buffer, int size){*/
    /*int origSize;*/
    /*int maxSizePossible;*/
    /*int sizeLeft, spaceLeft, actualSize;*/
    /*int blockAddress, indBlockAddress;*/
    /*int positionPointer;*/
    /*int i;*/

    /*// Inicializa o disco*/
    /*if(!diskInitialized){*/
        /*t2fs_first(&superblock);*/
    /*}*/

    /*char block[diskBlockSize];*/

    /*// Busca arquivo a ser escrito e atualiza os ponteiros*/
    /*file *fileWrite = findFile(handle);*/
    /*origSize = fileWrite->record.bytesFileSize;*/

    /*// Controla o máximo possível a ser escrito no arquivo*/
    /*maxSizePossible = 2*diskBlockSize + diskBlockSize*diskBlockSize + diskBlockSize*diskBlockSize*diskBlockSize;*/
    /*if (size + origSize > maxSizePossible){*/
        /*printf("Error: Limit size reached, isn't possible to write at such file length\n");*/
        /*return -1;*/
    /*}*/

    /*// Controla os tamanhos restantes para escrita no disco*/
    /*sizeLeft = size;*/
    /*spaceLeft = (fileWrite->record.blocksFileSize * diskBlockSize) - fileWrite->record.bytesFileSize;*/
    /*actualSize = fileWrite->record.bytesFileSize;*/

    /*// Escreve enquanto houver espaço*/
    /*while (sizeLeft > 0){*/
        /*// Alocar mais espaço*/
        /*if (spaceLeft == 0){*/
            /*positionPointer = 0;*/

            /*blockAddress = diskReserveBlock(); // Aloca um novo bloco no disco*/
            /*if (blockAddress < 1){*/
                /*printf("Error: Wasn't possible to allocate a new block\n");*/
                /*return -1;*/
            /*}*/

            /*// Atualiza novo espaço disponível no disco*/
            /*spaceLeft = diskBlockSize;*/
            /*fileWrite->record.blocksFileSize++;*/

            /*// Ponteiro direto 1*/
            /*if (actualSize == 0){*/
                /*fileWrite->record.dataPtr[0] = blockAddress;*/
            /*}*/
            /*// Ponteiro direto 2*/
            /*else if (actualSize < 2*diskBlockSize){*/
                /*fileWrite->record.dataPtr[1] = blockAddress;*/
            /*}*/
            /*// Aloca ponteiro indireto*/
            /*else if (actualSize == 2*diskBlockSize){*/
                /*indBlockAddress = diskReserveBlock();*/
                /*if (indBlockAddress < 1){*/
                    /*printf("Error: Wasn't possible to allocate a new indirection block");*/
                /*}*/

                /*// Inicializa bloco de índice*/
                /*fileWrite->record.singleIndPtr = indBlockAddress;*/
                /*block[0] = diskBlockSize;*/
                /*for (i = 1; i < diskBlockSize; i++){*/
                    /*block[i] = 0;*/
                /*}*/

                /*// Grava bloco de índice no disco*/
                /*[>write_block(indBlockAddress, block);<]*/
            /*}*/
            /*// Utiliza ponteiro indireto*/
            /*else if (actualSize < (diskBlockSize+2)*diskBlockSize){*/
                /*[>read_block(fileWrite->record.singleIndPtr, block);<]*/
                /*block[fileWrite->record.blocksFileSize - 3] = blockAddress;*/
                /*[>write_block(fileWrite->record.singleIndPtr, block);<]*/
            /*}*/
            /*else {*/
                /*printf("Error: Limit Reached. Double indirection pointers were not implemented yet in this version\n");*/
                /*return -1;*/
            /*}*/
        /*}*/
        /*// Usar o espaço restante*/
        /*else {*/
            /*positionPointer = actualSize % diskBlockSize;*/
            /*[>printf("positionPointer %d\n", positionPointer);<]*/

            /*if (actualSize < diskBlockSize){*/
                /*blockAddress = fileWrite->record.dataPtr[0];*/
            /*}*/
            /*else if (actualSize < 2*diskBlockSize){*/
                /*blockAddress = fileWrite->record.dataPtr[1];*/
            /*}*/
            /*else if (actualSize < (diskBlockSize+2)*diskBlockSize){*/
                /*[>read_block(fileWrite->record.singleIndPtr, block);<]*/
                /*blockAddress = block[fileWrite->record.blocksFileSize - 3];*/
            /*}*/
            /*else {*/
                /*printf("Error: Limit Reached. Double indirection pointers were not implemented yet in this version\n");*/
                /*return -1;*/
            /*}*/

            /*[>printf("actualSize %d, blockAddress %d, diskBlockSize %d\n", actualSize, blockAddress, diskBlockSize);<]*/
            /*// Lê o bloco*/
            /*[>read_block(blockAddress, block);<]*/
        /*}*/

        /*// Preenche o bloco*/
        /*i = 0;*/
        /*while (positionPointer < diskBlockSize && sizeLeft > 0){*/
            /*block[positionPointer] = block[i];*/
            /*i++;*/
            /*positionPointer++;*/
            /*sizeLeft--;*/
            /*spaceLeft--;*/
        /*}*/

        /*// Escreve no disco*/
        /*[>write_block(blockAddress, block);<]*/
        /*actualSize = origSize + size - sizeLeft;*/
    /*}*/

    /*// Atualiza arquivo*/
    /*fileWrite->record.bytesFileSize = actualSize;*/
    /*fileWrite->record.blocksFileSize = actualSize / diskBlockSize;*/
    /*if (positionPointer < diskBlockSize){*/
        /*fileWrite->record.blocksFileSize++;*/
    /*}*/

    /*[>read_block(fileWrite->pos, block);<]*/
    /*memcpy(block+fileWrite->blockPos, &(fileWrite->record), FILE_REGISTER);*/
    /*[>write_block(fileWrite->pos, block);<]*/

    /*return size;*/
/*}*/
