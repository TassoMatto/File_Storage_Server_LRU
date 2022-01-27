/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione richieste del client
 * @author              Simone Tassotti
 * @date                03/01/2022
 * @finish              26/01/2022
 */


#ifndef FILE_STORAGE_SERVER_LRU_SERVER_API_H


    #define FILE_STORAGE_SERVER_LRU_SERVER_API_H


    #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 2001112L
    #endif


    #define O_CREATE 127
    #define O_LOCK 128


    #include <stdlib.h>
    #include <stdio.h>
    #include <utils.h>
    #include <math.h>
    #include <FileStorageServer.h>


    /**
     * @brief       Argomenti per ogni thread del pool
     * @struct      Task_Package
     * @param fd    FD del client con cui comunica
     * @param pfd   Pipe per scrivere FD da riabilitare
     * @param cache Memoria cache da gestire per le richieste
     * @param log   File di log per il tracciamento delle operazioni
     */
    typedef struct {
        int fd;
        int pfd;
        LRU_Memory *cache;
        serverLogFile *log;
    } Task_Package;


    /**
     * @brief                       Accoglie le richieste del client
     * @fun                         ServerTaskds
     * @return                      (NULL) in caso di successo; altrimenti riporto un messaggio di errore
     */
    void* ServerTasks(unsigned int, void *);


#endif //FILE_STORAGE_SERVER_LRU_SERVER_API_H
