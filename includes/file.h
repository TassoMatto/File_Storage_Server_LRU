/**
 * @project             FILE_STORAGE_SERVER
 * @brief               Funzioni di gestione di un file in memoria virtuale
 * @author              Simone Tassotti
 * @date                28/12/2021
 * @finish              25/01/2022
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
     * @param time                      Tempo di ultimo utilizzo
     */
    typedef struct {
        char *pathname;
        size_t size;
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
     * @brief               Aggiorna il tempo di ultimo utilizzo
     * @fun                 updateTime
     */
    void updateTime(myFile *);


    /**
     * @brief                               Crea un file di tipo 'pathname' il quale può essere
     *                                      aperto da al più 'maxUtentiConnessiAlFile', accedendovi
     *                                      in mutua esclusione (se allocata) con 'lockAccessFile'
     * @fun                                 createFile
     * @return                              Ritorna la struttura del file tutta impostata
     *                                      In caso di errore ritorna NULL e setta errno
     */
    myFile* createFile(const char *, unsigned int, pthread_mutex_t *);


    /**
     * @brief                       Aggiorna il contenuto di 'file' aggiungendo in append 'toAdd'
     *                              di dimensione 'sizeToAdd'
     * @fun                         addContentToFile
     * @return                      Ritorna la dimensione finale del file con aggiunto il contenuto
     *                              In caso di errore ritorna NULL e setta errno
     */
    size_t addContentToFile(myFile *, void *, size_t);


    /**
     * @brief                   Controlla che 'fd' abbia aperto 'file'
     * @fun                     fileIsOpenedFrom
     * @return                  Se 'fd' ha aperto 'file' la funzione ritorna 1,
     *                          0 se non lo ha aperto e -1 se c'è un errore settando errno
     */
    int fileIsOpenedFrom(myFile *, int);


    /**
     * @brief               Controlla che 'fd' abbia effettuato una lock su 'file'
     * @fun                 fileIsLockedFrom
     * @return              Se 'fd' ha la lock su 'file' la funzione ritorna 1,
     *                      0 se non lo ha la lock e -1 se c'è un errore settando errno
     */
    int fileIsLockedFrom(myFile *, int);


    /**
     * @brief               Permette a 'fd' di aprire 'file'
     * @return              Ritorna 0 se il file è stato aperto correttamente,
     *                      1 se il file era già aperto da 'fd' e -1 se c'è
     *                      un errore; viene settato errno
     */
    int openFile(myFile *, int);


    /**
     * @brief               Esegue la chiusura di 'file' per conto di 'fd'
     * @fun                 closeFile
     * @return              Ritorna 0 se il file è stato chiuso correttamente,
     *                      -1 se c'è un errore; viene settato errno
     */
    int closeFile(myFile *file, int fd);


    /**
     * @brief               Effettua la lock su 'file' da parte di 'fd'
     * @fun                 lockFile
     * @return              Ritorna 0 se la lock è andata a buon fine;
     *                      1 se la lock è già presente nel file; -1 in
     *                      caso di errore e viene settato errno
     * @warning             In un caso speciale viene ritornato 1 e settato errno:
     *                      questo indica che la lock è in possesso di 'fd'
     */
    int lockFile(myFile *, int);


    /**
     * @brief               Effettua la unlock su 'file' da parte di 'fd'
     * @fun                 unlockFile
     * @return              Ritorna 0 se la unlock è andata a buonfine; -1 se
     *                      ci sono errori; viene settato errno
     */
    int unlockFile(myFile *, int);


    /**
     * @brief                               Distruggo 'file'
     * @fun                                 destroyFile
     */
    void destroyFile(myFile **);


#endif //FILE_STORAGE_SERVER_LRU_FILE_H
