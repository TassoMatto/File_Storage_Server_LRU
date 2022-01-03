/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di un file in memoria virtuale
 * @author              Simone Tassotti
 * @date                28/12/2021
 */


#include "file.h"


/**
 * @brief           Funzione di confronto tra FD per il confronto nella coda
 * @fun             compare_fd
 * @param v1        Primo FD da confrontare
 * @param v2        Secondo FD da confrontare
 * @return          (1) se corrispondono; (0) altrimenti
 */
static int compare_fd(const void *v1, const void *v2) {
    int fd1 = *(int *) v1;
    int fd2 = *(int *) v2;

    return fd1==fd2;
}


/**
 * @brief               Aggiorna il tempo di ultimo utilizzo
 * @fun                 updateTime
 * @param file          File su cui aggiornare il tempo di ultimo utilizzo
 */
void updateTime(myFile *file) {
    struct timeval timing;
    if(file != NULL)
    {
        if(gettimeofday(&timing, NULL) == -1)
            memcpy(&(file->time), &timing, sizeof(struct timeval));
    }
}


/**
 * @brief                               Crea la struttura del file
 * @fun                                 createFile
 * @param pathname                      Pathname del file da creare
 * @param maxUtentiConnessiAlFile       Numero massimo di utenti che si puo' aprire il file
 * @param lockAccessFile                Variabile per l'accesso esterno in mutua esclusione
 * @return                              Ritorna il file; in caso di errore ritorna NULL [setta errno]
 */
myFile* createFile(const char *pathname, unsigned int maxUtentiConnessiAlFile, pthread_mutex_t *lockAccessFile) {
    /** Variabili **/
    myFile *file = NULL;

    /** Controllo parametri **/
    if(pathname == NULL) { errno = EINVAL; return NULL; }
    if(maxUtentiConnessiAlFile == 0) { errno = EINVAL; return NULL; }

    /** Creo il file **/
    if((file = (myFile *) malloc(sizeof(myFile))) == NULL) { return NULL; }
    memset(file, 0, sizeof(myFile));
    file->utentiLocked = NULL;
    if((file->pathname = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) { free(file); return NULL; }
    strncpy(file->pathname, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    if(lockAccessFile != NULL) { file->lockAccessFile = lockAccessFile; }
    if((file->utentiConnessi =  (int *) calloc(maxUtentiConnessiAlFile, sizeof(unsigned int))) == NULL) { free(file->pathname); free(file); return NULL; }
    memset(file->utentiConnessi, -1, maxUtentiConnessiAlFile*sizeof(unsigned int));
    file->maxUtentiConnessiAlFile = maxUtentiConnessiAlFile;
    file->utenteLock = -1;
    updateTime(file);

    return file;
}


/**
 * @brief                       Aggiorna il contenuto di un file
 * @fun                         addContentToFile
 * @param file                  File da aggiornare
 * @param toAdd                 Buffer con cui aggiornare il file
 * @param sizeToAdd             Dimensione del buffer
 * @return                      Ritorna la nuova dimensione del file; altrimenti ritorna -1 [setta errno]
 */
size_t addContentToFile(myFile *file, void *toAdd, ssize_t sizeToAdd) {
    /** Variabili **/
    void *copyBuffer = NULL;

    /** Controllo parametri **/
    if(file == NULL) { errno = EINVAL; return -1; }
    if(toAdd == NULL) { errno = EINVAL; return -1; }
    if(sizeToAdd <= 0) { errno = EINVAL; return -1; }

    /** Aggiungo il contenuto al file **/
    if((copyBuffer = malloc(sizeToAdd+file->size)) == NULL) { return -1; }
    memcpy(copyBuffer, file->buffer, file->size);
    memcpy(copyBuffer+file->size, toAdd, sizeToAdd);
    free(file->buffer);
    file->size += sizeToAdd;
    file->buffer = copyBuffer;

    updateTime(file);
    return file->size;
}


/**
 * @brief                   Controlla che un file sia stato aperto da quel FD
 * @fun                     fileIsOpenedFrom
 * @param file              File da controllare
 * @param fd                FD su cui andare a verificare l'apertura
 * @return                  (1) se Fd ha aperto il file; (0) altrimenti [setta errno]
 */
static int fileIsOpenedFrom(myFile *file, int fd) {
    /** Variabili **/
    int index = -1;

    /** Controllo parametri **/
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Controllo apertura del file **/
    while(++index < file->numeroUtentiConnessi)
        if((file->utentiConnessi)[index] == fd) return 1;

    return 0;
}


/**
 * @brief               Controlla che FD abbia effettuato la lock
 * @fun                 fileIsLockedFrom
 * @param file          File su cui effettuare le verifiche
 * @param fd            FD per effettuare il controllo
 * @return              (1) se Fd ha la lock sul file; (0) se non ha la lock; (-1) se ci sono errori [setta errno]
 */
static int fileIsLockedFrom(myFile *file, int fd) {
    /** Variabili **/
    int index = -1;
    int *fdSearch = NULL;

    /** Controllo parametri **/
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }
    if((fdSearch = (int *) malloc(sizeof(int))) == NULL) { errno = EINVAL; return -1; }

    /** Controllo di lock sul file **/
    *fdSearch = fd;
    if(fileIsOpenedFrom(file, fd) == 0) return 0;
    while(++index < file->numeroUtentiConnessi) {
        if((file->utentiConnessi)[index] == fd) {
            if(searchElement(file->utentiLocked, fdSearch, compare_fd) == 1) { free(fdSearch); return 1; }
        }
    }

    free(fdSearch);
    return 0;
}


/**
 * @brief               Apertura del file da parte di fd
 * @param file          File da aprire
 * @param fd            FD che vuole aprire il file
 * @return              (0) file aperto correttamente; (1) file gia aperto da FD; (-1) altrimenti [setta errno]
 */
int openFile(myFile *file, int fd) {
    /** Controllo parametri **/
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Apertura del file da parte di fd **/
    if(file->numeroUtentiConnessi == file->maxUtentiConnessiAlFile) { printf("NOO\n"); errno = EMLINK; return -1; }
    if(fileIsOpenedFrom(file, fd)) return 1;

    (file->utentiConnessi)[file->numeroUtentiConnessi] = fd;
    (file->numeroUtentiConnessi)++;

    updateTime(file);
    return 0;
}


/**
 * @brief               Chiude il file per FD
 * @fun                 closeFile
 * @param file          File da chiudere per FD
 * @param fd            FD che deve chiudere il file
 * @return              (0) in caso di successo; (-1) altrimenti [setta errno]
 */
int closeFile(myFile *file, int fd) {
    /** Variabili **/
    int index = -1;

    /** Controllo parametri **/
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Chiudo il file **/
    while(++index < file->numeroUtentiConnessi) {
        if((file->utentiConnessi)[index] == fd) {
            int fd_save = (file->utentiConnessi)[file->numeroUtentiConnessi-1];
            (file->utentiConnessi)[file->numeroUtentiConnessi-1] = -1;
            (file->utentiConnessi)[index] = fd_save;

            updateTime(file);
            return 0;
        }
    }

    errno = EBADF;
    return -1;
}


/**
 * @brief               Effettua la lock sul file per FD
 * @fun                 lockFile
 * @param file          File su cui effettuare la lock
 * @param fd            FD che vuole effettuare la lock
 * @return              (0) se la lock e' stata effettuata; (1) se FD ha gia' fatto
 *                      la lock; (-1) altrimenti [setta errno]
 */
int lockFile(myFile *file, int fd) {
    /** Variabili **/
    int *fdInsert = NULL;

    /** Controllo parametri **/
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Lock del file **/
    if(!fileIsOpenedFrom(file, fd)) { errno = EBADF; return -1; }
    if(fileIsLockedFrom(file, fd) == 1) return 1;
    if((fdInsert = (int *) malloc(sizeof(int))) == NULL) return -1;
    *fdInsert = fd;
    if(file->utentiLocked == NULL) file->utenteLock = fd;
    if((file->utentiLocked = insertIntoQueue(file->utentiLocked, fdInsert, sizeof(int))) == NULL) { file->utenteLock = -1; free(fdInsert); return -1; }

    free(fdInsert);
    updateTime(file);
    return 0;
}


/**
 * @brief               Effettua la unlock sul file per FD
 * @fun                 unlockFile
 * @param file          File su cui effettuare la lock
 * @param fd            FD che vuole effettuare la lock
 * @return              (0) se la lock e' stata effettuata; (-1) altrimenti [setta errno]
 */
int unlockFile(myFile *file, int fd) {
    /** Variabili **/
    void *delete = NULL;

    /** Controllo parametri **/
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Lock del file **/
    if(!fileIsOpenedFrom(file, fd)) { errno = EBADF; return -1; }
    if(!fileIsLockedFrom(file, fd)) { errno = EBADF; return -1; }
    if((delete = deleteElementFromQueue(&(file->utentiLocked), &fd, compare_fd)) == NULL) { errno = EBADF; return -1; }
    if(file->utentiLocked != NULL) file->utenteLock = *(int *) file->utentiLocked->data;

    free(delete);
    updateTime(file);
    return 0;
}


/**
 * @brief                               Elimino un file
 * @fun                                 destroyFile
 * @param file                          File da cancellare
 */
void destroyFile(myFile **file) {
    /** Controllo parametri **/
    if((*file) == NULL) return;

    /** Dealloco la memoria **/
    if((*file)->pathname != NULL) free((*file)->pathname);
    if((*file)->buffer != NULL) free((*file)->buffer);
    free((*file)->utentiConnessi);
    destroyQueue(&((*file)->utentiLocked), free);
    free((*file));

    file = NULL;
}