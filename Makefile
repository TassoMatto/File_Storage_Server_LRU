# ----------------------------------------------
# Progetto - File Storage Server
# ----------------------------------------------

CC	=  gcc

CFLAGS	+= -std=c99 -Wall -Werror -g

DBGFLAGS = -g

LPTHREADS = -lpthread

MATH_H = -lm

INCLUDES = -I ./includes/

SS = ./server

CL = ./client

.DEFAULT_GOAL = all

.PHONY		:	all clean cleanall dbg test1 test2 test3

./server	: 	./includes/logFile/logFile.o ./includes/FileStorageServer/FileStorageServer.o ./includes/utils/utils.o ./includes/queue/queue.o ./includes/threadPool/threadPool.o ./includes/File/file.o ./includes/hashTable/icl_hash.o ./includes/FileStorageServer/FileStorageServer.o ./includes/API/Server_API.o ./server.o
	$(CC) -o $@ $^ $(LPTHREADS) $(MATH_H)

./client	:	./includes/API/Client_API.o	./client.o ./includes/utils/utils.o
	$(CC) -o $@ $^ $(LPTHREADS) $(MATH_H)

./%.o :	./%.c
	$(CC) $(CFLAGS) $(INCLUDES) -O3 $^ -c -o $@

all	:	$(SS)	$(CL)