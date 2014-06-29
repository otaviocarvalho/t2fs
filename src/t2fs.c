#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apidisk.h"
#include "t2fs.h"

#define MAX_BLOCK_SIZE 65536
#define FILE_REGISTER 64
#define MAX_FILES 20
#define MAX_FILE_NAME 40

#define SIZE_SECTOR_BYTES 256
#define SIZE_SUPERBLOCK_BYTES 256

typedef struct {
    t2fs_file handle;
    unsigned int currentPos;
    unsigned int pos;
    unsigned int blockPos;
    struct t2fs_record record;
} file;

unsigned long int diskInitialized = 0;
unsigned short diskVersion;
unsigned short diskSuperBlockSize;
unsigned long int diskSize;
unsigned long int diskBlocksNumber;
unsigned long int diskBlockSize;
struct t2fs_record diskBitMapReg;
struct t2fs_record diskRootDirReg;
struct t2fs_superbloco superBlockBuffer;

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

file *files[20];
int countFiles = 0;

int t2fs_first(struct t2fs_superbloco *findStruct);

char *t2fs_identify(void){
    char *str = malloc(sizeof(char) * 90);

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    str = memcpy(str, "Otavio (180470) - Lisandro (143764) - Tagline (180229)\0", 90);

    return str;
}

void initDisk(struct t2fs_superbloco *sblock){

    diskVersion = (unsigned short) sblock->Version;
    diskSuperBlockSize = (unsigned short) sblock->SuperBlockSize; // diskSize, tamanho do superbloco em blocos/setores
    diskSize = (unsigned long int) sblock->DiskSize; // blockSize * diskSize, tamanho do disco em bytes
    diskBlocksNumber = (unsigned long int) sblock->NofBlocks;
    diskBlockSize = (unsigned long int) sblock->BlockSize;
    // BitMapReg
    diskBitMapReg = sblock->BitMapReg;
    // RootDirReg
    diskRootDirReg = sblock->RootDirReg;

    // Teste copia do superbloco para uma variável global
    memcpy(&superBlockBuffer, sblock, sizeof(struct t2fs_superbloco));
    /*printf("read superblock copy struct: %c%c%c%c\n", superBlockBuffer.Id[0], superBlockBuffer.Id[1], superBlockBuffer.Id[2], superBlockBuffer.Id[3]);*/

    // Teste leitura dos campos do superbloco
    /*printf("read superblock struct: %c%c%c%c\n", sblock->Id[0], sblock->Id[1], sblock->Id[2], sblock->Id[3]);*/
    /*printf("version: %x\n", diskVersion);*/
    /*printf("SuperBlockSize: %d\n", diskSuperBlockSize);*/
    /*printf("diskSize: %ld\n", diskSize);*/
    /*printf("diskBlocksNumber: %ld\n", diskBlocksNumber);*/
    /*printf("blockSize: %ld\n", diskBlockSize);*/
    /*printf("diskRootDirReg name: %s\n", diskRootDirReg.name);*/

    // Teste de impressão do t2fs_record do diretório raíz
    /*printf("\nprint bitmap t2fs_record:\n");*/
    /*printf("%s\n", diskBitMapReg.name);*/
    /*printf("blocksFileSize: %ld\n", (unsigned long int) diskBitMapReg.blocksFileSize);*/
    /*printf("bytesFileSize: %ld\n", (unsigned long int) diskBitMapReg.bytesFileSize);*/
    /*printf("dataPtr[0]: %x\n", diskBitMapReg.dataPtr[0]);*/
    /*printf("dataPtr[1]: %x\n", diskBitMapReg.dataPtr[1]);*/
    /*printf("singleIndPtr: %x\n", diskBitMapReg.singleIndPtr);*/
    /*printf("doubleIndPtr: %x\n", diskBitMapReg.doubleIndPtr);*/

    // Teste de impressão do bitmap
    /*printf("print bitmap content:\n");*/
    /*unsigned char test_char;*/
    /*int i;*/
    /*for (i = 0; i < 128; i++) {*/
        /*memcpy(&test_char, &diskRootDirReg.dataPtr[0]+i, sizeof(unsigned char));*/
        /*printf("%x\n", test_char);*/
    /*}*/

    // Teste de leitura do primeiro arquivo do diretório raíz
    /*struct t2fs_record *test_record = malloc(sizeof(struct t2fs_record)) ;*/
    /*memcpy(test_record, &diskRootDirReg.dataPtr[0]+sizeof(struct t2fs_record)*0, sizeof(struct t2fs_record));*/
    /*printf("\nfilename: %s\n", test_record->name);*/
    /*printf("typeval: %x\n", test_record->TypeVal);*/
    /*printf("blocksFileSize: %ld\n", (unsigned long int) test_record->blocksFileSize);*/

    diskInitialized = 1;
}

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

    // Teste de mudança no superbloco, escrita no disco e posterior leitura do superbloco
    /*printf("valor do primeiro byte do bitmap alterado para %x\n", 4);*/
    /*superBlockBuffer.RootDirReg.dataPtr[0] = 4;*/
    /*memcpy(find, &superBlockBuffer, sizeof(struct t2fs_superbloco));*/
    /*write_sector(0, find);*/
    /*read_sector(0, find);*/
    /*memcpy(findStruct, find, sizeof(struct t2fs_superbloco));*/
    /*printf("read superblock struct: %c%c%c%c\n", findStruct->Id[0], findStruct->Id[1], findStruct->Id[2], findStruct->Id[3]);*/
    /*printf("valor do primeiro byte do bitmap lido do disco %x\n", findStruct->RootDirReg.dataPtr[0]);*/

    return 0;
}

int markBlockBitmap(int block){
    /*char block[diskBlockSize];*/
    /*int posBlock;*/
    /*int posBit;*/
    /*int posByte;*/

    // Encontra posição do bloco no bitmap
    /*posBlock = diskCtrlSize + (pos / (8 * diskBlockSize));*/
    /*read_block(posBlock, block);*/
    /*read_sector(posBlock, block);*/

    // Encontra byte e bit especifico a serem modificados
    /*posByte = posBlock / 8;*/
    /*posBit = 7 - (posBlock % 8);*/

    // Faz set ou unset do bit especificado
    /*if (setbit){*/
        /*block[posByte] = block[posByte] | (0x1 << posBit);*/
    /*}*/
    /*else {*/
        /*block[posByte] = block[posByte] & (0xFE << posBit);*/
    /*}*/

    return 0;
}

char *validateFilename(char *name){
    char* validatedName = malloc(sizeof(char)*MAX_FILE_NAME);
    int i = 0, j = 0;

    // Percorre até chegar ao fim da string
    while (name[i] != 0){
        validatedName[i] = name[i];
        i++;
    }

    // Preenche string com zeros até o fim
    for (j=i; j < MAX_FILE_NAME-1; j++){
        validatedName[j] = 0;
    }

    return validatedName;
}

int t2fs_next(struct t2fs_superbloco *find, struct t2fs_record *rec){
    /*printf("entrou t2fs_next\n");*/

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    char block[diskBlockSize];
    int i, j;
    int blockPos = 0, offsetBlockPos = 0;
    int firstValid = 0;
    int findNextValid = 0;

    int posRootBlock = diskCtrlSize + diskFreeBlockSize;

    // Verifica se já encontrou o primeiro arquivo válido
    /*if (find->fileName == NULL){*/
        /*[>printf("ENTROU NULL\n");<]*/
        /*firstValid = 1;*/
        /*blockPos = 0;*/
        /*offsetBlockPos = 0;*/
    /*}*/
    /*else {*/
        /*blockPos = find->blockPos;*/
        /*offsetBlockPos = find->offsetBlockPos;*/
    /*}*/

    // Percorre o diretório raiz
    for (i = blockPos; i < diskRootSize; i++) {
        /*read_block(posRootBlock + i, block);*/
        read_sector(posRootBlock + i, block);

        j = offsetBlockPos;
        while (j < diskBlockSize) {
            /*printf("entrou while i (diskRootSize)i: %d j (diskBlockSize): %d\n",i,j);*/
            /*printf("lendo: %s e procurando: %s | firstValid %d\n", block + j, find->fileName, firstValid);*/
            // Se for para encontrar o primeiro ou o próximo arquivo válido
            if (firstValid || findNextValid){
                // Verifica bit na primeira letra do arquivo
                if ( ((block + j)[0] & 0x80) == 0x80){
                    memcpy(rec, block+j, sizeof(struct t2fs_record));

                    // Cria record auxiliar para receber o bloco atual
                    struct t2fs_record *recAux = malloc(sizeof(struct t2fs_record));
                    memcpy(recAux, block+j, sizeof(struct t2fs_record));

                    // Copia nome do arquivo para estrutura find
                    /*find->fileName = malloc(40*sizeof(char));*/
                    /*memcpy(find->fileName, recAux->name, 40*sizeof(char));*/

                    // Acerta ponteiros para que retornem no mesmo ponto
                    /*find->blockPos = i;*/
                    /*find->offsetBlockPos = j;*/

                    /*printf("Encontrou %s e retornou %x\n", find->fileName, (unsigned char) find->fileName[0]);*/
                    /*printf("i %d j %d\n", find->blockPos, find->offsetBlockPos);*/

                    return 0;
                }
            }
            // Se for para encontrar o arquivo válido atual
            /*else if (strcmp(block + j, find->fileName) == 0){*/
                /*printf("FOUND WORD %s\n", find->fileName);*/
                /*findNextValid = 1;*/
            /*}*/

            j = j + diskFileEntrySize;
        }
    }

    return 1;
}

int markBitmap(int pos, short int setbit){
    char block[diskBlockSize];
    int posBlock;
    int posBit;
    int posByte;

    // Encontra posição do bloco no bitmap
    posBlock = diskCtrlSize + (pos / (8 * diskBlockSize));
    /*read_block(posBlock, block);*/
    read_sector(posBlock, block);

    // Encontra byte e bit especifico a serem modificados
    posByte = posBlock / 8;
    posBit = 7 - (posBlock % 8);

    // Faz set ou unset do bit especificado
    if (setbit){
        block[posByte] = block[posByte] | (0x1 << posBit);
    }
    else {
        block[posByte] = block[posByte] & (0xFE << posBit);
    }

    return 0;
}

int diskInsertRecord(struct t2fs_record *record){
    char block[diskBlockSize];
    int i, j;
    int found = 0;

    int posRootBlock = diskCtrlSize + diskFreeBlockSize;

    // Percorre o diretório raiz
    for (i = 0; i < diskRootSize; i++) {
        /*read_block(posRootBlock + i, block);*/
        read_sector(posRootBlock + i, block);

        /*printf("posRootBlock + i: %d", posRootBlock + i);*/

        j = 0;
        while (j < diskBlockSize && !found) {
            // Substitui se não for um registro válido (não tiver bit ativo)
            if ( (unsigned char) block[j] < 0xA1 || (unsigned char) block[j] > 0xFA){
                memcpy(block+j, record, sizeof(*record));
                /*printf("\n%x\n", (unsigned char) block[j]);*/
                block[j] = block[j] | 128;
                /*printf("\n%x\n", (unsigned char) block[j]);*/
                found = 1;
            }

            j = j + diskFileEntrySize;
        }

        if (found){
            /*printf("criando i = %d arquivo: %s\n", j, record->name);*/
            /*write_block(posRootBlock + i, block);*/
            write_sector(posRootBlock + i, block);
            markBitmap(posRootBlock + i, 1);
            return 0;
        }
    }

    return -1;
}

t2fs_file t2fs_create (char *name){
    char* validatedName;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    // Verifica se um arquivo pode ser criado
    if (countFiles > MAX_FILES-1){
        printf("ERROR: Max files limit reached!");
    }

    // Valida nome dentro do padrão especificado
    validatedName = validateFilename(name);

    // Monta arquivo a ser salvo no disco
    file* newFile = malloc(sizeof(file));
    memcpy(newFile->record.name, validatedName, MAX_FILE_NAME);
    newFile->record.name[39] = 0;
    newFile->record.blocksFileSize = 0;
    newFile->record.bytesFileSize = 0;
    newFile->record.dataPtr[0] = 0;
    newFile->record.dataPtr[1] = 0;
    newFile->record.singleIndPtr = 0;
    newFile->record.doubleIndPtr = 0;

    // Salva arquivo no disco e atualiza apontadores
    diskInsertRecord(&newFile->record);
    newFile->handle = countFiles;
    files[countFiles] = newFile;
    countFiles++;

    return newFile->handle;
}

t2fs_file t2fs_open(char *name){
    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    char block[diskBlockSize];
    int i, j;

    int posRootBlock = diskCtrlSize + diskFreeBlockSize;

   // Verifica se um arquivo pode ser criado
    if (countFiles > MAX_FILES-1){
        printf("ERROR: Max files limit reached!");
    }

    // Coloca nome passado como parâmetro no padrão para efetuar a pesquisa
    name = validateFilename(name);
    *name = *name | 0x80;

    // Percorre o diretório raiz
    for (i = 0; i < diskRootSize; i++) {
        /*read_block(posRootBlock + i, block);*/
        read_sector(posRootBlock + i, block);

        j = 0;
        while (j < diskBlockSize) {
            /*printf("i = %d arquivo: %s e Nome pesquisa: %s\n", j, block + j, name);*/

            if (strcmp(block + j, name) == 0){

                file *fileFound = (file *) malloc(sizeof(file));
                fileFound->pos = posRootBlock + i;
                fileFound->blockPos = j;
                fileFound->currentPos = 0;
                fileFound->handle = countFiles;

                memcpy(&(fileFound->record), block+j, sizeof(struct t2fs_record));
                files[countFiles] = fileFound;

                countFiles++;

                return fileFound->handle;
            }

            j = j + diskFileEntrySize;
        }
    }

    return -1;
}

int t2fs_close(t2fs_file handle){
    int i, j;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    for (i = 0; i < MAX_FILES; i++){
        if (files[i]->handle == handle){
            free(files[i]);

            for (j = 0; j < MAX_FILES; j++) {
                files[j-1] = files[j];
            }

            countFiles--;

            return 0;
        }
    }

    return -1;
}

file *findFile(t2fs_file handle){
    int i;
    for(i = 0; i < countFiles; i++){
        if(files[i]->handle == handle){
            return files[i];
        }
    }

    return NULL;
}

int t2fs_delete(char *name){
    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    file *deleteFile;
    int numBlocks;
    char block[diskBlockSize];
    int i;

    // Abre o arquivo a ser deletado
    int handle = t2fs_open(name);

    // Testa se encontrou o arquivo
    if (handle >= 0){
        // Encontra o arquivo dentre os abertos
        deleteFile = findFile(handle);
        /*printf("filename check %c\n", deleteFile->record.name[0] & 0x7f);*/

        // Descobre o número de blocos a serem deletados
        numBlocks = deleteFile->record.blocksFileSize;

        // Deleta blocos apontados por ponteiros diretos
        for (i = 0; i < 2; i++) {
            if (numBlocks > 0){
                markBitmap(deleteFile->record.dataPtr[i],0);
                numBlocks--;
            }
            else {
                break;
            }
        }

        // Deleta blocos apontados por ponteiros de indireção simples
        if (numBlocks > 0){
            markBitmap(deleteFile->record.singleIndPtr, 0);
            /*read_block(deleteFile->record.singleIndPtr, block);*/
            read_sector(deleteFile->record.singleIndPtr, block);

            for (i = 0; i < diskBlockSize; i++) {
                if (numBlocks > 0){
                    markBitmap(block[i], 0);
                    numBlocks--;
                }
                else {
                    break;
                }
            }
        }

        // Deleta blocos apontados por ponteiros de indireção dupla

        // Invalida registro pelo bit no nome
        /*read_block(deleteFile->pos, block);*/
        read_sector(deleteFile->pos, block);
        block[deleteFile->blockPos] = block[deleteFile->blockPos] & 0x7F;
        /*write_block(deleteFile->pos, block);*/
        write_sector(deleteFile->pos, block);

        // Fecha arquivo
        t2fs_close(handle);

        return 0;
    }
    else {
        return -1;
    }
}

int t2fs_seek(t2fs_file handle, unsigned int offset){
    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    file *file = findFile(handle);

    if (file != NULL){
        if (offset < file->record.bytesFileSize){
            file->currentPos = offset;
            return 0;
        }
        else {
            printf("Error: Offset greater than the size of the file\n");
        }
    }
    else {
        printf("Error: File not found!\n");
    }

    return -1;
}

int diskReserveBlock(){
    char block[diskBlockSize];
    int i;
    int bitmapBlock, dataInit;
    int posByte, posBit, auxByte;

    bitmapBlock = diskCtrlSize;
    dataInit = diskCtrlSize + diskFreeBlockSize + diskRootSize;

    /*read_block(bitmapBlock, block);*/
    for (i = dataInit; i < diskSize; i++) {
        if (i > (bitmapBlock-diskCtrlSize+1)*diskBlockSize*8){
            bitmapBlock++;
            /*read_block(bitmapBlock, block);*/
        }

        posByte = (i - (bitmapBlock-diskCtrlSize)) / 8;
        posBit = 7 - (i % 8);

        auxByte = block[posByte] &  (1 << posBit);

        if (auxByte == 0){
            markBitmap(i, 1);
            return i;
        }
    }

    return -1;
}

int t2fs_write(t2fs_file handle, char *buffer, int size){
    int origSize;
    int maxSizePossible;
    int sizeLeft, spaceLeft, actualSize;
    int blockAddress, indBlockAddress;
    int positionPointer;
    int i;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    char block[diskBlockSize];

    // Busca arquivo a ser escrito e atualiza os ponteiros
    file *fileWrite = findFile(handle);
    origSize = fileWrite->record.bytesFileSize;

    // Controla o máximo possível a ser escrito no arquivo
    maxSizePossible = 2*diskBlockSize + diskBlockSize*diskBlockSize + diskBlockSize*diskBlockSize*diskBlockSize;
    if (size + origSize > maxSizePossible){
        printf("Error: Limit size reached, isn't possible to write at such file length\n");
        return -1;
    }

    // Controla os tamanhos restantes para escrita no disco
    sizeLeft = size;
    spaceLeft = (fileWrite->record.blocksFileSize * diskBlockSize) - fileWrite->record.bytesFileSize;
    actualSize = fileWrite->record.bytesFileSize;

    // Escreve enquanto houver espaço
    while (sizeLeft > 0){
        // Alocar mais espaço
        if (spaceLeft == 0){
            positionPointer = 0;

            blockAddress = diskReserveBlock(); // Aloca um novo bloco no disco
            if (blockAddress < 1){
                printf("Error: Wasn't possible to allocate a new block\n");
                return -1;
            }

            // Atualiza novo espaço disponível no disco
            spaceLeft = diskBlockSize;
            fileWrite->record.blocksFileSize++;

            // Ponteiro direto 1
            if (actualSize == 0){
                fileWrite->record.dataPtr[0] = blockAddress;
            }
            // Ponteiro direto 2
            else if (actualSize < 2*diskBlockSize){
                fileWrite->record.dataPtr[1] = blockAddress;
            }
            // Aloca ponteiro indireto
            else if (actualSize == 2*diskBlockSize){
                indBlockAddress = diskReserveBlock();
                if (indBlockAddress < 1){
                    printf("Error: Wasn't possible to allocate a new indirection block");
                }

                // Inicializa bloco de índice
                fileWrite->record.singleIndPtr = indBlockAddress;
                block[0] = diskBlockSize;
                for (i = 1; i < diskBlockSize; i++){
                    block[i] = 0;
                }

                // Grava bloco de índice no disco
                /*write_block(indBlockAddress, block);*/
            }
            // Utiliza ponteiro indireto
            else if (actualSize < (diskBlockSize+2)*diskBlockSize){
                /*read_block(fileWrite->record.singleIndPtr, block);*/
                block[fileWrite->record.blocksFileSize - 3] = blockAddress;
                /*write_block(fileWrite->record.singleIndPtr, block);*/
            }
            else {
                printf("Error: Limit Reached. Double indirection pointers were not implemented yet in this version\n");
                return -1;
            }
        }
        // Usar o espaço restante
        else {
            positionPointer = actualSize % diskBlockSize;
            /*printf("positionPointer %d\n", positionPointer);*/

            if (actualSize < diskBlockSize){
                blockAddress = fileWrite->record.dataPtr[0];
            }
            else if (actualSize < 2*diskBlockSize){
                blockAddress = fileWrite->record.dataPtr[1];
            }
            else if (actualSize < (diskBlockSize+2)*diskBlockSize){
                /*read_block(fileWrite->record.singleIndPtr, block);*/
                blockAddress = block[fileWrite->record.blocksFileSize - 3];
            }
            else {
                printf("Error: Limit Reached. Double indirection pointers were not implemented yet in this version\n");
                return -1;
            }

            /*printf("actualSize %d, blockAddress %d, diskBlockSize %d\n", actualSize, blockAddress, diskBlockSize);*/
            // Lê o bloco
            /*read_block(blockAddress, block);*/
        }

        // Preenche o bloco
        i = 0;
        while (positionPointer < diskBlockSize && sizeLeft > 0){
            block[positionPointer] = block[i];
            i++;
            positionPointer++;
            sizeLeft--;
            spaceLeft--;
        }

        // Escreve no disco
        /*write_block(blockAddress, block);*/
        actualSize = origSize + size - sizeLeft;
    }

    // Atualiza arquivo
    fileWrite->record.bytesFileSize = actualSize;
    fileWrite->record.blocksFileSize = actualSize / diskBlockSize;
    if (positionPointer < diskBlockSize){
        fileWrite->record.blocksFileSize++;
    }

    /*read_block(fileWrite->pos, block);*/
    memcpy(block+fileWrite->blockPos, &(fileWrite->record), FILE_REGISTER);
    /*write_block(fileWrite->pos, block);*/

    return size;
}

int t2fs_read(t2fs_file handle, char *buffer, int size){
    int curPos, blockPos;
    int blockRead = 0, bytesRead = 0;
    int blockAddress;

     // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    char block[diskBlockSize];

    // Inicializa arquivo e variáveis
    file *fileRead = findFile(handle);
    curPos = fileRead->currentPos;

    // Lê o tamanho em caracteres passado por parâmetro
    while (size > 0){

        if (!blockRead){
            blockPos = curPos % diskBlockSize;

            // Ponteiro direto 1
            if (curPos < diskBlockSize){
                blockAddress = fileRead->record.dataPtr[0];
            }
            // Ponteiro direto 2
            else if (curPos < 2*diskBlockSize){
                blockAddress = fileRead->record.dataPtr[1];
            }
            // Indireção simples
            else if (curPos < (diskBlockSize+2)*diskBlockSize){
                /*read_block(fileRead->record.singleIndPtr, block);*/
                blockAddress = block[curPos / diskBlockSize - 2];
            }
            // Indireção dupla
            else {
                printf("Error: Limit Reached. Double indirection pointers were not implemented yet in this version\n");
                return -1;
            }

            /*read_block(blockAddress, block);*/
            blockRead = 1;
        }

        buffer[bytesRead] = block[blockPos-1];
        bytesRead++;
        size--;

        curPos++;
        blockPos++;
        if (blockPos > diskBlockSize){
            blockRead = 0;
        }
    }

    fileRead->currentPos = curPos;
    return bytesRead;
}

int t2fs_read_old(t2fs_file handle, char *buffer, int size){
    int curPos, blockPos;
    int blockRead = 0, bytesRead = 0;
    int blockAddress;

     // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    char block[diskBlockSize];

    // Inicializa arquivo e variáveis
    file *fileRead = findFile(handle);
    curPos = fileRead->currentPos;

    // Lê o tamanho em caracteres passado por parâmetro
    while (size > 0){

        if (!blockRead){
            blockPos = curPos % diskBlockSize;

            // Ponteiro direto 1
            if (curPos < diskBlockSize){
                blockAddress = fileRead->record.dataPtr[0];
            }
            // Ponteiro direto 2
            else if (curPos < 2*diskBlockSize){
                blockAddress = fileRead->record.dataPtr[1];
            }
            // Indireção simples
            else if (curPos < (diskBlockSize+2)*diskBlockSize){
                /*read_block(fileRead->record.singleIndPtr, block);*/
                blockAddress = block[curPos / diskBlockSize - 2];
            }
            // Indireção dupla
            else {
                printf("Error: Limit Reached. Double indirection pointers were not implemented yet in this version\n");
                return -1;
            }

            /*read_block(blockAddress, block);*/
            blockRead = 1;
        }

        buffer[bytesRead] = block[blockPos-1];
        bytesRead++;
        size--;

        curPos++;
        blockPos++;
        if (blockPos > diskBlockSize){
            blockRead = 0;
        }
    }

    fileRead->currentPos = curPos;
    return bytesRead;
}
