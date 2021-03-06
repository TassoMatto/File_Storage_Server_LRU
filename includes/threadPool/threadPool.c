/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di un pool di thread
 * @author              Simone Tassotti
 * @date                24/12/2021
 * @finish              25/01/2022
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
#define FREE_POOL_THREAD()                                                                  \
    do {                                                                                    \
        error = errno;                                                                      \
        if((pool->taskQueueMutex) != NULL) {                                                \
            pthread_mutex_destroy(pool->taskQueueMutex);                                    \
            free(pool->taskQueueMutex);                                                     \
        }                                                                                   \
        if(pool->emptyCondVar != NULL) {                                                    \
            pthread_cond_destroy(pool->emptyCondVar);                                       \
            free(pool->emptyCondVar);                                                       \
        }                                                                                   \
        if(pool->threads != NULL) { free(pool->threads); }                                  \
        if(pool->taskQueue != NULL) { destroyQueue(&(pool->taskQueue), pool->free_task); }  \
        if(pool != NULL) { free(pool); }                                                    \
        errno = error;                                                                      \
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
 * @brief           Macro che gestisce il tracciamento sul file di log
 * @macro           TRACE_ON_LOG
 * @param DELETE    Se voglio ritornare un valore di ritorno
 * @param RET_VALUE Valore di ritorno eventuale
 * @param TXT       Testo da mandare al log file e gli eventuali parametri
 */
#define TRACE_ON_LOG(DELETE, RET_VALUE, TXT, ...)                   \
    if(traceOnLog(log, (TXT), ##__VA_ARGS__) == -1) {               \
        if(!(DELETE)) return (RET_VALUE);                           \
        else {                                                      \
            FREE_POOL_THREAD()                                      \
            return (RET_VALUE);                                     \
        }                                                           \
    }

/**
 * @brief               Funzione per l'arresto dei thread worker del pool
 * @fun                 stopTasking
 * @param a, b          Argomenti non utilizzati nella funzione
 * @return              NULL sempre
 */
static void* stopTasking(unsigned int a, void *b) {
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
    char errorMSG[MAX_BUFFER_LEN];
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
    TRACE_ON_LOG(0, &errno, "[THREAD %d]: thread avviato correttamente\n", numeroDelThread)
    LOCK_POOL(NULL)
    do {
        /** Se la lista e' vuota mi metto in attesa */
        while(pool->isEmpty && (!pool->hardST)) {
            (pool->numeroDiThreadAttivi)--;
            TRACE_ON_LOG(0, &errno, "[THREAD %d]: Nessun task trovato, mi metto in attesa\n", numeroDelThread)
            if((error = pthread_cond_wait((pool->emptyCondVar), (pool->taskQueueMutex))) != 0) {
                return (void *) &errno;
            }
            TRACE_ON_LOG(0, &errno, "[THREAD %d]: Segnalato nuovo task in queue\n", numeroDelThread)
            (pool->numeroDiThreadAttivi)++;
        }

        /** Estraggo il task dalla coda e lo eseguo **/
        if((uncastedTask = deleteFirstElement(&(pool->taskQueue))) == NULL) {
            UNLOCK_POOL(NULL)
            break;
        }
        (pool->numeroTaskInCoda)--;
        if((pool->taskQueue == NULL) && (!pool->shutdown)) pool->isEmpty = 1;
        UNLOCK_POOL(NULL)
        TRACE_ON_LOG(0, &errno, "[THREAD %d]: Estratto dalla queue nuovo task da eseguire\n", numeroDelThread)
        t = (Task *) uncastedTask;
        work = t->to_do;
        TRACE_ON_LOG(0, &errno, "[THREAD %d]: Avvio del task da eseguire in corso...\n", numeroDelThread)
        if(work(numeroDelThread, t->argv) != NULL) {
            if(errno == ECOMM || errno == EPIPE || errno == EBADF) {
                TRACE_ON_LOG(0, &errno, "[THREAD %d]: Chiusura della connessione con il client\n", numeroDelThread)
            } else {
                if(strerror_r(errno, errorMSG, MAX_BUFFER_LEN) != 0) {
                    return &errno;
                }
                TRACE_ON_LOG(0, &errno, "[THREAD %d]: Stop improvviso del task - Errore:%s\n", numeroDelThread, errorMSG)
            }
        }
        pool->free_task(uncastedTask);
        TRACE_ON_LOG(0, &errno, "[THREAD %d]: Task terminato con successo\n", numeroDelThread)
        LOCK_POOL(NULL)
    } while((!(pool->shutdown)) || ((pool->taskQueue != NULL) && (!pool->hardST)));
    UNLOCK_POOL(NULL)


    /** Arresto del thread **/
    TRACE_ON_LOG(0, &errno, "[THREAD %d]: arresto in corso\n", numeroDelThread)
    free(status);
    return NULL;
}


/**
 * @brief                   Crea un pool di thread
 * @fun                     startThreadPool
 * @param numeroThread      Numero di thread da creare nel pool
 * @param free_task         Funzione per pulire i task una volta eseguiti
 * @param log               File di log
 * @return                  Ritorna la struttura che rappresenta il pool; in caso di errore ritorna NULL [setta errno]
 */
threadPool* startThreadPool(unsigned int numeroThread, void (*free_task)(void *), serverLogFile *log) {
    /** Variabili **/
    int error;
    Threads_Arg **arg = NULL;
    threadPool *pool = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(free_task == NULL) { errno = EINVAL; return NULL; }

    /** Alloco la struttura **/
    TRACE_ON_LOG(1, NULL, "[THREAD MANAGER]: Avvio del pool di %d thread\n")
    if((pool = (threadPool *) malloc(sizeof(threadPool))) == NULL) {
        return NULL;
    }
    memset(pool, 0, sizeof(threadPool));
    pool->log = log;
    pool->free_task = free_task;
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
    TRACE_ON_LOG(1, NULL, "[THREAD MANAGER]: Pool di thread avviato\n")

    /** Pool di thread avviato correttamente **/
    errno = 0;
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
    /** Variabili **/
    int error = 0;
    serverLogFile *log = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(pool == NULL) { errno = EINVAL; return -1; }
    if(task == NULL) { errno = EINVAL; return -1; }

    /** Prendo il controllo della coda e ci aggiungo il task da eseguire **/
    log = pool->log;
    TRACE_ON_LOG(0, errno, "[THREAD MANAGER]: Richiesta di inserimento nuovo task\n")
    LOCK_POOL(-1)
    if(pool->hardST) {
        UNLOCK_POOL(-1)
        errno = 0;
        return 0;
    }
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
    TRACE_ON_LOG(0, errno, "[THREAD MANAGER]: Task inviato correttamente\n")

    errno = 0;
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
    int index = -1, *status = NULL;
    int error = 0;
    serverLogFile *log = NULL;

    /** Controllo parametri **/
    errno = 0;
    if((hardShutdown < 0) || (hardShutdown > 1)) { errno = EINVAL; return -1; }

    /** Avvio fase di spegnimento **/
    log = pool->log;
    LOCK_POOL(-1)
    if(hardShutdown) {
        pool->hardST = 1;
        destroyQueue(&(pool->taskQueue), pool->free_task);
        pool->taskQueue = NULL;
        TRACE_ON_LOG(0, -1, "[THREAD MANAGER]:Arresto forzato; inizio procedura di hard-shutdown del pool\n")
    } else {
        TRACE_ON_LOG(0, -1, "[THREAD MANAGER]: Arresto soft del pool di thread; invio messaggio di arresto\n")
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
        if((error = pthread_join((pool->threads)[index], (void **) &status)) != 0) {
            errno = error;
            return -1;
        }
        free(status);
    }
    TRACE_ON_LOG(0, -1, "[THREAD MANAGER]: Pool di thread fermato correttamente\n")
    FREE_POOL_THREAD()

    errno = 0;
    return 0;
}