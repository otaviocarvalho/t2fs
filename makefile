SRC_T2FS = src/t2fs.c
OBJ_T2FS = bin/t2fs.o
OUT_LIB = lib/libt2fs.a
DEP_LIB_32 = lib/libapidisk32.a
DEP_LIB_64 = lib/libapidisk64.a
FLAGS = -I./include/ -L./include/ -lt2fs -Wall

FLAGS_TESTES_32 = -L./lib -lt2fs -lapidisk32 -Wall
FLAGS_TESTES_64 = -L./lib -lt2fs -lapidisk64 -Wall
TESTE_IDENTIFY = teste/teste_identify
TESTE_DELETE = teste/teste_delete
TESTE_CREATE = teste/teste_create

APP_COPY_SRC = src/copy2t2.c
APP_COPY_BIN = bin/copy2t2
APP_DIR_SRC = src/dirt2.c
APP_DIR_BIN = bin/dirt2
APP_MKDIR_SRC = src/mkdirt2.c
APP_MKDIR_BIN = bin/mkdirt2
APP_RMDIR_SRC = src/rmdirt2.c
APP_RMDIR_BIN = bin/rmdirt2

app_rmdir_compile: app_mkdir_compile
		gcc $(APP_RMDIR_SRC) -o $(APP_RMDIR_BIN) $(FLAGS_TESTES_64)
app_mkdir_compile: app_dir_compile
		gcc $(APP_MKDIR_SRC) -o $(APP_MKDIR_BIN) $(FLAGS_TESTES_64)
app_dir_compile: app_copy_compile
		gcc $(APP_DIR_SRC) -o $(APP_DIR_BIN) $(FLAGS_TESTES_64)
app_copy_compile: teste_create_run
		gcc $(APP_COPY_SRC) -o $(APP_COPY_BIN) $(FLAGS_TESTES_64)

teste_create_run: teste_create_compile
		./$(TESTE_CREATE)
teste_create_compile: teste_delete_run
		gcc $(TESTE_CREATE).c -o $(TESTE_CREATE) $(FLAGS_TESTES_64)

teste_delete_run: teste_delete_compile
		./$(TESTE_DELETE)
teste_delete_compile: teste_identify_run
		gcc $(TESTE_DELETE).c -o $(TESTE_DELETE) $(FLAGS_TESTES_64)

teste_identify_run: teste_identify_compile
		./$(TESTE_IDENTIFY)
teste_identify_compile: $(OUT_LIB)
		gcc $(TESTE_IDENTIFY).c -o $(TESTE_IDENTIFY) $(FLAGS_TESTES_64)

$(OUT_LIB): $(OBJ_T2FS)
		ar crs $(OUT_LIB) $(OBJ_T2FS) $(DEP_LIB_64)

$(OBJ_T2FS): $(SRC_T2FS)
		gcc -c $(SRC_T2FS) -o $(OBJ_T2FS) $(FLAGS)

clean:
		rm -rf $(OUT_LIB) $(OBJ_T2FS) $(TESTE_IDENTIFY) $(TESTE_DELETE) $(TESTE_CREATE)
