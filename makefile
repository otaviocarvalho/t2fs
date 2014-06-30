SRC_T2FS = src/t2fs.c
OBJ_T2FS = bin/t2fs.o
OUT_LIB = lib/libt2fs.a
#DEP_LIB_32 = lib/libapidisk32.a
DEP_LIB_64 = lib/libapidisk64.a
FLAGS = -I./include/ -L./include/ -lt2fs -Wall

#FLAGS_TESTES_32 = -L./lib -lt2fs -lapidisk32 -Wall
FLAGS_TESTES_64 = -L./lib -lt2fs -lapidisk64 -Wall
TESTE_IDENTIFY = teste/teste_identify

teste_identify_run: teste_identify_compile
		./$(TESTE_IDENTIFY)
teste_identify_compile: $(OUT_LIB)
		gcc $(TESTE_IDENTIFY).c -o $(TESTE_IDENTIFY) $(FLAGS_TESTES_64)

$(OUT_LIB): $(OBJ_T2FS)
		ar crs $(OUT_LIB) $(OBJ_T2FS) $(DEP_LIB_64)

$(OBJ_T2FS): $(SRC_T2FS)
		gcc -c $(SRC_T2FS) -o $(OBJ_T2FS) $(FLAGS)

clean:
		rm -rf $(OUT_LIB) $(OBJ_T2FS) $(TESTE_IDENTIFY)
