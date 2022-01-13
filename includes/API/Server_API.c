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

#define LOG_PRINT_FLAGS \
    (*flags == 0) ? "0" : ((*flags == (O_CREATE | O_LOCK)) ? "O_CREATE | O_LOCK" : "O_CREATE")


/**
 * @brief                       Accoglie le richieste del client
 * @fun                         ServerTaskds
 * @param numeroDelThread       Numero del thread che esegue la task
 * @param argv                  Argomenti che passo al thread
 * @return                      (NULL) in caso di successo; altrimenti riporto un messaggio di errore
 */
void* ServerTasks(unsigned int numeroDelThread, void *argv) {
    /** Variabili **/
    int pipe = -1, *fd = NULL, *flags = NULL, *N = NULL;
    char *request = NULL, *pathname = NULL, errorMsg[MAX_BUFFER_LEN];
    void *bufferFile = NULL;
    myFile **kickedFiles = NULL, **readFiles = NULL;
    myFile *resCancellazione = NULL;
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
    if(traceOnLog(log, "[THREAD %d]: Ascolto la richiesta del client\n", numeroDelThread) == -1) {
        close(*fd);
        free(fd);
        return (void *) &errno;
    }
    if(receiveMSG(*fd, (void *) &request, &requestSize) <= 0) {
        errno = ECOMM;
        close(*fd);
        free(fd);
        return (void *) &errno;
    }
    if(traceOnLog(log, "[THREAD %d]: Richiesta di \"%s\" da parte del client con fd:%d\n", numeroDelThread, request, *fd) == -1) {
        close(*fd);
        free(fd);
        free(request);
        return (void *) &errno;
    }

    /** openFile **/
    if(strncmp(request, "openFile", (size_t) fmax(9, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        int res = -1;
        size_t path_len = -1;

        /** Tentativo di openFile **/
        if(receiveMSG(*fd, (void **) &pathname, &path_len) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(receiveMSG(*fd, (void **) &flags, NULL) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: Tentativo da parte del client di fd:%d di \"openFile\" del file %s\n", numeroDelThread, *fd, pathname) == -1) {
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
                if((res == 0) && (traceOnLog(log, "[THREAD %d]: \"openFile\" su %s del client con fd:%d con flags:%s riuscita\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS) == -1)) {
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if((res == 1) && (traceOnLog(log, "[THREAD %d]: \"openFile\" su %s del client con fd:%d con flags:%s sospesa - Il client aveva già aperto il file\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS) == -1)) {
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if(res == -1) {
                    if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                        close(*fd);
                        free(fd);
                        free(pathname);
                        free(flags);
                        free(request);
                        return (void *) &errno;
                    }
                    if(traceOnLog(log, "[THREAD %d]: \"openFile\" su %s del client con fd:%d con flags:%s non riuscita - Errore: %s\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS, errorMsg) == -1) {
                        close(*fd);
                        free(fd);
                        free(pathname);
                        free(flags);
                        free(request);
                        return (void *) &errno;
                    }
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

                if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                    close(*fd);
                    free(fd);
                    free(pathname);
                    free(flags);
                    free(request);
                    return (void *) &errno;
                }
                if((res == -1) && (traceOnLog(log, "[THREAD %d]: \"openFile\" su %s del client con fd:%d con flags:%s non riuscita - Errore: %s\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS, errorMsg) == -1)) {
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if((res == 0) && (traceOnLog(log, "[THREAD %d]: \"openFile\" su %s del client con fd:%d con flags:%s riuscita\n", numeroDelThread, pathname, *fd, LOG_PRINT_FLAGS) == -1)) {
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
                if(traceOnLog(log, "[THREAD %d]: \"openFile\" su %s del client con fd:%d - Errore nell'identificazione dei flags", numeroDelThread, pathname, *fd) == -1) {
                    close(*fd);
                    free(fd);
                    free(pathname);
                    free(flags);
                    free(request);
                    return (void *) &errno;
                }
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
        size_t dimBuffer = -1;

        if(receiveMSG(*fd, (void **) &pathname, NULL) <= 0) {
            free(request);
            close(*fd);
            free(fd);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: tentativo di \"readFile\" del client fd:%d del file:%s\n", numeroDelThread, *fd, pathname) == -1) {
            free(request);
            free(pathname);
            close(*fd);
            free(fd);
            return (void *) &errno;
        }
        if((dimBuffer = readFileOnCache(cache, pathname, &bufferFile)) == -1) {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"readFile\" del client fd:%d del file:%s fallito - Errore%s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
            if(sendMSG(*fd, &errno, sizeof(int)) <= 0) {
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
        } else {
            if(traceOnLog(log, "[THREAD %d]: \"readFile\" del client fd:%d del file:%s riuscita - Invio del file al client in corso...\n", numeroDelThread, *fd, pathname) == -1) {
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
            errno = 0;
            if(sendMSG(*fd, &errno, sizeof(int)) <= 0) {
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
            if(sendMSG(*fd, bufferFile, dimBuffer) <= 0) {
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"readFile\" del client fd:%d del file:%s riuscita - Invio del file al client terminato\n", numeroDelThread, *fd, pathname) == -1) {
                free(bufferFile);
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
        }
    }

    /** readNFiles **/
    if(strncmp(request, "readNFiles", (size_t) fmax(11, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        int isSetErrno = 0, index = -1, res = 0;

        /** Ricevo il numero di file che il client vuole leggere **/
        if(receiveMSG(*fd, (void *) &N, NULL) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: tentativo di \"readNFiles\" del client fd:%d di %d file\n", numeroDelThread, *fd, *N) == -1) {
            free(N);
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        readFiles = readsRandFiles(cache, *N);
        isSetErrno = errno;
        if(sendMSG(*fd, (void *) &errno, sizeof(int)) <= 0) {
            free(N);
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(isSetErrno != 0) {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                free(N);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd:%d di %d file fallita - Errore: %s\n", numeroDelThread, *fd, *N, errorMsg) == -1) {
                free(N);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        } else {
            if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd:%d di %d file - Invio dei file presi dal server\n", numeroDelThread, *fd, *N) == -1) {
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
            while((readFiles != NULL) && (readFiles[++index] != NULL)) {
               res = 1;
               if(sendMSG(*fd, &res, sizeof(int)) <= 0) {
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
               if(sendMSG(*fd, readFiles[index]->pathname, (strnlen(readFiles[index]->pathname, MAX_PATHNAME)+1)*sizeof(char)) <= 0) {
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
               if(sendMSG(*fd, readFiles[index]->buffer, readFiles[index]->size) <= 0) {
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
            }
            res = 0;
            if(traceOnLog(log, "[THREAD %d]: \"readNFiles\" del client fd:%d di %d file - %d file inviati correttamente\n", numeroDelThread, *fd, index-1) == -1) {
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
            if(sendMSG(*fd, &res, sizeof(int)) <= 0) {
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
        }
    }

    /** writeFile **/
    if(strncmp(request, "writeFile", (size_t) fmax(10, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        size_t dimFile = -1;
        int isSetErrno = 0, index = -1, res = 0;

        /** Ricevo il pathname e scrivo tutto il file fisico nella memoria cache **/
        if(receiveMSG(*fd, (void **) &pathname, NULL) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;free(request);
        }
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"writeFile\" dal client fd:%d del file %s\n", numeroDelThread, *fd, pathname) == -1) {
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            return (void *) &errno;
        }
        kickedFiles = addFileOnCache(cache, pI, pathname, *fd, 1), isSetErrno = errno;
        if(kickedFiles != NULL) {
            if(traceOnLog(log, "[THREAD %d]: Troppi file sul server, invio dei file espulsi al client con fd:%d\n", numeroDelThread, *fd) == -1) {
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
                if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
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
                if(sendMSG(*fd, (void *) kickedFiles[index]->pathname, (strnlen(kickedFiles[index]->pathname, MAX_PATHNAME)+1)*sizeof(char)) <= 0) {
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
                if(sendMSG(*fd, (void *) kickedFiles[index]->buffer, kickedFiles[index]->size) <= 0) {
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
        if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(sendMSG(*fd, (void *) &isSetErrno, sizeof(int)) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(isSetErrno == 0) {
            if(traceOnLog(log, "[THREAD %d]: \"writeFile\" dal client fd:%d del file %s - File vuoto aggiunto al server correttamente\n", numeroDelThread, *fd, pathname) == -1) {
                close(*fd);
                free(request);
                free(fd);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"writeFile\" dal client fd:%d del file %s - Scrivo il suo contenuto nel server\n", numeroDelThread, *fd, pathname) == -1) {
                close(*fd);
                free(request);
                free(fd);
                return (void *) &errno;
            }
            if(receiveMSG(*fd, (void **) &bufferFile, &dimFile) <= 0) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                errno = ECOMM;
                return (void *) &errno;free(request);
            }
            kickedFiles = appendFile(cache, pathname, bufferFile, dimFile), isSetErrno = errno;
            if(kickedFiles != NULL) {
                res = 1, index = -1;
                if(traceOnLog(log, "[THREAD %d]: Espulsione dei file per sforamento di capacità di memoria\n", numeroDelThread) == -1) {
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    while(kickedFiles[++index] != NULL) {
                        destroyFile(&(kickedFiles[index]));
                    }
                    return (void *) &errno;
                }
                while(kickedFiles[++index] != NULL) {
                    if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
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
                    if(sendMSG(*fd, (void *) kickedFiles[index]->pathname, (strnlen(kickedFiles[index]->pathname, MAX_PATHNAME)+1)*sizeof(char)) <= 0) {
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
                    if(sendMSG(*fd, (void *) kickedFiles[index]->buffer, kickedFiles[index]->size) <= 0) {
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
                }
                index = -1;
                while(kickedFiles[++index] != NULL) {
                    destroyFile(&(kickedFiles[index]));
                }
                free(kickedFiles);
                res = 0;
            }
            if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                errno = ECOMM;
                return (void *) &errno;
            }
            if(sendMSG(*fd, (void *) &isSetErrno, sizeof(int)) <= 0) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                errno = ECOMM;
                return (void *) &errno;
            }
            if(isSetErrno != 0) {
                if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    return (void *) &errno;
                }
                if(traceOnLog(log,  "[THREAD %d]: \"writeFile\" dal client fd:%d del file %s fallita - Errore: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    return (void *) &errno;
                }
            } else {
                if(traceOnLog(log, "[THREAD %d]: \"writeFile\" dal client fd:%d del file %s - Contenuto aggiunto\n", numeroDelThread, pathname) == -1) {
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    return (void *) &errno;
                }
                if(traceOnLog(log, "[THREAD %d]: \"writeFile\" dal client fd:%d del file %s terminata con successo\n", numeroDelThread, pathname) == -1) {
                    close(*fd);
                    free(fd);
                    free(request);
                    free(pathname);
                    return (void *) &errno;
                }
            }
        }
        else {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                return (void *) &errno;
            }
            if(traceOnLog(log,  "[THREAD %d]: \"writeFile\" dal client fd:%d del file %s fallita - Errore: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                return (void *) &errno;
            }
        }


        free(pathname);
        free(kickedFiles);
    }

    /** appendToFile **/
    if(strncmp(request, "appendToFile", (size_t) fmax(13, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        size_t dimFile = -1;
        int index = -1, res = 0, isSetErrno = 0;

        /** Leggo pathname e buffer del file da aggiornare **/
        if(receiveMSG(*fd, (void **) &pathname, NULL) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"appendToFile\" da parte del client con fd:%d sul file:%s\n", numeroDelThread, *fd, pathname) == -1) {
            close(*fd);
            free(fd);
            free(pathname);
            free(request);
            return (void *) &errno;
        }
        if(receiveMSG(*fd, (void **) &bufferFile, &dimFile) <= 0) {
            close(*fd);
            free(fd);
            free(pathname);
            free(request);
            return (void *) &errno;
        }
        kickedFiles = appendFile(cache, pathname, bufferFile, dimFile), isSetErrno = errno;
        if(kickedFiles != NULL) {
            res = 1, index = -1;
            if(traceOnLog(log, "[THREAD %d]: Espulsione dei file per sforamento di capacità di memoria\n", numeroDelThread) == -1) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                while(kickedFiles[++index] != NULL) {
                    destroyFile(&(kickedFiles[index]));
                }
                return (void *) &errno;
            }
            while(kickedFiles[++index] != NULL) {
                if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
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
                if(sendMSG(*fd, (void *) kickedFiles[index]->pathname, (strnlen(kickedFiles[index]->pathname, MAX_PATHNAME)+1)*sizeof(char)) <= 0) {
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
                if(sendMSG(*fd, (void *) kickedFiles[index]->buffer, kickedFiles[index]->size) <= 0) {
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
            }
            index = -1;
            while(kickedFiles[++index] != NULL) {
                destroyFile(&(kickedFiles[index]));
            }
            free(kickedFiles);
            res = 0;
        }
        if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(sendMSG(*fd, (void *) &isSetErrno, sizeof(int)) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
    }

    /** lockFile **/
    if(strncmp(request, "lockFile", (size_t) fmax(9, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        int res = -1;

        /** Ricevo il pathname del file e provo ad effettuare la lock **/
        if(receiveMSG(*fd, (void **) &pathname, NULL) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"lockFile\" da parte del client con fd:%d sul file:%s\n", numeroDelThread, *fd, pathname) == -1) {
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        if(((res = lockFileOnCache(cache, pathname, *fd)) == -1) || (res == 0)) {
            if(res == -1) {
                if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
                if(traceOnLog(log, "[THREAD %d]: \"lockFile\" da parte del client con fd:%d sul file:%s fallito - Errore: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
                res = errno;
            } else {
                if(traceOnLog(log, "[THREAD %d]: \"lockFile\" da parte del client con fd:%d sul file:%s riuscito\n", numeroDelThread, *fd, pathname) == -1) {
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
            }
            if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                errno = ECOMM;
                return (void *) &errno;
            }
        } else {
            if(traceOnLog(log, "[THREAD %d]: \"lockFile\" da parte del client con fd:%d sul file:%s - File già occuppato\n", numeroDelThread, *fd, pathname) == -1) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        }
    }

    /** unlockFile **/
    if(strncmp(request, "unlockFile", (size_t) fmax(11, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        int res = -1, wakeUp = -1;

        /** Effettuo la unlock **/
        if(receiveMSG(*fd, (void **) &pathname, NULL) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"unlockFile\" da parte del client con fd:%d sul file:%s\n", numeroDelThread, *fd, pathname) == -1) {
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        res = unlockFileOnCache(cache, pathname, *fd);
        if(res == -1) {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" da parte del client con fd:%d sul file:%s fallito - Errore: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            res = errno;
        } else if(res == 0) {
            if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" da parte del client con fd:%d sul file:%s riuscito\n", numeroDelThread, *fd, pathname) == -1) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                errno = ECOMM;
                return (void *) &errno;
            }
        } else {
            wakeUp = res;
            res = 0;
            if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" da parte del client con fd:%d sul file:%s riuscito\n", numeroDelThread, *fd, pathname) == -1) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"unlockFile\" da parte del client con fd:%d sul file:%s - Concedo la lock al client con fd:%d\n", numeroDelThread, *fd, pathname, wakeUp) == -1) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(sendMSG(*fd, (void *) &res, sizeof(int)) <= 0) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                errno = ECOMM;
                return (void *) &errno;
            }
            if(sendMSG(wakeUp, (void *) &res, sizeof(int)) <= 0) {
                close(*fd);
                free(fd);
                free(request);
                free(pathname);
                errno = ECOMM;
                return (void *) &errno;
            }
        }
    }

    /** closeFile **/
    if(strncmp(request, "closeFile", (size_t) fmax(10, (float) requestSize)) == 0) {
        /** Variabili blocco **/
        int res = -1, wakeUp = 0;

        /** Effettuo la unlock **/
        if(receiveMSG(*fd, (void **) &pathname, NULL) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: Tentativo di \"closeFile\" da parte del client con fd:%d sul file:%s\n", numeroDelThread, *fd, pathname) == -1) {
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        res = closeFileOnCache(cache, pathname, *fd);
        if(res == -1) {
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: \"closeFile\" da parte del client con fd:%d sul file:%s fallita - Errore: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        } else {
            if(traceOnLog(log, "[THREAD %d]: \"closeFile\" da parte del client con fd:%d sul file:%s riuscita\n", numeroDelThread, *fd, pathname) == -1) {
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(res > 0) {
                wakeUp = res, res = 0;
                if(traceOnLog(log, "[THREAD %d]: \"closeFile\" da parte del client con fd:%d sul file:%s - Concedo la lock al client con fd:%d\n", numeroDelThread, *fd, pathname) == -1) {
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
                if(sendMSG(wakeUp, (void *) &res, sizeof(int)) <= 0) {
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
            }
        }
        if(sendMSG(*fd, (void *) &errno, sizeof(int)) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
    }

    /** removeFile **/
    if(strncmp(request, "removeFile", (size_t) fmax(11, (float) requestSize)) == 0) {
        /** Ricevo il pathname dal client **/
        if(receiveMSG(*fd, (void **) &pathname, NULL) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        resCancellazione = removeFileOnCache(cache, pathname);
        destroyFile(&resCancellazione);
        if(sendMSG(*fd, (void *) &errno, sizeof(int)) <= 0) {
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
    }

    /** Riabilito fd in lettura nel server **/
    if((write(pipe, (void *) fd, sizeof(int)) <= 0) && (errno != EPIPE)) {
        close(*fd);
        free(fd);
        free(request);
        return (void *) -1;
    }
    free(fd);
    free(request);

    return (void *) 0;
}
