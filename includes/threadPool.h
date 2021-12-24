/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di un pool di thread
 * @author              Simone Tassotti
 * @date                24/12/2021
 */

#ifndef FILE_STORAGE_SERVER_LRU_THREADPOOL_H

    #define FILE_STORAGE_SERVER_LRU_THREADPOOL_H

    #include <stdlib.h>
    #include <stdio.h>
    #include <queue.h>
    #include <pthread.h>
    #include <string.h>


    typedef struct {
        unsigned int numeroThread;
        unsigned int numeroDiThreadAttivi;

        pthread_t *threads;
        Queue *taskQueue;
        pthread_mutex_t *taskQueueMutex;
        pthread_cond_t *emptyCondVar;
        int isEmpty;

    } threadPool;


    /**
     * @brief                   Crea un pool di thread
     * @fun                     startThreadPool
     * @param numeroThread      Numero di thread da creare nel pool
     * @return                  Ritorna la struttura che rappresenta il pool; in caso di errore ritorna NULL [setta errno]
     */
    threadPool* startThreadPool(unsigned int);


#endif //FILE_STORAGE_SERVER_LRU_THREADPOOL_H
