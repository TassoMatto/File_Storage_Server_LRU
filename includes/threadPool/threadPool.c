/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di un pool di thread
 * @author              Simone Tassotti
 * @date                24/12/2021
 */


#include "threadPool.h"


/**
 * @brief               Argomenti base dei thread della pool
 * @struct              Threads_Arg
 * @param idThread      Id del thread
 * @param log           File di log
 * @param pool          Pool che gestisce il thread
 */
typedef struct {
    unsigned int idThread;
    serverLogFile *log;
    threadPool *pool;
} Threads_Arg;


/**
 * @brief               Dealloca l'intero pool di thread
 * @macro               FREE_POOL_THREAD
 */
#define FREE_POOL_THREAD()                                                          \
    do {                                                                            \
        error = errno;                                                              \
        if((pool->taskQueueMutex) != NULL) {                                        \
            pthread_mutex_destroy(pool->taskQueueMutex);                            \
            free(pool->taskQueueMutex);                                             \
        }                                                                           \
        if(pool->emptyCondVar != NULL) {                                            \
            pthread_cond_destroy(pool->emptyCondVar);                               \
            free(pool->emptyCondVar);                                               \
        }                                                                           \
        if(pool->threads != NULL) { free(pool->threads); }                          \
        if(pool->taskQueue != NULL) { destroyQueue(&(pool->taskQueue)); }           \
        if(pool != NULL) { free(pool); }                                            \
        errno = error;                                                              \
    } while(0);


/**
 * @brief               Lock del pool di thread
 * @macro               LOCK_POOL
 */
#define LOCK_POOL(RET_VALUE)                                        \
    if((error = pthread_mutex_lock(pool->taskQueueMutex)) != 0) {   \
        FREE_POOL_THREAD()                                          \
        return RET_VALUE;                                           \
    }


/**
 * @brief               Unlock del pool di thread
 * @macro               UNLOCK_POOL
 */
#define UNLOCK_POOL(RET_VALUE)                                      \
    if((error = pthread_mutex_unlock(pool->taskQueueMutex)) != 0) { \
        FREE_POOL_THREAD()                                          \
        return RET_VALUE;                                           \
    }


/**
 * @brief               Funzione per l'arresto dei thread worker del pool
 * @fun                 stopTasking
 * @param a, b          Argomenti non utilizzati nella funzione
 * @return              NULL sempre
 */
static void* stopTasking(unsigned int a, void *b) {
    printf("Thread stop\n\n");
    return NULL;
}


/**
 * @brief               Routine base che deve eseguire il thread worker del pool
 * @fun                 start_routine
 * @param argv          Argomenti passati al thread worker
 * @return              Ritorna NULL in caso di successo; altrimenti ritorna un valore contenente l'errore che ha
 *                      generato l'arresto
 */
static void* start_routine(void *argv) {
    /** Variabili **/
    int error = 0, *status = NULL;
    Threads_Arg *castedArg = NULL;
    threadPool *pool = NULL;
    serverLogFile *log = NULL;
    unsigned int numeroDelThread = -1;
    void *uncastedTask = NULL;
    Task *t = NULL;
    Task_Fun work = NULL;

    /** Cast degli argomenti **/
    castedArg = (Threads_Arg *) argv;
    pool = castedArg->pool;
    numeroDelThread = castedArg->idThread;
    log = castedArg->log;
    free(argv);

    /** Lavoro iterativo del thread **/
    if((status = (int *) malloc(sizeof(int))) == NULL) {
        return NULL;
    }
    if(traceOnLog(log, "Thread n°%d avviato\n", numeroDelThread) == -1) {
        *status = errno;
        return status;
    }
    LOCK_POOL(NULL)
    do {

        /** Se la lista e' vuota mi metto in attesa */
        while(pool->isEmpty) {
            (pool->numeroDiThreadAttivi)--;
            if(traceOnLog(log, "Thread n°%d in attesa di un task da eseguire\n", numeroDelThread) == -1) {
                *status = errno;
                return status;
            }
            if((error = pthread_cond_wait((pool->emptyCondVar), (pool->taskQueueMutex))) != 0) {
                *status = errno;
                return status;
            }
            if(traceOnLog(log, "Thread n°%d: tentativo di prendere un task\n", numeroDelThread) == -1) {
                *status = errno;
                return status;
            }
            if(traceOnLog(log, "Thread n°%d risvegliato\n", numeroDelThread) == -1) {
                *status = errno;
                return status;
            }
            (pool->numeroDiThreadAttivi)++;
        }


        /** Estraggo il task dalla coda e lo eseguo **/
        if((uncastedTask = deleteFirstElement(&(pool->taskQueue))) == NULL) {
            UNLOCK_POOL(NULL)
            break;
        }
        (pool->numeroTaskInCoda)--;
        UNLOCK_POOL(NULL)
        if(traceOnLog(log, "Thread n°%d: esecuzione nuovo Task\n", numeroDelThread) == -1) {
            *status = errno;
            return status;
        }
        t = (Task *) uncastedTask;
        work = t->to_do;
        if(traceOnLog(log, "Thread n°%d: avvio task in corso\n", numeroDelThread) == -1) {
            *status = errno;
            return status;
        }
        if(work(numeroDelThread, t->argv) != NULL) {
            *status = errno;
            return status;
        }
        free(uncastedTask);
        LOCK_POOL(NULL)
    } while(!(pool->shutdown));
    UNLOCK_POOL(NULL)


    /** Arresto del thread **/

    if(traceOnLog(log, "Thread n°%d: arresto in corso\n", numeroDelThread) == -1) {
        *status = errno;
        return status;
    }
    free(status);
    return NULL;
}


/**
 * @brief                   Funzione che esegue un thread in aiuto del pool
 * @fun                     threadHelper
 * @param argv              Argomenti del thread worker
 * @return                  (NULL) in caso di successo; altrimenti ritorna il numero dell'errore
 */
/*static void* threadHelper(void *argv) {
    *//** Variabili **//*
    int error = 0, *status = NULL;
    Threads_Arg *castedArg = NULL;
    threadPool *pool = NULL;
    serverLogFile *log = NULL;
    unsigned int numeroDelThread = -1;
    void *uncastedTask = NULL;
    Task *t = NULL;
    Task_Fun work = NULL;

    *//** Cast degli argomenti **//*
    castedArg = (Threads_Arg *) argv;
    pool = castedArg->pool;
    numeroDelThread = castedArg->idThread;
    log = castedArg->log;
    free(argv);

    *//** Lavoro iterativo del thread **//*
    if((status = (int *) malloc(sizeof(int))) == NULL) {
        return NULL;
    }
    if(traceOnLog(log, "Thread n°%d avviato\n", numeroDelThread) == -1) {
        *status = errno;
        return status;
    }
    LOCK_POOL(NULL)
    *//** Estraggo il task dalla coda e lo eseguo **//*
    if((uncastedTask = deleteFirstElement(&(pool->taskQueue))) == NULL) {
        UNLOCK_POOL(NULL)
        return NULL;
    }
    (pool->numeroTaskInCoda)--;
    UNLOCK_POOL(NULL)
    if(traceOnLog(log, "Thread n°%d: esecuzione nuovo Task\n", numeroDelThread) == -1) {
        *status = errno;
        return status;
    }
    t = (Task *) uncastedTask;
    work = t->to_do;
    if(traceOnLog(log, "Thread n°%d: avvio task in corso\n", numeroDelThread) == -1) {
        *status = errno;
        return status;
    }
    if(work(numeroDelThread, t->argv) != NULL) {
        *status = errno;
        return status;
    }
    free(uncastedTask);


    *//** Arresto del thread **//*
    if(traceOnLog(log, "Thread n°%d: arresto in corso\n", numeroDelThread) == -1) {
        *status = errno;
        return status;
    }
    free(status);
    return NULL;
}*/


/**
 * @brief                   Crea un pool di thread
 * @fun                     startThreadPool
 * @param numeroThread      Numero di thread da creare nel pool
 * @param log               File di log
 * @return                  Ritorna la struttura che rappresenta il pool; in caso di errore ritorna NULL [setta errno]
 */
threadPool* startThreadPool(unsigned int numeroThread, serverLogFile *log) {
    /** Variabili **/
    int error;
    Threads_Arg **arg = NULL;
    threadPool *pool = NULL;

    /** Alloco la struttura **/
    if(traceOnLog(log, "Avvio del pool di %d thread\n", numeroThread) == -1) {
        FREE_POOL_THREAD()
        return NULL;
    }
    if((pool = (threadPool *) malloc(sizeof(threadPool))) == NULL) return NULL;
    memset(pool, 0, sizeof(threadPool));
    if((pool->taskQueueMutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t))) == NULL) {
        FREE_POOL_THREAD()
        return NULL;
    }
    if((pool->emptyCondVar = (pthread_cond_t *) malloc(sizeof(pthread_cond_t))) == NULL) {
        FREE_POOL_THREAD()
        return NULL;
    }
    if((pool->threads = (pthread_t *) calloc(numeroThread, sizeof(pthread_t))) == NULL) {
        FREE_POOL_THREAD()
        return NULL;
    }
    pool->numeroThread = numeroThread;
    pool->isEmpty = 1;
    if((error = pthread_mutex_init(pool->taskQueueMutex, NULL)) != 0) {
        FREE_POOL_THREAD()
        return NULL;
    }
    if((error = pthread_cond_init(pool->emptyCondVar, NULL)) != 0) {
        FREE_POOL_THREAD()
        return NULL;
    }

    /** Avvio i thread **/
    LOCK_POOL(NULL)
    if((arg = (Threads_Arg **) calloc(numeroThread, sizeof(Threads_Arg *))) == NULL) {
        UNLOCK_POOL(NULL)
        FREE_POOL_THREAD()
        return NULL;
    }
    for(int i=0; i<numeroThread; i++) {
        if((arg[i] = (Threads_Arg *) malloc(sizeof(Threads_Arg))) == NULL) {
            UNLOCK_POOL(NULL)
            FREE_POOL_THREAD()
            return NULL;
        }
        arg[i]->idThread = i+1, arg[i]->pool = pool, arg[i]->log = log;
        if((error = pthread_create(&((pool->threads)[i]), NULL, start_routine, arg[i])) != 0) {
            UNLOCK_POOL(NULL)
            FREE_POOL_THREAD()
            return NULL;
        }
        (pool->numeroDiThreadAttivi)++;
    }
    free(arg);
    arg = NULL;
    UNLOCK_POOL(NULL)
    if(traceOnLog(log, "Pool di thread avviato\n", numeroThread) == -1) {
        FREE_POOL_THREAD()
        return NULL;
    }


    return pool;
}


/**
 * @brief                   Funzione che manda un task da eseguire al pool
 * @fun                     pushTask
 * @param pool              Pool di thread
 * @param task              Task da eseguire e mandare al pool
 * @return                  (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int pushTask(threadPool *pool, Task *task) {
    /* Variabili */
    int error = 0;

    /* Controllo parametri */
    if(pool == NULL) { errno = EINVAL; return -1; }
    if(task == NULL) { errno = EINVAL; return -1; }

    /* Prendo il controllo della coda e ci aggiungo il task da eseguire */
    LOCK_POOL(-1)
    if((pool->taskQueue = insertIntoQueue((pool->taskQueue), task, sizeof(Task))) == NULL) {
        return errno;
    }
    (pool->numeroTaskInCoda)++;
    if(pool->isEmpty) {
        pool->isEmpty = 0;
        if((error = pthread_cond_signal(pool->emptyCondVar)) != 0) {
            return errno;
        }
    }
    UNLOCK_POOL(-1)

    return 0;
}


/**
 * @brief               Avvia la fase di arresto del pool di thread
 * @fun                 stopThreadPool
 * @param pool          Pool di thread da fermare
 * @param hardShutdown  Seleziona il tipo di spegnimento del server
 * @return              Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int stopThreadPool(threadPool *pool, int hardShutdown) {
    /** Variabili **/
    Task *endTask = NULL;
    int index = -1;
    int error = 0;

    /** Controllo parametri **/
    if((hardShutdown < 0) || (hardShutdown > 1)) { errno = EINVAL; return -1; }

    /** Avvio fase di spegnimento **/
    printf("Stop pool\n");
    LOCK_POOL(-1)
    if(hardShutdown) {
        destroyQueue(&(pool->taskQueue));
        pool->taskQueue = NULL;
    }
    if((error = pthread_cond_broadcast(pool->emptyCondVar)) != 0) {
        return -1;
    }
    if((endTask = (Task *) malloc(sizeof(Task))) == NULL) return -1;
    while(++index < (pool->numeroThread)) {

        (endTask)->argv = NULL;
        (endTask)->to_do = stopTasking;
        if((pool->taskQueue = insertIntoQueue(pool->taskQueue, endTask, sizeof(Task))) == NULL) {
            return -1;
        }
        (pool->numeroTaskInCoda)++;
    }
    free(endTask);
    pool->isEmpty = 0;
    pool->shutdown = 1;
    UNLOCK_POOL(-1)
    index = -1;
    while(++index < (pool->numeroThread)) {
        if((error = pthread_join((pool->threads)[index], NULL)) != 0) {
            errno = error;
            return -1;
        }
    }
    FREE_POOL_THREAD()

    return 0;
}