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
    #include <logFile.h>
    #include <pthread.h>
    #include <string.h>


    /**
     * @brief                           Struttura che gestisce il pool di thread
     * @struct                          threadPool
     * @param shutdown                  Variabile che indica al pool il momento di arrestarsi
     * @param hardST                    Se si vuole spegnere il server immediatamente
     * @param numeroThread              Numero dei thread fissi del pool di thread
     * @param numeroDiThreadAttivi      Numero di thread attivi in quel istante
     * @param numeroThreadAiutanti      Numero di thread in supporto al pool
     * @param thread                    ID dei thread del pool fissi
     * @param taskQueue                 Coda dei task da far eseguire ai thread
     * @param numeroTaskInCoda          Numero dei task in coda
     * @param taskQueueMutex            Variabile Mutex per l'accesso concorrente alla coda dei task
     * @param emptyCondVar              Variabile condizione per segnalare se la coda dei task e' vuota o no
     * @param isEmpty                   Variabile per gestire il controllo di riempimento della coda
     * @param log                       File di log in caso tracciamento
     */
    typedef struct {
        int shutdown;
        int hardST;

        unsigned int numeroThread;
        unsigned int numeroDiThreadAttivi;
        unsigned int numeroThreadAiutanti;

        pthread_t *threads;
        Queue *taskQueue;
        void (*free_task)(void *);
        int numeroTaskInCoda;
        pthread_mutex_t *taskQueueMutex;
        pthread_cond_t *emptyCondVar;
        int isEmpty;

        serverLogFile *log;
    } threadPool;


    /**
     * @brief                   Struttura della funzione che rappresenta il task
     * @struct                  Task_Fun
     */
    typedef void* (*Task_Fun)(unsigned int, void* );


    /**
     * @brief                   Struttura che rappresenta il Task che aggiungo alla coda
     * @struct                  Task
     */
    typedef struct {
        Task_Fun to_do;
        void *argv;
    } Task;


    /**
     * @brief                   Crea un pool di thread
     * @fun                     startThreadPool
     * @return                  Ritorna la struttura che rappresenta il pool; in caso di errore ritorna NULL [setta errno]
     */
    threadPool* startThreadPool(unsigned int, void (*free_task)(void *), serverLogFile *);


    /**
     * @brief                   Funzione che manda un task da eseguire al pool
     * @fun                     pushTask
     * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int pushTask(threadPool *, Task *);


    /**
     * @brief               Avvia la fase di arresto del pool di thread
     * @fun                 stopThreadPool
     * @param pool          Pool di thread da fermare
     * @param hardShutdown  Seleziona il tipo di spegnimento del server
     * @return              Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int stopThreadPool(threadPool *, int);


#endif //FILE_STORAGE_SERVER_LRU_THREADPOOL_H
