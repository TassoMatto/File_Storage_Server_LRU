/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione richieste del client
 * @author              Simone Tassotti
 * @date                03/01/2022
 * @finish              26/01/2022
 */


#include "Server_API.h"


/**
 * @brief       Chiusura connessione con il client
 * @macro       CLIENT_GOODBYE
 */
#define CLIENT_GOODBYE                                              \
    do {                                                            \
        size_t b, tot = 0;                                          \
        errno=0;                                                    \
        logoutClient(cache);                                        \
        locks = deleteClientFromCache(cache, *fd);                  \
                                                                    \
        if(errno == 0 && locks != NULL) {                           \
           int i = -1;                                              \
           while(locks[++i] != -1) {                                \
               b = sendMSG(locks[i], (void *) &errno, sizeof(int)); \
               if(b != -1) tot += b;                                \
           }                                                        \
           free(locks);                                             \
        }                                                           \
    } while(0)


/**
 * @brief       Macro di supporto output sul file di log
 * @macro       LOG_PRINT_FLAGS
 */
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
    unsigned int bytesRead = 0, bytesWrite = 0, fromMem = 0;
    ssize_t bytes = -1;
    Task_Package *tp = NULL;
    serverLogFile *log = NULL;
    LRU_Memory *cache = NULL;

    /** Controllo parametri **/
    errno = 0;
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
    if((bytes += receiveMSG(*fd, (void *) &request, &requestSize)) <= 0) {
        CLIENT_GOODBYE;
        errno = ECOMM;
        close(*fd);
        free(fd);
        return (void *) &errno;
    }
    if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
        CLIENT_GOODBYE;
        errno = ECOMM;
        close(*fd);
        free(fd);
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: openFile - FILE: %s\n", numeroDelThread, *fd, pathname) == -1) {
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
                if((res == 0) && (traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: openFile - FILE: %s - MODALITA': %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname, LOG_PRINT_FLAGS) == -1)) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if((res == 1) && (traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: openFile - FILE: %s - MODALITA': %s - ESITO: già eseguita\n", numeroDelThread, *fd, pathname, LOG_PRINT_FLAGS) == -1)) {
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
                    if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: openFile - FILE: %s - MODALITA': %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
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
                if((res == -1) && (traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: openFile - FILE: %s - MODALITA': %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1)) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
                    free(flags);
                    free(request);
                    free(pathname);
                    errno = ECOMM;
                    return (void *) &errno;
                }
                if((res == 0) && (traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: openFile - FILE: %s - MODALITA': %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname, LOG_PRINT_FLAGS) == -1)) {
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
                    if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                        CLIENT_GOODBYE;
                        close(*fd);
                        free(fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
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
                    if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                        CLIENT_GOODBYE;
                        close(*fd);
                        free(fd);
                        free(flags);
                        free(request);
                        free(pathname);
                        errno = ECOMM;
                        return (void *) &errno;
                    }
                }
            break;

            default:
                if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: openFile - FILE: %s - MODALITA': Modalità non riconosciuta\"", numeroDelThread, *fd, pathname) == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                    CLIENT_GOODBYE;
                    close(*fd);
                    free(fd);
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            free(request);
            close(*fd);
            free(fd);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: readFile - FILE: %s\n", numeroDelThread, *fd, pathname) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: openFile - FILE: %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                CLIENT_GOODBYE;
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
        } else {
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                CLIENT_GOODBYE;
                free(bufferFile);
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                CLIENT_GOODBYE;
                free(bufferFile);
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                errno = ECOMM;
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: readFile - FILE: %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(bufferFile);
                free(request);
                free(pathname);
                close(*fd);
                free(fd);
                return (void *) &errno;
            }
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT %d - LETTI: %ldB\n", numeroDelThread, *fd, dimBuffer) == -1) {
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
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: readNFiles\n", numeroDelThread, *fd) == -1) {
            CLIENT_GOODBYE;
            free(request);
            close(*fd);
            free(fd);
            return (void *) &errno;
        }
        if((bytes = receiveMSG(*fd, (void *) &N, NULL)) <= 0) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return &errno;
        }
        bytesRead += bytes;
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return &errno;
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
        if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
        if(isSetErrno != 0) {
            if(strerror_r(isSetErrno, errorMsg, MAX_BUFFER_LEN) != 0) {
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
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: readNFiles - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, errorMsg) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: readNFiles - ESITO: file letti, invio al client\n", numeroDelThread, *fd) == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: readNFiles - ESITO: file inviato - FILE-READ: %s\n", numeroDelThread, *fd, readFiles[index]->pathname) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
            index = -1;
            while(++index < *N) {
                destroyFile(&(readFiles[index]));
            }
            if(readFiles != NULL) free(readFiles), readFiles = NULL;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: readNFiles - ESITO: eseguita correttamente\n", numeroDelThread, *fd) == -1) {
            CLIENT_GOODBYE;
            free(N);
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT %d - LETTI: %ldB\n", numeroDelThread, *fd, fromMem) == -1) {
            CLIENT_GOODBYE;
            free(N);
            close(*fd);
            free(fd);
            free(pathname);
            free(flags);
            free(request);
            return (void *) &errno;
        }

        free(N);
    }

    /** writeFile **/
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: writeFile - FILE: %s\n", numeroDelThread, *fd, pathname) == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            return (void *) &errno;
        }
        kickedFiles = addFileOnCache(cache, pathname, *fd, 1), isSetErrno = errno;
        if(kickedFiles != NULL) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: writeFile - FILE: %s - ESITO: espulsione file - KICK-FILE: %s\n", numeroDelThread, *fd, pathname, kickedFiles[index]->pathname) == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                fromMem += kickedFiles[index]->size;
            }
            index = -1;
            while(kickedFiles[++index] != NULL) {
                destroyFile(&(kickedFiles[index]));
            }
            free(kickedFiles);
            res = 0;
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
        if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
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
        if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(isSetErrno == 0) {
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: writeFile - FILE: %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: writeFile - FILE: %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(request);
                free(fd);
                return (void *) &errno;
            }
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT %d - RIMOSSI: %ldB\n", numeroDelThread, *fd, fromMem) == -1) {
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: appendToFile - FILE: %s\n", numeroDelThread, *fd, pathname) == -1) {
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }
        kickedFiles = appendFile(cache, pathname, *fd, bufferFile, dimFile), isSetErrno = errno;
        if(kickedFiles != NULL) {
            res = 1, index = -1;
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: appendToFile - FILE: %s - ESITO: espulsione file - KICK-FILE: %s\n", numeroDelThread, *fd, pathname, kickedFiles[index]->pathname) == -1) {
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
                fromMem += (unsigned int) (kickedFiles[index]->size);
                bytesWrite += bytes;
            }
            index = -1;
            while(kickedFiles[++index] != NULL) {
                destroyFile(&(kickedFiles[index]));
            }
            free(kickedFiles);
            res = 0;
        }
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
        if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(bufferFile);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
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
        bytesWrite += bytes;
        if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(bufferFile);
            free(request);
            free(pathname);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(isSetErrno != 0) {
            errno = isSetErrno;
            if(strerror_r(errno, errorMsg, MAX_BUFFER_LEN) != 0) {
                CLIENT_GOODBYE;
                close(*fd);
                free(bufferFile);
                free(fd);
                free(pathname);
                free(flags);
                free(request);
                return (void *) &errno;
            }
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: appendToFile - FILE: %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                close(*fd);
                free(bufferFile);
                free(fd);
                free(pathname);
                free(flags);
                free(request);
                return (void *) &errno;
            }
            dimFile=0;
        } else {
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: appendToFile - FILE: %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                close(*fd);
                free(bufferFile);
                free(fd);
                free(pathname);
                free(flags);
                free(request);
                return (void *) &errno;
            }
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT %d - RIMOSSI: %ldB - SCRITTI: %ldB\n", numeroDelThread, *fd, fromMem, dimFile) == -1) {
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: lockFile - FILE: %s\n", numeroDelThread, *fd, pathname) == -1) {
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
                if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: lockFile - FILE: %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
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
                if(traceOnLog(log,  "[THREAD %d]: CLIENT: %d - RICHIESTA: lockFile - FILE: %s - ESITO: Già eseguita\n", numeroDelThread, *fd, pathname) == -1) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
            } else {
                if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: lockFile - FILE: %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
        } else {
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: lockFile - FILE: %s - ESITO: file occupato\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: unlockFile - FILE: %s\n", numeroDelThread, *fd, pathname) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: unlockFile - FILE: %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
        } else if(res == 0) {
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: unlockFile - FILE: %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
        } else {
            wakeUp = res;
            res = 0;
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: unlockFile - FILE: %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
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
            if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                errno = ECOMM;
                return (void *) &errno;
            }
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
        if(traceOnLog(log, "[THREAD %d]: Ricevuto dati dal client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: closeFile - FILE: %s\n", numeroDelThread, *fd, pathname) == -1) {
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
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: closeFile - FILE: %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        } else {
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: closeFile - FILE: %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            if(res > 0) {
                wakeUp = res, res = 0;
                if((bytes = sendMSG(wakeUp, (void *) &res, sizeof(int))) <= 0) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
                bytesWrite += bytes;
                if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
                    CLIENT_GOODBYE;
                    free(pathname);
                    close(*fd);
                    free(fd);
                    free(request);
                    return (void *) &errno;
                }
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
        if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            return (void *) &errno;
        }

        free(pathname);
    }

    /** removeFile **/
    if(strncmp(request, "removeFile", (size_t) fmax(11, (double) requestSize)) == 0) {
        /** Variabili **/
        size_t removed = 0;

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
        if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
            CLIENT_GOODBYE;
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: removeFile - FILE: %s\n", numeroDelThread, *fd, pathname) == -1) {
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
        if(traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n") == -1) {
            CLIENT_GOODBYE;
            free(pathname);
            close(*fd);
            free(fd);
            free(request);
            errno = ECOMM;
            return (void *) &errno;
        }
        if(errno == 0) {
            removed = resCancellazione->size;
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: closeFile - FILE: %s - ESITO: eseguita correttamente\n", numeroDelThread, *fd, pathname) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
            int *fdUn = NULL;
            int result = ENOENT;
            while(resCancellazione != NULL && resCancellazione->utentiLocked != NULL) {
                fdUn = deleteFirstElement(&(resCancellazione->utentiLocked));
                if(fdUn != NULL) {
                    bytes = sendMSG(*fdUn, &result, sizeof(int));
                    if(bytes != -1) {
                        bytesWrite += bytes;
                        traceOnLog(log, "[THREAD %d]: Spedisco dati al client\n");
                    }
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
            if(traceOnLog(log, "[THREAD %d]: CLIENT: %d - RICHIESTA: closeFile - FILE: %s - ESITO: fallita - ERRORE: %s\n", numeroDelThread, *fd, pathname, errorMsg) == -1) {
                CLIENT_GOODBYE;
                free(pathname);
                close(*fd);
                free(fd);
                free(request);
                return (void *) &errno;
            }
        }
        if(traceOnLog(log, "[THREAD %d]: CLIENT %d - RIMOSSI: %ldB\n", numeroDelThread, *fd, removed) == -1) {
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
    bytesWrite += bytes;
    if(traceOnLog(log, "[THREAD %d]: CLIENT %d - INVIATI: %ldB - RICEVUTI: %ldB\n", numeroDelThread, *fd, bytesRead, bytesWrite) == -1) {
        CLIENT_GOODBYE;
        close(*fd);
        free(fd);
        free(request);
        return (void *) &errno;
    }
    free(fd);
    free(request);

    errno = 0;
    return (void *) 0;
}
