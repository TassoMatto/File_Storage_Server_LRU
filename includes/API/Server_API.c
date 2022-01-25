/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione delle API che ricevo dal client
 * @author              Simone Tassotti
 * @date                03/01/2022
 */


#include "Server_API.h"


#define FREE_MEMORY() \
    if(fd != NULL) { free(fd); } \
    if(flags != NULL) { free(flags); } \
    if(request != NULL) { free(request); }\
    if(pathname != NULL) { free(pathname); }\
    if(bufferFile != NULL) { destroyFile(&bufferFile); }

#define CLIENT_GOODBYE                                              \
    do {                                                            \
        size_t b, tot = 0;                                          \
        errno=0;                                                            \
        logoutClient(cache);                                        \
        traceOnLog(log, "%d\n", errno);                                                            \
        locks = deleteClientFromCache(cache, *fd);                  \
                                                                    \
        traceOnLog(log, "addio\n");                                                            \
        if(errno == 0 && locks != NULL) {                           \
           int i = -1;                                              \
           while(locks[++i] != -1) {                                \
               b = sendMSG(locks[i], (void *) &errno, sizeof(int)); \
               if(b != -1) tot += b;                                \
           }                                                        \
           traceOnLog(log, "[THREAD %d]: mandati \"%d\" B\n", tot); \
                free(locks);                                                            \
        }                                                           \
    } while(0)


#define LOG_PRINT_FLAGS \
    (*flags == 0) ? "0" : ((*flags == (O_CREATE | O_LOCK)) ? "O_CREATE | O_LOCK" : "O_CREATE")


/**
 * @brief                       Accoglie le richieste del client
 * @fun                         ServerTasks
 * @param numeroDelThread       Numero del thread che esegue la task
 * @param argv                  Argomenti che passo al thread
 * @return                      (NULL) in caso di successo; altrimenti riporto un messaggio di errore
 */
void* ServerTasks(unsigned int numeroDelThread, void *argv) {
    /** Variabili **/
    int *locks = NULL;
    int pipe = -1, *fd = NULL, *flags = NULL, *N = NULL;
    char *request = NULL, *pathname = NULL, errorMsg[MAX_BUFFER_LEN];
    void *bufferFile = NULL;
    myFile **kickedFiles = NULL, **readFiles = NULL;
    myFile *resCancellazione = NULL;
    size_t requestSize = 0;
    size_t path_len = -1;
    size_t bytesRead = 0, bytesWrite = 0, fromMem = 0;
    ssize_t bytes = -1;
    Task_Package *tp = NULL;
    serverLogFile *log = NULL;
    LRU_Memory *cache = NULL;

    /** Controllo parametri **/
    if(argv == NULL) { errno = EINVAL; return (void *) &errno; }

    /** Conversione argomenti **/
    if((fd = (int *) malloc(sizeof(int))) == NULL) {
        return (void *) &errno;
    }
    tp = (Task_Package *) argv;
    pipe = tp->pfd;
    *fd = tp->fd;
    log = tp->log;
    cache = tp->cache;

    /** Ascolto richiesta dal client **/
    errno = 0;
    if(traceOnLog(log, "[THREAD %d]: Ascolto la richiesta del client\n", numeroDelThread) == -1) {
        CLIENT_GOODBYE;
        close(*fd);
        free(fd);
        return (void *) &errno;
    }
    if(receiveMSG(*fd, (void *) &request, &requestSize) <= 0) {
        CLIENT_GOODBYE;
        errno = ECOMM;
        close(*fd);
        free(fd);
        return (void *) &errno;
    }
    if(traceOnLog(log, "[THREAD %d]: Richiesta di \"%s\" da parte del client con fd:%d\n", numeroDelThread, request, *fd) == -1) {
        CLIENT_GOODBYE;
        close(*fd);
        free(fd);
        free(request);
        return (void *) &errno;
    }

    /** openFile **/
    if(strncmp(request, "openFile", (size_t) fmax(9, (double) requestSize)) == 0) {
        /** Variabili blocco **/
        int res = -1;

        /** Tentativo di openFile **/
        if((bytes = receiveMSG(*fd, (void **) &pathname, &path_len)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if((bytes = receiveMSG(*fd, (void **) &flags, NULL)) <= 0) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: Tentativo da parte del client di fd \"%d\" di \"openFile\" del file \"%s\"\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }
        switch (*flags) {
            case 0:
                res = openFileOnCache(cache, pathname, *fd);
                if((res == 0) && (traceOnLog(log, "[THREAD %d]: \"openFile\" su \"%s\" del client con fd \"%d\" con flags \"%s\" riuscita\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS) == -1)) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if((res == 1) && (traceOnLog(log, "[THREAD %d]: \"openFile\" su \"%s\" del client con fd \"%d\" con flags \"%s\" sospesa - Il client aveva già aperto il file\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS) == -1)) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if(res == -1) {
                    if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                        CLIENT_GOODBYE;
                        close(*fd);
                        free(fd);
                        free(pathname);
                        free(flags);
                        free(request);
                        return (void *) &errno;
                    }
                    if(traceOnLog(log, "[THREAD %d]: \"openFile\" su \"%s\" del client con fd \"%d\" con flags \"%s\" non riuscita - Errore \"%s\"\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS, errorMsg) == -1) {
                        CLIENT_GOODBYE;
                        close(*fd);
                        free(fd);
                        free(pathname);
                        free(flags);
                        free(request);
                        return (void *) &errno;
                    }
                    if((bytes = sendMSG(*fd, (void *) &errno, sizeof(int))) <= 0) {
                        CLIENT_GOODBYE;
                        close(*fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                    bytesWrite += bytes;
                } else {
                    if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
                        CLIENT_GOODBYE;
                        close(*fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                    bytesWrite += bytes;
                }
            break;

            case O_CREATE:
            case O_CREATE | O_LOCK:
                errno = 0;
                if(createFileToInsert(cache, pathname, cache->maxUtentiPerFile, *fd, (*flags == (O_CREATE | O_LOCK))) == -1) res = -1;
                else res = 0;

                if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(pathname);
                    free(flags);
                    free(request);
                    return (void *) &errno;
                }
                if((res == -1) && (traceOnLog(log, "[THREAD %d]: \"openFile\" su \"%s\" del client con fd \"%d\" con flags \"%s\" non riuscita - Errore \"%s\"\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS, errorMsg) == -1)) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if((res == 0) && (traceOnLog(log, "[THREAD %d]: \"openFile\" su \"%s\" del client con fd \"%d\" con flags \"%s\" riuscita\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS) == -1)) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if(res == -1) {
                    if((bytes = sendMSG(*fd, (void *) &errno, sizeof(int))) <= 0) {
                        CLIENT_GOODBYE;
                        close(*fd);
                        free(fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                    bytesWrite += bytes;
                } else {
                    if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
                        CLIENT_GOODBYE;
                        close(*fd);
                        free(fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                    bytesWrite += bytes;
                }
            break;

            default:
                if(traceOnLog(log, "[THREAD %d]: \"openFile\" su \"%s\" del client con fd \"%d\" - Errore nell'identificazione dei flags", numeroDelThread, pathname, *fd) == -1) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(pathname);
                    free(flags);
                    free(request);
                    return (void *) &errno;
                }
                errno = EINVAL;
                if((bytes = sendMSG(*fd, (void *) &errno, sizeof(int))) <= 0) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                bytesWrite += bytes;
        }
        if(traceOnLog(log, "[THREAD %d]: \"openFile\" su \"%s\" del client con fd \"%d\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, pathname, *fd, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        free(pathname);
        free(flags);
    }

    /** readFile **/
    if(strncmp(request, "readFile", (size_t) fmax(9, (double) requestSize)) == 0) {
        /** Variabili blocco **/
        size_t dimBuffer = -1;

        if((bytes = receiveMSG(*fd, (void **) &pathname, &path_len)) <= 0) {
            CLIENT_GOODBYE;
            free(request);
            close(*fd);
            free(fd);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: tentativo di \"readFile\" del client fd \"%d\" del file \"%s\"\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            free(request);
            free(pathname);
            close(*fd);
            free(fd);
            return (void *) &errno;
        }
        if((dimBuffer = readFileOnCache(cache, pathname, *fd, &bufferFile)) == -1) {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                CLIENT_GOODBYE;
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"readFile\" del client fd \"%d\" del file \"%s\" fallito - Errore \"%s\"\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
            if((bytes = sendMSG(*fd, &errno, sizeof(int))) <= 0) {
                CLIENT_GOODBYE;
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
        } else {
            if(traceOnLog(log, "[THREAD %d]: \"readFile\" del client fd \"%d\" del file \"%s\" riuscita - Invio del file al client - Invio di \"%d\" B\n", numeroDelThread, *fd, pathname, ((float )dimBuffer)) == -1) {
                CLIENT_GOODBYE;
                free(bufferFile);
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
            errno = 0;
            if((bytes = sendMSG(*fd, &errno, sizeof(int))) <= 0) {
                CLIENT_GOODBYE;
                free(bufferFile);
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
            if((bytesWrite += sendMSG(*fd, bufferFile, dimBuffer)) <= 0) {
                CLIENT_GOODBYE;
                free(bufferFile);
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
            if(traceOnLog(log, "[THREAD %d]: \"readFile\" del client fd \"%d\" del file \"%s\" riuscita - Invio del file al client terminato\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(bufferFile);
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
        }
        if(traceOnLog(log, "[THREAD %d]: \"readFile\" del client fd \"%d\" del file \"%s\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, *fd, pathname, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: \"readFile\" del client fd \"%d\" del file \"%s\" - Letti dalla memoria: \"%d\" B\n", numeroDelThread, *fd, pathname, (dimBuffer)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        free(bufferFile);
        free(pathname);
    }

    /** readNFiles **/
    if(strncmp(request, "readNFiles", (size_t) fmax(11, (double) requestSize)) == 0) {
        /** Variabili blocco **/
        int isSetErrno = 0, index = -1, res = 0;

        /** Ricevo il numero di file che il client vuole leggere **/
        if((bytes = receiveMSG(*fd, (void *) &N, NULL)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: tentativo di \"readNFiles\" del client fd \"%d\" di \"%d\" file\n", numeroDelThread, *fd, *N) == -1) {
            CLIENT_GOODBYE;
            free(N);
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        readFiles = readsRandFiles(cache, *fd, N);
        isSetErrno = errno;
        if((bytes = sendMSG(*fd, (void *) &errno, sizeof(int))) <= 0) {
            CLIENT_GOODBYE;
            index = -1;
            while(readFiles[++index] != NULL) {
                destroyFile(&(readFiles[index]));
            }
            free(N);
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(isSetErrno != 0) {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                CLIENT_GOODBYE;
                index = -1;
                while(readFiles[++index] != NULL) {
                    destroyFile(&(readFiles[index]));
                }
                free(N);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd \"%d\" di \"%d\" file fallita - Errore \"%s\"\n", numeroDelThread, *fd, *N, errorMsg) == -1) {
                CLIENT_GOODBYE;
                index = -1;
                while(readFiles[++index] != NULL) {
                    destroyFile(&(readFiles[index]));
                }
                free(N);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        } else {
            if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd \"%d\" di \"%d\" file - Invio dei file presi dal server\n", numeroDelThread, *fd, *N) == -1) {
                CLIENT_GOODBYE;
                free(N);
                close(*fd);
                free(fd);
                free(request);
                while(readFiles[++index] != NULL) {
                    destroyFile(&(readFiles[index]));
                }
                free(readFiles);
                return (void *) &errno;
            }
            while(++index < *N) {
                res = 1;
                if((bytes = sendMSG(*fd, &res, sizeof(int))) <= 0) {
                   CLIENT_GOODBYE;
                   free(N);
                   close(*fd);
                   free(fd);
                   free(request);
                   while(index >= 0) {
                       destroyFile(&(readFiles[index]));
                       index--;
                   }
                   free(readFiles);
                   errno = ECOMM;
                   return (void *) &errno;
                }
                bytesWrite += bytes;
                if((bytes = sendMSG(*fd, readFiles[index]->pathname, (strnlen(readFiles[index]->pathname, MAX_PATHNAME)+1)*sizeof(char))) <= 0) {
                    CLIENT_GOODBYE;
                    free(N);
                    close(*fd);
                    free(fd);
                    free(request);
                    while(index >= 0) {
                       destroyFile(&(readFiles[index]));
                       index--;
                    }
                    free(readFiles);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                bytesWrite += bytes;
                if((bytes = sendMSG(*fd, readFiles[index]->buffer, readFiles[index]->size)) <= 0) {
                   CLIENT_GOODBYE;
                   free(N);
                   close(*fd);
                   free(fd);
                   free(request);
                   while(index >= 0) {
                       destroyFile(&(readFiles[index]));
                       index--;
                   }
                   free(readFiles);
                   errno = ECOMM;
                   return (void *) &errno;
               }
                bytesWrite += bytes;
                if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd \"%d\" di \"%d\" file - File \"%s\" inviato\n", numeroDelThread, *fd, *N, readFiles[index]->pathname) == -1) {
                    CLIENT_GOODBYE;
                    free(N);
                    close(*fd);
                    free(fd);
                    free(request);
                    while(readFiles[++index] != NULL) {
                        destroyFile(&(readFiles[index]));
                    }
                    free(readFiles);
                    return (void *) &errno;
                }
                fromMem += readFiles[index]->size;
            }
            res = 0;
            if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd \"%d\" - \"%d\" file inviati correttamente\n", numeroDelThread, *fd, *N) == -1) {
                CLIENT_GOODBYE;
                free(N);
                close(*fd);
                free(fd);
                free(request);
                index = -1;
                while(readFiles[++index] != NULL) {
                    destroyFile(&(readFiles[index]));
                }
                free(readFiles);
                return (void *) &errno;
            }
            if((bytes = sendMSG(*fd, &res, sizeof(int))) <= 0) {
                CLIENT_GOODBYE;
                free(N);
                close(*fd);
                free(fd);
                free(request);
                index = -1;
                while(readFiles[++index] != NULL) {
                    destroyFile(&(readFiles[index]));
                }
                free(readFiles);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
            index = -1;
            while(++index < *N) {
                destroyFile(&(readFiles[index]));
            }
            if(readFiles != NULL) free(readFiles), readFiles = NULL;
        }
        if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd \"%d\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, *fd, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd \"%d\" - Letti dalla memoria: \"%d\" B\n", numeroDelThread, *fd, (fromMem)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        free(N);
    }

    /** writeFile **/errno = 0;
    if(strncmp(request, "writeFile", (size_t) fmax(10, (double) requestSize)) == 0) {
        /** Variabili blocco **/
        int isSetErrno = 0, index = -1, res = 0;

        /** Ricevo il pathname e scrivo tutto il file fisico nella memoria cache **/
        if((bytes = receiveMSG(*fd, (void **) &pathname, NULL)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"writeFile\" dal client fd \"%d\" del file \"%s\"\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            return (void *) &errno;
        }
        kickedFiles = addFileOnCache(cache, pathname, *fd, 1), isSetErrno = errno;
        if(kickedFiles != NULL) {
            if(traceOnLog(log, "[THREAD %d]: Troppi file sul server, invio dei file espulsi al client con fd \"%d\"\n", numeroDelThread, *fd) == -1) {
                CLIENT_GOODBYE;
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                while(kickedFiles[++index] != NULL) {
                    destroyFile(&(kickedFiles[index]));
                }
                free(kickedFiles);
                return (void *) &errno;
            }
            res = 1;
            while(kickedFiles[++index] != NULL) {
                if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    while(index >= 0) {
                        destroyFile(&(kickedFiles[index]));
                        index--;
                    }
                    free(kickedFiles);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                bytesWrite += bytes;
                if((bytesWrite = sendMSG(*fd, (void *) kickedFiles[index]->pathname, (strnlen(kickedFiles[index]->pathname, MAX_PATHNAME)+1)*sizeof(char))) <= 0) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    while(index >= 0) {
                        destroyFile(&(kickedFiles[index]));
                        index--;
                    }
                    free(kickedFiles);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                bytesWrite += bytes;
                if((bytesWrite = sendMSG(*fd, (void *) kickedFiles[index]->buffer, kickedFiles[index]->size)) <= 0) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    while(index >= 0) {
                        destroyFile(&(kickedFiles[index]));
                        index--;
                    }
                    free(kickedFiles);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if(traceOnLog(log, "[THREAD %d]: \"writeFile\" dal client fd \"%d\" del file \"%s\" - Invio del file espulso \"%s\"\n", numeroDelThread, *fd, pathname, kickedFiles[index]->pathname) == -1) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    while(kickedFiles[++index] != NULL) {
                        destroyFile(&(kickedFiles[index]));
                    }
                    free(kickedFiles);
                    return (void *) &errno;
                }
                bytesWrite += bytes;
                fromMem += kickedFiles[index]->size;
            }
            index = -1;
            while(kickedFiles[++index] != NULL) {
                destroyFile(&(kickedFiles[index]));
            }
            free(kickedFiles);
            res = 0;
            if(traceOnLog(log, "[THREAD %d]: Espulsione dei file completata\n", numeroDelThread) == -1) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                free(kickedFiles);
                return (void *) &errno;
            }
        }
        if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesWrite += bytes;
        if((bytes = sendMSG(*fd, (void *) &isSetErrno, sizeof(int))) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesWrite += bytes;
        if(isSetErrno == 0) {
            if(traceOnLog(log, "[THREAD %d]: \"writeFile\" dal client fd \"%d\" del file \"%s\" completata\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(request);
                free(fd);
                return (void *) &errno;
            }
        }
        else {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(request);
                free(fd);
                return (void *) &errno;
            }
            if(traceOnLog(log,  "[THREAD %d]: \"writeFile\" dal client fd \"%d\" del file \"%s\" fallita - Errore \"%s\"\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(request);
                free(fd);
                return (void *) &errno;
            }
        }
        if(traceOnLog(log, "[THREAD %d]: \"writeFile\" del client fd \"%d\" del file \"%s\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, *fd, pathname, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: \"writeFile\" del client fd \"%d\" del file \"%s\" - Letti dalla memoria: \"%d\" B\n", numeroDelThread, *fd, pathname, (fromMem)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        free(pathname);
    }

    /** appendToFile **/
    if(strncmp(request, "appendToFile", (size_t) fmax(13, (double) requestSize)) == 0) {
        /** Variabili blocco **/
        size_t dimFile = -1;
        int index = -1, res = 0, isSetErrno = 0;

        /** Leggo pathname e buffer del file da aggiornare **/
        if((bytes = receiveMSG(*fd, (void **) &pathname, NULL)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"appendToFile\" da parte del client con fd \"%d\" sul file \"%s\"\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(request);
            return (void *) &errno;
        }
        if((bytes = receiveMSG(*fd, (void **) &bufferFile, &dimFile)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(request);
            return (void *) &errno;
        }
        bytesRead += bytes;
        traceOnLog(log, "SCOPRIAMOLO %d %d\n", bufferFile == NULL, dimFile <= 0);
        kickedFiles = appendFile(cache, pathname, *fd, bufferFile, dimFile), isSetErrno = errno;
        strerror_r(errno, errorMsg, MAX_BUFFER_LEN);
        traceOnLog(log, "1Vediamo %s\n", errorMsg);
        if(kickedFiles != NULL) {
            res = 1, index = -1;
            if(traceOnLog(log, "[THREAD %d]: Espulsione dei file per sforamento di capacità di memoria\n", numeroDelThread) == -1) {
                CLIENT_GOODBYE;
                close(*fd);
                free(fd);
                free(bufferFile);
                free(request);
                free(pathname);
                while(kickedFiles[++index] != NULL) {
                    destroyFile(&(kickedFiles[index]));
                }
                free(kickedFiles);
                return (void *) &errno;
            }
            strerror_r(errno, errorMsg, MAX_BUFFER_LEN);
            traceOnLog(log, "[THREAD %d]: BBBBBBBBBBBBBBBBBBBB   %s\n",numeroDelThread, errorMsg);
            while(kickedFiles[++index] != NULL) {
                if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(request);
                    free(bufferFile);
                    free(pathname);
                    while(index >= 0) {
                        destroyFile(&(kickedFiles[index]));
                        index--;
                    }
                    free(kickedFiles);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                bytesWrite += bytes;
                strerror_r(errno, errorMsg, MAX_BUFFER_LEN);
                traceOnLog(log, "[THREAD %d]: BBBBBBBBBBBBBBBBBBBB   %s\n", numeroDelThread, errorMsg);
                if((bytes = sendMSG(*fd, (void *) kickedFiles[index]->pathname, (strnlen(kickedFiles[index]->pathname, MAX_PATHNAME)+1)*sizeof(char))) <= 0) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(request);
                    free(bufferFile);
                    free(pathname);
                    while(index >= 0) {
                        destroyFile(&(kickedFiles[index]));
                        index--;
                    }
                    free(kickedFiles);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                fromMem += bytes;
                strerror_r(errno, errorMsg, MAX_BUFFER_LEN);
                traceOnLog(log, "[THREAD %d]: BBBBBBBBBBBBBBBBBBBB %d ----  %s\n", numeroDelThread, kickedFiles[index]->size, errorMsg);
                if((bytes = sendMSG(*fd, (void *) kickedFiles[index]->buffer, kickedFiles[index]->size)) <= 0) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(request);
                    free(bufferFile);
                    free(pathname);
                    while(index >= 0) {
                        destroyFile(&(kickedFiles[index]));
                        index--;
                    }
                    free(kickedFiles);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if(traceOnLog(log, "[THREAD %d]: \"appendToFile\" da parte del client con fd \"%d\" sul file \"%s\" - File \"%s\" espulso\n", numeroDelThread, *fd, pathname, kickedFiles[index]->pathname) == -1) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(request);
                    free(bufferFile);
                    free(pathname);
                    while(kickedFiles[++index] != NULL) {
                        destroyFile(&(kickedFiles[index]));
                    }
                    free(kickedFiles);
                    return (void *) &errno;
                }
                strerror_r(errno, errorMsg, MAX_BUFFER_LEN);
                traceOnLog(log, "[THREAD %d]: AAAAAAAAAAAAAAAAA   %s\n", numeroDelThread, errorMsg);
                fromMem += kickedFiles[index]->size;
                bytesWrite += bytes;
            }
            index = -1;
            while(kickedFiles[++index] != NULL) {
                destroyFile(&(kickedFiles[index]));
            }
            free(kickedFiles);
            if(traceOnLog(log, "[THREAD %d]: Espulsione dei file completata\n", numeroDelThread) == -1) {
                CLIENT_GOODBYE;
                close(*fd);
                free(fd);
                free(request);
                free(bufferFile);
                free(pathname);
                while(kickedFiles[++index] != NULL) {
                    destroyFile(&(kickedFiles[index]));
                }
                free(kickedFiles);
                return (void *) &errno;
            }
            res = 0;
        }
        strerror_r(errno, errorMsg, MAX_BUFFER_LEN);
        traceOnLog(log, "2Vediamo %s\n", errorMsg);
        if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(bufferFile);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesWrite += bytes;
        if((bytesWrite = sendMSG(*fd, (void *) &isSetErrno, sizeof(int))) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(bufferFile);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        strerror_r(errno, errorMsg, MAX_BUFFER_LEN);

        traceOnLog(log, "3Vediamo %s\n", errorMsg);
        bytesWrite += bytes;
        if(traceOnLog(log, "[THREAD %d]: \"appendToFile\" del client fd \"%d\" del file \"%s\" completata\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(bufferFile);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: \"appendToFile\" del client fd \"%d\" del file \"%s\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, *fd, pathname, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(bufferFile);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: \"appendToFile\" del client fd \"%d\" del file \"%s\" - Letti dalla memoria: \"%d\" B - Scritti nella memoria \"%d\" B\n", numeroDelThread, *fd, pathname, (fromMem), (dimFile)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(bufferFile);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        kickedFiles = NULL;
        free(pathname);
        free(bufferFile), bufferFile = NULL;
    }

    /** lockFile **/
    if(strncmp(request, "lockFile", (size_t) fmax(9, (double) requestSize)) == 0) {
        /** Variabili blocco **/
        int res = -1;

        /** Ricevo il pathname del file e provo ad effettuare la lock **/
        if((bytes = receiveMSG(*fd, (void **) &pathname, NULL)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"lockFile\" da parte del client con fd \"%d\" sul file \"%s\"\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        if(((res = lockFileOnCache(cache, pathname, *fd)) == -1) || (res == 0) || (res == *fd)) {
            if(res == -1) {
                if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
                if(traceOnLog(log, "[THREAD %d]: \"lockFile\" da parte del client con fd \"%d\" sul file \"%s\" fallito - Errore \"%s\"\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
                res = errno;
            } else if(res == *fd) {
                res = EALREADY;
                if(traceOnLog(log, "[THREAD %d]: \"lockFile\" da parte del client con fd \"%d\" sul file \"%s\" già eseguita\n", numeroDelThread, *fd, pathname) == -1) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
            } else {
                if(traceOnLog(log, "[THREAD %d]: \"lockFile\" da parte del client con fd \"%d\" sul file \"%s\" riuscita\n", numeroDelThread, *fd, pathname) == -1) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
            }
            if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
        } else {
            if(traceOnLog(log, "[THREAD %d]: \"lockFile\" da parte del client con fd \"%d\" sul file \"%s\" - File già occuppato\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        }
        if(traceOnLog(log, "[THREAD %d]: \"lockFile\" del client fd \"%d\" sul file \"%s\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, *fd, pathname, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        free(pathname);
    }

    /** unlockFile **/
    if(strncmp(request, "unlockFile", (size_t) fmax(11, (double) requestSize)) == 0) {
        /** Variabili blocco **/
        int res = -1, wakeUp = -1;

        /** Effettuo la unlock **/
        if((bytes = receiveMSG(*fd, (void **) &pathname, NULL)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"unlockFile\" da parte del client con fd \"%d\" sul file \"%s\"\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        res = unlockFileOnCache(cache, pathname, *fd);
        if(res == -1) {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" da parte del client con fd \"%d\" sul file \"%s\" fallito - Errore \"%s\"\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            res = errno;
            if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
        } else if(res == 0) {
            if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" da parte del client con fd \"%d\" sul file \"%s\" riuscito\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
        } else {
            wakeUp = res;
            res = 0;
            if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" da parte del client con fd \"%d\" sul file \"%s\" riuscito\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" da parte del client con fd \"%d\" sul file \"%s\" - Concedo la lock al client con fd \"%d\"\n", numeroDelThread, *fd, pathname, wakeUp) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if((bytes = sendMSG(*fd, (void *) &res, sizeof(int))) <= 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
            if((bytes = sendMSG(wakeUp, (void *) &res, sizeof(int))) <= 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
            bytesWrite += bytes;
        }
        if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" del client fd \"%d\" sul file \"%s\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, *fd, pathname, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        free(pathname);
    }

    /** closeFile **/
    if(strncmp(request, "closeFile", (size_t) fmax(10, (double) requestSize)) == 0) {
        /** Variabili blocco **/
        int res = -1, wakeUp = 0;

        /** Effettuo la unlock **/
        if((bytes = receiveMSG(*fd, (void **) &pathname, NULL)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"closeFile\" da parte del client con fd \"%d\" sul file \"%s\"\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        res = closeFileOnCache(cache, pathname, *fd);
        if(res == -1) {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"closeFile\" da parte del client con fd \"%d\" sul file \"%s\" fallita - Errore \"%s\"\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        } else {
            if(traceOnLog(log, "[THREAD %d]: \"closeFile\" da parte del client con fd \"%d\" sul file \"%s\" riuscita\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(res > 0) {
                wakeUp = res, res = 0;
                if(traceOnLog(log, "[THREAD %d]: \"closeFile\" da parte del client con fd \"%d\" sul file \"%s\" - Concedo la lock al client con fd \"%d\"\n", numeroDelThread, *fd, pathname) == -1) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
                if((bytes = sendMSG(wakeUp, (void *) &res, sizeof(int))) <= 0) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
                bytesWrite += bytes;
            }
        }
        if((bytes = sendMSG(*fd, (void *) &errno, sizeof(int))) <= 0) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        bytesWrite += bytes;
        if(traceOnLog(log, "[THREAD %d]: \"closeFile\" da parte del client con fd \"%d\" sul file \"%s\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, *fd, pathname, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        free(pathname);
    }

    /** removeFile **/
    if(strncmp(request, "removeFile", (size_t) fmax(11, (double) requestSize)) == 0) {
        /** Variabili **/

        /** Ricevo il pathname dal client **/
        if((bytes = receiveMSG(*fd, (void **) &pathname, NULL)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"removeFile\" da parte del client con fd \"%d\" sul file \"%s\"\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        resCancellazione = removeFileOnCache(cache, pathname, *fd);
        if((bytes = sendMSG(*fd, (void *) &errno, sizeof(int))) <= 0) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesWrite += bytes;
        if(errno == 0) {
            if(traceOnLog(log, "[THREAD %d]: \"removeFile\" da parte del client con fd \"%d\" sul file \"%s\" eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            int *fdUn = NULL;
            int result = ENOENT;
            if(traceOnLog(log, "[THREAD %d]: \"removeFile\" da parte del client con fd \"%d\" sul file \"%s\" - Avverto eventuali file in lock su di lui che è stato rimosso\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            while(resCancellazione != NULL && resCancellazione->utentiLocked != NULL) {
                fdUn = deleteFirstElement(&(resCancellazione->utentiLocked));
                if(fdUn != NULL) {
                    bytes = sendMSG(*fdUn, &result, sizeof(int));
                    if(bytes != -1) bytesWrite += bytes;
                    free(fdUn);
                }
            }
        } else {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"removeFile\" da parte del client con fd \"%d\" sul file \"%s\" fallita - Errore \"%s\"\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        }
        if(traceOnLog(log, "[THREAD %d]: \"removeFile\" del client fd \"%d\" sul file \"%s\" - Ricevuti: \"%d\" B e Mandati: \"%d\" B\n", numeroDelThread, *fd, pathname, (bytesRead), (bytesWrite)) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        destroyFile(&resCancellazione);
        free(pathname);
    }

    /** Riabilito fd in lettura nel server **/
    if(((bytes = write(pipe, (void *) fd, sizeof(int))) <= 0)) {
        CLIENT_GOODBYE;
        close(*fd);
        free(fd);
        free(request);
        return (void *) &errno;
    }
    free(fd);
    free(request);
    if(traceOnLog(log, "[THREAD %d]: Spediti sulla pipe \"%d\" B\n", numeroDelThread, bytes) == -1) {
        CLIENT_GOODBYE;
        close(*fd);
        free(fd);
        free(pathname);
        free(flags);
        free(request);
        return (void *) &errno;
    }


    errno = 0;
    return (void *) 0;
}
