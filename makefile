SRC_T2FS = src/t2fs.c
OBJ_T2FS = bin/t2fs.o
OUT_LIB = lib/libt2fs.a
#DEP_LIB = lib/libapidisk32.a
DEP_LIB = lib/libapidisk64.a
FLAGS = -I./include/ -L./include/ -lt2fs -Wall

$(OUT_LIB): $(OBJ_T2FS)
		ar crs $(OUT_LIB) $(OBJ_T2FS) $(DEP_LIB)

$(OBJ_T2FS): $(SRC_T2FS)
		gcc -c $(SRC_T2FS) -o $(OBJ_T2FS) $(FLAGS)

clean:
		rm -rf $(OUT_LIB) $(OBJ_T2FS)
