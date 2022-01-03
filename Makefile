# ----------------------------------------------
# Progetto - File Storage Server
# ----------------------------------------------

CC	=  gcc

CFLAGS	+= -Wall -Werror -g -DDEBUG

DBGFLAGS = -g

LPTHREADS = -lpthread

INCLUDES = -I ./includes/

SS = ./server

CL = ./client

.DEFAULT_GOAL = all

.PHONY		:	all clean cleanall dbg test1 test2 test3

./server	: ./includes/logFile/logFile.o ./includes/FileStorageServer/FileStorageServer.o ./includes/utils/utils.o ./includes/queue/queue.o ./includes/threadPool/threadPool.o ./includes/File/file.o ./includes/hashTable/icl_hash.o ./includes/FileStorageServer/FileStorageServer.o ./server.o
	$(CC) -o $@ $^ $(LPTHREADS)

./%.o :	./%.c
	$(CC) $(CFLAGS) $(INCLUDES) -O2 $^ -c -o $@

all	:	$(SS)