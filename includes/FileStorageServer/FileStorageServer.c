/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione della memoria cache con politica di espulsone LRU
 * @author              Simone Tassotti
 * @date                22/12/2021
 */

#include "FileStorageServer.h"


/**
 * @brief                   Macro che controlla che se bisogna espellere un file dal server
 * @macro                   MEMORY_MISS
 * @param ADD_FILE          Indica se un file viene aggiunto
 * @param SIZE_TO_ADD       Dimensione da andare ad aggiungere
 */
#define MEMORY_MISS(ADD_FILE, SIZE_TO_ADD) \
    do {                                   \
        int next = 0;                      \
        if(traceOnLog(cache->log, "[ATTENZIONE]: Controllo di possibile MemoryMiss...\n") == -1) { \
            pthread_mutex_unlock(cache->LRU_Access);                           \
            destroyFile(&toAdd);\
            free(copy);                                                                                                                     \
            return kickedFiles;                                    \
        }                                    \
        while((cache->maxFileOnline < (cache->fileOnline + (ADD_FILE))) || (cache->maxBytesOnline < (cache->bytesOnline + (SIZE_TO_ADD)))) {    \
            if(strncmp(pathname, cache->LRU[next]->pathname, MAX_PATHNAME) == 0) { next++; continue; }                               \
            (cache->numeroMemoryMiss)++;                                                                                                        \
            numKick++;                                                                                                                          \
            if((kickedFiles = (myFile **) realloc(kickedFiles, (numKick+1)*sizeof(myFile *))) == NULL) { free(copy); return NULL; }         \
            if((cache->LRU[next])->lockAccessFile != toAdd->lockAccessFile) {                      \
                if((error = pthread_mutex_lock(((cache->LRU)[next])->lockAccessFile)) != 0) {                                         \
                    pthread_mutex_unlock(cache->LRU_Access);                           \
                    errno = error;             \
                    destroyFile(&toAdd);\
                    free(copy);                                                                                                                     \
                    return kickedFiles;                                                                                                             \
                }\
            }                              \
            kickedFiles[numKick-1] = (cache->LRU)[next];                                                                                           \
            (cache->LRU)[next] = (cache->LRU)[--(cache->fileOnline)];                                                                              \
            (cache->LRU)[(cache->fileOnline)] = NULL;                                                                                           \
            (cache->bytesOnline) -= (kickedFiles[numKick-1])->size;                                                                             \
            if(traceOnLog(cache->log, "[ATTENZIONE]: File \"%s\" espulso dalla cache per \"%s\"\n", kickedFiles[numKick-1]->pathname, ((ADD_FILE == 1) ? "Troppi file nel server" : "Problemi di capacità")) == -1) { \
                pthread_mutex_unlock((kickedFiles[numKick-1])->lockAccessFile);                           \
                pthread_mutex_unlock(cache->LRU_Access);                            \
                destroyFile(&toAdd);\
                free(copy);                                                                                                                     \
                return kickedFiles;                                    \
            }\
            if(icl_hash_delete(cache->tabella, kickedFiles[numKick-1]->pathname, free, NULL) == -1) {                                           \
                pthread_mutex_unlock(((cache->LRU)[next])->lockAccessFile);                           \
                pthread_mutex_unlock(cache->LRU_Access);                           \
                errno = error;             \
                destroyFile(&toAdd);\
                free(copy);                                                                                                                     \
                return kickedFiles;                                                                                                             \
            }                              \
            if((kickedFiles[numKick-1])->lockAccessFile != toAdd->lockAccessFile) {\
                if((error = pthread_mutex_unlock((kickedFiles[numKick-1])->lockAccessFile)) != 0) {                                                          \
                    pthread_mutex_unlock(cache->LRU_Access);                               \
                    errno = error;             \
                    free(copy);                                                                                                                     \
                    destroyFile(&toAdd);\
                    return kickedFiles;                                                                                                             \
                }                          \
            }                               \
            kickedFiles[numKick] = NULL;                                                                                                        \
        }                                       \
    }while(0)


/**
 * @brief           Funzione destroyFile riadattata per la icl_hash_destroy
 * @fun             free_file
 * @param f         File da cancellare
 */
static void free_file(void *f) {
    /** Variabili **/
    myFile *file = NULL;

    /** Cast **/
    file = (myFile *) f;

    /** Cancellazione del file **/
    destroyFile(&file);
}


/**
 * @brief           Cancella la struttura userLink
 * @fun             free_userLink
 * @param f         Struttura da cancellare
 */
static void free_userLink(void *f) {
    /** Variabili **/
    userLink *uL = NULL;

    /** Cast **/
    uL = (userLink *) f;

    /** Dealloco **/
    free(uL->openFile);
    free(uL);
}


/**
 * @brief           Compara un file con il pathname che si vuole ricercare
 * @fun             findFileOnLRU
 * @param v1        Primo valore da comparare
 * @param v2        Secondo valore da comparare
 * @return          Ritorna 0 se i valori del pathname corrispondono oppure la differenza tra il pathname passato e
 *                  quello del file con cui si compara
 */
static int findFileOnLRU(const void *v1, const void *v2) {
    /** Variabili **/
    char *p = NULL;
    myFile **f = NULL;

    /** Cast **/
    p = (char *) v1;
    f = (myFile **) v2;

    /** Ritorno **/
    return strncmp(p, (*f)->pathname, strnlen((*f)->pathname, MAX_PATHNAME));
}


/**
 * @brief           Funzione per compare due utenti tramite fd
 * @fun             findUsers
 * @param v1        Primo fd da comparare
 * @param v2        Secondo fd da comparare
 * @return          Ritorna la differenza tra i due; se corrispondono ritorna 0
 */
static int findUsers(const void *v1, const void *v2) {
    /** Variabili **/
    Queue **q = NULL;
    int fd1, fd2;

    /** Cast **/
    q = (Queue **)v2;
    fd1 = *(int *) v1;
    fd2 = ((userLink *) (*q)->data)->fd;

    /** Differenza **/
    return (fd1 - fd2);
}


/**
 * @brief           Funzione per compare due utenti tramite pathname
 * @fun             findPath
 * @param v1        Primo pathname da comparare
 * @param v2        Secondo pathname da comparare
 * @return          Ritorna 0 se coincidono altrimenti la loro differenza
 */
static int findPath(const void *v1, const void *v2) {
    /** Variabili **/
    char *p1 = NULL, *p2 = NULL;
    size_t len1, len2;

    /** Cast **/
    p1 = (char *)v1;
    p2 = ((userLink*) (v2))->openFile;
    len1 = strnlen(p1, MAX_PATHNAME);
    len2 = strnlen(p2, MAX_PATHNAME);

    /** Comparazione **/
    return strncmp(p1, p2, (size_t) fmax((double) len1, (double) len2)) == 0;
}


/**
 * @brief                   Funzione usata nella qsort per riordinare gli elementi nella LRU
 * @fun                     orderLRUFiles
 * @param v1                Primo valore
 * @param v2                Secondo valore
 * @return                  Ritorna la differenza tra il primo e secondo valore
 */
static int orderLRUFiles(const void *v1, const void *v2) {
    /** Variabili **/
    myFile **f1 = NULL, **f2 = NULL;
    time_t t1Sec = -1, t2Sec = -1, t1uSec = -1, t2uSec = -1;

    /** Cast **/
    if(v1 != NULL) {
        (f1) = (myFile **) v1;
        t1Sec = ((*f1)->time).tv_sec;
        t1uSec = ((*f1)->time).tv_usec;
    }
    if(v2 != NULL) {
        (f2) = (myFile **) v2;
        t2Sec = ((*f2)->time).tv_sec;
        t2uSec = ((*f2)->time).tv_usec;
    }
    if(v1 == NULL && v2 != NULL) {
        return (int)(0 - t1Sec);
    } else if(v1 != NULL && v2 == NULL) {

    } if(v1 == NULL && v2 == NULL) return 0;


    /** Ritorno **/
    if((t1Sec - t2Sec) == 0)
        return (int) (t1uSec-t2uSec);
    else
        return (int) (t1Sec - t2Sec);
}


/**
 * @brief           Cancella la struttura ClientFile
 * @fun             free_ClientFile
 * @param v         Struttura da rimuovere
 */
static void free_ClientFile(void *v) {
    /** Variabili **/
    ClientFile *f = NULL;

    /** Cast **/
    f = (ClientFile *) v;
    destroyFile(&(f->f));
    free(f);
}


/**
 * @brief                       Funzione che aggiorna la coda LRU
 * @fun                         LRU_Update
 * @param LRU                   Coda LRU
 * @param numElements           Numero di elementi presenti nella coda
 */
static void LRU_Update(myFile **LRU, unsigned int numElements) {
    /** Controllo parametri **/
    if(LRU == NULL) { return; }

    /** Aggiornamento **/
    qsort(LRU, numElements, sizeof(myFile **), orderLRUFiles);
}


/**
 * @brief                   Funzione che gestisce le connessioni tra client e file
 * @fun                     linksManage
 * @param cache             Memoria cache
 * @param fd                Fd del client che dobbiamo gestire
 * @param cmp               Valore che vogliamo gestire insieme al client
 * @param link              Tipo di gestione delle connessioni
 * @param comp              Funzione di comparazione
 * @return                  In caso di successo ritorna la lista dei collegamenti
 *                          che si sono interrotti o NULL; in caso di errore [setta errno]
 */
static Queue* linksManage(LRU_Memory *cache, int fd, void *cmp, int link, Compare_Fun comp) {
    /** Variabili **/
    int error = 0;
    long i = -1;
    userLink *uL = NULL;
    Queue *del = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return NULL; }
    if(fd <= 0) { errno = EINVAL; return NULL; }
    if(link < 0 || link > 2) { errno = EINVAL; return NULL; }

    /** Gestisco i collegamenti in base alla modalità scelta **/
    if ((error = pthread_mutex_lock(cache->usersConnectedAccess)) != 0) {
        errno = error;
        return NULL;
    }
    while(++i < cache->usersOnlineOpenFile) {
        if(findUsers(&fd, (cache->usersConnected)+i) == 0) { break; }
    }
    if (link == 0) {
        if(i >= cache->usersOnlineOpenFile) { (cache->usersOnlineOpenFile)++; }
        if ((uL = (userLink *) malloc(sizeof(userLink))) == NULL) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            return NULL;
        }
        if ((uL->openFile = (char *) calloc((strnlen(cmp, MAX_PATHNAME) + 1), sizeof(char))) == NULL) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            free(uL);
            return NULL;
        }
        strncpy(uL->openFile, cmp, (strnlen(cmp, MAX_PATHNAME) + 1));
        uL->fd = fd;
        if ((cache->usersConnected[i] = insertIntoQueue(cache->usersConnected[i], uL, sizeof(*uL))) == NULL) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            free(uL->openFile);
            free(uL);
            return NULL;
        }
        free(uL);
    } else if (link == 1) {
        if(i >= cache->usersOnlineOpenFile) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            errno = ENOENT;
            return NULL;
        }
        if((del = (Queue *) malloc(sizeof(Queue))) == NULL) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            return NULL;
        }
        if((del->data = deleteElementFromQueue(cache->usersConnected+i, cmp, comp)) == NULL) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            free(del);
            return NULL;
        }
        del->next = NULL;
        if ((cache->usersConnected[i]) == NULL) {
            (cache->usersOnlineOpenFile)--;
            if(cache->usersOnlineOpenFile == 0)
                ((cache->usersConnected)[i]) = NULL;
            else {
                ((cache->usersConnected)[i]) = cache->usersConnected[cache->usersOnlineOpenFile];
                cache->usersConnected[cache->usersOnlineOpenFile] = NULL;
            }

        }
    } else {
        if(i >= cache->usersOnlineOpenFile) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            errno = ENOENT;
            return NULL;
        }

        del = cache->usersConnected[i];
        (cache->usersOnlineOpenFile)--;
        if(cache->usersOnlineOpenFile == 0)
            ((cache->usersConnected)[i]) = NULL;
        else {
            ((cache->usersConnected)[i]) = cache->usersConnected[cache->usersOnlineOpenFile];
            cache->usersConnected[cache->usersOnlineOpenFile] = NULL;
        }
    }
    if ((error = pthread_mutex_unlock(cache->usersConnectedAccess)) != 0) {
        destroyQueue(&del, free_userLink);
        errno = error;
    }

    errno = 0;
    return del;
}


/**
 * @brief                           Legge il contenuto del configFile del server e lo traduce in una struttura in memoria principale
 * @fun                             readConfigFile
 * @param configPathname            Pathname del config file
 * @return                          Ritorna la struttura delle impostazioni del server; in caso di errore ritorna NULL [setta errno]
 */
Settings* readConfigFile(const char *configPathname) {
    /** Variabili **/
    char *buffer = NULL, *commento = NULL, *opt = NULL;
    int error = 0;
    long valueOpt = -1;
    FILE *file = NULL;
    Settings *serverMemory = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(configPathname == NULL) { errno = EINVAL; return NULL; }
    if((file = fopen(configPathname, "r")) == NULL) { errno = ENOENT; return NULL; }

    /** Lettura del file **/
    if((serverMemory = (Settings *) malloc(sizeof(Settings))) == NULL) { error = errno; fclose(file); errno = error; return NULL; }
    if((buffer = (char *) calloc(MAX_BUFFER_LEN, sizeof(char))) == NULL) { error = errno; fclose(file); free(serverMemory); errno = error; return NULL; }
    memset(serverMemory, 0, sizeof(Settings));
    while((memset(buffer, 0, MAX_BUFFER_LEN*sizeof(char)), fgets(buffer, MAX_BUFFER_LEN, file)) != NULL) {

        if(strnlen(buffer, MAX_BUFFER_LEN) == 2) continue;

        if((commento = strchr(buffer, '#')) != NULL) {  // Se trovo un commento scarto tutto quello che c'e' dopo
            size_t lenCommento = strnlen(commento, MAX_BUFFER_LEN);
            memset(commento, 0, lenCommento*sizeof(char));
            continue;
        }

        // Imposto il numero di thread worker sempre "attivi"
        if((serverMemory->numeroThreadWorker == 0) && (strstr(buffer, "numeroThreadWorker") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->numeroThreadWorker = valueOpt; continue; }
        else if(serverMemory->numeroThreadWorker == 0) { serverMemory->numeroThreadWorker = DEFAULT_NUMERO_THREAD_WORKER; }

        // Imposto il numero massimo di MB che il server puo' imagazzinare
        if((serverMemory->maxMB == 0) && (strstr(buffer, "maxMB") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->maxMB = valueOpt; continue; }
        else if(serverMemory->maxMB == 0) serverMemory->maxMB = DEFAULT_MAX_MB;

        // Imposto il canale di comunicazione socket
        if((serverMemory->socket == NULL) && ((serverMemory->socket = (char *) calloc(MAX_BUFFER_LEN, sizeof(char))) == NULL)) { error = errno; free(buffer); fclose(file); free(serverMemory); errno = error; return NULL; }
        if(((opt = strstr(buffer, "socket")) != NULL) && ((opt = strrchr(opt, '=')) != NULL) && (strstr(opt+1, ".sk") != NULL)) {
            strncpy(serverMemory->socket, opt+1, strnlen(opt+1, MAX_BUFFER_LEN)-1);
            (serverMemory->socket)[strnlen(serverMemory->socket, MAX_BUFFER_LEN)-1] = '\0';
            continue;
        }
        else if(serverMemory->socket == NULL) strncpy(serverMemory->socket, DEFAULT_SOCKET, strnlen(DEFAULT_SOCKET, MAX_BUFFER_LEN)+1);

        // Imposto il numero massimo di file che si possono caricare
        if((serverMemory->maxNumeroFileCaricabili == 0) && (strstr(buffer, "maxNumeroFileCaricabili") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->maxNumeroFileCaricabili = valueOpt; continue; }
        else if(serverMemory->maxNumeroFileCaricabili == 0) serverMemory->maxNumeroFileCaricabili = DEFUALT_MAX_NUMERO_FILE;

        // Imposto il numero massimo di utenti connessi al server
        if((serverMemory->maxUtentiConnessi == 0) && (strstr(buffer, "maxUtentiConnessi") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->maxUtentiConnessi = valueOpt; continue; }
        else if(serverMemory->maxUtentiConnessi == 0) serverMemory->maxUtentiConnessi = DEFUALT_MAX_NUMERO_FILE;

        // Imposto il numero massimo di utenti che possono aprire un file contemporaneamente
        if((serverMemory->maxUtentiPerFile == 0) && (strstr(buffer, "maxUtentiPerFile") != NULL) && ((opt = strrchr(buffer, '=')) != NULL) && ((valueOpt = isNumber(opt+1)) != -1)) { serverMemory->maxUtentiPerFile = valueOpt; continue; }
        else if(serverMemory->maxUtentiPerFile == 0) serverMemory->maxUtentiPerFile = DEFUALT_MAX_NUMERO_UTENTI;
    }
    free(buffer);
    fclose(file);

    /** Ritorno le impostazioni del server **/
    errno = 0;
    return serverMemory;
}


/**
 * @brief                           Inizializza la struttura del server con politica LRU
 * @fun                             startLRUMemory
 * @param set                       Impostazioni del server lette dal file config
 * @param log                       Log per il tracciamento delle operazioni
 * @return                          Ritorna la struttura della memoria in caso di successo; NULL altrimenti [setta errno]
 */
LRU_Memory* startLRUMemory(Settings *set, serverLogFile *log) {
    /** Variabili **/
    int error = 0, index = -1;
    LRU_Memory *mem = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(set == NULL) { errno = EINVAL; return NULL; }
    if((set->maxUtentiPerFile == 0) || (set->maxNumeroFileCaricabili == 0) || (set->maxMB == 0)) { errno = EINVAL; return NULL; }

    /** Creo la struttura e la inizializzo **/
    if((mem = (LRU_Memory *) malloc(sizeof(LRU_Memory))) == NULL) { return NULL; }
    memset(mem, 0, sizeof(LRU_Memory));
    mem->maxBytesOnline = set->maxMB * 1000000;
    mem->maxFileOnline = set->maxNumeroFileCaricabili;
    mem->maxUtentiPerFile = set->maxUtentiPerFile;
    mem->maxUsersLoggedOnline = set->maxUtentiConnessi;
    if(log != NULL) mem->log = log;
    if((mem->tabella = icl_hash_create((int) ((set->maxNumeroFileCaricabili)*2), NULL, NULL)) == NULL) {
        free(mem);
        errno = EOPNOTSUPP;
        return NULL;
    }
    if((mem->LRU = (myFile **) calloc(set->maxNumeroFileCaricabili, sizeof(myFile *))) == NULL) {
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        return NULL;
    }
    if((mem->LRU_Access = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t))) == NULL) {
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        return NULL;
    }
    if((error = pthread_mutex_init(mem->LRU_Access, NULL)) != 0) {
        free(mem->LRU_Access);
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        errno = error;
        return NULL;
    }
    if((mem->notAdded = icl_hash_create((int) ((set->maxUtentiPerFile)*(set->maxNumeroFileCaricabili)), NULL, NULL)) == NULL) {
        pthread_mutex_destroy(mem->LRU_Access);
        free(mem->LRU_Access);
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        return NULL;
    }
    if((mem->notAddedAccess = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t))) == NULL) {
        icl_hash_destroy(mem->notAdded, free, free_ClientFile);
        pthread_mutex_destroy(mem->LRU_Access);
        free(mem->LRU_Access);
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        return NULL;
    }
    if((error = pthread_mutex_init(mem->notAddedAccess, NULL)) != 0) {
        free(mem->notAdded);
        icl_hash_destroy(mem->notAdded, free, free_ClientFile);
        pthread_mutex_destroy(mem->LRU_Access);
        free(mem->LRU_Access);
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        errno = error;
        return NULL;
    }
    if((mem->usersConnected = (Queue **) calloc(set->maxNumeroFileCaricabili, sizeof(Queue *))) == NULL) {
        pthread_mutex_destroy(mem->notAddedAccess);
        free(mem->notAdded);
        icl_hash_destroy(mem->notAdded, free, free_ClientFile);
        pthread_mutex_destroy(mem->LRU_Access);
        free(mem->LRU_Access);
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        return NULL;
    }
    if((mem->usersConnectedAccess = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t))) == NULL) {
        free(mem->usersConnected);
        pthread_mutex_destroy(mem->notAddedAccess);
        free(mem->notAdded);
        icl_hash_destroy(mem->notAdded, free, free_ClientFile);
        pthread_mutex_destroy(mem->LRU_Access);
        free(mem->LRU_Access);
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        return NULL;
    }
    if((error = pthread_mutex_init(mem->usersConnectedAccess, NULL)) != 0) {
        free(mem->usersConnectedAccess);
        free(mem->usersConnected);
        pthread_mutex_destroy(mem->notAddedAccess);
        free(mem->notAdded);
        icl_hash_destroy(mem->notAdded, free, free_ClientFile);
        pthread_mutex_destroy(mem->LRU_Access);
        free(mem->LRU_Access);
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        errno = error;
        return NULL;
    }
    memset(mem->usersConnected, 0, sizeof(Queue*)*(set->maxNumeroFileCaricabili));
    if((mem->Files_Access = (pthread_mutex_t *) calloc(2*(set->maxNumeroFileCaricabili), sizeof(pthread_mutex_t))) == NULL) {
        pthread_mutex_destroy(mem->usersConnectedAccess);
        free(mem->usersConnectedAccess);
        free(mem->usersConnected);
        pthread_mutex_destroy(mem->notAddedAccess);
        free(mem->notAdded);
        icl_hash_destroy(mem->notAdded, free, free_ClientFile);
        pthread_mutex_destroy(mem->LRU_Access);
        free(mem->LRU_Access);
        free(mem->LRU);
        icl_hash_destroy(mem->tabella, free, free_file);
        free(mem);
        return NULL;
    }
    while(++index < 2*(set->maxNumeroFileCaricabili)) {
        if((error = pthread_mutex_init((mem->Files_Access)+index, NULL)) != 0) {
            while (--index >= 0) {
                pthread_mutex_destroy(mem->Files_Access);
            }
            free(mem->Files_Access);
            pthread_mutex_destroy(mem->usersConnectedAccess);
            free(mem->usersConnectedAccess);
            free(mem->usersConnected);
            pthread_mutex_destroy(mem->notAddedAccess);
            free(mem->notAdded);
            icl_hash_destroy(mem->notAdded, free, free_ClientFile);
            pthread_mutex_destroy(mem->LRU_Access);
            free(mem->LRU_Access);
            free(mem->LRU);
            icl_hash_destroy(mem->tabella, free, free_file);
            free(mem);
            errno = error;
            return NULL;
        }
    }

    /** Ritorno la memoria cache **/
    errno = 0;
    return mem;
}


/**
 * @brief           Mi segnala il login di un client
 * @fun             loginClient
 * @param cache     Memoria cache
 * @return          Ritorna 0 in caso di successo; -1 altrimenti [setta errno]
 */
int loginClient(LRU_Memory *cache) {
    /** Variabili **/
    int error = 0;

    /** Aggiungo un client al server **/
    errno = 0;
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }
    if(cache->usersLoggedNow < cache->maxUsersLoggedOnline) (cache->usersLoggedNow)++;
    else { pthread_mutex_unlock(cache->LRU_Access); errno = EMLINK; return -1; }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }

    errno = 0;
    return 0;
}


/**
 * @brief               Mi indica il numero di client online in quell'istante
 * @fun                 clientOnline
 * @param cache         Memoria cache
 * @return              Ritorna il numero di client connessi; [setta errno] in caso di errore
 */
unsigned int clientOnline(LRU_Memory *cache) {
    /** Variabili **/
    unsigned int online = 0;
    int error = 0;

    /** Aggiungo un client al server **/
    errno = 0;
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }
    online = (int) (cache->usersLoggedNow);
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }

    errno = 0;
    return online;
}


/**
 * @brief           Segnala la disconnessione di un client
 * @fun             logoutClient
 * @param cache     Memoria cache
 * @return          Ritorna (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int logoutClient(LRU_Memory *cache) {
    /** Variabili **/
    int error = 0;

    /** Aggiungo un client al server **/
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }
    if(cache->usersLoggedNow > 0) (cache->usersLoggedNow)--;
    else { pthread_mutex_unlock(cache->LRU_Access); errno = EPERM; return -1; }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }

    errno = 0;
    return 0;
}


/**
 * @brief                   Crea il file che successivamente verra' aggiunto alla memoria cache
 * @fun                     createFileToInsert
 * @param pI                Tabella di Pre-Inserimento
 * @param pathname          Pathname del file da creare
 * @param maxUtenti         Numero massimo di utenti che il file puo'gestire
 * @param fd                FD del client che apre il file
 * @param lock              Flag che mi indica la possibilita' di effettuare la lock sul file da parte di fd
 * @return                  (0) in caso di successo; (-1) [setta errno] altrimenti
 */
int createFileToInsert(LRU_Memory *cache, const char *pathname, unsigned int maxUtenti, int fd, int lock) {
    /** Variabili **/
    int error = 0;
    myFile *create = NULL;
    ClientFile *cl = NULL;
    char *copy = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }
    if((lock < 0) || (lock > 1)) { errno = EINVAL; return -1; }

    /** Creo il file e lo aggiungo nella lista di PreInserimento **/
    if((copy = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        return -1;
    }
    strncpy(copy, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    if((create = createFile(copy, maxUtenti, NULL)) == NULL) {
        free(copy);
        return -1;
    }
    if(openFile(create, fd) != 0) {
        destroyFile(&create);
        free(copy);
        return -1;
    }
    if((lock) && (lockFile(create, fd) != 0)) {
        destroyFile(&create);
        free(copy);
        return -1;
    }
    if((cl = (ClientFile *) malloc(sizeof(ClientFile))) == NULL) {
        destroyFile(&create);
        free(copy);
        return -1;
    }
    cl->f = create;
    cl->fd_cl = fd;
    if((error = pthread_mutex_lock(cache->notAddedAccess)) != 0) {
        destroyFile(&create);
        free(cl);
        free(copy);
        errno = error;
        return -1;
    }
    if(icl_hash_insert(cache->notAdded, copy, cl) == NULL) {
        pthread_mutex_unlock(cache->notAddedAccess);
        free(cl);
        destroyFile(&create);
        free(copy);
        errno = EPERM;
        return -1;
    }
    if((error = pthread_mutex_unlock(cache->notAddedAccess)) != 0) {
        destroyFile(&create);
        free(cl);
        errno = error;
        return -1;
    }
    linksManage(cache, fd, (void *) pathname, 0, NULL);
    if(errno != 0) {
        return -1;
    }

    errno = 0;
    return 0;
}


/**
 * @brief                       Apre un file da parte di un fd sulla memoria cache
 * @fun                         openFileOnCache
 * @param cache                 Memoria cache
 * @param pathname              Pathname del file da aprire
 * @param openFD                FD che vuole aprire il file
 * @return                      (1) se file e' gia stato aperto; (0) se e' stato aperto con successo;
 *                              (-1) in caso di errore [setta errno]
 */
int openFileOnCache(LRU_Memory *cache, const char *pathname, int openFD) {
    /** Variabili **/
    int error = 0, result = -1;
    myFile *toOpen = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(openFD <= 0) { errno = EINVAL; return -1; }

    /** Tentativo di apertura del file **/
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }
    if((toOpen = icl_hash_find(cache->tabella, (void *) pathname)) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = ENOENT;
        return -1;
    }
    if((error = pthread_mutex_lock(toOpen->lockAccessFile)) != 0) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = error;
        return -1;
    }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        pthread_mutex_unlock(toOpen->lockAccessFile);
        errno = error;
        return -1;
    }
    result = openFile(toOpen, openFD);
    if(result == -1) {
        pthread_mutex_unlock(toOpen->lockAccessFile);
        return -1;
    }
    if((error = pthread_mutex_unlock(toOpen->lockAccessFile)) != 0) {
        errno = error;
        return -1;
    }
    linksManage(cache, openFD, (void *) pathname, 0, NULL);
    if(errno != 0) {
        return -1;
    }

    errno = 0;
    return result;
}


/**
 * @brief                   Chiude un file aperto da 'closeFD' (e lo unlocka se anche locked)
 * @fun                     closeFileOnCache
 * @param cache             Memoria cache
 * @param pathname          Pathname del file da chiudere
 * @param closeFD           FD che vuole chiudere il file
 * @return                  In caso di successo ritorna 0 o FD del client che ora detiene la lock
 *                          del file dopo 'closeFD'; -1 altrimenti e setta errno
 */
int closeFileOnCache(LRU_Memory *cache, const char *pathname, int closeFD) {
    /** Variabili **/
    int fdReturn = 0, error = 0, swap = 0;
    myFile *toClose = NULL;
    Queue *delete = NULL;
    ClientFile *cl = NULL;

    /** Controllo variabili **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(closeFD <= 0) { errno = EINVAL; return -1; }

    /** Chiudo il file nel server **/
    if((error = pthread_mutex_lock(cache->notAddedAccess)) != 0) {
        errno = error;
        return -1;
    }
    if((cl = icl_hash_find(cache->notAdded, (void *) pathname)) != NULL) {
        if(fileIsOpenedFrom(cl->f, closeFD)) {
            icl_hash_delete(cache->notAdded, (void *) pathname, free, free_ClientFile);
            swap = 1;
            fdReturn = 0;
        } else {
            pthread_mutex_unlock(cache->notAddedAccess);
            errno = EPERM;
            return -1;
        }
    }
    if((error = pthread_mutex_unlock(cache->notAddedAccess)) != 0) {
        errno = error;
        return -1;
    }
    if(!swap) {
        if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
            errno = error;
            return -1;
        }
        if((toClose = (myFile *) icl_hash_find(cache->tabella, (void *) pathname)) == NULL) {
            pthread_mutex_unlock(cache->LRU_Access);
            errno = ENOENT;
            return -1;
        }
        if((error = pthread_mutex_lock(toClose->lockAccessFile)) != 0) {
            pthread_mutex_unlock(cache->LRU_Access);
            errno = error;
            return -1;
        }
        if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
            pthread_mutex_unlock(toClose->lockAccessFile);
            errno = error;
            return -1;
        }
        if(fileIsLockedFrom(toClose, closeFD)) {
            if((fdReturn = unlockFile(toClose, closeFD)) == -1) {
                pthread_mutex_unlock(toClose->lockAccessFile);
                return -1;
            }
        }
        if(closeFile(toClose, closeFD) == -1) {
            pthread_mutex_unlock(toClose->lockAccessFile);
            return -1;
        }
        if((error = pthread_mutex_unlock(toClose->lockAccessFile)) != 0) {
            errno = error;
            return -1;
        }
    }
    delete = linksManage(cache, closeFD, (void *)pathname, 1, findPath);
    destroyQueue(&delete, free_userLink);

    errno = 0;
    return fdReturn;
}


/**
 * @brief               Funzione che aggiunge il file creato da fd nella memoria cache (gia' creato e aggiunto in Pre-Inserimento)
 * @fun                 addFileOnCache
 * @param cache         Memoria cache
 * @param pI            Memoria di Pre-Inserimento
 * @param pathname      Pathname del file da andare a caricare
 * @param fd            Fd di colui che aggiunge il file
 * @param checkLock     Flag che mi indica la volontà di controllare se il file è locked o meno da fd
 * @return              Ritorna i file espulsi in seguito a memory miss oppure NULL; in caso di errore nell'aggiunta del file
 *                      viene settato errno
 */
myFile** addFileOnCache(LRU_Memory *cache, const char *pathname, int fd, int checkLock) {
    /** Variabili **/
    int error = 0, numKick = 0, index = -1;
    char *copy = NULL;
    unsigned int hashPathname = 0;
    ClientFile *cl = NULL;
    myFile **kickedFiles = NULL, *toAdd = NULL;
    Queue *uL = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return NULL; }
    if(pathname == NULL) { errno = EINVAL; return NULL; }
    if(fd <= 0) { errno = EINVAL; return NULL; }

    /** Aggiungo il file **/
    if((copy = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        return NULL;
    }
    strncpy(copy, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    hashPathname = hash_pjw(copy), hashPathname %= (2*(cache->maxFileOnline));
    if((error = pthread_mutex_lock(cache->notAddedAccess)) != 0) {
        free(copy);
        errno = error;
        return NULL;
    }
    if((cl = (ClientFile *) icl_hash_find(cache->notAdded, copy)) == NULL) {
        pthread_mutex_unlock(cache->notAddedAccess);
        free(copy);
        errno = ENOENT;
        return NULL;
    }
    toAdd = cl->f;
    if(!fileIsOpenedFrom(toAdd, fd) || ((checkLock) && (!fileIsLockedFrom(toAdd, fd)))) {
        pthread_mutex_unlock(cache->notAddedAccess);
        free(copy);
        errno = EACCES;
        return NULL;
    }
    if(icl_hash_delete(cache->notAdded, copy, free, free) == -1) {
        pthread_mutex_unlock(cache->notAddedAccess);
        free(copy);
        errno = EAGAIN;
        return NULL;
    }
    if((error = pthread_mutex_unlock(cache->notAddedAccess)) != 0) {
        free(copy);
        destroyFile(&toAdd);
        errno = error;
        return NULL;
    }
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        destroyFile(&toAdd);
        free(copy);
        errno = error;
        return NULL;
    }
    LRU_Update(cache->LRU, cache->fileOnline);
    if(icl_hash_find(cache->tabella, copy) != NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        uL = linksManage(cache, fd, (void *) pathname, 1, findPath);
        if(uL != NULL) {
            destroyQueue(&uL, free_userLink);
        }
        if (errno != 0) {
            destroyFile(&toAdd);
            free(copy);
            return NULL;
        }
        destroyFile(&toAdd);
        free(copy);
        errno = EAGAIN;
        return NULL;
    }
    MEMORY_MISS(1, 0);
    toAdd->lockAccessFile = (cache->Files_Access) + hashPathname;
    if(icl_hash_insert(cache->tabella, copy, toAdd) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        destroyFile(&toAdd);
        free(copy);
        errno = EAGAIN;
        return kickedFiles;
    }
    (cache->LRU)[(cache->fileOnline)++] = toAdd;
    if((cache->massimoNumeroDiFileOnline) < (cache->fileOnline)) (cache->massimoNumeroDiFileOnline) = (cache->fileOnline);
    updateTime(toAdd);
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        errno = error;
        free(copy);
        destroyFile(&toAdd);
        return kickedFiles;
    }
    while(--numKick >= 0) {
        index = -1;
        while((kickedFiles[numKick]->utentiConnessi)[++index] != -1) {
            uL = linksManage(cache, (kickedFiles[numKick]->utentiConnessi)[index], (void *) (kickedFiles[numKick])->pathname, 1, findPath);
            if(uL != NULL) { destroyQueue(&uL, free_userLink); }
            if(errno != 0) {
                return kickedFiles;
            }
        }
    }

    errno = 0;
    return kickedFiles;
}


/**
 * @brief                   Rimuove un file dalla memoria cache
 * @fun                     removeFileOnCache
 * @param cache             Memoria cache da cui estrarre il file
 * @param pathname          Pathname del file da rimuovere
 * @param fd                Client che rimuove il file
 * @return                  Ritorna il file cancellato, altrimenti ritorna NULL (in caso di errore [setta errno])
 */
myFile* removeFileOnCache(LRU_Memory *cache, const char *pathname, int fd) {
    /** Variabili **/
    long index = -1;
    int error = 0;
    myFile *del = NULL;
    Queue *uL = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return NULL; }
    if(pathname == NULL) { errno = EINVAL; return NULL; }

    /** Cerco il file e lo cancello **/
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return NULL;
    }
    if(cache->fileOnline == 0) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = ENOENT;
        return NULL;
    }
    while(++index < cache->fileOnline) {
        if(findFileOnLRU(pathname, (cache->LRU)+index) == 0) break;
    }
    if(index >= cache->fileOnline) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = ENOENT;
        return NULL;
    }
    del = cache->LRU[index];
    if((error = pthread_mutex_lock(del->lockAccessFile)) != 0) {
        errno = error;
        pthread_mutex_unlock(cache->LRU_Access);
        return NULL;
    }
    if(!fileIsOpenedFrom(del, fd) || ((del->utenteLock != -1) && (del->utenteLock != fd))) {
        pthread_mutex_unlock(del->lockAccessFile);
        pthread_mutex_unlock(cache->LRU_Access);
        errno = EPERM;
        return NULL;
    }
    if(icl_hash_delete(cache->tabella, del->pathname, free, NULL) == -1) {
        pthread_mutex_unlock(del->lockAccessFile);
        pthread_mutex_unlock(cache->LRU_Access);
        errno = EAGAIN;
        return NULL;
    }
    cache->bytesOnline -= del->size;
    cache->LRU[index] = cache->LRU[--(cache->fileOnline)];
    cache->LRU[(cache->fileOnline)] = NULL;
    if((error = pthread_mutex_unlock(del->lockAccessFile)) != 0) {
        errno = error;
        pthread_mutex_unlock(cache->LRU_Access);
        return del;
    }
    del->lockAccessFile = NULL;
    LRU_Update((cache->LRU), cache->fileOnline);
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        errno = error;
        return del;
    }
    index = -1;
    while((del->utentiConnessi)[++index] != -1) {
        uL = linksManage(cache, (del->utentiConnessi)[index], (void *) pathname, 1, findPath);
        if(uL != NULL) { destroyQueue(&uL, free_userLink); }
        if(errno != 0) {
            return del;
        }
    }

    errno = 0;
    return del;
}


/**
 * @brief                       Aggiorna il contenuto di un file in modo atomico
 * @fun                         appendFile
 * @param cache                 Memoria cache
 * @param pathname              Pathname del file da aggiornare
 * @param fd                    Client che aggiunge il contenuto al file
 * @param buffer                Buffer da aggiungere
 * @param size                  Dimensione del buffer da aggiungere
 * @return                      Ritorna gli eventuali file espulsi; in caso di errore valutare se si setta errno
 */
myFile** appendFile(LRU_Memory *cache, const char *pathname, int fd, void *buffer, size_t size) {
    /** Variabili **/
    myFile **kickedFiles = NULL, *toAdd = NULL;
    int error = 0, numKick = 0;
    int index = -1;
    char *copy = NULL;
    Queue *uL = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return NULL; }
    if(pathname == NULL) { errno = EINVAL; return NULL; }
    if(buffer == NULL) { errno = EINVAL; return NULL; }

    /** Aggiungo al file il contenuto **/
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return NULL;
    }
    if((copy = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        return NULL;
    }
    strncpy(copy, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    LRU_Update(cache->LRU, cache->fileOnline);
    if((toAdd = icl_hash_find(cache->tabella, copy)) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        free(copy);
        errno = ENOENT;
        return NULL;
    }
    if((error = pthread_mutex_lock(toAdd->lockAccessFile)) != 0) {
        pthread_mutex_unlock(cache->LRU_Access);
        free(copy);
        errno = error;
        return kickedFiles;
    }
    if(cache->maxBytesOnline < size) {
        icl_hash_delete(cache->tabella, (void *) copy, free, NULL);
        pthread_mutex_unlock(cache->LRU_Access);
        pthread_mutex_unlock(toAdd->lockAccessFile);
        index = -1;
        while ((toAdd->utentiConnessi)[++index] != -1) {
            uL = linksManage(cache, (toAdd->utentiConnessi)[index], (void *) pathname, 1, findPath);
            if(uL != NULL) destroyQueue(&uL, free_userLink);
            if (errno != 0) {
                return NULL;
            }
        }
        destroyFile(&toAdd);
        errno = ETXTBSY;
        return NULL;
    }
    if(!fileIsOpenedFrom(toAdd, fd)) {
        pthread_mutex_unlock(toAdd->lockAccessFile);
        pthread_mutex_unlock(cache->LRU_Access);
        free(copy);
        errno = EPERM;
        return NULL;
    }
    if(toAdd->utenteLock != fd) {
        pthread_mutex_unlock(toAdd->lockAccessFile);
        pthread_mutex_unlock(cache->LRU_Access);
        free(copy);
        errno = EPERM;
        return NULL;
    }
    if(addContentToFile(toAdd, buffer, size) == -1) {
        pthread_mutex_unlock(toAdd->lockAccessFile);
        pthread_mutex_unlock(cache->LRU_Access);
        free(copy);
        return NULL;
    }
    MEMORY_MISS(0, size);
    cache->bytesOnline += size;
    if((cache->numeroMassimoBytesCaricato) < (cache->bytesOnline)) (cache->numeroMassimoBytesCaricato) = (cache->bytesOnline);
    if((error = pthread_mutex_unlock(toAdd->lockAccessFile)) != 0) {
        pthread_mutex_unlock(toAdd->lockAccessFile);
        free(copy);
        errno = error;
        return kickedFiles;
    }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        pthread_mutex_unlock(toAdd->lockAccessFile);
        free(copy);
        errno = error;
        return kickedFiles;
    }
    while(--numKick >= 0) {
        index = -1;
        while((kickedFiles[numKick]->utentiConnessi)[++index] != -1) {
            uL = linksManage(cache, (kickedFiles[numKick]->utentiConnessi)[index], (void *) (kickedFiles[numKick])->pathname, 1, findPath);
            if(uL != NULL) destroyQueue(&uL, free_userLink);
            if(errno != 0) {
                free(copy);
                return kickedFiles;
            }
        }
    }

    free(copy);
    errno = 0;
    return kickedFiles;
}


/**
 * @brief                   Funzione che legge il contenuto del file e ne restituisce una copia
 * @fun                     readFileOnCache
 * @param cache             Memoria cache
 * @param pathname          Pathname del file da leggere
 * @param fd                Client che legge il file dal server
 * @param dataContent       Contenuto del file
 * @return                  Ritorna la dimensione del buffer; (-1) altrimenti [setta errno]
 */
size_t readFileOnCache(LRU_Memory *cache, const char *pathname, int fd, void **dataContent) {
    /** Variabili **/
    int error = 0;
    size_t size = -1;
    myFile *readF = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }

    /** Trovo il file e lo leggo **/
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }
    if((readF = (myFile *) icl_hash_find(cache->tabella, (void *) pathname)) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = ENOENT;
        return -1;
    }
    if((error = pthread_mutex_lock(readF->lockAccessFile)) != 0) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = error;
        return -1;
    }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        pthread_mutex_unlock(readF->lockAccessFile);
        errno = error;
        return -1;
    }
    if(!fileIsOpenedFrom(readF, fd)) {
        pthread_mutex_unlock(readF->lockAccessFile);
        errno = EPERM;
        return -1;
    }
    if((readF->utenteLock != fd) && (readF->utenteLock != -1)) {
        pthread_mutex_unlock(readF->lockAccessFile);
        errno = EPERM;
        return -1;
    }
    if((*dataContent = malloc(readF->size)) == NULL) {
        pthread_mutex_unlock(readF->lockAccessFile);
        return -1;
    }
    memcpy(*dataContent, readF->buffer, readF->size);
    size = readF->size;
    updateTime(readF);
    if((error = pthread_mutex_unlock(readF->lockAccessFile)) != 0) {
        free(*dataContent);
        *dataContent = NULL;
        errno = error;
        return -1;
    }

    errno = 0;
    return size;
}


/**
 * @brief               Legge N file in ordine della LRU non locked
 * @fun                 readRandFiles
 * @param cache         Memoria cache
 * @param fd            Client che richiede la lettura dei file in modo random
 * @param N             Numero di file che si intende leggere
 * @return              Ritorna i file letti effettivamente; se ci sono errori viene settato errno e
 *                      viene ritornato NULL
 */
myFile** readsRandFiles(LRU_Memory *cache, int fd, int *N) {
    /** Variabili **/
    myFile **filesRead = NULL, **new = NULL;
    int error = 0, index = -1, nReads = 0;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return NULL; }

    /** Leggo i file in ordine di inserimento nella LRU **/
    if(*N <= 0) *N = (int) cache->maxFileOnline;
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return NULL;
    }
    LRU_Update(cache->LRU, cache->fileOnline);
    while((++index < cache->fileOnline) && (index < *N)) {
        if((error = pthread_mutex_lock((cache->LRU[index])->lockAccessFile)) != 0) {
            pthread_mutex_unlock(cache->LRU_Access);
            errno = error;
            return NULL;
        }
        if((cache->LRU[index]->utenteLock == -1) || (cache->LRU[index]->utenteLock == fd)) {
            nReads++;
            if((new = (myFile **) realloc(filesRead, (nReads+1)*sizeof(myFile *))) == NULL) {
                pthread_mutex_unlock((cache->LRU[index])->lockAccessFile);
                pthread_mutex_unlock(cache->LRU_Access);
                if(filesRead != NULL) {
                    while(--nReads >= 0) {
                        destroyFile(&(filesRead[nReads]));
                    }
                    free(filesRead);
                }
                errno = error;
                return NULL;
            }
            filesRead = new;
            if((filesRead[nReads-1] = (myFile *) malloc(sizeof(myFile))) == NULL) {
                pthread_mutex_unlock((cache->LRU[index])->lockAccessFile);
                pthread_mutex_unlock(cache->LRU_Access);
                if(filesRead != NULL) {
                    while(--nReads >= 0) {
                        destroyFile(&(filesRead[nReads]));
                    }
                    free(filesRead);
                }
                errno = error;
                return NULL;
            }
            memcpy(filesRead[nReads-1], (cache->LRU)[index], sizeof(myFile));
            if((filesRead[nReads-1]->pathname = calloc(strnlen((cache->LRU)[index]->pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
                pthread_mutex_unlock((cache->LRU[index])->lockAccessFile);
                pthread_mutex_unlock(cache->LRU_Access);
                if(filesRead != NULL) {
                    while(--nReads >= 0) {
                        destroyFile(&(filesRead[nReads]));
                    }
                    free(filesRead);
                }
                errno = error;
                return NULL;
            }
            if((filesRead[nReads-1]->buffer = malloc((cache->LRU)[index]->size)) == NULL) {
                pthread_mutex_unlock((cache->LRU[index])->lockAccessFile);
                pthread_mutex_unlock(cache->LRU_Access);
                if(filesRead != NULL) {
                    while(--nReads >= 0) {
                        destroyFile(&(filesRead[nReads]));
                    }
                    free(filesRead);
                }
                errno = error;
                return NULL;
            }
            memcpy(filesRead[nReads-1]->buffer, (cache->LRU)[index]->buffer, (cache->LRU)[index]->size);
            strncpy(filesRead[nReads-1]->pathname, (cache->LRU)[index]->pathname, strnlen((cache->LRU)[index]->pathname, MAX_PATHNAME)+1);
            filesRead[nReads-1]->utentiConnessi = NULL;
            filesRead[nReads-1]->lockAccessFile = NULL;
            filesRead[nReads-1]->utentiLocked = NULL;
            filesRead[nReads] = NULL;
        }
        updateTime(cache->LRU[index]);
        if((error = pthread_mutex_unlock((cache->LRU[index])->lockAccessFile)) != 0) {
            pthread_mutex_unlock(cache->LRU_Access);
            if(filesRead != NULL) {
                while(--nReads >= 0) {
                    destroyFile(&(filesRead[nReads]));
                }
                free(filesRead);
            }
            errno = error;
            return NULL;
        }
    }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        if(filesRead != NULL) {
            while(--nReads >= 0) {
                destroyFile(&(filesRead[nReads]));
            }
            free(filesRead);
        }
        errno = error;
        return NULL;
    }

    *N = nReads;
    errno = 0;
    return filesRead;
}


/**
 * @brief                   Effettua la lock su un file per quel fd
 * @fun                     lockFileOnCache
 * @param cache             Memoria cache
 * @param pathname          Pathname del file da bloccare
 * @param lockFD            Fd che effettua la lock
 * @return                  Ritorna (1) se il file e' locked gia'; (0) se la lock e' riuscita;
 *                          (-1) in caso di errore [setta errno]
 */
int lockFileOnCache(LRU_Memory *cache, const char *pathname, int lockFD) {
    /** Variabili **/
    int error = 0, lockResult = -1;
    myFile *fileToLock = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(lockFD <= 0) { errno = EINVAL; return -1; }

    /** Tento di effettuare la lock **/
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }
    if((fileToLock = (myFile *) icl_hash_find(cache->tabella, (void *) pathname)) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = ENOENT;
        return -1;
    }
    if((error = pthread_mutex_lock(fileToLock->lockAccessFile)) != 0) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = error;
        return -1;
    }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        pthread_mutex_unlock(fileToLock->lockAccessFile);
        errno = error;
        return -1;
    }
    if(((lockResult = lockFile(fileToLock, lockFD)) == -1) && ((errno != EALREADY))) {
        pthread_mutex_unlock(fileToLock->lockAccessFile);
        return -1;
    }
    if(errno == EALREADY) lockResult = lockFD;
    if((error = pthread_mutex_unlock(fileToLock->lockAccessFile)) != 0) {
        errno = error;
        return -1;
    }

    errno = 0;
    return lockResult;
}


/**
 * @brief                   Effettua la unlock su un file per quel fd
 * @fun                     unlockFileOnCache
 * @param cache             Memoria cache
 * @param pathname          Pathname del file da sbloccare
 * @param unlockFD          Fd che effettua la unlock
 * @return                  In caso di successo ritorna Fd del client da sbloccare;
 *                          (-1) in caso di errore [setta errno]
 */
int unlockFileOnCache(LRU_Memory *cache, const char *pathname, int unlockFD) {
    /** Variabili **/
    int error = 0, unlockResult = -1;
    myFile *fileToUnlock = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(unlockFD <= 0) { errno = EINVAL; return -1; }

    /** Tento di effettuare la unlock **/
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return -1;
    }
    if((fileToUnlock = (myFile *) icl_hash_find(cache->tabella, (void *) pathname)) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = ENOENT;
        return -1;
    }
    if((error = pthread_mutex_lock(fileToUnlock->lockAccessFile)) != 0) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = error;
        return -1;
    }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        pthread_mutex_unlock(fileToUnlock->lockAccessFile);
        errno = error;
        return -1;
    }
    if((unlockResult = unlockFile(fileToUnlock, unlockFD)) == -1) {
        pthread_mutex_unlock(fileToUnlock->lockAccessFile);
        return -1;
    }
    if((error = pthread_mutex_unlock(fileToUnlock->lockAccessFile)) != 0) {
        errno = error;
        return -1;
    }

    errno = 0;
    return unlockResult;
}


/**
 * @brief               Funzione che elimina ogni pendenza di un client disconnesso
 *                      su tutti i file in gestione a lui
 * @fun                 deleteClientFromCache
 * @param cache         Memoria cache
 * @param fd            Client che si è disconnesso
 * @return              Ritorna un puntatore ad un array di fd che specifica i client che sono in attesa dei file
 *                      locked da 'fd'; altrimenti, in caso di errore, setta errno
 */
int* deleteClientFromCache(LRU_Memory *cache, int fd) {
    /** Variabili **/
    int error = 0, *fdToUnlock = NULL, *new = NULL, numToUnlock = 0, app = -1;
    char *pathname = NULL;
    myFile *file = NULL;
    Queue *list = NULL;
    userLink *del = NULL;

    /** Controllo parametri **/
    if(cache == NULL) { errno = EINVAL; return NULL; }
    if(fd <= 0) { errno = EINVAL; return NULL; }


    errno=0;
    list = linksManage(cache, fd, (void *) NULL, 2, NULL);
    if((list == NULL) && (errno != 0)) {
        return NULL;
    }

    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        destroyQueue(&(list), free_userLink);
        errno = error;
        return NULL;
    }
    del = (userLink *) deleteFirstElement(&list);
    while(del != NULL) {
        pathname = del->openFile;
        file = icl_hash_find(cache->tabella, pathname);
        if(file == NULL) {
            free_userLink(del);
            del = (userLink *) deleteFirstElement(&list);
            continue;
        }
        if((error = pthread_mutex_lock(file->lockAccessFile)) != 0) {
            pthread_mutex_unlock(cache->LRU_Access);
            free_userLink(del);
            errno = error;
            return fdToUnlock;
        }
        app = unlockFile(file, fd);
        closeFile(file, fd);
        errno = 0;
        if(app != -1) {
            numToUnlock++;
            if((new = (int *) realloc(fdToUnlock, (numToUnlock+1)*sizeof(int))) == NULL) {
                pthread_mutex_unlock(file->lockAccessFile);
                pthread_mutex_unlock(cache->LRU_Access);
                free_userLink(del);
                return fdToUnlock;
            }
            fdToUnlock = new;
            fdToUnlock[numToUnlock-1] = app, fdToUnlock[numToUnlock] = -1;
        }
        if((error = pthread_mutex_unlock(file->lockAccessFile)) != 0) {
            pthread_mutex_unlock(cache->LRU_Access);
            errno = error;
            return fdToUnlock;
        }
        free_userLink(del);
        del = (userLink *) deleteFirstElement(&list);
    }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        errno = error;
        return fdToUnlock;
    }

    errno = 0;
    return fdToUnlock;
}


/**
 * @brief                   Cancella tutta la memoria e i settings della memoria cache
 * @fun                     deleteLRU
 * @param serverMemory      Settings del server letti dal config file
 * @param mem               Memoria cache
 */
void deleteLRU(Settings **serverMemory, LRU_Memory **cache) {
    /** Variabili **/
    int i = 0;


    /** Dealloco le impostazioni **/
    if(*serverMemory != NULL) {
        free((*serverMemory)->socket);
        free(*serverMemory);
        serverMemory = NULL;
    }

    /** Dealloco la memoria LRU **/
    if(*cache != NULL) {
        i = -1;

        /** Stampa delle statistiche del server **/
        printf("\t\t[STATISTICHE]\n\n");
        printf("Memoria occupata al momento dello shutdown: %.4lf MB\n", ((float) (*cache)->bytesOnline)/1000000);
        printf("Numero di file ancora presenti in memoria: %d\n", (*cache)->fileOnline);
        printf("Numero massimo di file memorizzato: %d\n", (*cache)->massimoNumeroDiFileOnline);
        printf("Massima capacità raggiunta dal server: %.4lf MB\n", ((float) (*cache)->numeroMassimoBytesCaricato)/1000000);
        printf("Meccanismo di espulsione file attivato %d volte\n", (*cache)->numeroMemoryMiss);
        printf("Verso il server sono state effettuate un numero di connessioni pari a %d\n", (*cache)->numTotLogin);
        printf("Lista dei file presenti al momento dello shutdown:\n");
        while(++i < ((*cache)->fileOnline)) {
            printf("File: %s\n", (*cache)->LRU[i]->pathname);
        }
        pthread_mutex_destroy((*cache)->Files_Access);
        pthread_mutex_destroy((*cache)->LRU_Access);
        pthread_mutex_destroy((*cache)->notAddedAccess);
        pthread_mutex_destroy((*cache)->usersConnectedAccess);
        icl_hash_destroy((*cache)->tabella, free, free_file);
        icl_hash_destroy((*cache)->notAdded, free, free_ClientFile);
        i = -1;
        while(((*cache)->usersConnected)[++i] != NULL) {
            destroyQueue(&(((*cache)->usersConnected)[i]), free_userLink);
        }

        free((*cache)->Files_Access);
        free((*cache)->LRU_Access);
        free((*cache)->notAddedAccess);
        free((*cache)->usersConnectedAccess);
        free((*cache)->usersConnected);
        free((*cache)->LRU);
        free(*cache);
        *cache = NULL;
    }
}
