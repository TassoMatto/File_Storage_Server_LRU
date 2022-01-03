/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Gestione di un file in memoria virtuale
 * @author              Simone Tassotti
 * @date                28/12/2021
 */

#ifndef FILE_STORAGE_SERVER_LRU_FILE_H


    #define FILE_STORAGE_SERVER_LRU_FILE_H

    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif

    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE
    #endif

    #include <stdlib.h>
    #include <stdio.h>
    #include <pthread.h>
    #include <errno.h>
    #include <string.h>
    #include <sys/time.h>
    #include <queue.h>
    #include <utils.h>


    /**
     * @brief                           Struttura che rappresenta un file
     * @struct                          myFile
     * @param pathname                  Pathname del file
     * @param size                      Dimensione del buffer del file
     * @param buffer                    Contenuto del file
     * @param utentiConnessi            Utenti che hanno aperto il file
     * @param utentiLocked              Utenti che hanno richiesto la lock sul file
     * @param utenteLock                Utente che ha la lock sul file
     * @param lockAccessFile            Lock per accedere in mutua esclusione al file
     * @param maxUtentiConnessiAlFile   Numero massimo di utenti che possono aprire al file
     * @param numeroUtentiConnessi      Numero di utenti hanno il file aperto
     */
    typedef struct {
        char *pathname;
        ssize_t size;
        void *buffer;

        int *utentiConnessi;
        Queue *utentiLocked;
        int utenteLock;
        pthread_mutex_t *lockAccessFile;
        unsigned int maxUtentiConnessiAlFile;
        unsigned int numeroUtentiConnessi;

        struct timeval time;
    } myFile;


    /**
     * @brief                               Crea la struttura del file
     * @fun                                 createFile
     * @return                              Ritorna il file; in caso di errore ritorna NULL [setta errno]
     */
    myFile* createFile(const char *, unsigned int, pthread_mutex_t *);


    /**
     * @brief               Aggiorna il tempo di ultimo utilizzo
     * @fun                 updateTime
     */
    void updateTime(myFile *);


    /**
     * @brief                       Aggiorna il contenuto di un file
     * @fun                         addContentToFile
     * @return                      Ritorna la nuova dimensione del file; altrimenti ritorna -1 [setta errno]
     */
    size_t addContentToFile(myFile *, void *, ssize_t);


    /**
     * @brief               Apertura del file da parte di fd
     * @return              (0) file aperto correttamente; (1) file gia aperto da FD; (-1) altrimenti [setta errno]
     */
    int openFile(myFile *, int);


    /**
     * @brief               Chiude il file per FD
     * @fun                 closeFile
     * @return              (0) in caso di successo; (-1) altrimenti [setta errno]
     */
    int closeFile(myFile *file, int fd);


    /**
    * @brief               Effettua la lock sul file per FD
    * @fun                 lockFile
    * @return              (0) se la lock e' stata effettuata; (1) se FD ha gia' fatto
    *                      la lock; (-1) altrimenti [setta errno]
    */
    int lockFile(myFile *, int);


    /**
    * @brief               Effettua la unlock sul file per FD
    * @fun                 unlockFile
    * @return              (0) se la lock e' stata effettuata; (-1) altrimenti [setta errno]
    */
    int unlockFile(myFile *, int);


    /**
     * @brief                               Elimino un file
     * @fun                                 destroyFile
     */
    void destroyFile(myFile **);


#endif //FILE_STORAGE_SERVER_LRU_FILE_H
