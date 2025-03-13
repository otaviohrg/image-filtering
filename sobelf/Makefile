SRC_DIR=src
HEADER_DIR=include
OBJ_DIR=obj

CC=gcc
CXX=nvcc
CFLAGS=-O3 -I$(HEADER_DIR)
LDFLAGS=-lm

SRC=dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	openbsd-reallocarray.c \
	quantize.c

OBJ= $(OBJ_DIR)/dgif_lib.o \
	$(OBJ_DIR)/egif_lib.o \
	$(OBJ_DIR)/gif_err.o \
	$(OBJ_DIR)/gif_font.o \
	$(OBJ_DIR)/gif_hash.o \
	$(OBJ_DIR)/gifalloc.o \
	$(OBJ_DIR)/openbsd-reallocarray.o \
	$(OBJ_DIR)/quantize.o

# Check if main.cu exists and set the source file and object file
MAIN_SRC=$(SRC_DIR)/main.cu
MAIN_OBJ=$(OBJ_DIR)/main.o

all: $(OBJ_DIR) sobelf

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

# Compilation rule for .c files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $^

# Compilation rule for main.cu (CUDA source)
$(OBJ_DIR)/main.o: $(MAIN_SRC)
	$(CXX) -Xcompiler -fopenmp $(CFLAGS) -c -o $@ $^

sobelf: $(OBJ) $(MAIN_OBJ)
	$(CXX) -Xcompiler -fopenmp $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f sobelf $(OBJ) $(MAIN_OBJ)
