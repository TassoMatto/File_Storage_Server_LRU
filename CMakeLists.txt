cmake_minimum_required(VERSION 3.10)
project(File_Storage_Server_LRU C)

set(CMAKE_C_STANDARD 99)

set(LOG_FILE includes)

include_directories(${LOG_FILE})

add_executable(File_Storage_Server_LRU server.c includes/logFile/logFile.c includes/logFile.h includes/FileStorageServer/FileStorageServer.c includes/FileStorageServer.h includes/utils/utils.c includes/utils.h includes/icl_hash.h includes/hashTable/icl_hash.c includes/queue/queue.c includes/queue.h includes/threadPool/threadPool.c includes/threadPool.h includes/File/file.c includes/file.h includes/API/Server_API.c includes/Server_API.h includes/API/Client_API.c includes/Client_API.h)
