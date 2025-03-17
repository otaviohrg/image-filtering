SRC_DIR=src
HEADER_DIR=include
OBJ_DIR=obj

#CC=gcc
CC = mpicc #MPI
CXX = nvcc #CUDA

CFLAGS=-O3 -I$(HEADER_DIR)
LDFLAGS=-lm

# Paths (Modify if necessary)
CUDA_PATH = /usr/local/cuda-12.2.0
CUDA_LIB = -L$(CUDA_PATH)/lib64
CUDA_INC = -I$(CUDA_PATH)/include

SRC= dgif_lib.c \
	egif_lib.c \
	gif_err.c \
	gif_font.c \
	gif_hash.c \
	gifalloc.c \
	main.c \
	openbsd-reallocarray.c \
	quantize.c \
	filters.cu

OBJ= $(OBJ_DIR)/dgif_lib.o \
	$(OBJ_DIR)/egif_lib.o \
	$(OBJ_DIR)/gif_err.o \
	$(OBJ_DIR)/gif_font.o \
	$(OBJ_DIR)/gif_hash.o \
	$(OBJ_DIR)/gifalloc.o \
	$(OBJ_DIR)/main.o \
	$(OBJ_DIR)/openbsd-reallocarray.o \
	$(OBJ_DIR)/quantize.o \

FILTERS_SRC= $(SRC_DIR)/filters.cu
FILTER_OBJ= $(OBJ_DIR)/filters.o

all: $(OBJ_DIR) sobelf

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -fopenmp $(CFLAGS) -c -o $@ $^

$(FILTER_OBJ): $(FILTERS_SRC)
	$(CXX) -Xcompiler -fopenmp $(CFLAGS) -c -o $@ $^

sobelf:$(OBJ) $(FILTER_OBJ)
	$(CC) -fopenmp $(CFLAGS) -g -o $@ $^ $(LDFLAGS) -lcudart -lstdc++ $(CUDA_LIB)

clean:
	rm -f sobelf $(OBJ) $(FILTER_OBJ)
