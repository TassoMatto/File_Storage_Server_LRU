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
        (cache->numeroMemoryMiss)++;                                                                                                        \
        numKick++;                                                                                                                          \
        if((kickedFiles = (myFile **) realloc(kickedFiles, (numKick+1)*sizeof(myFile *))) == NULL) { free(copy); return NULL; }             \
        if((error = pthread_mutex_lock(((cache->LRU)[0])->lockAccessFile)) != 0) {                                                          \
            errno = error;                                                                                                                  \
            free(copy);                                                                                                                     \
            return kickedFiles;                                                                                                             \
        }                                                                                                                                   \
        kickedFiles[numKick-1] = (cache->LRU)[0];                                                                                           \
        (cache->LRU)[0] = (cache->LRU)[--(cache->fileOnline)];                                                                              \
        (cache->LRU)[(cache->fileOnline)] = NULL;                                                                                           \
        (cache->bytesOnline) -= (kickedFiles[numKick-1])->size;                                                                             \
        if(icl_hash_delete(cache->tabella, kickedFiles[numKick-1]->pathname, free, NULL) == -1) {                                           \
            errno = error;                                                                                                                  \
            free(copy);                                                                                                                     \
            return kickedFiles;                                                                                                             \
        }                                                                                                                                   \
        kickedFiles[numKick] = NULL;                                                                                                        \
    }


/**
 * @brief           Funzione destroyFile riadattata per la icl_hash_destroy
 * @fun             free_file
 * @param f         File da cancellare
 */
static void free_file(void *f) {
    /** Variabili **/
    myFile *file = (myFile *) f;

    /** Cancellazione del file **/
    destroyFile(&file);
}


static void free_preIns(void *d) {
    ClientFile *cl = (ClientFile *) d;

    destroyFile(&(cl->f));
    free(cl);
}


/**
 * @brief           Compara un file con il pathname che si vuole ricercare
 * @fun             findFileOnLRU
 * @return          Ritorna 0 se i valori del pathname corrispondono oppure la differenza tra il pathname passato e
 *                  quello del file con cui si compara
 */
static int findFileOnLRU(const void *v1, const void *v2) {
    char *p = (char *) v1;
    myFile **f = (myFile **) v2;

    return strncmp(p, (*f)->pathname, strnlen((*f)->pathname, MAX_PATHNAME));
}


/**
 * @brief                   Funzione usata nella qsort per riordinare gli elementi nella LRU
 * @fun                     orderLRUFiles
 * @param v1                Primo valore
 * @param v2                Secondo valore
 * @return                  Ritorna la differenza tra il primo e secondo valore
 */
static int orderLRUFiles(const void *v1, const void *v2) {
    myFile **f1 = NULL, **f2 = NULL;
    time_t t1Sec = -1, t2Sec = -1, t1uSec = -1, t2uSec = -1;

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


    if((t1Sec - t2Sec) == 0)
        return (int) (t1uSec-t2uSec);
    else
        return (int) (t1Sec - t2Sec);
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
 * @brief                   Creo la tabella che conterranno i file vuoti
 *                          non ancora scritti e salvati nella cache
 * @fun                     createPreInserimento
 * @param dim               Dimensione della tabella di Pre-Inserimento
 * @param log               File di log per il tracciamento delle operazioni
 * @return                  Ritorna la struttura di Pre-Inserimento; in caso di errore [setta errno]
 */
Pre_Inserimento* createPreInserimento(int dim, serverLogFile *log) {
    /** Variabili **/
    int error = 0;
    Pre_Inserimento *pI = NULL;

    /** Creo la struttura che ospita i file prima di inserirli in memoria **/
    if((pI = (Pre_Inserimento *) malloc(sizeof(Pre_Inserimento))) == NULL) { return NULL; }
    memset(pI, 0, sizeof(Pre_Inserimento));
    if((pI->pI = icl_hash_create(dim, NULL, NULL)) == NULL) {
        errno = EAGAIN;
        free(pI);
        return NULL;
    }
    if(((pI->lockPI) = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t))) == NULL) {
        icl_hash_destroy(pI->pI, NULL, NULL);
        free(pI);
        return NULL;
    }
    if((error = pthread_mutex_init(pI->lockPI, NULL)) != 0) {
        free(pI->lockPI);
        icl_hash_destroy(pI->pI, NULL, NULL);
        free(pI);
    }
    pI->log = log;

    return pI;
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
int createFileToInsert(Pre_Inserimento *pI, const char *pathname, unsigned int maxUtenti, int fd, int lock) {
    /** Variabili **/
    int error = 0;
    myFile *create = NULL;
    ClientFile *cl = NULL;
    char *copy = NULL;

    /** Controllo parametri **/
    if(pI == NULL) { printf("NO1"); errno = EINVAL; return -1; }
    if(pathname == NULL) { printf("NO2");  errno = EINVAL; return -1; }
    if(fd <= 0) {  printf("NO3"); errno = EINVAL; return -1; }

    /** Creo il file e lo aggiungo nella lista di PreInserimento **/
    if((copy = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        return -1;
    }
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
    }
    cl->f = create;
    cl->fd_cl = fd;
    if((error = pthread_mutex_lock(pI->lockPI)) != 0) {
        free(cl);
        destroyFile(&create);
        free(copy);
        errno = error;
        return -1;
    }
    if(icl_hash_insert(pI->pI, copy, cl) == NULL) {
        pthread_mutex_unlock(pI->lockPI);
        free(cl);
        destroyFile(&create);
        free(copy);
        return -1;
    }
    if((error = pthread_mutex_unlock(pI->lockPI)) != 0) {
        free(cl);
        destroyFile(&create);
        free(copy);
        errno = error;
        return -1;
    }

    return 0;
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
    if((mem->Files_Access = (pthread_mutex_t *) calloc(2*(set->maxNumeroFileCaricabili), sizeof(pthread_mutex_t))) == NULL) {
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
 * @brief                   Controlla se un file esiste o meno nel server (per uso esterno al file FileStorageServer.c)
 * @fun                     fileExist
 * @param cache             Memoria cache su cui andare a controllare
 * @param pathname          Pathname del file da cercare
 * @return                  (1) se il file esiste; (0) se non esiste; (-1) in caso di errore nella ricerca
 */
int fileExist(LRU_Memory *cache, const char *pathname) {
    /** Variabili **/
    int error = 0, result = -1;
    char copyPathname[MAX_PATHNAME];

    /** Controllo parametri **/
    if(cache == NULL) { errno = EINVAL; return -1; }
    if(pathname == NULL) { errno = EINVAL; return -1; }

    /** Controllo esistenza file **/
    strncpy(copyPathname, pathname, strnlen(pathname, MAX_BUFFER_LEN)+1);
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) { errno = error; return -1; }
    result = (icl_hash_find(cache->tabella, copyPathname) != NULL);
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) { errno = error; return -1; }

    return result;
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

    return result;
}


/**
 * @brief               Funzione che aggiunge il file creato da fd nella memoria cache (gia' creato e aggiunto in Pre-Inserimento)
 * @fun                 addFileOnCache
 * @param cache         Memoria cache
 * @param pI            Memoria di Pre-Inserimento
 * @param pathname      Pathname del file da andare a caricare
 * @param fd            Fd di colui che aggiunge il file
 * @return              Ritorna i file espulsi in seguito a memory miss oppure NULL; in caso di errore nell'aggiunta del file
 *                      viene settato errno
 */
myFile** addFileOnCache(LRU_Memory *cache, Pre_Inserimento *pI, const char *pathname, int fd) {
    /** Variabili **/
    int error = 0, numKick = 0;
    char *copy = NULL;
    unsigned int hashPathname = 0;
    ClientFile *cl = NULL;
    myFile **kickedFiles = NULL, *toAdd = NULL;

    /** Controllo parametri **/
    if(cache == NULL) { errno = EINVAL; return NULL; }
    if(pI == NULL) { errno = EINVAL; return NULL; }
    if(pathname == NULL) { errno = EINVAL; return NULL; }
    if(fd <= 0) { errno = EINVAL; return NULL; }

    /** Aggiungo il file **/
    if((copy = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        return NULL;
    }
    strncpy(copy, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    hashPathname = hash_pjw(copy), hashPathname %= 2*(cache->maxFileOnline);
    if((error = pthread_mutex_lock(pI->lockPI)) != 0) {
        free(copy);
        errno = error;
        return NULL;
    }
    if((cl = (ClientFile *) icl_hash_find(pI->pI, copy)) == NULL) {
        pthread_mutex_unlock(pI->lockPI);
        free(copy);
        errno = EAGAIN;
        return NULL;
    }
    if(fd != cl->fd_cl) {
        pthread_mutex_unlock(pI->lockPI);
        free(copy);
        errno = EAGAIN;
        return NULL;
    }
    toAdd = cl->f;
    free(cl);
    if(!fileIsOpenedFrom(toAdd, fd) || !fileIsLockedFrom(toAdd, fd)) {
        pthread_mutex_unlock(pI->lockPI);
        free(copy);
        errno = EACCES;
        return NULL;
    }
    if(icl_hash_delete(pI->pI, copy, free, NULL) == -1) {
        pthread_mutex_unlock(pI->lockPI);
        destroyFile(&toAdd);
        free(copy);
        errno = EAGAIN;
        return NULL;
    }
    if((error = pthread_mutex_unlock(pI->lockPI)) != 0) {
        free(copy);
        destroyFile(&toAdd);
        errno = error;
        return NULL;
    }
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) { errno = error; free(copy); return NULL; }
    MEMORY_MISS(1, 0)
    updateTime(toAdd);
    toAdd->lockAccessFile = (cache->Files_Access) + hashPathname;
    if(icl_hash_insert(cache->tabella, copy, toAdd) == NULL) { pthread_mutex_unlock(cache->LRU_Access); free(copy); errno = EAGAIN; return kickedFiles; }
    (cache->LRU)[(cache->fileOnline)++] = toAdd;
    if((cache->massimoNumeroDiFileOnline) < (cache->fileOnline)) (cache->massimoNumeroDiFileOnline) = (cache->fileOnline);
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) { errno = error; free(copy); return kickedFiles; }
    LRU_Update(cache->LRU, cache->fileOnline);

    return kickedFiles;
}


/**
 * @brief                   Rimuove un file dalla memoria cache
 * @fun                     removeFileOnCache
 * @param cache             Memoria cache da cui estrarre il file
 * @param pathname          Pathname del file da rimuovere
 * @return                  Ritorna il file cancellato, altrimenti ritorna NULL (in caso di errore [setta errno])
 */
myFile* removeFileOnCache(LRU_Memory *cache, const char *pathname) {
    /** Variabili **/
    long index = -1;
    int error = 0;
    myFile **toDel = NULL, *del = NULL;

    /** Controllo parametri **/
    if(cache == NULL) { errno = EINVAL; return NULL; }
    if(pathname == NULL) { errno = EINVAL; return NULL; }

    /** Cerco il file e lo cancello **/
    if((error = pthread_mutex_lock(cache->LRU_Access)) != 0) { errno = error; return NULL; }
    if((toDel = (myFile **) bsearch(pathname, (cache->LRU), cache->fileOnline, sizeof(myFile **), findFileOnLRU)) == NULL) { pthread_mutex_unlock(cache->LRU_Access); return NULL; }
    if((error = pthread_mutex_lock(toDel[0]->lockAccessFile)) != 0) { errno = error; pthread_mutex_unlock(cache->LRU_Access); return NULL; }
    index = toDel - cache->LRU;
    printf("%ld\n", index);
    if(icl_hash_delete(cache->tabella, toDel[0]->pathname, free, NULL) == -1) {
        errno = EAGAIN;
        pthread_mutex_unlock(toDel[0]->lockAccessFile);
        pthread_mutex_unlock(cache->LRU_Access);
    }
    del = *(toDel);
    cache->bytesOnline -= del->size;
    cache->LRU[index] = cache->LRU[--(cache->fileOnline)];
    cache->LRU[(cache->fileOnline)] = NULL;
    if((error = pthread_mutex_unlock(del->lockAccessFile)) != 0) { errno = error; pthread_mutex_unlock(cache->LRU_Access); return NULL; }
    del->lockAccessFile = NULL;
    LRU_Update((cache->LRU), cache->fileOnline);
    if((error = pthread_mutex_unlock(cache->LRU_Access)) != 0) { errno = error; return NULL; }

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
myFile** appendFile(LRU_Memory *cache, const char *pathname, void *buffer, ssize_t size) {
    /** Variabili **/
    myFile **kickedFiles = NULL, *fileToEdit = NULL;
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
        return kickedFiles;
    }
    cache->bytesOnline += size;
    if((cache->numeroMassimoBytesCaricato) < (cache->bytesOnline)) (cache->numeroMassimoBytesCaricato) = (cache->bytesOnline);
    LRU_Update(cache->LRU, cache->fileOnline);

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
        errno = error;
        return -1;
    }

    return size;
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
    if((lockResult = lockFile(fileToLock, lockFD)) == -1) {
        pthread_mutex_unlock(fileToLock->lockAccessFile);
        return -1;
    }
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
 * @brief                   Cancella tutta la memoria e i settings della memoria cache
 * @fun                     deleteLRU
 * @param serverMemory      Settings del server letti dal config file
 * @param mem               Memoria cache
 * @param pI                Tabella di Pre-Inserimento
 */
void deleteLRU(Settings **serverMemory, LRU_Memory **mem, Pre_Inserimento **pI) {
    /** Dealloco le impostazioni **/
    if(*serverMemory != NULL) {
        free((*serverMemory)->socket);
        free(*serverMemory);
        serverMemory = NULL;
    }

    /** Dealloco la memoria LRU **/
    if(*mem != NULL) {
        pthread_mutex_destroy((*mem)->Files_Access);
        free((*mem)->Files_Access);
        icl_hash_destroy((*mem)->tabella, free, free_file);
        pthread_mutex_destroy((*mem)->LRU_Access);
        free((*mem)->LRU_Access);
        free((*mem)->LRU);
        free(*mem);
        *mem = NULL;
    }

    /** Dealloco il Pre-Inserimento **/
    if(*pI != NULL) {
        pthread_mutex_destroy((*pI)->lockPI);
        free((*pI)->lockPI);
        icl_hash_destroy((*pI)->pI, free, free_preIns);
        free(*pI);
        *pI = NULL;
    }
}
