/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione delle API che ricevo dal client
 * @author              Simone Tassotti
 * @date                03/01/2022
 */

#include "Server_API.h"


void* ServerTasks(unsigned int numeroDelThread, void *argv) {
    /** Variabili **/
    int pipe = -1, *fd = NULL;
    char *request = NULL;
    size_t requestSize = 0;
    Task_Package *tp = NULL;
    serverLogFile *log = NULL;
    Pre_Inserimento *pI = NULL;
    LRU_Memory *cache = NULL;

    /** Controllo parametri **/
    if(argv == NULL) { errno = EINVAL; return (void *) &errno; }

    /** Conversione argomenti **/
    if((fd = (int *) malloc(sizeof(int))) == NULL) {
        free(argv);
        return (void *) &errno;
    }
    tp = (Task_Package *) argv;
    pipe = tp->pfd;
    *fd = tp->fd;
    log = tp->log;
    cache = tp->cache;
    pI = tp->pI;

    /** Ascolto richiesta dal client **/
    if(receiveMSG(*fd, (void *) &request, &requestSize) <= 0) {
        errno = ECOMM;
        return (void *) &errno;
    }
    printf("Richiesta: %s\n", request);

    /** openFile **/
    if(strncmp(request, "openFile", (size_t) fmax(9, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        int *flags = NULL, res = -1;
        char *pathname = NULL;
        size_t path_len = -1;

        /** Tentativo di openFile **/
        if(traceOnLog(log, "Thread n°%d: richiesta di openFile\n", numeroDelThread) == -1) {
            close(*fd);
            free(argv);
            free(fd);
            return (void *) &errno;
        }
        if(receiveMSG(*fd, (void **) &pathname, &path_len) <= 0) {
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(receiveMSG(*fd, (void **) &flags, NULL) <= 0) {
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        printf("Pathname con flags: %s -- %d\n", pathname, *flags);

        switch (*flags) {
            case 0:
                if((res = openFileOnCache(cache, pathname, *fd)) == -1) {
                    if(traceOnLog(log, "Thread n°%d: openFile su %s fallita: errore n°%d\n", numeroDelThread, pathname, errno) == -1) {
                        close(*fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                }
                if((res == 0) && (traceOnLog(log, "Thread n°%d: openFile su %s riuscita: file aperto dal client con FD: %d\n", numeroDelThread, pathname, *fd) == -1)) {
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if((res == 1) && (traceOnLog(log, "Thread n°%d: openFile su %s sospesa: file gia' aperto dal client con FD: %d\n", numeroDelThread, pathname, *fd) == -1)) {
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if(res == -1) {
                    if(sendMSG(*fd, (void *) &errno, sizeof(int)) <= 0) {
                        close(*fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                } else {
                    if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
                        close(*fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                }
            break;

            case O_CREATE:
            case O_CREATE | O_LOCK:
                if(createFileToInsert(pI, pathname, cache->maxUtentiPerFile, *fd, (*flags == (O_CREATE | O_LOCK))) == -1) res = -1;
                else res = 0;

                if((res == -1) && (traceOnLog(log, "Thread n°%d: openFile su %s fallita: errore n°%d\n\n", numeroDelThread, pathname, errno) == -1)) {
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if((res == 0) && (traceOnLog(log, "Thread n°%d: openFile su %s riuscita: file creato con flags %s dal client con FD: %d\n", numeroDelThread, pathname, ((*flags == (O_CREATE | O_LOCK)) ? "O_CREATE | O_LOCK" : "O_CREATE"), *fd) == -1)) {
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if(res == -1) {
                    if(sendMSG(*fd, (void *) &errno, sizeof(int)) <= 0) {
                        close(*fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                } else {
                    if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
                        close(*fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                }
            break;

            default:
                printf("Problema gestione");
                errno = EINVAL;
                if(sendMSG(*fd, (void *) &errno, sizeof(int)) <= 0) {
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
        }
        free(pathname);
        free(flags);
    }

    /** readFile **/
    if(strncmp(request, "readFile", (size_t) fmax(9, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        char *pathname = NULL;
        void *bufferFile = NULL;
        size_t dimBuffer = -1;

        if(receiveMSG(*fd, (void **) &pathname, NULL) <= 0) {
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if((dimBuffer = readFileOnCache(cache, pathname, &bufferFile)) == -1) {
            if(sendMSG(*fd, &errno, sizeof(int)) <= 0) {
                free(bufferFile);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
        } else {
            if(sendMSG(*fd, 0, sizeof(int)) <= 0) {
                free(bufferFile);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
            if(sendMSG(*fd, bufferFile, dimBuffer) <= 0) {
                free(bufferFile);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
        }
        free(bufferFile);
        free(pathname);
    }

    /** Riabilito fd in lettura nel server **/
    if((write(pipe, (void *) fd, sizeof(int)) <= 0) && (errno != EPIPE)) {
        close(*fd);
        free(fd);
        printf("Non scrivo\n\n\n");
        return (void *) -1;
    }
    free(fd);
    free(request);

    return (void *) 0;
}
