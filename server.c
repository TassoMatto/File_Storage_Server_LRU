/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Programma principale di gestione del server
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

// valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes  --track-fds=yes  ./server config.txt
/** Header Files **/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <logFile.h>
#include <queue.h>
#include <FileStorageServer.h>
#include <threadPool.h>


#define _POSIX_C_SOURCE 200809L
#define REQUEST_QUEUE_SOCKET 20


/**
 * @brief                   Pulisce e dealloca il server
 * @macro                   freeServer
 */
#define FREE_SERVER()                                               \
    do {                                                            \
        error = errno;                                              \
        if(pool != NULL) { stopThreadPool(pool, 0); }               \
        if(fd_sk != -1) { close(fd_sk); }                           \
        unlink(setServer->socket);                                  \
        if(setServer != NULL) { deleteLRU(&setServer, &cacheLRU); } \
        if(log != NULL) { stopServerTracing(&log); }                \
        if(pfd[0] != -1) { close(pfd[0]); }                         \
        if(pfd[1] != -1) { close(pfd[1]); }                         \
        errno = error;                                              \
    } while(0);


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


/** Main **/
int main(int argc, char **argv) {
    /** Variabili **/
    int fd_sk = -1, fd_num = 0;
    int error = 0, pfd[2] = {-1, -1};
    sigset_t set, oldset;
    fd_set setInit/*, setRead*/;
    struct sockaddr_un sock_addr;
    serverLogFile *log = NULL;
    threadPool *pool = NULL;
    Settings *setServer = NULL;
    LRU_Memory *cacheLRU = NULL;

    /** Controllo parametri **/
    if(argc != 2) {
        fprintf(stderr, "Per favore specificare il file di config per avviare il server...\n");
        fprintf(stderr, "./server FILE_DI_CONFIG\n");
        FREE_SERVER()
        exit(errno);
    }

    /** Avvio il processo di logTrace **/
    if((log = startServerTracing("FileStorageServer.log")) == NULL) {
        perror("Creazione del file di log fallita\n");
        FREE_SERVER()
        exit(errno);
    }
    if(traceOnLog(log, "\t\t\t\t[FILE STORAGE SERVER]\n") == -1) {
        FREE_SERVER()
        exit(errno);
    }
    if(traceOnLog(log, "Avvio del server in corso...\n") == -1) {
        FREE_SERVER()
        exit(errno);
    }

    /** Maschero segnali **/
    if(sigemptyset(&set) == -1) {
        perror("Signal Masking error");
        FREE_SERVER()
        exit(errno);
    }
    if(sigaddset(&set, SIGINT) == -1) {
        perror("Signal Masking error");
        FREE_SERVER()
        exit(errno);
    }
    if(sigaddset(&set, SIGQUIT) == -1) {
        perror("Signal Masking error");
        FREE_SERVER()
        exit(errno);
    }
    if(sigaddset(&set, SIGHUP) == -1) {
        perror("Signal Masking error");
        FREE_SERVER()
        exit(errno);
    }
    if((error = pthread_sigmask(SIG_BLOCK, &set, &oldset)) == -1) {
        FREE_SERVER()
        perror("Signal Masking error");
        exit(errno);
    }
    if(traceOnLog(log, "Applicazione della maschera hai segnali SIGINT - SIGQUIT - SIGHUP per gestione personalizzata\n") == -1) {
        FREE_SERVER()
        exit(errno);
    }

    /** Traduzione della configurazione del server dal file di config **/
    if((setServer = readConfigFile(argv[1])) == NULL) {
        FREE_SERVER()
        exit(errno);
    }
    if(traceOnLog(log, "Lettura delle impostazioni del server da %s\n", argv[1]) == -1) {
        FREE_SERVER()
        exit(errno);
    }

    /** Apertura della pipe di comunicazione **/
    if(pipe(pfd) == -1) {
        FREE_SERVER()
        exit(errno);
    }
    fcntl(pfd[0], F_SETFL, O_NONBLOCK);
    if(traceOnLog(log, "Apertura pipe comunicazione Thread Manager - Thread Worker per riabilitazione del client in lettura sulla socket\n") == -1) {
        FREE_SERVER()
        exit(errno);
    }

    /** Apertura della socket **/
    if((fd_sk = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        FREE_SERVER()
        exit(errno);
    }
    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, setServer->socket, strnlen(setServer->socket, MAX_PATHNAME)+1);
    if(bind(fd_sk, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) == -1) {
        FREE_SERVER()
        exit(errno);
    }
    if(listen(fd_sk, REQUEST_QUEUE_SOCKET) == -1) {
        FREE_SERVER()
        exit(errno);
    }
    if(traceOnLog(log, "Apertura della socket %s\n", setServer->socket) == -1) {
        FREE_SERVER()
        exit(errno);
    }

    /** Preparazione degli fd da ascoltare in lettura **/
    if(fd_sk > fd_num) fd_num = fd_sk;
    FD_ZERO(&setInit);
    FD_SET(fd_sk, &setInit);            //Abilito il listen socket
    FD_SET(pfd[0], &setInit);           //Abilito la pipe in lettura sulla select
    update(0, &setInit); //DA TOGLIERE

    /** Avvio del thread pool **/
    if((pool = startThreadPool(setServer->numeroThreadWorker, log)) == NULL) {
        FREE_SERVER()
        exit(errno);
    }

    cacheLRU = startLRUMemory(setServer, log);
    myFile *f = createFile("ciao", 7, NULL);
    addFileOnCache(cacheLRU, &f);
    printf("%d\n", openFileOnCache(cacheLRU, "ciao", 29));

    f = createFile("ciaol", 7, NULL);
    addFileOnCache(cacheLRU, &f);
    printf("%d\n", openFileOnCache(cacheLRU, "ciaol", 29));
    printf("%d\n", openFileOnCache(cacheLRU, "cial", 29));

    printf("%d\n\n", lockFileOnCache(cacheLRU, "ciaol", 12));
    perror("Ops: ");
    printf("%d\n\n", lockFileOnCache(cacheLRU, "ciao", 29));
    printf("%d\n\n", unlockFileOnCache(cacheLRU, "ciao", 29));
    f = removeFileOnCache(cacheLRU, "ciao");
    destroyFile(&f);
    f = (removeFileOnCache(cacheLRU, "ciaol"));
    destroyFile(&f);


    /** Arresto del server **/
    if(traceOnLog(log, "Server in fase di spegnimento...\n", argv[1]) == -1) {
        FREE_SERVER()
        exit(errno);
    }
    FREE_SERVER()

    return 0;
}
