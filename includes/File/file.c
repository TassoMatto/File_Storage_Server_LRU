/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Funzioni di gestione di un file in memoria virtuale
 * @author              Simone Tassotti
 * @date                28/12/2021
 * @finish              25/01/2022
 */


#include "file.h"


/**
 * @brief           Funzione di confronto per cercare elementi nella coda
 * @fun             compare_fd
 * @param v1        Primo FD da confrontare
 * @param v2        Secondo FD da confrontare
 * @return          (1) Corrispondono
 *                  (0) Non Corrispondono
 */
static int compare_fd(const void *v1, const void *v2) {
    /** Variabili **/
    int fd1 = -1;
    int fd2 = -1;

    /** Cast **/
    fd1 = *(int *) v1;
    fd2 = *(int *) v2;

    /** Risultato **/
    return fd1==fd2;
}


/**
 * @brief               Aggiorna il tempo di ultimo utilizzo
 * @fun                 updateTime
 * @param file          File su cui aggiornare il tempo di ultimo utilizzo
 */
void updateTime(myFile *file) {
    /** Variabili **/
    struct timeval timing;

    /** Aggiorno il tempo del file **/
    if(gettimeofday(&timing, NULL) != -1) {
        memcpy(&(file->time), &timing, sizeof(struct timeval));
    }
}


/**
 * @brief                               Crea un file di tipo 'pathname' il quale può essere
 *                                      aperto da al più 'maxUtentiConnessiAlFile', accedendovi
 *                                      in mutua esclusione (se allocata) con 'lockAccessFile'
 * @fun                                 createFile
 * @param pathname                      Pathname del file da creare
 * @param maxUtentiConnessiAlFile       Numero massimo di utenti che si puo' aprire il file
 * @param lockAccessFile                Variabile per l'accesso in mutua esclusione
 * @return                              Ritorna la struttura del file tutta impostata
 *                                      In caso di errore ritorna NULL e setta errno
 */
myFile* createFile(const char *pathname, unsigned int maxUtentiConnessiAlFile, pthread_mutex_t *lockAccessFile) {
    /** Variabili **/
    myFile *file = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(pathname == NULL) { errno = EINVAL; return NULL; }
    if(maxUtentiConnessiAlFile == 0) { errno = EINVAL; return NULL; }

    /** Creo il file **/
    if((file = (myFile *) malloc(sizeof(myFile))) == NULL) {
        return NULL;
    }
    memset(file, 0, sizeof(myFile));
    if((file->pathname = (char *) calloc(strnlen(pathname, MAX_PATHNAME)+1, sizeof(char))) == NULL) {
        free(file);
        return NULL;
    }
    strncpy(file->pathname, pathname, strnlen(pathname, MAX_PATHNAME)+1);
    if(lockAccessFile != NULL) {    // Caso in cui passo una lock per accesso in mutua esclusione
        file->lockAccessFile = lockAccessFile;
    }
    if((file->utentiConnessi =  (int *) calloc(maxUtentiConnessiAlFile, sizeof(unsigned int))) == NULL) {
        free(file->pathname);
        free(file);
        return NULL;
    }
    memset(file->utentiConnessi, -1, maxUtentiConnessiAlFile*sizeof(unsigned int));
    file->maxUtentiConnessiAlFile = maxUtentiConnessiAlFile;
    file->utenteLock = -1;


    /** File creato correttamente **/
    updateTime(file);
    errno = 0;
    return file;
}


/**
 * @brief                       Aggiorna il contenuto di 'file' aggiungendo in append 'toAdd'
 *                              di dimensione 'sizeToAdd'
 * @fun                         addContentToFile
 * @param file                  File da aggiornare
 * @param toAdd                 Buffer con cui aggiornare il file
 * @param sizeToAdd             Dimensione del buffer
 * @return                      Ritorna la dimensione finale del file con aggiunto il contenuto
 *                              In caso di errore ritorna NULL e setta errno
 */
size_t addContentToFile(myFile *file, void *toAdd, size_t sizeToAdd) {
    /** Variabili **/
    void *copyBuffer = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(file == NULL) { errno = EINVAL; return -1; }
    if(toAdd == NULL) { errno = EINVAL; return -1; }
    if(sizeToAdd <= 0) { errno = EINVAL; return -1; }

    /** Aggiungo il contenuto al file **/
    if((copyBuffer = malloc(sizeToAdd+file->size)) == NULL) {
        return -1;
    }
    memcpy(copyBuffer, file->buffer, file->size);
    memcpy(copyBuffer+file->size, toAdd, sizeToAdd);
    free(file->buffer);
    file->size += sizeToAdd;
    file->buffer = copyBuffer;

    /** Contento aggiornato **/
    updateTime(file);
    errno=0;
    return file->size;
}


/**
 * @brief                   Controlla che 'fd' abbia aperto 'file'
 * @fun                     fileIsOpenedFrom
 * @param file              File da controllare
 * @param fd                FD su cui andare a verificare l'apertura
 * @return                  Se 'fd' ha aperto 'file' la funzione ritorna 1,
 *                          0 se non lo ha aperto e -1 se c'è un errore settando errno
 */
int fileIsOpenedFrom(myFile *file, int fd) {
    /** Variabili **/
    int index = -1;

    /** Controllo parametri **/
    errno = 0;
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Controllo apertura del file **/
    while(++index < file->numeroUtentiConnessi) {
        if((file->utentiConnessi)[index] == fd) {
            errno = 0;
            return 1;
        }
    }

    errno = 0;
    return 0;
}


/**
 * @brief               Controlla che 'fd' abbia effettuato una lock su 'file'
 * @fun                 fileIsLockedFrom
 * @param file          File su cui effettuare le verifiche
 * @param fd            FD per effettuare il controllo
 * @return              Se 'fd' ha la lock su 'file' la funzione ritorna 1,
 *                      0 se non lo ha la lock e -1 se c'è un errore settando errno
 */
int fileIsLockedFrom(myFile *file, int fd) {
    /** Variabili **/
    int res = -1;
    int *fdSearch = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }
    if((fdSearch = (int *) malloc(sizeof(int))) == NULL) {
        errno = EINVAL;
        return -1;
    }

    /** Controllo di lock sul file **/
    *fdSearch = fd;
    if(fileIsOpenedFrom(file, fd) == 0) {
        free(fdSearch);
        return 0;
    }
    res = elementExist(file->utentiLocked, fdSearch, compare_fd);
    if(res == 1) {
        free(fdSearch);
        errno = 0;
        return 1;
    }

    free(fdSearch);
    errno = 0;
    return 0;
}


/**
 * @brief               Permette a 'fd' di aprire 'file'
 * @param file          File da aprire
 * @param fd            FD che vuole aprire il file
 * @return              Ritorna 0 se il file è stato aperto correttamente,
 *                      1 se il file era già aperto da 'fd' e -1 se c'è
 *                      un errore; viene settato errno
 */
int openFile(myFile *file, int fd) {
    /** Controllo parametri **/
    errno = 0;
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Apertura del file da parte di fd **/
    if(file->numeroUtentiConnessi == file->maxUtentiConnessiAlFile) {
        errno = EMLINK;
        return -1;
    }
    if(fileIsOpenedFrom(file, fd)) {
        errno = 0;
        return 1;
    }

    /** Apro il file **/
    (file->utentiConnessi)[file->numeroUtentiConnessi] = fd;
    (file->numeroUtentiConnessi)++;
    updateTime(file);
    errno = 0;
    return 0;
}


/**
 * @brief               Esegue la chiusura di 'file' per conto di 'fd'
 * @fun                 closeFile
 * @param file          File da chiudere per FD
 * @param fd            FD che deve chiudere il file
 * @return              Ritorna 0 se il file è stato chiuso correttamente,
 *                      -1 se c'è un errore; viene settato errno
 */
int closeFile(myFile *file, int fd) {
    /** Variabili **/
    int index = -1;

    /** Controllo parametri **/
    errno = 0;
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Chiudo il file **/
    if(fileIsLockedFrom(file, fd) == 1) {
        errno = EPERM;
        return -1;
    }
    while(++index < file->numeroUtentiConnessi) {
        if((file->utentiConnessi)[index] == fd) {
            (file->numeroUtentiConnessi)--;
            if(file->numeroUtentiConnessi == 0) {
                (file->utentiConnessi)[0] = -1;
                updateTime(file);
                errno = 0;
                return 0;
            }
            int fd_save = (file->utentiConnessi)[file->numeroUtentiConnessi];
            (file->utentiConnessi)[file->numeroUtentiConnessi] = -1;
            (file->utentiConnessi)[index] = fd_save;
            updateTime(file);
            errno = 0;
            return 0;
        }
    }

    errno = EBADF;
    return -1;
}


/**
 * @brief               Effettua la lock su 'file' da parte di 'fd'
 * @fun                 lockFile
 * @param file          File su cui effettuare la lock
 * @param fd            FD che vuole effettuare la lock
 * @return              Ritorna 0 se la lock è andata a buon fine;
 *                      1 se la lock è già presente nel file; -1 in
 *                      caso di errore e viene settato errno
 * @warning             In un caso speciale viene ritornato 1 e settato errno:
 *                      questo indica che la lock è in possesso di 'fd'
 */
int lockFile(myFile *file, int fd) {
    /** Variabili **/
    int *fdInsert = NULL, res = 0;

    /** Controllo parametri **/
    errno = 0;
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Lock del file **/
    if(!fileIsOpenedFrom(file, fd)) {
        errno = EBADF;
        return -1;
    }
    if(fileIsLockedFrom(file, fd) == 1) {
        errno = EALREADY;
        return -1;
    }
    if((fdInsert = (int *) malloc(sizeof(int))) == NULL) return -1;
    *fdInsert = fd;
    if(file->utentiLocked == NULL) file->utenteLock = fd;
    else res = 1;
    if((file->utentiLocked = insertIntoQueue(file->utentiLocked, fdInsert, sizeof(int))) == NULL) {
        file->utenteLock = -1;
        free(fdInsert);
        return -1;
    }

    free(fdInsert);
    updateTime(file);
    errno = 0;
    return res;
}


/**
 * @brief               Effettua la unlock su 'file' da parte di 'fd'
 * @fun                 unlockFile
 * @param file          File su cui effettuare la unlock
 * @param fd            FD che vuole effettuare la unlock
 * @return              Ritorna 0 se la unlock è andata a buonfine; -1 se
 *                      ci sono errori; viene settato errno
 */
int unlockFile(myFile *file, int fd) {
    /** Variabili **/
    int retValue = 0;
    void *delete = NULL;

    /** Controllo parametri **/
    errno = 0;
    if(file == NULL) { errno = EINVAL; return -1; }
    if(fd <= 0) { errno = EINVAL; return -1; }

    /** Lock del file **/
    if(!fileIsOpenedFrom(file, fd)) {
        errno = EBADF;
        return -1;
    }
    if(!fileIsLockedFrom(file, fd)) {
        errno = EBADF;
        return -1;
    }
    retValue = file->utenteLock;
    if((delete = deleteElementFromQueue(&(file->utentiLocked), &fd, compare_fd)) == NULL) {
        errno = EBADF;
        return -1;
    }
    if((delete != NULL) && (file->utenteLock == *(int *) delete) && (file->utentiLocked != NULL)) file->utenteLock = *(int *) (file->utentiLocked)->data;
    else if(file->utentiLocked == NULL) file->utenteLock = -1;

    updateTime(file);
    free(delete);
    errno = 0;
    if((file->utentiLocked == NULL) || (retValue == file->utenteLock)) return 0;
    else return file->utenteLock;
}


/**
 * @brief                               Distruggo 'file'
 * @fun                                 destroyFile
 * @param file                          File da cancellare
 */
void destroyFile(myFile **file) {
    /** Controllo parametri **/
    if((*file) == NULL) return;

    /** Dealloco la memoria **/
    if((*file)->pathname != NULL) free((*file)->pathname);
    if((*file)->buffer != NULL) free((*file)->buffer);
    if((*file)->utentiConnessi != NULL) free((*file)->utentiConnessi);
    destroyQueue(&((*file)->utentiLocked), free);
    free(*file);

    *file = NULL;
}