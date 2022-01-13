/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione delle API che ricevo dal client
 * @author              Simone Tassotti
 * @date                03/01/2022
 */


#ifndef FILE_STORAGE_SERVER_LRU_SERVER_API_H


    #define FILE_STORAGE_SERVER_LRU_SERVER_API_H

    #define O_CREATE 127
    #define O_LOCK 128

    #define _POSIX_C_SOURCE 2001112L
    #include <stdlib.h>
    #include <stdio.h>
    #include <utils.h>
    #include <math.h>
    #include <FileStorageServer.h>

    typedef struct {
        int fd;
        int pfd;
        LRU_Memory *cache;
        Pre_Inserimento* pI;
        serverLogFile *log;
    } Task_Package;


    /**
     * @brief                       Accoglie le richieste del client
     * @fun                         ServerTaskds
     * @return                      (NULL) in caso di successo; altrimenti riporto un messaggio di errore
     */
    void* ServerTasks(unsigned int, void *);


#endif //FILE_STORAGE_SERVER_LRU_SERVER_API_H
