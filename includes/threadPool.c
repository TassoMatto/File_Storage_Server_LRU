/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di un pool di thread
 * @author              Simone Tassotti
 * @date                24/12/2021
 */


#include "threadPool.h"


typedef struct {
    unsigned int idThread;
    threadPool *pool;
} Threads_Arg;


/**
 * @brief               Dealloca l'intero pool di thread
 * @macro               FREE_POOL_THREAD
 */
#define FREE_POOL_THREAD() \
    do {                   \
        error = errno;     \
        /** Mettere qualcosa che mandi in tilt il pool e che lo faccia terminare **/                   \
        if(pool->taskQueueMutex != NULL) { \
            pthread_mutex_destroy(pool->taskQueueMutex);                   \
            free(pool->taskQueueMutex);        \
        }\
        if(pool->emptyCondVar != NULL) {   \
           pthread_cond_destroy(pool->emptyCondVar);                   \
            free(pool->emptyCondVar);                    \
        }                  \
        if(pool->threads != NULL) free(pool->threads); \
        if(pool != NULL) free(pool);     \
        errno = error;                       \
    } while(0);


/**
 * @brief               Lock del pool di thread
 * @macro               LOCK_POOL
 */
#define LOCK_POOL()                                                 \
    if((error = pthread_mutex_lock(pool->taskQueueMutex)) != 0) {   \
        FREE_POOL_THREAD()                                          \
        return NULL;                                                \
    }


/**
 * @brief               Unlock del pool di thread
 * @macro               UNLOCK_POOL
 */
#define UNLOCK_POOL()                                               \
    if((error = pthread_mutex_unlock(pool->taskQueueMutex)) != 0) { \
        FREE_POOL_THREAD()                                          \
        return NULL;                                                \
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
    Threads_Arg *castedArg = NULL;
    threadPool *pool = NULL;
    unsigned int numeroDelThread = -1;

    /** Cast degli argomenti **/
    castedArg = (Threads_Arg *) argv;
    pool = castedArg->pool;
    numeroDelThread = castedArg->idThread;
    free(castedArg);


    return NULL;
}


/**
 * @brief                   Crea un pool di thread
 * @fun                     startThreadPool
 * @param numeroThread      Numero di thread da creare nel pool
 * @return                  Ritorna la struttura che rappresenta il pool; in caso di errore ritorna NULL [setta errno]
 */
threadPool* startThreadPool(unsigned int numeroThread) {
    /** Variabili **/
    int error;
    threadPool *pool = NULL;

    /** Alloco la struttura **/
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
    LOCK_POOL()
    for(int i=0; i<numeroThread; i++) {
        if((error = pthread_create(&((pool->threads)[i]), NULL, start_routine, NULL)) != 0) {
            FREE_POOL_THREAD()
            return NULL;
        }
    }
    UNLOCK_POOL()

    return pool;
}