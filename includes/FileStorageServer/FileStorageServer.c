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
#define MEMORY_MISS(ADD_FILE, SIZE_TO_ADD)                                                                                                  \
    while((cache->maxFileOnline < (cache->fileOnline + (ADD_FILE))) || (cache->maxBytesOnline < (cache->bytesOnline + (SIZE_TO_ADD)))) {    \
    printf("Entro miss\n"); \
        (cache->numeroMemoryMiss)++;                                                                                                        \
        numKick++;                                                                                                                          \
        if((kickedFiles = (myFile **) realloc(kickedFiles, (numKick+1)*sizeof(myFile *))) == NULL) { free(copy); return NULL; }             \
        if((error = pthread_mutex_lock(((cache->LRU)[0])->lockAccessFile)) != 0) {                                                          \
            errno = error;                                                                                                                  \
            destroyFile(&toAdd);\
            free(copy);                                                                                                                     \
            return kickedFiles;                                                                                                             \
        }                                                                                                                                   \
        kickedFiles[numKick-1] = (cache->LRU)[0];                                                                                           \
        (cache->LRU)[0] = (cache->LRU)[--(cache->fileOnline)];                                                                              \
        (cache->LRU)[(cache->fileOnline)] = NULL;                                                                                           \
        (cache->bytesOnline) -= (kickedFiles[numKick-1])->size;                                                                             \
        if(icl_hash_delete(cache->tabella, kickedFiles[numKick-1]->pathname, free, NULL) == -1) {                                           \
            errno = error;                                                                                                                  \
            destroyFile(&toAdd);\
            free(copy);                                                                                                                     \
            return kickedFiles;                                                                                                             \
        }                                                                                                                                   \
        if((error = pthread_mutex_unlock((kickedFiles[numKick-1])->lockAccessFile)) != 0) {                                                          \
            errno = error;                                                                                                                  \
            free(copy);                                                                                                                     \
            destroyFile(&toAdd);\
            return kickedFiles;                                                                                                             \
        }\
        kickedFiles[numKick] = NULL;                                                                                                        \
    }


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
 * @brief           Ordinamento per fd degli utenti
 * @fun             orderUsers
 * @param v1        Primo utente da ordinare
 * @param v2        Secondo utente da ordinare
 * @return          Ritorna la differenza tra il primo e il secondo elemento
 */
static int orderUsers(const void *v1, const void *v2) {
    /** Variabili **/
    Queue **q1 = NULL, **q2 = NULL;

    /** Cast **/
    q1 = (Queue **)v1;
    q2 = (Queue **)v2;
    int fd1 = ((userLink *) (*q1)->data)->fd;
    int fd2 = ((userLink *) (*q2)->data)->fd;

    /** Differenza **/
    return (fd1 - fd2);
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

//static int findLink(const void *v1, const void *v2) {
//    int fd1 = *(int *)v1;
//    int fd2 = ((userLink *) (v2))->fd;
//
//    return (fd1 == fd2);
//}


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
    printf("STAMPO LE LUNGHEZZE: %ld - %ld", len1, len2);

    /** Comparazione **/
    return strcmp(p1, p2) == 0;
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
        t1Sec = ((*f2)->time).tv_sec;
        t1uSec = ((*f2)->time).tv_usec;
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
 * @brief                   Funzione che, a seconda delle casistiche, aggiunge/cancella i collegamenti
 *                          tra file e client
 * @fun                     linksManage
 * @param cache             Memoria cache
 * @param fd                Fd del client che dobbiamo gestire
 * @param cmp               Valore che vogliamo gestire insieme al client
 * @param link              Compito da eseguire
 * @param comp              Funzione di comparazionee
 * @return
 */
static Queue* linksManage(LRU_Memory *cache, int fd, void *cmp, int link, Compare_Fun comp) {
    /** Variabili **/
    int error = 0;
    long i = -1;
    userLink *uL = NULL;
    Queue **user = NULL;
    Queue *del = NULL;

    /** Controllo parametri **/
    if(cache == NULL) { errno = EINVAL; return NULL; }
    if(fd <= 0) { errno = EINVAL; return NULL; }
    if(link < 0 || link > 2) { errno = EINVAL; return NULL; }

    /** Gestisco i collegamenti in base alla modalità scelta **/
    if ((error = pthread_mutex_lock(cache->usersConnectedAccess)) != 0) {
        errno = error;
        return NULL;
    }
    user = bsearch(&fd, cache->usersConnected, cache->usersOnline, sizeof(Queue **), findUsers);
    if (user != NULL) i = user - cache->usersConnected;
    if (link == 0) {
        if (user == NULL) i = (cache->usersOnline), (cache->usersOnline)++;
        if ((uL = (userLink *) malloc(sizeof(userLink))) == NULL) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            return NULL;
        }
        if ((uL->openFile = (char *) calloc((strnlen(cmp, MAX_PATHNAME) + 1), sizeof(char))) == NULL) {
            free(uL);
            pthread_mutex_unlock(cache->usersConnectedAccess);
            return NULL;
        }
        printf("aaaaaaaaaaaaaa%ld --- %s\n", (strnlen(cmp, MAX_PATHNAME) + 1), (char *) cmp);
        strncpy(uL->openFile, cmp, (strnlen(cmp, MAX_PATHNAME) + 1));
        uL->fd = fd;
        if ((cache->usersConnected[i] = insertIntoQueue(cache->usersConnected[i], uL, sizeof(userLink))) == NULL) {
            free(uL->openFile);
            free(uL);
            pthread_mutex_unlock(cache->usersConnectedAccess);
            return NULL;
        }
        free(uL);
        printf("AGGIUNGO IN POSIZIONE %ld client %d -- %s\n", i, fd, ((userLink *)(cache->usersConnected[i])->data)->openFile);
    } else if (link == 1) {
        if((del = deleteElementFromQueue(&(cache->usersConnected[i]), cmp, comp)) == NULL) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            return NULL;
        }
        printf("QUALCOSA HO TROVATO\n");
        if ((cache->usersConnected[i]) == NULL) {
            (cache->usersOnline)--;
            cache->usersConnected[i] = cache->usersConnected[cache->usersOnline];
            cache->usersConnected[cache->usersOnline] = NULL;
        }
    } else {
        if (user == NULL) {
            pthread_mutex_unlock(cache->usersConnectedAccess);
            errno = ENOENT;
            return NULL;
        }
        i = user - cache->usersConnected;
        del = *user;
        cache->usersConnected[i] = NULL;
        (cache->usersOnline)--;
        cache->usersConnected[i] = cache->usersConnected[cache->usersOnline];
        cache->usersConnected[cache->usersOnline] = NULL;
    }
    qsort(cache->usersConnected, cache->usersOnline, sizeof(Queue **), orderUsers);
    if ((error = pthread_mutex_unlock(cache->usersConnectedAccess)) != 0) {
        errno = error;
    }

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
    if(set == NULL) { errno = EINVAL; return NULL; }
    if((set->maxUtentiPerFile == 0) || (set->maxNumeroFileCaricabili == 0) || (set->maxMB == 0)) { errno = EINVAL; return NULL; }

    /** Creo la struttura e la inizializzo **/
    if((mem = (LRU_Memory *) malloc(sizeof(LRU_Memory))) == NULL) { return NULL; }
    memset(mem, 0, sizeof(LRU_Memory));
    mem->maxBytesOnline = set->maxMB * 1000000;
    mem->maxFileOnline = set->maxNumeroFileCaricabili;
    mem->maxUtentiPerFile = set->maxUtentiPerFile;
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
    memset(mem->LRU, 0, (mem->maxFileOnline)*sizeof(myFile *));
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

    return mem;
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
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }
    if((lock < 0) || (lock > 1)) { errno = EINVAL; return -1; }

    /** Creo il file e lo aggiungo nella lista di PreInserimento **/
    if((copy = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        return -1;
    }
    strncpy(copy, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    if((create = createFile(pathname, maxUtenti, NULL)) == NULL) {
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
        free(cl);
        destroyFile(&create);
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
        free(cl);
        destroyFile(&create);
        free(copy);
        errno = error;
        return -1;
    }
    linksManage(cache, fd, (void *) pathname, 0, NULL);
    if(errno != 0) return -1;

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
    if(errno != 0)
        return -1;

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
    int fdReturn = 0, error = 0;
    myFile *toClose = NULL;
    userLink *delete = NULL;

    /** Controllo variabili **/
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }
    if(closeFD <= 0) { errno = EINVAL; return -1; }

    /** Chiudo il file nel server **/
    if((error = pthread_mutex_lock(cache->notAddedAccess)) != 0) {
        errno = error;
        return -1;
    }
    icl_hash_delete(cache->notAdded, (void *) pathname, free, free_ClientFile);
    if((error = pthread_mutex_unlock(cache->notAddedAccess)) != 0) {
        errno = error;
        return -1;
    }
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
    delete = (userLink *) linksManage(cache, closeFD, (void *)pathname, 1, findPath);
    free_userLink(delete);

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
    int error = 0, numKick = 0;
    char *copy = NULL;
    unsigned int hashPathname = 0;
    ClientFile *cl = NULL;
    myFile **kickedFiles = NULL, *toAdd = NULL;

    /** Controllo parametri **/
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
    if(fd != cl->fd_cl) {
        pthread_mutex_unlock(cache->notAddedAccess);
        free(copy);
        errno = EAGAIN;
        return NULL;
    }
    toAdd = cl->f;
    cl->f = NULL;
    if(!fileIsOpenedFrom(toAdd, fd) || ((checkLock) && (!fileIsLockedFrom(toAdd, fd)))) {
        pthread_mutex_unlock(cache->notAddedAccess);
        free(copy);
        destroyFile(&toAdd);
        errno = EACCES;
        return NULL;
    }
    if(icl_hash_delete(cache->notAdded, copy, free, free_ClientFile) == -1) {
        pthread_mutex_unlock(cache->notAddedAccess);
        destroyFile(&toAdd);
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
    MEMORY_MISS(1, 0)
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
    LRU_Update(cache->LRU, cache->fileOnline);
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        errno = error;
        free(copy);
        destroyFile(&toAdd);
        return kickedFiles;
    }

    return kickedFiles;
}


/**
 * @brief                   Rimuove un file dalla memoria cache
 * @fun                     removeFileOnCache
 * @param cache             Memoria cache da cui estrarre il file
 * @param pathname          Pathname del file da rimuovere
 * @return                  Ritorna il file cancellato, altrimenti ritorna NULL (in caso di errore [setta errno])
 */
myFile* removeFileOnCache(LRU_Memory *cache, const char *pathname, int fd) {
    /** Variabili **/
    long index = -1;
    int error = 0;
    myFile **toDel = NULL, *del = NULL;
    userLink *uL = NULL;

    /** Controllo parametri **/
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
    if((toDel = (myFile **) bsearch(pathname, (cache->LRU), cache->fileOnline, sizeof(myFile **), findFileOnLRU)) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        errno = ENOENT;
        return NULL;
    }
    index = toDel - cache->LRU;
    del = *(toDel);
    if((error = pthread_mutex_lock(del->lockAccessFile)) != 0) {
        errno = error;
        pthread_mutex_unlock(cache->LRU_Access);
        return NULL;
    }
    if(icl_hash_delete(cache->tabella, del->pathname, free, NULL) == -1) {
        pthread_mutex_unlock(toDel[0]->lockAccessFile);
        pthread_mutex_unlock(cache->LRU_Access);
        errno = EAGAIN;
        return del;
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
        uL = (userLink *) linksManage(cache, (del->utentiConnessi)[index], (void *) pathname, 1, findPath);
        free_userLink(uL);
        if(errno != 0) {
            return del;
        }
    }

    return del;
}


/**
 * @brief                       Aggiorna il contenuto di un file in modo atomico
 * @fun                         appendFile
 * @param cache                 Memoria cache
 * @param pathname              Pathname del file da aggiornare
 * @param buffer                Buffer da aggiungere
 * @param size                  Dimensione del buffer da aggiungere
 * @return                      Ritorna gli eventuali file espulsi; in caso di errore valutare se si setta errno
 */
myFile** appendFile(LRU_Memory *cache, const char *pathname, void *buffer, size_t size) {
    /** Variabili **/
    myFile **kickedFiles = NULL, *toAdd = NULL, *fileToEdit = NULL;
    int error = 0, numKick = 0;
    char *copy = NULL;

    /** Controllo parametri **/
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
    if((fileToEdit = icl_hash_find(cache->tabella, copy)) == NULL) {
        pthread_mutex_unlock(cache->LRU_Access);
        free(copy);
        return NULL;
    }
    MEMORY_MISS(0, size)
    if((error = pthread_mutex_lock(fileToEdit->lockAccessFile)) != 0) {
        pthread_mutex_unlock(cache->LRU_Access);
        free(copy);
        errno = error;
        return kickedFiles;
    }
    if(addContentToFile(fileToEdit, buffer, size) == -1) {
        pthread_mutex_unlock(fileToEdit->lockAccessFile);
        pthread_mutex_unlock(cache->LRU_Access);
        free(copy);
        return kickedFiles;
    }
    cache->bytesOnline += size;
    if((cache->numeroMassimoBytesCaricato) < (cache->bytesOnline)) (cache->numeroMassimoBytesCaricato) = (cache->bytesOnline);
    LRU_Update(cache->LRU, cache->fileOnline);
    if((error = pthread_mutex_unlock(fileToEdit->lockAccessFile)) != 0) {
        pthread_mutex_unlock(fileToEdit->lockAccessFile);
        free(copy);
        errno = error;
        return kickedFiles;
    }
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) {
        pthread_mutex_unlock(fileToEdit->lockAccessFile);
        free(copy);
        errno = error;
        return kickedFiles;
    }


    free(copy);
    return kickedFiles;
}


/**
 * @brief                   Funzione che legge il contenuto del file e ne restituisce una copia
 * @fun                     readFileOnCache
 * @param cache             Memoria cache
 * @param pathname          Pathname del file da leggere
 * @param dataContent       Contenuto del file
 * @return                  Ritorna la dimensione del buffer; (-1) altrimenti [setta errno]
 */
size_t readFileOnCache(LRU_Memory *cache, const char *pathname, void **dataContent) {
    /** Variabili **/
    int error = 0;
    size_t size = -1;
    myFile *readF = NULL;

    /** Controllo parametri **/
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
    if((*dataContent = malloc(readF->size)) == NULL) {
        pthread_mutex_unlock(readF->lockAccessFile);
        return -1;
    }
    memcpy(*dataContent, readF->buffer, readF->size);
    size = readF->size;
    if((error = pthread_mutex_unlock(readF->lockAccessFile)) != 0) {
        free(*dataContent);
        *dataContent = NULL;
        errno = error;
        return -1;
    }

    return size;
}


/**
 * @brief               Legge N file in ordine della LRU non locked
 * @fun                 readRandFiles
 * @param cache         Memoria cache
 * @param N             Numero di file che si intende leggere
 * @return              Ritorna i file letti effettivamente; se ci sono errori viene settato errno e
 *                      viene ritornato NULL
 */
myFile** readsRandFiles(LRU_Memory *cache, int N) {
    /** Variabili **/
    myFile **filesRead = NULL, **new = NULL;
    int error = 0, index = -1, nReads = 0;

    /** Controllo parametri **/
    if(cache == NULL) { errno = EINVAL; return NULL; }

    /** Leggo i file in ordine di inserimento nella LRU **/
    if(N <= 0) N = (int) cache->maxFileOnline;
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        errno = error;
        return NULL;
    }
    while(((cache->LRU)[++index] != NULL) && (index < N)) {
        if((error = pthread_mutex_lock((cache->LRU[index])->lockAccessFile)) != 0) {
            pthread_mutex_unlock(cache->LRU_Access);
            errno = error;
            return NULL;
        }
        if(cache->LRU[index]->utenteLock == -1) {
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
            filesRead[nReads] = NULL;
        }
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

    return lockResult;
}


/**
 * @brief                   Effettua la unlock su un file per quel fd
 * @fun                     unlockFileOnCache
 * @param cache             Memoria cache
 * @param pathname          Pathname del file da sbloccare
 * @param unlockFD          Fd che effettua la unlock
 * @return                  (0) se la unlock e' riuscita;
 *                          (-1) in caso di errore [setta errno]
 */
int unlockFileOnCache(LRU_Memory *cache, const char *pathname, int unlockFD) {
    /** Variabili **/
    int error = 0, unlockResult = -1;
    myFile *fileToUnlock = NULL;

    /** Controllo parametri **/
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

    list = linksManage(cache, fd, (void *) NULL, 2, NULL);
    if((list == NULL) && (errno != 0))
        return NULL;
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) {
        destroyQueue(&(list), free_userLink);
        errno = error;
        return NULL;
    }
    del = (userLink *) deleteFirstElement(&list);
    while(del != NULL) {
        pathname = del->openFile;
        file = icl_hash_find(cache->tabella, pathname);
        if(file == NULL) { break; }
        if((error = pthread_mutex_lock(file->lockAccessFile)) != 0) {
            pthread_mutex_unlock(cache->LRU_Access);
            free_userLink(del);
            errno = error;
            return fdToUnlock;
        }
        errno=0;
        app = unlockFile(file, fd);
        closeFile(file, fd);
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
void deleteLRU(Settings **serverMemory, LRU_Memory **mem) {
    /** Dealloco le impostazioni **/
    if(*serverMemory != NULL) {
        free((*serverMemory)->socket);
        free(*serverMemory);
        serverMemory = NULL;
    }

    /** Dealloco la memoria LRU **/
    if(*mem != NULL) {
        int i = -1;

        pthread_mutex_destroy((*mem)->Files_Access);
        pthread_mutex_destroy((*mem)->LRU_Access);
        pthread_mutex_destroy((*mem)->notAddedAccess);
        pthread_mutex_destroy((*mem)->usersConnectedAccess);
        icl_hash_destroy((*mem)->tabella, free, free_file);
        icl_hash_destroy((*mem)->notAdded, free, free_ClientFile);
        while(++i < (*mem)->massimoNumeroDiFileOnline)
           destroyQueue(&(((*mem)->usersConnected)[i]), free_userLink);
        free((*mem)->Files_Access);
        free((*mem)->LRU_Access);
        free((*mem)->notAddedAccess);
        free((*mem)->usersConnectedAccess);
        free((*mem)->usersConnected);
        free((*mem)->LRU);
        free(*mem);
        *mem = NULL;
    }
}
