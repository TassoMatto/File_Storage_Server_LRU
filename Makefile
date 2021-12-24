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

./server	: ./includes/logFile/logFile.o ./includes/FileStorageServer/FileStorageServer.o ./includes/utils/utils.o ./includes/queue/queue.o ./server.o
	$(CC) -o $@ $^ $(LPTHREADS)

./%.o :	./%.c
	$(CC) $(CFLAGS) $(INCLUDES) $^ -c -o $@

all	:	$(SS)