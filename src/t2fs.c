#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apidisk.h"
#include "t2fs.h"

/*#define MAX_BLOCK_SIZE 65536*/
/*#define FILE_REGISTER 64*/
#define MAX_FILES 20
#define MAX_FILE_NAME 39

#define SIZE_SECTOR_BYTES 256
/*#define SIZE_SUPERBLOCK_BYTES 256*/

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

unsigned long int diskInitialized = 0;
unsigned short diskVersion;
unsigned short diskSuperBlockSize;
unsigned long int diskSize;
unsigned long int diskBlocksNumber;
unsigned long int diskBlockSize;
struct t2fs_record diskBitMapReg;
struct t2fs_record diskRootDirReg;
struct t2fs_superbloco superblock;

struct file *openFiles;
int openFilesMap[20] = {0};
int countOpenFiles = 0;

// Protótipos de funções auxiliares
struct t2fs_record *findFile(char *name);
int t2fs_first(struct t2fs_superbloco *findStruct);

// Função auxiliar que converte o valor de um bloco para o seu equivalente em trilhas físicas do disco
int convertBlockToSector(int block_size, int block_number){
    return (block_number * (block_size / SIZE_SECTOR_BYTES)) + 1;
}

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
    int i;
    unsigned char block_copy;

    // Encontra byte e bit especifico a serem modificados
    posByte = block / 8; // Encontra o byte no qual escrever (8 blocos representados por byte)
    posBit = block % 8; // Encontra em qual bit do byte escrever (representação da direita para a esquerda, do 0 ao 7, bit 7 ativo == 0x80)

    // Endereçar ponteiros diretos
    int blockAddress;
    if (block < (diskBlockSize/8)){
        blockAddress = diskBitMapReg.dataPtr[0];
    }
    // Endereçar ponteiros indiretos
    else if (block < 2*diskBlockSize/8){
        blockAddress = diskBitMapReg.dataPtr[1];
    }

    // Lê bloco do disco físico
    char *find = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES));
    for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
        read_sector(convertBlockToSector(diskBlockSize, blockAddress)+i, find+(i*SIZE_SECTOR_BYTES));
    }

    memcpy(&block_copy, find+(posByte*sizeof(unsigned char)), sizeof(unsigned char));
    // Faz set ou unset do bit especificado
    if (setbit){
        block_copy = block_copy | (0x1 << posBit);
    }
    else {
        block_copy = block_copy & (0xFE << posBit);
    }

    // Salva o valor no bitmap
    memcpy(find+(posByte*sizeof(unsigned char)), &block_copy, sizeof(unsigned char));
    memcpy(&block_copy, find+(posByte*sizeof(unsigned char)), sizeof(unsigned char));

    // Persiste o bitmap alterado no disco
    for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
        write_sector(convertBlockToSector(diskBlockSize, blockAddress)+i, find+(i*SIZE_SECTOR_BYTES));
    }
}

// Procura e reserva uma nova posição para um bloco no bitmap
int diskBitmapReserveBlock(){;
    int i, j, k;
    int posByte, posBit, auxByte;
    char *find = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES));
    int newBlock;

    // Busca dados iniciais do bitmap
    int blockBytesCont = 0;
    int readNewBlock = 1;
    int size = diskBitMapReg.bytesFileSize;
    for (i = 0; i < size; i++) {
        // Primeiro ponteiro direto
        if (i < diskBlockSize && readNewBlock){
            // Lê todo o bloco (4 setores)
            for (j = 0; j < diskBlockSize/SIZE_SECTOR_BYTES; j++) {
                read_sector(convertBlockToSector(diskBlockSize, diskBitMapReg.dataPtr[0])+j, find+(j*SIZE_SECTOR_BYTES));
            }

            readNewBlock = 0;
            blockBytesCont = 0;
        }
        // Segundo ponteiro direto
        else if (i < 2*diskBlockSize && readNewBlock){
            // Lê todo o bloco (4 setores)
            for (j = 0; j < diskBlockSize/SIZE_SECTOR_BYTES; j++) {
                read_sector(convertBlockToSector(diskBlockSize, diskBitMapReg.dataPtr[1])+j, find+(j*SIZE_SECTOR_BYTES));
            }

            readNewBlock = 0;
            blockBytesCont = 0;
        }
        // Ponteiros indiretos

        // Percorre o byte atual
        for (k = 0; k < 8; k++) {
            posByte = i;
            posBit = k;
            auxByte = find[posByte] & (1 << posBit);

            // Retorna ao encontrar um espaço vazio
            if (auxByte == 0){
                newBlock = (posByte*8)+posBit;
                markBlockBitmap(newBlock, SET_BIT);

                free(find);
                // Retorna o número correspondente ao bloco vazio
                return newBlock;
            }

        }

        blockBytesCont++; // Contador que marca quando um novo bloco deve ser lido
        if (blockBytesCont > diskBlockSize){
            readNewBlock = 1;
        }
    }

    free(find);
    return -1;
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

    // Inicialização do diretório corrente do disco a partir da variável global do disco RootDirReg
    diskRootDirReg = sblock->RootDirReg;

    // Ativa a flag de conclusão da inicialização do disco
    diskInitialized = 1;
}

// Função que lê o superbloco do disco na inicialização
int t2fs_first(struct t2fs_superbloco *findStruct){
    int status_read;
    char *find_teste = malloc(SIZE_SECTOR_BYTES); // Lê um setor do disco 'físico'

    status_read = read_sector(0, find_teste);

    if (status_read != 0){
        return -1;
    }
    else {
        memcpy(findStruct, find_teste, sizeof(struct t2fs_superbloco));
    }

    if(!diskInitialized){
        initDisk(findStruct);
    }

    free(find_teste);

    return 0;
}

// Função que percorre a árvore em busca da estrutura de dados, que representa um arquivo, em função do seu caminho
struct t2fs_record *findFile(char *name){
    char *token, *string, *tofree;
    struct t2fs_record *file = malloc(sizeof(struct t2fs_record));
    char *find_folder = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES));
    int found, count_bytes;
    int i;

    tofree = string = strdup(name);
    if (string != NULL){
        // Testa leitura do primeiro arquivo do diretório raiz "/"
        if (strcmp(&string[0],"/")){
            for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
                read_sector(convertBlockToSector(diskBlockSize, diskRootDirReg.dataPtr[0])+i, find_folder+(i*SIZE_SECTOR_BYTES));
            }
            memcpy(file, find_folder, sizeof(struct t2fs_record));
            string++;
        }
        else {
            return NULL;
        }

        // Testa a leitura dos subdiretórios ou arquivos
        while((token = strsep(&string, "/")) != NULL){

            // Procura pelo nome no primeiro setor
            found = 0;
            count_bytes = 0;
            while ((count_bytes < SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES)) && !found){
                memcpy(file, find_folder+count_bytes, sizeof(struct t2fs_record));
                // Testa se encontrou o pedaço do caminho
                if (strcmp(token,file->name) == 0){
                    if (file->TypeVal == TYPEVAL_DIRETORIO || file->TypeVal == TYPEVAL_REGULAR){
                        found = 1;
                    }
                }
                count_bytes += sizeof(struct t2fs_record);
            }

            // Lê subdiretório se for uma pasta e ainda não encontrou a pasta/arquivo que procurava
            if (file->TypeVal == TYPEVAL_DIRETORIO && found){
                for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
                    read_sector(convertBlockToSector(diskBlockSize, file->dataPtr[0])+i, find_folder+(i*SIZE_SECTOR_BYTES));
                }
                memcpy(file, find_folder, sizeof(struct t2fs_record));
            }
        }

        free(tofree);
        // Retorna arquivo se encontrou o caminho completo
        if (found){
            free(find_folder);
            return file;
        }
    }

    free(find_folder);
    free(file);
    return NULL;
}

struct t2fs_record *findFolder(char *name){
    char *token, *string, *tofree;
    struct t2fs_record *file = malloc(sizeof(struct t2fs_record));
    char *find_folder = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES));
    int found, count_bytes;
    int i;

    tofree = string = strdup(name);

    if (string != NULL){
        // Testa leitura do primeiro arquivo do diretório raiz "/"
        if (string[0] == '/'){
            for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
                read_sector(convertBlockToSector(diskBlockSize, diskRootDirReg.dataPtr[0])+i, find_folder+(i*SIZE_SECTOR_BYTES));
            }
            memcpy(file, find_folder, sizeof(struct t2fs_record));
            string++;
        }
        else {
            return NULL;
        }

        // Retorna se estiver procurando pelo raiz
        if (strlen(name) == 1)
            return &diskRootDirReg;

        // Testa a leitura dos subdiretórios ou arquivos
        int firstTime = 1;
        while((token = strsep(&string, "/")) != NULL){
            if(!firstTime){
                // Lê subdiretório se for uma pasta e ainda não encontrou a pasta/arquivo que procurava
                if (file->TypeVal == TYPEVAL_DIRETORIO && found){
                    for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
                        read_sector(convertBlockToSector(diskBlockSize, file->dataPtr[0])+i, find_folder+(i*SIZE_SECTOR_BYTES));
                    }
                    memcpy(file, find_folder, sizeof(struct t2fs_record));
                }
            }
            else {
                firstTime = 0;
            }

            // Procura pelo nome no primeiro setor
            found = 0;
            count_bytes = 0;
            while ((count_bytes < SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES)) && !found){
                memcpy(file, find_folder+count_bytes, sizeof(struct t2fs_record));
                // Testa se encontrou o pedaço do caminho
                if (strcmp(token,file->name) == 0){
                    if (file->TypeVal == TYPEVAL_DIRETORIO || file->TypeVal == TYPEVAL_REGULAR){
                        found = 1;
                    }
                }
                count_bytes += sizeof(struct t2fs_record);
            }
        }

        free(tofree);
        // Retorna arquivo se encontrou o caminho completo
        if (found){
            return file;
        }
    }

    free(find_folder);
    free(file);
    return NULL;
}

// Função auxiliar para invalidação dos blocos alocados para um arquivo
int invalidateFile(char *name){
    char *token, *string, *tofree;
    struct t2fs_record *file = malloc(sizeof(struct t2fs_record));
    char *find_folder = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES));
    int found, count_bytes;
    int currentSector;
    int firstSectorBlock;
    int j;

    tofree = string = strdup(name);
    if (string != NULL){
        // Testa leitura do primeiro arquivo do diretório raiz "/"
        if (strcmp(&string[0],"/")){
            currentSector = firstSectorBlock = convertBlockToSector(diskBlockSize, diskRootDirReg.dataPtr[0]);
            for (j = 0; j < diskBlockSize/SIZE_SECTOR_BYTES; j++) {
                read_sector(currentSector+j, find_folder+(j*SIZE_SECTOR_BYTES));
            }
            memcpy(file, find_folder, sizeof(struct t2fs_record));
            string++;
        }
        else {
            return -1;
        }

        // Testa a leitura dos subdiretórios ou arquivos
        while((token = strsep(&string, "/")) != NULL){
            // Procura pelo nome no primeiro setor
            found = 0;
            count_bytes = 0;
            while ((count_bytes < SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES)) && !found){
                memcpy(file, find_folder+count_bytes, sizeof(struct t2fs_record));
                // Testa se encontrou o pedaço do caminho
                if (strcmp(token,file->name) == 0){
                    if (file->TypeVal == TYPEVAL_DIRETORIO || file->TypeVal == TYPEVAL_REGULAR){
                        found = 1;
                    }
                }
                else {
                    count_bytes += sizeof(struct t2fs_record);
                }
            }

            // Lê subdiretório se for uma pasta
            if (file->TypeVal == TYPEVAL_DIRETORIO){
                currentSector = convertBlockToSector(diskBlockSize, file->dataPtr[0]);

                for (j = 0; j < diskBlockSize/SIZE_SECTOR_BYTES; j++) {
                    read_sector(currentSector+j, find_folder+(j*SIZE_SECTOR_BYTES));
                }
                memcpy(file, find_folder, sizeof(struct t2fs_record));
            }
        }

        // Invalida arquivo se encontrou o caminho completo
        if (found){
            file->TypeVal = TYPEVAL_INVALIDO;
            memcpy(find_folder+count_bytes, file, sizeof(struct t2fs_record));
            for (j = 0; j < diskBlockSize/SIZE_SECTOR_BYTES; j++) {
                write_sector(currentSector+j, find_folder+(j*SIZE_SECTOR_BYTES));
            }
        }

        free(tofree);
    }

    free(find_folder);
    free(file);
    return 0;
}

// Função que deleta um arquivo do sistema de arquivos e libera seus blocos
int t2fs_delete(char *name){
    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    // Busca o arquivo a ser deletado
    struct t2fs_record *del_rec = findFile(name);

    // Testa se encontrou o arquivo
    if (del_rec != NULL){
        // Fecha o arquivo se ele se encontra entre os abertos
        /*int closeFile = findFile(handle);*/
        /*t2fs_close(handle);*/

        // Descobre o número de blocos a serem deletados
        int numBlocks = del_rec->blocksFileSize;

        // Deleta blocos apontados por ponteiros diretos
        int i;
        for (i = 0; i < 2; i++) {
            if (numBlocks > 0){
                markBlockBitmap(del_rec->dataPtr[i], UNSET_BIT);
                numBlocks--;
            }
        }
        // Deleta blocos apontados por ponteiros de indireção simples
        // Deleta blocos apontados por ponteiros de indireção dupla

        // Invalida registro no disco físico
        int return_invalidate = invalidateFile(name);
        if (return_invalidate == -1){
            return -1;
        }

        return 0;
    }
    else {
        return -1;
    }
}

char *validateFileName(char *name){
    char *token, *string;
    char *token_return;

    string = strdup(name);
    while((token = strsep(&string, "/")) != NULL){
        token_return = token;
    }

    if(strlen(token_return)>MAX_FILE_NAME){
        token_return[MAX_FILE_NAME-1] = '\0';
    }

    return token_return;
}

char *validateFolderName(char *name){
    char *fileName;
    char *folderName = malloc(sizeof(char)*MAX_FILE_NAME);

    fileName = validateFileName(name);

    // Modifica o retorno para raiz e sub-diretórios
    if (strlen(fileName)==strlen(name)-1){
        memset(folderName, '\0', sizeof(char)*MAX_FILE_NAME);
        memcpy(folderName, "/", sizeof(char));
    }
    else {
        memset(folderName, '\0', sizeof(char)*MAX_FILE_NAME);
        memcpy(folderName, name, strlen(name)-strlen(fileName)-1); // -1 retira a última barra do caminho
    }

    return folderName;
}

// Insere um novo record em uma pasta existente
int diskInsertRecord(struct t2fs_record *new_record, char *folder_name){
    struct t2fs_record *find_folder;
    struct t2fs_record *find_record = malloc(sizeof(struct t2fs_record));
    struct t2fs_record *free_record = malloc(sizeof(struct t2fs_record));
    char *find_folder_sector = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES));
    int find_record_displacement = 0;
    int find_record_sector = 0;
    int firstSectorBlock;
    int j;

    // Procura pasta no disco
    find_folder = findFolder(folder_name);

    // Retorna se não encontrou a pasta
    if (find_folder == NULL){
        return -1;
    }

    // Insere novo arquivo na pasta encontrada
    if (find_folder->TypeVal == TYPEVAL_DIRETORIO){
        // Ponteiro direto 1
        int currentSector = firstSectorBlock = convertBlockToSector(diskBlockSize, find_folder->dataPtr[0]);
        int currentBlock = find_folder->dataPtr[0];
        // Ponteiro direto 2
        // Indireção simples
        // Indireção dupla

        // Lê o bloco da pasta para escrita do novo record
        for (j = 0; j < diskBlockSize/SIZE_SECTOR_BYTES; j++) {
            read_sector(currentSector+j, find_folder_sector+(j*SIZE_SECTOR_BYTES));
        }
        memcpy(free_record, find_folder_sector, sizeof(struct t2fs_record));

        // Procura a primeira entrada inválida para escrita do novo record
        int i = 0;
        int found = 0;
        while(i < SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES)){
            // Guarda marcador do primeiro espaço livre
            if (free_record->TypeVal != TYPEVAL_DIRETORIO && free_record->TypeVal != TYPEVAL_REGULAR && !found){
                memcpy(find_record, free_record, sizeof(struct t2fs_record));
                find_record_displacement = i;
                find_record_sector = currentSector;
                found = 1;
            }

            // Se o arquivo já existe sobreescreve
            if (strcmp(free_record->name, new_record->name) == 0){
                memcpy(find_record, free_record, sizeof(struct t2fs_record));
                find_record_displacement = i;
                find_record_sector = currentSector;

                // Limpa os blocos alocados anteriormente
                /*diskCleanBlocks(free_record);*/

                found = 1;
            }

            // Lê a próxima entrada da pasta
            if (i != 0 && ((i + sizeof(struct t2fs_record)) % SIZE_SECTOR_BYTES == 0)){
                currentSector++; // Atualiza setor corrente a cada um lido
            }
            i += sizeof(struct t2fs_record);
            memcpy(free_record, find_folder_sector+i, sizeof(struct t2fs_record));
        }

        // Escreve o record e atualiza o bitmap ao encontrar uma entrada livre para escrita
        if (found){
            // Escreve o novo record
            memcpy(find_folder_sector+find_record_displacement, new_record, sizeof(struct t2fs_record));
            int block_bytes_displacement = (find_record_sector-firstSectorBlock)*SIZE_SECTOR_BYTES;
            write_sector(find_record_sector, find_folder_sector+block_bytes_displacement); // Atualiza apenas o setor modificado do bloco

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

t2fs_file t2fs_open(char *nome){
    int handle;
    struct t2fs_record *record_aux;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    struct file *file_aux = malloc(sizeof(struct file));
    // Procura t2fs_record do arquivo no disco
    record_aux = findFile(nome);

    if (record_aux != NULL){
        // Insere record na estrutura file a ser passada como parâmetro
        memcpy(&file_aux->record, record_aux, sizeof(struct t2fs_record));

        if (record_aux != NULL){
            // Define ou encontra o handle já utilizado para o arquivo
            handle = fileGetHandle(file_aux);

            // Retorna o handle ou marcador de erro
            if (handle >= 0){
                return handle;
            }
        }
    }

    return -1;
}

// Percorre o disco físico por força bruta, até encontrar um record com o nome correspondente, e o modifica
int diskModifyRecordByName(struct t2fs_record *modify_record){
    int i,j,k;
    int found;

    // Varredura dos arquivos/pastas de um diretório
    struct t2fs_record *read_rec = malloc(sizeof(struct t2fs_record));
    char *find_folder = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES)); // Lê um setor do disco 'físico'

    // Percorre todos os blocos do disco
    for (i = 1; i < diskBlocksNumber; i++) {
        // Percorre cada setor de um bloco
        for (j = 0; j < diskBlockSize/SIZE_SECTOR_BYTES; j++) {
            read_sector(convertBlockToSector(diskBlockSize, i)+j, find_folder+(j*SIZE_SECTOR_BYTES));
        }

        // Percorre cada record do bloco até encontrar
        k=0;
        found = 0;
        while(k < SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES) && !found){
            memcpy(read_rec, find_folder+k, sizeof(struct t2fs_record));

            // Encontrou record
            if (strcmp(modify_record->name, read_rec->name)==0){
                // Atualiza record no buffer
                memcpy(find_folder+k, modify_record, sizeof(struct t2fs_record));

                // Escreve buffer no disco e retorna
                for (j = 0; j < diskBlockSize/SIZE_SECTOR_BYTES; j++) {
                    write_sector(convertBlockToSector(diskBlockSize, i)+j, find_folder+(j*SIZE_SECTOR_BYTES));
                }

                found = 1;
            }

            k += sizeof(struct t2fs_record);
        }

        if (found)
            break;
    }

    free(read_rec);
    free(find_folder);

    return 1;
}

// Deleta um file da lista de arquivos abertos
int t2fs_close(t2fs_file handle){
    struct file *file_aux;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    file_aux = openFiles;
    while (file_aux != NULL){
        // Deleta arquivo ao encontrar
        if (file_aux->handle == handle){
            // Desmarca do Mapa
            openFilesMap[handle] = 0;

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

            // Reflete alterações no temporário file no disco
            diskModifyRecordByName(&file_aux->record);

            // Libera o ponteiro e retorna sucesso
            free(file_aux);
            return 0;
        }

        // Percorre até encontrar o último da lista
        file_aux = file_aux->next;
    }

    return -1;
}

// Função que cria um novo arquivo do sistema de arquivos no disco físico
t2fs_file t2fs_create(char *name){
    char* validatedName;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    // Valida o nome do arquivo
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
        int insert_record = diskInsertRecord(&newFile->record, validateFolderName(name));

        // Garante que inseriu record com sucesso
        if (insert_record < 0){
            t2fs_close(newFile->handle);
            return -1;
        }
    }
    else {
        return -1;
    }

    return newFile->handle;
}

// Função que cria uma nova pasta no sistema de arquivos no disco físico
t2fs_file t2fs_create_folder(char *name){
    char* validatedName;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    // Valida o nome do arquivo
    validatedName = validateFileName(name);

    // Monta arquivo a ser salvo no disco
    struct file* newFile = malloc(sizeof(struct file));
    memcpy(newFile->record.name, validatedName, sizeof(char)*MAX_FILE_NAME);
    newFile->record.TypeVal = 0x02; // 0xFF (registro inválido) OU  0x01 (arquivo regular) OU 0x02 (arquivo de diretório)
    newFile->record.blocksFileSize = 0;
    newFile->record.bytesFileSize = 0;
    newFile->record.dataPtr[0] = 0xFFFFFFFF;
    newFile->record.dataPtr[1] = 0xFFFFFFFF;
    newFile->record.singleIndPtr = 0xFFFFFFFF;
    newFile->record.doubleIndPtr = 0xFFFFFFFF;

    // Salva pasta no disco e atualiza apontadores
    int insert_folder = diskInsertRecord(&newFile->record, validateFolderName(name));

    // Garante que inseriu record com sucesso
    if (insert_folder < 0){
        return -1;
    }

    return 1;
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
    /*int curPosBlocks;*/
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
        curPosBytes = fileRead->currentBytesPos; // Posição a ler com relação ao disco
        bytesRead = 0; // Posição a ler com relação ao buffer

        //Lê o tamanho em caracteres passado por parâmetro
        readNewBlock = 1;
        while (size > 0){
            // Controla troca de bloco ao terminar de ler um bloco completo
            if (curPosBytes > diskBlockSize-1)
                readNewBlock = 1;

            // Incrementa ponteiro para bloco na primeira lida e nas trocas de bloco
            if (readNewBlock){
                /*curPosBlocks = curPosBytes % diskBlockSize; // Deslocamento em blocos com relação ao primeiro bloco*/

                // Ponteiro direto 1
                if (curPosBytes < diskBlockSize){
                    blockAddress = fileRead->record.dataPtr[0];
                }

                // Ponteiro direto 2
                else if (curPosBytes < 2*diskBlockSize){
                    blockAddress = fileRead->record.dataPtr[1];
                }
                // Indireção simples
                // Indireção dupla

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
            buffer[bytesRead] = block_read[curPosBytes];

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

int t2fs_write(t2fs_file handle, char *buffer, int size){
    int curPosBytes;
    /*int curPosBlocks;*/
    int writeNewBlock;
    int blockAddress;
    int i;

    // Inicializa o disco
    if(!diskInitialized){
        t2fs_first(&superblock);
    }

    // Aloca bloco para leitura dos setores do disco
    char *block_write = malloc(SIZE_SECTOR_BYTES*(diskBlockSize/SIZE_SECTOR_BYTES)); // Quatro setores

    // Inicializa arquivo e variáveis
    struct file *fileWrite = findFileHandle(handle);
    if(fileWrite != NULL){
        int bytesWritten = 0; // Posição a escrever com relação ao buffer
        curPosBytes = fileWrite->currentBytesPos; // Posição a escrever com relação ao bloco do disco físico

        // Escreve o tamanho em caracteres passado por parâmetro
        writeNewBlock = 1;
        /*int firstTime = 1;*/
        while (size > 0){
            // Controla troca de bloco ao terminar de ler um bloco completo
            if (curPosBytes > diskBlockSize)
                writeNewBlock = 1;

            // Incrementa ponteiro para bloco na primeira escrita e nas trocas de bloco
            if (writeNewBlock){
                /*curPosBlocks = curPosBytes % diskBlockSize; // Deslocamento em blocos com relação ao primeiro bloco*/

                // Controlar alocação de novos blocos em disco

                // Ponteiro direto 1
                if (curPosBytes < diskBlockSize){
                    // Aloca novo bloco se ainda não existir para a indireção
                    if (fileWrite->record.dataPtr[0] == 0xFFFFFFFF){
                        fileWrite->record.dataPtr[0] = diskBitmapReserveBlock();
                        fileWrite->record.blocksFileSize++;
                    }

                    // Atualiza para escrever no novo bloco
                    blockAddress = fileWrite->record.dataPtr[0];
                }
                // Ponteiro direto 2
                else if (curPosBytes < 2*diskBlockSize){
                    // Aloca novo bloco se ainda não existir para a indireção
                    if (fileWrite->record.dataPtr[1] == 0xFFFFFFFF){
                        fileWrite->record.dataPtr[1] = diskBitmapReserveBlock();
                        fileWrite->record.blocksFileSize++;
                    }

                    // Atualiza para escrever no novo bloco
                    blockAddress = fileWrite->record.dataPtr[1];
                }
                // Indireção simples
                // Indireção dupla

                // Escreve o conteúdo nos setores do bloco para um buffer
                for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
                    read_sector(convertBlockToSector(diskBlockSize, blockAddress)+i, block_write+(i*SIZE_SECTOR_BYTES));
                }

                // Incrementa bloco na estrutura do file aberto
                fileWrite->currentBlockPos++;

                // Marca para não ler novo bloco até chegar ao fim do novo bloco lido
                writeNewBlock = 0;
            }

            // Escreve cada caractere
            block_write[curPosBytes] = buffer[bytesWritten];

            // Incrementa os contadores a cada caractere lido
            curPosBytes++;
            bytesWritten++;
            size--;
        }

        // Escreve último bloco modificado no disco
        for (i = 0; i < diskBlockSize/SIZE_SECTOR_BYTES; i++) {
            write_sector(convertBlockToSector(diskBlockSize, blockAddress)+i, block_write+(i*SIZE_SECTOR_BYTES));
        }

        // Atualiza a posição em que parou de ler o file aberto
        fileWrite->currentBytesPos = curPosBytes;
        free(block_write);
        return bytesWritten;
    }

    free(block_write);
    return -1;
}
