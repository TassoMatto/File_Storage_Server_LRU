/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Programma principale di gestione del server
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

// valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes  --track-fds=yes  ./server config.txt
/** Header Files **/
#define _POSIX_C_SOURCE 2001112L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <logFile.h>
#include <FileStorageServer.h>
#include <threadPool.h>
#include <Server_API.h>


/**
 * @brief                   Pulisce e dealloca il server
 * @macro                   freeServer
 * @param HARDSHOT          Parametro che mi indica la volonta' di spegnere il server rapidamente o meno
 */
#define FREE_SERVER(HARDSHOT)                                       \
    do {                                                            \
        error = errno;                                              \
        if(status != NULL) { free(status); }                        \
        if(commitToPool != NULL) { free(commitToPool); }            \
        if(pool != NULL) { stopThreadPool(pool, (HARDSHOT)); }                                                                 \
        if(fd_sk != -1) { close(fd_sk); }                           \
        for(fd = 0; fd <= fd_num; fd++) {                           \
            if(FD_ISSET(fd, &setInit))                              \
                close(fd);                                          \
        }                                                            \
        unlink(setServer->socket);                                  \
        index = -1;                                                            \
        while((cacheLRU->usersConnected)[++index] != NULL) {            \
            printf("Elimonoooo\n");                                                        \
            fd = ((userLink *) (cacheLRU->usersConnected)[index]->data)->fd;                                                   \
            printf("%d\n", fd);close(fd);                                              \
        }                                                           \
        traceOnLog(log, "CIAOOOO\n");                                                            \
        if(setServer != NULL) { deleteLRU(&setServer, &cacheLRU); } \
        if(log != NULL) { printf("CI sonoooooooo\n");stopServerTracing(&log); }                \
        if(pfd[0] != -1) { close(pfd[0]); }                         \
        if(pfd[1] != -1) { close(pfd[1]); }                         \
        if(handler != NULL) free(handler);                          \
        errno = error;                                              \
    } while(0);


/**
 * @brief           Stampa sul log il testo specificato
 * @macro           TRACE_ON_LOG
 * @param TXT       Testo da scrivere
 */
#define TRACE_ON_LOG(TXT, ...)                          \
    if(traceOnLog(log, TXT, ##__VA_ARGS__) == -1) {       \
        FREE_SERVER(1)                                  \
        exit(errno);                                    \
    }


/**
 * @brief               Struttura per passare gli argomenti di interesse al signal Handler
 * @struct              argToHandler
 * @param runnable      Puntatore alla variabile che mi indica di uscire dal while del server per il controllo
 *                      delle connessioni
 * @param pfd           Puntatore alla pipe di ritorno degli fd riabilitati
 * @param fd            Fd principale di accettazione delle connessioni alla socket
 * @param set           Maschera degli fd attivi in lettura
 */
typedef struct {
    int *runnable;
    int *pfd;
    int fd;
    fd_set *set;
} argToHandler;


/**
 * @brief                   Aggiorna la lista degli fd pronti in lettura
 * @fun                     update
 * @param fd_num            Numero degli fd
 * @param set               Maschera degli fd
 * @return                  Ritorna il valore del fd piu' grande
 */
static int update(int fd_num, fd_set *set) {
    /** Variabili **/
    int max = -1, fd = -1;

    /** Aggiornamento **/
    for(fd=0; fd<=fd_num; fd++)
        if(FD_ISSET(fd, set))
            max=fd;

    return max;
}


/**
 * @brief           Funzione che dealloca i task del pool di thread
 * @fun             free_task
 * @param uTask     Task da deallocare
 */
static void free_task(void *uTask) {
    /** Variabili **/
    Task *t = NULL;
    Task_Package *tp = NULL;

    /** Controllo parametri **/
    if(uTask == NULL) return;

    /** Dealloco il task **/
    t = (Task *) uTask;
    tp = (Task_Package *) t->argv;
    if(tp != NULL) {
        free(tp);
    }
    free(t);
}


/**
 * @brief           Funzione che gestisce l'arrivo dei segnali SIGINT- SIGQUIT - SIGHUP
 * @fun             signalHandler
 * @param argv      Argomenti del signal handler
 * @return          Ritorna (1) o (0) a seconda se viene richiesto un Hardshutdown del server o meno
 */
static void* signalHandler(void *argv) {
    /** Variabili **/
    int *status = NULL;
    int error = 0, sig = -1, *runnable = NULL;
    int *pfd = NULL, fd = -1;
    fd_set *set = NULL;
    argToHandler *converted = NULL;
    sigset_t setSignal;

    /** Conversione argomenti **/
    if((status = (int *) malloc(sizeof(int))) == NULL) return &errno;
    converted = (argToHandler *) argv;
    pfd = converted->pfd;
    fd = converted->fd;
    set = converted->set;
    runnable = converted->runnable;
    free(argv);

    /** Imposto la maschera e gestisco i segnali **/
    if(sigemptyset(&setSignal) == -1) return (void *) &errno;
    if(sigaddset(&setSignal, SIGINT) == -1) return (void *) &errno;
    if(sigaddset(&setSignal, SIGQUIT) == -1) return (void *) &errno;
    if(sigaddset(&setSignal, SIGHUP) == -1) return (void *) &errno;
    if(sigaddset(&setSignal, SIGPIPE) == -1) return (void *) &errno;
    if((error = pthread_sigmask(SIG_SETMASK, &setSignal, NULL)) == -1) { errno = error; return (void *) &errno; }
    if((error = sigwait(&setSignal, &sig)) > 0) { errno = error; return (void *) &errno; }

    /** Arrivo del segnale da gestire **/
    FD_CLR(fd, set);
    *runnable = 0;
    switch (sig) {
        case SIGINT:
        case SIGQUIT:
//            close(pfd[0]);
            close(pfd[1]);
            FD_CLR(pfd[0], set);
            *status = 1;
        break;

        default:
            printf("\n\n\n\n\n\n\n\n\n\n\n\n\nDocile\n");
            *status = 0;
    }

    return status;
}


/** Main **/
int main(int argc, char **argv) {
    /** Variabili **/
    int *status = NULL;
    int index = -1;
    int fd = 0, fd_sk = -1, fd_cl = -1, fd_num = 0, selectRes = -1;
    int error = 0, pfd[2] = {-1, -1};
    int runnable = 1;
    ssize_t pipeBytes = -1;
    sigset_t set, oldset;
    fd_set setInit, setRead;
    struct sockaddr_un sock_addr;
    serverLogFile *log = NULL;
    threadPool *pool = NULL;
    pthread_t *handler = NULL;
    argToHandler *sigHand = NULL;
    Settings *setServer = NULL;
    LRU_Memory *cacheLRU = NULL;
    Task *commitToPool = NULL;
    Task_Package *taskPackage = NULL;
    struct timeval selectRefreshig, saveSelectRefreshig;

    /** Controllo parametri **/
    if(argc != 2) {
        fprintf(stderr, "Per favore specificare il file di config per avviare il server...\n");
        fprintf(stderr, "./server FILE_DI_CONFIG\n");
        FREE_SERVER(1)
        exit(errno);
    }

    /** Avvio il processo di logTrace **/
    if((log = startServerTracing("FileStorageServer.log")) == NULL) {
        perror("Creazione del file di log fallita\n");
        FREE_SERVER(1)
        exit(errno);
    }
    TRACE_ON_LOG("[THREAD MANAGER]:\t\t\t\t[FILE STORAGE SERVER]\n")
    TRACE_ON_LOG("[THREAD MANAGER]: Avvio del server in corso...\n")

    /** Maschero segnali **/
    if(sigemptyset(&set) == -1) {
        perror("Signal Masking error");
        FREE_SERVER(1)
        exit(errno);
    }
    if(sigaddset(&set, SIGINT) == -1) {
        perror("Signal Masking error");
        FREE_SERVER(1)
        exit(errno);
    }
    if(sigaddset(&set, SIGQUIT) == -1) {
        perror("Signal Masking error");
        FREE_SERVER(1)
        exit(errno);
    }
    if(sigaddset(&set, SIGHUP) == -1) {
        perror("Signal Masking error");
        FREE_SERVER(1)
        exit(errno);
    }
    if(sigaddset(&set, SIGPIPE) == -1) {
        perror("Signal Masking error");
        FREE_SERVER(1)
        exit(errno);
    }
    if((error = pthread_sigmask(SIG_BLOCK, &set, &oldset)) == -1) {
        FREE_SERVER(1)
        perror("Signal Masking error");
        exit(errno);
    }
    TRACE_ON_LOG("[THREAD MANAGER]: Applicazione della maschera hai segnali SIGINT - SIGQUIT - SIGHUP per gestione personalizzata\n")

    /** Traduzione della configurazione del server dal file di config **/
    if((setServer = readConfigFile(argv[1])) == NULL) {
        FREE_SERVER(1)
        exit(errno);
    }
    TRACE_ON_LOG("[THREAD MANAGER]: Lettura delle impostazioni del server da \"%s\"\n", argv[1])

    /** Apertura della pipe di comunicazione **/
    if(pipe(pfd) == -1) {
        FREE_SERVER(1)
        exit(errno);
    }
    fcntl(pfd[0], F_SETFL, O_NONBLOCK);
    TRACE_ON_LOG("[THREAD MANAGER]: Apertura pipe comunicazione Thread Manager - Thread Worker per riabilitazione del client in lettura sulla socket\n")

    /** Apertura della socket **/
    if((fd_sk = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        FREE_SERVER(1)
        exit(errno);
    }
    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, setServer->socket, strnlen(setServer->socket, MAX_PATHNAME)+1);
    if(bind(fd_sk, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) == -1) {
        FREE_SERVER(1)
        exit(errno);
    }
    if(listen(fd_sk, (int) setServer->maxUtentiConnessi) == -1) {
        FREE_SERVER(1)
        exit(errno);
    }
    TRACE_ON_LOG("[THREAD MANAGER]: Apertura della socket \"%s\"\n", setServer->socket)

    /** Preparazione degli fd da ascoltare in lettura **/
    if(fd_sk > fd_num) fd_num = fd_sk;
    FD_ZERO(&setInit);
    FD_SET(fd_sk, &setInit);            //Abilito il listen socket
    FD_SET(pfd[0], &setInit);           //Abilito la pipe in lettura sulla select

    /** Avvio del thread pool **/
    if((pool = startThreadPool(setServer->numeroThreadWorker, free_task, log)) == NULL) {
        FREE_SERVER(1)
        perror("Errore");
        exit(errno);
    }
    TRACE_ON_LOG("[THREAD MANAGER]: Avvio del threadpool effettuato correttamente: avviati \"%d\" thread\n", setServer->numeroThreadWorker)

    /** Avvio memoria cache LRU **/
    if((cacheLRU = startLRUMemory(setServer, log)) == NULL) {
        FREE_SERVER(1)
        exit(errno);
    }

    /** Gestione segnali personalizzata **/
    if((handler = (pthread_t *) malloc(sizeof(pthread_t))) == NULL) {
        FREE_SERVER(1)
        exit(errno);
    }
    if((sigHand = (argToHandler *) malloc(sizeof(argToHandler))) == NULL) {
        FREE_SERVER(1)
        exit(errno);
    }
    sigHand->set = &setInit;
    sigHand->pfd = pfd;
    sigHand->fd = fd_sk;
    sigHand->runnable = &runnable;
    if((error = pthread_create(handler, NULL, signalHandler, sigHand)) != 0) {
        FREE_SERVER(1)
        exit(errno);
    }
    TRACE_ON_LOG("[THREAD MANAGER]: Gestione dei segnali affidata a thread specializzato\n")

    /** Inizio del lavoro per il server **/
    selectRefreshig.tv_sec = 2;
    selectRefreshig.tv_usec = 0;
    if((commitToPool = (Task *) malloc(sizeof(Task))) == NULL) {
        FREE_SERVER(1)
        exit(errno);
    }
    TRACE_ON_LOG("[THREAD MANAGER]: Server avviato correttamente...\n")
    while(runnable ||  ((FD_ISSET(pfd[0], &setInit)) && (clientOnline(cacheLRU)>0))) {
        /** Aspetto che vi venga mandata una richiesta **/
        memcpy(&setRead, &setInit, sizeof(fd_set));
        saveSelectRefreshig = selectRefreshig;
        if(((selectRes = select(fd_num+1, &setRead, NULL, NULL, &saveSelectRefreshig)) == -1) && (errno != EINTR)) {
            FREE_SERVER(1)
            exit(errno);
        } else if (selectRes <= 0 && runnable) continue;

        TRACE_ON_LOG("[THREAD MANAGER]: Select: nuovi fd pronti in lettura...  %d\n", clientOnline(cacheLRU))
        for(fd = 0; fd <= fd_num; fd++) {
            if(FD_ISSET(fd, &setRead)) {
                TRACE_ON_LOG("[THREAD MANAGER]: fd:\"%d\" pronto in lettura\n", fd)
                if(fd == fd_sk) { /** Richiesta di connessione di un nuovo client **/
                    /** Abilito in lettura il nuovo client **/
                    TRACE_ON_LOG("[THREAD MANAGER]: Accept(): richiesta di accettazione di un client\n")
                    if(loginClient(cacheLRU) == -1) {
                        TRACE_ON_LOG("[THREAD MANAGE]: Troppi utenti connessi, il client deve attendere...\n")
                        continue;
                    }
                    if(((fd_cl = accept(fd_sk, NULL, 0)) == -1) && (errno != EINTR)) {
                        FREE_SERVER(1)
                        exit(errno);
                    }
                    (cacheLRU->numTotLogin)++;
                    FD_SET(fd_cl, &setInit);
                    if(fd_cl > fd_num) fd_num = fd_cl;
                    TRACE_ON_LOG("[THREAD MANAGER]: Accept(): client con fd:\"%d\" accettato\n", fd_cl)
                } else if(fd == pfd[0]) { /** Richiesta di riabilitazione di un client in lettura o chiusura della connessione col server **/
                    TRACE_ON_LOG("[THREAD MANAGER]: Pipe pronta in lettura: riabilitazione vecchi fd disabilitati\n")
                    pipeBytes = read(pfd[0], &fd_cl, sizeof(int));
                    printf("%ld\n\n\n\n", pipeBytes);
                    while(pipeBytes > 0) {
                        /** Controllo che il client non abbia chiuso la comunicazione **/
                        if ((fcntl(fd_cl, F_GETFD) != -1) || (errno != EBADF)) {
                            TRACE_ON_LOG("[THREAD MANAGER]: Client con fd:\"%d\" riabilitato\n", fd_cl)
                            FD_SET(fd_cl, &setInit);
                            if (fd_cl > fd_num) fd_num = fd_cl;
                        } //else {
//                            TRACE_ON_LOG("[THREAD MANAGER]: Client con fd:\"%d\" connessione chiusa\n", fd_cl)
//                            close(fd_cl);
//                            errno = 0;
//                        }
                        pipeBytes = read(pfd[0], &fd_cl, sizeof(int));
                    }
                    if(errno == EAGAIN) continue;
                    if(errno == EPIPE) break;
                    if((pipeBytes == -1) && (errno != 0) && (errno != EPIPE) && (errno != EAGAIN) && (errno != EINTR) && (errno != EBADF)) {
                        FREE_SERVER(1)
                        exit(errno);
                    }
                    errno = 0;
                } else {
                    if((taskPackage = (Task_Package *) malloc(sizeof(Task_Package))) == NULL) {
                        FREE_SERVER(1)
                        exit(errno);
                    }
                    TRACE_ON_LOG("[THREAD MANAGER]: Client con fd:\"%d\", invio task al pool di thread\n", fd)
                    taskPackage->fd = fd;
                    taskPackage->cache = cacheLRU;
                    taskPackage->pfd = pfd[1];
                    taskPackage->log = log;
                    commitToPool->argv = taskPackage;
                    commitToPool->to_do = ServerTasks;
                    if(pushTask(pool, commitToPool) == -1) {
                        FREE_SERVER(1)
                        exit(errno);
                    }
                    FD_CLR(fd, &setInit);
                    fd_num = update(fd_num, &setInit);
                    TRACE_ON_LOG("[THREAD MANAGER]: Client con fd:\"%d\", richiesta al pool di thread inviata correttamente\n", fd)
                }
            }
        }
    }


    /** Arresto del server **/
    TRACE_ON_LOG("[THREAD MANAGER]: Server in fase di spegnimento...\n")
    if((error = pthread_join(*handler, (void **) &status)) != 0) {
        FREE_SERVER(1)
        exit(errno);
    }
    if(*status == 1) {
        TRACE_ON_LOG("[THREAD MANAGER]: Arresto forzato: richiesta di spegnimento immediato\n")
        FREE_SERVER(1)
    } else {
        TRACE_ON_LOG("[THREAD MANAGER]: Arresto: richiesta di spegnimento graduale; attesa di completamento dei task pendenti nel pool\n")
        FREE_SERVER(0)
    }

    return 0;
}
