# ----------------------------------------------
# Progetto - File Storage Server
# Simone Tassotti
# 24/01/2022
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
	$(CC) -o $@ $^ $(LPTHREADS) $(MATH_H) -O3

./client	:	./includes/API/Client_API.o	./client.o ./includes/utils/utils.o ./includes/queue/queue.o
	$(CC) -o $@ $^ $(LPTHREADS) $(MATH_H) -O3

./%.o :	./%.c
	$(CC) $(CFLAGS) $(INCLUDES) -O3 $^ -c -o $@

test1	:	$(SS) $(CL)
	@clear
	@echo "TEST N°1 SUL FILE_STORAGE_SERVER\n\n\n"
	@{ valgrind --leak-check=full $(SS) ./test1/config.txt & } && ./test1/startClient.sh $$!

test2	:	$(SS) $(CL)
	@clear
	@echo "TEST N°2 SUL FILE_STORAGE_SERVER\n\n\n"
	@{ $(SS) ./test2/config.txt & } && ./test2/startClient.sh $$!

test3	:	$(SS) $(CL)
	@clear
	@echo "TEST N°3 SUL FILE_STORAGE_SERVER\n\n\n"
	@{ $(SS) ./test3/config.txt & } && ./test3/startClient.sh $$!

all	:	$(SS)	$(CL)

clean	:
	rm -f $(SS) $(CL) $(SS).o $(CL).o FileStorageServer.log

cleanall	:
	rm -f *.o */*/*.o *.sk $(SS) $(CL)